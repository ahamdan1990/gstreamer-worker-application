#ifndef GSTREAMER_WORKER_EVENT_BROADCASTER_H
#define GSTREAMER_WORKER_EVENT_BROADCASTER_H

#include <string>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

namespace gstreamer_worker {

class WebSocketServer;  // Forward declaration
struct PersonRecognitionEvent;  // Forward declaration

// Broadcast function type for WebSocket messages
using BroadcastFunc = std::function<void(const std::string&)>;

/**
 * @brief Event broadcaster for real-time notifications
 *
 * Broadcasts camera lifecycle events, motion detection events, and system status
 * to all connected WebSocket clients.
 */
class EventBroadcaster {
public:
    EventBroadcaster();
    ~EventBroadcaster();

    /**
     * @brief Set the WebSocket server instance
     * @param server Shared pointer to WebSocket server
     */
    void set_websocket_server(std::shared_ptr<WebSocketServer> server);

    /**
     * @brief Set custom broadcast function (alternative to WebSocketServer)
     * @param func Function to call for broadcasting messages
     */
    void set_broadcast_function(BroadcastFunc func);

    /**
     * @brief Emit a generic event
     * @param event_type Type of event (e.g., "camera_started", "motion_detected")
     * @param data Event data as JSON object
     * @param camera_id Optional camera ID (empty for system events)
     */
    void emit_event(
        const std::string& event_type,
        const nlohmann::json& data,
        const std::string& camera_id = ""
    );

    /**
     * @brief Emit worker started event
     */
    void emit_worker_started();

    /**
     * @brief Emit worker heartbeat event
     * @param total_cameras Total number of cameras
     * @param running_cameras Number of running cameras
     * @param uptime_seconds System uptime in seconds
     */
    void emit_worker_heartbeat(
        int total_cameras,
        int running_cameras,
        double uptime_seconds
    );

    /**
     * @brief Emit camera started event
     * @param camera_id Camera identifier
     * @param rtsp_url RTSP stream URL
     */
    void emit_camera_started(
        const std::string& camera_id,
        const std::string& rtsp_url
    );

    /**
     * @brief Emit camera stopped event
     * @param camera_id Camera identifier
     * @param reason Reason for stopping
     * @param uptime_seconds Camera uptime
     */
    void emit_camera_stopped(
        const std::string& camera_id,
        const std::string& reason,
        double uptime_seconds
    );

    /**
     * @brief Emit camera reconnecting event
     * @param camera_id Camera identifier
     * @param attempt Current reconnection attempt number
     * @param max_attempts Maximum reconnection attempts
     */
    void emit_camera_reconnecting(
        const std::string& camera_id,
        int attempt,
        int max_attempts
    );

    /**
     * @brief Emit camera error event
     * @param camera_id Camera identifier
     * @param error_message Error description
     * @param error_code Error code
     */
    void emit_camera_error(
        const std::string& camera_id,
        const std::string& error_message,
        const std::string& error_code
    );

    /**
     * @brief Emit camera status update event
     * @param camera_id Camera identifier
     * @param state Current state
     * @param metrics Performance metrics as JSON
     */
    void emit_camera_status(
        const std::string& camera_id,
        const std::string& state,
        const nlohmann::json& metrics
    );

    /**
     * @brief Emit motion detected event
     * @param camera_id Camera identifier
     * @param motion_area Motion area in pixels
     * @param num_contours Number of motion contours
     * @param confidence Detection confidence (0.0-1.0)
     */
    void emit_motion_detected(
        const std::string& camera_id,
        int motion_area,
        int num_contours,
        double confidence
    );

    /**
     * @brief Emit FPS drop event
     * @param camera_id Camera identifier
     * @param current_fps Current FPS
     * @param target_fps Target FPS
     */
    void emit_fps_drop(
        const std::string& camera_id,
        double current_fps,
        double target_fps
    );

    /**
     * @brief Emit pipeline crashed event
     * @param camera_id Camera identifier
     * @param error Error message
     */
    void emit_pipeline_crashed(
        const std::string& camera_id,
        const std::string& error
    );

    /**
     * @brief Emit pipeline recovered event
     * @param camera_id Camera identifier
     * @param recovery_time_seconds Time taken to recover
     */
    void emit_pipeline_recovered(
        const std::string& camera_id,
        double recovery_time_seconds
    );

    /**
     * @brief Emit person recognized event
     * @param event Person recognition event data
     */
    void emit_person_recognized(const PersonRecognitionEvent& event);

    /**
     * @brief Emit face detected event (unknown person)
     * @param camera_id Camera identifier
     * @param confidence Detection confidence
     * @param detection_count Number of detections
     * @param face_crop_paths Paths to saved face crop images
     */
    void emit_face_detected(
        const std::string& camera_id,
        float confidence,
        int detection_count,
        const std::vector<std::string>& face_crop_paths = {}
    );

    /**
     * @brief Emit log message for real-time monitoring
     * @param level Log level (DEBUG, INFO, WARNING, ERROR)
     * @param component Component name (e.g., "FaceDetector", "CameraStream")
     * @param message Log message
     * @param camera_id Optional camera ID
     */
    void emit_log(
        const std::string& level,
        const std::string& component,
        const std::string& message,
        const std::string& camera_id = ""
    );

    /**
     * @brief Check if WebSocket server is connected
     * @return true if server is set and has clients
     */
    bool is_connected() const;

private:
    std::shared_ptr<WebSocketServer> ws_server_;
    BroadcastFunc broadcast_func_;
    std::string log_tag_ = "[EventBroadcaster]";

    /**
     * @brief Build standard event JSON structure
     * @param event_type Event type
     * @param data Event data
     * @param camera_id Optional camera ID
     * @return JSON object with standard event structure
     */
    nlohmann::json build_event_json(
        const std::string& event_type,
        const nlohmann::json& data,
        const std::string& camera_id
    ) const;
};

} // namespace gstreamer_worker

#endif // GSTREAMER_WORKER_EVENT_BROADCASTER_H
