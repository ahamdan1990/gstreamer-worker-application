#include "face_detector.h"
#include "logger.h"

#include <iostream>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace gstreamer_worker {

FaceDetector::FaceDetector(
    const std::string& camera_id,
    const FaceDetectionConfig& config,
    FaceCallback on_face
)
    : camera_id_(camera_id)
    , config_(config)
    , on_face_(on_face)
    , memory_info_(Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU))
    , frame_counter_(0)
    , initialized_(false)
    , last_event_time_(std::chrono::steady_clock::now())
    , last_fps_update_(std::chrono::steady_clock::now())
    , fps_frame_count_(0)
{
    std::cout << "[FaceDetector:" << camera_id_ << "] Initializing..." << std::endl;

    // Initialize ONNX Runtime session
    try {
        initialize_session();
        initialized_ = true;
        std::cout << "[FaceDetector:" << camera_id_ << "] Initialized successfully" << std::endl;
        std::cout << "[FaceDetector:" << camera_id_ << "] Using: "
                  << (stats_.using_tensorrt ? "TensorRT" :
                      (stats_.using_cuda ? "CUDA" : "CPU")) << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[FaceDetector:" << camera_id_ << "] Initialization failed: "
                  << e.what() << std::endl;
        throw;
    }

    // Initialize face history buffer
    face_history_.resize(config_.required_frames, false);
}

FaceDetector::~FaceDetector() {
    std::cout << "[FaceDetector:" << camera_id_ << "] Shutting down" << std::endl;
}

void FaceDetector::initialize_session() {
    // Create ONNX Runtime environment
    env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "FaceDetector");

    // Create session options
    session_options_ = std::make_unique<Ort::SessionOptions>();
    session_options_->SetIntraOpNumThreads(1);
    session_options_->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    // Try to use TensorRT provider first
    if (config_.use_tensorrt) {
        try {
            OrtTensorRTProviderOptionsV2* tensorrt_options = nullptr;
            Ort::GetApi().CreateTensorRTProviderOptions(&tensorrt_options);

            // Configure TensorRT options with engine caching
            std::vector<const char*> keys = {
                "device_id",
                "trt_max_workspace_size",
                "trt_fp16_enable",
                "trt_engine_cache_enable",
                "trt_engine_cache_path"
            };
            std::vector<const char*> values = {
                "0",
                "2147483648",  // 2GB
                "1",  // Enable FP16
                "1",  // Enable engine caching
                "./engine_cache"  // Cache directory
            };

            Ort::GetApi().UpdateTensorRTProviderOptions(tensorrt_options, keys.data(), values.data(), keys.size());
            session_options_->AppendExecutionProvider_TensorRT_V2(*tensorrt_options);

            stats_.using_tensorrt = true;
            std::cout << "[FaceDetector:" << camera_id_ << "] TensorRT provider enabled" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[FaceDetector:" << camera_id_ << "] TensorRT not available: "
                      << e.what() << std::endl;
            stats_.using_tensorrt = false;
        }
    }

    // Fall back to CUDA provider
    if (!stats_.using_tensorrt && config_.use_cuda) {
        try {
            OrtCUDAProviderOptions cuda_options;
            cuda_options.device_id = 0;
            session_options_->AppendExecutionProvider_CUDA(cuda_options);
            stats_.using_cuda = true;
            std::cout << "[FaceDetector:" << camera_id_ << "] CUDA provider enabled" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[FaceDetector:" << camera_id_ << "] CUDA not available: "
                      << e.what() << std::endl;
            stats_.using_cuda = false;
        }
    }

    // Load model
    std::cout << "[FaceDetector:" << camera_id_ << "] Loading model: "
              << config_.model_path << std::endl;

#ifdef _WIN32
    std::wstring model_path_w(config_.model_path.begin(), config_.model_path.end());
    session_ = std::make_unique<Ort::Session>(*env_, model_path_w.c_str(), *session_options_);
#else
    session_ = std::make_unique<Ort::Session>(*env_, config_.model_path.c_str(), *session_options_);
