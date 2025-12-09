"""
Profile Management API Routes
Manages person profiles with face recognition integration
"""
from typing import List, Optional
from fastapi import APIRouter, Depends, HTTPException, status, UploadFile, File
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select, func, and_
from sqlalchemy.orm import joinedload
from uuid import UUID
from datetime import datetime
import httpx

from app.db.base import get_db
from app.models.profile import Profile
from app.models.person_event import PersonEvent
from app.core.config import settings
from app.services.compreface_service import CompreFaceService
import logging

logger = logging.getLogger(__name__)

router = APIRouter()


@router.get("/", response_model=List[dict])
async def list_profiles(
    skip: int = 0,
    limit: int = 100,
    is_active: Optional[bool] = None,
    search: Optional[str] = None,
    db: AsyncSession = Depends(get_db)
):
    """
    List all profiles with optional filtering
    
    - **skip**: Number of records to skip (pagination)
    - **limit**: Maximum number of records to return
    - **is_active**: Filter by active status
    - **search**: Search by name or compreface_subject_id
    """
    query = select(Profile)
    
    # Apply filters
    if is_active is not None:
        query = query.where(Profile.is_active == is_active)
    
    if search:
        search_pattern = f"%{search}%"
        query = query.where(
            (Profile.name.ilike(search_pattern)) |
            (Profile.compreface_subject_id.ilike(search_pattern))
        )
    
    # Apply pagination
    query = query.offset(skip).limit(limit).order_by(Profile.created_at.desc())
    
    result = await db.execute(query)
    profiles = result.scalars().all()
    
    return [
        {
            "id": str(profile.id),
            "name": profile.name,
            "compreface_subject_id": profile.compreface_subject_id,
            "description": profile.description,
            "email": profile.email,
            "phone": profile.phone,
            "images": profile.images or [],
            "is_active": profile.is_active,
            "auto_created": profile.auto_created,
            "alert_on_recognition": profile.alert_on_recognition,
            "total_detections": profile.total_detections,
            "last_seen_at": profile.last_seen_at.isoformat() if profile.last_seen_at else None,
            "last_seen_camera_id": profile.last_seen_camera_id,
            "avg_recognition_similarity": profile.avg_recognition_similarity,
            "tags": profile.tags or [],
            "created_at": profile.created_at.isoformat(),
            "updated_at": profile.updated_at.isoformat()
        }
        for profile in profiles
    ]


@router.get("/{profile_id}", response_model=dict)
async def get_profile(
    profile_id: UUID,
    db: AsyncSession = Depends(get_db)
):
    """Get profile by ID with statistics"""
    result = await db.execute(
        select(Profile)
        .options(joinedload(Profile.watchlists))
        .where(Profile.id == profile_id)
    )
    profile = result.unique().scalar_one_or_none()
    
    if not profile:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Profile with ID {profile_id} not found"
        )
    
    # Get event statistics
    event_count_result = await db.execute(
        select(func.count(PersonEvent.id))
        .where(PersonEvent.profile_id == profile_id)
    )
    event_count = event_count_result.scalar() or 0
    
    # Get recent events
    recent_events_result = await db.execute(
        select(PersonEvent)
        .where(PersonEvent.profile_id == profile_id)
        .order_by(PersonEvent.created_at.desc())
        .limit(10)
    )
    recent_events = recent_events_result.scalars().all()
    
    return {
        "id": str(profile.id),
        "name": profile.name,
        "compreface_subject_id": profile.compreface_subject_id,
        "description": profile.description,
        "email": profile.email,
        "phone": profile.phone,
        "images": profile.images or [],
        "is_active": profile.is_active,
        "auto_created": profile.auto_created,
        "alert_on_recognition": profile.alert_on_recognition,
        "total_detections": profile.total_detections,
        "last_seen_at": profile.last_seen_at.isoformat() if profile.last_seen_at else None,
        "last_seen_camera_id": profile.last_seen_camera_id,
        "avg_recognition_similarity": profile.avg_recognition_similarity,
        "tags": profile.tags or [],
        "extra_metadata": profile.extra_metadata,
        "created_at": profile.created_at.isoformat(),
        "updated_at": profile.updated_at.isoformat(),
        "watchlists": [
            {
                "id": str(wl.id),
                "name": wl.name,
                "color": wl.color
            }
            for wl in profile.watchlists
        ],
        "event_count": event_count,
        "recent_events": [
            {
                "id": str(event.id),
                "camera_id": event.camera_id,
                "similarity": event.similarity,
                "created_at": event.created_at.isoformat()
            }
            for event in recent_events
        ]
    }


