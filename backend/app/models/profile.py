"""
Profile model for person management and face recognition
"""
import uuid
from datetime import datetime
from sqlalchemy import Column, String, Boolean, DateTime, Text, JSON, Float, Integer, Table, ForeignKey
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import relationship

from app.db.base import Base


# Association table for many-to-many relationship between profiles and watchlists
watchlist_members = Table(
    'watchlist_members',
    Base.metadata,
    Column('id', UUID(as_uuid=True), primary_key=True, default=uuid.uuid4),
    Column('profile_id', UUID(as_uuid=True), ForeignKey('profiles.id', ondelete='CASCADE'), nullable=False),
    Column('watchlist_id', UUID(as_uuid=True), ForeignKey('watchlists.id', ondelete='CASCADE'), nullable=False),
    Column('added_at', DateTime, default=datetime.utcnow),
    Column('added_by', String(255)),  # User who added the member
    Column('notes', Text)  # Optional notes about why they're on this watchlist
)


class Profile(Base):
    """
    Profile for person management and face recognition

    Represents a person in the system with their biometric data, metadata,
    and associations with watchlists and events.
    """

    __tablename__ = "profiles"

    # Primary identification
    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    name = Column(String(255), nullable=False, index=True)
    compreface_subject_id = Column(String(255), unique=True, nullable=False, index=True)

    # Profile information
    description = Column(Text)
    email = Column(String(255))
    phone = Column(String(50))

    # Biometric data
    images = Column(JSON, default=list)  # Array of image URLs/paths
    face_encodings = Column(JSON)  # Optional: Store face encodings for faster matching

    # Status and settings
    is_active = Column(Boolean, default=True, index=True)
    auto_created = Column(Boolean, default=False)  # True if auto-created from unknown detection
    alert_on_recognition = Column(Boolean, default=False)  # Enable alerts when recognized

    # Statistics
    total_detections = Column(Integer, default=0)
    last_seen_at = Column(DateTime)
    last_seen_camera_id = Column(String(100))
    avg_recognition_similarity = Column(Float, default=0.0)

    # Metadata
    extra_metadata = Column(JSON)  # Flexible JSON for custom fields
    tags = Column(JSON, default=list)  # Array of tags for categorization

    # Audit fields
    created_at = Column(DateTime, default=datetime.utcnow, index=True)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    created_by = Column(String(255))  # User who created the profile

    # Relationships
    person_events = relationship("PersonEvent", back_populates="profile", cascade="all, delete-orphan")
    watchlists = relationship("Watchlist", secondary=watchlist_members, back_populates="profiles")
