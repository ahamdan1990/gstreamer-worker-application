#!/usr/bin/env python3
"""
Script to check and fix camera face detection configurations in database
"""

import asyncio
import sys
from pathlib import Path

# Add backend to path
sys.path.insert(0, str(Path(__file__).parent / 'backend'))

from app.database import get_db
from app.models.camera import Camera
from sqlalchemy import select


async def check_and_fix_cameras():
    """Check and fix camera configurations"""

    # Get database session
    db = next(get_db())

    try:
        # Get all cameras
        result = db.execute(select(Camera))
        cameras = result.scalars().all()

        print(f"\n{'='*80}")
        print(f"Found {len(cameras)} cameras in database")
        print(f"{'='*80}\n")

        # Correct face detection config based on working config
        correct_face_config = {
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
            "motion_triggered_detection": True,
            "motion_detection_cooldown": 1.5,
            "enable_compreface": True,
            "compreface_url": "http://localhost:8000",
            "compreface_api_key": "318c40d0-2142-40b9-b8ec-59d12acc157d",
            "compreface_subject": "unknown",
            "compreface_timeout_ms": 8000,
            "compreface_max_queue_size": 200,
            "enable_visualization": True,
            "draw_landmarks": True,
            "draw_confidence": True,
            "box_thickness": 2,
            "font_scale": 0.5
        }

        cameras_to_fix = []

        for camera in cameras:
            print(f"Camera: {camera.camera_id}")
            print(f"  Face Detection Enabled: {camera.face_detection_enabled}")

            if camera.face_detection_config:
                print(f"  Config: {camera.face_detection_config}")

                # Check for issues
                has_issues = False
                issues = []

                if not isinstance(camera.face_detection_config, dict):
                    issues.append("Config is not a dict")
                    has_issues = True
                elif "model_path" not in camera.face_detection_config:
                    issues.append("Missing model_path")
                    has_issues = True
                elif not camera.face_detection_config.get("model_path"):
                    issues.append("Empty model_path")
                    has_issues = True
                elif camera.face_detection_config["model_path"] != "models/scrfd/scrfd_10g_bnkps.onnx":
                    issues.append(f"Wrong model_path: {camera.face_detection_config['model_path']}")
                    has_issues = True

                # Check for None values
                for key, value in camera.face_detection_config.items():
                    if value is None:
                        issues.append(f"Null value for key: {key}")
                        has_issues = True

                if has_issues:
                    print(f"  ⚠️  Issues found:")
                    for issue in issues:
                        print(f"      - {issue}")
                    cameras_to_fix.append(camera)
                else:
                    print(f"  ✓ Config looks good")
            else:
                print(f"  ⚠️  No face detection config")
                if camera.face_detection_enabled:
                    cameras_to_fix.append(camera)

            print()

        # Ask user if they want to fix
        if cameras_to_fix:
            print(f"\n{'='*80}")
            print(f"Found {len(cameras_to_fix)} cameras with issues:")
            for cam in cameras_to_fix:
                print(f"  - {cam.camera_id}")
            print(f"{'='*80}\n")

            response = input("Do you want to fix these cameras? (yes/no): ").strip().lower()

            if response == 'yes':
                for camera in cameras_to_fix:
                    print(f"Fixing {camera.camera_id}...")
                    camera.face_detection_enabled = True
                    camera.face_detection_config = correct_face_config.copy()

                db.commit()
                print(f"\n✓ Fixed {len(cameras_to_fix)} cameras!")
                print("\nPlease restart the backend to sync changes to worker.")
            else:
                print("No changes made.")
        else:
            print("✓ All cameras have correct face detection configuration!")

    finally:
        db.close()


if __name__ == "__main__":
    asyncio.run(check_and_fix_cameras())
