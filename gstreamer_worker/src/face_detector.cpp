#include "face_detector.h"
#include "logger.h"
#include "cuda_kernels.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cuda_runtime.h>

namespace gstreamer_worker {

FaceDetector::FaceDetector(
    const std::string& camera_id,
    const FaceDetectionConfig& config,
    FaceCallback on_face,
    PersonTracker* person_tracker
)
    : camera_id_(camera_id)
    , config_(config)
    , on_face_(on_face)
    , person_tracker_(person_tracker)
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

    // Initialize CompreFace client if enabled
    if (config_.enable_compreface) {
        try {
            CompreFaceConfig cf_config;
            cf_config.base_url = config_.compreface_url;
            cf_config.api_key = config_.compreface_api_key;
            cf_config.timeout_ms = config_.compreface_timeout_ms;
            cf_config.max_queue_size = config_.compreface_max_queue_size;

            // Create recognition callback
            auto recognition_callback = [this](
                const std::string& camera_id,
                const std::string& face_crop_path,
                const std::optional<RecognitionResult>& result,
                const std::string& error
            ) {
                this->on_recognition_result(camera_id, face_crop_path, result, error);
            };

            compreface_client_ = std::make_unique<CompreFaceClient>(cf_config, recognition_callback);
            compreface_client_->start();

            std::cout << "[FaceDetector:" << camera_id_ << "] CompreFace client initialized" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[FaceDetector:" << camera_id_ << "] CompreFace initialization failed: "
                      << e.what() << std::endl;
        }
    }
}

