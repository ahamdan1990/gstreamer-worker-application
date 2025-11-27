# Enterprise Person Tracking & Management System - Implementation Plan

## 🎯 Overview

Complete enterprise-grade person tracking system with profile management, watchlists, events, webhooks, and real-time alerts integrated across the full stack (C++ Worker → FastAPI → Next.js).

---

## 📊 Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Next.js Frontend (Port 3000)                  │
│  ┌──────────────┬──────────────┬──────────────┬──────────────────┐ │
│  │   Camera     │   Profiles   │  Watchlists  │     Events       │ │
│  │   Settings   │  Management  │  Management  │    History       │ │
│  └──────────────┴──────────────┴──────────────┴──────────────────┘ │
│  ┌──────────────┬──────────────┬──────────────┬──────────────────┐ │
│  │   Webhooks   │    Alerts    │  Statistics  │   Real-time      │ │
│  │    Config    │  Notif System│  Dashboard   │   Updates (WS)   │ │
│  └──────────────┴──────────────┴──────────────┴──────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
                                    ↓ HTTP/WebSocket
┌─────────────────────────────────────────────────────────────────────┐
│                    FastAPI Middleware (Port 8002)                    │
│  ┌──────────────┬──────────────┬──────────────┬──────────────────┐ │
│  │   Camera     │   Profile    │  Watchlist   │     Event        │ │
│  │   Config API │   CRUD API   │   CRUD API   │   History API    │ │
│  └──────────────┴──────────────┴──────────────┴──────────────────┘ │
│  ┌──────────────┬──────────────┬──────────────┬──────────────────┐ │
│  │   Webhook    │    Event     │   Auto       │   PostgreSQL     │ │
│  │   Dispatch   │   Handlers   │   Profile    │   Database       │ │
│  │   Worker     │   (WS relay) │   Creation   │   Persistence    │ │
│  └──────────────┴──────────────┴──────────────┴──────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
                                    ↓ HTTP/WebSocket
┌─────────────────────────────────────────────────────────────────────┐
│              C++ Worker (Port 8081 HTTP, 8082 WS)                    │
│  ┌──────────────┬──────────────┬──────────────┬──────────────────┐ │
│  │ PersonTracker│  FaceDetector│   Profile    │    Watchlist     │ │
│  │  Core Logic  │  Recognition │   Storage    │    Matching      │ │
│  └──────────────┴──────────────┴──────────────┴──────────────────┘ │
│  ┌──────────────┬──────────────┬──────────────┬──────────────────┐ │
│  │   Camera     │    Event     │   Webhook    │    Statistics    │ │
│  │   Config API │  Broadcasting│   Triggers   │   Aggregation    │ │
│  └──────────────┴──────────────┴──────────────┴──────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 🔧 Phase 1: C++ Worker (Backend Core)

### 1.1 PersonTracker API Endpoints

**Add to `api_server.cpp`:**

