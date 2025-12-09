# CompreFace Similarity Threshold - Now Configurable! 🎉

## Overview
The CompreFace similarity threshold is now a **global system setting** that can be configured from the frontend settings page. Previously hardcoded at 0.88 (88%), it's now fully adjustable to meet your recognition accuracy needs.

---

## What Changed

### 1. **Worker (C++)** - Configuration Support
**Files Modified:**
- `gstreamer_worker/include/config.h:125`
  - Added `compreface_similarity_threshold` field to `FaceDetectionConfig`
  - Default: `0.88f` (88% match confidence)

- `gstreamer_worker/src/config_loader.cpp:459-461`
  - Parses `compreface_similarity_threshold` from JSON configuration

- `gstreamer_worker/src/face_detector.cpp:57`
  - Passes threshold from config to CompreFaceClient

**Result:** Worker now respects similarity threshold from camera configuration instead of using hardcoded value.

---

### 2. **Backend** - Global Settings System
**Files Modified:**

#### Settings API (`backend/app/api/routes/settings.py`)
- **NEW Endpoints:**
  - `GET /api/v1/settings/compreface-recognition/config`
    - Returns current similarity threshold setting
    - Auto-creates with default 0.88 if not found

  - `PUT /api/v1/settings/compreface-recognition/config`
    - Updates similarity threshold
    - Validates range: 0.0 to 1.0
    - Form parameter: `similarity_threshold` (float)

- **Added to Reset Defaults (Line 148-151):**
```python
"compreface_recognition": {
    "similarity_threshold": 0.88,
    "description": "Minimum similarity score (0.0-1.0) for face recognition match. Higher = stricter matching."
}
```

#### Worker Client (`backend/app/services/worker_client.py:78`)
- Added `compreface_similarity_threshold: float = 0.88` to FaceDetectionConfig

#### Camera API (`backend/app/api/routes/cameras.py`)
- **NEW Helper Function (Lines 38-52):** `get_global_similarity_threshold(db)`
  - Fetches global setting from database
  - Falls back to 0.88 if not found

- **Applied in create_camera (Lines 140-142):**
```python
face_config_dict = camera.face_detection_config.copy()
face_config_dict['compreface_similarity_threshold'] = await get_global_similarity_threshold(db)
worker_face_config = WorkerFaceConfig(**face_config_dict)
```

- **Applied in update_camera (Lines 248-250):**
  - Same logic ensures updated cameras use global setting

**Result:** All new/updated cameras automatically use the global similarity threshold setting.

---

### 3. **Frontend** - Settings UI
**File Modified:** `frontend-ui/app/settings/page.tsx`

**NEW Features:**
- **State Management (Lines 68-72):**
```typescript
const [comprefaceRecognition, setComprefaceRecognition] = useState({
  similarity_threshold: 0.88,
  description: '',
});
```

- **Load Settings (Lines 114-119):**
  - Fetches config from `/api/v1/settings/compreface-recognition/config`
  - Populates slider on page load

- **Save Function (Lines 195-221):**
  - Sends FormData with `similarity_threshold`
  - Shows success/error messages
  - Notifies user about when changes take effect

- **NEW UI Card (Lines 620-705):**
  - **Interactive Slider:** 0% to 100% (0.0 to 1.0)
  - **Real-time Display:** Shows current percentage and category (Lenient/Balanced/Strict)
  - **Helpful Guidance:**
    - Lower values (60-80%): More matches, may include false positives
    - Recommended (85-90%): Balanced accuracy
    - Higher values (90-95%): Stricter matching, may miss valid matches
  - **Visual Feedback:** Large percentage display, slider with markers
  - **Save Button:** Explicitly saves the new threshold

---

## How to Use

### Access Settings Page
1. Navigate to: `http://localhost:3000/settings`
2. Scroll to the **"Face Recognition"** card

### Adjust Similarity Threshold
1. **Use the slider** to adjust the threshold (0-100%)
2. **Watch the percentage** update in real-time
3. **Read the guidance** to understand impact:
   - **88%** (Default): Balanced accuracy, recommended for most use cases
   - **Lower (70-85%)**: More lenient, catches more matches but may have false positives
   - **Higher (90-95%)**: Very strict, only exact matches, may miss some valid recognitions

4. **Click "Save Recognition Settings"**
5. **See confirmation message:** "CompreFace recognition settings saved successfully!"

### Apply to Cameras
**For NEW Cameras:**
- Automatically uses the global threshold when added

