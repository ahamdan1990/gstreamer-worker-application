"""
Camera Management API Routes
Manages cameras in database and controls them via Worker API
"""
from typing import List
from fastapi import APIRouter, Depends, HTTPException, status, Form
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from uuid import UUID

from app.db.base import get_db
from app.models.camera import Camera
from app.models.settings import GlobalSettings
from app.schemas.camera import (
    CameraCreate,
    CameraUpdate,
    CameraResponse,
    CameraStatus,
    CameraControl
)
from app.services.worker_client import (
    WorkerClient,
    WorkerCameraConfig
)
from app.core.config import settings
import logging

logger = logging.getLogger(__name__)

router = APIRouter()


def get_worker_client() -> WorkerClient:
    """Get worker client instance"""
    return WorkerClient(base_url=settings.WORKER_API_URL)


async def get_global_similarity_threshold(db: AsyncSession) -> float:
    """Get the global CompreFace similarity threshold from settings"""
    try:
        result = await db.execute(
            select(GlobalSettings).where(GlobalSettings.setting_key == "compreface_recognition")
        )
        setting = result.scalar_one_or_none()

        if setting and "similarity_threshold" in setting.setting_value:
            return float(setting.setting_value["similarity_threshold"])
    except Exception as e:
        logger.warning(f"Failed to fetch global similarity threshold: {e}")

    # Default fallback
    return 0.88


@router.get("/", response_model=List[CameraResponse])
async def list_cameras(
    db: AsyncSession = Depends(get_db),
    worker: WorkerClient = Depends(get_worker_client)
):
    """List all cameras with fresh state from worker"""
    result = await db.execute(select(Camera))
    cameras = result.scalars().all()

    # Sync state from worker for all cameras
    try:
        worker_cameras = await worker.list_cameras()
        worker_state_map = {cam.camera_id: cam for cam in worker_cameras}

        for camera in cameras:
            if camera.camera_id in worker_state_map:
                # Camera exists in worker - update state from worker
                worker_cam = worker_state_map[camera.camera_id]
                camera.state = worker_cam.state
                camera.is_running = worker_cam.is_running
            else:
                # Camera not in worker - mark as ERROR
                camera.state = "ERROR"
                camera.is_running = False

        # Commit the updated states
        await db.commit()
    except Exception as e:
        logger.warning(f"Failed to sync state from worker: {e}")

    return cameras


@router.post("/", response_model=CameraResponse, status_code=status.HTTP_201_CREATED)
async def create_camera(
    camera_data: CameraCreate,
    db: AsyncSession = Depends(get_db),
    worker: WorkerClient = Depends(get_worker_client)
):
    """
    Create a new camera in database and add to worker
    """
    # Check if camera_id already exists
    result = await db.execute(
        select(Camera).where(Camera.camera_id == camera_data.camera_id)
    )
    if result.scalar_one_or_none():
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"Camera with ID '{camera_data.camera_id}' already exists"
        )

    # Create camera in database
    camera_dict = camera_data.model_dump()

    # Handle motion detection separately
    motion_config = camera_dict.pop('motion_detection', None)
    if motion_config:
        camera_dict['motion_detection_enabled'] = motion_config.get('enabled', False)
        camera_dict['motion_detection_config'] = motion_config

    # Handle face detection separately
    face_config = camera_dict.pop('face_detection', None)
    if face_config:
        camera_dict['face_detection_enabled'] = face_config.get('enabled', False)
        camera_dict['face_detection_config'] = face_config

    camera = Camera(**camera_dict)
    db.add(camera)
    await db.commit()
    await db.refresh(camera)

    # Add camera to worker (but don't start it yet)
    try:
        # Build motion detection config if enabled
        worker_motion_config = None
        if camera.motion_detection_enabled and camera.motion_detection_config:
            from app.services.worker_client import MotionDetectionConfig as WorkerMotionConfig
            worker_motion_config = WorkerMotionConfig(**camera.motion_detection_config)

        # Build face detection config if enabled
        worker_face_config = None
        if camera.face_detection_enabled and camera.face_detection_config:
            from app.services.worker_client import FaceDetectionConfig as WorkerFaceConfig
            # Apply global similarity threshold
            face_config_dict = camera.face_detection_config.copy()
            face_config_dict['compreface_similarity_threshold'] = await get_global_similarity_threshold(db)
            worker_face_config = WorkerFaceConfig(**face_config_dict)

        worker_config = WorkerCameraConfig(
            camera_id=camera.camera_id,
            rtsp_url=camera.rtsp_url,
            username=camera.username,
            password=camera.password,
            protocols=camera.protocols,
            latency_ms=camera.latency_ms,
            target_fps=camera.target_fps,
            enable_display=camera.enable_display,
            use_nvidia_decoder=camera.use_nvidia_decoder,
            motion_detection=worker_motion_config,
            face_detection=worker_face_config
        )
        # Use add_or_update for idempotent operation
        await worker.add_or_update_camera(worker_config)
        logger.info(f"Added camera {camera.camera_id} to worker")
    except Exception as e:
        logger.error(f"Failed to add camera to worker: {e}")
        # Don't fail the request - camera is in DB, can be synced later
        camera.state = "ERROR"
        await db.commit()

    return camera


