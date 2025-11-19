#include "pipeline_manager.h"
#include "api_server.h"
#include "logger.h"

#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <memory>
#include <httplib.h>

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
    LOG_INFO("main", "Camera " + camera_id + " state: " + stream_state_to_string(state));
}

// Error callback
void on_error(const std::string& camera_id, const std::string& error) {
    LOG_ERROR("main", "Camera " + camera_id + " error: " + error);
}

// Notify FastAPI that worker is ready
bool notify_fastapi_ready(const std::string& fastapi_url) {
    try {
        // Parse URL
        std::string protocol, host, path;
        int port = 80;

        // Simple URL parsing: http://host:port/path
        size_t proto_end = fastapi_url.find("://");
        if (proto_end != std::string::npos) {
            protocol = fastapi_url.substr(0, proto_end);
            size_t host_start = proto_end + 3;

            size_t port_start = fastapi_url.find(":", host_start);
            size_t path_start = fastapi_url.find("/", host_start);

            if (port_start != std::string::npos && (path_start == std::string::npos || port_start < path_start)) {
                host = fastapi_url.substr(host_start, port_start - host_start);
                size_t port_end = (path_start != std::string::npos) ? path_start : fastapi_url.length();
                port = std::stoi(fastapi_url.substr(port_start + 1, port_end - port_start - 1));
                path = (path_start != std::string::npos) ? fastapi_url.substr(path_start) : "/";
            } else if (path_start != std::string::npos) {
                host = fastapi_url.substr(host_start, path_start - host_start);
                path = fastapi_url.substr(path_start);
            } else {
                host = fastapi_url.substr(host_start);
                path = "/";
            }
        } else {
            return false;
        }

        // Add the worker ready endpoint path
        std::string full_path = path + (path.back() == '/' ? "" : "/") + "api/v1/worker/ready";

        LOG_INFO("main", "Notifying FastAPI at " + host + ":" + std::to_string(port) + full_path);

        // Create HTTP client and send POST request
        httplib::Client client(host.c_str(), port);
        client.set_connection_timeout(5, 0);  // 5 seconds
        client.set_read_timeout(10, 0);        // 10 seconds

        auto res = client.Post(full_path.c_str(), "", "application/json");

        if (res && res->status == 200) {
            LOG_INFO("main", "Successfully notified FastAPI - cameras will be synchronized");
            return true;
        } else if (res) {
            LOG_WARNING("main", "FastAPI notification failed with status: " + std::to_string(res->status));
            return false;
        } else {
            LOG_WARNING("main", "Failed to connect to FastAPI - cameras will not be auto-synced");
            return false;
        }

    } catch (const std::exception& e) {
        LOG_WARNING("main", std::string("Error notifying FastAPI: ") + e.what());
        return false;
    }
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n\n"
              << "Options:\n"
              << "  -h, --help              Show this help message\n"
              << "  --host HOST             API server host (default: 0.0.0.0)\n"
              << "  --port PORT             API server port (default: 8081)\n"
              << "  --fastapi-url URL       FastAPI base URL for camera sync (optional)\n"
              << "                          Example: http://localhost:8002\n"
              << "  --log-level LEVEL       Log level: DEBUG, INFO, WARNING, ERROR (default: INFO)\n"
              << "\nDescription:\n"
              << "  Starts the GStreamer worker with REST API server.\n"
              << "  Cameras are controlled via HTTP API on port 8081.\n"
              << "\nAPI Endpoints:\n"
              << "  GET    /health                  - Health check\n"
              << "  GET    /api/cameras             - List cameras\n"
              << "  POST   /api/cameras             - Add camera\n"
              << "  GET    /api/cameras/:id/status  - Get camera status\n"
              << "  POST   /api/cameras/:id/start   - Start camera\n"
              << "  POST   /api/cameras/:id/stop    - Stop camera\n"
              << "  DELETE /api/cameras/:id         - Remove camera\n"
              << "  GET    /api/system/status       - System status\n"
              << "\nExample:\n"
              << "  " << program_name << " --port 8081\n"
              << "\nThen use curl to add camera:\n"
              << "  curl -X POST http://localhost:8081/api/cameras \\\n"
              << "    -H 'Content-Type: application/json' \\\n"
              << "    -d '{\"camera_id\":\"cam1\",\"rtsp_url\":\"rtsp://...\"}'\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Default configuration
    std::string host = "0.0.0.0";
    int port = 8081;
    std::string fastapi_url = "";
    std::string log_level = "INFO";

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--fastapi-url" && i + 1 < argc) {
            fastapi_url = argv[++i];
        } else if (arg == "--log-level" && i + 1 < argc) {
            log_level = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    // Set log level
    if (log_level == "DEBUG") {
        Logger::instance().set_log_level(LogLevel::DEBUG);
    } else if (log_level == "WARNING") {
        Logger::instance().set_log_level(LogLevel::WARNING);
    } else if (log_level == "ERROR") {
        Logger::instance().set_log_level(LogLevel::ERROR);
    }

    // Print configuration
    std::cout << "========================================\n";
    std::cout << "  GStreamer Worker - API Mode\n";
    std::cout << "========================================\n";
    std::cout << "API Server: http://" << host << ":" << port << "\n";
    std::cout << "Log Level: " << log_level << "\n";
    std::cout << "========================================\n";
    std::cout << "\nAPI Documentation: http://" << host << ":" << port << "/health\n";
    std::cout << "Press Ctrl+C to stop\n" << std::endl;

    try {
        // Configure pipeline manager
        PipelineManagerConfig manager_config;
        manager_config.log_level = log_level;
        manager_config.enable_metrics = true;
        manager_config.metrics_interval = 60.0;

        // Create pipeline manager (shared ownership with API server)
        auto manager = std::make_shared<PipelineManager>(
            manager_config,
            on_state_changed,
            on_error
        );

        // Start pipeline manager
        LOG_INFO("main", "Starting pipeline manager");
        // Manager starts when first camera is added via API

        // Create and start API server
        APIServer api_server(manager, host, port);

        if (!api_server.start()) {
            LOG_ERROR("main", "Failed to start API server");
            return 1;
        }

        LOG_INFO("main", "API server started successfully");
        std::cout << "\n✅ API Server ready at http://" << host << ":" << port << "\n";
        std::cout << "   Health check: http://" << host << ":" << port << "/health\n";
        std::cout << "   List cameras: http://" << host << ":" << port << "/api/cameras\n" << std::endl;

        // Notify FastAPI if URL is provided
        if (!fastapi_url.empty()) {
            LOG_INFO("main", "Notifying FastAPI at: " + fastapi_url);
            notify_fastapi_ready(fastapi_url);
        }

        // Main loop - wait for shutdown signal
        while (!g_shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Graceful shutdown
        std::cout << "\nShutting down gracefully..." << std::endl;

        LOG_INFO("main", "Stopping API server");
        api_server.stop();

        LOG_INFO("main", "Stopping all cameras");
        manager->stop_all(true);

        LOG_INFO("main", "Application shutdown complete");
        return 0;

    } catch (const std::exception& e) {
        LOG_ERROR("main", std::string("Fatal error: ") + e.what());
        return 1;
    }
}