#endif

    // Get input/output names and shapes
    Ort::AllocatorWithDefaultOptions allocator;

    // Input info
    size_t num_input_nodes = session_->GetInputCount();
    for (size_t i = 0; i < num_input_nodes; i++) {
        input_names_alloc_.push_back(session_->GetInputNameAllocated(i, allocator));
        input_names_.push_back(input_names_alloc_.back().get());

        auto type_info = session_->GetInputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        input_shape_ = tensor_info.GetShape();

        std::cout << "[FaceDetector:" << camera_id_ << "] Input: " << input_names_alloc_.back().get()
                  << " Shape: [";
        for (size_t j = 0; j < input_shape_.size(); j++) {
            std::cout << input_shape_[j];
            if (j < input_shape_.size() - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }

    // Output info (SCRFD has 9 outputs: 3 scales × 3 outputs)
    size_t num_output_nodes = session_->GetOutputCount();
    for (size_t i = 0; i < num_output_nodes; i++) {
        output_names_alloc_.push_back(session_->GetOutputNameAllocated(i, allocator));
        output_names_.push_back(output_names_alloc_.back().get());
        std::cout << "[FaceDetector:" << camera_id_ << "] Output " << i << ": "
                  << output_names_alloc_.back().get() << std::endl;
    }
}

bool FaceDetector::process_frame(const cv::Mat& frame) {
    if (frame.empty()) {
        return false;
    }

    // Upload to GPU and use GPU path
    cv::cuda::GpuMat d_frame;
    d_frame.upload(frame);
    return process_frame_cuda(d_frame);
}

bool FaceDetector::process_frame_cuda(const cv::cuda::GpuMat& d_frame) {
    if (!initialized_) {
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

    bool faces_detected = false;

    try {
        // Preprocess frame (GPU → CPU tensor)
        std::vector<float> input_tensor;
        if (!preprocess_gpu(d_frame, input_tensor)) {
            return false;
        }

        // Run inference
        auto inference_start = std::chrono::high_resolution_clock::now();
        std::vector<std::vector<float>> outputs;
        if (!run_inference(input_tensor, outputs)) {
            return false;
        }
        auto inference_end = std::chrono::high_resolution_clock::now();
        double inference_time_ms = std::chrono::duration<double, std::milli>(
            inference_end - inference_start
        ).count();

        // Post-process outputs
        std::vector<FaceDetection> detections = postprocess(outputs, d_frame.cols, d_frame.rows);

        // DEBUG: Log raw detections
        std::cout << "[FaceDetector:" << camera_id_ << "] Raw detections: " << detections.size() << std::endl;
        if (!detections.empty()) {
            for (size_t i = 0; i < std::min(detections.size(), size_t(5)); i++) {
                std::cout << "  [" << i << "] Confidence: " << detections[i].confidence
                          << " BBox: [" << detections[i].bbox.x << "," << detections[i].bbox.y
                          << " " << detections[i].bbox.width << "x" << detections[i].bbox.height << "]" << std::endl;
            }
        }

        // Apply NMS
        size_t before_nms = detections.size();
        if (!detections.empty()) {
            detections = apply_nms(detections);
            std::cout << "[FaceDetector:" << camera_id_ << "] After NMS: " << detections.size()
                      << " (removed " << (before_nms - detections.size()) << ")" << std::endl;
        }

        // Store latest detections for visualization
        {
            std::lock_guard<std::mutex> lock(face_mutex_);
            latest_detections_ = detections;
        }

        // Check if we should trigger an event
        faces_detected = !detections.empty();
        std::cout << "[FaceDetector:" << camera_id_ << "] Faces detected: " << faces_detected
                  << ", Should trigger event: " << should_trigger_event(faces_detected) << std::endl;
        if (should_trigger_event(faces_detected)) {
            // Create face event
            FaceEvent event;
            event.camera_id = camera_id_;
            event.timestamp = std::chrono::duration<double>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            event.num_faces = static_cast<int>(detections.size());
            event.faces = detections;

            // Update last event time
            {
                std::lock_guard<std::mutex> lock(face_mutex_);
                last_event_time_ = std::chrono::steady_clock::now();
            }

            // Trigger callback
            if (on_face_) {
                on_face_(event);
            }

            // Update statistics
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.face_events++;
                stats_.total_faces_detected += detections.size();
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "[FaceDetector:" << camera_id_ << "] Error processing frame: "
                  << e.what() << std::endl;
        return false;
    }

    // Update performance metrics
    auto end_time = std::chrono::high_resolution_clock::now();
    double processing_time_ms = std::chrono::duration<double, std::milli>(
        end_time - start_time
    ).count();
    update_statistics(processing_time_ms, 0.0);  // TODO: track inference time separately

    return faces_detected;
}

bool FaceDetector::preprocess_gpu(const cv::cuda::GpuMat& d_frame, std::vector<float>& output) {
    try {
        // Apply ROI if configured
        cv::cuda::GpuMat d_working = d_frame;
        if (config_.roi.is_valid()) {
            d_working = apply_roi_cuda(d_frame);
        }

        // Resize to model input size (e.g., 640x640)
        cv::cuda::resize(d_working, d_resized_, cv::Size(config_.input_size, config_.input_size));

        // Convert BGR to RGB
        cv::cuda::cvtColor(d_resized_, d_working_, cv::COLOR_BGR2RGB);

        // Download to CPU for ONNX Runtime
        cv::Mat h_frame;
        d_working_.download(h_frame);

        // Convert to float and normalize for SCRFD: (pixel - 127.5) / 128.0 → [-1, 1]
        // This is equivalent to: pixel / 128.0 - 1.0
        h_frame.convertTo(h_frame, CV_32F, 1.0 / 128.0, -1.0);

        std::cout << "[FaceDetector:" << camera_id_ << "] Preprocessing: "
                  << "Input size: " << h_frame.cols << "x" << h_frame.rows
                  << ", Channels: " << h_frame.channels()
                  << ", Normalization: (x/128.0 - 1.0) -> [-1, 1]" << std::endl;

        // Reshape to CHW format (ONNX Runtime expects NCHW)
        int channels = h_frame.channels();
        int height = h_frame.rows;
        int width = h_frame.cols;

        output.resize(1 * channels * height * width);

        // Convert HWC → CHW
        for (int c = 0; c < channels; c++) {
            for (int h = 0; h < height; h++) {
                for (int w = 0; w < width; w++) {
                    output[c * height * width + h * width + w] =
                        h_frame.at<cv::Vec3f>(h, w)[c];
                }
            }
        }

        return true;

    } catch (const std::exception& e) {
        std::cerr << "[FaceDetector:" << camera_id_ << "] Preprocessing error: "
                  << e.what() << std::endl;
        return false;
    }
}

bool FaceDetector::run_inference(
    const std::vector<float>& input_tensor,
    std::vector<std::vector<float>>& outputs
) {
    try {
        // Create input tensor
        std::vector<int64_t> input_shape = {1, 3, config_.input_size, config_.input_size};

        auto input_tensor_ort = Ort::Value::CreateTensor<float>(
            memory_info_,
            const_cast<float*>(input_tensor.data()),
            input_tensor.size(),
            input_shape.data(),
            input_shape.size()
        );

        // Run inference
        auto output_tensors = session_->Run(
            Ort::RunOptions{nullptr},
            input_names_.data(),
            &input_tensor_ort,
            1,
            output_names_.data(),
            output_names_.size()
        );

        // Extract outputs
        outputs.resize(output_tensors.size());
        for (size_t i = 0; i < output_tensors.size(); i++) {
            float* output_data = output_tensors[i].GetTensorMutableData<float>();
            size_t output_size = output_tensors[i].GetTensorTypeAndShapeInfo().GetElementCount();
            outputs[i].assign(output_data, output_data + output_size);
        }

        return true;

    } catch (const std::exception& e) {
        std::cerr << "[FaceDetector:" << camera_id_ << "] Inference error: "
                  << e.what() << std::endl;
        return false;
    }
}

std::vector<FaceDetection> FaceDetector::postprocess(
    const std::vector<std::vector<float>>& outputs,
    int orig_width,
    int orig_height
) {
    std::vector<FaceDetection> all_detections;

    // SCRFD has 9 outputs: 3 scales (8, 16, 32) × 3 outputs (score, bbox, kps)
    // Output order: score_8, score_16, score_32, bbox_8, bbox_16, bbox_32, kps_8, kps_16, kps_32

    if (outputs.size() != 9) {
        std::cerr << "[FaceDetector:" << camera_id_ << "] Expected 9 outputs, got "
                  << outputs.size() << std::endl;
        return all_detections;
    }

    // Process each scale
    const int strides[] = {8, 16, 32};
    for (int i = 0; i < 3; i++) {
        std::vector<FaceDetection> scale_detections = decode_bboxes(
            outputs[3 + i],  // bbox_{8,16,32}
            outputs[i],      // score_{8,16,32}
            outputs[6 + i],  // kps_{8,16,32}
            strides[i],
            orig_width,
            orig_height
        );

        all_detections.insert(all_detections.end(),
                            scale_detections.begin(),
                            scale_detections.end());
    }

    return all_detections;
}

std::vector<FaceDetection> FaceDetector::decode_bboxes(
    const std::vector<float>& bbox_tensor,
    const std::vector<float>& score_tensor,
    const std::vector<float>& kps_tensor,
    int stride,
    int orig_width,
    int orig_height
) {
    std::vector<FaceDetection> detections;

    // Calculate feature map dimensions
    int fmap_h = config_.input_size / stride;
    int fmap_w = config_.input_size / stride;
    int num_anchors = fmap_h * fmap_w;

    // SCRFD uses 2 anchors per location (aspect ratios 1:1 and 1:1.5)
    const int num_anchors_per_loc = 2;
    const float anchor_sizes[][2] = {{1.0f, 1.0f}, {1.0f, 1.5f}};

    // Generate anchors for this stride
    std::vector<std::array<float, 4>> anchors;
    for (int i = 0; i < fmap_h; i++) {
        for (int j = 0; j < fmap_w; j++) {
            float cx = (j + 0.5f) * stride;
            float cy = (i + 0.5f) * stride;

            for (int k = 0; k < num_anchors_per_loc; k++) {
                float w = stride * anchor_sizes[k][0];
                float h = stride * anchor_sizes[k][1];
                anchors.push_back({cx, cy, w, h});
            }
        }
    }

    // Process each anchor
    for (size_t i = 0; i < anchors.size() && i < score_tensor.size(); i++) {
        float score = score_tensor[i];

        // Apply confidence threshold
        if (score < config_.confidence_threshold) {
            continue;
        }

        // Decode bounding box
        // SCRFD outputs: [dx, dy, dw, dh]
        float dx = bbox_tensor[i * 4 + 0];
        float dy = bbox_tensor[i * 4 + 1];
        float dw = bbox_tensor[i * 4 + 2];
        float dh = bbox_tensor[i * 4 + 3];

        // Apply deltas to anchor
        float cx = anchors[i][0] + dx * anchors[i][2];
        float cy = anchors[i][1] + dy * anchors[i][3];
        float w = anchors[i][2] * std::exp(dw);
        float h = anchors[i][3] * std::exp(dh);

        // Convert to x, y, w, h (top-left corner)
        float x = cx - w / 2.0f;
        float y = cy - h / 2.0f;

        // Scale to original image size (from model input size)
        float scale_x = static_cast<float>(orig_width) / config_.input_size;
        float scale_y = static_cast<float>(orig_height) / config_.input_size;

        x *= scale_x;
        y *= scale_y;
        w *= scale_x;
        h *= scale_y;

        // Clip to image bounds
        x = std::max(0.0f, std::min(x, static_cast<float>(orig_width)));
        y = std::max(0.0f, std::min(y, static_cast<float>(orig_height)));
        w = std::min(w, static_cast<float>(orig_width) - x);
        h = std::min(h, static_cast<float>(orig_height) - y);

        // Filter by minimum face size
        float face_size_ratio = (w * h) / (orig_width * orig_height);
        if (face_size_ratio < config_.min_face_size) {
            continue;
        }

        // Create detection
        FaceDetection det;
        det.confidence = score;

        // Normalize coordinates to [0, 1]
        det.bbox.x = x / orig_width;
        det.bbox.y = y / orig_height;
        det.bbox.width = w / orig_width;
        det.bbox.height = h / orig_height;

        // Decode landmarks (5 points × 2 coordinates = 10 values)
        for (int k = 0; k < 5; k++) {
            if (i * 10 + k * 2 + 1 < kps_tensor.size()) {
                float kp_x = kps_tensor[i * 10 + k * 2 + 0];
                float kp_y = kps_tensor[i * 10 + k * 2 + 1];

                // Landmarks are relative to anchor center
                kp_x = anchors[i][0] + kp_x * anchors[i][2];
                kp_y = anchors[i][1] + kp_y * anchors[i][3];

                // Scale to original image
                kp_x *= scale_x;
                kp_y *= scale_y;

                // Normalize to [0, 1]
                det.landmarks[k].x = kp_x / orig_width;
                det.landmarks[k].y = kp_y / orig_height;
            }
        }

        detections.push_back(det);

        // Limit number of faces per frame
        if (static_cast<int>(detections.size()) >= config_.max_faces) {
            break;
        }
    }

    return detections;
}

std::vector<FaceDetection> FaceDetector::apply_nms(
    const std::vector<FaceDetection>& detections
) {
    if (detections.empty()) {
        return detections;
    }

    std::vector<FaceDetection> result;
    std::vector<bool> suppressed(detections.size(), false);

    // Sort by confidence (descending)
    std::vector<size_t> indices(detections.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t i, size_t j) {
        return detections[i].confidence > detections[j].confidence;
    });

    // Apply NMS
    for (size_t i = 0; i < indices.size(); i++) {
        if (suppressed[indices[i]]) continue;

        result.push_back(detections[indices[i]]);

        for (size_t j = i + 1; j < indices.size(); j++) {
            if (suppressed[indices[j]]) continue;

            float iou = calculate_iou(
                detections[indices[i]].bbox,
                detections[indices[j]].bbox
            );

            if (iou > config_.nms_threshold) {
                suppressed[indices[j]] = true;
            }
        }
    }

    return result;
}

float FaceDetector::calculate_iou(const FaceBoundingBox& a, const FaceBoundingBox& b) {
    float x1 = std::max(a.x, b.x);
    float y1 = std::max(a.y, b.y);
    float x2 = std::min(a.x + a.width, b.x + b.width);
    float y2 = std::min(a.y + a.height, b.y + b.height);

    float intersection = std::max(0.0f, x2 - x1) * std::max(0.0f, y2 - y1);
    float area_a = a.width * a.height;
    float area_b = b.width * b.height;
    float union_area = area_a + area_b - intersection;

    return union_area > 0 ? intersection / union_area : 0.0f;
}

bool FaceDetector::should_trigger_event(bool has_faces) {
    std::lock_guard<std::mutex> lock(face_mutex_);

    // Update face history
    face_history_.pop_front();
    face_history_.push_back(has_faces);

    // Check if we have enough consecutive frames with faces
    int face_count = 0;
    for (bool f : face_history_) {
        if (f) face_count++;
    }

    if (face_count < config_.required_frames) {
        return false;
    }

    // Check cooldown period
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - last_event_time_).count();

    return elapsed >= config_.cooldown_seconds;
}

void FaceDetector::update_statistics(double processing_time_ms, double inference_time_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    stats_.frames_processed++;

    // Update average processing time (exponential moving average)
    const double alpha = 0.1;
    stats_.avg_processing_time_ms =
        alpha * processing_time_ms + (1.0 - alpha) * stats_.avg_processing_time_ms;
    stats_.avg_inference_time_ms =
        alpha * inference_time_ms + (1.0 - alpha) * stats_.avg_inference_time_ms;

    // Update FPS calculation
    fps_frame_count_++;
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - last_fps_update_).count();

    if (elapsed >= 1.0) {
        stats_.current_fps = fps_frame_count_ / elapsed;
        fps_frame_count_ = 0;
        last_fps_update_ = now;
    }
}

void FaceDetector::update_config(const FaceDetectionConfig& config) {
    if (!config.validate()) {
        std::cerr << "[FaceDetector:" << camera_id_ << "] Invalid configuration" << std::endl;
        return;
    }

    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = config;

    // Resize face history buffer if required_frames changed
    if (face_history_.size() != static_cast<size_t>(config_.required_frames)) {
        face_history_.clear();
        face_history_.resize(config_.required_frames, false);
    }

    std::cout << "[FaceDetector:" << camera_id_ << "] Configuration updated" << std::endl;
}

FaceDetectionConfig FaceDetector::get_config() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

void FaceDetector::reset() {
    std::lock_guard<std::mutex> lock(config_mutex_);

    // Reset face history
    std::fill(face_history_.begin(), face_history_.end(), false);

    std::cout << "[FaceDetector:" << camera_id_ << "] Reset completed" << std::endl;
}

FaceDetector::Statistics FaceDetector::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

std::vector<FaceDetection> FaceDetector::get_latest_detections() const {
    std::lock_guard<std::mutex> lock(face_mutex_);
    return latest_detections_;
}

cv::cuda::GpuMat FaceDetector::apply_roi_cuda(const cv::cuda::GpuMat& d_frame) const {
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
