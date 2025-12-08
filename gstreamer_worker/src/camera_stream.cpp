#include "camera_stream.h"
#include "motion_detector.h"
#include "face_detector.h"
#include "logger.h"
#include "callback_queue.h"

#include <gst/app/gstappsink.h>
#include <opencv2/opencv.hpp>

#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cmath>

namespace gstreamer_worker {

CameraStream::CameraStream(
    const CameraConfig& config,
    StateCallback on_state_changed,
    ErrorCallback on_error,
    MotionEventCallback on_motion,
    FaceEventCallback on_face,
    PersonTracker* person_tracker
)
    : config_(config)
    , log_tag_("CameraStream." + config.camera_id)
    , on_state_changed_(std::move(on_state_changed))
    , on_error_(std::move(on_error))
    , on_motion_(std::move(on_motion))
    , on_face_(std::move(on_face))
    , motion_detection_enabled_(false)
    , face_detection_enabled_(false)
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

    // Initialize callback queue for thread-safe GStreamer callbacks
    callback_queue_ = std::make_unique<CallbackQueue>();

    // Initialize motion detector if enabled
    if (config_.motion_detection.enabled) {
        try {
            motion_detector_ = std::make_unique<MotionDetector>(
                config_.camera_id,
                config_.motion_detection,
                [this](const MotionEvent& event) {
                    // Motion event callback
                    {
                        std::lock_guard<std::mutex> lock(metrics_mutex_);
                        metrics_.motion_events_detected++;
                        metrics_.last_motion_timestamp = event.timestamp;
                    }

                    // Forward to user callback
                    if (on_motion_) {
                        on_motion_(event);
                    }
                }
            );
            motion_detection_enabled_ = true;
            LOG_INFO(log_tag_, "Motion detection initialized");
        } catch (const std::exception& e) {
            LOG_ERROR(log_tag_, std::string("Failed to initialize motion detection: ") + e.what());
        }
    }

    // Initialize face detector if enabled
    if (config_.face_detection.enabled) {
        try {
            face_detector_ = std::make_unique<FaceDetector>(
                config_.camera_id,
                config_.face_detection,
                [this](const FaceEvent& event) {
                    // Face detection event callback
                    // Forward to user callback
                    if (on_face_) {
                        on_face_(event);
                    }
                },
                person_tracker  // Pass PersonTracker for enterprise tracking
            );
            face_detection_enabled_ = true;
            LOG_INFO(log_tag_, "Face detection initialized");
        } catch (const std::exception& e) {
            LOG_ERROR(log_tag_, std::string("Failed to initialize face detection: ") + e.what());
        }
    }

    LOG_INFO(log_tag_, "Camera stream created");
}