```cpp
// ========================================
// Person Tracking Endpoints
// ========================================

// GET /api/persons - List all tracked persons
server.Get("/api/persons", [this](const Request& req, Response& res) {
    if (!person_tracker_) {
        res.status = 503;
        res.set_content("{\"error\": \"Person tracking not enabled\"}", "application/json");
        return;
    }

    auto persons = person_tracker_->get_all_persons();
    json persons_array = json::array();

    for (const auto& person : persons) {
        json person_json;
        person_json["subject"] = person.subject;
        person_json["last_similarity"] = person.last_similarity;
        person_json["first_seen_global"] = system_clock::to_time_t(person.first_seen_global);
        person_json["last_seen_global"] = system_clock::to_time_t(person.last_seen_global);
        person_json["total_detections"] = person.total_detections;
        person_json["avg_similarity"] = person.avg_similarity;

        json cameras = json::array();
        for (const auto& [cam_id, appearance] : person.camera_appearances) {
            json cam_json;
            cam_json["camera_id"] = cam_id;
            cam_json["first_seen"] = system_clock::to_time_t(appearance.first_seen);
            cam_json["last_seen"] = system_clock::to_time_t(appearance.last_seen);
            cam_json["detection_count"] = appearance.detection_count;
            cam_json["dwell_time_seconds"] = appearance.dwell_time_seconds();
            cameras.push_back(cam_json);
        }
        person_json["camera_appearances"] = cameras;
        person_json["total_dwell_time"] = person.total_dwell_time_seconds();

        persons_array.push_back(person_json);
    }

    json response;
    response["persons"] = persons_array;
    response["count"] = persons_array.size();
    res.set_content(response.dump(), "application/json");
});

// GET /api/persons/:subject - Get specific person
server.Get("/api/persons/:subject", [this](const Request& req, Response& res) {
    if (!person_tracker_) {
        res.status = 503;
        res.set_content("{\"error\": \"Person tracking not enabled\"}", "application/json");
        return;
    }

    std::string subject = req.path_params.at("subject");
    auto person_opt = person_tracker_->get_person_presence(subject);

    if (!person_opt.has_value()) {
        res.status = 404;
        res.set_content("{\"error\": \"Person not found\"}", "application/json");
        return;
    }

    // ... serialize person ...
    res.set_content(person_json.dump(), "application/json");
});

// GET /api/persons/camera/:camera_id - Get persons at camera
server.Get("/api/persons/camera/:camera_id", [this](const Request& req, Response& res) {
    if (!person_tracker_) {
        res.status = 503;
        res.set_content("{\"error\": \"Person tracking not enabled\"}", "application/json");
        return;
    }

    std::string camera_id = req.path_params.at("camera_id");
    double max_age = 60.0;  // Default 60 seconds

    if (req.has_param("max_age")) {
        max_age = std::stod(req.get_param_value("max_age"));
    }

    auto persons = person_tracker_->get_persons_at_camera(camera_id, max_age);
    // ... serialize ...
});

// GET /api/persons/cross-camera - Get cross-camera persons
server.Get("/api/persons/cross-camera", [this](const Request& req, Response& res) {
    if (!person_tracker_) {
        res.status = 503;
        res.set_content("{\"error\": \"Person tracking not enabled\"}", "application/json");
        return;
    }

    auto persons = person_tracker_->get_cross_camera_persons();
    // ... serialize ...
});

// GET /api/tracking/stats - Get tracking statistics
server.Get("/api/tracking/stats", [this](const Request& req, Response& res) {
    if (!person_tracker_) {
        res.status = 503;
        res.set_content("{\"error\": \"Person tracking not enabled\"}", "application/json");
        return;
    }

    auto stats = person_tracker_->get_statistics();

    json stats_json;
    stats_json["total_persons_tracked"] = stats.total_persons_tracked;
    stats_json["currently_tracked"] = stats.currently_tracked;
    stats_json["cross_camera_persons"] = stats.cross_camera_persons;
    stats_json["total_events"] = stats.total_events;
    stats_json["avg_dwell_time_seconds"] = stats.avg_dwell_time_seconds;

    res.set_content(stats_json.dump(), "application/json");
});

// POST /api/tracking/reset - Trigger daily reset
server.Post("/api/tracking/reset", [this](const Request& req, Response& res) {
    if (!person_tracker_) {
        res.status = 503;
        res.set_content("{\"error\": \"Person tracking not enabled\"}", "application/json");
        return;
    }

    bool success = person_tracker_->perform_daily_reset();

    json response;
    response["success"] = success;
    response["message"] = success ? "Daily reset performed successfully" : "Daily reset failed";
    res.set_content(response.dump(), "application/json");
});

// GET /api/tracking/config - Get tracking config
server.Get("/api/tracking/config", [this](const Request& req, Response& res) {
    // Return current PersonTrackerConfig as JSON
});

// POST /api/tracking/config - Update tracking config
server.Post("/api/tracking/config", [this](const Request& req, Response& res) {
    // Parse JSON, update config, call person_tracker_->update_config()
});
```

### 1.2 Camera Configuration API

```cpp
// GET /api/cameras/:id/config - Get full camera configuration
server.Get("/api/cameras/:id/config", [this](const Request& req, Response& res) {
    std::string camera_id = req.path_params.at("id");

    // Get camera config from manager
    auto config = manager_->get_camera_config(camera_id);
    if (!config) {
        res.status = 404;
        return;
    }

    json config_json;
    // Serialize ALL settings from CameraConfig:
    config_json["camera_id"] = config->camera_id;
    config_json["rtsp_url"] = config->rtsp_url;
    config_json["username"] = config->username;
    config_json["password"] = "********";  // Mask password
    config_json["protocols"] = config->protocols;
    config_json["latency_ms"] = config->latency_ms;
    config_json["target_fps"] = config->target_fps;
    config_json["use_nvidia_decoder"] = config->use_nvidia_decoder;

    // Motion detection config
    json motion_json;
    motion_json["enabled"] = config->motion_detection.enabled;
    motion_json["algorithm"] = motion_algorithm_to_string(config->motion_detection.algorithm);
    motion_json["sensitivity"] = config->motion_detection.sensitivity;
    motion_json["min_contour_area"] = config->motion_detection.min_contour_area;
    motion_json["frame_skip"] = config->motion_detection.frame_skip;
    // ... all motion settings ...
    config_json["motion_detection"] = motion_json;

    // Face detection config
    json face_json;
    face_json["enabled"] = config->face_detection.enabled;
    face_json["model_path"] = config->face_detection.model_path;
    face_json["confidence_threshold"] = config->face_detection.confidence_threshold;
    face_json["frame_skip"] = config->face_detection.frame_skip;
    face_json["enable_compreface"] = config->face_detection.enable_compreface;
    face_json["compreface_url"] = config->face_detection.compreface_url;
    // ... all face detection settings ...
    config_json["face_detection"] = face_json;

    res.set_content(config_json.dump(), "application/json");
});

// PUT /api/cameras/:id/config - Update camera configuration
server.Put("/api/cameras/:id/config", [this](const Request& req, Response& res) {
    std::string camera_id = req.path_params.at("id");

    try {
        json config_json = json::parse(req.body);

        // Parse and validate configuration
        CameraConfig new_config;
        // Parse all fields from JSON...

        // Update camera config
        bool success = manager_->update_camera_config(camera_id, new_config);

        json response;
        response["success"] = success;
        res.set_content(response.dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 400;
        json error;
        error["error"] = e.what();
        res.set_content(error.dump(), "application/json");
    }
});
```

