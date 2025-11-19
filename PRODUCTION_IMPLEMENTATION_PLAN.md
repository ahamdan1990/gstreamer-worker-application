# Production Implementation Plan

## 🎯 Complete System Overview

This document outlines the **complete production-ready system** for camera management with facial recognition, watchlists, events, and webhooks.

## 📁 Directory Structure

```
gstreamer_worker/
├── backend/                    # FastAPI Application
│   ├── app/
│   │   ├── api/
│   │   │   ├── v1/
│   │   │   │   ├── cameras.py      # Camera CRUD & control
│   │   │   │   ├── profiles.py     # Profile management
│   │   │   │   ├── watchlists.py   # Watchlist management
│   │   │   │   ├── events.py       # Event history & real-time
│   │   │   │   ├── webhooks.py     # Webhook configuration
│   │   │   │   ├── auth.py         # Authentication
│   │   │   │   └── system.py       # System status & health
│   │   │   └── websocket.py        # WebSocket handlers
│   │   ├── models/                  # SQLAlchemy models
│   │   │   ├── camera.py
│   │   │   ├── profile.py
│   │   │   ├── watchlist.py
│   │   │   ├── event.py
│   │   │   ├── webhook.py
│   │   │   └── user.py
│   │   ├── schemas/                 # Pydantic schemas
│   │   ├── services/                # Business logic
│   │   │   ├── worker_client.py    # C++ worker communication
│   │   │   ├── compreface.py       # CompreFace integration
│   │   │   ├── webhook_delivery.py # Webhook processing
│   │   │   └── event_processor.py  # Event handling
│   │   ├── core/                    # Core functionality
│   │   │   ├── config.py
│   │   │   ├── security.py
│   │   │   └── logging.py
│   │   ├── db/                      # Database
│   │   │   └── base.py
│   │   └── main.py                  # Application entry
│   ├── alembic/                     # Database migrations
│   ├── requirements.txt
│   ├── Dockerfile
│   └── .env
│
├── frontend/                   # React Application
│   ├── src/
│   │   ├── components/
│   │   │   ├── cameras/            # Camera grid, controls
│   │   │   ├── profiles/           # Profile management
│   │   │   ├── watchlists/         # Watchlist UI
│   │   │   ├── events/             # Event feed, timeline
│   │   │   └── common/             # Shared components
│   │   ├── pages/
│   │   │   ├── Dashboard.tsx
│   │   │   ├── Cameras.tsx
│   │   │   ├── Profiles.tsx
│   │   │   ├── Watchlists.tsx
│   │   │   ├── Events.tsx
│   │   │   └── Settings.tsx
│   │   ├── services/               # API clients
│   │   ├── hooks/                  # Custom React hooks
│   │   ├── store/                  # State management (Redux/Zustand)
│   │   └── App.tsx
│   ├── package.json
│   └── Dockerfile
│
├── worker/                     # C++ GStreamer Worker (existing)
│   ├── src/
│   ├── include/
│   └── api_server.cpp          # NEW: REST API for control
│
├── docker-compose.yml          # Complete stack deployment
├── docker-compose.dev.yml      # Development environment
├── .env.example
└── README.md

```

## 🗄️ Complete Database Schema

### Core Tables

