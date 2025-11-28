#include "person_tracker.h"
#include "logger.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace gstreamer_worker {

using json = nlohmann::json;

// ========================================
// PersonTracker Implementation
// ========================================

PersonTracker::PersonTracker(
    const PersonTrackerConfig& config,
    PersonRecognitionCallback callback
)
    : config_(config)
    , callback_(callback)
    , running_(true)
    , last_reset_time_(std::chrono::system_clock::now())
{
    if (!config_.validate()) {
        throw std::invalid_argument("Invalid PersonTrackerConfig");
    }

    // Load existing tracking data if persistence is enabled
    if (config_.enable_persistence) {
        load_from_file();
    }

    std::cout << "[PersonTracker] Initialized with:" << std::endl;
    std::cout << "  - Presence timeout: " << config_.presence_timeout_seconds << "s" << std::endl;
    std::cout << "  - Camera cooldown: " << config_.same_camera_cooldown_seconds << "s" << std::endl;
    std::cout << "  - Cross-camera tracking: " << (config_.enable_cross_camera_tracking ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "  - Persistence: " << (config_.enable_persistence ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "  - Daily reset: " << (config_.enable_daily_reset ? "ENABLED" : "DISABLED");
    if (config_.enable_daily_reset) {
        std::cout << " (at " << config_.daily_reset_hour << ":00)";
    }
    std::cout << std::endl;

    // Start daily reset thread if enabled
    if (config_.enable_daily_reset) {
        daily_reset_thread_ = std::thread(&PersonTracker::daily_reset_worker, this);
        std::cout << "[PersonTracker] Daily reset thread started" << std::endl;
    }
}

PersonTracker::~PersonTracker() {
    // Stop daily reset thread
    running_ = false;
    reset_cv_.notify_all();
    if (daily_reset_thread_.joinable()) {
        daily_reset_thread_.join();
        std::cout << "[PersonTracker] Daily reset thread stopped" << std::endl;
    }

    // Save data before shutdown if persistence enabled
    if (config_.enable_persistence) {
        save_to_file();
    }

    std::cout << "[PersonTracker] Shutdown - Tracked " << stats_.total_persons_tracked << " persons total" << std::endl;
}

void PersonTracker::update_person(
    const std::string& camera_id,
    const RecognitionResult& result,
    const FaceDetection& detection,
    const std::string& face_crop_path
) {
    if (result.subject == "unknown" || !result.is_match) {
        return;  // Only track recognized persons
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    const std::string& subject = result.subject;

    // Check if this is a new person
    bool is_new_person = (persons_.find(subject) == persons_.end());

    // Get or create person presence
    PersonPresence& presence = persons_[subject];

    if (is_new_person) {
        presence.subject = subject;
        presence.first_seen_global = now;
        presence.avg_similarity = result.similarity;
        stats_.total_persons_tracked++;
    } else {
        // Update running average similarity
        presence.avg_similarity = (presence.avg_similarity * presence.total_detections + result.similarity)
                                 / (presence.total_detections + 1);
    }

    presence.last_seen_global = now;
    presence.last_similarity = result.similarity;
    presence.total_detections++;

    // Check if this is first time at this camera
    bool is_new_at_camera = (presence.camera_appearances.find(camera_id) == presence.camera_appearances.end());

    // Get or create camera appearance
    PersonAppearance& appearance = presence.camera_appearances[camera_id];

    if (is_new_at_camera) {
        appearance.camera_id = camera_id;
        appearance.first_seen = now;
    }

    appearance.last_seen = now;
    appearance.detection_count++;

    // Update running average confidence
    appearance.avg_confidence = (appearance.avg_confidence * (appearance.detection_count - 1) + detection.confidence)
                               / appearance.detection_count;

    // Store detection (limit to max)
    appearance.detections.push_back(detection);
    if (appearance.detections.size() > static_cast<size_t>(config_.max_detections_per_person)) {
        appearance.detections.erase(appearance.detections.begin());
    }

    // Check if we should trigger an event
    if (should_trigger_event(subject, camera_id)) {
        // Update last alert time
        last_alert_time_[subject + "_" + camera_id] = now;
        stats_.total_events++;

        // Create event
        PersonRecognitionEvent event;
        event.subject = subject;
        event.camera_id = camera_id;
        event.similarity = result.similarity;
        event.detection_confidence = detection.confidence;
        event.timestamp = now;
        event.is_new_person = is_new_person;
        event.is_new_at_camera = is_new_at_camera;
        event.dwell_time_seconds = appearance.dwell_time_seconds();
        event.other_cameras = presence.get_camera_list();
        event.face_crop_path = face_crop_path;

        // Remove current camera from other_cameras list
        event.other_cameras.erase(
            std::remove(event.other_cameras.begin(), event.other_cameras.end(), camera_id),
            event.other_cameras.end()
        );

        // Trigger callback
        if (callback_) {
            callback_(event);
        }

        // Log event
        std::cout << "[PersonTracker] 🎯 Event: " << subject << " @ " << camera_id;
        if (is_new_person) {
            std::cout << " (NEW PERSON)";
        } else if (is_new_at_camera) {
            std::cout << " (NEW AT CAMERA)";
        }
        std::cout << " | Dwell: " << std::fixed << std::setprecision(1) << event.dwell_time_seconds << "s";
        if (!event.other_cameras.empty()) {
            std::cout << " | Also seen at: ";
            for (size_t i = 0; i < event.other_cameras.size(); i++) {
                std::cout << event.other_cameras[i];
                if (i < event.other_cameras.size() - 1) std::cout << ", ";
            }
        }
        std::cout << std::endl;
    }

    // Cleanup stale data periodically
    static int update_counter = 0;
    if (++update_counter % 100 == 0) {
        cleanup_stale_data();
    }
}

std::optional<PersonPresence> PersonTracker::get_person_presence(
    const std::string& subject
) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = persons_.find(subject);
    if (it != persons_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<PersonPresence> PersonTracker::get_all_persons() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<PersonPresence> result;
    result.reserve(persons_.size());

    for (const auto& [subject, presence] : persons_) {
        result.push_back(presence);
    }

    return result;
}

std::vector<PersonPresence> PersonTracker::get_persons_at_camera(
    const std::string& camera_id,
    double max_age_seconds
) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<PersonPresence> result;
    auto now = std::chrono::system_clock::now();

    for (const auto& [subject, presence] : persons_) {
        auto it = presence.camera_appearances.find(camera_id);
        if (it != presence.camera_appearances.end()) {
            auto age = std::chrono::duration<double>(now - it->second.last_seen).count();
            if (age <= max_age_seconds) {
                result.push_back(presence);
            }
        }
    }

    return result;
}

std::vector<PersonPresence> PersonTracker::get_cross_camera_persons() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<PersonPresence> result;

    for (const auto& [subject, presence] : persons_) {
        if (presence.camera_appearances.size() > 1) {
            result.push_back(presence);
        }
    }

    return result;
}

void PersonTracker::remove_person(const std::string& subject) {
    std::lock_guard<std::mutex> lock(mutex_);
    persons_.erase(subject);

    // Remove from last_alert_time_
    for (auto it = last_alert_time_.begin(); it != last_alert_time_.end(); ) {
        if (it->first.find(subject) == 0) {
            it = last_alert_time_.erase(it);
        } else {
            ++it;
        }
    }
}

void PersonTracker::clear_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    persons_.clear();
    last_alert_time_.clear();

    std::cout << "[PersonTracker] Cleared all tracking data" << std::endl;
}

