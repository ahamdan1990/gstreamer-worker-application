# GStreamer Worker Fixes Applied

## Issues Fixed

### 1. ✅ Infinite Reconnection Loop for Unreachable Cameras

**Problem:**
- Cameras with wrong/unreachable IPs kept reconnecting indefinitely
- Default `max_reconnect_attempts` was `-1` (infinite retries)
- No graceful handling for permanently unavailable cameras

**Solution:**
- Changed default `max_reconnect_attempts` from `-1` to `15` in [config.h](include/config.h:31)
- Increased `reconnect_initial_delay` from `1.0` to `2.0` seconds for less aggressive retries
- Added ERROR state transition when max attempts reached
- Cameras now gracefully enter ERROR state after 15 failed reconnection attempts or 5 consecutive errors

**Files Modified:**
- `include/config.h` - Updated default reconnection settings
- `src/camera_stream.cpp` - Added max attempts check and ERROR state handling

---

### 2. ✅ Crash When Restarting Stopped Cameras

**Problem:**
- Starting a camera that was previously stopped caused crash: `terminate called without an active exception`
- Pipeline thread wasn't properly cleaned up before creating new thread
- GMainLoop resources were being reused without proper cleanup

**Solution:**
- Added proper thread cleanup in `start()` method before creating new thread
- Join and reset existing pipeline thread if present
- Reset reconnection counters when starting
- Added defensive check to create GMainLoop only if it doesn't exist
- Cameras with ERROR state can now be restarted

**Files Modified:**
- `src/camera_stream.cpp` - Enhanced `start()` method with proper cleanup (lines 38-69)

---

### 3. ✅ Segmentation Fault During Reconnection

**Problem:**
- Reconnection logic used separate detached threads
- Multiple threads tried to access GStreamer objects simultaneously
- `cleanup_pipeline()` called from reconnection thread while main thread still using resources
- Critical GLib errors: "instance with invalid (NULL) class pointer"

**Solution:**
- **Complete redesign of reconnection architecture:**
  - Removed `schedule_reconnect()` and `attempt_reconnect()` methods
  - All pipeline operations now happen in single main thread (`run_pipeline`)
  - Main thread has retry loop that handles both initial failures and runtime errors
  - Proper cleanup order: quit GLib loop → cleanup pipeline → wait → retry
- Thread-safe cleanup with correct resource release order
- No more detached threads managing pipeline lifecycle

**Files Modified:**
- `src/camera_stream.cpp` - Redesigned `run_pipeline()` with retry loop
- `src/camera_stream.cpp` - Simplified `handle_error()` to signal main thread
- `src/camera_stream.cpp` - Improved `cleanup_pipeline()` with proper ordering
- `include/camera_stream.h` - Removed unused method declarations

---

### 4. ✅ Crash on Application Shutdown

**Problem:**
- When shutting down with cameras in ERROR state, application crashed
- `std::thread` destructor called `std::terminate()` if thread not joined
- `stop()` method returned early for ERROR state without joining thread

**Solution:**
- Modified `stop()` to always join threads even for cameras in ERROR state
- Proper thread cleanup during destruction regardless of camera state
- Graceful shutdown now works with any camera state (RUNNING, ERROR, STOPPED, etc.)

**Files Modified:**
- `src/camera_stream.cpp` - Enhanced `stop()` method (lines 71-116)

---

## Test Results

### Before Fixes:
❌ Camera with unreachable IP: Infinite reconnection loop
❌ Restart camera after stop: `Aborted (core dumped)`
❌ Shutdown with ERROR camera: `terminate called without an active exception`

### After Fixes:
✅ Camera with unreachable IP: 5 attempts → ERROR state (graceful)
✅ Restart camera after stop: Works perfectly, multiple times
✅ Shutdown with ERROR camera: Clean shutdown with `Ctrl+C`
✅ Multiple cameras: Independent state management
✅ API control: All endpoints working correctly

---

## Configuration Changes

### Default Settings (can be overridden via config file or API):

```cpp
// Reconnection settings
max_reconnect_attempts = 15;        // Was: -1 (infinite)
max_consecutive_errors = 5;         // Unchanged
reconnect_initial_delay = 2.0;      // Was: 1.0 seconds
reconnect_max_delay = 60.0;         // Unchanged
reconnect_backoff_multiplier = 2.0; // Unchanged
```

---

## Next Steps

The C++ worker is now production-ready with robust error handling. You can proceed with:

1. **FastAPI Integration**
   - Create Python client to communicate with C++ worker API
   - Implement camera management routes in FastAPI
   - Store camera configurations in PostgreSQL

2. **Database Setup**
   - Install PostgreSQL
   - Run Alembic migrations
   - Persist camera state across restarts

3. **React Frontend**
   - Build camera management dashboard
   - Real-time status updates via WebSocket
   - Live video streaming

---

## API Usage Example

```bash
# Start API server
./build/gstreamer_worker_api --port 8081

# Add camera
curl -X POST http://localhost:8081/api/cameras \
  -H 'Content-Type: application/json' \
  -d '{
    "camera_id": "camera_1",
    "rtsp_url": "rtsp://user:pass@192.168.0.219",
    "target_fps": 12,
    "latency_ms": 150
  }'

# Start camera
curl -X POST http://localhost:8081/api/cameras/camera_1/start

# Check status
curl http://localhost:8081/api/cameras/camera_1/status

# Stop camera
curl -X POST http://localhost:8081/api/cameras/camera_1/stop

# Remove camera
curl -X DELETE http://localhost:8081/api/cameras/camera_1
```

---

**Build Date:** 2025-11-19
**Version:** 1.0.0
**Status:** Production Ready ✅
