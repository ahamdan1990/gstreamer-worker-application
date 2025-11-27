"""
Watchlist API routes
"""
from typing import List, Optional
from uuid import UUID
from fastapi import APIRouter, Depends, HTTPException, status, Form
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select, func, update, delete
from sqlalchemy.orm import selectinload

from app.db.base import get_db
from app.models.watchlist import Watchlist
from app.models.profile import Profile, watchlist_members

router = APIRouter()


@router.get("/", response_model=List[dict])
async def list_watchlists(
    skip: int = 0,
    limit: int = 100,
    is_active: Optional[bool] = None,
    db: AsyncSession = Depends(get_db)
):
    """
    List all watchlists with optional filtering
    """
    query = select(Watchlist).options(selectinload(Watchlist.profiles))

    if is_active is not None:
        query = query.where(Watchlist.is_active == is_active)

    query = query.offset(skip).limit(limit).order_by(Watchlist.priority.desc(), Watchlist.name)

    result = await db.execute(query)
    watchlists = result.scalars().all()

    return [
        {
            "id": str(w.id),
            "name": w.name,
            "description": w.description,
            "color": w.color,
            "icon": w.icon,
            "is_active": w.is_active,
            "enable_alerts": w.enable_alerts,
            "alert_on_cameras": w.alert_on_cameras or [],
            "alert_cooldown_minutes": w.alert_cooldown_minutes,
            "webhook_url": w.webhook_url,
            "webhook_enabled": w.webhook_enabled,
            "member_count": w.member_count,
            "total_detections": w.total_detections,
            "priority": w.priority,
            "extra_metadata": w.extra_metadata,
            "created_at": w.created_at.isoformat() if w.created_at else None,
            "updated_at": w.updated_at.isoformat() if w.updated_at else None,
            "created_by": w.created_by,
            "profiles": [
                {
                    "id": str(p.id),
                    "name": p.name,
                    "compreface_subject_id": p.compreface_subject_id,
                    "is_active": p.is_active,
                }
                for p in (w.profiles or [])
            ]
        }
        for w in watchlists
    ]


@router.get("/{watchlist_id}", response_model=dict)
async def get_watchlist(
    watchlist_id: UUID,
    db: AsyncSession = Depends(get_db)
):
    """
    Get a specific watchlist by ID with all members
    """
    result = await db.execute(
        select(Watchlist)
        .options(selectinload(Watchlist.profiles))
        .where(Watchlist.id == watchlist_id)
    )
    watchlist = result.scalar_one_or_none()

    if not watchlist:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Watchlist {watchlist_id} not found"
        )

    return {
        "id": str(watchlist.id),
        "name": watchlist.name,
        "description": watchlist.description,
        "color": watchlist.color,
        "icon": watchlist.icon,
        "is_active": watchlist.is_active,
        "enable_alerts": watchlist.enable_alerts,
        "alert_on_cameras": watchlist.alert_on_cameras or [],
        "alert_cooldown_minutes": watchlist.alert_cooldown_minutes,
        "webhook_url": watchlist.webhook_url,
        "webhook_enabled": watchlist.webhook_enabled,
        "member_count": watchlist.member_count,
        "total_detections": watchlist.total_detections,
        "priority": watchlist.priority,
        "extra_metadata": watchlist.extra_metadata,
        "created_at": watchlist.created_at.isoformat() if watchlist.created_at else None,
        "updated_at": watchlist.updated_at.isoformat() if watchlist.updated_at else None,
        "created_by": watchlist.created_by,
        "profiles": [
            {
                "id": str(p.id),
                "name": p.name,
                "compreface_subject_id": p.compreface_subject_id,
                "email": p.email,
                "phone": p.phone,
                "is_active": p.is_active,
                "total_detections": p.total_detections,
                "last_seen_at": p.last_seen_at.isoformat() if p.last_seen_at else None,
                "avg_recognition_similarity": p.avg_recognition_similarity,
            }
            for p in (watchlist.profiles or [])
        ]
    }


