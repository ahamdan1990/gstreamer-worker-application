#include "camera_stream.h"
#include "logger.h"

#include <sstream>
#include <chrono>
#include <thread>

namespace gstreamer_worker {

CameraStream::CameraStream(
    const CameraConfig& config,
    StateCallback on_state_changed,
    ErrorCallback on_error
)
    : config_(config)
    , log_tag_("CameraStream." + config.camera_id)
    , on_state_changed_(std::move(on_state_changed))
    , on_error_(std::move(on_error))
    , state_(StreamState::STOPPED)
    , running_(false)
    , reconnect_attempts_(0)
    , reconnect_delay_(config.reconnect_initial_delay)
    , consecutive_errors_(0)
{
    if (!config_.validate()) {
        LOG_ERROR(log_tag_, "Invalid configuration");
        throw std::invalid_argument("Invalid camera configuration");
    }

    LOG_INFO(log_tag_, "Camera stream created");
}

CameraStream::~CameraStream() {
    LOG_INFO(log_tag_, "Destroying camera stream");
    stop(true);
}

bool CameraStream::start() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (state_ != StreamState::STOPPED && state_ != StreamState::ERROR) {
        LOG_WARNING(log_tag_, "Cannot start: already in state " +
                    stream_state_to_string(state_));
        return false;
    }

    // Clean up old thread if it exists
    if (pipeline_thread_ && pipeline_thread_->joinable()) {
        LOG_WARNING(log_tag_, "Cleaning up previous pipeline thread");
        pipeline_thread_->join();
    }
    pipeline_thread_.reset();

    // Reset reconnection counters
    reconnect_attempts_ = 0;
    reconnect_delay_ = config_.reconnect_initial_delay;
    consecutive_errors_ = 0;

    set_state(StreamState::STARTING);
    running_ = true;

    // Start pipeline in separate thread
    pipeline_thread_ = std::make_unique<std::thread>(
        &CameraStream::run_pipeline, this
    );

    LOG_INFO(log_tag_, "Camera stream start initiated");
    return true;
}

void CameraStream::stop(bool wait) {
    bool already_stopped = false;

    {
        std::lock_guard<std::mutex> lock(state_mutex_);

        if (state_ == StreamState::STOPPED) {
            return;
        }

        // If already in ERROR state, just clean up threads
        if (state_ == StreamState::ERROR) {
            already_stopped = true;
        } else {
            LOG_INFO(log_tag_, "Stopping camera stream");
            set_state(StreamState::STOPPING);
            running_ = false;
        }
    }

    if (!already_stopped) {
        // Stop main loop first (signals pipeline thread to exit)
        if (loop_ && g_main_loop_is_running(loop_)) {
            g_main_loop_quit(loop_);
        }

        // Stop GStreamer pipeline
        if (pipeline_) {
            gst_element_set_state(pipeline_, GST_STATE_NULL);
        }
    }

    // Always wait for threads to finish if requested (even in ERROR state)
    if (wait) {
        if (pipeline_thread_ && pipeline_thread_->joinable()) {
            pipeline_thread_->join();
            pipeline_thread_.reset();
        }
        // Note: reconnect_timer_ and health_check_timer_ are detached, can't join
    }

    if (!already_stopped) {
        set_state(StreamState::STOPPED);
        LOG_INFO(log_tag_, "Camera stream stopped");
    }
}

StreamState CameraStream::get_state() const {
    return state_.load();
}

bool CameraStream::is_running() const {
    return state_.load() == StreamState::RUNNING;
}

StreamMetrics CameraStream::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    StreamMetrics metrics = metrics_;

    // Calculate uptime (only count when actually RUNNING)
    if (state_ == StreamState::RUNNING) {
        auto now = std::chrono::steady_clock::now();
        metrics.uptime_seconds = std::chrono::duration<double>(now - start_time_).count();
    } else {
        // Reset uptime when not running
        metrics.uptime_seconds = 0.0;
    }

    return metrics;
}

