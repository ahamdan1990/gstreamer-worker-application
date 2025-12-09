#ifndef GSTREAMER_WORKER_CONFIG_H
#define GSTREAMER_WORKER_CONFIG_H

#include "types.h"
#include <string>
#include <memory>

namespace gstreamer_worker {

/**
 * @brief Motion detection configuration
 */
struct MotionDetectionConfig {
    // Enable/disable motion detection
    bool enabled = false;

    // Algorithm selection
    MotionAlgorithm algorithm = MotionAlgorithm::MOG2_CUDA;

    // Detection sensitivity
    double sensitivity = 0.5;  // 0.0 to 1.0, higher = more sensitive
    int min_contour_area = 500;  // Minimum area in pixels to consider as motion
    int max_contours = 50;  // Maximum number of contours to process

    // Frame processing
    int frame_skip = 2;  // Process every Nth frame (1 = every frame, 2 = every other frame)
    int blur_size = 21;  // Gaussian blur kernel size for noise reduction

    // Background subtraction parameters (for MOG2/KNN)
    int history = 500;  // Number of last frames that affect the background model
    double var_threshold = 16.0;  // Threshold on the squared Mahalanobis distance
    bool detect_shadows = false;  // Detect and mark shadows (slower but more accurate)

    // Region of Interest (optional - if width/height are 0, uses full frame)
    MotionROI roi;

    // Event debouncing
    double cooldown_seconds = 1.0;  // Minimum time between motion events
    int required_frames = 3;  // Number of consecutive frames with motion to trigger event

    // Performance
    int max_frame_width = 640;  // Resize frame to this width for analysis (0 = no resize)
    int max_frame_height = 480;  // Resize frame to this height for analysis (0 = no resize)

    /**
     * @brief Validate configuration
     */
    bool validate() const {
        if (sensitivity < 0.0 || sensitivity > 1.0) return false;
        if (min_contour_area < 0) return false;
        if (frame_skip < 1) return false;
        if (blur_size < 1 || blur_size % 2 == 0) return false;
        if (cooldown_seconds < 0.0) return false;
        if (required_frames < 1) return false;
        return true;
    }
};

/**
 * @brief Face detection configuration
 */
struct FaceDetectionConfig {
    // Enable/disable face detection
    bool enabled = false;

    // Model configuration
    std::string model_path = "models/scrfd/scrfd_10g_bnkps.onnx";
    int input_size = 640;  // Model input size (640x640)

    // Detection parameters
    float confidence_threshold = 0.5f;  // Minimum confidence to accept detection
    float nms_threshold = 0.4f;         // Non-Maximum Suppression IoU threshold

    // Processing optimization
    int frame_skip = 3;  // Process every Nth frame (higher = less frequent detection)
    int max_frame_width = 1280;   // Resize before detection (0 = no resize)
    int max_frame_height = 720;

    // Region of Interest (optional - if width/height are 0, uses full frame)
    MotionROI roi;

    // Event filtering
    float min_face_size = 0.02f;  // Minimum face size as % of frame (0.02 = 2%)
    int max_faces = 20;           // Maximum faces to detect per frame
    int required_frames = 2;      // Consecutive frames to trigger event
    double cooldown_seconds = 2.0;  // Minimum time between face events

    // Performance
    bool use_tensorrt = true;     // Use TensorRT execution provider
    bool use_cuda = true;          // Use CUDA execution provider (fallback from TensorRT)
    int max_batch_size = 1;        // Batch size for inference

    // Face cropping and saving (for verification and CompreFace integration)
    bool save_faces = false;                          // Enable saving cropped faces
    std::string save_path = "./face_crops";          // Directory to save face crops
    float save_margin = 0.3f;                        // Margin around face (0.3 = 30%)
    float min_save_confidence = 0.4f;                // Minimum confidence to save face
    int max_saves_per_event = 3;                     // Max faces to save per event

    // Face quality filtering (blur detection)
    bool enable_blur_detection = true;               // Enable blur detection for quality filtering
    float min_laplacian_variance = 100.0f;           // Minimum Laplacian variance (higher = sharper)
    int blur_kernel_size = 3;                        // Kernel size for blur detection

