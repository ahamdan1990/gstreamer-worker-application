#include "pipeline_manager.h"
#include "logger.h"

#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

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
    // Additional handling can be added here if needed
    // Logging is already handled by PipelineManager
}

// Error callback
void on_error(const std::string& camera_id, const std::string& error) {
    // Additional error handling can be added here
    // For example: send alerts, update external monitoring systems, etc.
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n\n"
              << "Options:\n"
              << "  -h, --help              Show this help message\n"
              << "  --camera-id ID          Camera identifier (default: camera_1)\n"
              << "  --rtsp-url URL          RTSP URL (required)\n"
              << "  --username USER         RTSP username (optional)\n"
              << "  --password PASS         RTSP password (optional)\n"
              << "  --fps FPS               Target FPS (default: 12)\n"
              << "  --latency MS            Latency in milliseconds (default: 150)\n"
              << "  --no-nvidia             Disable NVIDIA hardware acceleration\n"
              << "  --no-display            Disable display (for testing)\n"
              << "  --log-level LEVEL       Log level: DEBUG, INFO, WARNING, ERROR (default: INFO)\n"
              << "\nExample:\n"
              << "  " << program_name << " --camera-id front_door \\\n"
              << "    --rtsp-url rtsp://192.168.0.219/stream \\\n"
              << "    --username service --password 'WSS4Sec$$' \\\n"
              << "    --fps 12 --latency 150\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Default configuration
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
        } else if (arg == "--log-level" && i + 1 < argc) {
            log_level = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    // Validate required arguments
    if (rtsp_url.empty()) {
        std::cerr << "Error: --rtsp-url is required\n" << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    // Print configuration
    std::cout << "========================================\n";
    std::cout << "  GStreamer Worker - Live View\n";
    std::cout << "========================================\n";
    std::cout << "Camera ID: " << camera_id << "\n";
    std::cout << "RTSP URL: " << rtsp_url << "\n";
    if (!username.empty()) {
        std::cout << "Username: " << username << "\n";
    }
    std::cout << "Target FPS: " << fps << "\n";
    std::cout << "Latency: " << latency << "ms\n";
    std::cout << "NVIDIA Acceleration: " << (use_nvidia ? "Enabled" : "Disabled") << "\n";
    std::cout << "Display: " << (enable_display ? "Enabled" : "Disabled") << "\n";
    std::cout << "Log Level: " << log_level << "\n";
    std::cout << "========================================\n";
    std::cout << "\nPress Ctrl+C to stop\n" << std::endl;

    try {
        // Configure pipeline manager
        PipelineManagerConfig manager_config;
        manager_config.log_level = log_level;
        manager_config.enable_metrics = true;
        manager_config.metrics_interval = 30.0;

        // Configure camera
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
        camera_config.max_reconnect_attempts = -1;  // Infinite retries

        // Create pipeline manager
        PipelineManager manager(manager_config, on_state_changed, on_error);

        // Add camera
        if (!manager.add_camera(camera_config)) {
            LOG_ERROR("main", "Failed to add camera");
            return 1;
        }

        // Start all cameras
        auto results = manager.start_all();
        if (results[camera_id]) {
            LOG_INFO("main", "Camera stream started successfully");
        } else {
            LOG_ERROR("main", "Failed to start camera stream");
            return 1;
        }

        // Main loop - wait for shutdown signal
        while (!g_shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Graceful shutdown
        std::cout << "\nShutting down gracefully..." << std::endl;
        manager.stop_all(true);

        // Print final metrics
        auto metrics = manager.get_camera_metrics(camera_id);
        std::cout << "\n========================================\n";
        std::cout << "  Final Statistics\n";
        std::cout << "========================================\n";
        std::cout << "Uptime: " << metrics.uptime_seconds << " seconds\n";
        std::cout << "Errors: " << metrics.errors_count << "\n";
        std::cout << "Reconnections: " << metrics.reconnections << "\n";
        if (!metrics.last_error.empty()) {
            std::cout << "Last Error: " << metrics.last_error << "\n";
        }
        std::cout << "========================================\n";

        LOG_INFO("main", "Application shutdown complete");
        return 0;

    } catch (const std::exception& e) {
        LOG_ERROR("main", std::string("Fatal error: ") + e.what());
        return 1;
    }
}