@router.post("/", response_model=dict, status_code=status.HTTP_201_CREATED)
async def create_watchlist(
    name: str = Form(...),
    description: Optional[str] = Form(None),
    color: str = Form("#3B82F6"),
    icon: Optional[str] = Form(None),
    enable_alerts: bool = Form(True),
    alert_on_cameras: Optional[str] = Form(None),  # JSON array as string
    alert_cooldown_minutes: int = Form(10),
    webhook_url: Optional[str] = Form(None),
    webhook_enabled: bool = Form(False),
    priority: int = Form(0),
    created_by: Optional[str] = Form(None),
    db: AsyncSession = Depends(get_db)
):
    """
    Create a new watchlist
    """
    import json

    # Parse alert_on_cameras if provided
    camera_ids = []
    if alert_on_cameras:
        try:
            camera_ids = json.loads(alert_on_cameras)
        except json.JSONDecodeError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid JSON for alert_on_cameras"
            )

    # Check if name already exists
    result = await db.execute(
        select(Watchlist).where(Watchlist.name == name)
    )
    if result.scalar_one_or_none():
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"Watchlist with name '{name}' already exists"
        )

    watchlist = Watchlist(
        name=name,
        description=description,
        color=color,
        icon=icon,
        is_active=True,
        enable_alerts=enable_alerts,
        alert_on_cameras=camera_ids,
        alert_cooldown_minutes=alert_cooldown_minutes,
        webhook_url=webhook_url,
        webhook_enabled=webhook_enabled,
        member_count=0,
        total_detections=0,
        priority=priority,
        created_by=created_by
    )

    db.add(watchlist)
    await db.commit()
    await db.refresh(watchlist)

    return {
        "id": str(watchlist.id),
        "name": watchlist.name,
        "description": watchlist.description,
        "color": watchlist.color,
        "icon": watchlist.icon,
        "is_active": watchlist.is_active,
        "enable_alerts": watchlist.enable_alerts,
        "alert_on_cameras": watchlist.alert_on_cameras or [],
        "alert_cooldown_minutes": watchlist.alert_cooldown_minutes,
        "webhook_url": watchlist.webhook_url,
        "webhook_enabled": watchlist.webhook_enabled,
        "member_count": watchlist.member_count,
        "total_detections": watchlist.total_detections,
        "priority": watchlist.priority,
        "created_at": watchlist.created_at.isoformat() if watchlist.created_at else None,
        "updated_at": watchlist.updated_at.isoformat() if watchlist.updated_at else None,
        "created_by": watchlist.created_by,
    }


@router.patch("/{watchlist_id}", response_model=dict)
async def update_watchlist(
    watchlist_id: UUID,
    name: Optional[str] = Form(None),
    description: Optional[str] = Form(None),
    color: Optional[str] = Form(None),
    icon: Optional[str] = Form(None),
    is_active: Optional[bool] = Form(None),
    enable_alerts: Optional[bool] = Form(None),
    alert_on_cameras: Optional[str] = Form(None),
    alert_cooldown_minutes: Optional[int] = Form(None),
    webhook_url: Optional[str] = Form(None),
    webhook_enabled: Optional[bool] = Form(None),
    priority: Optional[int] = Form(None),
    db: AsyncSession = Depends(get_db)
):
    """
    Update a watchlist
    """
    import json

    result = await db.execute(
        select(Watchlist).where(Watchlist.id == watchlist_id)
    )
    watchlist = result.scalar_one_or_none()

    if not watchlist:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Watchlist {watchlist_id} not found"
        )

    # Update fields
    if name is not None:
        watchlist.name = name
    if description is not None:
        watchlist.description = description
    if color is not None:
        watchlist.color = color
    if icon is not None:
        watchlist.icon = icon
    if is_active is not None:
        watchlist.is_active = is_active
    if enable_alerts is not None:
        watchlist.enable_alerts = enable_alerts
    if alert_on_cameras is not None:
        try:
            watchlist.alert_on_cameras = json.loads(alert_on_cameras)
        except json.JSONDecodeError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid JSON for alert_on_cameras"
            )
    if alert_cooldown_minutes is not None:
        watchlist.alert_cooldown_minutes = alert_cooldown_minutes
    if webhook_url is not None:
        watchlist.webhook_url = webhook_url
    if webhook_enabled is not None:
        watchlist.webhook_enabled = webhook_enabled
    if priority is not None:
        watchlist.priority = priority

    await db.commit()
    await db.refresh(watchlist)

    return {
        "id": str(watchlist.id),
        "name": watchlist.name,
        "description": watchlist.description,
        "color": watchlist.color,
        "icon": watchlist.icon,
        "is_active": watchlist.is_active,
        "enable_alerts": watchlist.enable_alerts,
        "alert_on_cameras": watchlist.alert_on_cameras or [],
        "alert_cooldown_minutes": watchlist.alert_cooldown_minutes,
        "webhook_url": watchlist.webhook_url,
        "webhook_enabled": watchlist.webhook_enabled,
        "member_count": watchlist.member_count,
        "total_detections": watchlist.total_detections,
        "priority": watchlist.priority,
        "created_at": watchlist.created_at.isoformat() if watchlist.created_at else None,
        "updated_at": watchlist.updated_at.isoformat() if watchlist.updated_at else None,
        "created_by": watchlist.created_by,
    }


