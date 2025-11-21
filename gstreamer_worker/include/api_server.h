#ifndef GSTREAMER_WORKER_API_SERVER_H
#define GSTREAMER_WORKER_API_SERVER_H

#include "pipeline_manager.h"
#include <memory>
#include <string>
#include <atomic>
#include <thread>

namespace gstreamer_worker {

/**
 * @brief REST API server for controlling the pipeline manager
 *
 * Provides HTTP endpoints:
 * - Add/remove cameras, start/stop streams, get status
 */
class APIServer {
public:
    /**
     * @brief Constructor
     * @param manager Pipeline manager to control
     * @param host Host to bind to (default: 0.0.0.0)
     * @param port Port to listen on (default: 8081)
     */
    APIServer(
        std::shared_ptr<PipelineManager> manager,
        const std::string& host = "0.0.0.0",
        int port = 8081
    );

    /**
     * @brief Destructor - stops the server
     */
    ~APIServer();

    /**
     * @brief Start the API server (non-blocking)
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the API server
     */
    void stop();

    /**
     * @brief Check if server is running
     */
    bool is_running() const;

private:
    std::shared_ptr<PipelineManager> manager_;
    std::string host_;
    int port_;
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> server_thread_;

    void* server_impl_;  // Opaque pointer to httplib::Server

    std::string log_tag_ = "APIServer";

    /**
     * @brief Main server loop (runs in separate thread)
     */
    void run_server();

    /**
     * @brief Setup API routes
     */
    void setup_routes();
};

} // namespace gstreamer_worker

#endif // GSTREAMER_WORKER_API_SERVER_H
