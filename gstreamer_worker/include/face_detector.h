#ifndef GSTREAMER_WORKER_FACE_DETECTOR_H
#define GSTREAMER_WORKER_FACE_DETECTOR_H

#include "config.h"
#include "types.h"
#include "compreface_client.h"

#include <opencv2/opencv.hpp>
#include <opencv2/core/cuda.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudawarping.hpp>

#include <onnxruntime_cxx_api.h>

#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>
#include <deque>
#include <vector>

namespace gstreamer_worker {

/**
 * @brief Callback for face detection events
 */
using FaceCallback = std::function<void(const FaceEvent& event)>;

/**
 * @brief Production-grade face detector with ONNX Runtime and TensorRT
 *
 * Features:
 * - SCRFD model (Sample and Computation Redistribution for Face Detection)
 * - TensorRT acceleration for maximum performance
 * - CUDA GPU processing with zero-copy design
 * - Multi-scale detection (8, 16, 32 strides)
 * - 5-point facial landmarks
 * - NMS (Non-Maximum Suppression) post-processing
 * - Thread-safe operation
 * - Comprehensive metrics
 */
class FaceDetector {
public:
    /**
     * @brief Constructor
     * @param camera_id Camera identifier
     * @param config Face detection configuration
     * @param on_face Callback for face detection events
     */
    FaceDetector(
        const std::string& camera_id,
        const FaceDetectionConfig& config,
        FaceCallback on_face = nullptr
    );

    /**
     * @brief Destructor
     */
    ~FaceDetector();

    // Delete copy/move constructors
    FaceDetector(const FaceDetector&) = delete;
    FaceDetector& operator=(const FaceDetector&) = delete;
    FaceDetector(FaceDetector&&) = delete;
    FaceDetector& operator=(FaceDetector&&) = delete;

    /**
     * @brief Process a video frame for face detection (CPU input)
     * @param frame Input frame (BGR format)
     * @return true if faces were detected in this frame
     */
    bool process_frame(const cv::Mat& frame);

    /**
     * @brief Process a video frame for face detection (GPU input - ZERO-COPY)
     * @param d_frame Input frame on GPU (CUDA GpuMat)
     * @return true if faces were detected in this frame
     *
     * This method processes frames that are already on the GPU, avoiding
     * CPU copies for maximum performance. Preprocessing is done on GPU,
     * only inference input/output and final results are CPU-side.
     */
    bool process_frame_cuda(const cv::cuda::GpuMat& d_frame);

    /**
     * @brief Update configuration at runtime
     * @param config New configuration
     */
    void update_config(const FaceDetectionConfig& config);

    /**
     * @brief Get current configuration
     */
    FaceDetectionConfig get_config() const;

    /**
     * @brief Reset the face detector (clears history)
     */
    void reset();

    /**
     * @brief Get detection statistics
     */
    struct Statistics {
        uint64_t frames_processed = 0;
        uint64_t frames_skipped = 0;
        uint64_t face_events = 0;
        uint64_t total_faces_detected = 0;
        double avg_processing_time_ms = 0.0;
        double avg_inference_time_ms = 0.0;
        double current_fps = 0.0;
        bool using_tensorrt = false;
        bool using_cuda = false;
    };

    Statistics get_statistics() const;

    /**
     * @brief Check if CUDA/TensorRT is available and being used
     */
    bool is_gpu_enabled() const { return stats_.using_tensorrt || stats_.using_cuda; }

    /**
     * @brief Get latest face detections for visualization
     * @return Vector of face detections from the most recent processed frame
     */
    std::vector<FaceDetection> get_latest_detections() const;

    /**
     * @brief Person tracking information (for analytics and cross-camera tracking)
     */
    struct PersonTrackingInfo {
        std::string subject;          // Person identity
        float recognition_confidence; // Recognition score
        bool is_recognized;           // Whether identity is confirmed
        double duration_seconds;      // How long tracked
        int frames_tracked;           // Number of frames
        FaceBoundingBox current_bbox; // Current position
    };