### 1.3 Event Broadcasting Enhancement

**Add to `event_broadcaster.cpp`:**

```cpp
void EventBroadcaster::broadcast_person_recognized(const PersonRecognitionEvent& event) {
    json j;
    j["event_type"] = "person_recognized";
    j["timestamp"] = std::chrono::system_clock::to_time_t(event.timestamp);
    j["camera_id"] = event.camera_id;

    json data;
    data["subject"] = event.subject;
    data["similarity"] = event.similarity;
    data["detection_confidence"] = event.detection_confidence;
    data["is_new_person"] = event.is_new_person;
    data["is_new_at_camera"] = event.is_new_at_camera;
    data["dwell_time_seconds"] = event.dwell_time_seconds;
    data["other_cameras"] = event.other_cameras;

    j["data"] = data;

    // Broadcast via WebSocket
    if (ws_server_) {
        ws_server_->broadcast(j.dump());
    }
}

void EventBroadcaster::broadcast_face_detected(
    const std::string& camera_id,
    const FaceDetection& detection,
    bool is_unknown
) {
    json j;
    j["event_type"] = is_unknown ? "face_detected_unknown" : "face_detected";
    j["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
    j["camera_id"] = camera_id;

    json data;
    data["confidence"] = detection.confidence;
    data["bbox"] = {
        {"x", detection.bbox.x},
        {"y", detection.bbox.y},
        {"width", detection.bbox.width},
        {"height", detection.bbox.height}
    };
    // Add face crop path if saved

    j["data"] = data;

    if (ws_server_) {
        ws_server_->broadcast(j.dump());
    }
}
```

---

## 📦 Phase 2: FastAPI Middleware

### 2.1 Database Models

**`app/models/profile.py`:**

```python
from sqlalchemy import Column, String, DateTime, Float, Boolean, JSON, Text
from sqlalchemy.dialects.postgresql import UUID
import uuid
from datetime import datetime

class Profile(Base):
    """Person profile for face recognition"""
    __tablename__ = "profiles"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    subject = Column(String(255), unique=True, nullable=False, index=True)
    full_name = Column(String(255), nullable=True)
    email = Column(String(255), nullable=True)
    phone = Column(String(50), nullable=True)

    # Profile photo
    photo_url = Column(String(500), nullable=True)
    thumbnail_url = Column(String(500), nullable=True)

    # Metadata
    employee_id = Column(String(100), nullable=True)
    department = Column(String(100), nullable=True)
    access_level = Column(String(50), nullable=True)
    notes = Column(Text, nullable=True)
    metadata = Column(JSON, nullable=True)  # Flexible metadata

    # Tracking
    is_auto_created = Column(Boolean, default=False)
    first_seen = Column(DateTime, nullable=True)
    last_seen = Column(DateTime, nullable=True)
    total_detections = Column(Integer, default=0)

    # Watchlist
    watchlist_ids = Column(JSON, nullable=True)  # Array of watchlist IDs

    # Status
    is_active = Column(Boolean, default=True)
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)

    # Relationships
    events = relationship("FaceEvent", back_populates="profile")
```

**`app/models/watchlist.py`:**

```python
class Watchlist(Base):
    """Watchlist for person alerts"""
    __tablename__ = "watchlists"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    name = Column(String(255), nullable=False)
    description = Column(Text, nullable=True)

    # Alert settings
    alert_enabled = Column(Boolean, default=True)
    alert_type = Column(String(50), default="info")  # info, warning, critical
    notification_channels = Column(JSON, nullable=True)  # email, webhook, sms

    # Webhook config for this watchlist
    webhook_url = Column(String(500), nullable=True)
    webhook_enabled = Column(Boolean, default=False)

    # Status
    is_active = Column(Boolean, default=True)
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
```

**`app/models/face_event.py`:**

