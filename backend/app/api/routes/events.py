"""
Person Events API routes
"""
from typing import List, Optional
from uuid import UUID
from datetime import datetime
from fastapi import APIRouter, Depends, HTTPException, status, Query
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select, func, and_
from sqlalchemy.orm import selectinload

from app.db.base import get_db
from app.models.person_event import PersonEvent
from app.models.face_detection_event import FaceDetectionEvent
from app.models.profile import Profile

router = APIRouter()


@router.get("/persons", response_model=dict)
async def list_person_events(
    skip: int = 0,
    limit: int = 100,
    camera_id: Optional[str] = None,
    profile_id: Optional[UUID] = None,
    acknowledged: Optional[bool] = None,
    start_date: Optional[str] = None,
    end_date: Optional[str] = None,
    db: AsyncSession = Depends(get_db)
):
    """
    List all person events with optional filtering
    """
    query = select(PersonEvent).options(selectinload(PersonEvent.profile))

    # Apply filters
    filters = []

    if camera_id:
        filters.append(PersonEvent.camera_id == camera_id)

    if profile_id:
        filters.append(PersonEvent.profile_id == profile_id)

    if acknowledged is not None:
        filters.append(PersonEvent.acknowledged == acknowledged)

    if start_date:
        try:
            start_dt = datetime.fromisoformat(start_date.replace('Z', '+00:00'))
            filters.append(PersonEvent.created_at >= start_dt)
        except ValueError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid start_date format. Use ISO format (YYYY-MM-DDTHH:MM:SS)"
            )

    if end_date:
        try:
            end_dt = datetime.fromisoformat(end_date.replace('Z', '+00:00'))
            filters.append(PersonEvent.created_at <= end_dt)
        except ValueError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid end_date format. Use ISO format (YYYY-MM-DDTHH:MM:SS)"
            )

    if filters:
        query = query.where(and_(*filters))

    # Get total count
    count_query = select(func.count()).select_from(PersonEvent)
    if filters:
        count_query = count_query.where(and_(*filters))

    total_result = await db.execute(count_query)
    total = total_result.scalar()

    # Apply pagination and ordering
    query = query.offset(skip).limit(limit).order_by(PersonEvent.created_at.desc())

    result = await db.execute(query)
    events = result.scalars().all()

    return {
        "events": [
            {
                "id": str(e.id),
                "profile_id": str(e.profile_id) if e.profile_id else None,
                "camera_id": e.camera_id,
                "subject": e.subject,
                "similarity": e.similarity,
                "detection_confidence": e.detection_confidence,
                "is_new_person": e.is_new_person,
                "is_new_at_camera": e.is_new_at_camera,
                "dwell_time_seconds": e.dwell_time_seconds,
                "other_cameras": e.other_cameras or [],
                "image_url": e.image_url,
                "thumbnail_url": e.thumbnail_url,
                "bounding_box": e.bounding_box,
                "alert_sent": e.alert_sent,
                "alert_sent_at": e.alert_sent_at.isoformat() if e.alert_sent_at else None,
                "alert_channels": e.alert_channels or [],
                "acknowledged": e.acknowledged,
                "acknowledged_at": e.acknowledged_at.isoformat() if e.acknowledged_at else None,
                "acknowledged_by": e.acknowledged_by,
                "extra_metadata": e.extra_metadata,
                "worker_metadata": e.worker_metadata,
                "created_at": e.created_at.isoformat() if e.created_at else None,
                "profile": {
                    "id": str(e.profile.id),
                    "name": e.profile.name,
                    "compreface_subject_id": e.profile.compreface_subject_id,
                    "is_active": e.profile.is_active,
                } if e.profile else None
            }
            for e in events
        ],
        "total": total,
        "skip": skip,
        "limit": limit
    }


