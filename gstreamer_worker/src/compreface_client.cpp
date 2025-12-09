#include "compreface_client.h"
#include "logger.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <queue>
#include <unordered_map>
#include <condition_variable>
#include <atomic>
#include <cmath>

namespace gstreamer_worker {

using json = nlohmann::json;

// Helper for CURL response
struct CurlResponse {
    std::string data;
    long http_code = 0;

    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }
};

// Recognition request
struct RecognitionRequest {
    std::string camera_id;
    std::string face_crop_path;
    float detection_confidence;
    int retry_count = 0;
    std::chrono::steady_clock::time_point queued_time;
};

// Cache entry
struct CacheEntry {
    RecognitionResult result;
    std::chrono::steady_clock::time_point timestamp;
};

// Circuit breaker states
enum class CircuitState {
    CLOSED,      // Normal operation
    OPEN,        // Failing, reject requests
    HALF_OPEN    // Testing if recovered
};

/**
 * @brief CompreFace client implementation (PIMPL)
 */
struct CompreFaceClient::Impl {
    // Configuration
    CompreFaceConfig config_;
    RecognitionCallback callback_;
    mutable std::mutex config_mutex_;

    // Worker threads
    std::vector<std::thread> workers_;
    std::atomic<bool> running_{false};

    // Request queue
    std::queue<RecognitionRequest> request_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // Result cache
    std::unordered_map<std::string, CacheEntry> cache_;
    mutable std::mutex cache_mutex_;

    // Statistics
    CompreFaceClient::Statistics stats_;
    mutable std::mutex stats_mutex_;
    std::chrono::steady_clock::time_point last_stats_update_;

    // Circuit breaker
    CircuitState circuit_state_{CircuitState::CLOSED};
    std::atomic<int> consecutive_failures_{0};
    std::atomic<int> consecutive_successes_{0};
    std::chrono::steady_clock::time_point circuit_open_time_;
    mutable std::mutex circuit_mutex_;

    // Circuit breaker thresholds
    static constexpr int FAILURE_THRESHOLD = 5;
    static constexpr int SUCCESS_THRESHOLD = 2;
    static constexpr int CIRCUIT_OPEN_DURATION_SEC = 30;

    Impl(const CompreFaceConfig& config, RecognitionCallback callback)
        : config_(config)
        , callback_(callback)
        , last_stats_update_(std::chrono::steady_clock::now())
    {
        // Initialize CURL globally
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    ~Impl() {
        curl_global_cleanup();
    }

    bool start() {
        if (running_.load()) {
            return false;
        }

        running_.store(true);

        // Start worker threads (2 threads for parallel recognition)
        int num_workers = 2;
        for (int i = 0; i < num_workers; i++) {
            workers_.emplace_back([this]() { worker_loop(); });
        }

        std::cout << "[CompreFaceClient] Started with " << num_workers << " workers" << std::endl;
        return true;
    }

    void stop() {
        if (!running_.load()) {
            return;
        }

        running_.store(false);
        queue_cv_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        workers_.clear();
        std::cout << "[CompreFaceClient] Stopped" << std::endl;
    }

    bool recognize_async(
        const std::string& camera_id,
        const std::string& face_crop_path,
        float detection_confidence
    ) {
        // Check if circuit is open
        if (is_circuit_open()) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.failed_requests++;
            return false;
        }

        // Check queue size
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (request_queue_.size() >= static_cast<size_t>(config_.max_queue_size)) {
                std::cerr << "[CompreFaceClient] Queue full, dropping request" << std::endl;
                return false;
            }

            // Add to queue
            RecognitionRequest req;
            req.camera_id = camera_id;
            req.face_crop_path = face_crop_path;
            req.detection_confidence = detection_confidence;
            req.queued_time = std::chrono::steady_clock::now();
            request_queue_.push(req);
        }

