#include "motion_detector.h"
#include <iostream>
#include <cmath>

namespace gstreamer_worker {

MotionDetector::MotionDetector(
    const std::string& camera_id,
    const MotionDetectionConfig& config,
    MotionCallback on_motion
)
    : camera_id_(camera_id)
    , config_(config)
    , on_motion_(on_motion)
    , cuda_available_(false)
    , using_cuda_(false)
    , frame_counter_(0)
    , initialized_(false)
    , last_event_time_(std::chrono::steady_clock::now())
    , last_fps_update_(std::chrono::steady_clock::now())
    , fps_frame_count_(0)
{
    // Check CUDA availability
    try {
        int cuda_device_count = cv::cuda::getCudaEnabledDeviceCount();
        cuda_available_ = (cuda_device_count > 0);

        if (cuda_available_) {
            cv::cuda::DeviceInfo device_info;
            std::cout << "[MotionDetector:" << camera_id_ << "] CUDA available - Device: "
                      << device_info.name() << std::endl;
        } else {
            std::cout << "[MotionDetector:" << camera_id_ << "] CUDA not available, using CPU" << std::endl;
        }
    } catch (const cv::Exception& e) {
        std::cerr << "[MotionDetector:" << camera_id_ << "] CUDA check failed: "
                  << e.what() << std::endl;
        cuda_available_ = false;
    }

    stats_.cuda_available = cuda_available_;

    // Initialize background subtractor
    initialize_bg_subtractor();

    // Initialize motion history buffer
    motion_history_.resize(config_.required_frames, false);

    std::cout << "[MotionDetector:" << camera_id_ << "] Initialized with algorithm: "
              << motion_algorithm_to_string(config_.algorithm) << std::endl;
}

MotionDetector::~MotionDetector() {
    std::cout << "[MotionDetector:" << camera_id_ << "] Shutting down" << std::endl;
}

void MotionDetector::initialize_bg_subtractor() {
    std::lock_guard<std::mutex> lock(config_mutex_);

    using_cuda_ = false;
    bg_subtractor_.release();
    cuda_bg_subtractor_.release();

    switch (config_.algorithm) {
        case MotionAlgorithm::MOG2_CUDA:
            if (cuda_available_) {
                try {
                    cuda_bg_subtractor_ = cv::cuda::createBackgroundSubtractorMOG2(
                        config_.history,
                        config_.var_threshold,
                        config_.detect_shadows
                    );
                    using_cuda_ = true;
                    std::cout << "[MotionDetector:" << camera_id_ << "] Using CUDA MOG2" << std::endl;
                    break;
                } catch (const cv::Exception& e) {
                    std::cerr << "[MotionDetector:" << camera_id_ << "] Failed to create CUDA MOG2: "
                              << e.what() << ", falling back to CPU MOG2" << std::endl;
                }
            }
            // Fall through to CPU MOG2 if CUDA fails or not available
            [[fallthrough]];

        case MotionAlgorithm::MOG2:
            bg_subtractor_ = cv::createBackgroundSubtractorMOG2(
                config_.history,
                config_.var_threshold,
                config_.detect_shadows
            );
            std::cout << "[MotionDetector:" << camera_id_ << "] Using CPU MOG2" << std::endl;
            break;

        case MotionAlgorithm::KNN:
            bg_subtractor_ = cv::createBackgroundSubtractorKNN(
                config_.history,
                config_.var_threshold,
                config_.detect_shadows
            );
            std::cout << "[MotionDetector:" << camera_id_ << "] Using KNN" << std::endl;
            break;

        case MotionAlgorithm::FRAME_DIFF:
            // No background subtractor needed for frame differencing
            std::cout << "[MotionDetector:" << camera_id_ << "] Using Frame Differencing" << std::endl;
            break;
    }

    stats_.using_cuda = using_cuda_;
    initialized_ = false;  // Reset initialization flag
}

bool MotionDetector::process_frame(const cv::Mat& frame) {
    if (frame.empty()) {
        return false;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Frame skipping for performance
    uint64_t current_count = frame_counter_++;
    if (config_.frame_skip > 1 && (current_count % config_.frame_skip) != 0) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.frames_skipped++;
        return false;
    }

    bool motion_detected = false;

    try {
        cv::Mat fg_mask;

        if (using_cuda_ && config_.algorithm == MotionAlgorithm::MOG2_CUDA) {
            // CUDA processing path
            cv::cuda::GpuMat d_processed;
            if (!preprocess_frame_cuda(frame, d_processed)) {
                return false;
            }

            cv::cuda::GpuMat d_fg_mask_local;
            if (!apply_background_subtraction_cuda(d_processed, d_fg_mask_local)) {
                return false;
            }

            // Download mask from GPU for contour analysis
            d_fg_mask_local.download(fg_mask);

        } else {
            // CPU processing path
            cv::Mat processed;
            if (!preprocess_frame(frame, processed)) {
                return false;
            }

            if (config_.algorithm == MotionAlgorithm::FRAME_DIFF) {
                if (!apply_frame_difference(processed, fg_mask)) {
                    return false;
                }
            } else {
                if (!apply_background_subtraction(processed, fg_mask)) {
                    return false;
                }
            }
        }

        // Analyze motion in foreground mask
        int motion_area = 0;
        MotionROI bounding_box;
        int num_contours = analyze_motion(fg_mask, motion_area, bounding_box);

        // Determine if significant motion was detected
        bool has_motion = (motion_area >= config_.min_contour_area && num_contours > 0);

        // Check if we should trigger a motion event
        if (should_trigger_event(has_motion)) {
            motion_detected = true;

            // Create motion event
            MotionEvent event;
            event.camera_id = camera_id_;
            event.timestamp = std::chrono::duration<double>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            event.motion_area = motion_area;
            event.num_contours = num_contours;
            event.bounding_box = bounding_box;
            event.confidence = std::min(1.0, static_cast<double>(motion_area) /
                                       (frame.cols * frame.rows) * 10.0);

            // Update last event time
            {
                std::lock_guard<std::mutex> lock(motion_mutex_);
                last_event_time_ = std::chrono::steady_clock::now();
            }

            // Trigger callback
            if (on_motion_) {
                on_motion_(event);
            }

            // Update statistics
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.motion_events++;
            }
        }

        initialized_ = true;

    } catch (const cv::Exception& e) {
        std::cerr << "[MotionDetector:" << camera_id_ << "] OpenCV error: "
                  << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[MotionDetector:" << camera_id_ << "] Error processing frame: "
                  << e.what() << std::endl;
        return false;
    }