CameraStream::~CameraStream() {
    LOG_INFO(log_tag_, "Destroying camera stream");
    stop(true);

    // Stop and cleanup timers
    stop_reconnect_timer();
    stop_health_check_timer();

    // Clear callback queue
    if (callback_queue_) {
        callback_queue_->clear();
    }
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
        // Clear callback queue to prevent processing during shutdown
        if (callback_queue_) {
            callback_queue_->clear();
        }

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
        // Stop timer threads BEFORE joining pipeline thread
        stop_reconnect_timer();
        stop_health_check_timer();

        if (pipeline_thread_ && pipeline_thread_->joinable()) {
            pipeline_thread_->join();
            pipeline_thread_.reset();
        }
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

                    // Calculate delay with exponential backoff (CRITICAL FIX Issue #13: Cap to prevent overflow)
                    double current_delay = reconnect_delay_.load();
                    double delay = std::min(current_delay, config_.reconnect_max_delay);
                    LOG_INFO(log_tag_, "Reconnecting in " + std::to_string(delay) + " seconds (attempt " +
                             std::to_string(reconnect_attempts_.load()) + ")");

                    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delay * 1000)));

                    // Update delay for next attempt, but cap it
                    double next_delay = current_delay * config_.reconnect_backoff_multiplier;
                    reconnect_delay_.store(std::min(next_delay, config_.reconnect_max_delay));
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

            // CRITICAL FIX (Issue #6): Properly manage GMainLoop lifecycle
            // Ensure no old loop exists before creating new one
            if (loop_) {
                if (g_main_loop_is_running(loop_)) {
                    LOG_WARNING(log_tag_, "Old loop still running, stopping it first");
                    g_main_loop_quit(loop_);
                    // Wait a bit for it to actually quit
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                g_main_loop_unref(loop_);
                loop_ = nullptr;
            }

            // Now safe to create new loop
            loop_ = g_main_loop_new(nullptr, FALSE);

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

                // Calculate delay with exponential backoff (CRITICAL FIX Issue #13: Cap to prevent overflow)
                double current_delay = reconnect_delay_.load();
                double delay = std::min(current_delay, config_.reconnect_max_delay);

                // Cap the exponential growth to prevent overflow/NaN
                if (current_delay > config_.reconnect_max_delay) {
                    reconnect_delay_.store(config_.reconnect_max_delay);
                }
                LOG_INFO(log_tag_, "Reconnecting in " + std::to_string(delay) + " seconds (attempt " +
                         std::to_string(reconnect_attempts_.load()) + ")");

                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delay * 1000)));

                // Update delay for next attempt, but cap it (already done above, remove duplicate)
                // reconnect_delay_ already updated above

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
        LOG_INFO(log_tag_, "Pipeline: " + pipeline_desc);

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

        // Set up appsink if motion detection is enabled
        if (config_.motion_detection.enabled && motion_detector_) {
            appsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), "motion_sink");
            if (appsink_) {
                // Set appsink properties (CRITICAL FIX Issue #15/#16: Increase buffer size)
                g_object_set(G_OBJECT(appsink_),
                           "emit-signals", TRUE,   // Use signals (more stable than callbacks)
                           "sync", FALSE,          // Don't sync with clock
                           "drop", TRUE,           // Drop frames if processing is slow
                           "max-buffers", 20,      // Increased from 5 to 20 to prevent freeze with motion+face detection
                           nullptr);

                // Connect to new-sample signal - QUEUE WORK, DON'T EXECUTE DIRECTLY
                g_signal_connect(appsink_, "new-sample",
                               G_CALLBACK(+[](GstElement* sink, gpointer user_data) -> GstFlowReturn {
                                   auto* stream = static_cast<CameraStream*>(user_data);
                                   if (!stream || !stream->motion_detection_enabled_.load()) {
                                       return GST_FLOW_OK;
                                   }

                                   // Pull sample but DON'T process here - queue it for safe processing
                                   GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
                                   if (sample && stream->callback_queue_) {
                                       // Queue the sample for processing in the pipeline thread
                                       stream->callback_queue_->enqueue_sample(sample,
                                           [stream](GstSample* s) {
                                               stream->process_motion_frame(s);
                                           });
                                       gst_sample_unref(sample);  // Release our reference, queue has its own
                                   }
                                   return GST_FLOW_OK;
                               }), this);

                LOG_INFO(log_tag_, "Motion detection appsink configured");
            } else {
                LOG_WARNING(log_tag_, "Could not find motion_sink element");
            }
        }

        // Set up visualization appsink if face visualization is enabled
        LOG_INFO(log_tag_, "Checking viz: face_detection.enabled=" +
                std::to_string(config_.face_detection.enabled) +
                ", enable_visualization=" +
                std::to_string(config_.face_detection.enable_visualization));

        if (config_.face_detection.enabled && config_.face_detection.enable_visualization) {
            LOG_INFO(log_tag_, "Looking for viz_sink element in pipeline");
            viz_appsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), "viz_sink");
            if (viz_appsink_) {
                // Set appsink properties (CRITICAL FIX Issue #15: Increase buffer size)
                g_object_set(G_OBJECT(viz_appsink_),
                           "emit-signals", TRUE,
                           "sync", FALSE,
                           "drop", TRUE,
                           "max-buffers", 5,       // Increased from 2 to 5 for better throughput
                           nullptr);

                // Connect to new-sample signal - QUEUE WORK, DON'T EXECUTE DIRECTLY
                g_signal_connect(viz_appsink_, "new-sample",
                               G_CALLBACK(+[](GstElement* sink, gpointer user_data) -> GstFlowReturn {
                                   auto* stream = static_cast<CameraStream*>(user_data);
                                   if (!stream || !stream->face_detection_enabled_.load()) {
                                       return GST_FLOW_OK;
                                   }

                                   // Pull sample but DON'T process here - queue it for safe processing
                                   GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
                                   if (sample && stream->callback_queue_) {
                                       // Queue the sample for processing in the pipeline thread
                                       stream->callback_queue_->enqueue_sample(sample,
                                           [stream](GstSample* s) {
                                               stream->process_visualization_frame(s);
                                           });
                                       gst_sample_unref(sample);  // Release our reference, queue has its own
                                   }
                                   return GST_FLOW_OK;
                               }), this);

                LOG_INFO(log_tag_, "Face visualization appsink configured");
            } else {
                LOG_WARNING(log_tag_, "Could not find viz_sink element");
            }

            // Set up cairo overlay for drawing on live feed
            cairo_overlay_ = gst_bin_get_by_name(GST_BIN(pipeline_), "face_overlay");
            if (cairo_overlay_) {
                // Connect to draw signal
                g_signal_connect(cairo_overlay_, "draw",
                               G_CALLBACK(+[](GstElement* overlay, cairo_t* cr,
                                            guint64 timestamp, guint64 duration,
                                            gpointer user_data) {
                                   auto* stream = static_cast<CameraStream*>(user_data);
                                   if (stream) {
                                       stream->draw_cairo_overlay(cr);
                                   }
                               }), this);
                LOG_INFO(log_tag_, "Cairo overlay configured for live face detection");
            } else {
                LOG_WARNING(log_tag_, "Could not find face_overlay element");
            }
        }

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

    // Add tee if motion detection is enabled
    if (config_.motion_detection.enabled) {
        ss << " ! tee name=t";

        // Motion detection branch - convert to BGR for OpenCV
        // Use appropriate converter based on decoder type
        if (config_.use_nvidia_decoder) {
            // For NVIDIA: nvvideoconvert can handle NVMM → system memory + format conversion
            ss << " t. ! queue max-size-buffers=2 leaky=downstream ! nvvideoconvert"
               << " ! video/x-raw,format=BGR ! appsink name=motion_sink max-buffers=2 drop=true";
        } else {
            // For CPU: use standard videoconvert
            ss << " t. ! queue max-size-buffers=2 leaky=downstream ! videoconvert"
               << " ! video/x-raw,format=BGR ! appsink name=motion_sink max-buffers=2 drop=true";
        }

        // Visualization branch - if face detection visualization is enabled
        if (config_.face_detection.enabled && config_.face_detection.enable_visualization) {
            if (config_.use_nvidia_decoder) {
                ss << " t. ! queue max-size-buffers=2 leaky=downstream ! nvvideoconvert"
                   << " ! video/x-raw,format=BGR ! appsink name=viz_sink max-buffers=2 drop=true";
            } else {
                ss << " t. ! queue max-size-buffers=2 leaky=downstream ! videoconvert"
                   << " ! video/x-raw,format=BGR ! appsink name=viz_sink max-buffers=2 drop=true";
            }
        }

        // Display/output branch
        ss << " t. ! queue max-size-buffers=10 leaky=downstream";

        // Add cairooverlay for face detection visualization
        if (config_.face_detection.enabled && config_.face_detection.enable_visualization) {
            if (config_.use_nvidia_decoder) {
                // For NVIDIA: convert NVMM to system memory, then to BGRA for Cairo
                ss << " ! nvvideoconvert ! video/x-raw ! videoconvert"
                   << " ! video/x-raw,format=BGRA ! cairooverlay name=face_overlay"
                   << " ! videoconvert ! nvvideoconvert";
            } else {
                // For CPU: convert to BGRA for Cairo
                ss << " ! videoconvert ! video/x-raw,format=BGRA ! cairooverlay name=face_overlay"
                   << " ! videoconvert";
            }
        }

        if (config_.enable_display) {
            if (config_.use_nvidia_decoder) {
                ss << " ! nveglglessink sync=" << (config_.display_sync ? "true" : "false");
            } else {
                ss << " ! autovideosink sync=" << (config_.display_sync ? "true" : "false");
            }
        } else {
            ss << " ! fakesink sync=false";
        }
    } else {
        // No motion detection - direct to display
        if (config_.enable_display) {
            if (config_.use_nvidia_decoder) {
                ss << " ! nveglglessink sync=" << (config_.display_sync ? "true" : "false");
            } else {
                ss << " ! autovideosink sync=" << (config_.display_sync ? "true" : "false");
            }
        } else {
            ss << " ! fakesink sync=false";
        }
    }

    return ss.str();
}