        queue_cv_.notify_one();
        return true;
    }

    std::optional<RecognitionResult> recognize_sync(const std::string& face_crop_path) {
        // Check cache first
        if (config_.cache_results) {
            auto cached = get_cached_result(face_crop_path);
            if (cached.has_value()) {
                return cached;
            }
        }

        // Perform recognition
        return perform_recognition(face_crop_path);
    }

    std::optional<RecognitionResult> get_cached_result(const std::string& face_crop_path) const {
        std::lock_guard<std::mutex> lock(cache_mutex_);

        auto it = cache_.find(face_crop_path);
        if (it == cache_.end()) {
            return std::nullopt;
        }

        // Check if cache entry is still valid
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration<double>(now - it->second.timestamp).count();

        if (age > config_.cache_duration_seconds) {
            return std::nullopt;
        }

        // Update statistics
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            const_cast<Impl*>(this)->stats_.cache_hits++;
        }

        return it->second.result;
    }

    void update_config(const CompreFaceConfig& config) {
        std::lock_guard<std::mutex> lock(config_mutex_);
        config_ = config;
        std::cout << "[CompreFaceClient] Configuration updated" << std::endl;
    }

    CompreFaceClient::Statistics get_statistics() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        std::lock_guard<std::mutex> queue_lock(queue_mutex_);

        auto stats = stats_;
        stats.queue_size = request_queue_.size();
        stats.circuit_breaker_open = (circuit_state_ == CircuitState::OPEN);

        return stats;
    }

    bool is_healthy() const {
        std::lock_guard<std::mutex> lock(circuit_mutex_);
        return circuit_state_ != CircuitState::OPEN;
    }