bool PersonTracker::save_to_file() {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        json j;
        j["version"] = "1.0";
        j["timestamp"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        j["total_persons_tracked"] = stats_.total_persons_tracked;
        j["total_events"] = stats_.total_events;

        // Serialize persons
        json persons_array = json::array();
        for (const auto& [subject, presence] : persons_) {
            json person_obj;
            person_obj["subject"] = presence.subject;
            person_obj["last_similarity"] = presence.last_similarity;
            person_obj["first_seen_global"] = std::chrono::system_clock::to_time_t(presence.first_seen_global);
            person_obj["last_seen_global"] = std::chrono::system_clock::to_time_t(presence.last_seen_global);
            person_obj["total_detections"] = presence.total_detections;
            person_obj["avg_similarity"] = presence.avg_similarity;

            // Serialize camera appearances
            json cameras_obj;
            for (const auto& [camera_id, appearance] : presence.camera_appearances) {
                json appearance_obj;
                appearance_obj["camera_id"] = appearance.camera_id;
                appearance_obj["first_seen"] = std::chrono::system_clock::to_time_t(appearance.first_seen);
                appearance_obj["last_seen"] = std::chrono::system_clock::to_time_t(appearance.last_seen);
                appearance_obj["detection_count"] = appearance.detection_count;
                appearance_obj["avg_confidence"] = appearance.avg_confidence;
                appearance_obj["dwell_time_seconds"] = appearance.dwell_time_seconds();

                cameras_obj[camera_id] = appearance_obj;
            }
            person_obj["camera_appearances"] = cameras_obj;
            person_obj["total_dwell_time_seconds"] = presence.total_dwell_time_seconds();

            persons_array.push_back(person_obj);
        }
        j["persons"] = persons_array;

        // Write to file
        std::ofstream file(config_.persistence_path);
        if (!file.is_open()) {
            std::cerr << "[PersonTracker] Failed to open persistence file for writing: "
                      << config_.persistence_path << std::endl;
            return false;
        }

        file << j.dump(2);  // Pretty print with 2-space indent
        file.close();

        std::cout << "[PersonTracker] Saved tracking data: " << persons_.size()
                  << " persons to " << config_.persistence_path << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[PersonTracker] Failed to save tracking data: " << e.what() << std::endl;
        return false;
    }
}

