# C++ Worker REST API Server Guide

## 🎯 What This Does

Your C++ GStreamer worker now has a **REST API server** that allows FastAPI (or any client) to:
- Add/remove cameras dynamically
- Start/stop camera streams
- Get real-time status
- Control everything via HTTP

## 🚀 Build the API Server

```bash
cd ~/Desktop/development/gstreamer/optimized_pipeline/gstreamer_worker

# Clean previous build (optional)
rm -rf build

# Build with API server
./build.sh

# Or manually:
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

You should now have: `build/gstreamer_worker_api`

## ▶️ Start the API Server

```bash
cd ~/Desktop/development/gstreamer/optimized_pipeline/gstreamer_worker/build

# Start API server (listens on port 8081)
./gstreamer_worker_api --port 8081
```

Output:
```
========================================
  GStreamer Worker - API Mode
========================================
API Server: http://0.0.0.0:8081
Log Level: INFO
========================================

✅ API Server ready at http://0.0.0.0:8081
   Health check: http://0.0.0.0:8081/health
   List cameras: http://0.0.0.0:8081/api/cameras

Press Ctrl+C to stop
```

## 🧪 Test the API

### 1. Health Check

```bash
curl http://localhost:8081/health
```

Response:
```json
{
  "status": "healthy",
  "running": false,
  "cameras": 0
}
```

### 2. Add Your First Camera

```bash
curl -X POST http://localhost:8081/api/cameras \
  -H 'Content-Type: application/json' \
  -d '{
    "camera_id": "front_door",
    "rtsp_url": "rtsp://service:WSS4Sec$$@192.168.0.219",
    "target_fps": 12,
    "latency_ms": 150,
    "enable_display": true
  }'
```

Response:
```json
{
  "success": true,
  "camera_id": "front_door",
  "message": "Camera added successfully"
}
```

### 3. Start the Camera

```bash
curl -X POST http://localhost:8081/api/cameras/front_door/start
```

Response:
```json
{
  "success": true,
  "camera_id": "front_door",
  "state": "STARTING",
  "message": "Camera starting"
}
```

**🎥 You should now see the live video window!**

### 4. Check Camera Status

```bash
curl http://localhost:8081/api/cameras/front_door/status
```

Response:
```json
{
  "camera_id": "front_door",
  "state": "RUNNING",
  "is_running": true,
  "metrics": {
    "frames_displayed": 150,
    "errors_count": 0,
    "reconnections": 0,
    "uptime_seconds": 12.5,
    "last_error": ""
  }
}
```

### 5. List All Cameras

```bash
curl http://localhost:8081/api/cameras
```

Response:
```json
[
  {
    "camera_id": "front_door",
    "state": "RUNNING",
    "is_running": true,
    "metrics": {
      "frames_displayed": 150,
      "errors_count": 0,
      "reconnections": 0,
      "uptime_seconds": 12.5
    }
  }
]
```

### 6. Stop Camera

```bash
curl -X POST http://localhost:8081/api/cameras/front_door/stop
```

### 7. Remove Camera

```bash
curl -X DELETE http://localhost:8081/api/cameras/front_door
```

## 📋 Complete API Reference

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/health` | Health check |
| GET | `/api/cameras` | List all cameras |
| POST | `/api/cameras` | Add new camera |
| GET | `/api/cameras/:id/status` | Get camera status |
| POST | `/api/cameras/:id/start` | Start camera stream |
| POST | `/api/cameras/:id/stop` | Stop camera stream |
| DELETE | `/api/cameras/:id` | Remove camera |
| GET | `/api/system/status` | System status |

## 🎯 Add Multiple Cameras

```bash
# Add camera 1
curl -X POST http://localhost:8081/api/cameras \
  -H 'Content-Type: application/json' \
  -d '{
    "camera_id": "camera_1",
    "rtsp_url": "rtsp://service:WSS4Sec$$@192.168.0.219",
    "target_fps": 12
  }'

# Add camera 2
curl -X POST http://localhost:8081/api/cameras \
  -H 'Content-Type: application/json' \
  -d '{
    "camera_id": "camera_2",
    "rtsp_url": "rtsp://service:WSS4Sec$$@192.168.0.83",
    "target_fps": 12
  }'

# Start both
curl -X POST http://localhost:8081/api/cameras/camera_1/start
curl -X POST http://localhost:8081/api/cameras/camera_2/start
```

**🎥 Both video windows should appear!**

## 🐛 Troubleshooting

### Build Errors

**Error: cpp-httplib not found**
```bash
# It should auto-download. If not, install manually:
sudo apt-get install libssl-dev
```

**Error: nlohmann/json not found**
```bash
# It should auto-download via CMake FetchContent
```

### Runtime Errors

**Error: Address already in use**
```bash
# Another process is using port 8081
# Use different port:
./gstreamer_worker_api --port 8082
```

**Error: Camera won't start**
```bash
# Check RTSP URL with VLC first
vlc rtsp://service:WSS4Sec$$@192.168.0.219

# Check GStreamer
gst-launch-1.0 rtspsrc location="rtsp://..." ! fakesink
```

## ✅ Success Checklist

- [ ] `./gstreamer_worker_api` starts without errors
- [ ] Health check returns 200 OK
- [ ] Can add camera via POST
- [ ] Can start camera and see video window
- [ ] Camera status shows RUNNING
- [ ] Can stop/remove camera

## 🎉 Next Steps

Once your C++ API server is working, we'll:
1. ✅ Create FastAPI backend that talks to this API
2. ✅ Add database persistence
3. ✅ Create React frontend
4. ✅ Full integration

**Your C++ worker is now API-ready! Test it before proceeding to FastAPI.**