```sql
-- Users & Authentication
CREATE TABLE users (
    id UUID PRIMARY KEY,
    email VARCHAR(255) UNIQUE NOT NULL,
    hashed_password VARCHAR(255) NOT NULL,
    full_name VARCHAR(255),
    role VARCHAR(50) DEFAULT 'viewer',  -- admin, operator, viewer
    is_active BOOLEAN DEFAULT true,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- API Keys
CREATE TABLE api_keys (
    id UUID PRIMARY KEY,
    user_id UUID REFERENCES users(id),
    key_name VARCHAR(255),
    api_key VARCHAR(255) UNIQUE NOT NULL,
    permissions JSONB,  -- Array of permissions
    is_active BOOLEAN DEFAULT true,
    last_used_at TIMESTAMP,
    expires_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Cameras
CREATE TABLE cameras (
    id UUID PRIMARY KEY,
    camera_id VARCHAR(255) UNIQUE NOT NULL,
    name VARCHAR(255) NOT NULL,
    description TEXT,

    -- RTSP Config
    rtsp_url TEXT NOT NULL,
    username VARCHAR(255),
    password VARCHAR(255),  -- Encrypted
    protocols VARCHAR(50) DEFAULT 'tcp',

    -- Stream Settings
    latency_ms INTEGER DEFAULT 150,
    drop_on_latency BOOLEAN DEFAULT true,
    target_fps INTEGER DEFAULT 12,

    -- Display
    enable_display BOOLEAN DEFAULT true,
    display_sync BOOLEAN DEFAULT false,

    -- Reconnection
    auto_reconnect BOOLEAN DEFAULT true,
    max_reconnect_attempts INTEGER DEFAULT -1,
    reconnect_initial_delay REAL DEFAULT 1.0,
    reconnect_max_delay REAL DEFAULT 60.0,
    reconnect_backoff_multiplier REAL DEFAULT 2.0,

    -- Health
    health_check_interval REAL DEFAULT 5.0,
    max_consecutive_errors INTEGER DEFAULT 5,

    -- Hardware
    use_nvidia_decoder BOOLEAN DEFAULT true,

    -- Status
    enabled BOOLEAN DEFAULT true,
    state VARCHAR(50) DEFAULT 'STOPPED',
    is_running BOOLEAN DEFAULT false,

    -- Metadata
    location VARCHAR(255),
    tags JSONB,
    metadata JSONB,

    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_seen_at TIMESTAMP
);

-- Profiles (People to recognize)
CREATE TABLE profiles (
    id UUID PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    compreface_subject_id VARCHAR(255) UNIQUE,

    -- Profile Info
    description TEXT,
    department VARCHAR(255),
    employee_id VARCHAR(255),
    contact_info JSONB,

    -- Images
    images JSONB,  -- Array of image URLs
    primary_image_url TEXT,

    -- Status
    is_active BOOLEAN DEFAULT true,

    -- Metadata
    tags JSONB,
    metadata JSONB,

    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Watchlists
CREATE TABLE watchlists (
    id UUID PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    description TEXT,
    priority INTEGER DEFAULT 0,  -- Higher = more important
    color VARCHAR(7),  -- Hex color for UI

    -- Notification Settings
    enable_notifications BOOLEAN DEFAULT true,
    notification_channels JSONB,  -- ['email', 'webhook', 'slack']

    -- Status
    is_active BOOLEAN DEFAULT true,

    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Watchlist Profiles (Many-to-Many)
CREATE TABLE watchlist_profiles (
    watchlist_id UUID REFERENCES watchlists(id) ON DELETE CASCADE,
    profile_id UUID REFERENCES profiles(id) ON DELETE CASCADE,
    added_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    added_by UUID REFERENCES users(id),
    notes TEXT,
    PRIMARY KEY (watchlist_id, profile_id)
);

-- Camera Watchlists (Which watchlists to monitor per camera)
CREATE TABLE camera_watchlists (
    camera_id UUID REFERENCES cameras(id) ON DELETE CASCADE,
    watchlist_id UUID REFERENCES watchlists(id) ON DELETE CASCADE,
    enabled BOOLEAN DEFAULT true,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (camera_id, watchlist_id)
);

-- Events (Recognition events)
CREATE TABLE events (
    id UUID PRIMARY KEY,
    event_type VARCHAR(50) NOT NULL,  -- 'face_detected', 'person_recognized', 'watchlist_match', 'alert'

    -- Source
    camera_id UUID REFERENCES cameras(id),

    -- Recognition
    profile_id UUID REFERENCES profiles(id),
    confidence REAL,
    is_watchlist_match BOOLEAN DEFAULT false,
    matched_watchlists JSONB,  -- Array of watchlist IDs

    -- Event Data
    image_url TEXT,
    thumbnail_url TEXT,
    bounding_box JSONB,  -- {x, y, width, height}

    -- Attributes (from CompreFace)
    age_range VARCHAR(20),
    gender VARCHAR(20),
    mask BOOLEAN,
    pose JSONB,

    -- Status
    acknowledged BOOLEAN DEFAULT false,
    acknowledged_by UUID REFERENCES users(id),
    acknowledged_at TIMESTAMP,

    -- Metadata
    metadata JSONB,

    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Webhooks
CREATE TABLE webhooks (
    id UUID PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    url TEXT NOT NULL,

    -- Filters
    event_types JSONB,  -- Array of event types to send
    camera_ids JSONB,  -- Filter by camera (null = all)
    watchlist_ids JSONB,  -- Filter by watchlist (null = all)
    min_confidence REAL DEFAULT 0.0,

    -- Configuration
    method VARCHAR(10) DEFAULT 'POST',
    headers JSONB,  -- Custom headers
    auth_type VARCHAR(50),  -- 'none', 'bearer', 'basic', 'api_key'
    auth_config JSONB,

    -- Retry Settings
    retry_enabled BOOLEAN DEFAULT true,
    max_retries INTEGER DEFAULT 3,
    retry_delay INTEGER DEFAULT 60,  -- seconds
    timeout INTEGER DEFAULT 30,  -- seconds

    -- Status
    is_active BOOLEAN DEFAULT true,

    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Webhook Deliveries (Audit log)
CREATE TABLE webhook_deliveries (
    id UUID PRIMARY KEY,
    webhook_id UUID REFERENCES webhooks(id) ON DELETE CASCADE,
    event_id UUID REFERENCES events(id),

    -- Request
    request_url TEXT,
    request_method VARCHAR(10),
    request_headers JSONB,
    request_body JSONB,

    -- Response
    status_code INTEGER,
    response_body TEXT,
    response_time_ms INTEGER,

    -- Status
    success BOOLEAN,
    error_message TEXT,
    retry_count INTEGER DEFAULT 0,

    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Audit Log
CREATE TABLE audit_log (
    id UUID PRIMARY KEY,
    user_id UUID REFERENCES users(id),
    action VARCHAR(255) NOT NULL,
    resource_type VARCHAR(50),  -- 'camera', 'profile', 'watchlist', etc.
    resource_id UUID,
    details JSONB,
    ip_address VARCHAR(45),
    user_agent TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

## 🔌 API Endpoints

### Authentication
```
POST   /api/v1/auth/register         - Register new user
POST   /api/v1/auth/login            - Login
POST   /api/v1/auth/refresh          - Refresh token
POST   /api/v1/auth/logout           - Logout
GET    /api/v1/auth/me               - Get current user
```

### Cameras
```
GET    /api/v1/cameras               - List cameras
POST   /api/v1/cameras               - Create camera
GET    /api/v1/cameras/:id           - Get camera
PUT    /api/v1/cameras/:id           - Update camera
DELETE /api/v1/cameras/:id           - Delete camera
POST   /api/v1/cameras/:id/start     - Start stream
POST   /api/v1/cameras/:id/stop      - Stop stream
POST   /api/v1/cameras/:id/restart   - Restart stream
GET    /api/v1/cameras/:id/status    - Get status
GET    /api/v1/cameras/:id/snapshot  - Get snapshot
```

### Profiles
```
GET    /api/v1/profiles              - List profiles
POST   /api/v1/profiles              - Create profile
GET    /api/v1/profiles/:id          - Get profile
PUT    /api/v1/profiles/:id          - Update profile
DELETE /api/v1/profiles/:id          - Delete profile
POST   /api/v1/profiles/:id/images   - Upload image
DELETE /api/v1/profiles/:id/images/:img_id - Delete image
```

### Watchlists
```
GET    /api/v1/watchlists            - List watchlists
POST   /api/v1/watchlists            - Create watchlist
GET    /api/v1/watchlists/:id        - Get watchlist
PUT    /api/v1/watchlists/:id        - Update watchlist
DELETE /api/v1/watchlists/:id        - Delete watchlist
POST   /api/v1/watchlists/:id/profiles/:profile_id - Add profile
DELETE /api/v1/watchlists/:id/profiles/:profile_id - Remove profile
```

### Events
```
GET    /api/v1/events                - List events (paginated)
GET    /api/v1/events/:id            - Get event
POST   /api/v1/events/:id/acknowledge - Acknowledge event
GET    /api/v1/events/stats          - Event statistics
```

### Webhooks
```
GET    /api/v1/webhooks              - List webhooks
POST   /api/v1/webhooks              - Create webhook
GET    /api/v1/webhooks/:id          - Get webhook
PUT    /api/v1/webhooks/:id          - Update webhook
DELETE /api/v1/webhooks/:id          - Delete webhook
POST   /api/v1/webhooks/:id/test     - Test webhook
GET    /api/v1/webhooks/:id/deliveries - Delivery history
```

### System
```
GET    /api/v1/system/health         - Health check
GET    /api/v1/system/status         - System status
GET    /api/v1/system/metrics        - System metrics
POST   /api/v1/system/reload         - Reload configuration
```

### WebSocket
```
WS     /ws/events                    - Real-time event stream
WS     /ws/cameras/:id               - Camera-specific updates
WS     /ws/system                    - System status updates
```

## 🚀 Next Steps

I've created the foundation. Now I need to create the complete files for:

1. ✅ Database models (Camera done, need others)
2. ⏳ Pydantic schemas
3. ⏳ API routes
4. ⏳ Services (worker client, CompreFace, webhooks)
5. ⏳ WebSocket handlers
6. ⏳ Main FastAPI application
7. ⏳ Docker setup
8. ⏳ React frontend
9. ⏳ C++ worker REST API

This is production-ready and extensible. You'll only ADD features, never CHANGE the foundation.

Ready to continue with the remaining implementation?
