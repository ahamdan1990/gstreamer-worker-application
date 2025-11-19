#include "config_loader.h"
#include "logger.h"

#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace gstreamer_worker {

bool ConfigLoader::load_from_file(
    const std::string& file_path,
    PipelineManagerConfig& manager_config,
    std::vector<CameraConfig>& cameras
) {
    try {
        // Read file
        std::ifstream file(file_path);
        if (!file.is_open()) {
            LOG_ERROR("ConfigLoader", "Failed to open config file: " + file_path);
            return false;
        }

        // Parse JSON
        json j;
        file >> j;

        return load_from_string(j.dump(), manager_config, cameras);

    } catch (const json::exception& e) {
        LOG_ERROR("ConfigLoader", std::string("JSON parsing error: ") + e.what());
        return false;
    } catch (const std::exception& e) {
        LOG_ERROR("ConfigLoader", std::string("Error loading config: ") + e.what());
        return false;
    }
}

bool ConfigLoader::save_to_file(
    const std::string& file_path,
    const PipelineManagerConfig& manager_config,
    const std::vector<CameraConfig>& cameras
) {
    try {
        json j;

        // Manager config
        j["manager"] = {
            {"log_level", manager_config.log_level},
            {"enable_metrics", manager_config.enable_metrics},
            {"metrics_interval", manager_config.metrics_interval}
        };

        // Cameras
        j["cameras"] = json::array();
        for (const auto& cam : cameras) {
            json cam_json = {
                {"camera_id", cam.camera_id},
                {"rtsp_url", cam.rtsp_url},
                {"protocols", cam.protocols},
                {"latency_ms", cam.latency_ms},
                {"drop_on_latency", cam.drop_on_latency},
                {"target_fps", cam.target_fps},
                {"enable_display", cam.enable_display},
                {"display_sync", cam.display_sync},
                {"auto_reconnect", cam.auto_reconnect},
                {"max_reconnect_attempts", cam.max_reconnect_attempts},
                {"reconnect_initial_delay", cam.reconnect_initial_delay},
                {"reconnect_max_delay", cam.reconnect_max_delay},
                {"reconnect_backoff_multiplier", cam.reconnect_backoff_multiplier},
                {"health_check_interval", cam.health_check_interval},
                {"max_consecutive_errors", cam.max_consecutive_errors},
                {"use_nvidia_decoder", cam.use_nvidia_decoder}
            };

            if (!cam.username.empty()) {
                cam_json["username"] = cam.username;
            }
            if (!cam.password.empty()) {
                cam_json["password"] = cam.password;
            }

            j["cameras"].push_back(cam_json);
        }

        // Write to file
        std::ofstream file(file_path);
        if (!file.is_open()) {
            LOG_ERROR("ConfigLoader", "Failed to open file for writing: " + file_path);
            return false;
        }

        file << j.dump(4);  // Pretty print with 4 spaces
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("ConfigLoader", std::string("Error saving config: ") + e.what());
        return false;
    }
}

bool ConfigLoader::load_from_string(
    const std::string& json_str,
    PipelineManagerConfig& manager_config,
    std::vector<CameraConfig>& cameras
) {
    try {
        json j = json::parse(json_str);

        // Parse manager config
        if (j.contains("manager")) {
            const auto& mgr = j["manager"];

            if (mgr.contains("log_level")) {
                manager_config.log_level = mgr["log_level"].get<std::string>();
            }
            if (mgr.contains("enable_metrics")) {
                manager_config.enable_metrics = mgr["enable_metrics"].get<bool>();
            }
            if (mgr.contains("metrics_interval")) {
                manager_config.metrics_interval = mgr["metrics_interval"].get<double>();
            }
        }

        // Parse cameras
        if (j.contains("cameras") && j["cameras"].is_array()) {
            cameras.clear();

            for (const auto& cam_json : j["cameras"]) {
                CameraConfig cam;

                // Required fields
                if (!cam_json.contains("camera_id") || !cam_json.contains("rtsp_url")) {
                    LOG_ERROR("ConfigLoader", "Camera missing required fields (camera_id, rtsp_url)");
                    continue;
                }

                cam.camera_id = cam_json["camera_id"].get<std::string>();
                cam.rtsp_url = cam_json["rtsp_url"].get<std::string>();

                // Optional fields with defaults
                if (cam_json.contains("username")) {
                    cam.username = cam_json["username"].get<std::string>();
                }
                if (cam_json.contains("password")) {
                    cam.password = cam_json["password"].get<std::string>();
                }
                if (cam_json.contains("protocols")) {
                    cam.protocols = cam_json["protocols"].get<std::string>();
                }
                if (cam_json.contains("latency_ms")) {
                    cam.latency_ms = cam_json["latency_ms"].get<int>();
                }
                if (cam_json.contains("drop_on_latency")) {
                    cam.drop_on_latency = cam_json["drop_on_latency"].get<bool>();
                }
                if (cam_json.contains("target_fps")) {
                    cam.target_fps = cam_json["target_fps"].get<int>();
                }
                if (cam_json.contains("enable_display")) {
                    cam.enable_display = cam_json["enable_display"].get<bool>();
                }
                if (cam_json.contains("display_sync")) {
                    cam.display_sync = cam_json["display_sync"].get<bool>();
                }
                if (cam_json.contains("auto_reconnect")) {
                    cam.auto_reconnect = cam_json["auto_reconnect"].get<bool>();
                }
                if (cam_json.contains("max_reconnect_attempts")) {
                    cam.max_reconnect_attempts = cam_json["max_reconnect_attempts"].get<int>();
                }
                if (cam_json.contains("reconnect_initial_delay")) {
                    cam.reconnect_initial_delay = cam_json["reconnect_initial_delay"].get<double>();
                }
                if (cam_json.contains("reconnect_max_delay")) {
                    cam.reconnect_max_delay = cam_json["reconnect_max_delay"].get<double>();
                }
                if (cam_json.contains("reconnect_backoff_multiplier")) {
                    cam.reconnect_backoff_multiplier = cam_json["reconnect_backoff_multiplier"].get<double>();
                }
                if (cam_json.contains("health_check_interval")) {
                    cam.health_check_interval = cam_json["health_check_interval"].get<double>();
                }
                if (cam_json.contains("max_consecutive_errors")) {
                    cam.max_consecutive_errors = cam_json["max_consecutive_errors"].get<int>();
                }
                if (cam_json.contains("use_nvidia_decoder")) {
                    cam.use_nvidia_decoder = cam_json["use_nvidia_decoder"].get<bool>();
                }

                // Validate
                if (!cam.validate()) {
                    LOG_ERROR("ConfigLoader", "Invalid camera configuration: " + cam.camera_id);
                    continue;
                }

                cameras.push_back(cam);
            }

            LOG_INFO("ConfigLoader", "Loaded " + std::to_string(cameras.size()) + " cameras");
        }

        return !cameras.empty();

    } catch (const json::exception& e) {
        LOG_ERROR("ConfigLoader", std::string("JSON parsing error: ") + e.what());
        return false;
    } catch (const std::exception& e) {
        LOG_ERROR("ConfigLoader", std::string("Error parsing config: ") + e.what());
        return false;
    }
}

bool ConfigLoader::validate_config_file(const std::string& file_path) {
    try {
        PipelineManagerConfig manager_config;
        std::vector<CameraConfig> cameras;

        return load_from_file(file_path, manager_config, cameras);

    } catch (const std::exception& e) {
        LOG_ERROR("ConfigLoader", std::string("Validation error: ") + e.what());
        return false;
    }
}

} // namespace gstreamer_worker
