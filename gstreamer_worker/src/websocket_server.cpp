#include "websocket_server.h"
#include "logger.h"
#include <ixwebsocket/IXWebSocketServer.h>
#include <ixwebsocket/IXWebSocket.h>

namespace gstreamer_worker {

WebSocketServer::WebSocketServer(
    const std::string& host,
    int port,
    const std::string& path
)
    : host_(host)
    , port_(port)
    , path_(path)
    , running_(false)
{
    LOG_INFO(log_tag_, "WebSocket server initialized on " + host + ":" +
             std::to_string(port) + path);
}

WebSocketServer::~WebSocketServer() {
    stop();
}

bool WebSocketServer::start() {
    if (running_.exchange(true)) {
        LOG_WARNING(log_tag_, "WebSocket server already running");
        return false;
    }

    try {
        // Create IXWebSocket server
        server_ = std::make_unique<ix::WebSocketServer>(port_, host_);

        // Set connection handler
        server_->setOnClientMessageCallback(
            [this](std::shared_ptr<ix::ConnectionState> connectionState,
                   ix::WebSocket& webSocket,
                   const ix::WebSocketMessagePtr& msg) {

                if (msg->type == ix::WebSocketMessageType::Open) {
                    // New client connected
                    std::lock_guard<std::mutex> lock(clients_mutex_);

                    // Store connection state
                    clients_[connectionState->getId()] = connectionState;

                    LOG_INFO(log_tag_, "New WebSocket client connected. Total clients: " +
                             std::to_string(clients_.size()));

                    // Send welcome message
                    webSocket.send(R"({"event_type":"connected","data":{"message":"Connected to GStreamer Worker"}})");
                }
                else if (msg->type == ix::WebSocketMessageType::Close) {
                    // Client disconnected
                    std::lock_guard<std::mutex> lock(clients_mutex_);

                    clients_.erase(connectionState->getId());
                    LOG_INFO(log_tag_, "WebSocket client disconnected. Total clients: " +
                             std::to_string(clients_.size()));
                }
                else if (msg->type == ix::WebSocketMessageType::Message) {
                    // Received message from client (optional: handle ping/pong, commands)
                    LOG_DEBUG(log_tag_, "Received message from client: " + msg->str);
                }
                else if (msg->type == ix::WebSocketMessageType::Error) {
                    LOG_ERROR(log_tag_, "WebSocket error: " + msg->errorInfo.reason);
                }
            }
        );

        // Start listening
        auto result = server_->listen();
        if (!result.first) {
            LOG_ERROR(log_tag_, "Failed to start WebSocket server: " + result.second);
            running_ = false;
            return false;
        }

        // Start the server
        server_->start();

        LOG_INFO(log_tag_, "WebSocket server started successfully on ws://" +
                 host_ + ":" + std::to_string(port_) + path_);

        return true;

    } catch (const std::exception& e) {
        LOG_ERROR(log_tag_, std::string("Failed to start WebSocket server: ") + e.what());
        running_ = false;
        return false;
    }
}

void WebSocketServer::stop() {
    if (!running_.exchange(false)) {
        return;  // Already stopped
    }

    LOG_INFO(log_tag_, "Stopping WebSocket server...");

    try {
        // Clear client list
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            clients_.clear();
        }

        // Stop the server
        if (server_) {
            server_->stop();
            server_.reset();
        }

        LOG_INFO(log_tag_, "WebSocket server stopped");

    } catch (const std::exception& e) {
        LOG_ERROR(log_tag_, std::string("Error stopping WebSocket server: ") + e.what());
    }
}

bool WebSocketServer::is_running() const {
    return running_.load();
}

void WebSocketServer::broadcast(const std::string& message) {
    if (!server_) {
        return;
    }

    std::lock_guard<std::mutex> lock(clients_mutex_);

    if (clients_.empty()) {
        return;  // No clients to broadcast to
    }

    // Get all connected clients and send message to each
    auto clients = server_->getClients();
    for (auto& client : clients) {
        try {
            client->send(message);
        } catch (const std::exception& e) {
            LOG_ERROR(log_tag_, std::string("Error broadcasting to client: ") + e.what());
        }
    }

    LOG_DEBUG(log_tag_, "Broadcasted message to " + std::to_string(clients_.size()) + " clients");
}

size_t WebSocketServer::get_connected_clients() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    return clients_.size();
}

} // namespace gstreamer_worker