@router.delete("/{watchlist_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_watchlist(
    watchlist_id: UUID,
    db: AsyncSession = Depends(get_db)
):
    """
    Delete a watchlist
    """
    result = await db.execute(
        select(Watchlist).where(Watchlist.id == watchlist_id)
    )
    watchlist = result.scalar_one_or_none()

    if not watchlist:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Watchlist {watchlist_id} not found"
        )

    await db.delete(watchlist)
    await db.commit()


@router.post("/{watchlist_id}/members/{profile_id}", status_code=status.HTTP_201_CREATED)
async def add_profile_to_watchlist(
    watchlist_id: UUID,
    profile_id: UUID,
    db: AsyncSession = Depends(get_db)
):
    """
    Add a profile to a watchlist
    """
    # Get watchlist
    result = await db.execute(
        select(Watchlist).options(selectinload(Watchlist.profiles))
        .where(Watchlist.id == watchlist_id)
    )
    watchlist = result.scalar_one_or_none()

    if not watchlist:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Watchlist {watchlist_id} not found"
        )

    # Get profile
    result = await db.execute(
        select(Profile).where(Profile.id == profile_id)
    )
    profile = result.scalar_one_or_none()

    if not profile:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Profile {profile_id} not found"
        )

    # Check if already a member
    if profile in watchlist.profiles:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"Profile {profile.name} is already in watchlist {watchlist.name}"
        )

    # Add profile to watchlist
    watchlist.profiles.append(profile)
    watchlist.member_count = len(watchlist.profiles)

    await db.commit()

    return {"message": f"Profile {profile.name} added to watchlist {watchlist.name}"}


@router.delete("/{watchlist_id}/members/{profile_id}", status_code=status.HTTP_204_NO_CONTENT)
async def remove_profile_from_watchlist(
    watchlist_id: UUID,
    profile_id: UUID,
    db: AsyncSession = Depends(get_db)
):
    """
    Remove a profile from a watchlist
    """
    # Get watchlist with profiles
    result = await db.execute(
        select(Watchlist).options(selectinload(Watchlist.profiles))
        .where(Watchlist.id == watchlist_id)
    )
    watchlist = result.scalar_one_or_none()

    if not watchlist:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Watchlist {watchlist_id} not found"
        )

    # Find profile in watchlist
    profile_to_remove = None
    for profile in watchlist.profiles:
        if profile.id == profile_id:
            profile_to_remove = profile
            break

    if not profile_to_remove:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Profile {profile_id} not in watchlist {watchlist.name}"
        )

    # Remove profile
    watchlist.profiles.remove(profile_to_remove)
    watchlist.member_count = len(watchlist.profiles)

    await db.commit()


@router.get("/{watchlist_id}/statistics", response_model=dict)
async def get_watchlist_statistics(
    watchlist_id: UUID,
    db: AsyncSession = Depends(get_db)
):
    """
    Get statistics for a watchlist
    """
    result = await db.execute(
        select(Watchlist).options(selectinload(Watchlist.profiles))
        .where(Watchlist.id == watchlist_id)
    )
    watchlist = result.scalar_one_or_none()

    if not watchlist:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Watchlist {watchlist_id} not found"
        )

    # Calculate statistics from profiles
    total_detections = sum(p.total_detections for p in watchlist.profiles)
    active_members = sum(1 for p in watchlist.profiles if p.is_active)
    avg_similarity = (
        sum(p.avg_recognition_similarity for p in watchlist.profiles) / len(watchlist.profiles)
        if watchlist.profiles else 0.0
    )

    return {
        "watchlist_id": str(watchlist.id),
        "name": watchlist.name,
        "member_count": watchlist.member_count,
        "active_members": active_members,
        "total_detections": total_detections,
        "avg_similarity": avg_similarity,
        "alert_enabled": watchlist.enable_alerts,
        "webhook_enabled": watchlist.webhook_enabled,
    }
