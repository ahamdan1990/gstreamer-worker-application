# Face Cropping & Saving Feature

## Overview
Automatically saves cropped face images when detected - perfect for verification and CompreFace integration!

## Configuration

Add to `gstreamer_worker/config/cameras.json` under `face_detection`:

```json
"save_faces": true,                    // Enable face saving
"save_path": "./face_crops",           // Directory to save crops
"save_margin": 0.3,                    // 30% margin around face
"min_save_confidence": 0.4,            // Only save faces ≥40% confidence
"max_saves_per_event": 3               // Max 3 faces per detection event
```

## Filename Format

Saved as: `{camera_id}_{timestamp}_{face_num}_{confidence}.jpg`

Examples:
- `front_door_1764071285.504_face1_52.jpg` (52% confidence)
- `front_door_1764071285.504_face2_44.jpg` (44% confidence)

## Features

✅ **30% Margin** - Extra space around face for better context
✅ **Quality Filter** - Only saves faces ≥40% confidence
✅ **GPU Optimized** - Downloads frame from GPU once per event
✅ **CompreFace Ready** - Compatible with face recognition APIs
✅ **Batch Limit** - Saves up to 3 faces per event (configurable)

## Usage

### Test Face Detection & Saving

```bash
cd /home/ahamdan/Desktop/development/gstreamer/optimized_pipeline/gstreamer_worker
./build/gstreamer_worker_multi --config config/cameras.json
```

### View Saved Faces

```bash
ls -lh face_crops/
# Example output:
# front_door_1764071285.504_face1_52.jpg  (221x286 pixels, 52% confidence)
# front_door_1764071285.504_face2_44.jpg  (124x523 pixels, 44% confidence)
```

### Verify Face Quality

```bash
# View a face crop
display face_crops/front_door_*.jpg

# Or use Python
python3 -c "from PIL import Image; Image.open('face_crops/front_door_*.jpg').show()"
```

## CompreFace Integration (Future)

These crops are ready for CompreFace face recognition:

```python
import requests

# Upload face to CompreFace
api_key = "your-api-key"
subject = "person_name"

with open("face_crops/front_door_1764071285.504_face1_52.jpg", "rb") as f:
    response = requests.post(
        "http://compreface-server:8000/api/v1/recognition/faces",
        headers={"x-api-key": api_key},
        params={"subject": subject},
        files={"file": f}
    )

print(f"Face registered: {response.json()}")
```

## Troubleshooting

### No faces being saved?
- Check `save_faces: true` in config
- Verify confidence ≥ `min_save_confidence` (default: 0.4)
- Look for save messages in logs: `[FaceDetector] Saved face crop: ...`

### Low quality faces?
- Increase `save_margin` (0.5 = 50% margin)
- Increase `min_save_confidence` (0.5 = 50% min confidence)
- Check lighting conditions on camera

### Too many faces saved?
- Decrease `max_saves_per_event` (try 1-2)
- Increase `min_save_confidence` (try 0.6)

## Directory Structure

```
gstreamer_worker/
├── face_crops/                          # Auto-created
│   ├── front_door_*.jpg                # Face crops from front_door camera
│   └── back_door_*.jpg                 # Face crops from back_door camera
└── config/
    └── cameras.json                     # Configuration
```

## Performance Impact

- **CPU**: Minimal (1-2ms per face for crop and save)
- **GPU**: None (crops from already-downloaded frame)
- **Disk**: ~5-20KB per face crop
- **Recommended**: Enable only when collecting dataset or debugging

## Security Notes

- ⚠️ Face crops may contain PII (personally identifiable information)
- 🔒 Ensure proper access controls on `face_crops/` directory
- 📋 Consider GDPR/privacy regulations when deploying
- 🗑️ Implement automatic cleanup of old crops (not included)
