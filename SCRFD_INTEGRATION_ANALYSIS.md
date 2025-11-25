# SCRFD Face Detection Integration Analysis

## Executive Summary

This document outlines the architecture and implementation plan for integrating SCRFD (Sample and Computation Redistribution for Efficient Face Detection) into the existing GStreamer camera surveillance pipeline.

**Key Goals:**
- Real-time face detection with GPU acceleration
- Seamless integration with existing motion detection system
- Minimal performance impact on video streaming
- Production-ready with proper error handling and monitoring

---

## 1. Current Architecture Analysis

### 1.1 Existing Pipeline Structure

```
┌─────────────────────────────────────────────────────────────┐
│ RTSP Source → Decoder → Converter → VideoRate → Tee        │
│                                                   ├→ Display │
│                                                   └→ AppSink │
│                                                       ↓      │
│                                              MotionDetector  │
└─────────────────────────────────────────────────────────────┘
```

**Current Processing Flow:**
1. **GStreamer Pipeline**: Handles video decoding and display
2. **AppSink**: Extracts frames in BGR format
3. **MotionDetector**: Processes frames with CUDA-accelerated OpenCV
4. **Event System**: WebSocket broadcasts motion events to FastAPI backend

### 1.2 Motion Detection Architecture (Template for Face Detection)

```cpp
class MotionDetector {
    // Core methods
    bool process_frame(const cv::Mat& frame);
    void update_config(const MotionDetectionConfig& config);

    // CUDA acceleration
    cv::cuda::GpuMat d_frame_;
    cv::cuda::GpuMat d_processed_;

    // Event callback
    MotionCallback on_motion_;

    // Performance tracking
    Statistics stats_;
};
```

**Key Features to Replicate:**
- ✅ CUDA GPU acceleration
- ✅ Configurable sensitivity/thresholds
- ✅ Event-based callbacks
- ✅ Thread-safe operation
- ✅ Performance metrics
- ✅ Frame skipping for efficiency

---

## 2. SCRFD Overview

### 2.1 What is SCRFD?

SCRFD is a state-of-the-art face detection model designed for:
- **High Accuracy**: Superior detection on various face scales
- **Real-time Performance**: Optimized for edge devices and GPUs
- **Robust Detection**: Works well with occlusions, rotations, and low resolution

### 2.2 Model Variants

| Model | Size | Speed (GPU) | mAP | Use Case |
|-------|------|-------------|-----|----------|
| SCRFD_500M | 2.5 MB | ~200 FPS | 90.3% | Edge devices, mobile |
| SCRFD_1G | 2.7 MB | ~140 FPS | 92.0% | Balanced (recommended) |
| SCRFD_2.5G | 3.2 MB | ~85 FPS | 93.8% | High accuracy |
| SCRFD_10G | 16.9 MB | ~30 FPS | 95.2% | Maximum accuracy |

**Recommendation**: Start with **SCRFD_1G** for best performance/accuracy balance.

### 2.3 Technical Requirements

**Runtime Options:**
1. **ONNX Runtime** (Recommended)
   - Cross-platform compatibility
   - TensorRT execution provider for NVIDIA GPUs
   - Easy integration with C++

2. **TensorRT** (Alternative)
   - Maximum GPU performance
   - More complex integration
   - NVIDIA-specific

**Dependencies:**
```bash
# ONNX Runtime with CUDA/TensorRT
- onnxruntime-gpu >= 1.16.0
- CUDA Toolkit 12.x
- cuDNN 8.x
- TensorRT 8.x (optional, for maximum performance)
```

---

## 3. Integration Design

### 3.1 Proposed Architecture

```
┌───────────────────────────────────────────────────────────────────┐
│ RTSP Source → Decoder → Converter → VideoRate → Tee              │
│                                                   ├→ Display       │
│                                                   └→ AppSink       │
│                                                       ↓            │
│                                              Frame Processor       │
│                                              ┌──────┴──────┐      │
│                                              ↓             ↓       │
│                                       MotionDetector  FaceDetector │
│                                              ↓             ↓       │
│                                       Motion Events  Face Events   │
└───────────────────────────────────────────────────────────────────┘
```

