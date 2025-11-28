#ifndef GSTREAMER_WORKER_TYPES_H
#define GSTREAMER_WORKER_TYPES_H

#include <string>
#include <cstdint>
#include <vector>

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

    // Motion detection metrics
    uint64_t motion_events_detected = 0;
    uint64_t frames_analyzed = 0;
    double last_motion_timestamp = 0.0;
    double motion_detection_fps = 0.0;

    StreamMetrics() = default;
};

/**
 * @brief Region of Interest for motion detection
 */
struct MotionROI {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool is_valid() const {
        return width > 0 && height > 0;
    }
};

/**
 * @brief Motion detection event
 */
struct MotionEvent {
    std::string camera_id;
    double timestamp;
    int motion_area;
    int num_contours;
    MotionROI bounding_box;
    double confidence;

    MotionEvent() : timestamp(0.0), motion_area(0), num_contours(0), confidence(0.0) {}
};

/**
 * @brief Motion detection algorithm type
 */
enum class MotionAlgorithm {
    MOG2,           // Gaussian Mixture-based Background/Foreground Segmentation (default)
    MOG2_CUDA,      // CUDA-accelerated MOG2
    KNN,            // K-Nearest Neighbors
    FRAME_DIFF      // Simple frame differencing
};

/**
 * @brief Convert MotionAlgorithm to string
 */
inline std::string motion_algorithm_to_string(MotionAlgorithm algo) {
    switch (algo) {
        case MotionAlgorithm::MOG2: return "MOG2";
        case MotionAlgorithm::MOG2_CUDA: return "MOG2_CUDA";
        case MotionAlgorithm::KNN: return "KNN";
        case MotionAlgorithm::FRAME_DIFF: return "FRAME_DIFF";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Bounding box for face detection (normalized coordinates 0-1)
 */
struct FaceBoundingBox {
    float x = 0.0f;       // Top-left x coordinate (normalized)
    float y = 0.0f;       // Top-left y coordinate (normalized)
    float width = 0.0f;   // Box width (normalized)
    float height = 0.0f;  // Box height (normalized)

    bool is_valid() const {
        return width > 0.0f && height > 0.0f;
    }
};

/**
 * @brief Facial landmark point (5 keypoints: left eye, right eye, nose, left mouth, right mouth)
 */
struct FaceLandmark {
    float x = 0.0f;  // X coordinate (normalized)
    float y = 0.0f;  // Y coordinate (normalized)
};

/**
 * @brief Single face detection result
 */
struct FaceDetection {
    FaceBoundingBox bbox;                    // Face bounding box
    float confidence = 0.0f;                 // Detection confidence [0-1]
    FaceLandmark landmarks[5];               // 5 facial keypoints

    // Additional quality metrics
    float blur_score = 0.0f;                 // Face quality: blur
    float brightness_score = 0.0f;           // Face quality: brightness

    FaceDetection() = default;
};

/**
 * @brief Face detection event (multiple faces in one frame)
 */
struct FaceEvent {
    std::string camera_id;
    double timestamp = 0.0;
    int num_faces = 0;
    std::vector<FaceDetection> faces;
    std::vector<std::string> face_crop_paths;  // Paths to saved face crop images

    FaceEvent() = default;
};

} // namespace gstreamer_worker

#endif // GSTREAMER_WORKER_TYPES_H
