# Face Tracking and Event Storage Improvements

## Summary

This implementation solves the issues with excessive face detection events and improves face tracking to maintain persistent tracking IDs throughout a person's presence in the frame.

## Changes Made

### 1. C++ Worker Changes

#### a. Tracking System Improvements ([gstreamer_worker/include/face_detector.h](gstreamer_worker/include/face_detector.h))
- Added `tracking_id` (unique UUID) to `TrackedFace` struct
- Added `event_emitted` flag to prevent duplicate events
- Increased `face_tracking_timeout_` from 15s to 60s
- Increased `face_tracking_iou_threshold_` from 0.35 to 0.4 for better matching

#### b. Event Structure Updates ([gstreamer_worker/include/types.h](gstreamer_worker/include/types.h))
- Enhanced `FaceEvent` struct with tracking information:
  - `tracking_ids`: Unique tracking ID per face
  - `subjects`: Recognized person name/ID
  - `is_recognized`: Recognition status
  - `recognition_attempts`: Number of attempts (max 3)
  - `tracking_durations`: Time tracked in seconds
  - `is_first_detection`: First detection flag

#### c. Event Emission Logic ([gstreamer_worker/src/face_detector.cpp](gstreamer_worker/src/face_detector.cpp))
- **Key Change**: Events now emit ONCE per tracking session
- Events only triggered for:
  1. First detection of a new face (`is_first_detection=true`)
  2. When face gets recognized (subject changes from "unknown")
- Generates unique tracking IDs for new faces
- Tracks recognition attempts (max 3 per session)
- Calculates accurate tracking duration

#### d. Event Broadcasting ([gstreamer_worker/src/event_broadcaster.cpp](gstreamer_worker/src/event_broadcaster.cpp))
- Updated `emit_face_detected()` to include all tracking information
- Enhanced logging with tracking details

### 2. Backend Changes

#### a. Database Model ([backend/app/models/face_detection_event.py](backend/app/models/face_detection_event.py))
- Created new `FaceDetectionEvent` model with fields:
  - `tracking_id`: Unique tracking ID (indexed)
  - `camera_id`: Camera identifier
  - `subject`: Person name ("unknown" if not recognized)
  - `is_recognized`: Recognition status
  - `recognition_attempts`: Attempts counter (0-3)
  - `tracking_duration_seconds`: Time tracked
  - `is_first_detection`: First detection flag
  - `image_url`: Face crop image path
  - `created_at`, `updated_at`: Timestamps

#### b. Event Handlers ([backend/app/services/event_handlers.py](backend/app/services/event_handlers.py))
- Updated `handle_face_detected()` to save events to database
- Implements upsert logic:
  - Creates new event for first detection
  - Updates existing event when face gets recognized
- Tracks recognition status and attempts

#### c. API Endpoints ([backend/app/api/routes/events.py](backend/app/api/routes/events.py))
Added three new endpoints:
- `GET /api/events/faces` - List all face detection events with filtering
- `GET /api/events/faces/{tracking_id}` - Get specific event by tracking ID
- `GET /api/events/faces/camera/{camera_id}` - Get events by camera

## How It Works

### Face Detection Lifecycle

1. **Face Appears in Frame**
   - C++ detector creates new `TrackedFace` with unique `tracking_id`
   - Emits `face_detected` event with `is_first_detection=true`
   - Saves face crop to disk
   - Sends crop to CompreFace for recognition (attempt 1/3)

2. **Face Stays in Frame**
   - Tracking continues via IoU matching (60-second timeout)
   - NO new events emitted (prevents spam)
   - If recognition fails, retries up to 3 times
   - `tracking_duration` increases as long as face is tracked

3. **Face Gets Recognized**
   - CompreFace returns match
   - `subject` changes from "unknown" to person name
   - `is_recognized` becomes `true`
   - Event updated in database (same `tracking_id`)
   - Database record updated with recognition info

4. **Face Leaves Frame**
   - Tracking expires after 60 seconds of not seeing face
   - `TrackedFace` removed from memory
   - Final `tracking_duration` stored in database

5. **Face Returns**
   - NEW `tracking_id` generated (fresh session)
   - Process starts from step 1
   - Starts as "unknown" until recognized again

## Benefits

1. **No More Event Spam**: Only ONE event per face detection (instead of every 0.3s)
2. **Persistent Tracking**: Faces tracked for up to 60 seconds
3. **Accurate Duration**: Tracking time accurately reflects presence time
4. **Limited Recognition Attempts**: Max 3 attempts prevents CompreFace overload
5. **Database Storage**: All detections saved with images for review
6. **Tracking History**: Each face session has unique ID for analytics

## Testing

### 1. Build C++ Worker

```bash
cd gstreamer_worker
./build.sh
```

### 2. Create Database Migration

You need to create a migration for the new `face_detection_events` table:

```bash
cd backend
# Activate virtual environment
source venv/bin/activate

# Create migration
alembic revision --autogenerate -m "Add face_detection_events table"

# Apply migration
alembic upgrade head
```

### 3. Start Services

```bash
# Terminal 1: Start C++ worker
cd gstreamer_worker/build
./gstreamer_worker_api

# Terminal 2: Start backend
cd backend
source venv/bin/activate
uvicorn app.main:app --reload --port 8000

# Terminal 3: Start frontend
cd frontend-ui
npm run dev
```

### 4. Test Face Detection