### 3.2 FaceDetector Class Design

```cpp
// include/face_detector.h

namespace gstreamer_worker {

struct FaceDetectionConfig {
    bool enabled = false;
    std::string model_path = "models/scrfd_1g.onnx";

    // Detection parameters
    float confidence_threshold = 0.5;
    float nms_threshold = 0.4;
    int input_size = 640;  // Model input resolution

    // Processing optimization
    int frame_skip = 3;  // Process every Nth frame
    int max_frame_width = 1280;  // Resize before detection
    int max_frame_height = 720;

    // ROI (Region of Interest)
    bool use_roi = false;
    MotionROI roi;

    // Performance
    bool use_tensorrt = false;  // Use TensorRT provider
    int max_batch_size = 1;

    // Event filtering
    float min_face_size = 0.02;  // Minimum face size (% of frame)
    int required_frames = 2;  // Consecutive frames to trigger event
    double cooldown_seconds = 2.0;
};

struct FaceDetection {
    std::string camera_id;
    double timestamp;

    // Bounding box (normalized 0-1)
    struct BoundingBox {
        float x, y, width, height;
    } bbox;

    float confidence;

    // Facial landmarks (5 points: eyes, nose, mouth corners)
    std::vector<cv::Point2f> landmarks;

    // Face quality scores
    float blur_score = 0.0;
    float brightness_score = 0.0;
};

struct FaceEvent {
    std::string camera_id;
    double timestamp;
    int num_faces;
    std::vector<FaceDetection> faces;
    cv::Mat frame_snapshot;  // Optional: Store frame for recognition
};

using FaceCallback = std::function<void(const FaceEvent& event)>;

class FaceDetector {
public:
    FaceDetector(
        const std::string& camera_id,
        const FaceDetectionConfig& config,
        FaceCallback on_face_detected = nullptr
    );

    ~FaceDetector();

    // Core methods
    bool process_frame(const cv::Mat& frame);
    void update_config(const FaceDetectionConfig& config);
    FaceDetectionConfig get_config() const;
    void reset();

    // Statistics
    struct Statistics {
        uint64_t frames_processed = 0;
        uint64_t frames_skipped = 0;
        uint64_t face_events = 0;
        uint64_t total_faces_detected = 0;
        double avg_processing_time_ms = 0.0;
        double current_fps = 0.0;
        bool using_tensorrt = false;
    };

    Statistics get_statistics() const;

private:
    // ONNX Runtime session
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::Env> env_;

    // Model preprocessing
    cv::Mat preprocess(const cv::Mat& frame);

    // Post-processing
    std::vector<FaceDetection> postprocess(
        const std::vector<Ort::Value>& outputs,
        int orig_width, int orig_height
    );

    // NMS (Non-Maximum Suppression)
    std::vector<FaceDetection> apply_nms(
        const std::vector<FaceDetection>& detections
    );

    // Event triggering
    bool should_trigger_event(const std::vector<FaceDetection>& faces);

    // Configuration and state
    std::string camera_id_;
    FaceDetectionConfig config_;
    std::mutex config_mutex_;
    FaceCallback on_face_detected_;

    // Frame tracking
    std::atomic<uint64_t> frame_counter_;
    std::deque<bool> face_history_;
    std::chrono::steady_clock::time_point last_event_time_;

    // Performance metrics
    Statistics stats_;
    std::mutex stats_mutex_;
};

} // namespace gstreamer_worker
```

### 3.3 Integration with CameraStream

```cpp
class CameraStream {
public:
    // Existing
    CameraStream(
        const CameraConfig& config,
        StateCallback on_state_changed,
        ErrorCallback on_error,
        MotionEventCallback on_motion,
        FaceEventCallback on_face  // NEW
    );

    // Face detection control
    void enable_face_detection(bool enable);
    bool is_face_detection_enabled() const;
    void update_face_config(const FaceDetectionConfig& config);

private:
    std::unique_ptr<FaceDetector> face_detector_;  // NEW

    // Frame processing callback (from appsink)
    static GstFlowReturn on_new_sample(GstAppSink* appsink, gpointer user_data);

    void process_frame(const cv::Mat& frame) {
        // Motion detection (existing)
        if (motion_detector_) {
            motion_detector_->process_frame(frame);
        }

        // Face detection (NEW)
        if (face_detector_) {
            face_detector_->process_frame(frame);
        }
    }
};
```