```python
class FaceEvent(Base):
    """Face detection/recognition event"""
    __tablename__ = "face_events"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    camera_id = Column(UUID(as_uuid=True), ForeignKey("cameras.id"), nullable=False)
    profile_id = Column(UUID(as_uuid=True), ForeignKey("profiles.id"), nullable=True)

    # Event type
    event_type = Column(String(50), nullable=False)  # face_detected, person_recognized

    # Detection data
    confidence = Column(Float, nullable=False)
    similarity = Column(Float, nullable=True)  # For recognized faces

    # Bounding box
    bbox_x = Column(Float, nullable=True)
    bbox_y = Column(Float, nullable=True)
    bbox_width = Column(Float, nullable=True)
    bbox_height = Column(Float, nullable=True)

    # Face crop image
    face_crop_path = Column(String(500), nullable=True)
    face_thumbnail_path = Column(String(500), nullable=True)

    # Person tracking data (from PersonTracker)
    is_new_person = Column(Boolean, default=False)
    is_new_at_camera = Column(Boolean, default=False)
    dwell_time_seconds = Column(Float, nullable=True)
    other_cameras = Column(JSON, nullable=True)  # Array of camera IDs

    # Watchlist match
    watchlist_match = Column(Boolean, default=False)
    matched_watchlists = Column(JSON, nullable=True)  # Array of watchlist IDs

    # Metadata
    metadata = Column(JSON, nullable=True)

    # Timestamps
    detected_at = Column(DateTime, nullable=False, default=datetime.utcnow, index=True)
    created_at = Column(DateTime, default=datetime.utcnow)

    # Relationships
    camera = relationship("Camera", back_populates="face_events")
    profile = relationship("Profile", back_populates="events")
```

**`app/models/webhook.py`:**

```python
class Webhook(Base):
    """Webhook configuration for event notifications"""
    __tablename__ = "webhooks"

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    name = Column(String(255), nullable=False)
    url = Column(String(500), nullable=False)

    # Authentication
    auth_type = Column(String(50), default="none")  # none, bearer, basic, api_key
    auth_token = Column(String(500), nullable=True)
    auth_header_name = Column(String(100), nullable=True)

    # Event filtering
    event_types = Column(JSON, nullable=False)  # ["person_recognized", "face_detected_unknown"]
    camera_ids = Column(JSON, nullable=True)  # Filter by specific cameras
    watchlist_only = Column(Boolean, default=False)  # Only send watchlist matches

    # Retry configuration
    retry_enabled = Column(Boolean, default=True)
    max_retries = Column(Integer, default=3)
    retry_delay_seconds = Column(Integer, default=60)

    # Status
    is_active = Column(Boolean, default=True)
    last_triggered_at = Column(DateTime, nullable=True)
    last_success_at = Column(DateTime, nullable=True)
    last_failure_at = Column(DateTime, nullable=True)
    failure_count = Column(Integer, default=0)

    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
```

### 2.2 API Routes

**`app/api/routes/profiles.py`:**

```python
@router.get("/profiles", response_model=List[ProfileResponse])
async def list_profiles(
    skip: int = 0,
    limit: int = 100,
    watchlist_id: Optional[UUID] = None,
    is_active: Optional[bool] = None,
    db: AsyncSession = Depends(get_db)
):
    """List all profiles with filtering"""
    query = select(Profile)

    if watchlist_id:
        query = query.filter(Profile.watchlist_ids.contains([str(watchlist_id)]))
    if is_active is not None:
        query = query.filter(Profile.is_active == is_active)

    query = query.offset(skip).limit(limit)
    result = await db.execute(query)
    profiles = result.scalars().all()

    return profiles

@router.post("/profiles", response_model=ProfileResponse)
async def create_profile(
    profile_create: ProfileCreate,
    db: AsyncSession = Depends(get_db),
    worker: WorkerClient = Depends(get_worker_client)
):
    """Create new profile"""
    # Create in database
    profile = Profile(**profile_create.dict())
    db.add(profile)
    await db.commit()
    await db.refresh(profile)

    # Register with CompreFace if photo provided
    if profile.photo_url:
        # Call CompreFace API to add subject
        pass

    return profile

@router.post("/profiles/auto-create", response_model=ProfileResponse)
async def auto_create_profile_from_unknown(
    face_event_id: UUID,
    profile_data: ProfileCreate,
    db: AsyncSession = Depends(get_db)
):
    """Auto-create profile from unknown face detection"""
    # Get face event
    event = await db.get(FaceEvent, face_event_id)
    if not event or event.profile_id:
        raise HTTPException(status_code=400, detail="Invalid event or already has profile")

    # Create profile
    profile = Profile(**profile_data.dict())
    profile.is_auto_created = True
    profile.photo_url = event.face_crop_path
    db.add(profile)

    # Link event to profile
    event.profile_id = profile.id

    await db.commit()

    return profile
```

**`app/api/routes/events.py`:**

```python
@router.get("/events", response_model=List[FaceEventResponse])
async def list_face_events(
    skip: int = 0,
    limit: int = 100,
    camera_id: Optional[UUID] = None,
    profile_id: Optional[UUID] = None,
    event_type: Optional[str] = None,
    start_date: Optional[datetime] = None,
    end_date: Optional[datetime] = None,
    db: AsyncSession = Depends(get_db)
):
    """List face detection/recognition events with filtering"""
    query = select(FaceEvent).order_by(FaceEvent.detected_at.desc())

    if camera_id:
        query = query.filter(FaceEvent.camera_id == camera_id)
    if profile_id:
        query = query.filter(FaceEvent.profile_id == profile_id)
    if event_type:
        query = query.filter(FaceEvent.event_type == event_type)
    if start_date:
        query = query.filter(FaceEvent.detected_at >= start_date)
    if end_date:
        query = query.filter(FaceEvent.detected_at <= end_date)

    query = query.offset(skip).limit(limit)
    result = await db.execute(query)
    events = result.scalars().all()

    return events

@router.get("/events/stats", response_model=EventStats)
async def get_event_statistics(
    camera_id: Optional[UUID] = None,
    start_date: Optional[datetime] = None,
    db: AsyncSession = Depends(get_db)
):
    """Get event statistics"""
    # Aggregate stats from database
    # Total events, unique persons, watchlist matches, etc.
    pass
```