gboolean CameraStream::bus_message_callback(
    GstBus* /* bus */,
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
    if (!running_ || health_check_timer_running_.load()) {
        return;  // Don't start if already running
    }

    health_check_timer_running_.store(true);
    health_check_timer_ = std::make_unique<std::thread>([this]() {
        LOG_INFO(log_tag_, "Health check thread started - processing callbacks every 10ms");
        int health_check_counter = 0;
        while (health_check_timer_running_.load() && running_.load()) {
            // CRITICAL FIX: Process callbacks very frequently (every 10ms for responsive processing)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            process_pending_callbacks();

            // Do health check less frequently (every ~500ms)
            health_check_counter++;
            if (health_check_counter >= 50) {  // 50 * 10ms = 500ms
                if (health_check_timer_running_.load() && running_.load()) {
                    check_health();
                }
                health_check_counter = 0;
            }
        }
        LOG_INFO(log_tag_, "Health check thread stopped");
    });
    // DO NOT detach - we'll join it in destructor
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

    // 2. Clean up appsinks
    if (appsink_) {
        gst_object_unref(appsink_);
        appsink_ = nullptr;
    }

    if (viz_appsink_) {
        gst_object_unref(viz_appsink_);
        viz_appsink_ = nullptr;
    }

    if (cairo_overlay_) {
        gst_object_unref(cairo_overlay_);
        cairo_overlay_ = nullptr;
    }

    // 3. Stop and cleanup GStreamer pipeline
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }

    // 4. Cleanup bus
    if (bus_) {
        gst_bus_remove_watch(bus_);
        gst_object_unref(bus_);
        bus_ = nullptr;
    }

    // 5. Finally unref the loop (after it's no longer running)
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