@router.post("/", response_model=dict, status_code=status.HTTP_201_CREATED)
async def create_profile(
    name: str,
    compreface_subject_id: str,
    description: Optional[str] = None,
    email: Optional[str] = None,
    phone: Optional[str] = None,
    alert_on_recognition: bool = False,
    tags: Optional[List[str]] = None,
    db: AsyncSession = Depends(get_db)
):
    """
    Create a new profile
    
    - **name**: Person's name (required)
    - **compreface_subject_id**: CompreFace subject ID (required, unique)
    - **description**: Optional description
    - **email**: Optional email address
    - **phone**: Optional phone number
    - **alert_on_recognition**: Enable alerts when recognized
    - **tags**: Optional tags for categorization
    """
    # Check if compreface_subject_id already exists
    result = await db.execute(
        select(Profile).where(Profile.compreface_subject_id == compreface_subject_id)
    )
    if result.scalar_one_or_none():
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"Profile with CompreFace subject ID '{compreface_subject_id}' already exists"
        )
    
    # Create profile
    profile = Profile(
        name=name,
        compreface_subject_id=compreface_subject_id,
        description=description,
        email=email,
        phone=phone,
        alert_on_recognition=alert_on_recognition,
        tags=tags or [],
        images=[],
        auto_created=False
    )
    
    db.add(profile)
    await db.commit()
    await db.refresh(profile)
    
    logger.info(f"Created profile: {profile.name} ({profile.compreface_subject_id})")
    
    return {
        "id": str(profile.id),
        "name": profile.name,
        "compreface_subject_id": profile.compreface_subject_id,
        "description": profile.description,
        "email": profile.email,
        "phone": profile.phone,
        "is_active": profile.is_active,
        "alert_on_recognition": profile.alert_on_recognition,
        "tags": profile.tags,
        "created_at": profile.created_at.isoformat()
    }


@router.patch("/{profile_id}", response_model=dict)
async def update_profile(
    profile_id: UUID,
    name: Optional[str] = None,
    description: Optional[str] = None,
    email: Optional[str] = None,
    phone: Optional[str] = None,
    is_active: Optional[bool] = None,
    alert_on_recognition: Optional[bool] = None,
    tags: Optional[List[str]] = None,
    db: AsyncSession = Depends(get_db)
):
    """Update profile fields"""
    result = await db.execute(
        select(Profile).where(Profile.id == profile_id)
    )
    profile = result.scalar_one_or_none()
    
    if not profile:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Profile with ID {profile_id} not found"
        )
    
    # Update fields
    if name is not None:
        profile.name = name
    if description is not None:
        profile.description = description
    if email is not None:
        profile.email = email
    if phone is not None:
        profile.phone = phone
    if is_active is not None:
        profile.is_active = is_active
    if alert_on_recognition is not None:
        profile.alert_on_recognition = alert_on_recognition
    if tags is not None:
        profile.tags = tags
    
    profile.updated_at = datetime.utcnow()
    
    await db.commit()
    await db.refresh(profile)
    
    logger.info(f"Updated profile: {profile.name}")
    
    return {
        "id": str(profile.id),
        "name": profile.name,
        "compreface_subject_id": profile.compreface_subject_id,
        "description": profile.description,
        "email": profile.email,
        "phone": profile.phone,
        "is_active": profile.is_active,
        "alert_on_recognition": profile.alert_on_recognition,
        "tags": profile.tags,
        "updated_at": profile.updated_at.isoformat()
    }


