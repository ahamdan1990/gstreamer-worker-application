#include "pipeline_manager.h"
#include "config_loader.h"
#include "logger.h"

#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <iomanip>

using namespace gstreamer_worker;

// Global flag for graceful shutdown
static std::atomic<bool> g_shutdown_requested(false);

// Signal handler for Ctrl+C
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutdown requested..." << std::endl;
        g_shutdown_requested = true;
    }
}

// State change callback
void on_state_changed(const std::string& camera_id, StreamState state) {
    // Custom handling for state changes
    // Example: Send notification, update database, etc.
}

// Error callback
void on_error(const std::string& camera_id, const std::string& error) {
    // Custom error handling
    // Example: Send alert, log to monitoring system, etc.
}

// Motion event callback
void on_motion_detected(const MotionEvent& event) {
    // Log motion event with detailed information
    std::stringstream ss;
    ss << "\n╔══════════════════════════════════════════════╗\n";
    ss << "║         MOTION DETECTED                      ║\n";
    ss << "╠══════════════════════════════════════════════╣\n";
    ss << "║ Camera: " << std::left << std::setw(35) << event.camera_id << "║\n";
    ss << "║ Motion Area: " << std::setw(30) << (std::to_string(event.motion_area) + " pixels") << "║\n";
    ss << "║ Contours: " << std::setw(33) << event.num_contours << "║\n";
    ss << "║ Confidence: " << std::setw(31) << std::fixed << std::setprecision(2) << (event.confidence * 100.0) << "%" << "║\n";
    ss << "║ Bounding Box: [" << event.bounding_box.x << "," << event.bounding_box.y
       << " " << event.bounding_box.width << "x" << event.bounding_box.height << "]" << std::setw(10) << "║\n";
    ss << "╚══════════════════════════════════════════════╝\n";

    LOG_INFO("MotionEvent", ss.str());
}

