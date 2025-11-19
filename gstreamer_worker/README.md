# GStreamer Worker - Production-Ready RTSP Stream Manager (C++)

A robust, enterprise-grade C++ application for managing multiple RTSP camera streams using GStreamer with NVIDIA hardware acceleration. Built for production environments with comprehensive error handling, automatic reconnection, and health monitoring.

## Features

### Core Capabilities
- ✅ **Multi-camera support** - Manage unlimited concurrent RTSP streams
- ✅ **NVIDIA hardware acceleration** - nvv4l2decoder, nvvideoconvert for optimal performance
- ✅ **Live video display** - Real-time video display with minimal latency
- ✅ **Thread-safe operations** - Safe concurrent access to all operations
- ✅ **Modern C++17** - RAII, smart pointers, move semantics

### Production Features
- ✅ **Automatic reconnection** - Exponential backoff with configurable retry limits
- ✅ **State management** - Comprehensive state tracking (stopped, starting, running, error, reconnecting)
- ✅ **Error handling** - Robust error detection and recovery
- ✅ **Health monitoring** - Periodic health checks
- ✅ **Metrics & statistics** - Error counts, reconnections, uptime tracking
- ✅ **Graceful shutdown** - Clean resource cleanup and thread management
- ✅ **Logging** - Thread-safe logging with configurable levels
- ✅ **Event callbacks** - React to state changes and errors

## Architecture

```
PipelineManager
    ├── CameraStream 1
    │   └── GStreamer Pipeline
    ├── CameraStream 2
    │   └── GStreamer Pipeline
    └── CameraStream N
        └── GStreamer Pipeline
```

### Pipeline Flow

```
rtspsrc → rtph264depay → h264parse → nvv4l2decoder →
nvvideoconvert → videorate → video/x-raw(memory:NVMM) → nveglglessink
```

## Installation

### System Requirements

- Ubuntu 20.04+ (or compatible Linux distribution)
- NVIDIA GPU with driver >= 470
- GStreamer 1.14+
- CMake 3.16+
- C++17 compatible compiler (GCC 7+, Clang 5+)

### Install Dependencies

```bash
# Build tools
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git

# GStreamer
sudo apt-get install -y \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-tools \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav \
    gstreamer1.0-rtsp

# NVIDIA GStreamer plugins (if not already installed)
sudo apt-get install -y \
    gstreamer1.0-plugins-nvvideo4linux2 \
    gstreamer1.0-plugins-nvvideoconvert

# GLib development files
sudo apt-get install -y libglib2.0-dev

# Display support
sudo apt-get install -y \
    gstreamer1.0-x \
    gstreamer1.0-gl
```

### Build

```bash
# Create build directory
mkdir -p build
cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

# Optional: Install
sudo make install
```

#### Build Types

- **Release** (default): Optimized for performance (-O3)
  ```bash
  cmake .. -DCMAKE_BUILD_TYPE=Release
  ```

- **Debug**: Debug symbols, no optimization (-g -O0)
  ```bash
  cmake .. -DCMAKE_BUILD_TYPE=Debug
  ```

### Verify Installation

```bash
# Check GStreamer
gst-inspect-1.0 --version
gst-inspect-1.0 nvv4l2decoder

# Test the application
./gstreamer_worker --help
```

## Usage

### Basic Usage

```bash
./gstreamer_worker \
    --camera-id front_door \
    --rtsp-url rtsp://192.168.0.219/stream \
    --username service \
    --password 'WSS4Sec$$' \
    --fps 12 \
    --latency 150
```

### Command Line Options

```
Options:
  -h, --help              Show help message
  --camera-id ID          Camera identifier (default: camera_1)
  --rtsp-url URL          RTSP URL (required)
  --username USER         RTSP username (optional)
  --password PASS         RTSP password (optional)
  --fps FPS               Target FPS (default: 12)
  --latency MS            Latency in milliseconds (default: 150)
  --no-nvidia             Disable NVIDIA hardware acceleration
  --no-display            Disable display (for testing)
  --log-level LEVEL       Log level: DEBUG, INFO, WARNING, ERROR (default: INFO)
```

### Examples

#### Single Camera with NVIDIA Acceleration

