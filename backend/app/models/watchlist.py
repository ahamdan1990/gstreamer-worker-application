"""
Watchlist model for person monitoring
"""
import uuid
from datetime import datetime
from sqlalchemy import Column, String, Boolean, DateTime, Text, JSON, Integer
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import relationship

from app.db.base import Base
from app.models.profile import watchlist_members


class Watchlist(Base):
    """
    Watchlist for monitoring specific groups of people

    Watchlists allow organizing profiles into groups (e.g., "VIP Guests",
    "Security Threats", "Employees") with custom alert settings.
    """

    __tablename__ = "watchlists"

    # Primary identification
    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    name = Column(String(255), nullable=False, unique=True, index=True)
    description = Column(Text)

    # Color/icon for UI display
    color = Column(String(7), default="#3B82F6")  # Hex color code
    icon = Column(String(50))  # Icon name/emoji

    # Alert settings
    is_active = Column(Boolean, default=True, index=True)
    enable_alerts = Column(Boolean, default=True)  # Send alerts when members are detected
    alert_on_cameras = Column(JSON, default=list)  # Specific camera IDs to alert on (empty = all)
    alert_cooldown_minutes = Column(Integer, default=10)  # Min time between alerts for same person

    # Webhook settings
    webhook_url = Column(String(500))  # Custom webhook for this watchlist
    webhook_enabled = Column(Boolean, default=False)

    # Statistics
    member_count = Column(Integer, default=0)  # Cached count of members
    total_detections = Column(Integer, default=0)  # Total detections across all members

    # Metadata
    extra_metadata = Column(JSON)  # Flexible JSON for custom fields
    priority = Column(Integer, default=0)  # Priority for alert ordering (higher = more important)

    # Audit fields
    created_at = Column(DateTime, default=datetime.utcnow, index=True)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    created_by = Column(String(255))  # User who created the watchlist

    # Relationships
    profiles = relationship("Profile", secondary=watchlist_members, back_populates="watchlists")
