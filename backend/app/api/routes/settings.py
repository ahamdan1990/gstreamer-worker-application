"""
Global Settings API routes
"""
from typing import List, Optional
from fastapi import APIRouter, Depends, HTTPException, status, Form
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
import json

from app.db.base import get_db
from app.models.settings import GlobalSettings

router = APIRouter()


@router.get("/", response_model=List[dict])
async def list_settings(
    db: AsyncSession = Depends(get_db)
):
    """List all global settings"""
    result = await db.execute(select(GlobalSettings))
    settings = result.scalars().all()

    return [
        {
            "id": str(s.id),
            "setting_key": s.setting_key,
            "setting_value": s.setting_value,
            "description": s.description,
            "created_at": s.created_at.isoformat() if s.created_at else None,
            "updated_at": s.updated_at.isoformat() if s.updated_at else None,
            "updated_by": s.updated_by,
        }
        for s in settings
    ]


@router.get("/{setting_key}", response_model=dict)
async def get_setting(
    setting_key: str,
    db: AsyncSession = Depends(get_db)
):
    """Get a specific global setting by key"""
    result = await db.execute(
        select(GlobalSettings).where(GlobalSettings.setting_key == setting_key)
    )
    setting = result.scalar_one_or_none()

    if not setting:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Setting '{setting_key}' not found"
        )

    return {
        "id": str(setting.id),
        "setting_key": setting.setting_key,
        "setting_value": setting.setting_value,
        "description": setting.description,
        "created_at": setting.created_at.isoformat() if setting.created_at else None,
        "updated_at": setting.updated_at.isoformat() if setting.updated_at else None,
        "updated_by": setting.updated_by,
    }


@router.put("/{setting_key}", response_model=dict)
async def update_setting(
    setting_key: str,
    setting_value: str = Form(...),  # JSON string
    updated_by: Optional[str] = Form(None),
    db: AsyncSession = Depends(get_db)
):
    """Update a global setting"""
    result = await db.execute(
        select(GlobalSettings).where(GlobalSettings.setting_key == setting_key)
    )
    setting = result.scalar_one_or_none()

    if not setting:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Setting '{setting_key}' not found"
        )

    # Parse JSON value
    try:
        value_dict = json.loads(setting_value)
    except json.JSONDecodeError:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="Invalid JSON for setting_value"
        )

    # Update setting
    setting.setting_value = value_dict
    setting.updated_by = updated_by

    await db.commit()
    await db.refresh(setting)

    return {
        "id": str(setting.id),
        "setting_key": setting.setting_key,
        "setting_value": setting.setting_value,
        "description": setting.description,
        "updated_at": setting.updated_at.isoformat() if setting.updated_at else None,
        "updated_by": setting.updated_by,
        "message": f"Setting '{setting_key}' updated successfully"
    }


@router.post("/{setting_key}/reset", response_model=dict)
async def reset_setting(
    setting_key: str,
    db: AsyncSession = Depends(get_db)
):
    """Reset a setting to its default value"""
    result = await db.execute(
        select(GlobalSettings).where(GlobalSettings.setting_key == setting_key)
    )
    setting = result.scalar_one_or_none()

    if not setting:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Setting '{setting_key}' not found"
        )

    # Default configurations
    defaults = {
        "person_tracking": {
            "enabled": True,
            "presence_timeout_seconds": 300.0,
            "same_camera_cooldown_seconds": 10.0,
            "max_detections_per_person": 100,
            "enable_cross_camera_tracking": True,
            "enable_persistence": True,
            "persistence_path": "./person_tracking.json",
            "enable_daily_reset": True,
            "daily_reset_hour": 0,
            "archive_path": "./tracking_archives"
        },
        "manager": {
            "log_level": "INFO",
            "enable_metrics": True,
            "metrics_interval": 30.0
        },
        "compreface_recognition": {
            "similarity_threshold": 0.88,
            "description": "Minimum similarity score (0.0-1.0) for face recognition match. Higher = stricter matching."
        }
    }

    if setting_key not in defaults:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"No default configuration available for '{setting_key}'"
        )

    setting.setting_value = defaults[setting_key]
    await db.commit()
    await db.refresh(setting)

    return {
        "id": str(setting.id),
        "setting_key": setting.setting_key,
        "setting_value": setting.setting_value,
        "message": f"Setting '{setting_key}' reset to default"
    }


# Specific endpoints for person tracking

@router.get("/person-tracking/config", response_model=dict)
async def get_person_tracking_config(
    db: AsyncSession = Depends(get_db)
):
    """Get person tracking configuration"""
    result = await db.execute(
        select(GlobalSettings).where(GlobalSettings.setting_key == "person_tracking")
    )
    setting = result.scalar_one_or_none()

    if not setting:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Person tracking configuration not found"
        )

    return setting.setting_value