    // Update performance metrics
    auto end_time = std::chrono::high_resolution_clock::now();
    double processing_time_ms = std::chrono::duration<double, std::milli>(
        end_time - start_time
    ).count();
    update_statistics(processing_time_ms);

    return motion_detected;
}

bool MotionDetector::preprocess_frame(const cv::Mat& frame, cv::Mat& processed) {
    cv::Mat working = frame;

    // Apply ROI if configured
    if (config_.roi.is_valid()) {
        working = apply_roi(frame);
    }

    // Resize for performance if configured
    if (config_.max_frame_width > 0 && config_.max_frame_height > 0) {
        if (working.cols > config_.max_frame_width || working.rows > config_.max_frame_height) {
            cv::resize(working, working,
                      cv::Size(config_.max_frame_width, config_.max_frame_height));
        }
    }

    // Convert to grayscale
    if (working.channels() == 3) {
        cv::cvtColor(working, working, cv::COLOR_BGR2GRAY);
    }

    // Apply Gaussian blur for noise reduction
    cv::GaussianBlur(working, processed,
                    cv::Size(config_.blur_size, config_.blur_size), 0);

    return true;
}

bool MotionDetector::preprocess_frame_cuda(const cv::Mat& frame, cv::cuda::GpuMat& d_processed) {
    try {
        // Upload to GPU
        d_frame_.upload(frame);

        // Apply ROI if configured
        cv::cuda::GpuMat d_working = d_frame_;
        if (config_.roi.is_valid()) {
            d_working = apply_roi_cuda(d_frame_);
        }

        // Resize for performance if configured
        if (config_.max_frame_width > 0 && config_.max_frame_height > 0) {
            if (d_working.cols > config_.max_frame_width || d_working.rows > config_.max_frame_height) {
                cv::cuda::resize(d_working, d_resized_,
                               cv::Size(config_.max_frame_width, config_.max_frame_height));
                d_working = d_resized_;
            }
        }

        // Convert to grayscale
        if (d_working.channels() == 3) {
            cv::cuda::cvtColor(d_working, d_gray_, cv::COLOR_BGR2GRAY);
        } else {
            d_gray_ = d_working;
        }

        // Apply Gaussian blur
        auto gaussian_filter = cv::cuda::createGaussianFilter(
            d_gray_.type(), d_gray_.type(),
            cv::Size(config_.blur_size, config_.blur_size), 0
        );
        gaussian_filter->apply(d_gray_, d_processed);

        return true;

    } catch (const cv::Exception& e) {
        std::cerr << "[MotionDetector:" << camera_id_ << "] CUDA preprocessing error: "
                  << e.what() << std::endl;
        return false;
    }
}

