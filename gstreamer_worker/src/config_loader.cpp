#include "config_loader.h"
#include "logger.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace gstreamer_worker {

// Helper function to convert string to MotionAlgorithm
static MotionAlgorithm string_to_motion_algorithm(const std::string& str) {
    std::string upper_str = str;
    std::transform(upper_str.begin(), upper_str.end(), upper_str.begin(), ::toupper);

    if (upper_str == "MOG2_CUDA") return MotionAlgorithm::MOG2_CUDA;
    if (upper_str == "MOG2") return MotionAlgorithm::MOG2;
    if (upper_str == "KNN") return MotionAlgorithm::KNN;
    if (upper_str == "FRAME_DIFF") return MotionAlgorithm::FRAME_DIFF;

    // Default to MOG2_CUDA
    return MotionAlgorithm::MOG2_CUDA;
}

bool ConfigLoader::load_from_file(
    const std::string& file_path,
    PipelineManagerConfig& manager_config,
    std::vector<CameraConfig>& cameras,
    PersonTrackerConfig& person_tracking_config
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

        return load_from_string(j.dump(), manager_config, cameras, person_tracking_config);

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

            // Motion detection configuration
            if (cam.motion_detection.enabled) {
                json motion_json = {
                    {"enabled", cam.motion_detection.enabled},
                    {"algorithm", motion_algorithm_to_string(cam.motion_detection.algorithm)},
                    {"sensitivity", cam.motion_detection.sensitivity},
                    {"min_contour_area", cam.motion_detection.min_contour_area},
                    {"max_contours", cam.motion_detection.max_contours},
                    {"frame_skip", cam.motion_detection.frame_skip},
                    {"blur_size", cam.motion_detection.blur_size},
                    {"history", cam.motion_detection.history},
                    {"var_threshold", cam.motion_detection.var_threshold},
                    {"detect_shadows", cam.motion_detection.detect_shadows},
                    {"cooldown_seconds", cam.motion_detection.cooldown_seconds},
                    {"required_frames", cam.motion_detection.required_frames},
                    {"max_frame_width", cam.motion_detection.max_frame_width},
                    {"max_frame_height", cam.motion_detection.max_frame_height}
                };

                // Add ROI if configured
                if (cam.motion_detection.roi.is_valid()) {
                    motion_json["roi"] = {
                        {"x", cam.motion_detection.roi.x},
                        {"y", cam.motion_detection.roi.y},
                        {"width", cam.motion_detection.roi.width},
                        {"height", cam.motion_detection.roi.height}
                    };
                }

                cam_json["motion_detection"] = motion_json;
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
    std::vector<CameraConfig>& cameras,
    PersonTrackerConfig& person_tracking_config
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

        // Parse person tracking config
        if (j.contains("person_tracking")) {
            const auto& pt = j["person_tracking"];

            if (pt.contains("enabled") && !pt["enabled"].get<bool>()) {
                // Person tracking disabled, use defaults
                person_tracking_config = PersonTrackerConfig();
            } else {
                // Parse person tracking settings
                if (pt.contains("presence_timeout_seconds")) {
                    person_tracking_config.presence_timeout_seconds = pt["presence_timeout_seconds"].get<double>();
                }
                if (pt.contains("same_camera_cooldown_seconds")) {
                    person_tracking_config.same_camera_cooldown_seconds = pt["same_camera_cooldown_seconds"].get<double>();
                }
                if (pt.contains("max_detections_per_person")) {
                    person_tracking_config.max_detections_per_person = pt["max_detections_per_person"].get<int>();
                }
                if (pt.contains("enable_cross_camera_tracking")) {
                    person_tracking_config.enable_cross_camera_tracking = pt["enable_cross_camera_tracking"].get<bool>();
                }
                if (pt.contains("enable_persistence")) {
                    person_tracking_config.enable_persistence = pt["enable_persistence"].get<bool>();
                }
                if (pt.contains("persistence_path")) {
                    person_tracking_config.persistence_path = pt["persistence_path"].get<std::string>();
                }
                if (pt.contains("enable_daily_reset")) {
                    person_tracking_config.enable_daily_reset = pt["enable_daily_reset"].get<bool>();
                }
                if (pt.contains("daily_reset_hour")) {
                    person_tracking_config.daily_reset_hour = pt["daily_reset_hour"].get<int>();
                }
                if (pt.contains("archive_path")) {
                    person_tracking_config.archive_path = pt["archive_path"].get<std::string>();
                }

                // Validate person tracking config
                if (!person_tracking_config.validate()) {
                    LOG_ERROR("ConfigLoader", "Invalid person tracking configuration");
                    person_tracking_config = PersonTrackerConfig();  // Reset to defaults
                }
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

                // Motion detection configuration
                if (cam_json.contains("motion_detection")) {
                    const auto& motion_json = cam_json["motion_detection"];

                    if (motion_json.contains("enabled")) {
                        cam.motion_detection.enabled = motion_json["enabled"].get<bool>();
                    }
                    if (motion_json.contains("algorithm")) {
                        std::string algo_str = motion_json["algorithm"].get<std::string>();
                        cam.motion_detection.algorithm = string_to_motion_algorithm(algo_str);
                    }
                    if (motion_json.contains("sensitivity")) {
                        cam.motion_detection.sensitivity = motion_json["sensitivity"].get<double>();
                    }
                    if (motion_json.contains("min_contour_area")) {
                        cam.motion_detection.min_contour_area = motion_json["min_contour_area"].get<int>();
                    }
                    if (motion_json.contains("max_contours")) {
                        cam.motion_detection.max_contours = motion_json["max_contours"].get<int>();
                    }
                    if (motion_json.contains("frame_skip")) {
                        cam.motion_detection.frame_skip = motion_json["frame_skip"].get<int>();
                    }
                    if (motion_json.contains("blur_size")) {
                        cam.motion_detection.blur_size = motion_json["blur_size"].get<int>();
                    }
                    if (motion_json.contains("history")) {
                        cam.motion_detection.history = motion_json["history"].get<int>();
                    }
                    if (motion_json.contains("var_threshold")) {
                        cam.motion_detection.var_threshold = motion_json["var_threshold"].get<double>();
                    }
                    if (motion_json.contains("detect_shadows")) {
                        cam.motion_detection.detect_shadows = motion_json["detect_shadows"].get<bool>();
                    }
                    if (motion_json.contains("cooldown_seconds")) {
                        cam.motion_detection.cooldown_seconds = motion_json["cooldown_seconds"].get<double>();
                    }
                    if (motion_json.contains("required_frames")) {
                        cam.motion_detection.required_frames = motion_json["required_frames"].get<int>();
                    }
                    if (motion_json.contains("max_frame_width")) {
                        cam.motion_detection.max_frame_width = motion_json["max_frame_width"].get<int>();
                    }
                    if (motion_json.contains("max_frame_height")) {
                        cam.motion_detection.max_frame_height = motion_json["max_frame_height"].get<int>();
                    }

                    // ROI configuration
                    if (motion_json.contains("roi")) {
                        const auto& roi_json = motion_json["roi"];
                        if (roi_json.contains("x")) {
                            cam.motion_detection.roi.x = roi_json["x"].get<int>();
                        }
                        if (roi_json.contains("y")) {
                            cam.motion_detection.roi.y = roi_json["y"].get<int>();
                        }
                        if (roi_json.contains("width")) {
                            cam.motion_detection.roi.width = roi_json["width"].get<int>();
                        }
                        if (roi_json.contains("height")) {
                            cam.motion_detection.roi.height = roi_json["height"].get<int>();
                        }
                    }

                    // Validate motion detection config
                    if (!cam.motion_detection.validate()) {
                        LOG_WARNING("ConfigLoader",
                            "Invalid motion detection config for camera " + cam.camera_id +
                            ", disabling motion detection");
                        cam.motion_detection.enabled = false;
                    }
                }

                // Face detection configuration
                if (cam_json.contains("face_detection")) {
                    const auto& face_json = cam_json["face_detection"];

                    if (face_json.contains("enabled")) {
                        cam.face_detection.enabled = face_json["enabled"].get<bool>();
                    }
                    if (face_json.contains("model_path")) {
                        cam.face_detection.model_path = face_json["model_path"].get<std::string>();
                    }
                    if (face_json.contains("input_size")) {
                        cam.face_detection.input_size = face_json["input_size"].get<int>();
                    }
                    if (face_json.contains("confidence_threshold")) {
                        cam.face_detection.confidence_threshold = face_json["confidence_threshold"].get<float>();
                    }
                    if (face_json.contains("nms_threshold")) {
                        cam.face_detection.nms_threshold = face_json["nms_threshold"].get<float>();
                    }
                    if (face_json.contains("frame_skip")) {
                        cam.face_detection.frame_skip = face_json["frame_skip"].get<int>();
                    }
                    if (face_json.contains("max_frame_width")) {
                        cam.face_detection.max_frame_width = face_json["max_frame_width"].get<int>();
                    }
                    if (face_json.contains("max_frame_height")) {
                        cam.face_detection.max_frame_height = face_json["max_frame_height"].get<int>();
                    }
                    if (face_json.contains("min_face_size")) {
                        cam.face_detection.min_face_size = face_json["min_face_size"].get<float>();
                    }
                    if (face_json.contains("max_faces")) {
                        cam.face_detection.max_faces = face_json["max_faces"].get<int>();
                    }
                    if (face_json.contains("required_frames")) {
                        cam.face_detection.required_frames = face_json["required_frames"].get<int>();
                    }
                    if (face_json.contains("cooldown_seconds")) {
                        cam.face_detection.cooldown_seconds = face_json["cooldown_seconds"].get<double>();
                    }
                    if (face_json.contains("use_tensorrt")) {
                        cam.face_detection.use_tensorrt = face_json["use_tensorrt"].get<bool>();
                    }
                    if (face_json.contains("use_cuda")) {
                        cam.face_detection.use_cuda = face_json["use_cuda"].get<bool>();
                    }
                    if (face_json.contains("max_batch_size")) {
                        cam.face_detection.max_batch_size = face_json["max_batch_size"].get<int>();
                    }

                    // Face saving configuration (for verification and CompreFace integration)
                    if (face_json.contains("save_faces")) {
                        cam.face_detection.save_faces = face_json["save_faces"].get<bool>();
                    }
                    if (face_json.contains("save_path")) {
                        cam.face_detection.save_path = face_json["save_path"].get<std::string>();
                    }
                    if (face_json.contains("save_margin")) {
                        cam.face_detection.save_margin = face_json["save_margin"].get<float>();
                    }
                    if (face_json.contains("min_save_confidence")) {
                        cam.face_detection.min_save_confidence = face_json["min_save_confidence"].get<float>();
                    }
                    if (face_json.contains("max_saves_per_event")) {
                        cam.face_detection.max_saves_per_event = face_json["max_saves_per_event"].get<int>();
                    }

                    // Blur detection configuration
                    if (face_json.contains("enable_blur_detection")) {
                        cam.face_detection.enable_blur_detection = face_json["enable_blur_detection"].get<bool>();
                    }
                    if (face_json.contains("min_laplacian_variance")) {
                        cam.face_detection.min_laplacian_variance = face_json["min_laplacian_variance"].get<float>();
                    }
                    if (face_json.contains("blur_kernel_size")) {
                        cam.face_detection.blur_kernel_size = face_json["blur_kernel_size"].get<int>();
                    }

                    // Motion-triggered detection configuration
                    if (face_json.contains("motion_triggered_detection")) {
                        cam.face_detection.motion_triggered_detection = face_json["motion_triggered_detection"].get<bool>();
                    }
                    if (face_json.contains("motion_detection_cooldown")) {
                        cam.face_detection.motion_detection_cooldown = face_json["motion_detection_cooldown"].get<float>();
                    }

                    // CompreFace configuration
                    if (face_json.contains("enable_compreface")) {
                        cam.face_detection.enable_compreface = face_json["enable_compreface"].get<bool>();
                    }
                    if (face_json.contains("compreface_url")) {
                        cam.face_detection.compreface_url = face_json["compreface_url"].get<std::string>();
                    }
                    if (face_json.contains("compreface_api_key")) {
                        cam.face_detection.compreface_api_key = face_json["compreface_api_key"].get<std::string>();
                    }
                    if (face_json.contains("compreface_subject")) {
                        cam.face_detection.compreface_subject = face_json["compreface_subject"].get<std::string>();
                    }
                    if (face_json.contains("compreface_similarity_threshold")) {
                        cam.face_detection.compreface_similarity_threshold = face_json["compreface_similarity_threshold"].get<float>();
                    }
                    if (face_json.contains("compreface_timeout_ms")) {
                        cam.face_detection.compreface_timeout_ms = face_json["compreface_timeout_ms"].get<int>();
                    }
                    if (face_json.contains("compreface_max_queue_size")) {
                        cam.face_detection.compreface_max_queue_size = face_json["compreface_max_queue_size"].get<int>();
                    }

                    // Visualization configuration
                    if (face_json.contains("enable_visualization")) {
                        cam.face_detection.enable_visualization = face_json["enable_visualization"].get<bool>();
                    }
                    if (face_json.contains("draw_landmarks")) {
                        cam.face_detection.draw_landmarks = face_json["draw_landmarks"].get<bool>();
                    }
                    if (face_json.contains("draw_confidence")) {
                        cam.face_detection.draw_confidence = face_json["draw_confidence"].get<bool>();
                    }
                    if (face_json.contains("box_thickness")) {
                        cam.face_detection.box_thickness = face_json["box_thickness"].get<int>();
                    }
                    if (face_json.contains("font_scale")) {
                        cam.face_detection.font_scale = face_json["font_scale"].get<float>();
                    }

                    // ROI configuration (optional)
                    if (face_json.contains("roi")) {
                        const auto& roi_json = face_json["roi"];
                        if (roi_json.contains("x")) {
                            cam.face_detection.roi.x = roi_json["x"].get<int>();
                        }
                        if (roi_json.contains("y")) {
                            cam.face_detection.roi.y = roi_json["y"].get<int>();
                        }
                        if (roi_json.contains("width")) {
                            cam.face_detection.roi.width = roi_json["width"].get<int>();
                        }
                        if (roi_json.contains("height")) {
                            cam.face_detection.roi.height = roi_json["height"].get<int>();
                        }
                    }

                    // Validate face detection config
                    if (!cam.face_detection.validate()) {
                        LOG_WARNING("ConfigLoader",
                            "Invalid face detection config for camera " + cam.camera_id +
                            ", disabling face detection");
                        cam.face_detection.enabled = false;
                    }
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
        PersonTrackerConfig person_tracking_config;

        return load_from_file(file_path, manager_config, cameras, person_tracking_config);

    } catch (const std::exception& e) {
        LOG_ERROR("ConfigLoader", std::string("Validation error: ") + e.what());
        return false;
    }
}

} // namespace gstreamer_worker
