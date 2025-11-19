#ifndef GSTREAMER_WORKER_TYPES_H
#define GSTREAMER_WORKER_TYPES_H

#include <string>
#include <cstdint>

namespace gstreamer_worker {

/**
 * @brief Stream state enumeration
 */
enum class StreamState {
    STOPPED,
    STARTING,
    RUNNING,
    ERROR,
    RECONNECTING,
    STOPPING
};

/**
 * @brief Convert StreamState to string
 */
inline std::string stream_state_to_string(StreamState state) {
    switch (state) {
        case StreamState::STOPPED: return "STOPPED";
        case StreamState::STARTING: return "STARTING";
        case StreamState::RUNNING: return "RUNNING";
        case StreamState::ERROR: return "ERROR";
        case StreamState::RECONNECTING: return "RECONNECTING";
        case StreamState::STOPPING: return "STOPPING";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Stream metrics
 */
struct StreamMetrics {
    uint64_t frames_displayed = 0;
    uint64_t errors_count = 0;
    uint64_t reconnections = 0;
    double uptime_seconds = 0.0;
    std::string last_error;

    StreamMetrics() = default;
};

} // namespace gstreamer_worker

#endif // GSTREAMER_WORKER_TYPES_H
