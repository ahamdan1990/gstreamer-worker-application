#ifndef GSTREAMER_WORKER_CAMERA_STREAM_H
#define GSTREAMER_WORKER_CAMERA_STREAM_H

#include "config.h"
#include "types.h"
#include "person_tracker.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <cairo/cairo.h>
#include <opencv2/core.hpp>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <functional>

namespace gstreamer_worker {

// Forward declarations
class MotionDetector;
class FaceDetector;

/**
 * @brief Callback for state changes
 */
using StateCallback = std::function<void(const std::string& camera_id, StreamState state)>;

/**
 * @brief Callback for errors
 */
using ErrorCallback = std::function<void(const std::string& camera_id, const std::string& error)>;

/**
 * @brief Callback for motion events
 */
using MotionEventCallback = std::function<void(const MotionEvent& event)>;

/**
 * @brief Callback for face detection events
 */
using FaceEventCallback = std::function<void(const FaceEvent& event)>;

/**
 * @brief Individual camera stream handler with production-grade error handling
 *
 * Features:
 * - Automatic reconnection with exponential backoff
 * - State management and health monitoring
 * - Live video display
 * - Comprehensive error handling
 * - Thread-safe operations
 * - Metrics and statistics
 */
class CameraStream {
public:
    /**
     * @brief Constructor
     * @param config Camera configuration
     * @param on_state_changed Callback for state changes
     * @param on_error Callback for errors
     * @param on_motion Callback for motion events
     * @param on_face Callback for face detection events
     * @param person_tracker Optional person tracker for enterprise tracking
     */
    CameraStream(
        const CameraConfig& config,
        StateCallback on_state_changed = nullptr,
        ErrorCallback on_error = nullptr,
        MotionEventCallback on_motion = nullptr,
        FaceEventCallback on_face = nullptr,
        PersonTracker* person_tracker = nullptr
    );

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~CameraStream();

    // Delete copy/move constructors and assignment operators
    CameraStream(const CameraStream&) = delete;
    CameraStream& operator=(const CameraStream&) = delete;
    CameraStream(CameraStream&&) = delete;
    CameraStream& operator=(CameraStream&&) = delete;

    /**
     * @brief Start the camera stream
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the camera stream
     * @param wait If true, wait for pipeline thread to finish
     */
    void stop(bool wait = true);

    /**
     * @brief Get current stream state
     */
    StreamState get_state() const;

    /**
     * @brief Check if stream is running
     */
    bool is_running() const;

    /**
     * @brief Get stream metrics
     */
    StreamMetrics get_metrics() const;

    /**
     * @brief Get camera ID
     */
    std::string get_camera_id() const { return config_.camera_id; }

    /**
     * @brief Get camera configuration
     */
    const CameraConfig& get_config() const { return config_; }

    /**
     * @brief Enable or disable motion detection at runtime
     */
    void enable_motion_detection(bool enable);

    /**
     * @brief Check if motion detection is enabled
     */
    bool is_motion_detection_enabled() const;

    /**
     * @brief Update motion detection configuration
     */
    void update_motion_config(const MotionDetectionConfig& config);

    /**
     * @brief Enable or disable face detection at runtime
     */
    void enable_face_detection(bool enable);

    /**
     * @brief Check if face detection is enabled
     */
    bool is_face_detection_enabled() const;

    /**
     * @brief Update face detection configuration
     */
    void update_face_config(const FaceDetectionConfig& config);

private:
    // Configuration
    CameraConfig config_;
    std::string log_tag_;

    // Callbacks
    StateCallback on_state_changed_;
    ErrorCallback on_error_;
    MotionEventCallback on_motion_;
    FaceEventCallback on_face_;

    // GStreamer components
    GstElement* pipeline_ = nullptr;
    GstElement* appsink_ = nullptr;  // For motion detection frame capture
    GstElement* viz_appsink_ = nullptr;  // For visualization overlay (legacy)
    GstElement* cairo_overlay_ = nullptr;  // For drawing face bboxes on live feed
    GstBus* bus_ = nullptr;
    GMainLoop* loop_ = nullptr;

    // Motion detection
    std::unique_ptr<MotionDetector> motion_detector_;
    std::atomic<bool> motion_detection_enabled_;

    // Face detection
    std::unique_ptr<FaceDetector> face_detector_;
    std::atomic<bool> face_detection_enabled_;

    // Visualization
    std::mutex viz_mutex_;
    std::vector<FaceDetection> latest_detections_;
    int viz_frame_width_ = 0;
    int viz_frame_height_ = 0;

    // Motion-triggered face detection
    std::atomic<bool> motion_recently_detected_{false};
    std::chrono::steady_clock::time_point last_motion_time_;
    std::mutex motion_mutex_;

    // State management
    std::atomic<StreamState> state_;
    mutable std::mutex state_mutex_;
    std::unique_ptr<std::thread> pipeline_thread_;
    std::atomic<bool> running_;

    // Reconnection state
    std::atomic<int> reconnect_attempts_;
    std::atomic<double> reconnect_delay_;
    std::unique_ptr<std::thread> reconnect_timer_;

    // Health monitoring
    std::chrono::steady_clock::time_point last_frame_time_;
    std::atomic<int> consecutive_errors_;
    std::unique_ptr<std::thread> health_check_timer_;

    // Metrics
    mutable std::mutex metrics_mutex_;
    StreamMetrics metrics_;
    std::chrono::steady_clock::time_point start_time_;

    /**
     * @brief Main pipeline execution loop (runs in separate thread)
     */
    void run_pipeline();

    /**
     * @brief Create the GStreamer pipeline
     */
    bool create_pipeline();

    /**
     * @brief Build GStreamer pipeline description string
     */
    std::string build_pipeline_description() const;

    /**
     * @brief GStreamer bus message callback
     */
    static gboolean bus_message_callback(GstBus* bus, GstMessage* message, gpointer user_data);

    /**
     * @brief Handle GStreamer bus message
     */
    void handle_bus_message(GstMessage* message);

    /**
     * @brief Handle pipeline errors
     */
    void handle_error(const std::string& error_msg);

    /**
     * @brief Start periodic health check
     */
    void start_health_check();

    /**
     * @brief Check pipeline health
     */
    void check_health();

    /**
     * @brief Clean up pipeline resources
     */
    void cleanup_pipeline();

    /**
     * @brief Set stream state and trigger callback
     */
    void set_state(StreamState new_state);

    /**
     * @brief Process frame for motion detection
     */
    void process_motion_frame(GstSample* sample);

    /**
     * @brief Process frame for visualization overlay
     */
    void process_visualization_frame(GstSample* sample);

    /**
     * @brief Draw face detections on frame
     */
    void draw_face_detections(cv::Mat& frame, const std::vector<FaceDetection>& detections);

    /**
     * @brief Draw face detections using Cairo on live video feed
     */
    void draw_cairo_overlay(cairo_t* cr);
};

} // namespace gstreamer_worker

#endif // GSTREAMER_WORKER_CAMERA_STREAM_H
