# Motion Detection Feature

## Overview

The GStreamer Worker now includes a robust, production-ready motion detection system with CUDA acceleration support. This feature enables real-time motion detection on video streams with minimal performance impact.

## Features

- **CUDA Acceleration**: Automatically uses CUDA when available for maximum performance
- **Multiple Algorithms**:
  - MOG2_CUDA (default) - CUDA-accelerated Gaussian Mixture Model
  - MOG2 - CPU-based Gaussian Mixture Model
  - KNN - K-Nearest Neighbors background subtraction
  - FRAME_DIFF - Simple frame differencing
- **Configurable Sensitivity**: Adjust detection thresholds, contour sizes, and frame skipping
- **Region of Interest (ROI)**: Focus detection on specific areas
- **Event Debouncing**: Prevent false positives with configurable cooldown periods
- **Performance Optimized**: Frame resizing, skipping, and efficient processing
- **Thread-Safe**: Designed for concurrent operation with multiple camera streams

## Architecture

### Components

1. **MotionDetector** ([motion_detector.h](include/motion_detector.h), [motion_detector.cpp](src/motion_detector.cpp))
   - Core motion detection logic
   - CUDA-accelerated background subtraction
   - Contour analysis and event generation

2. **CameraStream Integration** ([camera_stream.h](include/camera_stream.h), [camera_stream.cpp](src/camera_stream.cpp))
   - GStreamer pipeline with appsink for frame capture
   - Frame processing and motion detector integration
   - Metrics tracking

3. **Configuration** ([config.h](include/config.h), [types.h](include/types.h))
   - MotionDetectionConfig structure
   - MotionEvent structure
   - MotionAlgorithm enum

## Configuration

### JSON Configuration Example

```json
{
  "camera_id": "front_door",
  "rtsp_url": "rtsp://192.168.0.219/stream1",
  "use_nvidia_decoder": true,
  "motion_detection": {
    "enabled": true,
    "algorithm": "MOG2_CUDA",
    "sensitivity": 0.5,
    "min_contour_area": 500,
    "max_contours": 50,
    "frame_skip": 2,
    "blur_size": 21,
    "history": 500,
    "var_threshold": 16.0,
    "detect_shadows": false,
    "cooldown_seconds": 1.0,
    "required_frames": 3,
    "max_frame_width": 640,
    "max_frame_height": 480,
    "roi": {
      "x": 100,
      "y": 100,
      "width": 800,
      "height": 600
    }
  }
}
```

### Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `enabled` | bool | false | Enable/disable motion detection |
| `algorithm` | string | "MOG2_CUDA" | Algorithm: MOG2_CUDA, MOG2, KNN, FRAME_DIFF |
| `sensitivity` | double | 0.5 | Sensitivity (0.0-1.0), higher = more sensitive |
| `min_contour_area` | int | 500 | Minimum area in pixels to consider as motion |
| `max_contours` | int | 50 | Maximum number of contours to process |
| `frame_skip` | int | 2 | Process every Nth frame |
| `blur_size` | int | 21 | Gaussian blur kernel size (must be odd) |
| `history` | int | 500 | Number of frames for background model |
| `var_threshold` | double | 16.0 | Detection threshold for background subtraction |
| `detect_shadows` | bool | false | Detect and mark shadows (slower) |
| `cooldown_seconds` | double | 1.0 | Minimum time between motion events |
| `required_frames` | int | 3 | Consecutive frames with motion to trigger event |
| `max_frame_width` | int | 640 | Resize frame width for analysis (0 = no resize) |
| `max_frame_height` | int | 480 | Resize frame height for analysis (0 = no resize) |
| `roi` | object | null | Region of Interest (optional) |

### Algorithm Selection Guide

- **MOG2_CUDA**: Best performance with CUDA GPU, recommended for production
- **MOG2**: Good accuracy, CPU-based, works without CUDA
- **KNN**: Alternative to MOG2, may work better in certain scenarios
- **FRAME_DIFF**: Fastest, simplest, but less accurate

### Sensitivity Tuning

- **Low (0.1-0.3)**: Only detect large, obvious movements
- **Medium (0.4-0.6)**: Balanced detection (recommended)
- **High (0.7-1.0)**: Detect subtle movements (may have false positives)

## Motion Events

### MotionEvent Structure

```cpp
struct MotionEvent {
    std::string camera_id;      // Camera identifier
    double timestamp;           // Unix timestamp
    int motion_area;           // Total motion area in pixels
    int num_contours;          // Number of motion contours detected
    MotionROI bounding_box;    // Bounding box of motion
    double confidence;         // Confidence score (0.0-1.0)
};
```

### Event Callback

Motion events are delivered via callback:

```cpp
CameraStream stream(
    config,
    nullptr,  // state callback
    nullptr,  // error callback
    [](const MotionEvent& event) {
        std::cout << "Motion detected on " << event.camera_id
                  << " at " << event.timestamp
                  << " (area: " << event.motion_area << ")" << std::endl;
    }
);
```

## Metrics