FaceDetector::~FaceDetector() {
    std::cout << "[FaceDetector:" << camera_id_ << "] Shutting down" << std::endl;

    // Stop CompreFace client
    if (compreface_client_) {
        compreface_client_->stop();
    }

    // Clean up GPU memory for zero-copy preprocessing
    if (d_preprocessed_tensor_) {
        cudaFree(d_preprocessed_tensor_);
        d_preprocessed_tensor_ = nullptr;
    }

    // Destroy CUDA stream
    if (cuda_stream_) {
        cudaStreamDestroy(cuda_stream_);
        cuda_stream_ = 0;
    }
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

        // Apply NMS
        if (!detections.empty()) {
            detections = apply_nms(detections);
        }

        // Filter out faces that are already being tracked
        // Only trigger events for NEW faces
        std::vector<FaceDetection> new_face_detections = filter_tracked_faces(detections);

        // Add new faces to tracking (so we don't re-detect them)
        if (!new_face_detections.empty()) {
            update_face_tracking(new_face_detections);
        }

        // Store latest detections for visualization (show all detections)
        {
            std::lock_guard<std::mutex> lock(face_mutex_);
            latest_detections_ = detections;
        }

        // Check if we should trigger an event (only for NEW faces)
        faces_detected = !new_face_detections.empty();
        if (should_trigger_event(faces_detected)) {
            // Create face event (only for NEW faces)
            FaceEvent event;
            event.camera_id = camera_id_;
            event.timestamp = std::chrono::duration<double>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            event.num_faces = static_cast<int>(new_face_detections.size());
            event.faces = new_face_detections;

            // Update last event time
            {
                std::lock_guard<std::mutex> lock(face_mutex_);
                last_event_time_ = std::chrono::steady_clock::now();
            }

            // Save face crops for verification and CompreFace integration (only NEW faces)
            save_face_crops(d_frame, new_face_detections, event.timestamp);

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
        cv::cuda::GpuMat d_working = d_frame;
        if (config_.roi.is_valid()) {
            d_working = apply_roi_cuda(d_frame);
        }

        cv::cuda::resize(d_working, d_resized_, cv::Size(config_.input_size, config_.input_size));

        const int height   = config_.input_size;
        const int width    = config_.input_size;
        const int channels = 3;
        const size_t tensor_size = static_cast<size_t>(channels) * height * width;

        if (!d_preprocessed_tensor_ || d_preprocessed_tensor_size_ != tensor_size) {
            if (d_preprocessed_tensor_) {
                cudaFree(d_preprocessed_tensor_);
            }
            cudaMalloc(&d_preprocessed_tensor_, tensor_size * sizeof(float));
            d_preprocessed_tensor_size_ = tensor_size;

            if (!cuda_stream_) {
                cudaStreamCreate(&cuda_stream_);
            }

            std::cout << "[FaceDetector:" << camera_id_ << "] Allocated GPU tensor: "
                      << tensor_size * sizeof(float) / (1024 * 1024) << " MB" << std::endl;
        }

        // 🔴 IMPORTANT: use GpuMat.step (bytes per row)
        const int input_step = static_cast<int>(d_resized_.step);

        launch_preprocess_face_detection_coalesced(
            d_resized_.ptr<uint8_t>(),
            input_step,
            d_preprocessed_tensor_,
            height,
            width,
            cuda_stream_
        );

        cudaStreamSynchronize(cuda_stream_);

        output.resize(tensor_size);
        cudaMemcpy(output.data(), d_preprocessed_tensor_,
                   tensor_size * sizeof(float), cudaMemcpyDeviceToHost);

#ifdef DEBUG
        // Optional: sanity-check input range
        auto [min_it, max_it] = std::minmax_element(output.begin(), output.end());
        std::cout << "[DEBUG] input_tensor min=" << *min_it
                  << " max=" << *max_it << std::endl;
#endif

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[FaceDetector:" << camera_id_ << "] Preprocessing error: "
                  << e.what() << std::endl;
        return false;
    } catch (const cudaError_t& err) {
        std::cerr << "[FaceDetector:" << camera_id_ << "] CUDA error: "
                  << cudaGetErrorString(err) << std::endl;
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
        // SCRFD 10g uses distance-based encoding: [left, top, right, bottom]
        // These are distances from the anchor point to box edges (in strides)
        float dist_left = bbox_tensor[i * 4 + 0];
        float dist_top = bbox_tensor[i * 4 + 1];
        float dist_right = bbox_tensor[i * 4 + 2];
        float dist_bottom = bbox_tensor[i * 4 + 3];

        // Convert distances to box coordinates
        float left = anchors[i][0] - dist_left * stride;
        float top = anchors[i][1] - dist_top * stride;
        float right = anchors[i][0] + dist_right * stride;
        float bottom = anchors[i][1] + dist_bottom * stride;

        // Convert to x, y, w, h format
        float x = left;
        float y = top;
        float w = right - left;
        float h = bottom - top;

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

        // Skip invalid boxes
        if (w <= 0 || h <= 0) {
            continue;
        }

        // Filter by minimum face size
        float face_size_ratio = (w * h) / (orig_width * orig_height);
        if (face_size_ratio < config_.min_face_size) {
            continue;
        }

        // TEMPORARILY DISABLED FOR DEBUGGING
        // Filter out abnormally tall/wide faces (likely false positives)
        // float aspect_ratio = w / h;
        // if (aspect_ratio < 0.5f || aspect_ratio > 2.0f) {
        //     continue;
        // }

        // Filter out faces that span too much of the frame (likely edge artifacts)
        // Real faces shouldn't take up more than 80% of frame height/width
        float height_ratio = h / orig_height;
        float width_ratio = w / orig_width;
        if (height_ratio > 0.8f || width_ratio > 0.8f) {
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

std::vector<FaceDetector::PersonTrackingInfo> FaceDetector::get_tracked_persons() const {
    std::lock_guard<std::mutex> lock(face_mutex_);

    std::vector<PersonTrackingInfo> persons;
    persons.reserve(tracked_faces_.size());

    for (const auto& tracked : tracked_faces_) {
        PersonTrackingInfo info;
        info.subject = tracked.subject;
        info.recognition_confidence = tracked.recognition_confidence;
        info.is_recognized = tracked.is_recognized;
        info.duration_seconds = tracked.get_duration_seconds();
        info.frames_tracked = tracked.frames_tracked;
        info.current_bbox = tracked.bbox;
        persons.push_back(info);
    }

    return persons;
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

void FaceDetector::save_face_crops(
    const cv::cuda::GpuMat& d_frame,
    const std::vector<FaceDetection>& detections,
    double event_timestamp
) {
    if (!config_.save_faces || detections.empty()) {
        return;
    }

    // Create save directory if it doesn't exist
    std::string mkdir_cmd = "mkdir -p " + config_.save_path;
    system(mkdir_cmd.c_str());

    // Download frame from GPU to CPU once
    cv::Mat h_frame;
    d_frame.download(h_frame);

    int saved_count = 0;
    int face_number = 1;  // Counter for saved faces (increments only when face is actually saved)

    for (size_t i = 0; i < detections.size() && saved_count < config_.max_saves_per_event; i++) {
        const auto& detection = detections[i];

        // Skip if confidence too low
        if (detection.confidence < config_.min_save_confidence) {
            continue;
        }

        // Convert normalized coordinates to pixels
        int frame_width = h_frame.cols;
        int frame_height = h_frame.rows;

        // CRITICAL FIX: Dynamically adjust margin to prevent including other faces
        // When multiple faces are close together, we need to ensure each crop
        // contains ONLY the intended face, not neighboring faces.
        float margin = config_.save_margin;

        // Check if crop with current margin would overlap with any other detected faces
        bool has_nearby_faces = false;
        for (size_t j = 0; j < detections.size(); j++) {
            if (i == j) continue;  // Skip current face

            const auto& other_face = detections[j];

            // Calculate potential crop bounds with current margin
            float crop_x_min = detection.bbox.x - (detection.bbox.width * margin / 2.0f);
            float crop_y_min = detection.bbox.y - (detection.bbox.height * margin / 2.0f);
            float crop_x_max = crop_x_min + detection.bbox.width * (1.0f + margin);
            float crop_y_max = crop_y_min + detection.bbox.height * (1.0f + margin);

            // Check if other face's center is inside our crop region
            float other_center_x = other_face.bbox.x + other_face.bbox.width / 2.0f;
            float other_center_y = other_face.bbox.y + other_face.bbox.height / 2.0f;

            if (other_center_x >= crop_x_min && other_center_x <= crop_x_max &&
                other_center_y >= crop_y_min && other_center_y <= crop_y_max) {
                has_nearby_faces = true;

                // Calculate distance between faces
                float dist_x = std::abs(detection.bbox.x - other_face.bbox.x);
                float dist_y = std::abs(detection.bbox.y - other_face.bbox.y);
                float distance = std::sqrt(dist_x * dist_x + dist_y * dist_y);

                // Reduce margin proportionally to distance
                // If faces are very close, use minimal margin (0.1)
                // If faces are farther, can use more margin
                float max_safe_margin = std::max(0.1f, (distance / detection.bbox.width) - 1.0f);
                margin = std::min(margin, max_safe_margin);

                std::cout << "[FaceDetector:" << camera_id_ << "] ⚠️  Face #" << (i+1)
                          << " has nearby face #" << (j+1) << " at distance " << distance
                          << " - reducing margin from " << config_.save_margin
                          << " to " << margin << " to prevent overlap" << std::endl;
            }
        }

        // Use minimum margin of 0.1 (10%) to ensure we have some context
        margin = std::max(0.1f, margin);

        // Calculate face box with adjusted margin
        float x = detection.bbox.x - (detection.bbox.width * margin / 2.0f);
        float y = detection.bbox.y - (detection.bbox.height * margin / 2.0f);
        float w = detection.bbox.width * (1.0f + margin);
        float h = detection.bbox.height * (1.0f + margin);

        // Clamp to frame boundaries
        x = std::max(0.0f, std::min(x, 1.0f));
        y = std::max(0.0f, std::min(y, 1.0f));
        w = std::min(w, 1.0f - x);
        h = std::min(h, 1.0f - y);

        // Convert to pixel coordinates
        int px = static_cast<int>(x * frame_width);
        int py = static_cast<int>(y * frame_height);
        int pw = static_cast<int>(w * frame_width);
        int ph = static_cast<int>(h * frame_height);

        // Ensure valid crop dimensions
        if (pw <= 0 || ph <= 0 || px + pw > frame_width || py + ph > frame_height) {
            continue;
        }

        // Crop face with margin
        cv::Mat face_crop = h_frame(cv::Rect(px, py, pw, ph));

        // Check if this face belongs to a tracked face - implement retry logic
        bool should_skip = false;
        {
            std::lock_guard<std::mutex> lock(face_mutex_);
            for (auto& tracked : tracked_faces_) {
                float iou = calculate_iou(detection.bbox, tracked.bbox);
                if (iou > face_tracking_iou_threshold_) {
                    // Skip if already successfully recognized OR max attempts reached
                    if (tracked.is_recognized || tracked.recognition_attempts >= MAX_RECOGNITION_ATTEMPTS) {
                        should_skip = true;
                        std::cout << "[FaceDetector:" << camera_id_ << "] Face already processed: "
                                  << "recognized=" << (tracked.is_recognized ? "YES" : "NO")
                                  << ", attempts=" << tracked.recognition_attempts << std::endl;
                        break;
                    }
                    // Allow retry if recognition failed and haven't reached max attempts
                    std::cout << "[FaceDetector:" << camera_id_ << "] Retry attempt "
                              << (tracked.recognition_attempts + 1) << "/" << MAX_RECOGNITION_ATTEMPTS << std::endl;
                }
            }
        }

        if (should_skip) {
            continue;
        }

        // Check minimum face size in pixels (CompreFace needs at least 80x80)
        const int MIN_FACE_WIDTH = 80;
        const int MIN_FACE_HEIGHT = 80;
        if (pw < MIN_FACE_WIDTH || ph < MIN_FACE_HEIGHT) {
            std::cout << "[FaceDetector:" << camera_id_ << "] Skipping small face ("
                      << pw << "x" << ph << " pixels) - too small for CompreFace (needs 80x80+)" << std::endl;
            continue;
        }

        // Check if face is frontal (not profile/partial/back-of-head)
        if (!is_frontal_face(detection)) {
            // Skip non-frontal faces - only save frontal faces for recognition
            std::cout << "[FaceDetector:" << camera_id_ << "] Skipping non-frontal face (profile/partial/turning)" << std::endl;
            continue;
        }

        // Check blur if enabled
        if (config_.enable_blur_detection) {
            double blur_score = calculate_blur_score(face_crop);
            if (blur_score < config_.min_laplacian_variance) {
                // Skip blurry images - only save high quality for CompreFace
                std::cout << "[FaceDetector:" << camera_id_ << "] Skipping blurry face (score: "
                          << static_cast<int>(blur_score) << " < "
                          << static_cast<int>(config_.min_laplacian_variance) << ")" << std::endl;
                continue;
            }
        }

        // Check brightness (prevent saving dark/black images from low-light)
        cv::Scalar mean_brightness = cv::mean(face_crop);
        double avg_brightness = (mean_brightness[0] + mean_brightness[1] + mean_brightness[2]) / 3.0;
        const double MIN_BRIGHTNESS = 30.0;  // 0-255 scale, 30 = very dark threshold
        if (avg_brightness < MIN_BRIGHTNESS) {
            std::cout << "[FaceDetector:" << camera_id_ << "] Skipping dark/underexposed face (brightness: "
                      << static_cast<int>(avg_brightness) << " < " << static_cast<int>(MIN_BRIGHTNESS)
                      << ") - improve lighting!" << std::endl;
            continue;
        }

        // Generate filename: {camera_id}_{timestamp}_{face_num}_{confidence}.jpg
        // This format is compatible with CompreFace and easy to organize
        std::stringstream filename;
        filename << config_.save_path << "/"
                 << camera_id_ << "_"
                 << std::fixed << std::setprecision(3) << event_timestamp << "_"
                 << "face" << face_number << "_"
                 << static_cast<int>(detection.confidence * 100) << ".jpg";
        face_number++;  // Increment for next saved face

        // Save face crop
        try {
            cv::imwrite(filename.str(), face_crop);
            std::cout << "[FaceDetector:" << camera_id_ << "] Saved face crop: "
                      << filename.str() << " (" << pw << "x" << ph << " pixels, "
                      << static_cast<int>(detection.confidence * 100) << "% confidence)"
                      << std::endl;
            saved_count++;

            // Send to CompreFace for recognition
            if (compreface_client_ && config_.enable_compreface) {
                std::cout << "[FaceDetector:" << camera_id_ << "] Sending to CompreFace: " << filename.str() << std::endl;
                bool queued = compreface_client_->recognize_async(
                    camera_id_,
                    filename.str(),
                    detection.confidence
                );
                if (queued) {
                    std::cout << "[FaceDetector:" << camera_id_ << "] Successfully queued for recognition" << std::endl;
                } else {
                    std::cerr << "[FaceDetector:" << camera_id_ << "] Failed to queue recognition (queue full or circuit breaker open)" << std::endl;
                }
            } else {
                if (!compreface_client_) {
                    std::cerr << "[FaceDetector:" << camera_id_ << "] CompreFace client not initialized!" << std::endl;
                }
                if (!config_.enable_compreface) {
                    std::cout << "[FaceDetector:" << camera_id_ << "] CompreFace disabled in config" << std::endl;
                }
            }

            // Increment recognition attempts for this tracked face
            {
                std::lock_guard<std::mutex> lock(face_mutex_);
                for (auto& tracked : tracked_faces_) {
                    float iou = calculate_iou(detection.bbox, tracked.bbox);
                    if (iou > face_tracking_iou_threshold_) {
                        tracked.crop_saved = true;
                        tracked.recognition_attempts++;
                        std::cout << "[FaceDetector:" << camera_id_ << "] Recognition attempt "
                                  << tracked.recognition_attempts << " sent for this person" << std::endl;
                        break;
                    }
                }
            }

        } catch (const std::exception& e) {
            std::cerr << "[FaceDetector:" << camera_id_ << "] Failed to save face crop: "
                      << e.what() << std::endl;
        }
    }
}

std::vector<FaceDetection> FaceDetector::filter_tracked_faces(
    const std::vector<FaceDetection>& detections
) {
    std::lock_guard<std::mutex> lock(face_mutex_);

    // Remove stale tracked faces (not seen for face_tracking_timeout_ seconds)
    auto now = std::chrono::steady_clock::now();

    // Log tracking summary for removed faces (production analytics)
    for (auto it = tracked_faces_.begin(); it != tracked_faces_.end(); ) {
        double elapsed = std::chrono::duration<double>(now - it->last_seen).count();
        if (elapsed > face_tracking_timeout_) {
            double duration = it->get_duration_seconds();
            std::cout << "[FaceDetector:" << camera_id_ << "] Person tracking ended: "
                      << "subject=" << it->subject
                      << ", duration=" << std::fixed << std::setprecision(1) << duration << "s"
                      << ", frames=" << it->frames_tracked
                      << (it->is_recognized ? " [RECOGNIZED]" : " [UNKNOWN]")
                      << std::endl;
            it = tracked_faces_.erase(it);
        } else {
            ++it;
        }
    }

    // Filter out detections that match existing tracked faces
    std::vector<FaceDetection> new_faces;
    for (const auto& detection : detections) {
        bool is_tracked = false;

        // Check if this detection matches any tracked face
        for (auto& tracked : tracked_faces_) {
            float iou = calculate_iou(detection.bbox, tracked.bbox);
            if (iou > face_tracking_iou_threshold_) {
                // This face is already being tracked
                is_tracked = true;
                tracked.last_seen = now;
                tracked.frames_tracked++;
                tracked.bbox = detection.bbox;  // Update position
                break;
            }
        }

        if (!is_tracked) {
            new_faces.push_back(detection);
        }
    }

    return new_faces;
}

void FaceDetector::update_face_tracking(const std::vector<FaceDetection>& detections) {
    std::lock_guard<std::mutex> lock(face_mutex_);

    auto now = std::chrono::steady_clock::now();

    // CRITICAL FIX (Issue #7): Remove stale tracks FIRST to prevent unbounded memory growth
    // Remove faces that haven't been seen for face_tracking_timeout_ seconds
    tracked_faces_.erase(
        std::remove_if(tracked_faces_.begin(), tracked_faces_.end(),
            [this, now](const TrackedFace& tf) {
                auto time_since_last_seen = std::chrono::duration<double>(now - tf.last_seen).count();
                if (time_since_last_seen > face_tracking_timeout_) {
                    std::cout << "[FaceDetector:" << camera_id_ << "] Removing stale track "
                              << "(last seen " << time_since_last_seen << "s ago, "
                              << "tracked for " << tf.frames_tracked << " frames)" << std::endl;
                    return true;  // Remove this track
                }
                return false;  // Keep this track
            }),
        tracked_faces_.end()
    );

    // Update existing tracks or add new faces
    for (const auto& detection : detections) {
        // Check if already tracked (update existing track)
        bool already_tracked = false;
        for (auto& tracked : tracked_faces_) {
            float iou = calculate_iou(detection.bbox, tracked.bbox);
            if (iou > face_tracking_iou_threshold_) {
                // Update existing track
                tracked.bbox = detection.bbox;
                tracked.last_seen = now;
                tracked.frames_tracked++;
                already_tracked = true;
                break;
            }
        }

        if (!already_tracked) {
            // Add new track
            TrackedFace tf;
            tf.bbox = detection.bbox;
            tf.first_seen = now;
            tf.last_seen = now;
            tf.frames_tracked = 1;
            tracked_faces_.push_back(tf);

            std::cout << "[FaceDetector:" << camera_id_ << "] Started tracking new face "
                      << "(total tracks: " << tracked_faces_.size() << ")" << std::endl;
        }
    }
}

double FaceDetector::calculate_blur_score(const cv::Mat& face_crop) {
    // Convert to grayscale if needed
    cv::Mat gray;
    if (face_crop.channels() == 3) {
        cv::cvtColor(face_crop, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = face_crop;
    }

    // Calculate Laplacian
    cv::Mat laplacian;
    cv::Laplacian(gray, laplacian, CV_64F, config_.blur_kernel_size);

    // Calculate variance of Laplacian
    cv::Scalar mean, stddev;
    cv::meanStdDev(laplacian, mean, stddev);
    double variance = stddev[0] * stddev[0];

    return variance;
}

bool FaceDetector::is_frontal_face(const FaceDetection& detection) {
    // SCRFD 5-point landmarks:
    // [0] = left eye, [1] = right eye, [2] = nose, [3] = left mouth, [4] = right mouth

    const auto& landmarks = detection.landmarks;

    // Check if landmarks are valid (not at 0,0)
    bool has_valid_landmarks = true;
    for (int i = 0; i < 5; i++) {
        if (landmarks[i].x < 0.01 && landmarks[i].y < 0.01) {
            has_valid_landmarks = false;
            break;
        }
    }

    if (!has_valid_landmarks) {
        return false;  // No landmarks = can't determine orientation
    }

    // 1. Check eye symmetry (both eyes should be visible and roughly at same height)
    float left_eye_x = landmarks[0].x;
    float left_eye_y = landmarks[0].y;
    float right_eye_x = landmarks[1].x;
    float right_eye_y = landmarks[1].y;

    // Eyes should be horizontally aligned (not tilted more than 15 degrees)
    float eye_y_diff = std::abs(left_eye_y - right_eye_y);
    float eye_distance = std::abs(right_eye_x - left_eye_x);

    if (eye_distance < 0.01) {
        return false;  // Eyes too close = profile or bad detection
    }

    float eye_tilt_ratio = eye_y_diff / eye_distance;
    if (eye_tilt_ratio > 0.3) {  // More than ~17 degree tilt
        return false;
    }

    // 2. Check if eyes are above nose (detect upside-down or weird angles)
    float nose_y = landmarks[2].y;
    float avg_eye_y = (left_eye_y + right_eye_y) / 2.0f;

    if (nose_y < avg_eye_y) {
        return false;  // Nose above eyes = inverted or bad detection
    }

    // 3. Check nose centering (should be roughly centered between eyes for frontal face)
    float nose_x = landmarks[2].x;
    float eye_center_x = (left_eye_x + right_eye_x) / 2.0f;
    float nose_offset = std::abs(nose_x - eye_center_x);

    // Nose should be within 30% of inter-eye distance from center
    if (nose_offset > eye_distance * 0.3) {
        return false;  // Nose too far off-center = profile
    }

    // 4. Check mouth position (should be below nose and centered)
    float left_mouth_x = landmarks[3].x;
    float right_mouth_x = landmarks[4].x;
    float left_mouth_y = landmarks[3].y;
    float right_mouth_y = landmarks[4].y;

    float mouth_center_y = (left_mouth_y + right_mouth_y) / 2.0f;

    if (mouth_center_y < nose_y) {
        return false;  // Mouth above nose = bad detection
    }

    // 5. Check face width vs inter-eye distance ratio
    // For frontal faces, inter-eye distance should be roughly 40-50% of face width
    float face_width = detection.bbox.width;
    float eye_to_face_ratio = eye_distance / face_width;

    if (eye_to_face_ratio < 0.25 || eye_to_face_ratio > 0.65) {
        return false;  // Unusual ratio = profile or extreme angle
    }

    // All checks passed - this is a frontal face
    return true;
}

void FaceDetector::on_recognition_result(
    const std::string& camera_id,
    const std::string& face_crop_path,
    const std::optional<RecognitionResult>& result,
    const std::string& error
) {
    std::lock_guard<std::mutex> lock(face_mutex_);

    if (!result.has_value()) {
        // Recognition failed - reset crop_saved to allow retry
        if (!error.empty()) {
            std::cerr << "[FaceDetector:" << camera_id_ << "] Recognition failed: " << error << std::endl;
            std::cerr << "[FaceDetector:" << camera_id_ << "] Will retry with better quality image" << std::endl;
        }

        // Find the most recently saved face and mark for retry
        for (auto& tracked : tracked_faces_) {
            if (tracked.crop_saved && !tracked.is_recognized) {
                tracked.crop_saved = false;  // Allow retry
                tracked.failed_recognitions++;
                std::cout << "[FaceDetector:" << camera_id_ << "] Reset retry flag (failed: "
                          << tracked.failed_recognitions << ")" << std::endl;
                break;
            }
        }
        return;
    }

    const auto& recognition = result.value();

    // Find the most recently updated tracked face that doesn't have recognition yet
    // This assumes face crops are sent shortly after detection
    for (auto& tracked : tracked_faces_) {
        if (!tracked.is_recognized || tracked.subject == "unknown") {
            tracked.subject = recognition.subject;
            tracked.recognition_confidence = recognition.similarity;
            tracked.is_recognized = recognition.is_match;
            tracked.face_id = recognition.face_id;

            std::cout << "[FaceDetector:" << camera_id_ << "] ✅ Person recognized: "
                      << "subject=" << recognition.subject
                      << ", similarity=" << std::fixed << std::setprecision(2) << (recognition.similarity * 100.0f) << "%"
                      << ", match=" << (recognition.is_match ? "YES" : "NO")
                      << " (after " << tracked.recognition_attempts << " attempts)"
                      << std::endl;

            // Update PersonTracker with recognition result
            if (person_tracker_ && recognition.is_match) {
                // Create FaceDetection from tracked face data
                FaceDetection detection;
                detection.bbox = tracked.bbox;
                detection.confidence = tracked.recognition_confidence;
                // Note: landmarks not available here, but not needed for PersonTracker

                // Update person tracking
                person_tracker_->update_person(camera_id_, recognition, detection);
            }

            // Only update the first unrecognized face
            break;
        }
    }
}

} // namespace gstreamer_worker
