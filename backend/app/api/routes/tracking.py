"""
Person Tracking API Routes
Proxies to C++ Worker PersonTracker API for real-time tracking data
"""
from typing import List, Optional
from fastapi import APIRouter, Depends, HTTPException, status
import httpx
import logging

from app.core.config import settings

logger = logging.getLogger(__name__)

router = APIRouter()


async def get_worker_tracking_data(endpoint: str) -> dict:
    """
    Generic function to fetch data from worker PersonTracker API
    
    Args:
        endpoint: API endpoint path (e.g., "/api/persons")
    
    Returns:
        JSON response from worker
    
    Raises:
        HTTPException: If worker is unavailable or returns error
    """
    worker_url = f"{settings.WORKER_API_URL}{endpoint}"
    
    try:
        async with httpx.AsyncClient(timeout=10.0) as client:
            response = await client.get(worker_url)
            
            if response.status_code == 503:
                raise HTTPException(
                    status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                    detail="Person tracking not enabled in worker"
                )
            
            response.raise_for_status()
            return response.json()
            
    except httpx.HTTPError as e:
        logger.error(f"Worker API error: {e}")
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail=f"Worker API unavailable: {str(e)}"
        )


async def post_worker_tracking_command(endpoint: str) -> dict:
    """Post command to worker PersonTracker API"""
    worker_url = f"{settings.WORKER_API_URL}{endpoint}"
    
    try:
        async with httpx.AsyncClient(timeout=10.0) as client:
            response = await client.post(worker_url)
            
            if response.status_code == 503:
                raise HTTPException(
                    status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
                    detail="Person tracking not enabled in worker"
                )
            
            response.raise_for_status()
            return response.json()
            
    except httpx.HTTPError as e:
        logger.error(f"Worker API error: {e}")
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail=f"Worker API unavailable: {str(e)}"
        )


async def delete_worker_person(subject: str) -> dict:
    """Delete person from worker tracking"""
    worker_url = f"{settings.WORKER_API_URL}/api/persons/{subject}"
    
    try:
        async with httpx.AsyncClient(timeout=10.0) as client:
            response = await client.delete(worker_url)
            response.raise_for_status()
            return response.json()
            
    except httpx.HTTPError as e:
        logger.error(f"Worker API error: {e}")
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail=f"Worker API unavailable: {str(e)}"
        )


@router.get("/persons", response_model=dict)
async def get_tracked_persons():
    """
    Get all currently tracked persons from C++ worker
    
    Returns real-time tracking data including:
    - Subject ID
    - Camera appearances with dwell times
    - First/last seen timestamps
    - Recognition statistics
    """
    return await get_worker_tracking_data("/api/persons")


@router.get("/persons/{subject}", response_model=dict)
async def get_tracked_person(subject: str):
    """
    Get specific person's tracking data from C++ worker
    
    - **subject**: CompreFace subject ID
    """
    return await get_worker_tracking_data(f"/api/persons/{subject}")


@router.get("/persons/camera/{camera_id}", response_model=dict)
async def get_persons_at_camera(
    camera_id: str,
    max_age_seconds: Optional[float] = 60.0
):
    """
    Get persons currently at a specific camera
    
    - **camera_id**: Camera identifier
    - **max_age_seconds**: Maximum age of last detection (default: 60 seconds)
    """
    endpoint = f"/api/persons/camera/{camera_id}"
    if max_age_seconds:
        endpoint += f"?max_age_seconds={max_age_seconds}"
    
    return await get_worker_tracking_data(endpoint)


@router.get("/persons/cross-camera", response_model=dict)
async def get_cross_camera_persons():
    """
    Get persons who have been seen at multiple cameras
    
    Useful for identifying people moving between different areas
    """
    return await get_worker_tracking_data("/api/persons/cross-camera")


@router.get("/stats", response_model=dict)
async def get_tracking_statistics():
    """
    Get person tracking statistics from C++ worker
    
    Returns:
    - Total persons tracked (all-time)
    - Currently tracked persons
    - Cross-camera person count
    - Total recognition events
    - Average dwell time
    """
    return await get_worker_tracking_data("/api/tracking/stats")


@router.post("/reset", response_model=dict)
async def trigger_daily_reset():
    """
    Manually trigger daily reset of tracking data
    
    Archives current tracking data and clears the tracker.
    Normally runs automatically at configured hour (default: midnight).
    """
    return await post_worker_tracking_command("/api/tracking/reset")


@router.post("/clear", response_model=dict)
async def clear_tracking_data():
    """
    Clear all tracking data without archiving
    
    WARNING: This permanently removes all in-memory tracking data
    """
    return await post_worker_tracking_command("/api/tracking/clear")


@router.delete("/persons/{subject}", response_model=dict)
async def remove_person_from_tracking(subject: str):
    """
    Remove specific person from active tracking
    
    - **subject**: CompreFace subject ID to remove
    """
    return await delete_worker_person(subject)


@router.get("/live/camera/{camera_id}", response_model=dict)
async def get_live_camera_presence(
    camera_id: str,
    max_age_seconds: float = 5.0
):
    """
    Get persons currently visible at camera (very recent detections)
    
    - **camera_id**: Camera identifier
    - **max_age_seconds**: Maximum age in seconds (default: 5 for "live" view)
    """
    endpoint = f"/api/persons/camera/{camera_id}?max_age_seconds={max_age_seconds}"
    return await get_worker_tracking_data(endpoint)