void CameraStream::run_pipeline() {
    LOG_INFO(log_tag_, "Pipeline thread started");

    try {
        // Main loop: create pipeline and run, reconnect on errors
        while (running_) {
            // Create pipeline
            if (!create_pipeline()) {
                LOG_ERROR(log_tag_, "Failed to create pipeline");

                // Check if we should retry
                if (config_.auto_reconnect && running_) {
                    if (config_.max_reconnect_attempts > 0 &&
                        reconnect_attempts_ >= config_.max_reconnect_attempts) {
                        LOG_ERROR(log_tag_, "Max reconnect attempts reached");
                        running_ = false;
                        set_state(StreamState::ERROR);
                        break;
                    }

                    reconnect_attempts_++;
                    set_state(StreamState::RECONNECTING);

                    // Calculate delay with exponential backoff
                    double delay = std::min(reconnect_delay_.load(), config_.reconnect_max_delay);
                    LOG_INFO(log_tag_, "Reconnecting in " + std::to_string(delay) + " seconds (attempt " +
                             std::to_string(reconnect_attempts_.load()) + ")");

                    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delay * 1000)));
                    reconnect_delay_ = reconnect_delay_.load() * config_.reconnect_backoff_multiplier;
                    continue;
                } else {
                    running_ = false;
                    set_state(StreamState::ERROR);
                    break;
                }
            }

            // Update metrics
            {
                std::lock_guard<std::mutex> lock(metrics_mutex_);
                start_time_ = std::chrono::steady_clock::now();
            }

            // Start health monitoring
            start_health_check();

            // Create and run main loop
            if (!loop_) {
                loop_ = g_main_loop_new(nullptr, FALSE);
            }

            LOG_INFO(log_tag_, "Starting GLib main loop");
            g_main_loop_run(loop_);
            LOG_INFO(log_tag_, "GLib main loop stopped");

            // Cleanup after loop exits
            cleanup_pipeline();

            // If we're still running and in RECONNECTING state, wait before retrying
            if (running_ && state_ == StreamState::RECONNECTING) {
                // Check max attempts
                if (config_.max_reconnect_attempts > 0 &&
                    reconnect_attempts_ >= config_.max_reconnect_attempts) {
                    LOG_ERROR(log_tag_, "Max reconnect attempts reached");
                    running_ = false;
                    set_state(StreamState::ERROR);
                    break;
                }

                // Calculate delay with exponential backoff
                double delay = std::min(reconnect_delay_.load(), config_.reconnect_max_delay);
                LOG_INFO(log_tag_, "Reconnecting in " + std::to_string(delay) + " seconds (attempt " +
                         std::to_string(reconnect_attempts_.load()) + ")");

                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delay * 1000)));
                reconnect_delay_ = reconnect_delay_.load() * config_.reconnect_backoff_multiplier;

                // Loop will retry
                continue;
            }

            // Otherwise (STOPPING/ERROR or not reconnecting), exit
            break;
        }

    } catch (const std::exception& e) {
        LOG_ERROR(log_tag_, std::string("Pipeline thread exception: ") + e.what());
        set_state(StreamState::ERROR);
    }

    // Final cleanup
    cleanup_pipeline();
}

bool CameraStream::create_pipeline() {
    LOG_INFO(log_tag_, "Creating GStreamer pipeline");

    try {
        // Build pipeline description
        std::string pipeline_desc = build_pipeline_description();
        LOG_DEBUG(log_tag_, "Pipeline: " + pipeline_desc);

        // Create pipeline
        GError* error = nullptr;
        pipeline_ = gst_parse_launch(pipeline_desc.c_str(), &error);

        if (error) {
            std::string error_msg = error->message;
            g_error_free(error);
            LOG_ERROR(log_tag_, "Failed to create pipeline: " + error_msg);
            return false;
        }

        if (!pipeline_) {
            LOG_ERROR(log_tag_, "Failed to create pipeline from description");
            return false;
        }

        // Get bus and add watch
        bus_ = gst_element_get_bus(pipeline_);
        gst_bus_add_watch(bus_, bus_message_callback, this);

        // Start pipeline
        GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            LOG_ERROR(log_tag_, "Unable to set pipeline to PLAYING state");
            return false;
        }

        set_state(StreamState::RUNNING);

        LOG_INFO(log_tag_, "Pipeline created and started successfully");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(log_tag_, std::string("Error creating pipeline: ") + e.what());
        return false;
    }
}

std::string CameraStream::build_pipeline_description() const {
    std::stringstream ss;

    std::string rtsp_location = config_.get_rtsp_location();

    // RTSP source
    ss << "rtspsrc location=\"" << rtsp_location << "\" "
       << "protocols=" << config_.protocols << " "
       << "latency=" << config_.latency_ms << " "
       << "drop-on-latency=" << (config_.drop_on_latency ? "true" : "false") << " "
       << "! rtph264depay ! h264parse";

    // Decoder
    if (config_.use_nvidia_decoder) {
        ss << " ! nvv4l2decoder";
    } else {
        ss << " ! avdec_h264";
    }

    // Video conversion and rate limiting
    if (config_.use_nvidia_decoder) {
        ss << " ! nvvideoconvert"
           << " ! videorate"
           << " ! video/x-raw(memory:NVMM),format=NV12,framerate=" << config_.target_fps << "/1";
    } else {
        ss << " ! videoconvert"
           << " ! videorate"
           << " ! video/x-raw,format=I420,framerate=" << config_.target_fps << "/1";
    }

    // Display
    if (config_.enable_display) {
        if (config_.use_nvidia_decoder) {
            ss << " ! nveglglessink sync=" << (config_.display_sync ? "true" : "false");
        } else {
            ss << " ! autovideosink sync=" << (config_.display_sync ? "true" : "false");
        }
    } else {
        ss << " ! fakesink sync=false";
    }

    return ss.str();
}

gboolean CameraStream::bus_message_callback(
    GstBus* bus,
    GstMessage* message,
    gpointer user_data
) {
    auto* stream = static_cast<CameraStream*>(user_data);
    stream->handle_bus_message(message);
    return TRUE;
}