@router.delete("/{profile_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_profile(
    profile_id: UUID,
    db: AsyncSession = Depends(get_db)
):
    """Delete profile (cascade deletes person_events)"""
    result = await db.execute(
        select(Profile).where(Profile.id == profile_id)
    )
    profile = result.scalar_one_or_none()
    
    if not profile:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Profile with ID {profile_id} not found"
        )
    
    logger.info(f"Deleting profile: {profile.name} ({profile.compreface_subject_id})")
    
    await db.delete(profile)
    await db.commit()
    
    return None


@router.get("/{profile_id}/events", response_model=List[dict])
async def get_profile_events(
    profile_id: UUID,
    skip: int = 0,
    limit: int = 50,
    db: AsyncSession = Depends(get_db)
):
    """Get all person recognition events for a profile"""
    # Verify profile exists
    profile_result = await db.execute(
        select(Profile).where(Profile.id == profile_id)
    )
    if not profile_result.scalar_one_or_none():
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Profile with ID {profile_id} not found"
        )
    
    # Get events
    result = await db.execute(
        select(PersonEvent)
        .where(PersonEvent.profile_id == profile_id)
        .order_by(PersonEvent.created_at.desc())
        .offset(skip)
        .limit(limit)
    )
    events = result.scalars().all()
    
    return [
        {
            "id": str(event.id),
            "camera_id": event.camera_id,
            "subject": event.subject,
            "similarity": event.similarity,
            "detection_confidence": event.detection_confidence,
            "is_new_person": event.is_new_person,
            "is_new_at_camera": event.is_new_at_camera,
            "dwell_time_seconds": event.dwell_time_seconds,
            "image_url": event.image_url,
            "alert_sent": event.alert_sent,
            "acknowledged": event.acknowledged,
            "created_at": event.created_at.isoformat()
        }
        for event in events
    ]


@router.get("/subject/{subject_id}", response_model=dict)
async def get_profile_by_subject(
    subject_id: str,
    db: AsyncSession = Depends(get_db)
):
    """Get profile by CompreFace subject ID"""
    result = await db.execute(
        select(Profile).where(Profile.compreface_subject_id == subject_id)
    )
    profile = result.scalar_one_or_none()
    
    if not profile:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Profile with subject ID '{subject_id}' not found"
        )
    
    return {
        "id": str(profile.id),
        "name": profile.name,
        "compreface_subject_id": profile.compreface_subject_id,
        "is_active": profile.is_active,
        "alert_on_recognition": profile.alert_on_recognition,
        "total_detections": profile.total_detections,
        "last_seen_at": profile.last_seen_at.isoformat() if profile.last_seen_at else None
    }


