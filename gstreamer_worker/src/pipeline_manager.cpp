#include "pipeline_manager.h"
#include "logger.h"
#include "event_broadcaster.h"

#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>

namespace gstreamer_worker {

PipelineManager::PipelineManager(
    const PipelineManagerConfig& config,
    StateCallback on_state_changed,
    ErrorCallback on_error,
    MotionEventCallback on_motion,
    FaceEventCallback on_face,
    PersonTracker* person_tracker
)
    : config_(config)
    , on_state_changed_(std::move(on_state_changed))
    , on_error_(std::move(on_error))
    , on_motion_(std::move(on_motion))
    , on_face_(std::move(on_face))
    , person_tracker_(person_tracker)
    , running_(false)
{
    if (!config_.validate()) {
        LOG_ERROR(log_tag_, "Invalid configuration");
        throw std::invalid_argument("Invalid pipeline manager configuration");
    }

    // Set log level
    if (config_.log_level == "DEBUG") {
        Logger::instance().set_log_level(LogLevel::DEBUG);
    } else if (config_.log_level == "INFO") {
        Logger::instance().set_log_level(LogLevel::INFO);
    } else if (config_.log_level == "WARNING") {
        Logger::instance().set_log_level(LogLevel::WARNING);
    } else if (config_.log_level == "ERROR") {
        Logger::instance().set_log_level(LogLevel::ERROR);
    }

    // Initialize GStreamer
    gst_init(nullptr, nullptr);

    LOG_INFO(log_tag_, "Pipeline manager created");
}

PipelineManager::~PipelineManager() {
    LOG_INFO(log_tag_, "Destroying pipeline manager");
    stop_all(true);
}

bool PipelineManager::add_camera(const CameraConfig& config) {
    std::lock_guard<std::mutex> lock(streams_mutex_);

    if (streams_.find(config.camera_id) != streams_.end()) {
        LOG_WARNING(log_tag_, "Camera " + config.camera_id + " already exists");
        return false;
    }

    LOG_INFO(log_tag_, "Adding camera: " + config.camera_id);

    try {
        // Create camera stream with wrapped callbacks
        auto stream = std::make_unique<CameraStream>(
            config,
            [this](const std::string& camera_id, StreamState state) {
                handle_state_change(camera_id, state);
            },
            [this](const std::string& camera_id, const std::string& error) {
                handle_error(camera_id, error);
            },
            on_motion_,      // Pass motion callback directly
            on_face_,        // Pass face callback directly
            person_tracker_  // Pass person tracker for enterprise tracking
        );

        streams_[config.camera_id] = std::move(stream);

        // Auto-start if manager is already running
        if (running_) {
            streams_[config.camera_id]->start();
        }

        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(log_tag_, "Failed to add camera " + config.camera_id + ": " + e.what());
        return false;
    }
}

std::map<std::string, bool> PipelineManager::add_cameras(
    const std::vector<CameraConfig>& configs
) {
    std::map<std::string, bool> results;
    for (const auto& config : configs) {
        results[config.camera_id] = add_camera(config);
    }
    return results;
}

bool PipelineManager::remove_camera(const std::string& camera_id, bool wait) {
    std::lock_guard<std::mutex> lock(streams_mutex_);

    auto it = streams_.find(camera_id);
    if (it == streams_.end()) {
        LOG_WARNING(log_tag_, "Camera " + camera_id + " not found");
        return false;
    }

    LOG_INFO(log_tag_, "Removing camera: " + camera_id);

    it->second->stop(wait);
    streams_.erase(it);

    return true;
}

std::map<std::string, bool> PipelineManager::start_all() {
    if (running_.exchange(true)) {
        LOG_WARNING(log_tag_, "Manager already running");
        return {};
    }

    manager_start_time_ = std::chrono::steady_clock::now();

    LOG_INFO(log_tag_, "Starting " + std::to_string(get_camera_count()) + " camera streams");

    std::map<std::string, bool> results;

    {
        std::lock_guard<std::mutex> lock(streams_mutex_);
        for (auto& [camera_id, stream] : streams_) {
            results[camera_id] = stream->start();
        }
    }

    // Start metrics collection if enabled
    if (config_.enable_metrics) {
        start_metrics_collection();
    }

    int successful = 0;
    for (const auto& [camera_id, success] : results) {
        if (success) successful++;
    }

    LOG_INFO(log_tag_,
        "Started " + std::to_string(successful) + " out of " +
        std::to_string(results.size()) + " camera streams");

    return results;
}

void PipelineManager::stop_all(bool wait) {
    if (!running_.exchange(false)) {
        return;
    }

    LOG_INFO(log_tag_, "Stopping all camera streams");

    // Stop metrics collection
    if (metrics_timer_ && metrics_timer_->joinable()) {
        metrics_timer_->join();
    }

    // Stop all streams
    {
        std::lock_guard<std::mutex> lock(streams_mutex_);
        for (auto& [camera_id, stream] : streams_) {
            stream->stop(wait);
        }
    }

    LOG_INFO(log_tag_, "All camera streams stopped");
}

CameraStream* PipelineManager::get_camera(const std::string& camera_id) {
    std::lock_guard<std::mutex> lock(streams_mutex_);

    auto it = streams_.find(camera_id);
    if (it != streams_.end()) {
        return it->second.get();
    }

    return nullptr;
}

StreamState PipelineManager::get_camera_state(const std::string& camera_id) const {
    std::lock_guard<std::mutex> lock(streams_mutex_);

    auto it = streams_.find(camera_id);
    if (it != streams_.end()) {
        return it->second->get_state();
    }

    return StreamState::STOPPED;
}

std::map<std::string, StreamState> PipelineManager::get_all_states() const {
    std::lock_guard<std::mutex> lock(streams_mutex_);

    std::map<std::string, StreamState> states;
    for (const auto& [camera_id, stream] : streams_) {
        states[camera_id] = stream->get_state();
    }

    return states;
}

StreamMetrics PipelineManager::get_camera_metrics(const std::string& camera_id) const {
    std::lock_guard<std::mutex> lock(streams_mutex_);

    auto it = streams_.find(camera_id);
    if (it != streams_.end()) {
        return it->second->get_metrics();
    }

    return StreamMetrics();
}

std::map<std::string, StreamMetrics> PipelineManager::get_all_metrics() const {
    std::lock_guard<std::mutex> lock(streams_mutex_);

    std::map<std::string, StreamMetrics> metrics;
    for (const auto& [camera_id, stream] : streams_) {
        metrics[camera_id] = stream->get_metrics();
    }

    return metrics;
}

bool PipelineManager::is_running() const {
    return running_.load();
}

size_t PipelineManager::get_camera_count() const {
    std::lock_guard<std::mutex> lock(streams_mutex_);
    return streams_.size();
}

size_t PipelineManager::get_running_camera_count() const {
    std::lock_guard<std::mutex> lock(streams_mutex_);

    size_t count = 0;
    for (const auto& [camera_id, stream] : streams_) {
        if (stream->is_running()) {
            count++;
        }
    }

    return count;
}

void PipelineManager::set_event_broadcaster(std::shared_ptr<EventBroadcaster> broadcaster) {
    event_broadcaster_ = broadcaster;
    LOG_INFO(log_tag_, "Event broadcaster attached to PipelineManager");
}

void PipelineManager::handle_state_change(
    const std::string& camera_id,
    StreamState new_state
) {
    LOG_INFO(log_tag_,
        "Camera " + camera_id + " state changed to " +
        stream_state_to_string(new_state));

    // Emit WebSocket event
    if (event_broadcaster_) {
        std::string state_str = stream_state_to_string(new_state);

        if (new_state == StreamState::RUNNING) {
            auto stream = get_camera(camera_id);
            std::string rtsp_url = stream ? stream->get_config().rtsp_url : "";
            event_broadcaster_->emit_camera_started(camera_id, rtsp_url);
        }
        else if (new_state == StreamState::STOPPED) {
            event_broadcaster_->emit_camera_stopped(camera_id, "manual", 0.0);
        }
        else if (new_state == StreamState::RECONNECTING) {
            event_broadcaster_->emit_camera_reconnecting(camera_id, 1, 10);
        }
        else if (new_state == StreamState::ERROR) {
            event_broadcaster_->emit_camera_error(camera_id, "Stream error", "STREAM_ERROR");
        }
    }

    // Call user callback
    if (on_state_changed_) {
        try {
            on_state_changed_(camera_id, new_state);
        } catch (const std::exception& e) {
            LOG_ERROR(log_tag_,
                "Error in state change callback for " + camera_id + ": " + e.what());
        }
    }
}

void PipelineManager::handle_error(
    const std::string& camera_id,
    const std::string& error_msg
) {
    LOG_ERROR(log_tag_, "Camera " + camera_id + " error: " + error_msg);

    // Emit WebSocket event
    if (event_broadcaster_) {
        event_broadcaster_->emit_camera_error(camera_id, error_msg, "ERROR");
    }

    // Call user callback
    if (on_error_) {
        try {
            on_error_(camera_id, error_msg);
        } catch (const std::exception& e) {
            LOG_ERROR(log_tag_,
                "Error in error callback for " + camera_id + ": " + e.what());
        }
    }
}

void PipelineManager::start_metrics_collection() {
    if (!running_) {
        return;
    }

    metrics_timer_ = std::make_unique<std::thread>([this]() {
        while (running_) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(
                    static_cast<int>(config_.metrics_interval * 1000)
                )
            );

            if (running_) {
                collect_metrics();
            }
        }
    });
}