void CameraStream::process_motion_frame(GstSample* sample) {
    if (!motion_detector_ || !motion_detection_enabled_.load()) {
        return;
    }

    try {
        // Get buffer from sample
        GstBuffer* buffer = gst_sample_get_buffer(sample);
        if (!buffer) {
            return;
        }

        // Get caps to determine frame dimensions
        GstCaps* caps = gst_sample_get_caps(sample);
        if (!caps) {
            return;
        }

        GstStructure* structure = gst_caps_get_structure(caps, 0);
        int width, height;
        if (!gst_structure_get_int(structure, "width", &width) ||
            !gst_structure_get_int(structure, "height", &height)) {
            return;
        }

        // Map buffer to access data (READ-ONLY)
        GstMapInfo map;
        if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            return;
        }

        // Create OpenCV Mat view of buffer data (BGR format)
        cv::Mat frame_view(height, width, CV_8UC3, map.data);

        // NOTE: Drawing bounding boxes on the appsink buffer causes pipeline issues.
        // The appsink is for analysis only. To add visual overlays to the live video,
        // we would need to use GStreamer overlay elements (cairooverlay, textoverlay, etc.)
        // or implement a custom GStreamer element that draws on the display branch.

        // ZERO-COPY GPU UPLOAD: Upload once to GPU, reuse buffer across frames
        // Thread-local ensures each camera thread has its own GPU buffer (no races)
        // This eliminates the 6.2MB CPU copy and enables full GPU processing
        static thread_local cv::cuda::GpuMat d_frame;
        d_frame.upload(frame_view);  // Single upload to GPU

        // Unmap buffer immediately after upload (pipeline no longer blocked)
        gst_buffer_unmap(buffer, &map);

        // Process motion detection on GPU - ZERO CPU COPIES until final contour analysis!
        bool motion_detected = motion_detector_->process_frame_cuda(d_frame);

        // Update motion state for motion-triggered face detection
        if (motion_detected) {
            std::lock_guard<std::mutex> lock(motion_mutex_);
            last_motion_time_ = std::chrono::steady_clock::now();
            motion_recently_detected_.store(true);
        }

        // Process face detection on GPU - REUSES the same d_frame buffer (zero-copy!)
        // Only run face detection if:
        // 1. Face detection is enabled
        // 2. Motion-triggered mode is disabled OR motion was detected recently
        bool should_detect_faces = false;
        if (face_detector_ && face_detection_enabled_.load()) {
            if (config_.face_detection.motion_triggered_detection) {
                // Check if motion was detected recently (within cooldown period)
                std::lock_guard<std::mutex> lock(motion_mutex_);
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration<double>(now - last_motion_time_).count();
                should_detect_faces = (elapsed <= config_.face_detection.motion_detection_cooldown);

                if (!should_detect_faces && motion_recently_detected_.load()) {
                    // Motion cooldown expired
                    motion_recently_detected_.store(false);
                }
            } else {
                // Motion-triggered mode disabled, always detect
                should_detect_faces = true;
            }

            if (should_detect_faces) {
                face_detector_->process_frame_cuda(d_frame);

                // Store latest detections for visualization
                if (config_.face_detection.enable_visualization) {
                    auto detections = face_detector_->get_latest_detections();
                    std::lock_guard<std::mutex> lock(viz_mutex_);
                    latest_detections_ = detections;
                    viz_frame_width_ = width;
                    viz_frame_height_ = height;
                }
            }
        }

        // Update metrics
        {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            metrics_.frames_analyzed++;

            // Update motion detection FPS
            auto stats = motion_detector_->get_statistics();
            metrics_.motion_detection_fps = stats.current_fps;
        }

    } catch (const std::exception& e) {
        LOG_ERROR(log_tag_, std::string("Error processing motion frame: ") + e.what());
    }
}

