#ifndef GSTREAMER_WORKER_WEBSOCKET_SERVER_H
#define GSTREAMER_WORKER_WEBSOCKET_SERVER_H

#include <string>
#include <memory>
#include <atomic>
#include <map>
#include <mutex>

// Forward declaration of IXWebSocket types
namespace ix {
    class WebSocketServer;
    class WebSocket;
    class ConnectionState;
}

namespace gstreamer_worker {

/**
 * @brief WebSocket server for real-time event broadcasting
 *
 * Manages WebSocket connections and broadcasts events to all connected clients.
 * Uses IXWebSocket library for the underlying WebSocket protocol implementation.
 */
class WebSocketServer {
public:
    /**
     * @brief Construct a WebSocket server
     * @param host Host address to bind to (default: "0.0.0.0")
     * @param port Port number to listen on (default: 8081)
     * @param path WebSocket endpoint path (default: "/ws")
     */
    WebSocketServer(
        const std::string& host = "0.0.0.0",
        int port = 8081,
        const std::string& path = "/ws"
    );

    ~WebSocketServer();

    /**
     * @brief Start the WebSocket server
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the WebSocket server
     */
    void stop();

    /**
     * @brief Check if server is running
     * @return true if server is currently running
     */
    bool is_running() const;

    /**
     * @brief Broadcast message to all connected clients
     * @param message Message to broadcast (JSON string)
     */
    void broadcast(const std::string& message);

    /**
     * @brief Get number of connected clients
     * @return Number of active WebSocket connections
     */
    size_t get_connected_clients() const;

    /**
     * @brief Get server host
     * @return Host address
     */
    std::string get_host() const { return host_; }

    /**
     * @brief Get server port
     * @return Port number
     */
    int get_port() const { return port_; }

    /**
     * @brief Get WebSocket path
     * @return Endpoint path
     */
    std::string get_path() const { return path_; }

private:
    std::string host_;
    int port_;
    std::string path_;
    std::atomic<bool> running_;

    std::unique_ptr<ix::WebSocketServer> server_;
    mutable std::mutex clients_mutex_;
    std::map<std::string, std::shared_ptr<ix::ConnectionState>> clients_;

    std::string log_tag_ = "[WebSocketServer]";

    /**
     * @brief Handle new WebSocket connection
     * @param webSocket Shared pointer to the new WebSocket connection
     */
    void on_connection(std::shared_ptr<ix::WebSocket> webSocket);

    /**
     * @brief Remove client from active connections
     * @param webSocket Shared pointer to the WebSocket connection
     */
    void remove_client(std::shared_ptr<ix::WebSocket> webSocket);
};

} // namespace gstreamer_worker

#endif // GSTREAMER_WORKER_WEBSOCKET_SERVER_H