---

## 4. Implementation Plan

### Phase 1: Core Infrastructure (Week 1)

**Tasks:**
1. ✅ Add ONNX Runtime dependency to CMakeLists.txt
2. ✅ Create `face_detector.h` and `face_detector.cpp`
3. ✅ Implement basic SCRFD model loading
4. ✅ Test preprocessing and inference pipeline
5. ✅ Implement NMS and post-processing

**Deliverables:**
- Standalone face detector that can process OpenCV frames
- Unit tests for preprocessing/postprocessing
- Benchmark script for performance testing

### Phase 2: GStreamer Integration (Week 2)

**Tasks:**
1. ✅ Integrate FaceDetector into CameraStream
2. ✅ Add face detection configuration to CameraConfig
3. ✅ Implement face event callbacks
4. ✅ Add face detection metrics to StreamMetrics
5. ✅ Update WebSocket events for face detection

**Deliverables:**
- Face detection working in real-time pipeline
- Face events broadcast via WebSocket
- Configuration validation

### Phase 3: API & Database (Week 2)

**Tasks:**
1. ✅ Update database schema for face detection config
2. ✅ Add API endpoints for face detection control
3. ✅ Create face events table
4. ✅ Implement face event storage and retrieval
5. ✅ Add face detection to camera CRUD operations

**Deliverables:**
- Face detection fully integrated with REST API
- Face events stored in database
- API documentation updated

### Phase 4: Frontend & Testing (Week 3)

**Tasks:**
1. ✅ Add face detection toggle to camera configuration UI
2. ✅ Create face events page
3. ✅ Display face bounding boxes on live feed (optional)
4. ✅ Performance testing with multiple cameras
5. ✅ Load testing and optimization

**Deliverables:**
- Complete UI for face detection management
- Performance benchmarks
- Production deployment guide

---

## 5. Performance Considerations

### 5.1 Expected Performance

**Single Camera (1080p @ 12 FPS):**
```
Resolution: 1920x1080
Model: SCRFD_1G
Input Size: 640x640
Frame Skip: 3 (process every 3rd frame)

Expected Performance:
- Inference Time: ~8-12ms (GPU)
- Preprocessing: ~2-3ms
- Postprocessing: ~1-2ms
- Total: ~12-17ms per processed frame
- Effective FPS: ~60-80 FPS
- With frame skip (3): ~4 FPS face detection
```

**Multi-Camera Scaling:**
```
4 Cameras:
- Sequential: ~48-68ms latency
- GPU Memory: ~800MB (200MB per camera)
- CPU: ~15-20% (preprocessing)
- GPU: ~40-60% utilization

Recommendation: Process cameras sequentially to avoid GPU contention
```

### 5.2 Optimization Strategies

**1. Frame Preprocessing on GPU**
```cpp
// Use CUDA preprocessing instead of CPU
cv::cuda::GpuMat d_frame, d_resized, d_normalized;
cv::cuda::resize(d_frame, d_resized, cv::Size(640, 640));
cv::cuda::normalize(d_resized, d_normalized, ...);
```

**2. Batch Processing (Future)**
```cpp
// Process multiple frames in a batch
std::vector<cv::Mat> frames = {frame1, frame2, frame3};
std::vector<FaceEvent> results = face_detector->process_batch(frames);
```

**3. Dynamic Frame Skipping**
```cpp
// Skip more frames when no motion detected
int frame_skip = motion_detected ? 2 : 5;
```

**4. TensorRT Optimization**
```cpp
// Use TensorRT for maximum speed
FaceDetectionConfig config;
config.use_tensorrt = true;  // 2-3x faster inference
```

### 5.3 Resource Management

