#include "api_server.h"
#include "logger.h"
#include "config_loader.h"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace gstreamer_worker {

APIServer::APIServer(
    std::shared_ptr<PipelineManager> manager,
    const std::string& host,
    int port
)
    : manager_(manager)
    , host_(host)
    , port_(port)
    , running_(false)
    , server_impl_(nullptr)
{
    LOG_INFO(log_tag_, "API Server initialized on " + host + ":" + std::to_string(port));
}

APIServer::~APIServer() {
    stop();
}

bool APIServer::start() {
    if (running_.exchange(true)) {
        LOG_WARNING(log_tag_, "API Server already running");
        return false;
    }

    LOG_INFO(log_tag_, "Starting API Server...");

    server_thread_ = std::make_unique<std::thread>(&APIServer::run_server, this);

    return true;
}

void APIServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    LOG_INFO(log_tag_, "Stopping API Server...");

    // Stop the HTTP server
    if (server_impl_) {
        auto* server = static_cast<httplib::Server*>(server_impl_);
        server->stop();
    }

    // Wait for thread
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }

    LOG_INFO(log_tag_, "API Server stopped");
}

bool APIServer::is_running() const {
    return running_.load();
}


void APIServer::run_server() {
    try {
        httplib::Server server;
        server_impl_ = &server;

        // Setup all routes
        setup_routes();

        // CORS headers
        server.set_default_headers({
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type"},
        });

        LOG_INFO(log_tag_, "API Server listening on http://" + host_ + ":" + std::to_string(port_));

        // This is blocking
        server.listen(host_.c_str(), port_);

    } catch (const std::exception& e) {
        LOG_ERROR(log_tag_, std::string("API Server error: ") + e.what());
        running_ = false;
    }

    server_impl_ = nullptr;
}

