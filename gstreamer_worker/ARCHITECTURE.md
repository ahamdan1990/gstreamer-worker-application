# Production Web Application Architecture

## Overview

This document describes the complete architecture for a production-ready web application to manage GStreamer camera streams.

## System Components

### 1. Database Layer

**Purpose**: Persist camera configurations, status, metrics, and system state

**Database**: SQLite (development) / PostgreSQL (production)

**Tables**:

```sql
-- Cameras table
CREATE TABLE cameras (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    camera_id VARCHAR(255) UNIQUE NOT NULL,
    rtsp_url TEXT NOT NULL,
    username VARCHAR(255),
    password VARCHAR(255),  -- Encrypted
    protocols VARCHAR(50) DEFAULT 'tcp',
    latency_ms INTEGER DEFAULT 150,
    drop_on_latency BOOLEAN DEFAULT TRUE,
    target_fps INTEGER DEFAULT 12,
    enable_display BOOLEAN DEFAULT TRUE,
    display_sync BOOLEAN DEFAULT FALSE,
    auto_reconnect BOOLEAN DEFAULT TRUE,
    max_reconnect_attempts INTEGER DEFAULT -1,
    reconnect_initial_delay REAL DEFAULT 1.0,
    reconnect_max_delay REAL DEFAULT 60.0,
    reconnect_backoff_multiplier REAL DEFAULT 2.0,
    health_check_interval REAL DEFAULT 5.0,
    max_consecutive_errors INTEGER DEFAULT 5,
    use_nvidia_decoder BOOLEAN DEFAULT TRUE,
    enabled BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Camera status (real-time state)
CREATE TABLE camera_status (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    camera_id VARCHAR(255) UNIQUE NOT NULL,
    state VARCHAR(50) NOT NULL,  -- STOPPED, STARTING, RUNNING, ERROR, RECONNECTING
    is_running BOOLEAN DEFAULT FALSE,
    last_updated TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (camera_id) REFERENCES cameras(camera_id) ON DELETE CASCADE
);

-- Camera metrics (historical data)
CREATE TABLE camera_metrics (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    camera_id VARCHAR(255) NOT NULL,
    frames_displayed INTEGER DEFAULT 0,
    errors_count INTEGER DEFAULT 0,
    reconnections INTEGER DEFAULT 0,
    uptime_seconds REAL DEFAULT 0,
    last_error TEXT,
    recorded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (camera_id) REFERENCES cameras(camera_id) ON DELETE CASCADE
);

-- System configuration
CREATE TABLE system_config (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    key VARCHAR(255) UNIQUE NOT NULL,
    value TEXT NOT NULL,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- API keys for authentication
CREATE TABLE api_keys (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    key_name VARCHAR(255) NOT NULL,
    api_key VARCHAR(255) UNIQUE NOT NULL,
    permissions TEXT,  -- JSON array of permissions
    enabled BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_used TIMESTAMP
);

-- Audit log
CREATE TABLE audit_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    action VARCHAR(255) NOT NULL,
    camera_id VARCHAR(255),
    user_id VARCHAR(255),
    details TEXT,  -- JSON
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### 2. REST API

**Port**: 8080 (configurable)

**Endpoints**:

#### Camera Management

```
GET    /api/cameras              - List all cameras
GET    /api/cameras/:id          - Get camera details
POST   /api/cameras              - Add new camera
PUT    /api/cameras/:id          - Update camera configuration
DELETE /api/cameras/:id          - Remove camera
```

#### Camera Control

```
POST   /api/cameras/:id/start    - Start camera stream
POST   /api/cameras/:id/stop     - Stop camera stream
POST   /api/cameras/:id/restart  - Restart camera stream
GET    /api/cameras/:id/status   - Get current status
GET    /api/cameras/:id/metrics  - Get camera metrics
```

#### System

```
GET    /api/system/status        - System health and status
GET    /api/system/metrics       - Global metrics
POST   /api/system/reload        - Reload configuration
GET    /api/system/health        - Health check endpoint
```

#### Streaming (Future)

```
GET    /api/cameras/:id/snapshot - Get JPEG snapshot
GET    /api/cameras/:id/stream   - Get MJPEG stream
```

### 3. WebSocket

**Port**: 8081 (configurable)

**Events** (Server → Client):

```json
// Camera state change
{
  "type": "camera_state",
  "camera_id": "front_door",
  "state": "RUNNING",
  "timestamp": "2025-01-19T10:30:00Z"
}

// Camera error
{
  "type": "camera_error",
  "camera_id": "back_door",
  "error": "Connection timeout",
  "timestamp": "2025-01-19T10:30:00Z"
}

// Metrics update
{
  "type": "metrics_update",
  "camera_id": "parking_lot",
  "metrics": {
    "frames_displayed": 1500,
    "uptime_seconds": 125.3,
    "errors_count": 0
  },
  "timestamp": "2025-01-19T10:30:00Z"
}

