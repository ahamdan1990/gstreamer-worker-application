#ifndef GSTREAMER_WORKER_PIPELINE_MANAGER_H
#define GSTREAMER_WORKER_PIPELINE_MANAGER_H

#include "config.h"
#include "camera_stream.h"
#include "types.h"

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>

namespace gstreamer_worker {

/**
 * @brief Production-ready manager for multiple camera streams
 *
 * Features:
 * - Manages multiple camera streams concurrently
 * - Thread-safe operations
 * - Global metrics and monitoring
 * - Event callbacks for state changes and errors
 * - Graceful shutdown
 * - Health monitoring across all cameras
 */
class PipelineManager {
public:
    /**
     * @brief Constructor
     * @param config Manager configuration
     * @param on_state_changed Global callback for state changes
     * @param on_error Global callback for errors
     */
    PipelineManager(
        const PipelineManagerConfig& config = PipelineManagerConfig(),
        StateCallback on_state_changed = nullptr,
        ErrorCallback on_error = nullptr
    );

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~PipelineManager();

    // Delete copy/move constructors and assignment operators
    PipelineManager(const PipelineManager&) = delete;
    PipelineManager& operator=(const PipelineManager&) = delete;
    PipelineManager(PipelineManager&&) = delete;
    PipelineManager& operator=(PipelineManager&&) = delete;

    /**
     * @brief Add a camera stream
     * @param config Camera configuration
     * @return true if added successfully
     */
    bool add_camera(const CameraConfig& config);

    /**
     * @brief Add multiple camera streams
     * @param configs Vector of camera configurations
     * @return Map of camera_id to success status
     */
    std::map<std::string, bool> add_cameras(const std::vector<CameraConfig>& configs);

    /**
     * @brief Remove a camera stream
     * @param camera_id Camera identifier
     * @param wait If true, wait for stream to stop
     * @return true if removed successfully
     */
    bool remove_camera(const std::string& camera_id, bool wait = true);

    /**
     * @brief Start all camera streams
     * @return Map of camera_id to success status
     */
    std::map<std::string, bool> start_all();

    /**
     * @brief Stop all camera streams
     * @param wait If true, wait for all streams to stop
     */
    void stop_all(bool wait = true);

    /**
     * @brief Get a camera stream by ID
     * @param camera_id Camera identifier
     * @return Pointer to CameraStream or nullptr if not found
     */
    CameraStream* get_camera(const std::string& camera_id);

    /**
     * @brief Get state of a specific camera
     * @param camera_id Camera identifier
     * @return StreamState (STOPPED if not found)
     */
    StreamState get_camera_state(const std::string& camera_id) const;

    /**
     * @brief Get states of all cameras
     * @return Map of camera_id to StreamState
     */
    std::map<std::string, StreamState> get_all_states() const;

    /**
     * @brief Get metrics for a specific camera
     * @param camera_id Camera identifier
     * @return StreamMetrics (default if not found)
     */
    StreamMetrics get_camera_metrics(const std::string& camera_id) const;

    /**
     * @brief Get metrics for all cameras
     * @return Map of camera_id to StreamMetrics
     */
    std::map<std::string, StreamMetrics> get_all_metrics() const;

    /**
     * @brief Check if manager is running
     */
    bool is_running() const;

    /**
     * @brief Get number of cameras
     */
    size_t get_camera_count() const;

    /**
     * @brief Get number of running cameras
     */
    size_t get_running_camera_count() const;

private:
    // Configuration
    PipelineManagerConfig config_;
    std::string log_tag_ = "PipelineManager";

    // Callbacks
    StateCallback on_state_changed_;
    ErrorCallback on_error_;

    // Camera streams
    std::map<std::string, std::unique_ptr<CameraStream>> streams_;
    mutable std::mutex streams_mutex_;

    // Manager state
    std::atomic<bool> running_;

    // Metrics collection
    std::unique_ptr<std::thread> metrics_timer_;
    std::chrono::steady_clock::time_point manager_start_time_;

    /**
     * @brief Handle state change from camera stream
     */
    void handle_state_change(const std::string& camera_id, StreamState new_state);

    /**
     * @brief Handle error from camera stream
     */
    void handle_error(const std::string& camera_id, const std::string& error_msg);

    /**
     * @brief Start periodic metrics collection
     */
    void start_metrics_collection();

    /**
     * @brief Collect and log metrics
     */
    void collect_metrics();
};

} // namespace gstreamer_worker

#endif // GSTREAMER_WORKER_PIPELINE_MANAGER_H
