#include "event_broadcaster.h"
#include "websocket_server.h"
#include "logger.h"
#include <chrono>
#include <unistd.h>

using json = nlohmann::json;

namespace gstreamer_worker {

EventBroadcaster::EventBroadcaster()
    : ws_server_(nullptr)
{
    LOG_INFO(log_tag_, "Event broadcaster initialized");
}

EventBroadcaster::~EventBroadcaster() {
    LOG_INFO(log_tag_, "Event broadcaster destroyed");
}

void EventBroadcaster::set_websocket_server(std::shared_ptr<WebSocketServer> server) {
    ws_server_ = server;
    LOG_INFO(log_tag_, "WebSocket server attached to broadcaster");
}

void EventBroadcaster::set_broadcast_function(BroadcastFunc func) {
    broadcast_func_ = func;
    LOG_INFO(log_tag_, "Custom broadcast function attached to broadcaster");
}

nlohmann::json EventBroadcaster::build_event_json(
    const std::string& event_type,
    const nlohmann::json& data,
    const std::string& camera_id
) const {
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count() / 1000.0;

    json event = {
        {"event_type", event_type},
        {"timestamp", timestamp},
        {"data", data}
    };

    if (!camera_id.empty()) {
        event["camera_id"] = camera_id;
    }

    return event;
}

void EventBroadcaster::emit_event(
    const std::string& event_type,
    const nlohmann::json& data,
    const std::string& camera_id
) {
    // Check if we have either a WebSocket server or broadcast function
    if (!ws_server_ && !broadcast_func_) {
        return;  // No broadcast mechanism attached, skip
    }

    json event = build_event_json(event_type, data, camera_id);
    std::string message = event.dump();

    // Broadcast using whichever mechanism is available
    if (broadcast_func_) {
        broadcast_func_(message);
    } else if (ws_server_) {
        ws_server_->broadcast(message);
    }

    LOG_DEBUG(log_tag_, "Emitted event: " + event_type +
              (camera_id.empty() ? "" : " (camera: " + camera_id + ")"));
}

void EventBroadcaster::emit_worker_started() {
    json data = {
        {"version", "1.0.0"},
        {"pid", getpid()}
    };

    emit_event("worker_started", data);
    LOG_INFO(log_tag_, "Worker started event emitted");
}

void EventBroadcaster::emit_worker_heartbeat(
    int total_cameras,
    int running_cameras,
    double uptime_seconds
) {
    json data = {
        {"uptime_seconds", uptime_seconds},
        {"total_cameras", total_cameras},
        {"running_cameras", running_cameras}
    };

    emit_event("worker_heartbeat", data);
}

void EventBroadcaster::emit_camera_started(
    const std::string& camera_id,
    const std::string& rtsp_url
) {
    json data = {
        {"state", "RUNNING"},
        {"rtsp_url", rtsp_url}
    };

    emit_event("camera_started", data, camera_id);
    LOG_INFO(log_tag_, "Camera started event emitted: " + camera_id);
}

void EventBroadcaster::emit_camera_stopped(
    const std::string& camera_id,
    const std::string& reason,
    double uptime_seconds
) {
    json data = {
        {"state", "STOPPED"},
        {"reason", reason},
        {"uptime_seconds", uptime_seconds}
    };

    emit_event("camera_stopped", data, camera_id);
    LOG_INFO(log_tag_, "Camera stopped event emitted: " + camera_id);
}

void EventBroadcaster::emit_camera_reconnecting(
    const std::string& camera_id,
    int attempt,
    int max_attempts
) {
    json data = {
        {"state", "RECONNECTING"},
        {"attempt", attempt},
        {"max_attempts", max_attempts}
    };

    emit_event("camera_reconnecting", data, camera_id);
    LOG_WARNING(log_tag_, "Camera reconnecting event emitted: " + camera_id +
                " (attempt " + std::to_string(attempt) + "/" + std::to_string(max_attempts) + ")");
}

void EventBroadcaster::emit_camera_error(
    const std::string& camera_id,
    const std::string& error_message,
    const std::string& error_code
) {
    json data = {
        {"state", "ERROR"},
        {"error_message", error_message},
        {"error_code", error_code}
    };

    emit_event("camera_error", data, camera_id);
    LOG_ERROR(log_tag_, "Camera error event emitted: " + camera_id + " - " + error_message);
}

void EventBroadcaster::emit_camera_status(
    const std::string& camera_id,
    const std::string& state,
    const nlohmann::json& metrics
) {
    json data = {
        {"state", state},
        {"metrics", metrics}
    };

    emit_event("camera_status", data, camera_id);
}

void EventBroadcaster::emit_motion_detected(
    const std::string& camera_id,
    int motion_area,
    int num_contours,
    double confidence
) {
    json data = {
        {"motion_area", motion_area},
        {"num_contours", num_contours},
        {"confidence", confidence}
    };

    emit_event("motion_detected", data, camera_id);
    LOG_INFO(log_tag_, "Motion detected event emitted: " + camera_id +
             " (area=" + std::to_string(motion_area) + "px, confidence=" + std::to_string(confidence) + ")");
}

void EventBroadcaster::emit_fps_drop(
    const std::string& camera_id,
    double current_fps,
    double target_fps
) {
    double drop_percentage = ((target_fps - current_fps) / target_fps) * 100.0;

    json data = {
        {"current_fps", current_fps},
        {"target_fps", target_fps},
        {"drop_percentage", drop_percentage}
    };

    emit_event("fps_drop", data, camera_id);
    LOG_WARNING(log_tag_, "FPS drop event emitted: " + camera_id +
                " (" + std::to_string(current_fps) + "/" + std::to_string(target_fps) + " FPS)");
}

void EventBroadcaster::emit_pipeline_crashed(
    const std::string& camera_id,
    const std::string& error
) {
    json data = {
        {"error", error},
        {"pipeline_state", "ERROR"}
    };

    emit_event("pipeline_crashed", data, camera_id);
    LOG_ERROR(log_tag_, "Pipeline crashed event emitted: " + camera_id + " - " + error);
}

void EventBroadcaster::emit_pipeline_recovered(
    const std::string& camera_id,
    double recovery_time_seconds
) {
    json data = {
        {"state", "RUNNING"},
        {"recovery_time_seconds", recovery_time_seconds}
    };

    emit_event("pipeline_recovered", data, camera_id);
    LOG_INFO(log_tag_, "Pipeline recovered event emitted: " + camera_id +
             " (recovery time: " + std::to_string(recovery_time_seconds) + "s)");
}

bool EventBroadcaster::is_connected() const {
    return ws_server_ && ws_server_->get_connected_clients() > 0;
}

} // namespace gstreamer_worker
