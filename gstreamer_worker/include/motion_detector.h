#ifndef GSTREAMER_WORKER_MOTION_DETECTOR_H
#define GSTREAMER_WORKER_MOTION_DETECTOR_H

#include "config.h"
#include "types.h"

#include <opencv2/opencv.hpp>
#include <opencv2/core/cuda.hpp>
#include <opencv2/cudabgsegm.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudawarping.hpp>
#include <opencv2/cudafilters.hpp>
#include <opencv2/cudaarithm.hpp>

#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>
#include <deque>

namespace gstreamer_worker {

/**
 * @brief Callback for motion detection events
 */
using MotionCallback = std::function<void(const MotionEvent& event)>;

/**
 * @brief Production-grade motion detector with CUDA acceleration
 *
 * Features:
 * - Multiple algorithms (MOG2, MOG2_CUDA, KNN, Frame Difference)
 * - CUDA acceleration for improved performance
 * - Configurable sensitivity and ROI
 * - Event debouncing and filtering
 * - Thread-safe operation
 * - Comprehensive metrics
 */
class MotionDetector {
public:
    /**
     * @brief Constructor
     * @param camera_id Camera identifier
     * @param config Motion detection configuration
     * @param on_motion Callback for motion events
     */
    MotionDetector(
        const std::string& camera_id,
        const MotionDetectionConfig& config,
        MotionCallback on_motion = nullptr
    );

    /**
     * @brief Destructor
     */
    ~MotionDetector();

    // Delete copy/move constructors
    MotionDetector(const MotionDetector&) = delete;
    MotionDetector& operator=(const MotionDetector&) = delete;
    MotionDetector(MotionDetector&&) = delete;
    MotionDetector& operator=(MotionDetector&&) = delete;

    /**
     * @brief Process a video frame for motion detection
     * @param frame Input frame (BGR format)
     * @return true if motion was detected in this frame
     */
    bool process_frame(const cv::Mat& frame);

    /**
     * @brief Process a video frame for motion detection (GPU input - ZERO-COPY)
     * @param d_frame Input frame on GPU (CUDA GpuMat)
     * @return true if motion was detected in this frame
     *
     * This method processes frames that are already on the GPU, avoiding
     * CPU copies for maximum performance. All processing is done on GPU
     * until contour analysis which downloads only the tiny foreground mask.
     */
    bool process_frame_cuda(const cv::cuda::GpuMat& d_frame);

    /**
     * @brief Update configuration at runtime
     * @param config New configuration
     */
    void update_config(const MotionDetectionConfig& config);

    /**
     * @brief Get current configuration
     */
    MotionDetectionConfig get_config() const;

    /**
     * @brief Reset the motion detector (clears background model)
     */
    void reset();

    /**
     * @brief Get detection statistics
     */
    struct Statistics {
        uint64_t frames_processed = 0;
        uint64_t frames_skipped = 0;
        uint64_t motion_events = 0;
        double avg_processing_time_ms = 0.0;
        double current_fps = 0.0;
        bool cuda_available = false;
        bool using_cuda = false;
    };

    Statistics get_statistics() const;

    /**
     * @brief Check if CUDA is available and being used
     */
    bool is_cuda_enabled() const { return cuda_available_ && using_cuda_; }

private:
    // Configuration
    std::string camera_id_;
    MotionDetectionConfig config_;
    mutable std::mutex config_mutex_;
    MotionCallback on_motion_;

    // Background subtraction
    cv::Ptr<cv::BackgroundSubtractor> bg_subtractor_;
    cv::Ptr<cv::cuda::BackgroundSubtractorMOG2> cuda_bg_subtractor_;

    // CUDA support
    bool cuda_available_;
    bool using_cuda_;
    cv::cuda::GpuMat d_frame_;
    cv::cuda::GpuMat d_gray_;
    cv::cuda::GpuMat d_resized_;
    cv::cuda::GpuMat d_fg_mask_;
    cv::cuda::GpuMat d_blurred_;

    // Cached CUDA Gaussian filter (avoid per-frame recreation)
    cv::Ptr<cv::cuda::Filter> gaussian_filter_;
    int gaussian_filter_kernel_size_ = 0;

    // Cached CUDA Morphology filters (CRITICAL FIX: Eliminate CPU-GPU round trips)
    cv::Ptr<cv::cuda::Filter> morph_open_filter_;
    cv::Ptr<cv::cuda::Filter> morph_close_filter_;
    cv::Mat morph_kernel_;

    // Frame differencing (for FRAME_DIFF algorithm)
    cv::Mat prev_frame_;
    cv::cuda::GpuMat d_prev_frame_;

    // Processing state
    std::atomic<uint64_t> frame_counter_;
    std::atomic<bool> initialized_;

    // Motion event tracking
    std::chrono::steady_clock::time_point last_event_time_;
    std::deque<bool> motion_history_;  // Track motion over recent frames
    std::mutex motion_mutex_;

    // Performance metrics
    mutable std::mutex stats_mutex_;
    Statistics stats_;
    std::chrono::steady_clock::time_point last_fps_update_;
    uint64_t fps_frame_count_;

    /**
     * @brief Initialize background subtractor based on algorithm
     */
    void initialize_bg_subtractor();

    /**
     * @brief Preprocess frame (resize, convert to grayscale, blur)
     * @param frame Input frame
     * @param processed Output processed frame
     * @return true if preprocessing succeeded
     */
    bool preprocess_frame(const cv::Mat& frame, cv::Mat& processed);

    /**
     * @brief Preprocess frame using CUDA
     */
    bool preprocess_frame_cuda(const cv::Mat& frame, cv::cuda::GpuMat& d_processed);

    /**
     * @brief Apply background subtraction
     */
    bool apply_background_subtraction(const cv::Mat& frame, cv::Mat& fg_mask);

    /**
     * @brief Apply background subtraction using CUDA
     */
    bool apply_background_subtraction_cuda(const cv::cuda::GpuMat& d_frame, cv::cuda::GpuMat& d_fg_mask);

    /**
     * @brief Apply frame differencing
     */
    bool apply_frame_difference(const cv::Mat& frame, cv::Mat& diff_mask);

    /**
     * @brief Apply frame differencing using CUDA
     */
    bool apply_frame_difference_cuda(const cv::cuda::GpuMat& d_frame, cv::cuda::GpuMat& d_diff_mask);

    /**
     * @brief Analyze foreground mask for motion
     * @param fg_mask Foreground mask
     * @param motion_area Output total motion area in pixels
     * @param bounding_box Output bounding box of motion
     * @return Number of significant contours found
     */
    int analyze_motion(const cv::Mat& fg_mask, int& motion_area, MotionROI& bounding_box);

    /**
     * @brief Check if motion event should be triggered
     * @param has_motion Whether current frame has motion
     * @return true if event should be triggered
     */
    bool should_trigger_event(bool has_motion);

    /**
     * @brief Update performance statistics
     * @param processing_time_ms Time taken to process frame
     */
    void update_statistics(double processing_time_ms);

    /**
     * @brief Apply ROI to frame
     */
    cv::Mat apply_roi(const cv::Mat& frame) const;

    /**
     * @brief Apply ROI to CUDA frame
     */
    cv::cuda::GpuMat apply_roi_cuda(const cv::cuda::GpuMat& d_frame) const;
};

} // namespace gstreamer_worker

#endif // GSTREAMER_WORKER_MOTION_DETECTOR_H