    /**
     * @brief Get all currently tracked persons with identity and duration
     * @return Vector of person tracking information
     *
     * Use this for:
     * - Analytics dashboards
     * - Cross-camera tracking
     * - Duration monitoring
     * - Identity reporting
     */
    std::vector<PersonTrackingInfo> get_tracked_persons() const;

private:
    // Configuration
    std::string camera_id_;
    FaceDetectionConfig config_;
    mutable std::mutex config_mutex_;
    FaceCallback on_face_;

    // CompreFace client for face recognition
    std::unique_ptr<CompreFaceClient> compreface_client_;

    // ONNX Runtime session
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::SessionOptions> session_options_;
    Ort::MemoryInfo memory_info_;

    // Model input/output info
    std::vector<Ort::AllocatedStringPtr> input_names_alloc_;
    std::vector<Ort::AllocatedStringPtr> output_names_alloc_;
    std::vector<const char*> input_names_;
    std::vector<const char*> output_names_;
    std::vector<int64_t> input_shape_;

    // GPU processing buffers
    cv::cuda::GpuMat d_working_;
    cv::cuda::GpuMat d_resized_;
    cv::cuda::GpuMat d_normalized_;

    // Processing state
    std::atomic<uint64_t> frame_counter_;
    std::atomic<bool> initialized_;

    // Face event tracking
    std::chrono::steady_clock::time_point last_event_time_;
    std::deque<bool> face_history_;  // Track faces over recent frames
    std::vector<FaceDetection> latest_detections_;  // Latest detections for visualization

    // Face tracking (to avoid re-detecting same faces)
    struct TrackedFace {
        FaceBoundingBox bbox;
        std::chrono::steady_clock::time_point first_seen;
        std::chrono::steady_clock::time_point last_seen;
        int frames_tracked = 0;

        // Identity information from CompreFace
        std::string subject = "unknown";      // Recognized person name/ID
        float recognition_confidence = 0.0f;  // Recognition similarity score
        bool is_recognized = false;           // Whether identity is confirmed
        std::string face_id;                  // CompreFace face ID
        bool crop_saved = false;              // Whether high-quality crop has been saved
        int recognition_attempts = 0;         // Number of recognition attempts (for retry logic)
        int failed_recognitions = 0;          // Number of failed recognition attempts

        // Duration tracking
        double get_duration_seconds() const {
            auto duration = std::chrono::duration<double>(last_seen - first_seen);
            return duration.count();
        }
    };
    std::vector<TrackedFace> tracked_faces_;
    double face_tracking_timeout_ = 15.0;  // Remove tracked faces after 15 seconds (increased)
    float face_tracking_iou_threshold_ = 0.35f;  // IoU threshold for matching faces (better separation)
    const int MAX_RECOGNITION_ATTEMPTS = 3;  // Max retry attempts per person

    mutable std::mutex face_mutex_;

    // Performance metrics
    mutable std::mutex stats_mutex_;
    Statistics stats_;
    std::chrono::steady_clock::time_point last_fps_update_;
    uint64_t fps_frame_count_;

    /**
     * @brief Initialize ONNX Runtime session with TensorRT/CUDA
     */
    void initialize_session();

    /**
     * @brief Preprocess frame for model input (GPU)
     * @param d_frame Input frame on GPU
     * @param output Output preprocessed tensor (CPU memory for ONNX Runtime)
     * @return true if preprocessing succeeded
     */
    bool preprocess_gpu(const cv::cuda::GpuMat& d_frame, std::vector<float>& output);

    /**
     * @brief Run inference with ONNX Runtime
     * @param input_tensor Input tensor
     * @param outputs Output tensors (9 outputs for SCRFD)
     * @return true if inference succeeded
     */
    bool run_inference(
        const std::vector<float>& input_tensor,
        std::vector<std::vector<float>>& outputs
    );

    /**
     * @brief Post-process model outputs to face detections
     * @param outputs Model output tensors
     * @param orig_width Original frame width
     * @param orig_height Original frame height
     * @return Vector of detected faces
     */
    std::vector<FaceDetection> postprocess(
        const std::vector<std::vector<float>>& outputs,
        int orig_width,
        int orig_height
    );