Motion detection adds the following metrics to StreamMetrics:

- `motion_events_detected`: Total number of motion events
- `frames_analyzed`: Total frames analyzed for motion
- `last_motion_timestamp`: Timestamp of last motion event
- `motion_detection_fps`: Current motion detection processing FPS

## Building

### Prerequisites

1. **OpenCV 4.x with CUDA support**
   ```bash
   pkg-config --modversion opencv4
   # Should show 4.x.x

   python3 -c "import cv2; print('CUDA:', cv2.cuda.getCudaEnabledDeviceCount())"
   # Should show CUDA device count > 0
   ```

2. **CUDA Toolkit 12.x**
   ```bash
   export CUDA_HOME=/usr/local/cuda-12.6
   export PATH=/usr/local/cuda-12.6/bin:$PATH
   export LD_LIBRARY_PATH=/usr/local/cuda-12.6/lib64:$LD_LIBRARY_PATH
   ```

3. **GStreamer 1.14+ with app plugin**
   ```bash
   pkg-config --modversion gstreamer-app-1.0
   ```

### Build Steps

```bash
cd gstreamer_worker
mkdir -p build && cd build

# Configure with CUDA support
cmake .. -DCMAKE_BUILD_TYPE=Release -DCUDA_TOOLKIT_ROOT_DIR=/usr/local/cuda-12.6

# Build
make -j$(nproc)

# The executables will be in the build directory:
# - gstreamer_worker (single camera)
# - gstreamer_worker_multi (multi-camera with config file)
# - gstreamer_worker_api (with REST API)
```

## Usage

### Command Line

```bash
# Run with configuration file
./gstreamer_worker_multi ../config/cameras.json

# Run API server
./gstreamer_worker_api ../config/cameras.json
```

### Runtime Control

Motion detection can be controlled at runtime:

```cpp
// Enable/disable
stream.enable_motion_detection(true);

// Check status
bool enabled = stream.is_motion_detection_enabled();

// Update configuration
MotionDetectionConfig new_config;
new_config.sensitivity = 0.7;
stream.update_motion_config(new_config);
```

## Performance Considerations

### Frame Processing Performance

With CUDA acceleration:
- **MOG2_CUDA**: ~100-300 FPS (640x480)
- **CPU MOG2**: ~30-60 FPS (640x480)

Actual performance depends on:
- GPU model (NVIDIA T1000, RTX series, etc.)
- Frame resolution
- `frame_skip` setting
- Number of concurrent streams

### Resource Usage

- **GPU Memory**: ~100-200 MB per stream (CUDA mode)
- **CPU Usage**: ~5-10% per stream (CUDA mode), ~30-50% (CPU mode)
- **Bandwidth**: Minimal (frames processed in-place)

### Optimization Tips

1. **Use frame skipping**: Set `frame_skip` to 2 or 3 for 12 FPS cameras
2. **Resize frames**: Set `max_frame_width` to 640 for HD streams
3. **Use ROI**: Focus on relevant areas only
4. **Adjust cooldown**: Increase `cooldown_seconds` to reduce events
5. **CUDA mode**: Always use MOG2_CUDA when GPU is available

## Pipeline Architecture

The motion detection integrates with GStreamer using a tee element:

```
RTSP Source → Decoder → Converter → VideoRate → Tee
                                                  ├→ Display/Output
                                                  └→ Queue → VideoConvert → AppSink (BGR)
                                                                              ↓
                                                                        MotionDetector
```

## Troubleshooting

### Motion Detection Not Working

1. **Check CUDA availability**:
   ```bash
   python3 -c "import cv2; print(cv2.cuda.getCudaEnabledDeviceCount())"
   ```

2. **Check log output**: Look for "Motion detection initialized" message

3. **Verify configuration**: Ensure `enabled: true` in config

4. **Check appsink**: Look for "Motion detection appsink configured" message

### False Positives

- Increase `min_contour_area`
- Decrease `sensitivity`
- Enable `detect_shadows`
- Increase `required_frames`
- Use ROI to exclude problematic areas

### Missing Motion Events

- Increase `sensitivity`
- Decrease `min_contour_area`
- Decrease `cooldown_seconds`
- Check `frame_skip` (lower = more frequent analysis)

### Performance Issues

- Increase `frame_skip`
- Reduce `max_frame_width` and `max_frame_height`
- Use ROI to limit detection area
- Switch to FRAME_DIFF algorithm for faster processing

## Future Enhancements

Potential future improvements:

1. **Object Classification**: Distinguish between people, vehicles, animals
2. **Zone-Based Detection**: Multiple ROIs with different sensitivities
3. **Motion Tracking**: Track objects across frames
4. **Recording Triggers**: Automatic recording when motion detected
5. **Notification Integration**: Webhooks, email, SMS alerts
6. **Heat Maps**: Visualize motion patterns over time
7. **Face Detection Integration**: Trigger face detection on motion events

## License

This motion detection feature is part of the GStreamer Worker project.

## Support

For issues or questions, please create an issue on the GitHub repository.