@router.get("/{camera_id}", response_model=CameraResponse)
async def get_camera(
    camera_id: str,
    db: AsyncSession = Depends(get_db)
):
    """Get camera by camera_id"""
    result = await db.execute(
        select(Camera).where(Camera.camera_id == camera_id)
    )
    camera = result.scalar_one_or_none()

    if not camera:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Camera '{camera_id}' not found"
        )

    return camera


@router.patch("/{camera_id}", response_model=CameraResponse)
async def update_camera(
    camera_id: str,
    camera_update: CameraUpdate,
    db: AsyncSession = Depends(get_db),
    worker: WorkerClient = Depends(get_worker_client)
):
    """Update camera configuration"""
    result = await db.execute(
        select(Camera).where(Camera.camera_id == camera_id)
    )
    camera = result.scalar_one_or_none()

    if not camera:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Camera '{camera_id}' not found"
        )

    # Track if camera was running before update
    was_running = camera.is_running

    # Update only provided fields
    update_data = camera_update.model_dump(exclude_unset=True)

    # Handle motion detection separately
    motion_config = update_data.pop('motion_detection', None)
    if motion_config is not None:
        camera.motion_detection_enabled = motion_config.get('enabled', False)
        camera.motion_detection_config = motion_config

    # Handle face detection separately
    face_config = update_data.pop('face_detection', None)
    if face_config is not None:
        camera.face_detection_enabled = face_config.get('enabled', False)
        camera.face_detection_config = face_config

    # Update other fields
    for field, value in update_data.items():
        setattr(camera, field, value)

    await db.commit()
    await db.refresh(camera)

    logger.info(f"Updated camera {camera_id} in database")

    # Sync updated configuration to worker
    try:
        # Build motion detection config if enabled
        worker_motion_config = None
        if camera.motion_detection_enabled and camera.motion_detection_config:
            from app.services.worker_client import MotionDetectionConfig as WorkerMotionConfig
            worker_motion_config = WorkerMotionConfig(**camera.motion_detection_config)

        # Build face detection config if enabled
        worker_face_config = None
        if camera.face_detection_enabled and camera.face_detection_config:
            from app.services.worker_client import FaceDetectionConfig as WorkerFaceConfig
            # Apply global similarity threshold
            face_config_dict = camera.face_detection_config.copy()
            face_config_dict['compreface_similarity_threshold'] = await get_global_similarity_threshold(db)
            worker_face_config = WorkerFaceConfig(**face_config_dict)

        worker_config = WorkerCameraConfig(
            camera_id=camera.camera_id,
            rtsp_url=camera.rtsp_url,
            username=camera.username,
            password=camera.password,
            protocols=camera.protocols,
            latency_ms=camera.latency_ms,
            target_fps=camera.target_fps,
            enable_display=camera.enable_display,
            use_nvidia_decoder=camera.use_nvidia_decoder,
            motion_detection=worker_motion_config,
            face_detection=worker_face_config
        )

        # Update worker configuration (remove and re-add with new config)
        await worker.add_or_update_camera(worker_config)
        logger.info(f"Synced updated configuration to worker for camera {camera_id}")

        # Restart camera if it was running before the update
        if was_running:
            await worker.start_camera(camera_id)
            logger.info(f"Restarted camera {camera_id} with new configuration")
            camera.is_running = True
            await db.commit()

    except Exception as e:
        logger.error(f"Failed to sync camera configuration to worker: {e}")
        # Don't fail the request - configuration is updated in database
        # Worker will get updated config on next sync

    return camera