**`app/api/routes/webhooks.py`:**

```python
@router.post("/webhooks", response_model=WebhookResponse)
async def create_webhook(
    webhook_create: WebhookCreate,
    db: AsyncSession = Depends(get_db)
):
    """Create webhook configuration"""
    webhook = Webhook(**webhook_create.dict())
    db.add(webhook)
    await db.commit()
    await db.refresh(webhook)
    return webhook

@router.post("/webhooks/{webhook_id}/test", response_model=WebhookTestResult)
async def test_webhook(
    webhook_id: UUID,
    db: AsyncSession = Depends(get_db)
):
    """Test webhook by sending a test event"""
    webhook = await db.get(Webhook, webhook_id)
    if not webhook:
        raise HTTPException(status_code=404, detail="Webhook not found")

    # Send test event
    test_event = {
        "event_type": "test",
        "timestamp": datetime.utcnow().isoformat(),
        "data": {"message": "This is a test event"}
    }

    success = await dispatch_webhook(webhook, test_event)
    return {"success": success}
```

### 2.3 Webhook Dispatch Worker

**`app/services/webhook_dispatcher.py`:**

```python
import httpx
from typing import Dict, Any
from app.models.webhook import Webhook

class WebhookDispatcher:
    def __init__(self):
        self.client = httpx.AsyncClient(timeout=30.0)

    async def dispatch(self, webhook: Webhook, event: Dict[str, Any]) -> bool:
        """Dispatch event to webhook URL"""
        try:
            headers = {"Content-Type": "application/json"}

            # Add authentication
            if webhook.auth_type == "bearer":
                headers["Authorization"] = f"Bearer {webhook.auth_token}"
            elif webhook.auth_type == "api_key" and webhook.auth_header_name:
                headers[webhook.auth_header_name] = webhook.auth_token

            # Send request
            response = await self.client.post(
                webhook.url,
                json=event,
                headers=headers
            )

            response.raise_for_status()
            return True

        except Exception as e:
            logger.error(f"Webhook dispatch failed: {e}")
            return False

    async def dispatch_with_retry(self, webhook: Webhook, event: Dict[str, Any]):
        """Dispatch with retry logic"""
        if not webhook.is_active:
            return

        for attempt in range(webhook.max_retries + 1):
            success = await self.dispatch(webhook, event)
            if success:
                # Update webhook stats
                webhook.last_triggered_at = datetime.utcnow()
                webhook.last_success_at = datetime.utcnow()
                webhook.failure_count = 0
                break
            else:
                webhook.failure_count += 1
                webhook.last_failure_at = datetime.utcnow()

                if attempt < webhook.max_retries:
                    await asyncio.sleep(webhook.retry_delay_seconds)
```

### 2.4 Event Handler Integration

**`app/services/event_handlers.py`:**

```python
async def handle_person_recognized(event_data: Dict[str, Any], db: AsyncSession):
    """Handle person_recognized event from worker"""

    # Get or create profile
    subject = event_data["data"]["subject"]
    profile = await get_profile_by_subject(db, subject)

    if not profile:
        # Auto-create profile if enabled
        if settings.AUTO_CREATE_PROFILES:
            profile = Profile(
                subject=subject,
                is_auto_created=True,
                first_seen=datetime.utcnow()
            )
            db.add(profile)
            await db.flush()

    # Create face event record
    face_event = FaceEvent(
        camera_id=event_data["camera_id"],
        profile_id=profile.id if profile else None,
        event_type="person_recognized",
        similarity=event_data["data"]["similarity"],
        confidence=event_data["data"]["detection_confidence"],
        is_new_person=event_data["data"]["is_new_person"],
        is_new_at_camera=event_data["data"]["is_new_at_camera"],
        dwell_time_seconds=event_data["data"]["dwell_time_seconds"],
        other_cameras=event_data["data"]["other_cameras"],
        detected_at=datetime.fromtimestamp(event_data["timestamp"])
    )

    # Check watchlist match
    if profile and profile.watchlist_ids:
        face_event.watchlist_match = True
        face_event.matched_watchlists = profile.watchlist_ids

    db.add(face_event)
    await db.commit()

    # Dispatch webhooks
    if face_event.watchlist_match or settings.WEBHOOK_ALL_EVENTS:
        await dispatch_to_webhooks(face_event, db)

    # Broadcast via WebSocket to frontend
    await broadcast_to_frontend(face_event)

async def dispatch_to_webhooks(face_event: FaceEvent, db: AsyncSession):
    """Dispatch event to configured webhooks"""
    query = select(Webhook).filter(
        Webhook.is_active == True,
        Webhook.event_types.contains(["person_recognized"])
    )

    result = await db.execute(query)
    webhooks = result.scalars().all()

    for webhook in webhooks:
        # Check filters
        if webhook.watchlist_only and not face_event.watchlist_match:
            continue
        if webhook.camera_ids and str(face_event.camera_id) not in webhook.camera_ids:
            continue

        # Dispatch
        event_payload = {
            "event_type": "person_recognized",
            "camera_id": str(face_event.camera_id),
            "profile_id": str(face_event.profile_id) if face_event.profile_id else None,
            "similarity": face_event.similarity,
            "is_new_person": face_event.is_new_person,
            "watchlist_match": face_event.watchlist_match,
            "timestamp": face_event.detected_at.isoformat()
        }

        await webhook_dispatcher.dispatch_with_retry(webhook, event_payload)
```

