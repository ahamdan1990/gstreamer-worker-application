# Startup Issue Analysis and Fix

## Problem Summary

When starting the application with uvicorn, you encountered the following error:
```
ERROR | Failed to sync camera Balcony: Client error '400 Bad Request'
Worker response: {"error":"Worker is shutting down"}
Camera synchronization complete: 0 synced, 0 started
```

## Root Causes

### 1. **Primary Issue: Multiple Worker Processes Conflict**

**What happened:**
- There were **TWO worker processes** running simultaneously on port 8081:
  - OLD process (PID 340941) from Dec 03: `gstreamer_enhanced_worker_v2`
  - NEW process (PID 2995966): `gstreamer_worker_api`
- Both processes bound to the same port, causing random failures
- Some requests went to the old (stale) process, others to the new one

**Solution:**
```bash
# Kill all old worker processes
ps aux | grep -E "gstreamer.*worker" | grep -v grep
kill -9 <old_process_pids>
```

### 2. **Secondary Issue: Race Condition During Initialization**

**What happened:**
- FastAPI backend started the worker process at 15:26:03
- FastAPI immediately tried to sync cameras at 15:26:05 (only 2 seconds later)
- Worker was still initializing:
  - Loading PersonTracker from `person_tracking.json`
  - Initializing ONNX Runtime with TensorRT
  - **Compiling TensorRT engine files** (takes 5-10 seconds on first run)
- During this initialization period, adding cameras with face detection failed

**Solution Applied:**
Enhanced the initialization waiting logic in [backend/app/main.py](backend/app/main.py:114-138):

```python
# Increased max wait time for TensorRT compilation
max_wait = 30  # Increased from 10 to 30 seconds

# Additional wait after health check passes
await asyncio.sleep(5)  # Wait for full initialization
```

### 3. **Additional Improvement: Retry Logic for Transient Errors**

Added exponential backoff retry mechanism in [backend/app/main.py](backend/app/main.py:282-343):

```python
# Retry logic for transient initialization errors
max_retries = 3
retry_delay = 2  # seconds

for attempt in range(max_retries):
    try:
        await worker.add_or_update_camera(worker_config)
        break  # Success
    except Exception as e:
        if "shutting down" in str(e).lower() and attempt < max_retries - 1:
            await asyncio.sleep(retry_delay)
            retry_delay *= 2  # Exponential backoff
            continue
```

## Verification

After fixes, all operations work correctly:

```bash
# Health check
curl http://localhost:8081/health
# ✓ {"service": "DeepStream Enhanced Worker", "status": "healthy", "version": "1.0.0"}

# List cameras
curl http://localhost:8081/api/cameras
# ✓ Returns camera list

# Add camera
curl -X POST http://localhost:8081/api/cameras -H 'Content-Type: application/json' \
  -d '{"camera_id":"test","rtsp_url":"rtsp://test"}'
# ✓ {"success": true, "camera_id": "test", "message": "Camera added successfully"}

# Delete camera
curl -X DELETE http://localhost:8081/api/cameras/test
# ✓ {"success": true, "camera_id": "test", "message": "Camera removed"}
```

## Best Practices Going Forward

1. **Before starting the application:**
   ```bash
   # Check for existing worker processes
   ps aux | grep -E "gstreamer.*worker" | grep -v grep

   # Kill any old/stale workers
   pkill -f gstreamer_worker
   ```

2. **Monitor startup logs:**
   - Wait for "Worker is healthy and ready" message
   - Wait for "Camera synchronization complete" message
   - First startup with TensorRT may take 10-15 seconds

3. **Port conflicts:**
   ```bash
   # Check what's using the ports
   ss -tulnp | grep -E ':(8081|8082)'
   ```

## Files Modified

1. [backend/app/main.py](backend/app/main.py) - Lines 114-343
   - Increased worker initialization wait time
   - Added retry logic for camera synchronization
   - Added exponential backoff for transient errors

## Summary

The issue was caused by:
1. **Port conflict** from multiple worker processes (PRIMARY)
2. **Race condition** during worker initialization (SECONDARY)

Both issues have been fixed:
- ✅ Old worker processes killed
- ✅ Proper initialization waiting added
- ✅ Retry mechanism for transient errors implemented

Your application should now start reliably without the "Worker is shutting down" error.