void APIServer::setup_routes() {
    auto* server = static_cast<httplib::Server*>(server_impl_);

    // Health check
    server->Get("/health", [this](const httplib::Request&, httplib::Response& res) {
        json response = {
            {"status", "healthy"},
            {"running", manager_->is_running()},
            {"cameras", manager_->get_camera_count()}
        };
        res.set_content(response.dump(), "application/json");
    });

    // List all cameras
    server->Get("/api/cameras", [this](const httplib::Request&, httplib::Response& res) {
        try {
            json cameras_json = json::array();

            auto states = manager_->get_all_states();
            auto metrics = manager_->get_all_metrics();

            for (const auto& [camera_id, state] : states) {
                json camera_json = {
                    {"camera_id", camera_id},
                    {"state", stream_state_to_string(state)},
                    {"is_running", state == StreamState::RUNNING}
                };

                // Add metrics if available
                if (metrics.count(camera_id)) {
                    const auto& m = metrics[camera_id];
                    camera_json["metrics"] = {
                        {"frames_displayed", m.frames_displayed},
                        {"errors_count", m.errors_count},
                        {"reconnections", m.reconnections},
                        {"uptime_seconds", m.uptime_seconds},
                        {"frames_analyzed", m.frames_analyzed},
                        {"motion_events_detected", m.motion_events_detected},
                        {"motion_detection_fps", m.motion_detection_fps},
                        {"last_motion_timestamp", m.last_motion_timestamp}
                    };
                }

                cameras_json.push_back(camera_json);
            }

            res.set_content(cameras_json.dump(), "application/json");
        } catch (const std::exception& e) {
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    // Get camera status
    server->Get("/api/cameras/:id/status", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string camera_id = req.path_params.at("id");

            auto state = manager_->get_camera_state(camera_id);
            auto metrics = manager_->get_camera_metrics(camera_id);

            json response = {
                {"camera_id", camera_id},
                {"state", stream_state_to_string(state)},
                {"is_running", state == StreamState::RUNNING},
                {"metrics", {
                    {"frames_displayed", metrics.frames_displayed},
                    {"errors_count", metrics.errors_count},
                    {"reconnections", metrics.reconnections},
                    {"uptime_seconds", metrics.uptime_seconds},
                    {"last_error", metrics.last_error},
                    {"frames_analyzed", metrics.frames_analyzed},
                    {"motion_events_detected", metrics.motion_events_detected},
                    {"motion_detection_fps", metrics.motion_detection_fps},
                    {"last_motion_timestamp", metrics.last_motion_timestamp}
                }}
            };

            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            json error = {{"error", e.what()}};
            res.status = 404;
            res.set_content(error.dump(), "application/json");
        }
    });

    // Add camera
    server->Post("/api/cameras", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);

            CameraConfig config;
            config.camera_id = body["camera_id"];
            config.rtsp_url = body["rtsp_url"];

            if (body.contains("username")) config.username = body["username"];
            if (body.contains("password")) config.password = body["password"];
            if (body.contains("protocols")) config.protocols = body["protocols"];
            if (body.contains("latency_ms")) config.latency_ms = body["latency_ms"];
            if (body.contains("target_fps")) config.target_fps = body["target_fps"];
            if (body.contains("enable_display")) config.enable_display = body["enable_display"];
            if (body.contains("use_nvidia_decoder")) config.use_nvidia_decoder = body["use_nvidia_decoder"];

            // Parse motion detection configuration
            if (body.contains("motion_detection")) {
                auto md = body["motion_detection"];
                config.motion_detection.enabled = md.value("enabled", false);

                if (config.motion_detection.enabled) {
                    // Algorithm selection
                    if (md.contains("algorithm")) {
                        std::string algo = md["algorithm"];
                        if (algo == "MOG2_CUDA") config.motion_detection.algorithm = MotionAlgorithm::MOG2_CUDA;
                        else if (algo == "MOG2") config.motion_detection.algorithm = MotionAlgorithm::MOG2;
                        else if (algo == "KNN") config.motion_detection.algorithm = MotionAlgorithm::KNN;
                        else if (algo == "FRAME_DIFF") config.motion_detection.algorithm = MotionAlgorithm::FRAME_DIFF;
                    }

                    // Detection parameters
                    if (md.contains("sensitivity")) config.motion_detection.sensitivity = md["sensitivity"];
                    if (md.contains("min_contour_area")) config.motion_detection.min_contour_area = md["min_contour_area"];
                    if (md.contains("max_contours")) config.motion_detection.max_contours = md["max_contours"];

                    // Frame processing
                    if (md.contains("frame_skip")) config.motion_detection.frame_skip = md["frame_skip"];
                    if (md.contains("blur_size")) config.motion_detection.blur_size = md["blur_size"];

                    // Background subtraction parameters
                    if (md.contains("history")) config.motion_detection.history = md["history"];
                    if (md.contains("var_threshold")) config.motion_detection.var_threshold = md["var_threshold"];
                    if (md.contains("detect_shadows")) config.motion_detection.detect_shadows = md["detect_shadows"];

                    // Event debouncing
                    if (md.contains("cooldown_seconds")) config.motion_detection.cooldown_seconds = md["cooldown_seconds"];
                    if (md.contains("required_frames")) config.motion_detection.required_frames = md["required_frames"];

                    // Frame size limits
                    if (md.contains("max_frame_width")) config.motion_detection.max_frame_width = md["max_frame_width"];
                    if (md.contains("max_frame_height")) config.motion_detection.max_frame_height = md["max_frame_height"];

                    // Region of Interest (ROI)
                    if (md.contains("roi") && !md["roi"].is_null()) {
                        auto roi = md["roi"];
                        config.motion_detection.roi.x = roi.value("x", 0);
                        config.motion_detection.roi.y = roi.value("y", 0);
                        config.motion_detection.roi.width = roi.value("width", 0);
                        config.motion_detection.roi.height = roi.value("height", 0);
                    }
                }
            }

            bool success = manager_->add_camera(config);

            json response = {
                {"success", success},
                {"camera_id", config.camera_id},
                {"message", success ? "Camera added successfully" : "Failed to add camera"}
            };

            res.status = success ? 201 : 400;
            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            json error = {{"error", e.what()}};
            res.status = 400;
            res.set_content(error.dump(), "application/json");
        }
    });

    // Start camera
    server->Post("/api/cameras/:id/start", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string camera_id = req.path_params.at("id");

            auto camera = manager_->get_camera(camera_id);
            if (!camera) {
                json error = {{"error", "Camera not found"}};
                res.status = 404;
                res.set_content(error.dump(), "application/json");
                return;
            }

            bool success = camera->start();

            json response = {
                {"success", success},
                {"camera_id", camera_id},
                {"state", "STARTING"},
                {"message", success ? "Camera starting" : "Failed to start camera"}
            };

            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    // Stop camera
    server->Post("/api/cameras/:id/stop", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string camera_id = req.path_params.at("id");

            auto camera = manager_->get_camera(camera_id);
            if (!camera) {
                json error = {{"error", "Camera not found"}};
                res.status = 404;
                res.set_content(error.dump(), "application/json");
                return;
            }

            camera->stop(false);

            json response = {
                {"success", true},
                {"camera_id", camera_id},
                {"state", "STOPPING"},
                {"message", "Camera stopping"}
            };

            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    // Delete camera
    server->Delete("/api/cameras/:id", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string camera_id = req.path_params.at("id");

            bool success = manager_->remove_camera(camera_id, true);

            json response = {
                {"success", success},
                {"camera_id", camera_id},
                {"message", success ? "Camera removed" : "Failed to remove camera"}
            };

            res.status = success ? 200 : 404;
            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    // System status
    server->Get("/api/system/status", [this](const httplib::Request&, httplib::Response& res) {
        try {
            json response = {
                {"running", manager_->is_running()},
                {"total_cameras", manager_->get_camera_count()},
                {"running_cameras", manager_->get_running_camera_count()},
                {"timestamp", std::time(nullptr)}
            };

            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            json error = {{"error", e.what()}};
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    LOG_INFO(log_tag_, "API routes configured");
}

} // namespace gstreamer_worker
