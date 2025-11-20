# Motion Detection Integration - Full Stack Implementation

## 🎉 What's Been Implemented

### 1. **Motion Detection in Database & API**
- ✅ Added `motion_detection_enabled` and `motion_detection_config` fields to Camera model
- ✅ Database migration applied successfully
- ✅ Pydantic schemas updated with MotionDetectionConfig
- ✅ API endpoints handle motion detection configuration
- ✅ Worker client passes motion detection to C++ worker

### 2. **Worker Health Monitoring & Auto-Restart**
- ✅ Background task monitors worker health every 5 seconds
- ✅ Automatic worker restart on crash
- ✅ Automatic camera re-sync when worker reconnects
- ✅ Comprehensive logging of all worker state changes

### 3. **Comprehensive Logging**
- ✅ Timestamped logging with proper formatting
- ✅ File logging to `logs/fastapi.log`
- ✅ Request/response logging middleware
- ✅ Worker health state transitions logged
- ✅ Camera sync operations logged

## 📊 System Architecture

```
┌─────────────────────────────────────────────────────────┐
│              FastAPI (Port 8002)                        │
│  ┌─────────────────────────────────────────────────┐   │
│  │  Health Monitor (Background Task)                │   │
│  │  - Checks worker every 5s                        │   │
│  │  - Auto-restart on failure                       │   │
│  │  - Auto-sync cameras on reconnect                │   │
│  └─────────────────────────────────────────────────┘   │
│                      ↕                                   │
│  ┌─────────────────────────────────────────────────┐   │
│  │  PostgreSQL Database                             │   │
│  │  - Cameras (with motion_detection_config JSON)   │   │
│  │  - Events, Users, Profiles, etc.                 │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                        ↕
┌─────────────────────────────────────────────────────────┐
│         C++ Worker (Port 8081)                          │
│  ┌─────────────────────────────────────────────────┐   │
│  │  Motion Detection (CUDA-Accelerated)             │   │
│  │  - MOG2_CUDA algorithm                           │   │
│  │  - Real-time motion events                       │   │
│  │  - Configurable sensitivity & ROI                │   │
│  └─────────────────────────────────────────────────┘   │
│                      ↓                                   │
│  ┌─────────────────────────────────────────────────┐   │
│  │  Camera Streams (GStreamer)                      │   │
│  │  - RTSP input → NVIDIA decoder → Motion detect   │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

## 🚀 Testing the Integration

### Step 1: Start the System

```bash
./START.sh
```

You should see:
```
========================================
  GStreamer Camera Manager
  Production Startup Script
========================================

✓ PostgreSQL already running

Starting FastAPI (will auto-start C++ Worker)...

2025-11-20 14:30:00 | INFO     | __main__             | ============================================================
2025-11-20 14:30:00 | INFO     | __main__             | FastAPI Application Starting
2025-11-20 14:30:00 | INFO     | __main__             | Starting C++ worker process...
2025-11-20 14:30:01 | INFO     | __main__             | Worker process started with PID: 12345
2025-11-20 14:30:02 | INFO     | __main__             | Worker is healthy and ready
2025-11-20 14:30:02 | INFO     | __main__             | Starting worker health monitoring...
2025-11-20 14:30:02 | INFO     | __main__             | Worker health monitoring started
2025-11-20 14:30:02 | INFO     | __main__             | ============================================================
2025-11-20 14:30:02 | INFO     | __main__             | FastAPI Application Ready
2025-11-20 14:30:02 | INFO     | __main__             | ============================================================
```

### Step 2: Create a Camera with Motion Detection

```bash
curl -X POST "http://localhost:8002/api/v1/cameras/" \
  -H "Content-Type: application/json" \
  -d '{
    "camera_id": "front_door",
    "name": "Front Door Camera",
    "description": "Main entrance",
    "rtsp_url": "rtsp://192.168.0.219",
    "username": "service",
    "password": "WSS4Sec$$",
    "protocols": "tcp",
    "latency_ms": 150,
    "target_fps": 12,
    "enable_display": false,
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
      "max_frame_height": 480
    }
  }'
```

### Step 3: Start the Camera

```bash
curl -X POST "http://localhost:8002/api/v1/cameras/front_door/start"
```

### Step 4: Check Worker Status

```bash
# FastAPI API
curl http://localhost:8002/api/v1/cameras/

# Worker API (direct)
curl http://localhost:8081/api/cameras
```

## 🧪 Testing Worker Auto-Restart

### Test 1: Kill Worker Manually

```bash
# Find worker PID
ps aux | grep gstreamer_worker_api

# Kill it
kill -9 <PID>
```

**Expected behavior:**
1. Within 5 seconds, FastAPI detects worker is offline
2. Logs: `❌ Worker is OFFLINE! Attempting automatic restart...`
3. Worker restarts automatically
4. Logs: `✅ Worker restarted successfully`
5. Cameras are automatically re-synced
6. All cameras that were running are restarted

### Test 2: Check Logs

```bash
# Real-time logs
tail -f backend/logs/fastapi.log

