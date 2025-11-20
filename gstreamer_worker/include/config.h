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
