"""
Camera Pydantic schemas
"""
from typing import Optional, Dict, Any
from datetime import datetime
from pydantic import BaseModel, Field
from uuid import UUID


class MotionDetectionConfig(BaseModel):
    """Motion detection configuration schema"""
    enabled: bool = False
    algorithm: str = "MOG2_CUDA"  # MOG2_CUDA, MOG2, KNN, FRAME_DIFF
    sensitivity: float = Field(0.5, ge=0.0, le=1.0)
    min_contour_area: int = Field(500, ge=0)
    max_contours: int = Field(50, ge=1)
    frame_skip: int = Field(2, ge=0)
    blur_size: int = Field(21, ge=1)  # Must be odd
    history: int = Field(500, ge=1)
    var_threshold: float = Field(16.0, ge=0.0)
    detect_shadows: bool = False
    cooldown_seconds: float = Field(1.0, ge=0.0)
    required_frames: int = Field(3, ge=1)
    max_frame_width: int = Field(640, ge=0)
    max_frame_height: int = Field(480, ge=0)
    roi: Optional[Dict[str, int]] = None  # {x, y, width, height}


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
    motion_detection: Optional[MotionDetectionConfig] = None


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
    motion_detection: Optional[MotionDetectionConfig] = None


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


class MotionDetectionMetrics(BaseModel):
    """Motion detection metrics"""
    frames_analyzed: int = 0
    motion_events_detected: int = 0
    motion_detection_fps: float = 0.0
    last_motion_timestamp: Optional[float] = None


class CameraMetrics(BaseModel):
    """Camera performance metrics"""
    uptime_seconds: float = 0.0
    errors_count: int = 0
    reconnections: int = 0
    frames_displayed: int = 0
    motion: Optional[MotionDetectionMetrics] = None


class CameraStatus(BaseModel):
    """Camera status response"""
    camera_id: str
    state: str
    is_running: bool
    last_seen_at: Optional[datetime] = None
    metrics: Optional[CameraMetrics] = None


class CameraControl(BaseModel):
    """Camera control action"""
    action: str  # 'start', 'stop', 'restart'