@router.delete("/{camera_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_camera(
    camera_id: str,
    db: AsyncSession = Depends(get_db),
    worker: WorkerClient = Depends(get_worker_client)
):
    """Delete camera from database and worker"""
    result = await db.execute(
        select(Camera).where(Camera.camera_id == camera_id)
    )
    camera = result.scalar_one_or_none()

    if not camera:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Camera '{camera_id}' not found"
        )

    # Remove from worker first
    try:
        await worker.remove_camera(camera_id)
        logger.info(f"Removed camera {camera_id} from worker")
    except Exception as e:
        logger.warning(f"Failed to remove camera from worker: {e}")
        # Continue with database deletion anyway

    # Delete from database
    await db.delete(camera)
    await db.commit()


@router.post("/{camera_id}/start", response_model=CameraStatus)
async def start_camera(
    camera_id: str,
    db: AsyncSession = Depends(get_db),
    worker: WorkerClient = Depends(get_worker_client)
):
    """Start camera stream"""
    result = await db.execute(
        select(Camera).where(Camera.camera_id == camera_id)
    )
    camera = result.scalar_one_or_none()

    if not camera:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Camera '{camera_id}' not found"
        )

    if not camera.enabled:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"Camera '{camera_id}' is disabled"
        )

    # Ensure camera exists in worker before starting
    try:
        # Check if camera exists in worker by listing all cameras
        worker_cameras = await worker.list_cameras()
        camera_exists = any(cam.camera_id == camera_id for cam in worker_cameras)

        if not camera_exists:
            # Camera doesn't exist in worker, sync it first
            logger.info(f"Camera {camera_id} not in worker, syncing first...")

            # Build motion detection config if enabled
            worker_motion_config = None
            if camera.motion_detection_enabled and camera.motion_detection_config:
                from app.services.worker_client import MotionDetectionConfig as WorkerMotionConfig
                worker_motion_config = WorkerMotionConfig(**camera.motion_detection_config)

            # Build RTSP URL with embedded credentials for worker
            rtsp_url = camera.rtsp_url
            if camera.username and camera.password and "@" not in rtsp_url:
                # Embed credentials in URL: rtsp://user:pass@host
                rtsp_url = rtsp_url.replace("rtsp://", f"rtsp://{camera.username}:{camera.password}@")

            worker_config = WorkerCameraConfig(
                camera_id=camera.camera_id,
                rtsp_url=rtsp_url,
                username=camera.username,
                password=camera.password,
                protocols=camera.protocols,
                latency_ms=camera.latency_ms,
                target_fps=camera.target_fps,
                enable_display=camera.enable_display,
                use_nvidia_decoder=camera.use_nvidia_decoder,
                motion_detection=worker_motion_config
            )

            await worker.add_or_update_camera(worker_config)
            logger.info(f"Synced camera {camera_id} to worker")

        # Now start the camera
        result = await worker.start_camera(camera_id)
        logger.info(f"Started camera {camera_id}")

        # Update database state
        camera.state = result.get("state", "STARTING")
        camera.is_running = True
        await db.commit()

        return CameraStatus(
            camera_id=camera.camera_id,
            state=camera.state,
            is_running=camera.is_running,
            last_seen_at=camera.last_seen_at
        )
    except Exception as e:
        logger.error(f"Failed to start camera: {e}")
        camera.state = "ERROR"
        camera.is_running = False
        await db.commit()
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to start camera: {str(e)}"
        )


@router.post("/{camera_id}/stop", response_model=CameraStatus)
async def stop_camera(
    camera_id: str,
    db: AsyncSession = Depends(get_db),
    worker: WorkerClient = Depends(get_worker_client)
):
    """Stop camera stream"""
    result = await db.execute(
        select(Camera).where(Camera.camera_id == camera_id)
    )
    camera = result.scalar_one_or_none()

    if not camera:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Camera '{camera_id}' not found"
        )

    # Stop camera in worker
    try:
        result = await worker.stop_camera(camera_id)
        logger.info(f"Stopped camera {camera_id}")

        # Update database state
        camera.state = "STOPPED"
        camera.is_running = False
        await db.commit()

        return CameraStatus(
            camera_id=camera.camera_id,
            state=camera.state,
            is_running=camera.is_running,
            last_seen_at=camera.last_seen_at
        )
    except Exception as e:
        logger.error(f"Failed to stop camera: {e}")
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to stop camera: {str(e)}"
        )