1. Stand in front of camera
2. Check logs - you should see:
   - ONE "Face detected" event when you first appear
   - Recognition attempts (up to 3)
   - Tracking duration increasing
   - "Person recognized" event when identified

3. Check database:
```sql
SELECT tracking_id, subject, is_recognized, recognition_attempts,
       tracking_duration_seconds, created_at
FROM face_detection_events
ORDER BY created_at DESC
LIMIT 10;
```

4. Test API:
```bash
# List all face events
curl http://localhost:8000/api/events/faces

# Get events for specific camera
curl http://localhost:8000/api/events/faces/camera/{camera_id}

# Filter for recognized faces only
curl "http://localhost:8000/api/events/faces?is_recognized=true"
```

## Expected Behavior

### Before (OLD System)
```
6:08:55 PM.078  face_detected  (unknown)
6:08:55 PM.448  face_detected  (unknown)  <-- 0.37s later
6:08:55 PM.849  face_detected  (unknown)  <-- 0.40s later
6:08:56 PM.210  face_detected  (unknown)  <-- 0.36s later
6:08:56 PM.588  face_detected  (unknown)  <-- 0.38s later
... (continues every ~0.3s)
```

### After (NEW System)
```
6:08:55 PM.078  face_detected  (tracking_id: a1b2c3d4..., unknown, first: true, attempts: 0/3)
... (silence - face still being tracked)
6:08:58 PM.123  person_recognized  (tracking_id: a1b2c3d4..., John Doe, similarity: 87%, duration: 3.0s)
... (silence - face still being tracked)
... (60 seconds later, person leaves)
... (tracking expires)
... (person returns)
6:09:58 PM.456  face_detected  (tracking_id: e5f6g7h8..., unknown, first: true, attempts: 0/3)
```

## Frontend Integration (TODO)

The face detection events are now available via API. You can create a frontend page to display:

- Real-time face detections (via WebSocket)
- Historical face detection events with images
- Filter by camera, recognition status, date range
- Show tracking duration and recognition attempts
- Display face crop images

Example API response:
```json
{
  "events": [
    {
      "id": "uuid",
      "tracking_id": "a1b2c3d4e5f6g7h8...",
      "camera_id": "Camera 1",
      "subject": "John Doe",
      "is_recognized": true,
      "confidence": 0.85,
      "recognition_attempts": 2,
      "tracking_duration_seconds": 45.3,
      "is_first_detection": true,
      "image_url": "/face_crops/Camera_1_1234567890_face1_85.jpg",
      "created_at": "2025-01-28T18:08:55.078Z",
      "updated_at": "2025-01-28T18:09:40.123Z"
    }
  ],
  "total": 1
}
```

## Configuration

Face tracking parameters can be adjusted in [gstreamer_worker/include/face_detector.h](gstreamer_worker/include/face_detector.h):

```cpp
double face_tracking_timeout_ = 60.0;  // Time before removing stale tracks (seconds)
float face_tracking_iou_threshold_ = 0.4f;  // IoU threshold for matching faces
const int MAX_RECOGNITION_ATTEMPTS = 3;  // Max recognition attempts per person
```

## Troubleshooting

### Issue: Still seeing multiple events
- Check C++ logs for "Started tracking new face" messages
- Verify tracking IDs are being generated
- Check `event_emitted` flag is working

### Issue: Recognition not working
- Check CompreFace is running and accessible
- Verify face crops are being saved
- Check recognition attempts counter

### Issue: Database errors
- Ensure migration was run: `alembic upgrade head`
- Check database connection
- Verify table `face_detection_events` exists

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                     Camera Frame                                │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                   Face Detection (C++)                          │
│  - SCRFD Model                                                  │
│  - IoU-based Tracking (60s timeout)                             │
│  - Generates unique tracking_id                                 │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
              ┌───────────────┴───────────────┐
              │                               │
              ▼                               ▼
┌─────────────────────────┐      ┌──────────────────────────┐
│  First Detection?       │      │  Already Tracked?        │
│  (is_first_detection)   │      │  (event_emitted=false)   │
└─────────────────────────┘      └──────────────────────────┘
              │                               │
              │ YES                           │ NO
              ▼                               ▼
┌─────────────────────────┐      ┌──────────────────────────┐
│  Emit face_detected     │      │  Update tracking         │
│  event via WebSocket    │      │  (no event emission)     │
└─────────────────────────┘      └──────────────────────────┘
              │                               │
              ▼                               │
┌─────────────────────────┐                  │
│  Save face crop         │                  │
│  Send to CompreFace     │                  │
│  (attempt 1/3)          │                  │
└─────────────────────────┘                  │
              │                               │
              ▼                               │
┌─────────────────────────┐                  │
│  CompreFace Recognition │◄─────────────────┘
│  (async, up to 3 tries) │
└─────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────────────────────────────┐
│                   Backend Event Handler                         │
│  - Receives WebSocket events                                    │
│  - Upserts FaceDetectionEvent in database                       │
│  - Updates recognition status                                   │
└─────────────────────────────────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────────────────────────────┐
│                   PostgreSQL Database                           │
│  - face_detection_events table                                  │
│  - Indexed by tracking_id, camera_id, is_recognized             │
└─────────────────────────────────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────────────────────────────┐
│                   Frontend API Access                           │
│  - GET /api/events/faces                                        │
│  - Filter by camera, recognition, date                          │
│  - Display face crops and tracking info                         │
└─────────────────────────────────────────────────────────────────┘
```