---

## 🎨 Phase 3: Next.js Frontend

### 3.1 Camera Settings Page

**`app/cameras/[camera_id]/settings/page.tsx`:**

```typescript
export default function CameraSettingsPage({ params }: { params: { camera_id: string } }) {
  const [config, setConfig] = useState<CameraConfig | null>(null);

  useEffect(() => {
    // Load camera config
    api.getCameraConfig(params.camera_id).then(setConfig);
  }, [params.camera_id]);

  const handleSave = async () => {
    await api.updateCameraConfig(params.camera_id, config);
    toast.success("Camera settings updated");
  };

  return (
    <div className="space-y-6">
      <Card>
        <CardHeader>
          <CardTitle>General Settings</CardTitle>
        </CardHeader>
        <CardContent className="space-y-4">
          <div>
            <Label>RTSP URL</Label>
            <Input value={config?.rtsp_url} onChange={...} />
          </div>
          <div>
            <Label>Target FPS</Label>
            <Input type="number" value={config?.target_fps} onChange={...} />
          </div>
          {/* ... more general settings ... */}
        </CardContent>
      </Card>

      <Card>
        <CardHeader>
          <CardTitle>Motion Detection</CardTitle>
        </CardHeader>
        <CardContent className="space-y-4">
          <div className="flex items-center space-x-2">
            <Switch checked={config?.motion_detection.enabled} onCheckedChange={...} />
            <Label>Enable Motion Detection</Label>
          </div>

          <div>
            <Label>Sensitivity: {config?.motion_detection.sensitivity}</Label>
            <Slider
              min={0}
              max={1}
              step={0.1}
              value={[config?.motion_detection.sensitivity]}
              onValueChange={...}
            />
          </div>

          <div>
            <Label>Algorithm</Label>
            <Select value={config?.motion_detection.algorithm} onValueChange={...}>
              <SelectItem value="MOG2_CUDA">MOG2 (CUDA)</SelectItem>
              <SelectItem value="MOG2">MOG2</SelectItem>
              <SelectItem value="KNN">KNN</SelectItem>
            </Select>
          </div>
          {/* ... more motion settings ... */}
        </CardContent>
      </Card>

      <Card>
        <CardHeader>
          <CardTitle>Face Detection & Recognition</CardTitle>
        </CardHeader>
        <CardContent className="space-y-4">
          <div className="flex items-center space-x-2">
            <Switch checked={config?.face_detection.enabled} onCheckedChange={...} />
            <Label>Enable Face Detection</Label>
          </div>

          <div>
            <Label>Confidence Threshold: {config?.face_detection.confidence_threshold}</Label>
            <Slider
              min={0}
              max={1}
              step={0.05}
              value={[config?.face_detection.confidence_threshold]}
              onValueChange={...}
            />
          </div>

          <div className="flex items-center space-x-2">
            <Switch checked={config?.face_detection.enable_compreface} onCheckedChange={...} />
            <Label>Enable CompreFace Recognition</Label>
          </div>

          {config?.face_detection.enable_compreface && (
            <>
              <div>
                <Label>CompreFace URL</Label>
                <Input value={config?.face_detection.compreface_url} onChange={...} />
              </div>
              <div>
                <Label>API Key</Label>
                <Input type="password" value={config?.face_detection.compreface_api_key} onChange={...} />
              </div>
            </>
          )}

          <div className="flex items-center space-x-2">
            <Switch checked={config?.face_detection.enable_blur_detection} onCheckedChange={...} />
            <Label>Enable Blur Detection</Label>
          </div>

          {/* ... more face detection settings ... */}
        </CardContent>
      </Card>

      <Button onClick={handleSave} className="w-full">
        Save Settings
      </Button>
    </div>
  );
}
```

### 3.2 Profiles Page

**`app/profiles/page.tsx`:**