    // Face quality filtering (frontal face and size filters - NEW)
    bool enable_frontal_face_filter = true;          // Filter out non-frontal faces using landmarks
    float eye_tilt_threshold = 0.3f;                 // Max eye tilt angle (radians) for frontal faces
    float min_eye_distance = 0.01f;                  // Min normalized eye distance (relative to face width)
    float nose_center_offset_threshold = 0.3f;       // Max nose offset from face center (relative)
    float eye_to_face_ratio_min = 0.25f;             // Min eye distance to face width ratio
    float eye_to_face_ratio_max = 0.65f;             // Max eye distance to face width ratio
    bool enable_min_face_size_filter = true;         // Enable minimum face size filtering
    int min_face_width = 80;                         // Minimum face width in pixels
    int min_face_height = 80;                        // Minimum face height in pixels

    // Motion-triggered face detection (performance optimization)
    bool motion_triggered_detection = true;          // Only detect faces when motion is detected
    float motion_detection_cooldown = 2.0f;          // Continue detecting for N seconds after motion stops

    // CompreFace integration
    bool enable_compreface = false;                  // Enable face recognition via CompreFace
    std::string compreface_url = "http://localhost:8000";  // CompreFace API URL
    std::string compreface_api_key = "";             // CompreFace API key
    std::string compreface_subject = "unknown";      // Default subject name
    float compreface_similarity_threshold = 0.88f;   // Minimum similarity for match (0.0-1.0, default 0.88 = 88%)
    int compreface_timeout_ms = 5000;                // HTTP request timeout
    int compreface_max_queue_size = 100;             // Max faces in recognition queue

    // Live visualization settings
    bool enable_visualization = false;                // Draw bounding boxes on live feed
    bool draw_landmarks = true;                       // Draw facial landmarks
    bool draw_confidence = true;                      // Draw confidence scores
    int box_thickness = 2;                            // Bounding box line thickness
    float font_scale = 0.5f;                          // Text font scale

    /**
     * @brief Validate configuration
     */
    bool validate() const {
        if (confidence_threshold < 0.0f || confidence_threshold > 1.0f) return false;
        if (nms_threshold < 0.0f || nms_threshold > 1.0f) return false;
        if (frame_skip < 1) return false;
        if (min_face_size < 0.0f || min_face_size > 1.0f) return false;
        if (max_faces < 1) return false;
        if (required_frames < 1) return false;
        if (cooldown_seconds < 0.0) return false;
        if (input_size <= 0) return false;
        if (save_margin < 0.0f || save_margin > 2.0f) return false;
        if (min_save_confidence < 0.0f || min_save_confidence > 1.0f) return false;
        return true;
    }
};

/**
 * @brief Configuration for a single camera stream
 */
struct CameraConfig {
    // Identity
    std::string camera_id;
    std::string rtsp_url;
    std::string username;
    std::string password;

    // RTSP settings
    std::string protocols = "tcp";  // tcp, udp, or tcp+udp
    int latency_ms = 150;
    bool drop_on_latency = true;
    int target_fps = 12;

    // Display settings
    bool enable_display = true;
    bool display_sync = false;

    // Reconnection settings
    bool auto_reconnect = true;
    int max_reconnect_attempts = 15;  // Max attempts before giving up (use -1 for infinite)
    double reconnect_initial_delay = 2.0;  // seconds (increased from 1.0)
    double reconnect_max_delay = 60.0;     // seconds
    double reconnect_backoff_multiplier = 2.0;

    // Health monitoring
    double health_check_interval = 5.0;  // seconds
    int max_consecutive_errors = 5;

    // Hardware acceleration
    bool use_nvidia_decoder = true;

    // Motion detection
    MotionDetectionConfig motion_detection;

    // Face detection
    FaceDetectionConfig face_detection;

    /**
     * @brief Get RTSP URL with credentials if provided
     */
    std::string get_rtsp_location() const;

    /**
     * @brief Validate configuration
     */
    bool validate() const;
};

/**
 * @brief Global pipeline manager configuration
 */
struct PipelineManagerConfig {
    // Logging
    std::string log_level = "INFO";

    // Metrics
    bool enable_metrics = true;
    double metrics_interval = 30.0;  // seconds

    /**
     * @brief Validate configuration
     */
    bool validate() const;
};

} // namespace gstreamer_worker

#endif // GSTREAMER_WORKER_CONFIG_H
