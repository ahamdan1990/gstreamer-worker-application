"""
Profile model (placeholder for future expansion)
"""
import uuid
from datetime import datetime
from sqlalchemy import Column, String, Boolean, DateTime, Text, JSON
from sqlalchemy.dialects.postgresql import UUID

from app.db.base import Base


class Profile(Base):
    """Profile for face recognition (future feature)"""

    __tablename__ = "profiles"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    name = Column(String(255), nullable=False)
    compreface_subject_id = Column(String(255), unique=True)
    description = Column(Text)
    images = Column(JSON)  # Array of image URLs
    is_active = Column(Boolean, default=True)
    extra_metadata = Column(JSON)

    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