```typescript
export default function ProfilesPage() {
  const [profiles, setProfiles] = useState<Profile[]>([]);
  const [searchTerm, setSearchTerm] = useState("");

  useEffect(() => {
    api.getProfiles().then(setProfiles);
  }, []);

  const filteredProfiles = profiles.filter(p =>
    p.full_name?.toLowerCase().includes(searchTerm.toLowerCase()) ||
    p.subject.toLowerCase().includes(searchTerm.toLowerCase())
  );

  return (
    <div className="space-y-6">
      <div className="flex justify-between items-center">
        <div>
          <h1 className="text-3xl font-bold">Profiles</h1>
          <p className="text-muted-foreground">Manage person profiles and watchlists</p>
        </div>
        <Button onClick={() => router.push("/profiles/new")}>
          <UserPlus className="mr-2 h-4 w-4" />
          Add Profile
        </Button>
      </div>

      <div className="flex gap-4">
        <Input
          placeholder="Search profiles..."
          value={searchTerm}
          onChange={(e) => setSearchTerm(e.target.value)}
          className="max-w-sm"
        />
        <Select>
          <SelectTrigger className="w-[200px]">
            <SelectValue placeholder="Filter by watchlist" />
          </SelectTrigger>
          <SelectContent>
            <SelectItem value="all">All Profiles</SelectItem>
            {/* Watchlist options */}
          </SelectContent>
        </Select>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
        {filteredProfiles.map(profile => (
          <Card key={profile.id} className="cursor-pointer hover:shadow-lg transition-shadow"
                onClick={() => router.push(`/profiles/${profile.id}`)}>
            <CardHeader className="flex flex-row items-center gap-4">
              <Avatar className="h-16 w-16">
                <AvatarImage src={profile.thumbnail_url} />
                <AvatarFallback>{profile.full_name?.[0] || "?"}</AvatarFallback>
              </Avatar>
              <div className="flex-1">
                <CardTitle className="text-lg">{profile.full_name || profile.subject}</CardTitle>
                <p className="text-sm text-muted-foreground">{profile.email}</p>
              </div>
              {profile.is_auto_created && (
                <Badge variant="secondary">Auto</Badge>
              )}
            </CardHeader>
            <CardContent>
              <div className="space-y-2 text-sm">
                <div className="flex justify-between">
                  <span className="text-muted-foreground">Department:</span>
                  <span>{profile.department || "—"}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-muted-foreground">Total Detections:</span>
                  <span>{profile.total_detections}</span>
                </div>
                <div className="flex justify-between">
                  <span className="text-muted-foreground">Last Seen:</span>
                  <span>{formatRelativeTime(profile.last_seen)}</span>
                </div>
                {profile.watchlist_ids && profile.watchlist_ids.length > 0 && (
                  <div className="flex flex-wrap gap-1 mt-2">
                    {profile.watchlist_ids.map(wid => (
                      <Badge key={wid} variant="outline" className="text-xs">
                        {watchlists.find(w => w.id === wid)?.name}
                      </Badge>
                    ))}
                  </div>
                )}
              </div>
            </CardContent>
          </Card>
        ))}
      </div>
    </div>
  );
}
```

### 3.3 Events Page

**`app/events/page.tsx`:**

```typescript
export default function EventsPage() {
  const [events, setEvents] = useState<FaceEvent[]>([]);
  const [filters, setFilters] = useState({
    camera_id: null,
    event_type: "all",
    start_date: null,
    end_date: null
  });

  useEffect(() => {
    api.getFaceEvents(filters).then(setEvents);
  }, [filters]);

  return (
    <div className="space-y-6">
      <div className="flex justify-between items-center">
        <div>
          <h1 className="text-3xl font-bold">Face Detection Events</h1>
          <p className="text-muted-foreground">All face detections and recognitions</p>
        </div>
      </div>

      {/* Filters */}
      <Card>
        <CardContent className="pt-6">
          <div className="flex gap-4">
            <Select value={filters.camera_id} onValueChange={...}>
              <SelectTrigger className="w-[200px]">
                <SelectValue placeholder="All Cameras" />
              </SelectTrigger>
              <SelectContent>
                {cameras.map(cam => (
                  <SelectItem key={cam.id} value={cam.id}>{cam.camera_id}</SelectItem>
                ))}
              </SelectContent>
            </Select>

            <Select value={filters.event_type} onValueChange={...}>
              <SelectTrigger className="w-[200px]">
                <SelectValue placeholder="Event Type" />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="all">All Events</SelectItem>
                <SelectItem value="person_recognized">Recognized</SelectItem>
                <SelectItem value="face_detected_unknown">Unknown</SelectItem>
              </SelectContent>
            </Select>

            {/* Date pickers */}
          </div>
        </CardContent>
      </Card>

      {/* Events Table */}
      <Card>
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead>Timestamp</TableHead>
              <TableHead>Camera</TableHead>
              <TableHead>Person</TableHead>
              <TableHead>Type</TableHead>
              <TableHead>Confidence</TableHead>
              <TableHead>Watchlist</TableHead>
              <TableHead>Actions</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            {events.map(event => (
              <TableRow key={event.id}>
                <TableCell>{formatDateTime(event.detected_at)}</TableCell>
                <TableCell>
                  <Badge variant="outline">{event.camera.camera_id}</Badge>
                </TableCell>
                <TableCell>
                  <div className="flex items-center gap-2">
                    <Avatar className="h-8 w-8">
                      <AvatarImage src={event.face_thumbnail_path} />
                      <AvatarFallback>?</AvatarFallback>
                    </Avatar>
                    <span>
                      {event.profile ? event.profile.full_name : "Unknown"}
                    </span>
                  </div>
                </TableCell>
                <TableCell>
                  <Badge variant={event.event_type === "person_recognized" ? "default" : "secondary"}>
                    {event.event_type === "person_recognized" ? "Recognized" : "Unknown"}
                  </Badge>
                </TableCell>
                <TableCell>{(event.similarity * 100).toFixed(1)}%</TableCell>
                <TableCell>
                  {event.watchlist_match && (
                    <Badge variant="destructive">Match</Badge>
                  )}
                </TableCell>
                <TableCell>
                  <DropdownMenu>
                    <DropdownMenuTrigger asChild>
                      <Button variant="ghost" size="sm">
                        <MoreHorizontal className="h-4 w-4" />
                      </Button>
                    </DropdownMenuTrigger>
                    <DropdownMenuContent align="end">
                      <DropdownMenuItem onClick={() => viewDetails(event)}>
                        View Details
                      </DropdownMenuItem>
                      {!event.profile && (
                        <DropdownMenuItem onClick={() => createProfile(event)}>
                          Create Profile
                        </DropdownMenuItem>
                      )}
                      <DropdownMenuItem onClick={() => downloadImage(event)}>
                        Download Image
                      </DropdownMenuItem>
                    </DropdownMenuContent>
                  </DropdownMenu>
                </TableCell>
              </TableRow>
            ))}
          </TableBody>
        </Table>
      </Card>
    </div>
  );
}
```

