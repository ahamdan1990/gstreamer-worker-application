"""
Camera Management API Routes
Manages cameras in database and controls them via Worker API
"""
from typing import List
from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from uuid import UUID

from app.db.base import get_db
from app.models.camera import Camera
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


@router.get("/", response_model=List[CameraResponse])
async def list_cameras(
    db: AsyncSession = Depends(get_db)
):
    """List all cameras"""
    result = await db.execute(select(Camera))
    cameras = result.scalars().all()
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
    camera = Camera(**camera_data.model_dump())
    db.add(camera)
    await db.commit()
    await db.refresh(camera)

    # Add camera to worker (but don't start it yet)
    try:
        worker_config = WorkerCameraConfig(
            camera_id=camera.camera_id,
            rtsp_url=camera.rtsp_url,
            username=camera.username,
            password=camera.password,
            protocols=camera.protocols,
            latency_ms=camera.latency_ms,
            target_fps=camera.target_fps,
            enable_display=camera.enable_display,
            use_nvidia_decoder=camera.use_nvidia_decoder
        )
        await worker.add_camera(worker_config)
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
    db: AsyncSession = Depends(get_db)
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

    # Update only provided fields
    update_data = camera_update.model_dump(exclude_unset=True)
    for field, value in update_data.items():
        setattr(camera, field, value)

    await db.commit()
    await db.refresh(camera)

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

    # Start camera in worker
    try:
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
    """Get real-time camera status from worker"""
    result = await db.execute(
        select(Camera).where(Camera.camera_id == camera_id)
    )
    camera = result.scalar_one_or_none()

    if not camera:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Camera '{camera_id}' not found"
        )

    # Get live status from worker
    try:
        worker_status = await worker.get_camera_status(camera_id)

        # Update database with latest status
        camera.state = worker_status.state
        camera.is_running = worker_status.is_running
        await db.commit()

        return CameraStatus(
            camera_id=camera.camera_id,
            state=worker_status.state,
            is_running=worker_status.is_running,
            last_seen_at=camera.last_seen_at
        )
    except Exception as e:
        logger.error(f"Failed to get camera status: {e}")
        # Return database state if worker is unreachable
        return CameraStatus(
            camera_id=camera.camera_id,
            state=camera.state,
            is_running=camera.is_running,
            last_seen_at=camera.last_seen_at
        )
