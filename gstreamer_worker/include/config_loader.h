#ifndef GSTREAMER_WORKER_CONFIG_LOADER_H
#define GSTREAMER_WORKER_CONFIG_LOADER_H

#include "config.h"
#include "person_tracker.h"
#include <string>
#include <vector>
#include <memory>

namespace gstreamer_worker {

/**
 * @brief Configuration file loader for cameras and manager settings
 *
 * Supports JSON configuration files for easy management of multiple cameras
 */
class ConfigLoader {
public:
    /**
     * @brief Load configuration from JSON file
     * @param file_path Path to JSON configuration file
     * @param manager_config Output manager configuration
     * @param cameras Output camera configurations
     * @param person_tracking_config Output person tracking configuration
     * @return true if loaded successfully
     */
    static bool load_from_file(
        const std::string& file_path,
        PipelineManagerConfig& manager_config,
        std::vector<CameraConfig>& cameras,
        PersonTrackerConfig& person_tracking_config
    );

    /**
     * @brief Save configuration to JSON file
     * @param file_path Path to JSON configuration file
     * @return true if saved successfully
     */
    static bool save_to_file(
        const std::string& file_path,
        const PipelineManagerConfig& manager_config,
        const std::vector<CameraConfig>& cameras
    );

    /**
     * @brief Load camera configuration from JSON string
     */
    static bool load_from_string(
        const std::string& json_str,
        PipelineManagerConfig& manager_config,
        std::vector<CameraConfig>& cameras,
        PersonTrackerConfig& person_tracking_config
    );

    /**
     * @brief Validate configuration file
     */
    static bool validate_config_file(const std::string& file_path);

private:
    static CameraConfig parse_camera_config(const void* json_obj);
    static PipelineManagerConfig parse_manager_config(const void* json_obj);
    static PersonTrackerConfig parse_person_tracking_config(const void* json_obj);
};

} // namespace gstreamer_worker

#endif // GSTREAMER_WORKER_CONFIG_LOADER_H