void CameraStream::enable_motion_detection(bool enable) {
    if (!motion_detector_) {
        LOG_WARNING(log_tag_, "Motion detector not initialized");
        return;
    }

    bool was_enabled = motion_detection_enabled_.exchange(enable);
    if (was_enabled != enable) {
        LOG_INFO(log_tag_, std::string("Motion detection ") +
                 (enable ? "enabled" : "disabled"));

        if (enable) {
            // Reset motion detector when enabling
            motion_detector_->reset();
        }
    }
}

bool CameraStream::is_motion_detection_enabled() const {
    return motion_detection_enabled_.load();
}

void CameraStream::update_motion_config(const MotionDetectionConfig& config) {
    if (!motion_detector_) {
        LOG_WARNING(log_tag_, "Motion detector not initialized");
        return;
    }

    motion_detector_->update_config(config);
    config_.motion_detection = config;
    LOG_INFO(log_tag_, "Motion detection configuration updated");
}

void CameraStream::enable_face_detection(bool enable) {
    if (!face_detector_) {
        LOG_WARNING(log_tag_, "Face detector not initialized");
        return;
    }

    bool was_enabled = face_detection_enabled_.exchange(enable);
    if (was_enabled != enable) {
        LOG_INFO(log_tag_, std::string("Face detection ") +
                 (enable ? "enabled" : "disabled"));

        if (enable) {
            // Reset face detector when enabling
            face_detector_->reset();
        }
    }
}

bool CameraStream::is_face_detection_enabled() const {
    return face_detection_enabled_.load();
}

void CameraStream::update_face_config(const FaceDetectionConfig& config) {
    if (!face_detector_) {
        LOG_WARNING(log_tag_, "Face detector not initialized");
        return;
    }

    face_detector_->update_config(config);
    config_.face_detection = config;
    LOG_INFO(log_tag_, "Face detection configuration updated");
}