```bash
./gstreamer_worker \
    --camera-id office_cam \
    --rtsp-url rtsp://192.168.0.100/stream \
    --username admin \
    --password admin123 \
    --fps 12
```

#### Without Display (Background Processing)

```bash
./gstreamer_worker \
    --camera-id parking_lot \
    --rtsp-url rtsp://192.168.0.200/stream \
    --no-display
```

#### Debug Mode

```bash
./gstreamer_worker \
    --camera-id debug_cam \
    --rtsp-url rtsp://192.168.0.50/stream \
    --log-level DEBUG
```

#### Software Decoding (No NVIDIA GPU)

```bash
./gstreamer_worker \
    --camera-id test_cam \
    --rtsp-url rtsp://192.168.0.75/stream \
    --no-nvidia
```

## Configuration

### Camera Configuration

The `CameraConfig` struct can be customized programmatically:

```cpp
CameraConfig config;
config.camera_id = "front_door";
config.rtsp_url = "rtsp://192.168.0.100/stream";
config.username = "admin";
config.password = "password";

// RTSP settings
config.protocols = "tcp";           // tcp, udp, or tcp+udp
config.latency_ms = 150;            // RTSP latency buffer
config.drop_on_latency = true;      // Drop frames if latency exceeded
config.target_fps = 12;             // Target framerate

// Display
config.enable_display = true;       // Show live video window
config.display_sync = false;        // Sync to clock (false = lower latency)

// Reconnection
config.auto_reconnect = true;       // Enable automatic reconnection
config.max_reconnect_attempts = -1; // -1 = infinite
config.reconnect_initial_delay = 1.0;      // Initial delay (seconds)
config.reconnect_max_delay = 60.0;         // Max delay (seconds)
config.reconnect_backoff_multiplier = 2.0; // Exponential backoff

// Health monitoring
config.health_check_interval = 5.0;        // Health check interval (seconds)
config.max_consecutive_errors = 5;         // Max errors before stopping

// Hardware acceleration
config.use_nvidia_decoder = true;          // Use nvv4l2decoder
```

### Pipeline Manager Configuration

```cpp
PipelineManagerConfig config;
config.log_level = "INFO";          // DEBUG, INFO, WARNING, ERROR
config.enable_metrics = true;       // Enable periodic metrics logging
config.metrics_interval = 30.0;     // Metrics log interval (seconds)
```

## API Reference

### CameraStream Class

```cpp
class CameraStream {
public:
    CameraStream(
        const CameraConfig& config,
        StateCallback on_state_changed = nullptr,
        ErrorCallback on_error = nullptr
    );

    bool start();
    void stop(bool wait = true);
    StreamState get_state() const;
    bool is_running() const;
    StreamMetrics get_metrics() const;
    std::string get_camera_id() const;
};
```

### PipelineManager Class

```cpp
class PipelineManager {
public:
    PipelineManager(
        const PipelineManagerConfig& config = PipelineManagerConfig(),
        StateCallback on_state_changed = nullptr,
        ErrorCallback on_error = nullptr
    );

    bool add_camera(const CameraConfig& config);
    std::map<std::string, bool> add_cameras(const std::vector<CameraConfig>& configs);
    bool remove_camera(const std::string& camera_id, bool wait = true);

    std::map<std::string, bool> start_all();
    void stop_all(bool wait = true);

    CameraStream* get_camera(const std::string& camera_id);
    StreamState get_camera_state(const std::string& camera_id) const;
    std::map<std::string, StreamState> get_all_states() const;

    StreamMetrics get_camera_metrics(const std::string& camera_id) const;
    std::map<std::string, StreamMetrics> get_all_metrics() const;

    bool is_running() const;
    size_t get_camera_count() const;
    size_t get_running_camera_count() const;
};
```

### StreamState Enum

```cpp
enum class StreamState {
    STOPPED,
    STARTING,
    RUNNING,
    ERROR,
    RECONNECTING,
    STOPPING
};
```

### StreamMetrics Struct

```cpp
struct StreamMetrics {
    uint64_t frames_displayed;
    uint64_t errors_count;
    uint64_t reconnections;
    double uptime_seconds;
    std::string last_error;
};
```

## Extending the Application

### Adding Multiple Cameras

