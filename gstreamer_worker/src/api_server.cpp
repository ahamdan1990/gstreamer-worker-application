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
                        {"uptime_seconds", m.uptime_seconds}
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
                    {"last_error", metrics.last_error}
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
