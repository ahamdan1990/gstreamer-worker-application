"""
Event Pydantic schemas
"""
from typing import Optional, Dict, Any
from datetime import datetime
from pydantic import BaseModel, Field
from uuid import UUID


class MotionEventWebhook(BaseModel):
    """Motion event received from worker via webhook"""
    camera_id: str
    motion_area: int
    num_contours: int
    confidence: float
    bounding_box: Dict[str, int]  # {x, y, width, height}
    timestamp: Optional[float] = None


class EventCreate(BaseModel):
    """Schema for creating an event"""
    event_type: str = Field(..., min_length=1, max_length=50)
    camera_id: UUID
    detected_face: bool = False
    confidence: Optional[float] = None
    image_url: Optional[str] = None
    thumbnail_url: Optional[str] = None
    bounding_box: Optional[Dict[str, int]] = None
    extra_metadata: Optional[Dict[str, Any]] = None


class EventResponse(BaseModel):
    """Schema for event responses"""
    id: UUID
    event_type: str
    camera_id: UUID
    detected_face: bool
    confidence: Optional[float]
    image_url: Optional[str]
    thumbnail_url: Optional[str]
    bounding_box: Optional[Dict[str, int]]
    acknowledged: bool
    acknowledged_at: Optional[datetime]
    extra_metadata: Optional[Dict[str, Any]]
    created_at: datetime

    class Config:
        from_attributes = True
