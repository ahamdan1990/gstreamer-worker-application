"""
Camera Pydantic schemas
"""
from typing import Optional, Dict, Any
from datetime import datetime
from pydantic import BaseModel, Field
from uuid import UUID


class CameraBase(BaseModel):
    camera_id: str = Field(..., min_length=1, max_length=255)
    name: str = Field(..., min_length=1, max_length=255)
    description: Optional[str] = None
    rtsp_url: str = Field(..., min_length=1)
    username: Optional[str] = None
    password: Optional[str] = None
    protocols: str = "tcp"
    latency_ms: int = Field(150, ge=0, le=5000)
    drop_on_latency: bool = True
    target_fps: int = Field(12, ge=1, le=60)
    enable_display: bool = True
    display_sync: bool = False
    auto_reconnect: bool = True
    max_reconnect_attempts: int = -1
    use_nvidia_decoder: bool = True
    enabled: bool = True
    location: Optional[str] = None
    tags: Optional[Dict[str, Any]] = None
    extra_metadata: Optional[Dict[str, Any]] = None


class CameraCreate(CameraBase):
    """Schema for creating a camera"""
    pass


class CameraUpdate(BaseModel):
    """Schema for updating a camera (all fields optional)"""
    name: Optional[str] = None
    description: Optional[str] = None
    rtsp_url: Optional[str] = None
    username: Optional[str] = None
    password: Optional[str] = None
    protocols: Optional[str] = None
    latency_ms: Optional[int] = None
    drop_on_latency: Optional[bool] = None
    target_fps: Optional[int] = None
    enable_display: Optional[bool] = None
    enabled: Optional[bool] = None
    location: Optional[str] = None
    tags: Optional[Dict[str, Any]] = None
    extra_metadata: Optional[Dict[str, Any]] = None


class CameraResponse(CameraBase):
    """Schema for camera responses"""
    id: UUID
    state: str
    is_running: bool
    created_at: datetime
    updated_at: datetime
    last_seen_at: Optional[datetime] = None

    class Config:
        from_attributes = True


class CameraStatus(BaseModel):
    """Camera status response"""
    camera_id: str
    state: str
    is_running: bool
    last_seen_at: Optional[datetime] = None


class CameraControl(BaseModel):
    """Camera control action"""
    action: str  # 'start', 'stop', 'restart'
