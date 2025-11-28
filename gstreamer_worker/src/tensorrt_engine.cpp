#include "tensorrt_engine.h"

#include <NvInfer.h>
#include <NvOnnxParser.h>

#include <fstream>
#include <iostream>
#include <chrono>
#include <algorithm>

namespace gstreamer_worker {

/**
 * @brief TensorRT logger implementation
 */
class TensorRTEngine::Logger : public nvinfer1::ILogger {
public:
    void log(Severity severity, const char* msg) noexcept override {
        // Filter out INFO messages for cleaner output
        if (severity <= Severity::kWARNING) {
            const char* severity_str = "";
            switch (severity) {
                case Severity::kINTERNAL_ERROR: severity_str = "INTERNAL_ERROR"; break;
                case Severity::kERROR: severity_str = "ERROR"; break;
                case Severity::kWARNING: severity_str = "WARNING"; break;
                case Severity::kINFO: severity_str = "INFO"; break;
                case Severity::kVERBOSE: severity_str = "VERBOSE"; break;
            }
            std::cerr << "[TensorRT:" << severity_str << "] " << msg << std::endl;
        }
    }
};

TensorRTEngine::TensorRTEngine(
    const std::string& model_path,
    bool use_fp16,
    bool use_int8,
    int max_batch_size
)
    : model_path_(model_path)
    , use_fp16_(use_fp16)
    , use_int8_(use_int8)
    , max_batch_size_(max_batch_size)
    , logger_(std::make_unique<Logger>())
{
    std::cout << "[TensorRT] Initializing engine from: " << model_path << std::endl;
    std::cout << "[TensorRT]   FP16: " << (use_fp16 ? "enabled" : "disabled") << std::endl;
    std::cout << "[TensorRT]   INT8: " << (use_int8 ? "enabled" : "disabled") << std::endl;
    std::cout << "[TensorRT]   Max batch size: " << max_batch_size << std::endl;

    // Create TensorRT runtime
    runtime_ = nvinfer1::createInferRuntime(*logger_);
    if (!runtime_) {
        throw std::runtime_error("Failed to create TensorRT runtime");
    }

    // Check if we have a pre-built engine file (*.trt or *.engine)
    std::string engine_path = model_path;
    if (model_path.find(".onnx") != std::string::npos) {
        // Generate engine file name
        engine_path = model_path.substr(0, model_path.find_last_of('.'));
        if (use_fp16) engine_path += "_fp16";
        if (use_int8) engine_path += "_int8";
        engine_path += ".engine";

        // Try to load existing engine
        std::ifstream engine_file(engine_path, std::ios::binary);
        if (engine_file.good()) {
            std::cout << "[TensorRT] Found existing engine: " << engine_path << std::endl;
            if (!load_engine_from_file(engine_path)) {
                std::cout << "[TensorRT] Failed to load engine, rebuilding..." << std::endl;
                if (!build_engine_from_onnx(model_path)) {
                    throw std::runtime_error("Failed to build TensorRT engine from ONNX");
                }
                // Save for next time
                save_engine_to_file(engine_path);
            }
        } else {
            // Build from ONNX
            std::cout << "[TensorRT] Building engine from ONNX (this may take a few minutes)..." << std::endl;
            if (!build_engine_from_onnx(model_path)) {
                throw std::runtime_error("Failed to build TensorRT engine from ONNX");
            }
            // Save for next time
            save_engine_to_file(engine_path);
        }
    } else {
        // Load pre-built engine
        if (!load_engine_from_file(model_path)) {
            throw std::runtime_error("Failed to load TensorRT engine from file");
        }
    }

    // Create execution context
    context_ = engine_->createExecutionContext();
    if (!context_) {
        throw std::runtime_error("Failed to create TensorRT execution context");
    }

    // Initialize tensor bindings
    if (!initialize_bindings()) {
        throw std::runtime_error("Failed to initialize tensor bindings");
    }

    std::cout << "[TensorRT] Engine initialized successfully" << std::endl;
    std::cout << "[TensorRT]   Inputs: " << input_names_.size() << std::endl;
    std::cout << "[TensorRT]   Outputs: " << output_names_.size() << std::endl;
}

TensorRTEngine::~TensorRTEngine() {
    if (context_) {
        delete context_;
    }
    if (engine_) {
        delete engine_;
    }
    if (runtime_) {
        delete runtime_;
    }
}

bool TensorRTEngine::build_engine_from_onnx(const std::string& onnx_path) {
    // Create builder
    auto builder = nvinfer1::createInferBuilder(*logger_);
    if (!builder) {
        std::cerr << "[TensorRT] Failed to create builder" << std::endl;
        return false;
    }

    // Create network definition
    const auto explicit_batch = 1U << static_cast<uint32_t>(
        nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
    auto network = builder->createNetworkV2(explicit_batch);
    if (!network) {
        std::cerr << "[TensorRT] Failed to create network" << std::endl;
        delete builder;
        return false;
    }

    // Create ONNX parser
    auto parser = nvonnxparser::createParser(*network, *logger_);
    if (!parser) {
        std::cerr << "[TensorRT] Failed to create ONNX parser" << std::endl;
        delete network;
        delete builder;
        return false;
    }

    // Parse ONNX model
    if (!parser->parseFromFile(onnx_path.c_str(),
        static_cast<int>(nvinfer1::ILogger::Severity::kWARNING))) {
        std::cerr << "[TensorRT] Failed to parse ONNX model" << std::endl;
        for (int i = 0; i < parser->getNbErrors(); ++i) {
            std::cerr << "[TensorRT] Parser error: " << parser->getError(i)->desc() << std::endl;
        }
        delete parser;
        delete network;
        delete builder;
        return false;
    }

    std::cout << "[TensorRT] ONNX model parsed successfully" << std::endl;

    // Create builder config
    auto config = builder->createBuilderConfig();
    if (!config) {
        std::cerr << "[TensorRT] Failed to create builder config" << std::endl;
        delete parser;
        delete network;
        delete builder;
        return false;
    }

    // Set memory pool limits (8GB for workspace)
    config->setMemoryPoolLimit(nvinfer1::MemoryPoolType::kWORKSPACE, 8ULL << 30);

    // Enable FP16 mode if requested and supported
    if (use_fp16_ && builder->platformHasFastFp16()) {
        config->setFlag(nvinfer1::BuilderFlag::kFP16);
        std::cout << "[TensorRT] FP16 mode enabled" << std::endl;
    } else if (use_fp16_) {
        std::cout << "[TensorRT] FP16 requested but not supported on this platform" << std::endl;
    }

    // Enable INT8 mode if requested and supported
    if (use_int8_ && builder->platformHasFastInt8()) {
        config->setFlag(nvinfer1::BuilderFlag::kINT8);
        std::cout << "[TensorRT] INT8 mode enabled (requires calibration)" << std::endl;
        // Note: INT8 requires calibration data which is not implemented here
        // For production, you'd need to implement IInt8Calibrator
    } else if (use_int8_) {
        std::cout << "[TensorRT] INT8 requested but not supported on this platform" << std::endl;
    }

    // Build engine
    std::cout << "[TensorRT] Building optimized engine (this may take several minutes)..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    auto serialized_engine = builder->buildSerializedNetwork(*network, *config);
    if (!serialized_engine) {
        std::cerr << "[TensorRT] Failed to build engine" << std::endl;
        delete config;
        delete parser;
        delete network;
        delete builder;
        return false;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    std::cout << "[TensorRT] Engine built in " << duration << " seconds" << std::endl;

    // Deserialize engine
    engine_ = runtime_->deserializeCudaEngine(serialized_engine->data(), serialized_engine->size());
    delete serialized_engine;

    if (!engine_) {
        std::cerr << "[TensorRT] Failed to deserialize engine" << std::endl;
        delete config;
        delete parser;
        delete network;
        delete builder;
        return false;
    }

    // Cleanup
    delete config;
    delete parser;
    delete network;
    delete builder;

    return true;
}

bool TensorRTEngine::load_engine_from_file(const std::string& engine_path) {
    std::ifstream file(engine_path, std::ios::binary);
    if (!file.good()) {
        std::cerr << "[TensorRT] Failed to open engine file: " << engine_path << std::endl;
        return false;
    }

    // Read entire file
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> engine_data(size);
    file.read(engine_data.data(), size);
    file.close();

    // Deserialize engine
    engine_ = runtime_->deserializeCudaEngine(engine_data.data(), size);
    if (!engine_) {
        std::cerr << "[TensorRT] Failed to deserialize engine" << std::endl;
        return false;
    }

    std::cout << "[TensorRT] Loaded engine from file: " << engine_path << std::endl;
    return true;
}

bool TensorRTEngine::save_engine_to_file(const std::string& engine_path) {
    if (!engine_) {
        std::cerr << "[TensorRT] No engine to save" << std::endl;
        return false;
    }

    auto serialized_engine = engine_->serialize();
    if (!serialized_engine) {
        std::cerr << "[TensorRT] Failed to serialize engine" << std::endl;
        return false;
    }

    std::ofstream file(engine_path, std::ios::binary);
    if (!file.good()) {
        std::cerr << "[TensorRT] Failed to create engine file: " << engine_path << std::endl;
        delete serialized_engine;
        return false;
    }

    file.write(reinterpret_cast<const char*>(serialized_engine->data()), serialized_engine->size());
    file.close();
    delete serialized_engine;

    std::cout << "[TensorRT] Saved engine to: " << engine_path << std::endl;
    return true;
}

bool TensorRTEngine::initialize_bindings() {
    if (!engine_) {
        std::cerr << "[TensorRT] No engine to initialize bindings" << std::endl;
        return false;
    }

    int num_bindings = engine_->getNbIOTensors();
    std::cout << "[TensorRT] Initializing " << num_bindings << " tensor bindings" << std::endl;

    for (int i = 0; i < num_bindings; ++i) {
        const char* name = engine_->getIOTensorName(i);
        auto dims = engine_->getTensorShape(name);
        auto dtype = engine_->getTensorDataType(name);
        bool is_input = (engine_->getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT);

        // Calculate size in bytes
        size_t size_bytes = 1;
        for (int d = 0; d < dims.nbDims; ++d) {
            size_bytes *= dims.d[d];
        }

        // Data type size
        size_t dtype_size = 0;
        switch (dtype) {
            case nvinfer1::DataType::kFLOAT: dtype_size = 4; break;
            case nvinfer1::DataType::kHALF: dtype_size = 2; break;
            case nvinfer1::DataType::kINT8: dtype_size = 1; break;
            case nvinfer1::DataType::kINT32: dtype_size = 4; break;
            case nvinfer1::DataType::kBOOL: dtype_size = 1; break;
            default:
                std::cerr << "[TensorRT] Unsupported data type for tensor: " << name << std::endl;
                return false;
        }
        size_bytes *= dtype_size;

        // Store tensor info
        TensorInfo info;
        info.name = name;
        info.dims.resize(dims.nbDims);
        for (int d = 0; d < dims.nbDims; ++d) {
            info.dims[d] = dims.d[d];
        }
        info.size_bytes = size_bytes;
        info.is_input = is_input;
        info.binding_index = i;

        tensor_info_[name] = info;

        if (is_input) {
            input_names_.push_back(name);
        } else {
            output_names_.push_back(name);
        }

        std::cout << "[TensorRT]   " << (is_input ? "Input " : "Output")
                  << " '" << name << "': [";
        for (int d = 0; d < dims.nbDims; ++d) {
            std::cout << dims.d[d];
            if (d < dims.nbDims - 1) std::cout << ", ";
        }
        std::cout << "] (" << size_bytes / 1024 << " KB)" << std::endl;
    }

    return true;
}

bool TensorRTEngine::infer(
    const std::unordered_map<std::string, void*>& d_input_buffers,
    const std::unordered_map<std::string, void*>& d_output_buffers,
    cudaStream_t stream
) {
    if (!context_) {
        std::cerr << "[TensorRT] No execution context" << std::endl;
        return false;
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Set input bindings
    for (const auto& [name, buffer] : d_input_buffers) {
        if (tensor_info_.find(name) == tensor_info_.end()) {
            std::cerr << "[TensorRT] Unknown input tensor: " << name << std::endl;
            return false;
        }
        if (!context_->setTensorAddress(name.c_str(), buffer)) {
            std::cerr << "[TensorRT] Failed to set input tensor address: " << name << std::endl;
            return false;
        }
    }

    // Set output bindings
    for (const auto& [name, buffer] : d_output_buffers) {
        if (tensor_info_.find(name) == tensor_info_.end()) {
            std::cerr << "[TensorRT] Unknown output tensor: " << name << std::endl;
            return false;
        }
        if (!context_->setTensorAddress(name.c_str(), buffer)) {
            std::cerr << "[TensorRT] Failed to set output tensor address: " << name << std::endl;
            return false;
        }
    }

    // Execute inference
    if (!context_->enqueueV3(stream)) {
        std::cerr << "[TensorRT] Inference failed" << std::endl;
        return false;
    }

    // Synchronize stream if provided
    if (stream) {
        cudaStreamSynchronize(stream);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    // Update metrics
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.total_inferences++;
        metrics_.avg_inference_time_ms =
            (metrics_.avg_inference_time_ms * (metrics_.total_inferences - 1) + duration_ms) /
            metrics_.total_inferences;
        metrics_.using_fp16 = use_fp16_;
        metrics_.using_int8 = use_int8_;
    }

    return true;
}

const TensorRTEngine::TensorInfo* TensorRTEngine::get_tensor_info(const std::string& tensor_name) const {
    auto it = tensor_info_.find(tensor_name);
    if (it == tensor_info_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> TensorRTEngine::get_input_names() const {
    return input_names_;
}

std::vector<std::string> TensorRTEngine::get_output_names() const {
    return output_names_;
}

TensorRTEngine::PerformanceMetrics TensorRTEngine::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_;
}

} // namespace gstreamer_worker