**GPU Memory:**
```
Per Camera Allocation:
- Model Weights: ~3MB (shared across cameras)
- Input Tensor: 640x640x3x4 = ~4.7MB
- Output Tensors: ~2MB
- Total per camera: ~10MB
- 10 cameras: ~100MB + 3MB shared = ~103MB

Available: NVIDIA T1000 has 4GB
Headroom: ~3.8GB for other operations
```

**CPU Usage:**
```
- Frame copying: ~2%
- Preprocessing: ~3-5%
- Postprocessing: ~2-3%
- Per camera: ~7-10%
- 10 cameras: ~70-100%

Mitigation: Use GPU preprocessing, reduce frame resolution
```

---

## 6. API Changes

### 6.1 Configuration Endpoints

**Add Face Detection to Camera Config:**
```http
POST /api/cameras
Content-Type: application/json

{
  "camera_id": "front_door",
  "rtsp_url": "rtsp://...",
  "face_detection": {
    "enabled": true,
    "model_path": "models/scrfd_1g.onnx",
    "confidence_threshold": 0.6,
    "frame_skip": 3,
    "use_tensorrt": false
  }
}
```

**Update Face Detection Config:**
```http
PATCH /api/cameras/front_door/face-detection
Content-Type: application/json

{
  "enabled": true,
  "confidence_threshold": 0.7,
  "frame_skip": 2
}
```

### 6.2 Face Events Endpoints

```http
# Get face events
GET /api/face-events?camera_id=front_door&limit=100&offset=0

Response:
{
  "events": [
    {
      "id": "evt_123",
      "camera_id": "front_door",
      "timestamp": "2025-11-21T13:30:00Z",
      "num_faces": 2,
      "faces": [
        {
          "bbox": {"x": 0.3, "y": 0.2, "width": 0.15, "height": 0.2},
          "confidence": 0.92,
          "landmarks": [[0.35, 0.25], [0.40, 0.25], ...]
        }
      ]
    }
  ],
  "total": 1250
}

# Get face event by ID
GET /api/face-events/evt_123

# Get face event snapshot (image)
GET /api/face-events/evt_123/snapshot
```

### 6.3 WebSocket Events

```json
{
  "event_type": "face_detected",
  "camera_id": "front_door",
  "timestamp": 1700567890.123,
  "data": {
    "num_faces": 2,
    "faces": [
      {
        "bbox": {"x": 0.3, "y": 0.2, "width": 0.15, "height": 0.2},
        "confidence": 0.92
      }
    ]
  }
}
```

---

## 7. Database Schema Updates

```sql
-- Face detection configuration (part of cameras table)
ALTER TABLE cameras ADD COLUMN face_detection_enabled BOOLEAN DEFAULT FALSE;
ALTER TABLE cameras ADD COLUMN face_detection_config JSON;

-- Face events table
CREATE TABLE face_events (
    id SERIAL PRIMARY KEY,
    event_id VARCHAR(255) UNIQUE NOT NULL,
    camera_id VARCHAR(255) NOT NULL,
    timestamp TIMESTAMP NOT NULL,
    num_faces INTEGER NOT NULL,
    faces JSON NOT NULL,  -- Array of face detections with bbox, confidence, landmarks
    snapshot_path VARCHAR(512),  -- Path to saved frame snapshot
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (camera_id) REFERENCES cameras(camera_id) ON DELETE CASCADE
);

CREATE INDEX idx_face_events_camera_id ON face_events(camera_id);
CREATE INDEX idx_face_events_timestamp ON face_events(timestamp);

-- Face detection metrics (add to camera_metrics table)
ALTER TABLE camera_metrics ADD COLUMN faces_detected INTEGER DEFAULT 0;
ALTER TABLE camera_metrics ADD COLUMN face_detection_fps REAL DEFAULT 0;
ALTER TABLE camera_metrics ADD COLUMN last_face_timestamp REAL DEFAULT 0;
```

---

## 8. Configuration Examples

### 8.1 Low-Latency Setup (Edge Devices)

