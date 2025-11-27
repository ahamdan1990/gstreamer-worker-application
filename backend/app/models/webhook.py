"""
Webhook model for third-party integrations
"""
import uuid
from datetime import datetime
from sqlalchemy import Column, String, Boolean, DateTime, Text, JSON, Integer
from sqlalchemy.dialects.postgresql import UUID

from app.db.base import Base


class Webhook(Base):
    """
    Webhook configuration for sending events to external systems

    Allows configuring HTTP callbacks to notify external systems about
    face detection, person recognition, motion, and other events.
    """

    __tablename__ = "webhooks"

    # Primary identification
    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    name = Column(String(255), nullable=False)
    description = Column(Text)

    # Webhook configuration
    url = Column(String(500), nullable=False)  # Target URL
    method = Column(String(10), default="POST")  # HTTP method (POST, PUT, etc.)
    is_active = Column(Boolean, default=True, index=True)

    # Authentication
    auth_type = Column(String(50))  # none, bearer, basic, api_key, custom
    auth_header = Column(String(100))  # Header name for authentication
    auth_value = Column(Text)  # Authentication value (encrypted in production)

    # Event filtering
    event_types = Column(JSON, default=list)  # Which events to send (person_recognized, face_detected, motion, etc.)
    camera_ids = Column(JSON, default=list)  # Filter by specific cameras (empty = all)
    watchlist_ids = Column(JSON, default=list)  # Filter by specific watchlists (empty = all)

    # Request settings
    headers = Column(JSON)  # Custom HTTP headers
    timeout_seconds = Column(Integer, default=10)  # Request timeout
    retry_enabled = Column(Boolean, default=True)  # Enable automatic retries
    retry_max_attempts = Column(Integer, default=3)  # Max retry attempts
    retry_backoff_seconds = Column(Integer, default=5)  # Backoff between retries

    # Payload template
    payload_template = Column(JSON)  # Custom JSON template for payload

    # Statistics
    total_calls = Column(Integer, default=0)  # Total webhook calls made
    successful_calls = Column(Integer, default=0)  # Successful responses
    failed_calls = Column(Integer, default=0)  # Failed responses
    last_called_at = Column(DateTime)  # Last time webhook was called
    last_status_code = Column(Integer)  # Last HTTP status code
    last_error = Column(Text)  # Last error message

    # Metadata
    extra_metadata = Column(JSON)  # Flexible JSON for custom fields

    # Audit fields
    created_at = Column(DateTime, default=datetime.utcnow, index=True)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    created_by = Column(String(255))  # User who created the webhook