### 3.4 Real-Time Alerts

**`components/alert-banner.tsx`:**

```typescript
export function AlertBanner() {
  const [alerts, setAlerts] = useState<PersonRecognitionEvent[]>([]);

  useEffect(() => {
    const ws = wsClient.connect();

    const unsubscribe = wsClient.subscribe((event: WebSocketEvent) => {
      if (event.event_type === "person_recognized") {
        const personEvent = event.data as PersonRecognitionEvent;

        // Check if watchlist match
        const isWatchlistMatch = /* check profile watchlist */;

        if (isWatchlistMatch || personEvent.is_new_person) {
          // Show alert
          setAlerts(prev => [personEvent, ...prev].slice(0, 5));

          // Show toast notification
          toast({
            title: isWatchlistMatch ? "⚠️ Watchlist Alert" : "🎯 New Person Detected",
            description: `${personEvent.subject} at ${personEvent.camera_id}`,
            variant: isWatchlistMatch ? "destructive" : "default"
          });
        }
      }
    });

    return () => {
      unsubscribe();
      ws.disconnect();
    };
  }, []);

  if (alerts.length === 0) return null;

  return (
    <div className="fixed top-4 right-4 z-50 space-y-2 w-96">
      {alerts.map((alert, idx) => (
        <Alert key={idx} variant={alert.watchlist_match ? "destructive" : "default"}>
          <AlertTitle>
            {alert.is_new_person ? "New Person" : "Person Recognized"}
          </AlertTitle>
          <AlertDescription>
            <div className="flex items-center gap-2 mt-2">
              <Avatar>
                <AvatarImage src={/* profile photo */} />
                <AvatarFallback>{alert.subject[0]}</AvatarFallback>
              </Avatar>
              <div>
                <p className="font-semibold">{alert.subject}</p>
                <p className="text-xs text-muted-foreground">
                  {alert.camera_id} • {alert.similarity * 100}% match
                </p>
              </div>
            </div>
          </AlertDescription>
        </Alert>
      ))}
    </div>
  );
}
```

---

## 🎯 Implementation Priority

### Week 1: Core Backend
1. ✅ PersonTracker API endpoints (C++)
2. ✅ Camera config API endpoints (C++)
3. ✅ Event broadcasting enhancement
4. ✅ Database models (FastAPI)

### Week 2: Profile & Event System
1. ✅ Profile CRUD API (FastAPI)
2. ✅ Event logging system
3. ✅ Auto-create profiles
4. ✅ Watchlist management

### Week 3: Webhooks & Frontend Core
1. ✅ Webhook system (FastAPI)
2. ✅ Frontend profiles page
3. ✅ Frontend events page
4. ✅ Camera settings page

### Week 4: Polish & Production
1. ✅ Real-time alerts
2. ✅ Statistics dashboard
3. ✅ Testing & bug fixes
4. ✅ Documentation

---

## 📝 Next Steps

**Would you like me to start implementing Phase 1.1 (PersonTracker API endpoints in C++)?** This will add all 8 REST API endpoints to your `api_server.cpp` to expose PersonTracker functionality.