@router.post("/sync-compreface", response_model=dict)
async def sync_compreface_subjects(
    db: AsyncSession = Depends(get_db)
):
    """
    Sync subjects from CompreFace to profiles

    Fetches all subjects from CompreFace and creates profiles for any that don't exist yet.
    Returns a summary of synced, existing, and failed subjects.
    """
    try:
        # Fetch all subjects from CompreFace
        async with httpx.AsyncClient() as client:
            response = await client.get(
                f"{settings.COMPREFACE_URL}/api/v1/recognition/subjects/",
                headers={"x-api-key": settings.COMPREFACE_API_KEY},
                timeout=30.0
            )
            response.raise_for_status()
            data = response.json()

            # Get subjects list from response
            compreface_subjects = data.get("subjects", [])

        if not compreface_subjects:
            return {
                "message": "No subjects found in CompreFace",
                "synced": 0,
                "existing": 0,
                "failed": 0,
                "subjects": []
            }

        synced = []
        existing = []
        failed = []

        for subject_id in compreface_subjects:
            try:
                # Check if profile already exists
                result = await db.execute(
                    select(Profile).where(Profile.compreface_subject_id == subject_id)
                )
                profile = result.scalar_one_or_none()

                if profile:
                    existing.append(subject_id)
                    logger.info(f"Profile already exists for subject: {subject_id}")
                else:
                    # Create new profile
                    new_profile = Profile(
                        name=subject_id,  # Use subject_id as default name
                        compreface_subject_id=subject_id,
                        auto_created=True,
                        is_active=True,
                        alert_on_recognition=False
                    )
                    db.add(new_profile)
                    await db.commit()
                    await db.refresh(new_profile)

                    synced.append({
                        "subject_id": subject_id,
                        "profile_id": str(new_profile.id),
                        "name": new_profile.name
                    })
                    logger.info(f"Created profile for subject: {subject_id}")

            except Exception as e:
                failed.append({
                    "subject_id": subject_id,
                    "error": str(e)
                })
                logger.error(f"Failed to sync subject {subject_id}: {str(e)}")

        return {
            "message": f"Synced {len(synced)} new profiles from CompreFace",
            "synced": len(synced),
            "existing": len(existing),
            "failed": len(failed),
            "subjects": {
                "synced": synced,
                "existing": existing,
                "failed": failed
            }
        }

    except httpx.HTTPStatusError as e:
        logger.error(f"CompreFace API error: {e.response.status_code} - {e.response.text}")
        raise HTTPException(
            status_code=status.HTTP_502_BAD_GATEWAY,
            detail=f"Failed to connect to CompreFace: {e.response.status_code}"
        )
    except httpx.RequestError as e:
        logger.error(f"CompreFace connection error: {str(e)}")
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail=f"CompreFace service unavailable: {str(e)}"
        )
    except Exception as e:
        logger.error(f"Unexpected error during CompreFace sync: {str(e)}")
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to sync CompreFace subjects: {str(e)}"
        )


@router.get("/{profile_id}/compreface-images", response_model=dict)
async def get_profile_compreface_images(
    profile_id: UUID,
    page: int = 0,
    size: int = 20,
    db: AsyncSession = Depends(get_db)
):
    """
    Fetch reference images for a profile from CompreFace.

    Returns the subject's saved face examples from CompreFace with image URLs.

    Args:
        profile_id: Profile UUID
        page: Page number (default: 0)
        size: Images per page (default: 20)

    Returns:
        {
            "images": [{"image_id": "...", "image_url": "..."}],
            "total": int,
            "page": int,
            "page_size": int,
            "total_pages": int
        }
    """
    # Get profile
    result = await db.execute(
        select(Profile).where(Profile.id == profile_id)
    )
    profile = result.scalar_one_or_none()

    if not profile:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Profile not found"
        )

    if not profile.compreface_subject_id:
        return {
            "images": [],
            "total": 0,
            "page": page,
            "page_size": size,
            "total_pages": 0
        }

    try:
        # Initialize CompreFace service
        compreface = CompreFaceService(
            base_url=settings.COMPREFACE_URL,
            api_key=settings.COMPREFACE_API_KEY
        )

        # Fetch images from CompreFace
        response = await compreface.get_subject_images(
            subject=profile.compreface_subject_id,
            page=page,
            size=size
        )

        if not response:
            return {
                "images": [],
                "total": 0,
                "page": page,
                "page_size": size,
                "total_pages": 0
            }

        # Build image list with direct URLs
        images = [
            {
                "image_id": face.image_id,
                "subject": face.subject,
                "image_url": compreface.get_image_url(face.image_id)
            }
            for face in response.faces
        ]

        return {
            "images": images,
            "total": response.total_elements,
            "page": response.page_number,
            "page_size": response.page_size,
            "total_pages": response.total_pages
        }

    except Exception as e:
        logger.error(f"Failed to fetch CompreFace images for profile {profile_id}: {e}")
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Failed to fetch CompreFace images: {str(e)}"
        )