// System status
{
  "type": "system_status",
  "running_cameras": 5,
  "total_cameras": 8,
  "timestamp": "2025-01-19T10:30:00Z"
}
```

**Commands** (Client → Server):

```json
// Subscribe to camera updates
{
  "command": "subscribe",
  "camera_id": "front_door"
}

// Unsubscribe
{
  "command": "unsubscribe",
  "camera_id": "front_door"
}
```

### 4. Web Interface

**Pages**:

1. **Dashboard** (`/`)
   - Grid view of all cameras
   - Status indicators (green/red/yellow)
   - Quick actions (start/stop)
   - Real-time metrics

2. **Camera Management** (`/cameras`)
   - List view with search/filter
   - Add new camera form
   - Edit camera configuration
   - Delete camera

3. **Camera Details** (`/cameras/:id`)
   - Live stream preview
   - Detailed metrics
   - Configuration
   - Logs

4. **System Status** (`/system`)
   - Global metrics
   - Server health
   - Resource usage
   - Audit log

5. **Settings** (`/settings`)
   - System configuration
   - API keys management
   - User management

## API Request/Response Examples

### Add Camera

**Request**:
```http
POST /api/cameras
Content-Type: application/json

{
  "camera_id": "front_door",
  "rtsp_url": "rtsp://192.168.0.219/stream",
  "username": "admin",
  "password": "password123",
  "target_fps": 12,
  "latency_ms": 150,
  "enable_display": true
}
```

**Response**:
```json
{
  "success": true,
  "camera_id": "front_door",
  "message": "Camera added successfully"
}
```

### Start Camera

**Request**:
```http
POST /api/cameras/front_door/start
```

**Response**:
```json
{
  "success": true,
  "camera_id": "front_door",
  "state": "STARTING",
  "message": "Camera stream starting"
}
```

### Get Camera Status

**Request**:
```http
GET /api/cameras/front_door/status
```

**Response**:
```json
{
  "camera_id": "front_door",
  "state": "RUNNING",
  "is_running": true,
  "metrics": {
    "frames_displayed": 1500,
    "errors_count": 0,
    "reconnections": 1,
    "uptime_seconds": 125.3,
    "last_error": null
  },
  "last_updated": "2025-01-19T10:30:00Z"
}
```

## Security

### Authentication

- API Key-based authentication
- JWT tokens for web sessions
- Rate limiting per API key
- IP whitelisting support

### Authorization

**Roles**:
- `admin`: Full access to all operations
- `operator`: Can start/stop cameras, view status
- `viewer`: Read-only access

**Permissions**:
- `cameras:read`
- `cameras:write`
- `cameras:control`
- `system:read`
- `system:write`

### Data Security

- Passwords encrypted in database (AES-256)
- HTTPS/WSS in production
- CORS configuration
- Input validation and sanitization
- SQL injection prevention (parameterized queries)

## Deployment

### Development

```bash
# Start database
./scripts/init_db.sh

# Start API server
./build/gstreamer_api_server --port 8080 --db database/cameras.db

# Start web interface
cd web && python3 -m http.server 3000
```

### Production

```bash
# Using systemd
sudo systemctl start gstreamer-worker
sudo systemctl enable gstreamer-worker

# Using Docker
docker-compose up -d

# Behind reverse proxy (nginx)
```

## Configuration Files

### `config/api_server.json`

```json
{
  "server": {
    "host": "0.0.0.0",
    "port": 8080,
    "websocket_port": 8081,
    "cors_enabled": true,
    "cors_origins": ["*"]
  },
  "database": {
    "type": "sqlite",
    "path": "database/cameras.db"
  },
  "security": {
    "require_auth": true,
    "jwt_secret": "your-secret-key",
    "session_timeout": 3600
  },
  "logging": {
    "level": "INFO",
    "file": "logs/api_server.log"
  }
}
```

## Monitoring & Observability

### Metrics Exposed

- Total cameras
- Running cameras
- Failed cameras
- Total frames processed
- Average FPS per camera
- Error rate
- API request rate
- API latency

### Health Checks

```http
GET /api/system/health
```

Response:
```json
{
  "status": "healthy",
  "version": "1.0.0",
  "uptime": 12345,
  "cameras": {
    "total": 10,
    "running": 8,
    "error": 2
  },
  "database": "connected",
  "memory_usage_mb": 512
}
```

## Scaling Considerations

### Horizontal Scaling

- Multiple API servers behind load balancer
- Shared database (PostgreSQL)
- Redis for session management
- Message queue for async operations

### Vertical Scaling

- GPU per worker
- Thread pool sizing
- Connection pooling
- Cache layer (Redis)

## Next Steps

1. Implement Database Layer
2. Implement REST API Server
3. Add WebSocket Support
4. Create Web Interface
5. Add Authentication/Authorization
6. Create Docker deployment
7. Add monitoring/metrics
8. Write API documentation (OpenAPI/Swagger)
9. Create automated tests
10. Production deployment guide