@router.put("/person-tracking/config", response_model=dict)
async def update_person_tracking_config(
    enabled: Optional[bool] = Form(None),
    presence_timeout_seconds: Optional[float] = Form(None),
    same_camera_cooldown_seconds: Optional[float] = Form(None),
    max_detections_per_person: Optional[int] = Form(None),
    enable_cross_camera_tracking: Optional[bool] = Form(None),
    enable_persistence: Optional[bool] = Form(None),
    persistence_path: Optional[str] = Form(None),
    enable_daily_reset: Optional[bool] = Form(None),
    daily_reset_hour: Optional[int] = Form(None),
    archive_path: Optional[str] = Form(None),
    updated_by: Optional[str] = Form(None),
    db: AsyncSession = Depends(get_db)
):
    """Update person tracking configuration"""
    result = await db.execute(
        select(GlobalSettings).where(GlobalSettings.setting_key == "person_tracking")
    )
    setting = result.scalar_one_or_none()

    if not setting:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Person tracking configuration not found"
        )

    # Get current config
    config = setting.setting_value

    # Update fields that were provided
    if enabled is not None:
        config["enabled"] = enabled
    if presence_timeout_seconds is not None:
        config["presence_timeout_seconds"] = presence_timeout_seconds
    if same_camera_cooldown_seconds is not None:
        config["same_camera_cooldown_seconds"] = same_camera_cooldown_seconds
    if max_detections_per_person is not None:
        config["max_detections_per_person"] = max_detections_per_person
    if enable_cross_camera_tracking is not None:
        config["enable_cross_camera_tracking"] = enable_cross_camera_tracking
    if enable_persistence is not None:
        config["enable_persistence"] = enable_persistence
    if persistence_path is not None:
        config["persistence_path"] = persistence_path
    if enable_daily_reset is not None:
        config["enable_daily_reset"] = enable_daily_reset
    if daily_reset_hour is not None:
        config["daily_reset_hour"] = daily_reset_hour
    if archive_path is not None:
        config["archive_path"] = archive_path

    setting.setting_value = config
    setting.updated_by = updated_by

    await db.commit()
    await db.refresh(setting)

    return {
        "setting_key": "person_tracking",
        "setting_value": setting.setting_value,
        "message": "Person tracking configuration updated successfully"
    }


# Specific endpoints for manager settings

@router.get("/manager/config", response_model=dict)
async def get_manager_config(
    db: AsyncSession = Depends(get_db)
):
    """Get manager configuration"""
    result = await db.execute(
        select(GlobalSettings).where(GlobalSettings.setting_key == "manager")
    )
    setting = result.scalar_one_or_none()

    if not setting:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Manager configuration not found"
        )

    return setting.setting_value


@router.put("/manager/config", response_model=dict)
async def update_manager_config(
    log_level: Optional[str] = Form(None),
    enable_metrics: Optional[bool] = Form(None),
    metrics_interval: Optional[float] = Form(None),
    updated_by: Optional[str] = Form(None),
    db: AsyncSession = Depends(get_db)
):
    """Update manager configuration"""
    result = await db.execute(
        select(GlobalSettings).where(GlobalSettings.setting_key == "manager")
    )
    setting = result.scalar_one_or_none()

    if not setting:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Manager configuration not found"
        )

    # Get current config
    config = setting.setting_value

    # Update fields that were provided
    if log_level is not None:
        if log_level not in ["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"]:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid log_level. Must be one of: DEBUG, INFO, WARNING, ERROR, CRITICAL"
            )
        config["log_level"] = log_level
    if enable_metrics is not None:
        config["enable_metrics"] = enable_metrics
    if metrics_interval is not None:
        config["metrics_interval"] = metrics_interval

    setting.setting_value = config
    setting.updated_by = updated_by

    await db.commit()
    await db.refresh(setting)

    return {
        "setting_key": "manager",
        "setting_value": setting.setting_value,
        "message": "Manager configuration updated successfully"
    }


# Specific endpoints for CompreFace recognition settings

@router.get("/compreface-recognition/config", response_model=dict)
async def get_compreface_recognition_config(
    db: AsyncSession = Depends(get_db)
):
    """Get CompreFace recognition configuration"""
    result = await db.execute(
        select(GlobalSettings).where(GlobalSettings.setting_key == "compreface_recognition")
    )
    setting = result.scalar_one_or_none()

    if not setting:
        # Auto-create with default value if not found
        default_config = {
            "similarity_threshold": 0.88,
            "description": "Minimum similarity score (0.0-1.0) for face recognition match. Higher = stricter matching."
        }
        setting = GlobalSettings(
            setting_key="compreface_recognition",
            setting_value=default_config,
            description="CompreFace face recognition settings"
        )
        db.add(setting)
        await db.commit()
        await db.refresh(setting)

    return setting.setting_value


@router.put("/compreface-recognition/config", response_model=dict)
async def update_compreface_recognition_config(
    similarity_threshold: Optional[float] = Form(None),
    updated_by: Optional[str] = Form(None),
    db: AsyncSession = Depends(get_db)
):
    """Update CompreFace recognition configuration"""
    result = await db.execute(
        select(GlobalSettings).where(GlobalSettings.setting_key == "compreface_recognition")
    )
    setting = result.scalar_one_or_none()

    if not setting:
        # Auto-create if not found
        default_config = {
            "similarity_threshold": 0.88,
            "description": "Minimum similarity score (0.0-1.0) for face recognition match. Higher = stricter matching."
        }
        setting = GlobalSettings(
            setting_key="compreface_recognition",
            setting_value=default_config,
            description="CompreFace face recognition settings"
        )
        db.add(setting)

    # Get current config
    config = setting.setting_value

    # Validate and update similarity_threshold
    if similarity_threshold is not None:
        if similarity_threshold < 0.0 or similarity_threshold > 1.0:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="similarity_threshold must be between 0.0 and 1.0"
            )
        config["similarity_threshold"] = similarity_threshold

    setting.setting_value = config
    setting.updated_by = updated_by

    await db.commit()
    await db.refresh(setting)

    return {
        "setting_key": "compreface_recognition",
        "setting_value": setting.setting_value,
        "message": "CompreFace recognition configuration updated successfully"
    }
