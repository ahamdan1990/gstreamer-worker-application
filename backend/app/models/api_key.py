"""
API Key model
"""
import uuid
from datetime import datetime
from sqlalchemy import Column, String, Boolean, DateTime, JSON, ForeignKey
from sqlalchemy.dialects.postgresql import UUID

from app.db.base import Base


class APIKey(Base):
    """API Keys for authentication"""

    __tablename__ = "api_keys"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    user_id = Column(UUID(as_uuid=True), ForeignKey("users.id"))
    key_name = Column(String(255))
    api_key = Column(String(255), unique=True, nullable=False, index=True)
    permissions = Column(JSON)
    is_active = Column(Boolean, default=True)
    last_used_at = Column(DateTime)
    expires_at = Column(DateTime)

    created_at = Column(DateTime, default=datetime.utcnow)
