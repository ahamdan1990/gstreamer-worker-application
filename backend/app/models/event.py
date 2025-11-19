"""
Event model
"""
import uuid
from datetime import datetime
from sqlalchemy import Column, String, Float, Boolean, DateTime, Text, JSON, ForeignKey
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import relationship

from app.db.base import Base


class Event(Base):
    """Recognition and detection events"""

    __tablename__ = "events"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    event_type = Column(String(50), nullable=False, index=True)  # 'face_detected', 'person_recognized', etc.

    # Source
    camera_id = Column(UUID(as_uuid=True), ForeignKey("cameras.id"), nullable=False, index=True)

    # Recognition (for MVP, simplified)
    detected_face = Column(Boolean, default=True)
    confidence = Column(Float)

    # Event Data
    image_url = Column(Text)
    thumbnail_url = Column(Text)
    bounding_box = Column(JSON)  # {x, y, width, height}

    # Status
    acknowledged = Column(Boolean, default=False)
    acknowledged_at = Column(DateTime)

    # Metadata
    extra_metadata = Column(JSON)

    created_at = Column(DateTime, default=datetime.utcnow, index=True)

    # Relationships
    camera = relationship("Camera", back_populates="events")

    def __repr__(self):
        return f"<Event(type={self.event_type}, camera_id={self.camera_id})>"