    /**
     * @brief Apply Non-Maximum Suppression
     * @param detections Input detections
     * @return Filtered detections after NMS
     */
    std::vector<FaceDetection> apply_nms(const std::vector<FaceDetection>& detections);

    /**
     * @brief Calculate IoU (Intersection over Union) between two boxes
     */
    float calculate_iou(const FaceBoundingBox& a, const FaceBoundingBox& b);

    /**
     * @brief Check if face event should be triggered
     * @param has_faces Whether current frame has faces
     * @return true if event should be triggered
     */
    bool should_trigger_event(bool has_faces);

    /**
     * @brief Update performance statistics
     * @param processing_time_ms Time taken to process frame
     * @param inference_time_ms Time taken for inference only
     */
    void update_statistics(double processing_time_ms, double inference_time_ms);

    /**
     * @brief Decode bounding boxes from model output
     * @param bbox_tensor Bounding box tensor from model
     * @param score_tensor Score tensor from model
     * @param stride Feature stride (8, 16, or 32)
     * @param orig_width Original frame width
     * @param orig_height Original frame height
     * @return Vector of detected faces for this scale
     */
    std::vector<FaceDetection> decode_bboxes(
        const std::vector<float>& bbox_tensor,
        const std::vector<float>& score_tensor,
        const std::vector<float>& kps_tensor,
        int stride,
        int orig_width,
        int orig_height
    );

    /**
     * @brief Apply ROI to frame (if configured)
     */
    cv::cuda::GpuMat apply_roi_cuda(const cv::cuda::GpuMat& d_frame) const;

    /**
     * @brief Save cropped face images with margin
     * @param frame Original frame (GPU)
     * @param detections Detected faces
     * @param event_timestamp Timestamp for filename
     *
     * Saves faces to: {save_path}/{camera_id}_{timestamp}_{face_num}_{confidence}.jpg
     * Format is compatible with CompreFace face recognition system
     */
    void save_face_crops(
        const cv::cuda::GpuMat& frame,
        const std::vector<FaceDetection>& detections,
        double event_timestamp
    );

    /**
     * @brief Filter detections to only include NEW faces (not currently tracked)
     * @param detections Input detections
     * @return Only new faces that aren't currently being tracked
     *
     * This prevents re-triggering events for the same person who's still in frame.
     * Uses IoU-based tracking with a 5-second timeout.
     */
    std::vector<FaceDetection> filter_tracked_faces(
        const std::vector<FaceDetection>& detections
    );

    /**
     * @brief Update face tracking with current detections
     * @param detections Current frame's detections
     *
     * Updates tracked faces, removes stale tracks, adds new faces.
     */
    void update_face_tracking(const std::vector<FaceDetection>& detections);

    /**
     * @brief Calculate blur score using Laplacian variance
     * @param face_crop Face image to analyze
     * @return Laplacian variance (higher = sharper image)
     *
     * Uses Laplacian operator to detect edges. Blurry images have low variance.
     * Typical thresholds: <50 very blurry, 50-100 slightly blurry, >100 sharp
     */
    double calculate_blur_score(const cv::Mat& face_crop);

    /**
     * @brief Check if a face is frontal and good quality
     * @param detection Face detection with landmarks
     * @return true if face is frontal, false if profile/partial/back-of-head
     *
     * Uses 5-point landmarks to determine face orientation:
     * - Both eyes visible and symmetric
     * - Eyes above nose (not inverted)
     * - Nose centered between eyes
     * - Face not at extreme angle (pitch/yaw)
     */
    bool is_frontal_face(const FaceDetection& detection);

    /**
     * @brief Handle CompreFace recognition results
     * @param camera_id Camera identifier
     * @param face_crop_path Path to face crop image
     * @param result Recognition result (if successful)
     * @param error Error message (if failed)
     *
     * Updates tracked faces with recognition information
     */
    void on_recognition_result(
        const std::string& camera_id,
        const std::string& face_crop_path,
        const std::optional<RecognitionResult>& result,
        const std::string& error
    );
};

} // namespace gstreamer_worker

#endif // GSTREAMER_WORKER_FACE_DETECTOR_H