void CameraStream::process_visualization_frame(GstSample* sample) {
    static int frame_count = 0;
    frame_count++;

    try {
        // Get buffer from sample
        GstBuffer* buffer = gst_sample_get_buffer(sample);
        if (!buffer) {
            return;
        }

        // Get caps to determine frame dimensions
        GstCaps* caps = gst_sample_get_caps(sample);
        if (!caps) {
            return;
        }

        GstStructure* structure = gst_caps_get_structure(caps, 0);
        int width, height;
        if (!gst_structure_get_int(structure, "width", &width) ||
            !gst_structure_get_int(structure, "height", &height)) {
            return;
        }

        // Map buffer to access data
        GstMapInfo map;
        if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            return;
        }

        // Create OpenCV Mat from buffer data (BGR format)
        cv::Mat frame(height, width, CV_8UC3, map.data);

        // Unmap buffer (we no longer need the separate cv::imshow window)
        gst_buffer_unmap(buffer, &map);

        // Get latest detections count for logging
        std::vector<FaceDetection> detections;
        {
            std::lock_guard<std::mutex> lock(viz_mutex_);
            detections = latest_detections_;
        }

        // Log detection count periodically
        if (frame_count % 30 == 0) {
            LOG_INFO(log_tag_, "Viz frame #" + std::to_string(frame_count) +
                     ", detections: " + std::to_string(detections.size()));
        }

        // Note: Visualization is now done via Cairo overlay on the live feed

    } catch (const std::exception& e) {
        LOG_ERROR(log_tag_, std::string("Error processing visualization frame: ") + e.what());
    }
}

void CameraStream::draw_face_detections(cv::Mat& frame, const std::vector<FaceDetection>& detections) {
    int frame_width = frame.cols;
    int frame_height = frame.rows;

    for (const auto& detection : detections) {
        // Convert normalized coordinates to pixel coordinates
        int x = static_cast<int>(detection.bbox.x * frame_width);
        int y = static_cast<int>(detection.bbox.y * frame_height);
        int w = static_cast<int>(detection.bbox.width * frame_width);
        int h = static_cast<int>(detection.bbox.height * frame_height);

        // Draw bounding box
        cv::Scalar box_color(0, 255, 0);  // Green
        cv::rectangle(frame, cv::Rect(x, y, w, h), box_color, config_.face_detection.box_thickness);

        // Draw confidence score if enabled
        if (config_.face_detection.draw_confidence) {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(2) << (detection.confidence * 100) << "%";
            std::string confidence_text = ss.str();

            // Calculate text size for background
            int baseline = 0;
            cv::Size text_size = cv::getTextSize(
                confidence_text,
                cv::FONT_HERSHEY_SIMPLEX,
                config_.face_detection.font_scale,
                1,
                &baseline
            );

            // Draw background rectangle for text
            cv::Point text_org(x, y - 5);
            cv::rectangle(
                frame,
                cv::Point(text_org.x, text_org.y - text_size.height - 2),
                cv::Point(text_org.x + text_size.width, text_org.y + baseline),
                box_color,
                cv::FILLED
            );

            // Draw confidence text
            cv::putText(
                frame,
                confidence_text,
                text_org,
                cv::FONT_HERSHEY_SIMPLEX,
                config_.face_detection.font_scale,
                cv::Scalar(0, 0, 0),  // Black text
                1,
                cv::LINE_AA
            );
        }

        // Draw landmarks if enabled
        if (config_.face_detection.draw_landmarks) {
            cv::Scalar landmark_color(255, 0, 0);  // Blue
            for (int i = 0; i < 5; i++) {
                int lx = static_cast<int>(detection.landmarks[i].x * frame_width);
                int ly = static_cast<int>(detection.landmarks[i].y * frame_height);
                cv::circle(frame, cv::Point(lx, ly), 2, landmark_color, -1);
            }
        }
    }

    // Draw info overlay
    std::stringstream info_ss;
    info_ss << "Faces: " << detections.size();
    cv::putText(
        frame,
        info_ss.str(),
        cv::Point(10, 30),
        cv::FONT_HERSHEY_SIMPLEX,
        0.7,
        cv::Scalar(0, 255, 255),  // Yellow
        2,
        cv::LINE_AA
    );
}

