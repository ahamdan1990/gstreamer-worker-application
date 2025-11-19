"""
Camera model
"""
import uuid
from datetime import datetime
from sqlalchemy import Column, String, Integer, Boolean, Float, DateTime, Text, JSON
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import relationship

from app.db.base import Base


class Camera(Base):
    """Camera stream configuration"""

    __tablename__ = "cameras"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    camera_id = Column(String(255), unique=True, nullable=False, index=True)
    name = Column(String(255), nullable=False)
    description = Column(Text)

    # RTSP Configuration
    rtsp_url = Column(Text, nullable=False)
    username = Column(String(255))
    password = Column(String(255))  # Encrypted
    protocols = Column(String(50), default="tcp")

    # Stream Settings
    latency_ms = Column(Integer, default=150)
    drop_on_latency = Column(Boolean, default=True)
    target_fps = Column(Integer, default=12)

    # Display Settings
    enable_display = Column(Boolean, default=True)
    display_sync = Column(Boolean, default=False)

    # Reconnection Settings
    auto_reconnect = Column(Boolean, default=True)
    max_reconnect_attempts = Column(Integer, default=-1)
    reconnect_initial_delay = Column(Float, default=1.0)
    reconnect_max_delay = Column(Float, default=60.0)
    reconnect_backoff_multiplier = Column(Float, default=2.0)

    # Health Monitoring
    health_check_interval = Column(Float, default=5.0)
    max_consecutive_errors = Column(Integer, default=5)

    # Hardware
    use_nvidia_decoder = Column(Boolean, default=True)

    # Status
    enabled = Column(Boolean, default=True)
    state = Column(String(50), default="STOPPED")  # STOPPED, STARTING, RUNNING, ERROR, RECONNECTING
    is_running = Column(Boolean, default=False)

    # Metadata
    location = Column(String(255))
    tags = Column(JSON)  # Array of tags
    extra_metadata = Column(JSON)  # Flexible metadata

    # Timestamps
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    last_seen_at = Column(DateTime)

    # Relationships
    events = relationship("Event", back_populates="camera", cascade="all, delete-orphan")

    def __repr__(self):
        return f"<Camera(camera_id={self.camera_id}, state={self.state})>"