@router.get("/persons/{event_id}", response_model=dict)
async def get_person_event(
    event_id: UUID,
    db: AsyncSession = Depends(get_db)
):
    """
    Get a specific person event by ID
    """
    result = await db.execute(
        select(PersonEvent)
        .options(selectinload(PersonEvent.profile))
        .where(PersonEvent.id == event_id)
    )
    event = result.scalar_one_or_none()

    if not event:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Person event {event_id} not found"
        )

    return {
        "id": str(event.id),
        "profile_id": str(event.profile_id) if event.profile_id else None,
        "camera_id": event.camera_id,
        "subject": event.subject,
        "similarity": event.similarity,
        "detection_confidence": event.detection_confidence,
        "is_new_person": event.is_new_person,
        "is_new_at_camera": event.is_new_at_camera,
        "dwell_time_seconds": event.dwell_time_seconds,
        "other_cameras": event.other_cameras or [],
        "image_url": event.image_url,
        "thumbnail_url": event.thumbnail_url,
        "bounding_box": event.bounding_box,
        "alert_sent": event.alert_sent,
        "alert_sent_at": event.alert_sent_at.isoformat() if event.alert_sent_at else None,
        "alert_channels": event.alert_channels or [],
        "acknowledged": event.acknowledged,
        "acknowledged_at": event.acknowledged_at.isoformat() if event.acknowledged_at else None,
        "acknowledged_by": event.acknowledged_by,
        "extra_metadata": event.extra_metadata,
        "worker_metadata": event.worker_metadata,
        "created_at": event.created_at.isoformat() if event.created_at else None,
        "profile": {
            "id": str(event.profile.id),
            "name": event.profile.name,
            "compreface_subject_id": event.profile.compreface_subject_id,
            "email": event.profile.email,
            "phone": event.profile.phone,
            "is_active": event.profile.is_active,
            "total_detections": event.profile.total_detections,
            "last_seen_at": event.profile.last_seen_at.isoformat() if event.profile.last_seen_at else None,
        } if event.profile else None
    }


@router.post("/persons/{event_id}/acknowledge", response_model=dict)
async def acknowledge_person_event(
    event_id: UUID,
    acknowledged_by: Optional[str] = Query(None),
    db: AsyncSession = Depends(get_db)
):
    """
    Acknowledge a person event
    """
    result = await db.execute(
        select(PersonEvent).where(PersonEvent.id == event_id)
    )
    event = result.scalar_one_or_none()

    if not event:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Person event {event_id} not found"
        )

    # Update acknowledgment fields
    event.acknowledged = True
    event.acknowledged_at = datetime.utcnow()
    event.acknowledged_by = acknowledged_by or "system"

    await db.commit()
    await db.refresh(event)

    return {
        "id": str(event.id),
        "acknowledged": event.acknowledged,
        "acknowledged_at": event.acknowledged_at.isoformat() if event.acknowledged_at else None,
        "acknowledged_by": event.acknowledged_by,
    }


@router.get("/persons/camera/{camera_id}", response_model=dict)
async def get_events_by_camera(
    camera_id: str,
    skip: int = 0,
    limit: int = 50,
    db: AsyncSession = Depends(get_db)
):
    """
    Get all person events for a specific camera
    """
    query = (
        select(PersonEvent)
        .options(selectinload(PersonEvent.profile))
        .where(PersonEvent.camera_id == camera_id)
        .offset(skip)
        .limit(limit)
        .order_by(PersonEvent.created_at.desc())
    )

    result = await db.execute(query)
    events = result.scalars().all()

    return {
        "events": [
            {
                "id": str(e.id),
                "profile_id": str(e.profile_id) if e.profile_id else None,
                "subject": e.subject,
                "similarity": e.similarity,
                "detection_confidence": e.detection_confidence,
                "is_new_person": e.is_new_person,
                "dwell_time_seconds": e.dwell_time_seconds,
                "acknowledged": e.acknowledged,
                "created_at": e.created_at.isoformat() if e.created_at else None,
                "profile": {
                    "name": e.profile.name,
                    "compreface_subject_id": e.profile.compreface_subject_id,
                } if e.profile else None
            }
            for e in events
        ],
        "camera_id": camera_id,
        "total": len(events)
    }