void PipelineManager::collect_metrics() {
    std::stringstream ss;
    ss << "\n========== Pipeline Manager Metrics ==========\n";

    // Manager info
    auto uptime = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - manager_start_time_
    ).count();

    ss << "Manager Uptime: " << uptime << " seconds\n";
    ss << "Total Cameras: " << get_camera_count() << "\n";
    ss << "Running Cameras: " << get_running_camera_count() << "\n";

    // Per-camera metrics
    auto all_metrics = get_all_metrics();
    auto all_states = get_all_states();

    uint64_t total_errors = 0;
    uint64_t total_reconnections = 0;
    uint64_t total_motion_events = 0;
    uint64_t total_frames_analyzed = 0;

    ss << "\nPer-Camera Status:\n";
    for (const auto& [camera_id, metrics] : all_metrics) {
        auto state = all_states[camera_id];

        ss << "  " << camera_id << ":\n";
        ss << "    State: " << stream_state_to_string(state) << "\n";
        ss << "    Uptime: " << metrics.uptime_seconds << "s\n";
        ss << "    Errors: " << metrics.errors_count << "\n";
        ss << "    Reconnections: " << metrics.reconnections << "\n";

        // Motion detection metrics
        if (metrics.frames_analyzed > 0) {
            ss << "    Motion Detection:\n";
            ss << "      Frames Analyzed: " << metrics.frames_analyzed << "\n";
            ss << "      Motion Events: " << metrics.motion_events_detected << "\n";
            ss << "      Detection FPS: " << std::fixed << std::setprecision(1)
               << metrics.motion_detection_fps << "\n";

            if (metrics.last_motion_timestamp > 0.0) {
                auto time_since_motion = std::chrono::duration<double>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count() - metrics.last_motion_timestamp;

                ss << "      Last Motion: " << std::fixed << std::setprecision(1)
                   << time_since_motion << "s ago\n";
            }
        }

        if (!metrics.last_error.empty()) {
            ss << "    Last Error: " << metrics.last_error << "\n";
        }

        total_errors += metrics.errors_count;
        total_reconnections += metrics.reconnections;
        total_motion_events += metrics.motion_events_detected;
        total_frames_analyzed += metrics.frames_analyzed;
    }

    ss << "\nGlobal Totals:\n";
    ss << "  Total Errors: " << total_errors << "\n";
    ss << "  Total Reconnections: " << total_reconnections << "\n";

    if (total_frames_analyzed > 0) {
        ss << "  Total Motion Events: " << total_motion_events << "\n";
        ss << "  Total Frames Analyzed: " << total_frames_analyzed << "\n";
    }

    ss << "==============================================\n";

    LOG_INFO(log_tag_, ss.str());
}

} // namespace gstreamer_worker
