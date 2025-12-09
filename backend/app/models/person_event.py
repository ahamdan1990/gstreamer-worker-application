"""
PersonEvent model for tracking person recognition events
"""
import uuid
from datetime import datetime
from sqlalchemy import Column, String, DateTime, Float, Boolean, JSON, ForeignKey, Integer
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import relationship

from app.db.base import Base


class PersonEvent(Base):
    """
    Person recognition event

    Tracks each time a person is recognized by the system, including
    similarity scores, location, and metadata.
    """

    __tablename__ = "person_events"

    # Primary identification
    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)

    # Foreign keys
    profile_id = Column(UUID(as_uuid=True), ForeignKey('profiles.id', ondelete='SET NULL'), index=True)
    camera_id = Column(String(100), nullable=False, index=True)

    # Recognition data
    subject = Column(String(255), nullable=False, index=True)  # CompreFace subject ID
    similarity = Column(Float, nullable=False)  # Recognition similarity (0.0-1.0)
    detection_confidence = Column(Float, nullable=False)  # Face detection confidence

    # Event metadata
    is_new_person = Column(Boolean, default=False)  # First time seeing this person
    is_new_at_camera = Column(Boolean, default=False)  # First time at this camera
    dwell_time_seconds = Column(Float, default=0.0)  # Time spent at camera
    other_cameras = Column(JSON, default=list)  # Other cameras where person was seen

    # Media
    image_url = Column(String(500))  # Face crop image URL
    thumbnail_url = Column(String(500))  # Thumbnail URL
    bounding_box = Column(JSON)  # Face bounding box coordinates

    # Alert status
    alert_sent = Column(Boolean, default=False)  # Whether an alert was sent
    alert_sent_at = Column(DateTime)  # When alert was sent
    alert_channels = Column(JSON, default=list)  # Which channels alert was sent to (webhook, email, etc.)

    # Acknowledged status
    acknowledged = Column(Boolean, default=False, index=True)
    acknowledged_at = Column(DateTime)
    acknowledged_by = Column(String(255))  # User who acknowledged

    # Metadata
    extra_metadata = Column(JSON)  # Flexible JSON for custom fields
    worker_metadata = Column(JSON)  # Metadata from C++ worker (PersonRecognitionEvent data)

    # Audit
    created_at = Column(DateTime, default=datetime.utcnow, index=True)

    # Relationships
    profile = relationship("Profile", back_populates="person_events")