# Search for specific events
grep "Worker" backend/logs/fastapi.log
grep "Motion" backend/logs/fastapi.log
grep "camera" backend/logs/fastapi.log
```

## 📋 Motion Detection Configuration Reference

### Algorithm Options

| Algorithm | Description | Performance | Use Case |
|-----------|-------------|-------------|----------|
| MOG2_CUDA | CUDA-accelerated Gaussian Mixture | ~200 FPS | Production (default) |
| MOG2 | CPU-based Gaussian Mixture | ~50 FPS | No CUDA available |
| KNN | K-Nearest Neighbors | ~40 FPS | Alternative to MOG2 |
| FRAME_DIFF | Simple frame differencing | ~300 FPS | Fastest, less accurate |

### Sensitivity Tuning

| Sensitivity | Motion Detection Level | False Positives |
|-------------|------------------------|-----------------|
| 0.1 - 0.3 | Large movements only | Very Low |
| 0.4 - 0.6 | Balanced (recommended) | Low |
| 0.7 - 1.0 | Subtle movements | Higher |

### Parameters Explained

```json
{
  "enabled": true,              // Enable motion detection
  "algorithm": "MOG2_CUDA",     // Algorithm to use
  "sensitivity": 0.5,           // 0.0-1.0, higher = more sensitive
  "min_contour_area": 500,      // Minimum pixels to consider as motion
  "max_contours": 50,           // Max number of motion areas to track
  "frame_skip": 2,              // Process every Nth frame (saves CPU)
  "blur_size": 21,              // Gaussian blur size (must be odd)
  "history": 500,               // Background model frames
  "var_threshold": 16.0,        // Detection threshold
  "detect_shadows": false,      // Slower but more accurate
  "cooldown_seconds": 1.0,      // Min time between events
  "required_frames": 3,         // Consecutive frames needed
  "max_frame_width": 640,       // Resize for analysis (0 = no resize)
  "max_frame_height": 480,      // Resize for analysis (0 = no resize)
  "roi": {                      // Optional region of interest
    "x": 100,
    "y": 100,
    "width": 800,
    "height": 600
  }
}
```

## 📊 API Endpoints

### Camera Management

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/cameras/` | List all cameras |
| POST | `/api/v1/cameras/` | Create new camera (with motion config) |
| GET | `/api/v1/cameras/{id}` | Get camera details |
| PATCH | `/api/v1/cameras/{id}` | Update camera (incl. motion config) |
| DELETE | `/api/v1/cameras/{id}` | Delete camera |
| POST | `/api/v1/cameras/{id}/start` | Start camera stream |
| POST | `/api/v1/cameras/{id}/stop` | Stop camera stream |
| GET | `/api/v1/cameras/{id}/status` | Get real-time status |

### System Management

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/health` | FastAPI health check |
| POST | `/api/v1/sync` | Manual camera sync to worker |
| POST | `/api/v1/worker/ready` | Worker callback (automatic) |

### Worker API (Direct)

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `http://localhost:8081/health` | Worker health check |
| GET | `http://localhost:8081/api/cameras` | List worker cameras |
| GET | `http://localhost:8081/api/cameras/{id}/status` | Camera status |

## 🔍 Monitoring & Debugging

### View FastAPI Logs

```bash
# Real-time
tail -f backend/logs/fastapi.log

# Filter by level
grep "ERROR" backend/logs/fastapi.log
grep "WARNING" backend/logs/fastapi.log

# Watch for worker events
tail -f backend/logs/fastapi.log | grep -E "Worker|worker"
```

### Check Worker Health

```bash
# Health check
curl http://localhost:8081/health

# Should return:
# {"status":"healthy","cameras":1}
```

### Verify Motion Detection

```bash
# Get camera status
curl http://localhost:8081/api/cameras/front_door/status

# Should show motion detection metrics:
# {
#   "camera_id": "front_door",
#   "state": "RUNNING",
#   "is_running": true,
#   "metrics": {
#     "frames_analyzed": 1234,
#     "motion_events_detected": 56,
#     "motion_detection_fps": 15.2,
#     ...
#   }
# }
```

## 🐛 Troubleshooting

### Issue: Worker not starting

**Solution:**
```bash
# Check worker executable
ls -la gstreamer_worker/build/gstreamer_worker_api

# Check logs
tail -f backend/logs/fastapi.log | grep "worker"

# Check environment
cat backend/.env | grep WORKER
```

### Issue: Motion detection not working

**Solution:**
```bash
# 1. Verify CUDA is available
python3 -c "import cv2; print('CUDA:', cv2.cuda.getCudaEnabledDeviceCount())"

# 2. Check camera config in database
curl http://localhost:8002/api/v1/cameras/front_door

# 3. Check worker received config
curl http://localhost:8081/api/cameras/front_door/status

# 4. Check worker logs
# (Worker logs show motion events when detected)
```

### Issue: Cameras not syncing on worker restart

**Solution:**
```bash
# Manual sync trigger
curl -X POST http://localhost:8002/api/v1/sync

# Check logs
grep "sync" backend/logs/fastapi.log
```

## 🎯 Next Steps

1. **Frontend Integration**
   - React components for camera management
   - Motion detection configuration UI
   - Real-time motion event display

2. **Event Processing**
   - Store motion events in database
   - Webhook notifications
   - Face detection triggers

3. **Advanced Features**
   - Multiple ROIs per camera
   - Motion heat maps
   - Object classification

## 📝 Notes

- Worker health monitoring runs every 5 seconds
- Automatic restart has 3-second stabilization delay
- Motion events include bounding boxes and confidence scores
- All camera state is persisted in PostgreSQL
- Worker can be manually restarted without losing database state
