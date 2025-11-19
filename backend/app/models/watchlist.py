"""
Watchlist models (placeholder for future expansion)
"""
import uuid
from datetime import datetime
from sqlalchemy import Column, String, Integer, Boolean, DateTime, Text, ForeignKey
from sqlalchemy.dialects.postgresql import UUID

from app.db.base import Base


class Watchlist(Base):
    """Watchlist (future feature)"""

    __tablename__ = "watchlists"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    name = Column(String(255), nullable=False)
    description = Column(Text)
    priority = Column(Integer, default=0)
    is_active = Column(Boolean, default=True)

    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)


class WatchlistProfile(Base):
    """Many-to-many relationship (future feature)"""

    __tablename__ = "watchlist_profiles"

    watchlist_id = Column(UUID(as_uuid=True), ForeignKey("watchlists.id"), primary_key=True)
    profile_id = Column(UUID(as_uuid=True), ForeignKey("profiles.id"), primary_key=True)
    added_at = Column(DateTime, default=datetime.utcnow)