@router.get("/persons/profile/{profile_id}", response_model=dict)
async def get_events_by_profile(
    profile_id: UUID,
    skip: int = 0,
    limit: int = 50,
    db: AsyncSession = Depends(get_db)
):
    """
    Get all person events for a specific profile
    """
    query = (
        select(PersonEvent)
        .where(PersonEvent.profile_id == profile_id)
        .offset(skip)
        .limit(limit)
        .order_by(PersonEvent.created_at.desc())
    )

    result = await db.execute(query)
    events = result.scalars().all()

    return {
        "events": [
            {
                "id": str(e.id),
                "camera_id": e.camera_id,
                "subject": e.subject,
                "similarity": e.similarity,
                "detection_confidence": e.detection_confidence,
                "is_new_person": e.is_new_person,
                "is_new_at_camera": e.is_new_at_camera,
                "dwell_time_seconds": e.dwell_time_seconds,
                "acknowledged": e.acknowledged,
                "created_at": e.created_at.isoformat() if e.created_at else None,
            }
            for e in events
        ],
        "profile_id": str(profile_id),
        "total": len(events)
    }


@router.get("/statistics", response_model=dict)
async def get_events_statistics(
    camera_id: Optional[str] = None,
    start_date: Optional[str] = None,
    end_date: Optional[str] = None,
    db: AsyncSession = Depends(get_db)
):
    """
    Get statistics about person events
    """
    filters = []

    if camera_id:
        filters.append(PersonEvent.camera_id == camera_id)

    if start_date:
        try:
            start_dt = datetime.fromisoformat(start_date.replace('Z', '+00:00'))
            filters.append(PersonEvent.created_at >= start_dt)
        except ValueError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid start_date format"
            )

    if end_date:
        try:
            end_dt = datetime.fromisoformat(end_date.replace('Z', '+00:00'))
            filters.append(PersonEvent.created_at <= end_dt)
        except ValueError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid end_date format"
            )

    # Total events
    total_query = select(func.count()).select_from(PersonEvent)
    if filters:
        total_query = total_query.where(and_(*filters))
    total_result = await db.execute(total_query)
    total_events = total_result.scalar()

    # New persons
    new_persons_query = select(func.count()).select_from(PersonEvent).where(PersonEvent.is_new_person == True)
    if filters:
        new_persons_query = new_persons_query.where(and_(*filters))
    new_persons_result = await db.execute(new_persons_query)
    new_persons = new_persons_result.scalar()

    # Acknowledged events
    ack_query = select(func.count()).select_from(PersonEvent).where(PersonEvent.acknowledged == True)
    if filters:
        ack_query = ack_query.where(and_(*filters))
    ack_result = await db.execute(ack_query)
    acknowledged = ack_result.scalar()

    # Alerts sent
    alerts_query = select(func.count()).select_from(PersonEvent).where(PersonEvent.alert_sent == True)
    if filters:
        alerts_query = alerts_query.where(and_(*filters))
    alerts_result = await db.execute(alerts_query)
    alerts_sent = alerts_result.scalar()

    return {
        "total_events": total_events,
        "new_persons": new_persons,
        "acknowledged_events": acknowledged,
        "alerts_sent": alerts_sent,
        "acknowledgment_rate": round((acknowledged / total_events * 100), 2) if total_events > 0 else 0.0,
        "alert_rate": round((alerts_sent / total_events * 100), 2) if total_events > 0 else 0.0,
    }


# ============================================================================
# Face Detection Events (with Tracking)
# ============================================================================