// Face detection event callback
void on_face_detected(const FaceEvent& event) {
    // Log face detection event with detailed information
    std::stringstream ss;
    ss << "\n╔══════════════════════════════════════════════╗\n";
    ss << "║         🎭 FACE DETECTED                     ║\n";
    ss << "╠══════════════════════════════════════════════╣\n";
    ss << "║ Camera: " << std::left << std::setw(35) << event.camera_id << "║\n";
    ss << "║ Number of Faces: " << std::setw(27) << event.num_faces << "║\n";
    ss << "║ Timestamp: " << std::setw(32) << std::fixed << std::setprecision(3) << event.timestamp << "║\n";
    ss << "╠══════════════════════════════════════════════╣\n";

    // List each detected face
    for (int i = 0; i < event.num_faces && i < 5; i++) {  // Show max 5 faces
        const auto& face = event.faces[i];
        ss << "║ Face #" << (i + 1) << "                                     ║\n";
        ss << "║   Confidence: " << std::setw(29) << std::fixed << std::setprecision(1) << (face.confidence * 100.0) << "%" << "║\n";
        ss << "║   BBox: [" << std::fixed << std::setprecision(3)
           << face.bbox.x << "," << face.bbox.y << " "
           << face.bbox.width << "x" << face.bbox.height << "]";

        // Pad to align with border
        int bbox_len = ss.str().length() - ss.str().rfind("║   BBox:") - 8;
        ss << std::string(std::max(0, 35 - bbox_len), ' ') << "║\n";

        if (i < event.num_faces - 1 && i < 4) {
            ss << "║                                              ║\n";
        }
    }

    if (event.num_faces > 5) {
        ss << "║   ... and " << (event.num_faces - 5) << " more faces               ║\n";
    }

    ss << "╚══════════════════════════════════════════════╝\n";

    LOG_INFO("FaceEvent", ss.str());
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n\n"
              << "Options:\n"
              << "  -h, --help                 Show this help message\n"
              << "  -c, --config FILE          Load cameras from JSON config file\n"
              << "  --validate-config FILE     Validate config file and exit\n"
              << "  --log-level LEVEL          Log level: DEBUG, INFO, WARNING, ERROR (default: INFO)\n"
              << "\nSingle Camera Mode (without config file):\n"
              << "  --camera-id ID             Camera identifier (default: camera_1)\n"
              << "  --rtsp-url URL             RTSP URL (required)\n"
              << "  --username USER            RTSP username (optional)\n"
              << "  --password PASS            RTSP password (optional)\n"
              << "  --fps FPS                  Target FPS (default: 12)\n"
              << "  --latency MS               Latency in milliseconds (default: 150)\n"
              << "  --no-nvidia                Disable NVIDIA hardware acceleration\n"
              << "  --no-display               Disable display\n"
              << "\nExamples:\n"
              << "  # Load multiple cameras from config\n"
              << "  " << program_name << " --config config/cameras.json\n\n"
              << "  # Validate config file\n"
              << "  " << program_name << " --validate-config config/cameras.json\n\n"
              << "  # Single camera mode\n"
              << "  " << program_name << " --camera-id front_door \\\n"
              << "    --rtsp-url rtsp://192.168.0.219/stream \\\n"
              << "    --username service --password 'WSS4Sec$$'\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Command line options
    std::string config_file;
    std::string validate_config;
    bool use_config_file = false;

    // Single camera mode defaults
    std::string camera_id = "camera_1";
    std::string rtsp_url;
    std::string username;
    std::string password;
    int fps = 12;
    int latency = 150;
    bool use_nvidia = true;
    bool enable_display = true;
    std::string log_level = "INFO";

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            config_file = argv[++i];
            use_config_file = true;
        } else if (arg == "--validate-config" && i + 1 < argc) {
            validate_config = argv[++i];
        } else if (arg == "--log-level" && i + 1 < argc) {
            log_level = argv[++i];
        } else if (arg == "--camera-id" && i + 1 < argc) {
            camera_id = argv[++i];
        } else if (arg == "--rtsp-url" && i + 1 < argc) {
            rtsp_url = argv[++i];
        } else if (arg == "--username" && i + 1 < argc) {
            username = argv[++i];
        } else if (arg == "--password" && i + 1 < argc) {
            password = argv[++i];
        } else if (arg == "--fps" && i + 1 < argc) {
            fps = std::stoi(argv[++i]);
        } else if (arg == "--latency" && i + 1 < argc) {
            latency = std::stoi(argv[++i]);
        } else if (arg == "--no-nvidia") {
            use_nvidia = false;
        } else if (arg == "--no-display") {
            enable_display = false;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    // Validate config mode
    if (!validate_config.empty()) {
        std::cout << "Validating config file: " << validate_config << std::endl;
        if (ConfigLoader::validate_config_file(validate_config)) {
            std::cout << "✓ Configuration file is valid" << std::endl;
            return 0;
        } else {
            std::cerr << "✗ Configuration file is invalid" << std::endl;
            return 1;
        }
    }

    try {
        PipelineManagerConfig manager_config;
        std::vector<CameraConfig> cameras;

        // Load from config file or create single camera
        if (use_config_file) {
            // Load from JSON config
            std::cout << "Loading configuration from: " << config_file << std::endl;

            if (!ConfigLoader::load_from_file(config_file, manager_config, cameras)) {
                std::cerr << "Error: Failed to load configuration file" << std::endl;
                return 1;
            }

            if (cameras.empty()) {
                std::cerr << "Error: No valid cameras found in configuration" << std::endl;
                return 1;
            }

            std::cout << "Loaded " << cameras.size() << " camera(s) from config" << std::endl;

        } else {
            // Single camera mode from command line
            if (rtsp_url.empty()) {
                std::cerr << "Error: --rtsp-url is required (or use --config)" << std::endl;
                print_usage(argv[0]);
                return 1;
            }

            manager_config.log_level = log_level;
            manager_config.enable_metrics = true;
            manager_config.metrics_interval = 30.0;

            CameraConfig camera_config;
            camera_config.camera_id = camera_id;
            camera_config.rtsp_url = rtsp_url;
            camera_config.username = username;
            camera_config.password = password;
            camera_config.target_fps = fps;
            camera_config.latency_ms = latency;
            camera_config.protocols = "tcp";
            camera_config.drop_on_latency = true;
            camera_config.enable_display = enable_display;
            camera_config.display_sync = false;
            camera_config.use_nvidia_decoder = use_nvidia;
            camera_config.auto_reconnect = true;
            camera_config.max_reconnect_attempts = -1;

            cameras.push_back(camera_config);
        }

        // Print configuration summary
        std::cout << "========================================\n";
        std::cout << "  GStreamer Worker - Multi-Camera\n";
        std::cout << "========================================\n";
        std::cout << "Log Level: " << manager_config.log_level << "\n";
        std::cout << "Metrics: " << (manager_config.enable_metrics ? "Enabled" : "Disabled") << "\n";
        std::cout << "Total Cameras: " << cameras.size() << "\n";
        std::cout << "========================================\n";

        for (const auto& cam : cameras) {
            std::cout << "\nCamera: " << cam.camera_id << "\n";
            std::cout << "  RTSP URL: " << cam.rtsp_url << "\n";
            std::cout << "  FPS: " << cam.target_fps << "\n";
            std::cout << "  Latency: " << cam.latency_ms << "ms\n";
            std::cout << "  Display: " << (cam.enable_display ? "Yes" : "No") << "\n";
            std::cout << "  NVIDIA: " << (cam.use_nvidia_decoder ? "Yes" : "No") << "\n";
        }
        std::cout << "========================================\n";
        std::cout << "\nPress Ctrl+C to stop\n" << std::endl;

        // Create pipeline manager
        PipelineManager manager(manager_config, on_state_changed, on_error, on_motion_detected, on_face_detected);

        // Add all cameras
        auto add_results = manager.add_cameras(cameras);
        int added = 0;
        for (const auto& [id, success] : add_results) {
            if (success) {
                added++;
            } else {
                LOG_ERROR("main", "Failed to add camera: " + id);
            }
        }

        if (added == 0) {
            LOG_ERROR("main", "Failed to add any cameras");
            return 1;
        }

        LOG_INFO("main", "Added " + std::to_string(added) + " camera(s)");

        // Start all cameras
        auto start_results = manager.start_all();
        int started = 0;
        for (const auto& [id, success] : start_results) {
            if (success) {
                started++;
                LOG_INFO("main", "Started camera: " + id);
            } else {
                LOG_ERROR("main", "Failed to start camera: " + id);
            }
        }

        if (started == 0) {
            LOG_ERROR("main", "Failed to start any cameras");
            return 1;
        }

        LOG_INFO("main", "Started " + std::to_string(started) + "/" +
                 std::to_string(cameras.size()) + " camera(s)");

        // Main loop - wait for shutdown signal
        while (!g_shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Graceful shutdown
        std::cout << "\nShutting down gracefully..." << std::endl;
        manager.stop_all(true);

        // Print final statistics
        std::cout << "\n========================================\n";
        std::cout << "  Final Statistics\n";
        std::cout << "========================================\n";

        auto all_metrics = manager.get_all_metrics();
        for (const auto& [camera_id, metrics] : all_metrics) {
            std::cout << "\n" << camera_id << ":\n";
            std::cout << "  Uptime: " << metrics.uptime_seconds << " seconds\n";
            std::cout << "  Errors: " << metrics.errors_count << "\n";
            std::cout << "  Reconnections: " << metrics.reconnections << "\n";
            if (!metrics.last_error.empty()) {
                std::cout << "  Last Error: " << metrics.last_error << "\n";
            }
        }
        std::cout << "========================================\n";

        LOG_INFO("main", "Application shutdown complete");
        return 0;

    } catch (const std::exception& e) {
        LOG_ERROR("main", std::string("Fatal error: ") + e.what());
        return 1;
    }
}