@router.get("/{camera_id}/status", response_model=CameraStatus)
async def get_camera_status(
    camera_id: str,
    db: AsyncSession = Depends(get_db),
    worker: WorkerClient = Depends(get_worker_client)
):
    """Get camera status from cache (WebSocket-updated) with HTTP fallback"""
    from app.schemas.camera import CameraMetrics, MotionDetectionMetrics
    from app.services.state_cache import camera_state_cache

    result = await db.execute(
        select(Camera).where(Camera.camera_id == camera_id)
    )
    camera = result.scalar_one_or_none()

    if not camera:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Camera '{camera_id}' not found"
        )

    # Try cache first (updated in real-time via WebSocket)
    cached_state = await camera_state_cache.get_camera(camera_id)

    if cached_state:
        logger.debug(f"Cache hit for {camera_id}")

        # Parse metrics from cache
        cached_metrics = cached_state.get('metrics', {})
        motion_metrics = None

        # Extract motion detection metrics if present
        if 'frames_analyzed' in cached_metrics and cached_metrics['frames_analyzed'] > 0:
            motion_metrics = MotionDetectionMetrics(
                frames_analyzed=cached_metrics.get('frames_analyzed', 0),
                motion_events_detected=cached_metrics.get('motion_events_detected', 0),
                motion_detection_fps=cached_metrics.get('motion_detection_fps', 0.0),
                last_motion_timestamp=cached_metrics.get('last_motion_timestamp')
            )

        # Build complete metrics
        metrics = CameraMetrics(
            uptime_seconds=cached_metrics.get('uptime_seconds', 0.0),
            errors_count=cached_metrics.get('errors_count', 0),
            reconnections=cached_metrics.get('reconnections', 0),
            frames_displayed=cached_metrics.get('frames_displayed', 0),
            motion=motion_metrics
        )

        return CameraStatus(
            camera_id=camera.camera_id,
            state=cached_state.get('state', camera.state),
            is_running=cached_state.get('is_running', camera.is_running),
            last_seen_at=camera.last_seen_at,
            metrics=metrics
        )

    # Fallback to HTTP worker call if cache miss
    logger.warning(f"Cache miss for {camera_id}, falling back to HTTP worker call")

    try:
        worker_status = await worker.get_camera_status(camera_id)

        # Update database with latest status
        camera.state = worker_status.state
        camera.is_running = worker_status.is_running
        await db.commit()

        # Parse metrics from worker
        worker_metrics = worker_status.metrics
        motion_metrics = None

        # Extract motion detection metrics if present
        if 'frames_analyzed' in worker_metrics and worker_metrics['frames_analyzed'] > 0:
            motion_metrics = MotionDetectionMetrics(
                frames_analyzed=worker_metrics.get('frames_analyzed', 0),
                motion_events_detected=worker_metrics.get('motion_events_detected', 0),
                motion_detection_fps=worker_metrics.get('motion_detection_fps', 0.0),
                last_motion_timestamp=worker_metrics.get('last_motion_timestamp')
            )

        # Build complete metrics
        metrics = CameraMetrics(
            uptime_seconds=worker_metrics.get('uptime_seconds', 0.0),
            errors_count=worker_metrics.get('errors_count', 0),
            reconnections=worker_metrics.get('reconnections', 0),
            frames_displayed=worker_metrics.get('frames_displayed', 0),
            motion=motion_metrics
        )

        return CameraStatus(
            camera_id=camera.camera_id,
            state=worker_status.state,
            is_running=worker_status.is_running,
            last_seen_at=camera.last_seen_at,
            metrics=metrics
        )
    except Exception as e:
        logger.error(f"Failed to get camera status: {e}")
        # Return database state if worker is unreachable
        return CameraStatus(
            camera_id=camera.camera_id,
            state=camera.state,
            is_running=camera.is_running,
            last_seen_at=camera.last_seen_at,
            metrics=None
        )


# Face Detection Configuration Endpoints