void CameraStream::handle_bus_message(GstMessage* message) {
    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError* err = nullptr;
            gchar* debug_info = nullptr;
            gst_message_parse_error(message, &err, &debug_info);

            std::string error_msg = err->message;
            LOG_ERROR(log_tag_, "Pipeline error: " + error_msg);

            if (debug_info) {
                LOG_DEBUG(log_tag_, std::string("Debug info: ") + debug_info);
                g_free(debug_info);
            }
            g_error_free(err);

            handle_error("GStreamer error: " + error_msg);
            break;
        }

        case GST_MESSAGE_WARNING: {
            GError* err = nullptr;
            gchar* debug_info = nullptr;
            gst_message_parse_warning(message, &err, &debug_info);

            LOG_WARNING(log_tag_, std::string("Pipeline warning: ") + err->message);

            if (debug_info) {
                LOG_DEBUG(log_tag_, std::string("Debug info: ") + debug_info);
                g_free(debug_info);
            }
            g_error_free(err);
            break;
        }

        case GST_MESSAGE_EOS: {
            LOG_INFO(log_tag_, "End of stream");
            handle_error("End of stream reached");
            break;
        }

        case GST_MESSAGE_STATE_CHANGED: {
            if (GST_MESSAGE_SRC(message) == GST_OBJECT(pipeline_)) {
                GstState old_state, new_state, pending_state;
                gst_message_parse_state_changed(message, &old_state, &new_state, &pending_state);

                LOG_DEBUG(log_tag_,
                    std::string("Pipeline state: ") +
                    gst_element_state_get_name(old_state) + " -> " +
                    gst_element_state_get_name(new_state));
            }
            break;
        }

        case GST_MESSAGE_ELEMENT: {
            const GstStructure* structure = gst_message_get_structure(message);
            if (structure) {
                gchar* str = gst_structure_to_string(structure);
                LOG_DEBUG(log_tag_, std::string("Element message: ") + str);
                g_free(str);
            }
            break;
        }

        default:
            break;
    }
}

void CameraStream::handle_error(const std::string& error_msg) {
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.errors_count++;
        metrics_.last_error = error_msg;
    }

    consecutive_errors_++;

    // Call error callback
    if (on_error_) {
        on_error_(config_.camera_id, error_msg);
    }

    // Check if we should attempt reconnection
    if (config_.auto_reconnect && running_) {
        if (consecutive_errors_ >= config_.max_consecutive_errors) {
            LOG_ERROR(log_tag_,
                "Max consecutive errors (" + std::to_string(config_.max_consecutive_errors) +
                ") reached, stopping pipeline");
            running_ = false;
            set_state(StreamState::ERROR);

            // Quit the main loop to let thread exit cleanly
            if (loop_ && g_main_loop_is_running(loop_)) {
                g_main_loop_quit(loop_);
            }
        } else {
            // Signal main pipeline thread to reconnect by quitting the loop
            reconnect_attempts_++;
            set_state(StreamState::RECONNECTING);

            // Update metrics
            {
                std::lock_guard<std::mutex> lock(metrics_mutex_);
                metrics_.reconnections++;
            }

            // Quit the main loop so run_pipeline can handle reconnection
            if (loop_ && g_main_loop_is_running(loop_)) {
                g_main_loop_quit(loop_);
            }
        }
    } else {
        running_ = false;
        set_state(StreamState::ERROR);
        if (loop_ && g_main_loop_is_running(loop_)) {
            g_main_loop_quit(loop_);
        }
    }
}

// schedule_reconnect and attempt_reconnect removed - reconnection now handled in run_pipeline

void CameraStream::start_health_check() {
    if (!running_) {
        return;
    }

    health_check_timer_ = std::make_unique<std::thread>([this]() {
        while (running_) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(
                    static_cast<int>(config_.health_check_interval * 1000)
                )
            );

            if (running_) {
                check_health();
            }
        }
    });
    health_check_timer_->detach();
}

void CameraStream::check_health() {
    if (!running_ || state_ != StreamState::RUNNING) {
        return;
    }

    // Additional health checks can be added here
    // For example: check if frames are being rendered, memory usage, etc.
}

void CameraStream::cleanup_pipeline() {
    // Cleanup in correct order to avoid use-after-free

    // 1. Stop the GLib main loop if running
    if (loop_) {
        if (g_main_loop_is_running(loop_)) {
            g_main_loop_quit(loop_);
        }
    }

    // 2. Stop and cleanup GStreamer pipeline
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }

    // 3. Cleanup bus
    if (bus_) {
        gst_bus_remove_watch(bus_);
        gst_object_unref(bus_);
        bus_ = nullptr;
    }

    // 4. Finally unref the loop (after it's no longer running)
    if (loop_) {
        g_main_loop_unref(loop_);
        loop_ = nullptr;
    }
}

void CameraStream::set_state(StreamState new_state) {
    StreamState old_state = state_.exchange(new_state);

    if (old_state != new_state) {
        LOG_INFO(log_tag_,
            "State changed: " + stream_state_to_string(old_state) +
            " -> " + stream_state_to_string(new_state));

        if (on_state_changed_) {
            on_state_changed_(config_.camera_id, new_state);
        }
    }
}

} // namespace gstreamer_worker