@router.get("/faces", response_model=dict)
async def list_face_detection_events(
    skip: int = 0,
    limit: int = 100,
    camera_id: Optional[str] = None,
    tracking_id: Optional[str] = None,
    is_recognized: Optional[bool] = None,
    start_date: Optional[str] = None,
    end_date: Optional[str] = None,
    db: AsyncSession = Depends(get_db)
):
    """
    List all face detection events with optional filtering

    Shows face detections with tracking information including:
    - Unique tracking IDs
    - Recognition status
    - Tracking duration
    - Recognition attempts
    """
    query = select(FaceDetectionEvent)

    # Apply filters
    filters = []

    if camera_id:
        filters.append(FaceDetectionEvent.camera_id == camera_id)

    if tracking_id:
        filters.append(FaceDetectionEvent.tracking_id == tracking_id)

    if is_recognized is not None:
        filters.append(FaceDetectionEvent.is_recognized == is_recognized)

    if start_date:
        try:
            start_dt = datetime.fromisoformat(start_date.replace('Z', '+00:00'))
            filters.append(FaceDetectionEvent.created_at >= start_dt)
        except ValueError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid start_date format. Use ISO format (YYYY-MM-DDTHH:MM:SS)"
            )

    if end_date:
        try:
            end_dt = datetime.fromisoformat(end_date.replace('Z', '+00:00'))
            filters.append(FaceDetectionEvent.created_at <= end_dt)
        except ValueError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid end_date format. Use ISO format (YYYY-MM-DDTHH:MM:SS)"
            )

    if filters:
        query = query.where(and_(*filters))

    # Get total count
    count_query = select(func.count()).select_from(FaceDetectionEvent)
    if filters:
        count_query = count_query.where(and_(*filters))

    total_result = await db.execute(count_query)
    total = total_result.scalar()

    # Apply pagination and ordering
    query = query.offset(skip).limit(limit).order_by(FaceDetectionEvent.created_at.desc())

    result = await db.execute(query)
    events = result.scalars().all()

    return {
        "events": [
            {
                "id": str(e.id),
                "tracking_id": e.tracking_id,
                "camera_id": e.camera_id,
                "subject": e.subject,
                "is_recognized": e.is_recognized,
                "confidence": e.confidence,
                "recognition_attempts": e.recognition_attempts,
                "tracking_duration_seconds": e.tracking_duration_seconds,
                "is_first_detection": e.is_first_detection,
                "image_url": e.image_url,
                "thumbnail_url": e.thumbnail_url,
                "bounding_box": e.bounding_box,
                "extra_metadata": e.extra_metadata,
                "worker_metadata": e.worker_metadata,
                "created_at": e.created_at.isoformat() if e.created_at else None,
                "updated_at": e.updated_at.isoformat() if e.updated_at else None,
            }
            for e in events
        ],
        "total": total,
        "skip": skip,
        "limit": limit
    }


@router.get("/faces/daily-summary", response_model=dict)
async def get_daily_person_summary(
    start_date: Optional[str] = None,
    end_date: Optional[str] = None,
    camera_id: Optional[str] = None,
    db: AsyncSession = Depends(get_db)
):
    """
    Get daily summary of recognized persons

    Groups recognized faces by person and date, showing:
    - Person name
    - Number of detections per day
    - First and last detection times
    - Sample face crops
    """
    query = select(FaceDetectionEvent).where(FaceDetectionEvent.is_recognized == True)

    filters = []

    if camera_id:
        filters.append(FaceDetectionEvent.camera_id == camera_id)

    if start_date:
        try:
            start_dt = datetime.fromisoformat(start_date.replace('Z', '+00:00'))
            filters.append(FaceDetectionEvent.created_at >= start_dt)
        except ValueError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid start_date format"
            )

    if end_date:
        try:
            end_dt = datetime.fromisoformat(end_date.replace('Z', '+00:00'))
            filters.append(FaceDetectionEvent.created_at <= end_dt)
        except ValueError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid end_date format"
            )

    if filters:
        query = query.where(and_(*filters))

    query = query.order_by(FaceDetectionEvent.created_at.desc())

    result = await db.execute(query)
    events = result.scalars().all()

    # Group by subject and date
    from collections import defaultdict
    daily_summary = defaultdict(lambda: defaultdict(list))

    for event in events:
        if event.created_at:
            date_key = event.created_at.date().isoformat()
            daily_summary[event.subject][date_key].append(event)

    # Format response
    summary_data = []
    for subject, dates in daily_summary.items():
        for date, detections in dates.items():
            # Get first and last detection times
            times = [d.created_at for d in detections if d.created_at]
            first_seen = min(times) if times else None
            last_seen = max(times) if times else None

            # Get sample images (up to 3)
            sample_images = [
                d.image_url for d in detections[:3] if d.image_url
            ]

            # Calculate average confidence
            confidences = [d.confidence for d in detections if d.confidence]
            avg_confidence = sum(confidences) / len(confidences) if confidences else 0

            # Get cameras used
            cameras = list(set(d.camera_id for d in detections))

            summary_data.append({
                "subject": subject,
                "date": date,
                "detection_count": len(detections),
                "first_seen": first_seen.isoformat() if first_seen else None,
                "last_seen": last_seen.isoformat() if last_seen else None,
                "avg_confidence": round(avg_confidence, 2),
                "sample_images": sample_images,
                "cameras": cameras,
                "total_duration": sum(d.tracking_duration_seconds for d in detections)
            })

    # Sort by date (newest first), then by subject
    summary_data.sort(key=lambda x: (x["date"], x["subject"]), reverse=True)

    return {
        "summary": summary_data,
        "total_days": len(set(item["date"] for item in summary_data)),
        "total_persons": len(set(item["subject"] for item in summary_data)),
        "total_detections": sum(item["detection_count"] for item in summary_data)
    }