bool MotionDetector::apply_background_subtraction(const cv::Mat& frame, cv::Mat& fg_mask) {
    if (!bg_subtractor_) {
        return false;
    }

    try {
        // Apply background subtraction
        bg_subtractor_->apply(frame, fg_mask);

        // Threshold based on sensitivity
        double threshold = 255.0 * (1.0 - config_.sensitivity);
        cv::threshold(fg_mask, fg_mask, threshold, 255, cv::THRESH_BINARY);

        // Morphological operations to reduce noise
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
        cv::morphologyEx(fg_mask, fg_mask, cv::MORPH_OPEN, kernel);
        cv::morphologyEx(fg_mask, fg_mask, cv::MORPH_CLOSE, kernel);

        return true;

    } catch (const cv::Exception& e) {
        std::cerr << "[MotionDetector:" << camera_id_ << "] Background subtraction error: "
                  << e.what() << std::endl;
        return false;
    }
}

bool MotionDetector::apply_background_subtraction_cuda(const cv::cuda::GpuMat& d_frame,
                                                       cv::cuda::GpuMat& d_fg_mask) {
    if (!cuda_bg_subtractor_) {
        return false;
    }

    try {
        // Apply CUDA background subtraction
        cuda_bg_subtractor_->apply(d_frame, d_fg_mask);

        // Threshold based on sensitivity
        double threshold = 255.0 * (1.0 - config_.sensitivity);
        cv::cuda::threshold(d_fg_mask, d_fg_mask, threshold, 255, cv::THRESH_BINARY);

        // Morphological operations (download/upload for now, as CUDA morph ops are limited)
        cv::Mat h_mask;
        d_fg_mask.download(h_mask);

        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
        cv::morphologyEx(h_mask, h_mask, cv::MORPH_OPEN, kernel);
        cv::morphologyEx(h_mask, h_mask, cv::MORPH_CLOSE, kernel);

        d_fg_mask.upload(h_mask);

        return true;

    } catch (const cv::Exception& e) {
        std::cerr << "[MotionDetector:" << camera_id_ << "] CUDA background subtraction error: "
                  << e.what() << std::endl;
        return false;
    }
}

bool MotionDetector::apply_frame_difference(const cv::Mat& frame, cv::Mat& diff_mask) {
    if (prev_frame_.empty()) {
        frame.copyTo(prev_frame_);
        diff_mask = cv::Mat::zeros(frame.size(), CV_8UC1);
        return true;
    }

    try {
        // Compute absolute difference
        cv::absdiff(frame, prev_frame_, diff_mask);

        // Threshold based on sensitivity
        double threshold = 30.0 * (1.0 - config_.sensitivity);
        cv::threshold(diff_mask, diff_mask, threshold, 255, cv::THRESH_BINARY);

        // Morphological operations
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
        cv::morphologyEx(diff_mask, diff_mask, cv::MORPH_OPEN, kernel);
        cv::morphologyEx(diff_mask, diff_mask, cv::MORPH_CLOSE, kernel);

        // Update previous frame
        frame.copyTo(prev_frame_);

        return true;

    } catch (const cv::Exception& e) {
        std::cerr << "[MotionDetector:" << camera_id_ << "] Frame difference error: "
                  << e.what() << std::endl;
        return false;
    }
}

bool MotionDetector::apply_frame_difference_cuda(const cv::cuda::GpuMat& d_frame,
                                                 cv::cuda::GpuMat& d_diff_mask) {
    if (d_prev_frame_.empty()) {
        d_frame.copyTo(d_prev_frame_);
        d_diff_mask = cv::cuda::GpuMat(d_frame.size(), CV_8UC1, cv::Scalar(0));
        return true;
    }

    try {
        // Compute absolute difference
        cv::cuda::absdiff(d_frame, d_prev_frame_, d_diff_mask);

        // Threshold based on sensitivity
        double threshold = 30.0 * (1.0 - config_.sensitivity);
        cv::cuda::threshold(d_diff_mask, d_diff_mask, threshold, 255, cv::THRESH_BINARY);

        // Update previous frame
        d_frame.copyTo(d_prev_frame_);

        return true;

    } catch (const cv::Exception& e) {
        std::cerr << "[MotionDetector:" << camera_id_ << "] CUDA frame difference error: "
                  << e.what() << std::endl;
        return false;
    }
}

