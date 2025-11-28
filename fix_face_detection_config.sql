-- Fix face detection configurations for all cameras
-- Based on working config from gstreamer_worker/config/cameras.json

-- Complete face detection configuration
\set face_config '{"enabled": true, "model_path": "models/scrfd/scrfd_10g_bnkps.onnx", "input_size": 640, "confidence_threshold": 0.3, "nms_threshold": 0.5, "frame_skip": 2, "max_frame_width": 1920, "max_frame_height": 1080, "min_face_size": 0.0003, "max_faces": 30, "required_frames": 1, "cooldown_seconds": 0.3, "use_tensorrt": true, "use_cuda": true, "max_batch_size": 4, "save_faces": true, "save_path": "./face_crops", "save_margin": 0.3, "min_save_confidence": 0.2, "max_saves_per_event": 5, "enable_blur_detection": true, "min_laplacian_variance": 250.0, "blur_kernel_size": 3, "motion_triggered_detection": true, "motion_detection_cooldown": 1.5, "enable_compreface": true, "compreface_url": "http://localhost:8000", "compreface_api_key": "318c40d0-2142-40b9-b8ec-59d12acc157d", "compreface_subject": "unknown", "compreface_timeout_ms": 8000, "compreface_max_queue_size": 200, "enable_visualization": true, "draw_landmarks": true, "draw_confidence": true, "box_thickness": 2, "font_scale": 0.5}'

-- Update all cameras with correct face detection config
UPDATE cameras
SET
    face_detection_enabled = true,
    face_detection_config = :'face_config'::jsonb
WHERE camera_id IN ('Experience Center', 'front_door', 'stock', 'stock_2');

-- Verify the updates
SELECT
    camera_id,
    face_detection_enabled,
    face_detection_config->>'model_path' as model_path,
    face_detection_config->>'use_tensorrt' as use_tensorrt,
    face_detection_config->>'enable_compreface' as enable_compreface
FROM cameras
ORDER BY camera_id;