```json
{
  "camera_id": "entrance",
  "face_detection": {
    "enabled": true,
    "model_path": "models/scrfd_500m.onnx",
    "confidence_threshold": 0.5,
    "nms_threshold": 0.4,
    "input_size": 320,
    "frame_skip": 5,
    "max_frame_width": 640,
    "max_frame_height": 480,
    "use_tensorrt": false,
    "min_face_size": 0.03,
    "required_frames": 1,
    "cooldown_seconds": 1.0
  }
}
```

### 8.2 High-Accuracy Setup (Server)

```json
{
  "camera_id": "security_gate",
  "face_detection": {
    "enabled": true,
    "model_path": "models/scrfd_10g.onnx",
    "confidence_threshold": 0.7,
    "nms_threshold": 0.3,
    "input_size": 640,
    "frame_skip": 2,
    "max_frame_width": 1920,
    "max_frame_height": 1080,
    "use_tensorrt": true,
    "min_face_size": 0.02,
    "required_frames": 3,
    "cooldown_seconds": 2.0,
    "use_roi": true,
    "roi": {
      "x": 200,
      "y": 100,
      "width": 1000,
      "height": 800
    }
  }
}
```

### 8.3 Balanced Setup (Recommended)

```json
{
  "camera_id": "lobby",
  "face_detection": {
    "enabled": true,
    "model_path": "models/scrfd_1g.onnx",
    "confidence_threshold": 0.6,
    "nms_threshold": 0.4,
    "input_size": 640,
    "frame_skip": 3,
    "max_frame_width": 1280,
    "max_frame_height": 720,
    "use_tensorrt": true,
    "min_face_size": 0.025,
    "required_frames": 2,
    "cooldown_seconds": 1.5
  }
}
```

---

## 9. Testing Strategy

### 9.1 Unit Tests

```cpp
// test/face_detector_test.cpp
TEST(FaceDetectorTest, ModelLoading) {
    FaceDetectionConfig config;
    config.model_path = "models/scrfd_1g.onnx";

    FaceDetector detector("test_cam", config);
    ASSERT_TRUE(detector.is_initialized());
}

TEST(FaceDetectorTest, SingleFaceDetection) {
    cv::Mat frame = cv::imread("test_data/single_face.jpg");

    FaceDetector detector("test", config);
    bool detected = detector.process_frame(frame);

    ASSERT_TRUE(detected);
    auto stats = detector.get_statistics();
    ASSERT_EQ(stats.total_faces_detected, 1);
}

TEST(FaceDetectorTest, PerformanceBenchmark) {
    FaceDetector detector("test", config);
    cv::Mat frame = cv::imread("test_data/frame.jpg");

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        detector.process_frame(frame);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double avg_ms = duration.count() / 100.0;

    ASSERT_LT(avg_ms, 20.0);  // Should be under 20ms per frame
}
```

### 9.2 Integration Tests

```bash
# Test full pipeline with face detection
./gstreamer_worker_api --config test_config_with_faces.json

# Expected:
# - Camera starts successfully
# - Face detection initializes
# - Faces detected in test video
# - Events sent via WebSocket
# - No memory leaks
```

### 9.3 Load Testing

```bash
# 10 cameras with face detection enabled
./load_test.sh --cameras 10 --duration 300 --with-faces

# Monitor:
# - GPU memory usage
# - GPU utilization
# - CPU usage
# - Latency
# - Detection accuracy
```

---

## 10. Deployment Checklist

### 10.1 Prerequisites

- [ ] ONNX Runtime 1.16+ installed
- [ ] CUDA Toolkit 12.x installed
- [ ] TensorRT 8.x installed (optional)
- [ ] SCRFD models downloaded
- [ ] Database schema updated
- [ ] Config files updated

### 10.2 Model Setup

```bash
# Download SCRFD models
mkdir -p models
cd models

# SCRFD 1G (recommended)
wget https://github.com/deepinsight/insightface/releases/download/v0.7/scrfd_1g.onnx

# Or other variants
wget https://github.com/deepinsight/insightface/releases/download/v0.7/scrfd_500m.onnx
wget https://github.com/deepinsight/insightface/releases/download/v0.7/scrfd_10g.onnx
```

### 10.3 Build Configuration

