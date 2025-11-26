#ifndef GSTREAMER_WORKER_COMPREFACE_CLIENT_H
#define GSTREAMER_WORKER_COMPREFACE_CLIENT_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <chrono>

namespace gstreamer_worker {

/**
 * @brief Recognition result from CompreFace
 */
struct RecognitionResult {
    std::string subject;           // Person ID/name
    float similarity;              // Similarity score [0-1]
    float confidence;              // Recognition confidence [0-1]
    bool is_match;                 // Whether this is a match
    std::string face_id;           // CompreFace face ID

    RecognitionResult() = default;
};

/**
 * @brief Configuration for CompreFace API
 */
struct CompreFaceConfig {
    std::string base_url = "http://localhost:8000";
    std::string api_key;
    std::string recognition_service_url;  // Full URL: base_url + /api/v1/recognition/recognize
    float similarity_threshold = 0.9f;     // Minimum similarity for match (90% - enterprise strict matching)
    int timeout_ms = 5000;                 // HTTP timeout
    int max_retries = 3;                   // Retry failed requests
    int max_queue_size = 100;              // Max faces in recognition queue
    bool cache_results = true;             // Cache recent recognitions
    int cache_duration_seconds = 60;       // Cache duration

    bool validate() const {
        return !api_key.empty() && !base_url.empty() &&
               similarity_threshold >= 0.0f && similarity_threshold <= 1.0f &&
               timeout_ms > 0;
    }
};

/**
 * @brief Callback for recognition results
 */
using RecognitionCallback = std::function<void(
    const std::string& camera_id,
    const std::string& face_crop_path,
    const std::optional<RecognitionResult>& result,
    const std::string& error
)>;

/**
 * @brief Enterprise-grade CompreFace HTTP client
 *
 * Features:
 * - Async recognition with thread pool
 * - Retry logic with exponential backoff
 * - Result caching to reduce API calls
 * - Connection pooling
 * - Circuit breaker for fault tolerance
 * - Comprehensive error handling
 * - Performance metrics
 */
class CompreFaceClient {
public:
    /**
     * @brief Constructor
     * @param config CompreFace configuration
     * @param callback Callback for recognition results
     */
    explicit CompreFaceClient(
        const CompreFaceConfig& config,
        RecognitionCallback callback = nullptr
    );

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~CompreFaceClient();

    // Delete copy/move constructors
    CompreFaceClient(const CompreFaceClient&) = delete;
    CompreFaceClient& operator=(const CompreFaceClient&) = delete;
    CompreFaceClient(CompreFaceClient&&) = delete;
    CompreFaceClient& operator=(CompreFaceClient&&) = delete;

    /**
     * @brief Start the client (worker threads)
     */
    bool start();

    /**
     * @brief Stop the client gracefully
     */
    void stop();

    /**
     * @brief Recognize a face from image file (async)
     * @param camera_id Camera identifier
     * @param face_crop_path Path to face crop image
     * @param detection_confidence Original detection confidence
     * @return true if queued successfully
     */
    bool recognize_async(
        const std::string& camera_id,
        const std::string& face_crop_path,
        float detection_confidence
    );

    /**
     * @brief Recognize a face synchronously (blocking)
     * @param face_crop_path Path to face crop image
     * @return Recognition result or nullopt on error
     */
    std::optional<RecognitionResult> recognize_sync(
        const std::string& face_crop_path
    );

    /**
     * @brief Check if a face crop has been recognized recently (cache check)
     * @param face_crop_path Path to face crop
     * @return Cached result if available
     */
    std::optional<RecognitionResult> get_cached_result(
        const std::string& face_crop_path
    ) const;

    /**
     * @brief Update configuration at runtime
     */
    void update_config(const CompreFaceConfig& config);

    /**
     * @brief Get performance statistics
     */
    struct Statistics {
        uint64_t total_requests = 0;
        uint64_t successful_recognitions = 0;
        uint64_t failed_requests = 0;
        uint64_t cache_hits = 0;
        double avg_response_time_ms = 0.0;
        uint64_t queue_size = 0;
        bool circuit_breaker_open = false;
    };

    Statistics get_statistics() const;

    /**
     * @brief Check if client is healthy
     */
    bool is_healthy() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gstreamer_worker

#endif // GSTREAMER_WORKER_COMPREFACE_CLIENT_H