bool PersonTracker::load_from_file() {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        std::ifstream file(config_.persistence_path);
        if (!file.is_open()) {
            std::cout << "[PersonTracker] No existing persistence file found" << std::endl;
            return false;
        }

        json j;
        file >> j;
        file.close();

        // Load statistics
        if (j.contains("total_persons_tracked")) {
            stats_.total_persons_tracked = j["total_persons_tracked"].get<uint64_t>();
        }
        if (j.contains("total_events")) {
            stats_.total_events = j["total_events"].get<uint64_t>();
        }

        // Load persons
        if (j.contains("persons") && j["persons"].is_array()) {
            for (const auto& person_obj : j["persons"]) {
                PersonPresence presence;
                presence.subject = person_obj["subject"].get<std::string>();
                presence.last_similarity = person_obj["last_similarity"].get<float>();
                presence.first_seen_global = std::chrono::system_clock::from_time_t(
                    person_obj["first_seen_global"].get<time_t>()
                );
                presence.last_seen_global = std::chrono::system_clock::from_time_t(
                    person_obj["last_seen_global"].get<time_t>()
                );
                presence.total_detections = person_obj["total_detections"].get<int>();
                presence.avg_similarity = person_obj["avg_similarity"].get<float>();

                // Load camera appearances
                if (person_obj.contains("camera_appearances")) {
                    for (const auto& [camera_id, appearance_obj] : person_obj["camera_appearances"].items()) {
                        PersonAppearance appearance;
                        appearance.camera_id = appearance_obj["camera_id"].get<std::string>();
                        appearance.first_seen = std::chrono::system_clock::from_time_t(
                            appearance_obj["first_seen"].get<time_t>()
                        );
                        appearance.last_seen = std::chrono::system_clock::from_time_t(
                            appearance_obj["last_seen"].get<time_t>()
                        );
                        appearance.detection_count = appearance_obj["detection_count"].get<int>();
                        appearance.avg_confidence = appearance_obj["avg_confidence"].get<float>();

                        presence.camera_appearances[camera_id] = appearance;
                    }
                }

                persons_[presence.subject] = presence;
            }
        }

        std::cout << "[PersonTracker] Loaded " << persons_.size()
                  << " persons from " << config_.persistence_path << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[PersonTracker] Failed to load tracking data: " << e.what() << std::endl;
        return false;
    }
}

PersonTracker::Statistics PersonTracker::get_statistics() const {
    std::lock_guard<std::mutex> lock(mutex_);

    Statistics stats = stats_;
    stats.currently_tracked = persons_.size();
    stats.cross_camera_persons = 0;

    double total_dwell = 0.0;
    int count = 0;

    for (const auto& [subject, presence] : persons_) {
        if (presence.camera_appearances.size() > 1) {
            stats.cross_camera_persons++;
        }

        double dwell = presence.total_dwell_time_seconds();
        if (dwell > 0.0) {
            total_dwell += dwell;
            count++;
        }
    }

    if (count > 0) {
        stats.avg_dwell_time_seconds = total_dwell / count;
    }

    return stats;
}

