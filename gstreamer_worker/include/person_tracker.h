#ifndef GSTREAMER_WORKER_PERSON_TRACKER_H
#define GSTREAMER_WORKER_PERSON_TRACKER_H

#include "compreface_client.h"
#include "types.h"

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <mutex>
#include <functional>

namespace gstreamer_worker {

/**
 * @brief Person appearance at a specific camera
 */
struct PersonAppearance {
    std::string camera_id;
    std::chrono::system_clock::time_point first_seen;
    std::chrono::system_clock::time_point last_seen;
    int detection_count = 0;
    float avg_confidence = 0.0f;
    std::vector<FaceDetection> detections;  // Recent detections

    // Calculate dwell time in seconds
    double dwell_time_seconds() const {
        return std::chrono::duration<double>(last_seen - first_seen).count();
    }
};

/**
 * @brief Person presence across multiple cameras
 */
struct PersonPresence {
    std::string subject;  // Person ID
    float last_similarity;  // Last recognition similarity
    std::chrono::system_clock::time_point first_seen_global;  // First appearance anywhere
    std::chrono::system_clock::time_point last_seen_global;   // Last appearance anywhere

    // Per-camera appearances
    std::map<std::string, PersonAppearance> camera_appearances;

    // Total statistics
    int total_detections = 0;
    float avg_similarity = 0.0f;

    // Get total dwell time across all cameras
    double total_dwell_time_seconds() const {
        double total = 0.0;
        for (const auto& [camera_id, appearance] : camera_appearances) {
            total += appearance.dwell_time_seconds();
        }
        return total;
    }

    // Get list of cameras where person was seen
    std::vector<std::string> get_camera_list() const {
        std::vector<std::string> cameras;
        for (const auto& [camera_id, _] : camera_appearances) {
            cameras.push_back(camera_id);
        }
        return cameras;
    }
};

/**
 * @brief Event triggered when person is recognized
 */
struct PersonRecognitionEvent {
    std::string subject;           // Person ID
    std::string camera_id;         // Camera where recognized
    float similarity;              // Recognition similarity
    float detection_confidence;    // Detection confidence
    std::chrono::system_clock::time_point timestamp;
    bool is_new_person;            // First time seeing this person
    bool is_new_at_camera;         // First time at this camera
    double dwell_time_seconds;     // How long at this camera
    std::vector<std::string> other_cameras;  // Other cameras where seen
};

/**
 * @brief Callback for person recognition events
 */
using PersonRecognitionCallback = std::function<void(const PersonRecognitionEvent& event)>;

/**
 * @brief Configuration for person tracking
 */
struct PersonTrackerConfig {
    double presence_timeout_seconds = 300.0;  // Remove person after 5 min of no detection
    double same_camera_cooldown_seconds = 10.0;  // Cooldown before re-alerting at same camera
    int max_detections_per_person = 100;     // Max detections to store per person
    bool enable_cross_camera_tracking = true;  // Track across cameras
    bool enable_persistence = false;          // Save to database/file
    std::string persistence_path = "./person_tracking.json";  // Where to save data

    bool validate() const {
        return presence_timeout_seconds > 0.0 &&
               same_camera_cooldown_seconds >= 0.0 &&
               max_detections_per_person > 0;
    }
};

/**
 * @brief Enterprise-grade person tracking system
 *
 * Features:
 * - Track persons across multiple cameras
 * - Calculate dwell time per camera and total
 * - Historical presence data
 * - Thread-safe operations
 * - Optional persistence (JSON/database)
 * - Real-time event notifications
 * - Automatic cleanup of stale data
 */
class PersonTracker {
public:
    /**
     * @brief Constructor
     * @param config Tracker configuration
     * @param callback Event callback
     */
    explicit PersonTracker(
        const PersonTrackerConfig& config,
        PersonRecognitionCallback callback = nullptr
    );

    /**
     * @brief Destructor
     */
    ~PersonTracker();

    // Delete copy/move constructors
    PersonTracker(const PersonTracker&) = delete;
    PersonTracker& operator=(const PersonTracker&) = delete;
    PersonTracker(PersonTracker&&) = delete;
    PersonTracker& operator=(PersonTracker&&) = delete;

    /**
     * @brief Update person tracking with recognition result
     * @param camera_id Camera identifier
     * @param result Recognition result from CompreFace
     * @param detection Face detection data
     */
    void update_person(
        const std::string& camera_id,
        const RecognitionResult& result,
        const FaceDetection& detection
    );

    /**
     * @brief Get current presence of a person
     * @param subject Person ID
     * @return Person presence or nullopt if not tracked
     */
    std::optional<PersonPresence> get_person_presence(
        const std::string& subject
    ) const;

    /**
     * @brief Get all currently tracked persons
     */
    std::vector<PersonPresence> get_all_persons() const;

    /**
     * @brief Get persons currently at a specific camera
     * @param camera_id Camera identifier
     * @param max_age_seconds Maximum age of last detection (default: 60s)
     * @return List of persons at camera
     */
    std::vector<PersonPresence> get_persons_at_camera(
        const std::string& camera_id,
        double max_age_seconds = 60.0
    ) const;

    /**
     * @brief Get persons who visited multiple cameras
     * @return Persons with cross-camera presence
     */
    std::vector<PersonPresence> get_cross_camera_persons() const;

    /**
     * @brief Remove person from tracking
     * @param subject Person ID
     */
    void remove_person(const std::string& subject);

    /**
     * @brief Clear all tracking data
     */
    void clear_all();

    /**
     * @brief Save tracking data to file (if persistence enabled)
     */
    bool save_to_file();

    /**
     * @brief Load tracking data from file
     */
    bool load_from_file();

    /**
     * @brief Get statistics
     */
    struct Statistics {
        uint64_t total_persons_tracked = 0;
        uint64_t currently_tracked = 0;
        uint64_t cross_camera_persons = 0;
        uint64_t total_events = 0;
        double avg_dwell_time_seconds = 0.0;
    };

    Statistics get_statistics() const;

    /**
     * @brief Update configuration
     */
    void update_config(const PersonTrackerConfig& config);

private:
    PersonTrackerConfig config_;
    PersonRecognitionCallback callback_;

    mutable std::mutex mutex_;
    std::map<std::string, PersonPresence> persons_;  // subject -> presence
    std::map<std::string, std::chrono::system_clock::time_point> last_alert_time_;  // subject+camera -> time

    Statistics stats_;

    /**
     * @brief Check if person should trigger an event at camera
     */
    bool should_trigger_event(
        const std::string& subject,
        const std::string& camera_id
    );

    /**
     * @brief Cleanup stale person data
     */
    void cleanup_stale_data();
};

} // namespace gstreamer_worker

#endif // GSTREAMER_WORKER_PERSON_TRACKER_H
