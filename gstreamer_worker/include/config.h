#ifndef GSTREAMER_WORKER_CONFIG_H
#define GSTREAMER_WORKER_CONFIG_H

#include <string>
#include <memory>

namespace gstreamer_worker {

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