@router.get("/{camera_id}/face-detection", response_model=dict)
async def get_face_detection_config(
    camera_id: str,
    db: AsyncSession = Depends(get_db)
):
    """Get face detection configuration for a camera"""
    result = await db.execute(
        select(Camera).where(Camera.camera_id == camera_id)
    )
    camera = result.scalar_one_or_none()

    if not camera:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Camera '{camera_id}' not found"
        )

    return {
        "camera_id": camera_id,
        "enabled": camera.face_detection_enabled or False,
        "config": camera.face_detection_config or {}
    }


@router.put("/{camera_id}/face-detection", response_model=dict)
async def update_face_detection_config(
    camera_id: str,
    enabled: bool = Form(...),
    config: str = Form(...),  # JSON string
    db: AsyncSession = Depends(get_db),
    worker: WorkerClient = Depends(get_worker_client)
):
    """Update face detection configuration for a camera"""
    import json

    result = await db.execute(
        select(Camera).where(Camera.camera_id == camera_id)
    )
    camera = result.scalar_one_or_none()

    if not camera:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Camera '{camera_id}' not found"
        )

    # Parse config JSON
    try:
        config_dict = json.loads(config)
    except json.JSONDecodeError:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Invalid JSON for config"
        )

    # Track if camera was running before update
    was_running = camera.is_running

    # Update camera
    camera.face_detection_enabled = enabled
    camera.face_detection_config = config_dict

    await db.commit()
    await db.refresh(camera)

    # Notify worker to reload configuration
    try:
        # Build motion detection config if enabled
        worker_motion_config = None
        if camera.motion_detection_enabled and camera.motion_detection_config:
            from app.services.worker_client import MotionDetectionConfig as WorkerMotionConfig
            worker_motion_config = WorkerMotionConfig(**camera.motion_detection_config)

        # Build face detection config with new settings
        worker_face_config = None
        if camera.face_detection_enabled and camera.face_detection_config:
            from app.services.worker_client import FaceDetectionConfig as WorkerFaceConfig
            worker_face_config = WorkerFaceConfig(**camera.face_detection_config)

        worker_config = WorkerCameraConfig(
            camera_id=camera.camera_id,
            rtsp_url=camera.rtsp_url,
            username=camera.username,
            password=camera.password,
            protocols=camera.protocols,
            latency_ms=camera.latency_ms,
            target_fps=camera.target_fps,
            enable_display=camera.enable_display,
            use_nvidia_decoder=camera.use_nvidia_decoder,
            motion_detection=worker_motion_config,
            face_detection=worker_face_config
        )

        # Update worker configuration (remove and re-add with new config)
        await worker.add_or_update_camera(worker_config)
        logger.info(f"Synced face detection config to worker for camera {camera_id}")

        # Restart camera if it was running before the update
        if was_running:
            await worker.start_camera(camera_id)
            logger.info(f"Restarted camera {camera_id} with new face detection configuration")
            camera.is_running = True
            await db.commit()

    except Exception as e:
        logger.error(f"Failed to sync face detection config to worker: {e}")
        # Don't fail the request - configuration is updated in database

    return {
        "camera_id": camera_id,
        "enabled": camera.face_detection_enabled,
        "config": camera.face_detection_config,
        "message": "Face detection configuration updated successfully and synced to worker"
    }


@router.post("/{camera_id}/face-detection/reset", response_model=dict)
async def reset_face_detection_config(
    camera_id: str,
    db: AsyncSession = Depends(get_db)
):
    """Reset face detection configuration to defaults"""
    result = await db.execute(
        select(Camera).where(Camera.camera_id == camera_id)
    )
    camera = result.scalar_one_or_none()

    if not camera:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Camera '{camera_id}' not found"
        )

    # Default face detection configuration (from cameras.json)
    default_config = {
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
        # Face Quality Filtering
        "enable_frontal_face_filter": True,
        "eye_tilt_threshold": 0.3,
        "min_eye_distance": 0.01,
        "nose_center_offset_threshold": 0.3,
        "eye_to_face_ratio_min": 0.25,
        "eye_to_face_ratio_max": 0.65,
        "enable_min_face_size_filter": True,
        "min_face_width": 80,
        "min_face_height": 80,
        # Hardware
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

    camera.face_detection_config = default_config
    camera.face_detection_enabled = True

    await db.commit()
    await db.refresh(camera)

    return {
        "camera_id": camera_id,
        "config": camera.face_detection_config,
        "message": "Face detection configuration reset to defaults"
    }
