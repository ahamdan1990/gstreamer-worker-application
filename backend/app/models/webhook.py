"""
Webhook models (placeholder for future expansion)
"""
import uuid
from datetime import datetime
from sqlalchemy import Column, String, Integer, Boolean, DateTime, Text, JSON, ForeignKey
from sqlalchemy.dialects.postgresql import UUID

from app.db.base import Base


class Webhook(Base):
    """Webhook configuration (future feature)"""

    __tablename__ = "webhooks"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    name = Column(String(255), nullable=False)
    url = Column(Text, nullable=False)
    event_types = Column(JSON)  # Array of event types
    is_active = Column(Boolean, default=True)

    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)


class WebhookDelivery(Base):
    """Webhook delivery log (future feature)"""

    __tablename__ = "webhook_deliveries"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    webhook_id = Column(UUID(as_uuid=True), ForeignKey("webhooks.id"))
    event_id = Column(UUID(as_uuid=True), ForeignKey("events.id"))
    status_code = Column(Integer)
    success = Column(Boolean)
    error_message = Column(Text)

    created_at = Column(DateTime, default=datetime.utcnow)