private:
    void worker_loop() {
        while (running_.load()) {
            RecognitionRequest req;

            // Wait for request
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [this] {
                    return !request_queue_.empty() || !running_.load();
                });

                if (!running_.load()) {
                    break;
                }

                req = request_queue_.front();
                request_queue_.pop();
            }

            // Check cache first
            std::optional<RecognitionResult> cached_result;
            if (config_.cache_results) {
                cached_result = get_cached_result(req.face_crop_path);
            }

            std::optional<RecognitionResult> result;
            std::string error;

            if (cached_result.has_value()) {
                result = cached_result;
            } else {
                // Perform recognition with retries
                auto start_time = std::chrono::high_resolution_clock::now();
                result = perform_recognition_with_retry(req);
                auto end_time = std::chrono::high_resolution_clock::now();

                double response_time_ms = std::chrono::duration<double, std::milli>(
                    end_time - start_time
                ).count();

                // Update statistics
                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.total_requests++;

                    if (result.has_value()) {
                        stats_.successful_recognitions++;
                        record_success();

                        // Update average response time (exponential moving average)
                        const double alpha = 0.2;
                        stats_.avg_response_time_ms = alpha * response_time_ms +
                                                     (1.0 - alpha) * stats_.avg_response_time_ms;
                    } else {
                        stats_.failed_requests++;
                        error = "Recognition failed after retries";
                        record_failure();
                    }
                }

                // Cache result
                if (result.has_value() && config_.cache_results) {
                    std::lock_guard<std::mutex> lock(cache_mutex_);
                    CacheEntry entry;
                    entry.result = result.value();
                    entry.timestamp = std::chrono::steady_clock::now();
                    cache_[req.face_crop_path] = entry;
                }
            }

            // Invoke callback
            if (callback_) {
                callback_(req.camera_id, req.face_crop_path, result, error);
            }
        }
    }

    std::optional<RecognitionResult> perform_recognition_with_retry(RecognitionRequest& req) {
        int max_retries = config_.max_retries;

        for (int attempt = 0; attempt <= max_retries; attempt++) {
            // Check if circuit is open
            if (is_circuit_open()) {
                return std::nullopt;
            }

            auto result = perform_recognition(req.face_crop_path);
            if (result.has_value()) {
                return result;
            }

            // Exponential backoff before retry
            if (attempt < max_retries) {
                int delay_ms = static_cast<int>(std::pow(2, attempt) * 100);
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            }
        }

        return std::nullopt;
    }

    std::optional<RecognitionResult> perform_recognition(const std::string& face_crop_path) {
        // Read image file
        std::ifstream file(face_crop_path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[CompreFaceClient] Failed to open: " << face_crop_path << std::endl;
            return std::nullopt;
        }

        std::vector<char> image_data((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
        file.close();

        if (image_data.empty()) {
            std::cerr << "[CompreFaceClient] Empty image file: " << face_crop_path << std::endl;
            return std::nullopt;
        }

        // Prepare CURL request
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::cerr << "[CompreFaceClient] Failed to initialize CURL" << std::endl;
            return std::nullopt;
        }

        CurlResponse response;
        struct curl_slist* headers = nullptr;

        // Set headers
        std::string api_key_header = "x-api-key: " + config_.api_key;
        headers = curl_slist_append(headers, api_key_header.c_str());

        // Build recognition URL with query parameters
        // det_prob_threshold: face detection confidence threshold (0-1)
        // limit: max number of faces to return (0 = all)
        // prediction_count: number of predictions per face
        // Use 0.2 threshold to match our SCRFD detector (0.3) with margin
        std::string url = config_.base_url + "/api/v1/recognition/recognize?limit=0&det_prob_threshold=0.2&prediction_count=1";
        if (!config_.recognition_service_url.empty()) {
            url = config_.recognition_service_url;
        }

        // Prepare multipart form data
        curl_mime* mime = curl_mime_init(curl);
        curl_mimepart* part = curl_mime_addpart(mime);
        curl_mime_name(part, "file");
        curl_mime_data(part, image_data.data(), image_data.size());
        curl_mime_filename(part, "face.jpg");
        curl_mime_type(part, "image/jpeg");

        // Set CURL options
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlResponse::write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, config_.timeout_ms);

        // Perform request
        CURLcode res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.http_code);

        // Cleanup
        curl_slist_free_all(headers);
        curl_mime_free(mime);
        curl_easy_cleanup(curl);

        // Check for errors
        if (res != CURLE_OK) {
            std::cerr << "[CompreFaceClient] CURL error: " << curl_easy_strerror(res) << std::endl;
            return std::nullopt;
        }

        if (response.http_code != 200) {
            std::cerr << "[CompreFaceClient] HTTP error: " << response.http_code << std::endl;
            std::cerr << "[CompreFaceClient] URL: " << url << std::endl;
            std::cerr << "[CompreFaceClient] Response: " << response.data << std::endl;
            return std::nullopt;
        }

        // Parse JSON response
        try {
            json j = json::parse(response.data);

            // DEBUG: Show full CompreFace response
            std::cout << "[CompreFaceClient] ✅ SUCCESS Response for: " << face_crop_path << std::endl;
            std::cout << "[CompreFaceClient] Full JSON Response:" << std::endl;
            std::cout << j.dump(2) << std::endl;  // Pretty print with 2-space indent

            // CompreFace response format:
            // {
            //   "result": [
            //     {
            //       "subject": "person_name",
            //       "similarity": 0.95,
            //       "face_id": "uuid"
            //     }
            //   ]
            // }

            if (!j.contains("result") || !j["result"].is_array() || j["result"].empty()) {
                // No match found (face detected but not recognized in database)
                std::cout << "[CompreFaceClient] ℹ️  Face detected but NO MATCH in database (empty result array)" << std::endl;
                std::cout << "[CompreFaceClient] Full response: " << j.dump(2) << std::endl;
                return std::nullopt;
            }

            // CRITICAL FIX: Process ALL faces returned by CompreFace, not just result[0]
            // When multiple people are in the same crop (due to close proximity or crop margin),
            // CompreFace will return multiple faces in the result array.
            auto& results_array = j["result"];
            int num_faces_detected = results_array.size();

            if (num_faces_detected > 1) {
                std::cout << "[CompreFaceClient] ⚠️  MULTIPLE FACES detected in single crop: "
                          << num_faces_detected << " faces" << std::endl;
                std::cout << "[CompreFaceClient] This happens when:" << std::endl;
                std::cout << "[CompreFaceClient]   1. Multiple people are close together" << std::endl;
                std::cout << "[CompreFaceClient]   2. Crop margin is too large and includes neighboring faces" << std::endl;
                std::cout << "[CompreFaceClient] ➡️  Selecting face with HIGHEST similarity score..." << std::endl;
            }

            // Find the face with highest similarity across ALL detected faces
            RecognitionResult best_result;
            float best_similarity = -1.0f;
            int best_face_idx = -1;

            for (int i = 0; i < num_faces_detected; i++) {
                auto& face = results_array[i];

                // Parse subjects array for this face
                if (face.contains("subjects") && face["subjects"].is_array() && !face["subjects"].empty()) {
                    auto& first_subject = face["subjects"][0];
                    float similarity = first_subject.value("similarity", 0.0f);

                    std::cout << "[CompreFaceClient]   Face #" << (i + 1) << ": "
                              << first_subject.value("subject", "unknown")
                              << " (similarity: " << similarity << ")" << std::endl;

                    if (similarity > best_similarity) {
                        best_similarity = similarity;
                        best_face_idx = i;
                        best_result.subject = first_subject.value("subject", "unknown");
                        best_result.similarity = similarity;
                        best_result.face_id = face.value("face_id", "");
                    }
                }
            }

            if (best_face_idx == -1) {
                // No subjects found in any face - face detected but no match
                std::cout << "[CompreFaceClient] ℹ️  No recognized subjects in any detected face" << std::endl;
                return std::nullopt;
            }

            if (num_faces_detected > 1) {
                std::cout << "[CompreFaceClient] ✅ Selected Face #" << (best_face_idx + 1)
                          << ": " << best_result.subject
                          << " (highest similarity: " << best_result.similarity << ")" << std::endl;
            }

            best_result.confidence = best_result.similarity;  // Use similarity as confidence
            best_result.is_match = (best_result.similarity >= config_.similarity_threshold);

            return best_result;

        } catch (const std::exception& e) {
            std::cerr << "[CompreFaceClient] JSON parse error: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    bool is_circuit_open() const {
        std::lock_guard<std::mutex> lock(circuit_mutex_);

        if (circuit_state_ == CircuitState::OPEN) {
            // Check if we should move to half-open
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration<double>(now - circuit_open_time_).count();

            if (elapsed >= CIRCUIT_OPEN_DURATION_SEC) {
                const_cast<Impl*>(this)->circuit_state_ = CircuitState::HALF_OPEN;
                return false;
            }
            return true;
        }

        return false;
    }

    void record_failure() {
        std::lock_guard<std::mutex> lock(circuit_mutex_);

        consecutive_failures_++;
        consecutive_successes_.store(0);

        if (circuit_state_ == CircuitState::HALF_OPEN) {
            // Failed in half-open, go back to open
            circuit_state_ = CircuitState::OPEN;
            circuit_open_time_ = std::chrono::steady_clock::now();
            std::cout << "[CompreFaceClient] Circuit breaker: OPEN (failed in half-open)" << std::endl;
        } else if (consecutive_failures_.load() >= FAILURE_THRESHOLD) {
            circuit_state_ = CircuitState::OPEN;
            circuit_open_time_ = std::chrono::steady_clock::now();
            std::cout << "[CompreFaceClient] Circuit breaker: OPEN (failure threshold reached)" << std::endl;
        }
    }

    void record_success() {
        std::lock_guard<std::mutex> lock(circuit_mutex_);

        consecutive_successes_++;
        consecutive_failures_.store(0);

        if (circuit_state_ == CircuitState::HALF_OPEN) {
            if (consecutive_successes_.load() >= SUCCESS_THRESHOLD) {
                circuit_state_ = CircuitState::CLOSED;
                std::cout << "[CompreFaceClient] Circuit breaker: CLOSED (recovered)" << std::endl;
            }
        }
    }
};

