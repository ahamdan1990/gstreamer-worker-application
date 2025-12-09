#!/usr/bin/env python3
"""
Fix face detection configuration in database
"""
import asyncio
from sqlalchemy import select
from sqlalchemy.ext.asyncio import create_async_engine, AsyncSession
from sqlalchemy.orm import sessionmaker
import sys
import os

# Add backend to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'backend'))

from app.models.camera import Camera
from app.core.config import settings

async def fix_face_detection_config():
    """Fix face detection configuration for all cameras"""
    engine = create_async_engine(str(settings.DATABASE_URL))
    async_session = sessionmaker(engine, class_=AsyncSession, expire_on_commit=False)

    async with async_session() as session:
        # Get all cameras
        result = await session.execute(select(Camera))
        cameras = result.scalars().all()

        print(f"Found {len(cameras)} cameras")

        for camera in cameras:
            print(f"\n=== Camera: {camera.camera_id} ===")

            if camera.face_detection_enabled and camera.face_detection_config:
                config = camera.face_detection_config
                print(f"Current config: {config}")

                # Set default values for None/missing fields
                defaults = {
                    "enabled": True,
                    "model_path": "models/scrfd/scrfd_10g_bnkps.onnx",
                    "input_size": 640,
                    "confidence_threshold": 0.3,
                    "nms_threshold": 0.5,
                    "frame_skip": 2,
                    "max_frame_width": 1920,
                    "max_frame_height": 1080,
                    "min_face_size": 0.0003,
                    "max_faces": 30,
                    "required_frames": 1,
                    "cooldown_seconds": 0.3,
                    "enable_frontal_face_filter": True,
                    "eye_tilt_threshold": 0.3,
                    "min_eye_distance": 0.01,
                    "nose_center_offset_threshold": 0.3,
                    "eye_to_face_ratio_min": 0.25,
                    "eye_to_face_ratio_max": 0.65,
                    "enable_min_face_size_filter": True,
                    "min_face_width": 80,
                    "min_face_height": 80,
                    "use_tensorrt": True,
                    "use_cuda": True,
                    "max_batch_size": 4,
                    "save_faces": True,
                    "save_path": "./face_crops",
                    "save_margin": 0.3,
                    "min_save_confidence": 0.2,
                    "max_saves_per_event": 5,
                    "enable_blur_detection": True,
                    "min_laplacian_variance": 250.0,
                    "blur_kernel_size": 3,
                    # CRITICAL: Set motion_triggered_detection to False so faces are always detected
                    "motion_triggered_detection": False,  # Change from True to False!
                    "motion_detection_cooldown": 1.5,
                    "enable_compreface": True,
                    "compreface_url": "http://localhost:8000",
                    "compreface_api_key": "318c40d0-2142-40b9-b8ec-59d12acc157d",
                    "compreface_subject": "unknown",
                    "compreface_similarity_threshold": 0.88,
                    "compreface_timeout_ms": 8000,
                    "compreface_max_queue_size": 200,
                    "enable_visualization": True,
                    "draw_landmarks": True,
                    "draw_confidence": True,
                    "box_thickness": 2,
                    "font_scale": 0.5
                }

                # Update config with defaults for None/missing values
                updated = False
                for key, default_value in defaults.items():
                    if key not in config or config[key] is None:
                        print(f"  Setting {key} = {default_value}")
                        config[key] = default_value
                        updated = True

                if updated:
                    camera.face_detection_config = config
                    print(f"✓ Updated face detection config for {camera.camera_id}")
                else:
                    print(f"  No updates needed")
            else:
                print(f"  Face detection not enabled or no config")

        # Commit changes
        await session.commit()
        print("\n✓ All cameras updated successfully!")
        print("\n⚠️  IMPORTANT: Restart the cameras for changes to take effect:")
        print("   1. Stop all cameras")
        print("   2. Restart the backend/worker")
        print("   3. Start cameras again")

if __name__ == "__main__":
    asyncio.run(fix_face_detection_config())