```cmake
# CMakeLists.txt additions
find_package(onnxruntime REQUIRED)

target_link_libraries(gstreamer_worker_api
    ${ONNXRUNTIME_LIBRARIES}
    # ... existing libraries
)
```

### 10.4 Runtime Verification

```bash
# Test ONNX Runtime
python3 -c "import onnxruntime; print(onnxruntime.get_available_providers())"
# Expected: ['TensorrtExecutionProvider', 'CUDAExecutionProvider', 'CPUExecutionProvider']

# Test model loading
./test_face_detector models/scrfd_1g.onnx test_image.jpg
# Expected: Detections printed with bounding boxes
```

---

## 11. Future Enhancements

### Phase 5: Face Recognition (Optional)

After face detection is stable, add face recognition:

1. **Face Embedding Extraction**
   - Use ArcFace or similar model
   - Extract 512-dim embeddings
   - Store in vector database (FAISS, Milvus)

2. **Watchlist Management**
   - Add known faces to watchlist
   - Real-time matching against detected faces
   - Alert on watchlist matches

3. **Face Tracking**
   - Track faces across frames
   - Associate detections with person IDs
   - Count unique visitors

### Phase 6: Advanced Features

1. **Face Quality Assessment**
   - Blur detection
   - Brightness/contrast analysis
   - Pose estimation
   - Only save high-quality faces

2. **Smart Recording Triggers**
   - Start recording when face detected
   - Pre-roll and post-roll buffers
   - Auto-clip generation

3. **Privacy Features**
   - Face blurring/pixelation
   - Anonymization mode
   - GDPR compliance tools

---

## 12. Risk Mitigation

### 12.1 Performance Risks

**Risk**: Face detection slows down video streaming
**Mitigation**:
- Process face detection in separate thread
- Use frame skipping
- Dynamic adjustment based on load

**Risk**: GPU memory exhaustion with many cameras
**Mitigation**:
- Implement model sharing across cameras
- Monitor GPU memory usage
- Graceful degradation (disable face detection if memory low)

### 12.2 Accuracy Risks

**Risk**: False positives in certain lighting
**Mitigation**:
- Confidence threshold tuning
- Required consecutive frames
- Brightness normalization

**Risk**: Missed detections on small/distant faces
**Mitigation**:
- Use higher resolution model
- Reduce input_size to maintain aspect ratio
- Use SCRFD_10G for critical cameras

### 12.3 Integration Risks

**Risk**: Breaking existing motion detection
**Mitigation**:
- Independent processing paths
- Extensive testing
- Feature flag for gradual rollout

**Risk**: Database performance with many events
**Mitigation**:
- Index optimization
- Event aggregation
- Auto-archival of old events

---

## 13. Success Metrics

### 13.1 Performance Metrics

- [ ] Face detection latency < 20ms per frame
- [ ] GPU memory usage < 200MB per camera
- [ ] CPU usage increase < 10% per camera
- [ ] No impact on video streaming FPS
- [ ] Accuracy > 95% on test dataset

### 13.2 Reliability Metrics

- [ ] 99.9% uptime for face detection service
- [ ] Zero memory leaks over 7-day test
- [ ] Graceful degradation under high load
- [ ] Error rate < 0.1%

### 13.3 Usability Metrics

- [ ] Face detection config in < 30 seconds
- [ ] Face events searchable and filterable
- [ ] Real-time event updates via WebSocket
- [ ] API response time < 100ms

---

## 14. Conclusion

The proposed SCRFD integration follows the proven architecture pattern established by the motion detection system. By leveraging ONNX Runtime with TensorRT, we can achieve real-time face detection with minimal performance impact.

**Next Steps:**
1. Review and approve this design document
2. Set up development environment with ONNX Runtime
3. Begin Phase 1 implementation (Core Infrastructure)
4. Conduct performance benchmarks with target hardware
5. Iterate based on test results

**Estimated Timeline**: 3-4 weeks for complete implementation
**Resource Requirements**: 1 developer, access to test cameras, NVIDIA GPU (T1000 or better)

---

**Document Version**: 1.0
**Last Updated**: 2025-11-21
**Author**: Claude (AI Assistant)
**Status**: Ready for Review