int MotionDetector::analyze_motion(const cv::Mat& fg_mask, int& motion_area,
                                   MotionROI& bounding_box) {
    motion_area = 0;
    bounding_box = MotionROI();

    try {
        // Find contours
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(fg_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        if (contours.empty()) {
            return 0;
        }

        // Filter and analyze contours
        std::vector<std::vector<cv::Point>> significant_contours;
        int min_x = fg_mask.cols, min_y = fg_mask.rows;
        int max_x = 0, max_y = 0;

        for (const auto& contour : contours) {
            double area = cv::contourArea(contour);
            if (area >= config_.min_contour_area) {
                significant_contours.push_back(contour);
                motion_area += static_cast<int>(area);

                // Update bounding box
                cv::Rect rect = cv::boundingRect(contour);
                min_x = std::min(min_x, rect.x);
                min_y = std::min(min_y, rect.y);
                max_x = std::max(max_x, rect.x + rect.width);
                max_y = std::max(max_y, rect.y + rect.height);

                // Limit number of contours processed
                if (significant_contours.size() >= static_cast<size_t>(config_.max_contours)) {
                    break;
                }
            }
        }

        // Set bounding box
        if (!significant_contours.empty()) {
            bounding_box.x = min_x;
            bounding_box.y = min_y;
            bounding_box.width = max_x - min_x;
            bounding_box.height = max_y - min_y;
        }

        return static_cast<int>(significant_contours.size());

    } catch (const cv::Exception& e) {
        std::cerr << "[MotionDetector:" << camera_id_ << "] Motion analysis error: "
                  << e.what() << std::endl;
        return 0;
    }
}

bool MotionDetector::should_trigger_event(bool has_motion) {
    std::lock_guard<std::mutex> lock(motion_mutex_);

    // Update motion history
    motion_history_.pop_front();
    motion_history_.push_back(has_motion);

    // Check if we have enough consecutive frames with motion
    int motion_count = 0;
    for (bool m : motion_history_) {
        if (m) motion_count++;
    }

    if (motion_count < config_.required_frames) {
        return false;
    }

    // Check cooldown period
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - last_event_time_).count();

    return elapsed >= config_.cooldown_seconds;
}

void MotionDetector::update_statistics(double processing_time_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    stats_.frames_processed++;

    // Update average processing time (exponential moving average)
    const double alpha = 0.1;  // Smoothing factor
    stats_.avg_processing_time_ms =
        alpha * processing_time_ms + (1.0 - alpha) * stats_.avg_processing_time_ms;

    // Update FPS calculation
    fps_frame_count_++;
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - last_fps_update_).count();

    if (elapsed >= 1.0) {  // Update FPS every second
        stats_.current_fps = fps_frame_count_ / elapsed;
        fps_frame_count_ = 0;
        last_fps_update_ = now;
    }
}

void MotionDetector::update_config(const MotionDetectionConfig& config) {
    if (!config.validate()) {
        std::cerr << "[MotionDetector:" << camera_id_ << "] Invalid configuration" << std::endl;
        return;
    }

    std::lock_guard<std::mutex> lock(config_mutex_);

    bool algo_changed = (config_.algorithm != config.algorithm);
    config_ = config;

    // Reinitialize if algorithm changed
    if (algo_changed) {
        initialize_bg_subtractor();
    }

    // Resize motion history buffer if required_frames changed
    if (motion_history_.size() != static_cast<size_t>(config_.required_frames)) {
        motion_history_.clear();
        motion_history_.resize(config_.required_frames, false);
    }

    std::cout << "[MotionDetector:" << camera_id_ << "] Configuration updated" << std::endl;
}

MotionDetectionConfig MotionDetector::get_config() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

void MotionDetector::reset() {
    std::lock_guard<std::mutex> lock(config_mutex_);

    // Reinitialize background subtractor
    initialize_bg_subtractor();

    // Clear previous frames
    prev_frame_.release();
    d_prev_frame_.release();

    // Reset motion history
    std::fill(motion_history_.begin(), motion_history_.end(), false);

    initialized_ = false;

    std::cout << "[MotionDetector:" << camera_id_ << "] Reset completed" << std::endl;
}

MotionDetector::Statistics MotionDetector::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

cv::Mat MotionDetector::apply_roi(const cv::Mat& frame) const {
    const auto& roi = config_.roi;

    // Validate ROI bounds
    int x = std::max(0, std::min(roi.x, frame.cols - 1));
    int y = std::max(0, std::min(roi.y, frame.rows - 1));
    int w = std::min(roi.width, frame.cols - x);
    int h = std::min(roi.height, frame.rows - y);

    if (w <= 0 || h <= 0) {
        return frame;
    }

    return frame(cv::Rect(x, y, w, h));
}

cv::cuda::GpuMat MotionDetector::apply_roi_cuda(const cv::cuda::GpuMat& d_frame) const {
    const auto& roi = config_.roi;

    // Validate ROI bounds
    int x = std::max(0, std::min(roi.x, d_frame.cols - 1));
    int y = std::max(0, std::min(roi.y, d_frame.rows - 1));
    int w = std::min(roi.width, d_frame.cols - x);
    int h = std::min(roi.height, d_frame.rows - y);

    if (w <= 0 || h <= 0) {
        return d_frame;
    }

    return d_frame(cv::Rect(x, y, w, h));
}

} // namespace gstreamer_worker
