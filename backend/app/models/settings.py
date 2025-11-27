"""
Global Settings model for system-wide configuration
"""
import uuid
from datetime import datetime
from sqlalchemy import Column, String, Integer, Boolean, Float, DateTime, Text, JSON
from sqlalchemy.dialects.postgresql import UUID

from app.db.base import Base


class GlobalSettings(Base):
    """
    Global system settings for manager and person tracking configuration.
    This stores system-wide settings that apply across all cameras.
    """

    __tablename__ = "global_settings"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    setting_key = Column(String(255), unique=True, nullable=False, index=True)  # e.g., "person_tracking", "manager"
    setting_value = Column(JSON, nullable=False)  # The configuration as JSON
    description = Column(Text)  # Human-readable description

    # Metadata
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    updated_by = Column(String(255))  # User who last updated

    def __repr__(self):
        return f"<GlobalSettings(key={self.setting_key})>"
