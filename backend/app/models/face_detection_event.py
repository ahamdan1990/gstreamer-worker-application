"""
FaceDetectionEvent model for tracking face detection events with tracking IDs
"""
import uuid
from datetime import datetime
from sqlalchemy import Column, String, DateTime, Float, Boolean, JSON, Integer
from sqlalchemy.dialects.postgresql import UUID

from app.db.base import Base


class FaceDetectionEvent(Base):
    """
    Face detection event with tracking information

    Tracks each face detection with a unique tracking ID throughout its lifecycle.
    Includes tracking duration, recognition status, and attempts.
    """

    __tablename__ = "face_detection_events"

    # Primary identification
    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)

    # Tracking information
    tracking_id = Column(String(255), nullable=False, index=True)  # Unique tracking ID from C++
    camera_id = Column(String(100), nullable=False, index=True)

    # Detection data
    confidence = Column(Float, nullable=False)  # Detection confidence (0.0-1.0)
    bounding_box = Column(JSON)  # Face bounding box coordinates

    # Recognition data
    subject = Column(String(255), default="unknown", index=True)  # Recognized person name/ID
    is_recognized = Column(Boolean, default=False, index=True)  # Whether face is recognized
    recognition_attempts = Column(Integer, default=0)  # Number of recognition attempts (max 3)

    # Tracking state
    tracking_duration_seconds = Column(Float, default=0.0)  # How long face has been tracked
    is_first_detection = Column(Boolean, default=True)  # Whether this is first detection of tracking session

    # Media
    image_url = Column(String(500))  # Face crop image URL
    thumbnail_url = Column(String(500))  # Thumbnail URL

    # Metadata
    extra_metadata = Column(JSON)  # Flexible JSON for custom fields
    worker_metadata = Column(JSON)  # Metadata from C++ worker (FaceEvent data)

    # Audit
    created_at = Column(DateTime, default=datetime.utcnow, index=True)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)

    def __repr__(self):
        return f"<FaceDetectionEvent(tracking_id={self.tracking_id}, subject={self.subject}, camera_id={self.camera_id})>"