@router.get("/faces/{tracking_id}", response_model=dict)
async def get_face_detection_event(
    tracking_id: str,
    db: AsyncSession = Depends(get_db)
):
    """
    Get a specific face detection event by tracking ID
    """
    result = await db.execute(
        select(FaceDetectionEvent)
        .where(FaceDetectionEvent.tracking_id == tracking_id)
    )
    event = result.scalar_one_or_none()

    if not event:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Face detection event with tracking_id {tracking_id} not found"
        )

    return {
        "id": str(event.id),
        "tracking_id": event.tracking_id,
        "camera_id": event.camera_id,
        "subject": event.subject,
        "is_recognized": event.is_recognized,
        "confidence": event.confidence,
        "recognition_attempts": event.recognition_attempts,
        "tracking_duration_seconds": event.tracking_duration_seconds,
        "is_first_detection": event.is_first_detection,
        "image_url": event.image_url,
        "thumbnail_url": event.thumbnail_url,
        "bounding_box": event.bounding_box,
        "extra_metadata": event.extra_metadata,
        "worker_metadata": event.worker_metadata,
        "created_at": event.created_at.isoformat() if event.created_at else None,
        "updated_at": event.updated_at.isoformat() if event.updated_at else None,
    }


@router.get("/faces/camera/{camera_id}", response_model=dict)
async def get_face_events_by_camera(
    camera_id: str,
    skip: int = 0,
    limit: int = 50,
    is_recognized: Optional[bool] = None,
    db: AsyncSession = Depends(get_db)
):
    """
    Get all face detection events for a specific camera
    """
    query = (
        select(FaceDetectionEvent)
        .where(FaceDetectionEvent.camera_id == camera_id)
    )

    if is_recognized is not None:
        query = query.where(FaceDetectionEvent.is_recognized == is_recognized)

    query = query.offset(skip).limit(limit).order_by(FaceDetectionEvent.created_at.desc())

    result = await db.execute(query)
    events = result.scalars().all()

    return {
        "events": [
            {
                "id": str(e.id),
                "tracking_id": e.tracking_id,
                "subject": e.subject,
                "is_recognized": e.is_recognized,
                "confidence": e.confidence,
                "recognition_attempts": e.recognition_attempts,
                "tracking_duration_seconds": e.tracking_duration_seconds,
                "image_url": e.image_url,
                "created_at": e.created_at.isoformat() if e.created_at else None,
            }
            for e in events
        ],
        "camera_id": camera_id,
        "total": len(events)
    }