// CompreFaceClient implementation

CompreFaceClient::CompreFaceClient(
    const CompreFaceConfig& config,
    RecognitionCallback callback
)
    : impl_(std::make_unique<Impl>(config, callback))
{
}

CompreFaceClient::~CompreFaceClient() {
    stop();
}

bool CompreFaceClient::start() {
    return impl_->start();
}

void CompreFaceClient::stop() {
    impl_->stop();
}

bool CompreFaceClient::recognize_async(
    const std::string& camera_id,
    const std::string& face_crop_path,
    float detection_confidence
) {
    return impl_->recognize_async(camera_id, face_crop_path, detection_confidence);
}

std::optional<RecognitionResult> CompreFaceClient::recognize_sync(
    const std::string& face_crop_path
) {
    return impl_->recognize_sync(face_crop_path);
}

std::optional<RecognitionResult> CompreFaceClient::get_cached_result(
    const std::string& face_crop_path
) const {
    return impl_->get_cached_result(face_crop_path);
}

void CompreFaceClient::update_config(const CompreFaceConfig& config) {
    impl_->update_config(config);
}

CompreFaceClient::Statistics CompreFaceClient::get_statistics() const {
    return impl_->get_statistics();
}

bool CompreFaceClient::is_healthy() const {
    return impl_->is_healthy();
}

} // namespace gstreamer_worker