void PersonTracker::update_config(const PersonTrackerConfig& config) {
    if (!config.validate()) {
        std::cerr << "[PersonTracker] Invalid configuration provided" << std::endl;
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;

    std::cout << "[PersonTracker] Configuration updated" << std::endl;
}

bool PersonTracker::should_trigger_event(
    const std::string& subject,
    const std::string& camera_id
) {
    std::string key = subject + "_" + camera_id;
    auto it = last_alert_time_.find(key);

    if (it == last_alert_time_.end()) {
        return true;  // First detection at this camera
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration<double>(now - it->second).count();

    return elapsed >= config_.same_camera_cooldown_seconds;
}

void PersonTracker::cleanup_stale_data() {
    auto now = std::chrono::system_clock::now();

    for (auto it = persons_.begin(); it != persons_.end(); ) {
        auto elapsed = std::chrono::duration<double>(now - it->second.last_seen_global).count();

        if (elapsed > config_.presence_timeout_seconds) {
            std::cout << "[PersonTracker] Removing stale person: " << it->first
                      << " (last seen " << std::fixed << std::setprecision(1)
                      << elapsed << "s ago)" << std::endl;
            it = persons_.erase(it);
        } else {
            ++it;
        }
    }
}

void PersonTracker::daily_reset_worker() {
    std::cout << "[PersonTracker] Daily reset worker started" << std::endl;

    while (running_) {
        // Check every hour if it's time to reset
        std::unique_lock<std::mutex> lock(mutex_);
        reset_cv_.wait_for(lock, std::chrono::hours(1), [this] { return !running_; });

        if (!running_) break;

        if (should_perform_reset()) {
            lock.unlock();  // Release lock before performing reset
            perform_daily_reset();
        }
    }

    std::cout << "[PersonTracker] Daily reset worker stopped" << std::endl;
}

bool PersonTracker::should_perform_reset() const {
    if (!config_.enable_daily_reset) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm = *std::localtime(&now_time_t);

    auto last_reset_time_t = std::chrono::system_clock::to_time_t(last_reset_time_);
    std::tm last_reset_tm = *std::localtime(&last_reset_time_t);

    // Check if we're on a different day and past the reset hour
    bool different_day = (now_tm.tm_year != last_reset_tm.tm_year ||
                         now_tm.tm_mon != last_reset_tm.tm_mon ||
                         now_tm.tm_mday != last_reset_tm.tm_mday);

    bool past_reset_hour = (now_tm.tm_hour >= config_.daily_reset_hour);

    return different_day && past_reset_hour;
}

bool PersonTracker::perform_daily_reset() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::cout << "[PersonTracker] ⏰ Performing daily reset..." << std::endl;

    try {
        // Get current date for archive filename
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm = *std::localtime(&now_time_t);

        std::ostringstream archive_filename;
        archive_filename << config_.archive_path << "/"
                        << "person_tracking_"
                        << std::put_time(&now_tm, "%Y-%m-%d_%H-%M-%S")
                        << ".json";

        // Create archive directory if it doesn't exist
        std::string mkdir_cmd = "mkdir -p " + config_.archive_path;
        int result = std::system(mkdir_cmd.c_str());
        if (result != 0) {
            std::cerr << "[PersonTracker] Failed to create archive directory: " << config_.archive_path << std::endl;
        }

        // Save current tracking data to archive
        json j;
        j["version"] = "1.0";
        j["reset_time"] = std::chrono::system_clock::to_time_t(now);
        j["total_persons_tracked"] = stats_.total_persons_tracked;
        j["total_events"] = stats_.total_events;

        // Serialize persons
        json persons_array = json::array();
        for (const auto& [subject, presence] : persons_) {
            json person_obj;
            person_obj["subject"] = presence.subject;
            person_obj["last_similarity"] = presence.last_similarity;
            person_obj["first_seen_global"] = std::chrono::system_clock::to_time_t(presence.first_seen_global);
            person_obj["last_seen_global"] = std::chrono::system_clock::to_time_t(presence.last_seen_global);
            person_obj["total_detections"] = presence.total_detections;
            person_obj["avg_similarity"] = presence.avg_similarity;

            // Serialize camera appearances
            json cameras_obj;
            for (const auto& [camera_id, appearance] : presence.camera_appearances) {
                json appearance_obj;
                appearance_obj["camera_id"] = appearance.camera_id;
                appearance_obj["first_seen"] = std::chrono::system_clock::to_time_t(appearance.first_seen);
                appearance_obj["last_seen"] = std::chrono::system_clock::to_time_t(appearance.last_seen);
                appearance_obj["detection_count"] = appearance.detection_count;
                appearance_obj["avg_confidence"] = appearance.avg_confidence;
                appearance_obj["dwell_time_seconds"] = appearance.dwell_time_seconds();

                cameras_obj[camera_id] = appearance_obj;
            }
            person_obj["camera_appearances"] = cameras_obj;
            person_obj["total_dwell_time_seconds"] = presence.total_dwell_time_seconds();

            persons_array.push_back(person_obj);
        }
        j["persons"] = persons_array;

        // Write archive file
        std::ofstream archive_file(archive_filename.str());
        if (archive_file.is_open()) {
            archive_file << j.dump(2);
            archive_file.close();
            std::cout << "[PersonTracker] ✅ Archived " << persons_.size()
                     << " persons to " << archive_filename.str() << std::endl;
        } else {
            std::cerr << "[PersonTracker] Failed to create archive file: "
                     << archive_filename.str() << std::endl;
        }

        // Clear current tracking data
        persons_.clear();
        last_alert_time_.clear();

        // Update last reset time
        last_reset_time_ = now;

        std::cout << "[PersonTracker] ✅ Daily reset complete - tracking data cleared" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[PersonTracker] Daily reset failed: " << e.what() << std::endl;
        return false;
    }
}

} // namespace gstreamer_worker