```cpp
#include "pipeline_manager.h"

int main() {
    PipelineManager manager;

    // Camera 1
    CameraConfig cam1;
    cam1.camera_id = "front_door";
    cam1.rtsp_url = "rtsp://192.168.0.100/stream";
    manager.add_camera(cam1);

    // Camera 2
    CameraConfig cam2;
    cam2.camera_id = "back_door";
    cam2.rtsp_url = "rtsp://192.168.0.101/stream";
    manager.add_camera(cam2);

    // Start all
    manager.start_all();

    // Wait...
    std::this_thread::sleep_for(std::chrono::hours(1));

    // Stop all
    manager.stop_all();

    return 0;
}
```

### Custom Callbacks

```cpp
void on_state_changed(const std::string& camera_id, StreamState state) {
    std::cout << "Camera " << camera_id
              << " changed to " << stream_state_to_string(state) << std::endl;

    // Send notification, update database, etc.
}

void on_error(const std::string& camera_id, const std::string& error) {
    std::cerr << "ERROR on camera " << camera_id << ": " << error << std::endl;

    // Send alert, log to monitoring system, etc.
}

int main() {
    PipelineManager manager(
        PipelineManagerConfig(),
        on_state_changed,
        on_error
    );

    // Add cameras and start...
}
```

## Troubleshooting

### Build Errors

**Error: `gst/gst.h` not found**
```bash
sudo apt-get install libgstreamer1.0-dev
```

**Error: CMake version too old**
```bash
# Install newer CMake from Kitware APT repository
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ focal main'
sudo apt-get update
sudo apt-get install cmake
```

### Runtime Errors

**Pipeline fails to start**
1. Verify RTSP URL: `gst-launch-1.0 rtspsrc location="rtsp://..." ! fakesink`
2. Check NVIDIA plugins: `gst-inspect-1.0 nvv4l2decoder`
3. Try with `--no-nvidia` flag to use software decoding

**No video display**
1. Check X11 display: `echo $DISPLAY`
2. Install display plugins: `sudo apt-get install gstreamer1.0-x gstreamer1.0-gl`
3. Test with: `gst-launch-1.0 videotestsrc ! autovideosink`

**Frequent reconnections**
1. Increase latency: `--latency 300`
2. Check network stability
3. Verify camera is not overloaded
4. Enable TCP: `--protocols tcp` (should be default)

### Performance Issues

**High CPU usage**
- Enable NVIDIA acceleration (remove `--no-nvidia`)
- Reduce FPS: `--fps 5`
- Check GPU usage: `nvidia-smi`

**High memory usage**
- Monitor with: `top -p $(pgrep gstreamer_worker)`
- Reduce number of concurrent cameras
- Check for memory leaks in extended runs

## Best Practices

1. **Always use NVIDIA hardware acceleration** when available (5-10x faster)
2. **Set appropriate latency** based on network conditions (100-300ms)
3. **Enable TCP protocol** for reliable streaming
4. **Implement callbacks** for production monitoring
5. **Use graceful shutdown** (Ctrl+C) to ensure clean resource cleanup
6. **Monitor metrics** regularly to detect issues early
7. **Set `max_reconnect_attempts`** to prevent infinite retry loops in development

## Performance Tuning

### For Maximum Reliability
```cpp
config.latency_ms = 200;
config.drop_on_latency = false;
config.reconnect_max_delay = 120.0;
```

### For Low Latency
```cpp
config.latency_ms = 50;
config.drop_on_latency = true;
config.display_sync = false;
```

### For Resource-Constrained Systems
```cpp
config.target_fps = 5;
config.use_nvidia_decoder = false;  // If no GPU
```

## Future Enhancements

The codebase is designed to easily support:
- Frame extraction via appsink for ML/AI processing
- JPEG encoding for CompreFace integration
- REST API for remote control
- Multi-threaded frame processing
- Recording to disk
- Motion detection
- Custom GStreamer elements

## License

This project is licensed under the MIT License.

## Contributing

Contributions are welcome! Please ensure code follows the existing style and includes appropriate error handling.

## Support

For issues and questions:
- Check logs with `--log-level DEBUG`
- Verify GStreamer installation with `gst-inspect-1.0`
- Test RTSP URL independently with `gst-launch-1.0`