void CameraStream::draw_cairo_overlay(cairo_t* cr) {
    // Get latest detections
    std::vector<FaceDetection> detections;
    int frame_width, frame_height;
    {
        std::lock_guard<std::mutex> lock(viz_mutex_);
        detections = latest_detections_;
        frame_width = viz_frame_width_;
        frame_height = viz_frame_height_;
    }

    if (frame_width == 0 || frame_height == 0) {
        return;  // No frame dimensions yet
    }

    // Draw each face detection
    for (const auto& detection : detections) {
        // Convert normalized coordinates to pixel coordinates
        double x = detection.bbox.x * frame_width;
        double y = detection.bbox.y * frame_height;
        double w = detection.bbox.width * frame_width;
        double h = detection.bbox.height * frame_height;

        // Draw bounding box (green)
        cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 1.0);  // Green, opaque
        cairo_set_line_width(cr, config_.face_detection.box_thickness);
        cairo_rectangle(cr, x, y, w, h);
        cairo_stroke(cr);

        // Draw confidence score if enabled
        if (config_.face_detection.draw_confidence) {
            char confidence_text[32];
            snprintf(confidence_text, sizeof(confidence_text), "%.0f%%", detection.confidence * 100);

            // Position text above bbox
            double text_x = x;
            double text_y = y - 5;

            // Set font
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 14.0 * config_.face_detection.font_scale);

            // Get text extents for background
            cairo_text_extents_t extents;
            cairo_text_extents(cr, confidence_text, &extents);

            // Draw background rectangle for text
            cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 0.7);  // Semi-transparent green
            cairo_rectangle(cr, text_x, text_y - extents.height - 4,
                          extents.width + 8, extents.height + 6);
            cairo_fill(cr);

            // Draw text (black)
            cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
            cairo_move_to(cr, text_x + 4, text_y - 2);
            cairo_show_text(cr, confidence_text);
        }

        // Draw landmarks if enabled (blue circles)
        if (config_.face_detection.draw_landmarks) {
            cairo_set_source_rgba(cr, 0.0, 0.0, 1.0, 1.0);  // Blue
            for (int i = 0; i < 5; i++) {
                double lx = detection.landmarks[i].x * frame_width;
                double ly = detection.landmarks[i].y * frame_height;
                cairo_arc(cr, lx, ly, 2.0, 0, 2 * M_PI);
                cairo_fill(cr);
            }
        }
    }

    // Draw face count overlay (top-left corner)
    if (!detections.empty()) {
        char info_text[64];
        snprintf(info_text, sizeof(info_text), "Faces: %zu", detections.size());

        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 20.0);

        // Draw text with shadow for visibility
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.7);  // Black shadow
        cairo_move_to(cr, 12, 32);
        cairo_show_text(cr, info_text);

        cairo_set_source_rgba(cr, 0.0, 1.0, 1.0, 1.0);  // Cyan text
        cairo_move_to(cr, 10, 30);
        cairo_show_text(cr, info_text);
    }
}

void CameraStream::process_pending_callbacks() {
    if (callback_queue_) {
        // Process all pending callbacks (max 10 per call to avoid blocking too long)
        callback_queue_->process_pending(10);
    }
}

void CameraStream::start_reconnect_timer() {
    if (reconnect_timer_running_.load()) {
        return;  // Already running
    }

    reconnect_timer_running_.store(true);
    reconnect_timer_ = std::make_unique<std::thread>([this]() {
        LOG_INFO(log_tag_, "Reconnect timer thread started");
        while (reconnect_timer_running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            // Timer logic can be implemented here if needed
        }
        LOG_INFO(log_tag_, "Reconnect timer thread stopped");
    });
}

void CameraStream::stop_reconnect_timer() {
    if (!reconnect_timer_running_.load()) {
        return;  // Not running
    }

    reconnect_timer_running_.store(false);

    if (reconnect_timer_ && reconnect_timer_->joinable()) {
        reconnect_timer_->join();
        reconnect_timer_.reset();
    }
}

void CameraStream::stop_health_check_timer() {
    if (!health_check_timer_running_.load()) {
        return;  // Not running
    }

    health_check_timer_running_.store(false);

    if (health_check_timer_ && health_check_timer_->joinable()) {
        health_check_timer_->join();
        health_check_timer_.reset();
    }
}

} // namespace gstreamer_worker