**For EXISTING Cameras:**
- Option 1: **Update camera** (any field) → Threshold is applied
- Option 2: **Restart camera** → Worker reloads with new config
- Option 3: **Restart worker** → All cameras reload with new threshold

---

## API Endpoints

### Get Current Threshold
```bash
curl http://localhost:8000/api/v1/settings/compreface-recognition/config
```

**Response:**
```json
{
  "similarity_threshold": 0.88,
  "description": "Minimum similarity score (0.0-1.0) for face recognition match. Higher = stricter matching."
}
```

### Update Threshold
```bash
curl -X PUT http://localhost:8000/api/v1/settings/compreface-recognition/config \
  -F "similarity_threshold=0.90"
```

**Response:**
```json
{
  "setting_key": "compreface_recognition",
  "setting_value": {
    "similarity_threshold": 0.90,
    "description": "..."
  },
  "message": "CompreFace recognition configuration updated successfully"
}
```

---

## Configuration Flow

```
Frontend Settings Page
        ↓
    [User adjusts slider to 85%]
        ↓
Backend Settings API
    (Saves 0.85 to database)
        ↓
    Global Setting Stored
        ↓
[User adds/updates camera]
        ↓
Camera API
    (Fetches global threshold: 0.85)
        ↓
Worker Client
    (Includes in camera config)
        ↓
Worker Face Detector
    (Creates CompreFaceClient with threshold: 0.85)
        ↓
CompreFace API
    (Only matches with similarity >= 0.85 are accepted)
```

---

## Technical Details

### Database Schema
**Table:** `global_settings`
- **setting_key:** `"compreface_recognition"`
- **setting_value:** JSON containing `{"similarity_threshold": 0.88, "description": "..."}`
- Auto-created on first GET request if not found

### Configuration Struct (C++)
```cpp
struct FaceDetectionConfig {
    // ... other fields ...
    float compreface_similarity_threshold = 0.88f;  // NEW!
    int compreface_timeout_ms = 5000;
    int compreface_max_queue_size = 100;
};
```

### CompreFaceClient Usage
```cpp
CompreFaceConfig cf_config;
cf_config.base_url = config_.compreface_url;
cf_config.api_key = config_.compreface_api_key;
cf_config.similarity_threshold = config_.compreface_similarity_threshold;  // Uses config value
cf_config.timeout_ms = config_.compreface_timeout_ms;
```

---

## Testing Checklist

### ✅ Backend
- [x] GET endpoint returns default 0.88
- [x] PUT endpoint updates threshold
- [x] PUT validates range (0.0-1.0)
- [x] Invalid values are rejected with 400 error
- [x] Camera creation fetches global threshold
- [x] Camera update fetches global threshold

### ✅ Frontend
- [x] Settings page loads current threshold
- [x] Slider updates percentage display
- [x] Save button sends correct value
- [x] Success/error messages display
- [x] Guidance text helps user understand impact

### ✅ Worker
- [x] Worker parses compreface_similarity_threshold from JSON
- [x] CompreFaceClient uses configured threshold
- [x] Recognition respects new threshold value

---

## Troubleshooting

### Threshold not applying to existing cameras
**Solution:** Update or restart the camera to reload configuration

### Frontend shows wrong value
**Solution:** Hard refresh browser (Ctrl+Shift+R), check backend is running

### Recognition too strict/lenient
**Recommended Values:**
- **General use:** 85-88% (balanced)
- **High security:** 92-95% (strict, may miss some matches)
- **Testing/development:** 70-80% (lenient, catches more faces)

### Setting not persisting
**Solution:** Check database connection, verify global_settings table exists

---

## Success! 🎉

You now have full control over CompreFace face recognition accuracy from a user-friendly settings page. Adjust the threshold based on your specific needs:

- **Need more matches?** Lower the threshold
- **Need higher accuracy?** Raise the threshold
- **Testing different subjects?** Experiment with different values

The system provides real-time feedback and helpful guidance to help you find the perfect balance between accuracy and coverage.

---

## Files Summary

### Worker (C++)
- ✅ `gstreamer_worker/include/config.h`
- ✅ `gstreamer_worker/src/config_loader.cpp`
- ✅ `gstreamer_worker/src/face_detector.cpp`

### Backend (Python)
- ✅ `backend/app/api/routes/settings.py`
- ✅ `backend/app/services/worker_client.py`
- ✅ `backend/app/api/routes/cameras.py`

### Frontend (TypeScript)
- ✅ `frontend-ui/app/settings/page.tsx`

**Total:** 7 files modified, all changes production-ready and tested!
