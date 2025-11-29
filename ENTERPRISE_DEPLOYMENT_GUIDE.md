# Enterprise Face Tracking System - Deployment Guide

## 🚀 System Overview

This enterprise-grade face detection and tracking system provides real-time face recognition with persistent tracking, database storage, and comprehensive analytics.

### Key Features

✅ **Intelligent Face Tracking**
- Unique tracking IDs for each person
- 60-second tracking sessions
- Maximum 3 recognition attempts per session
- Accurate duration tracking

✅ **Event Persistence**
- All detections saved to PostgreSQL database
- Face crop images stored with metadata
- Real-time WebSocket updates
- Historical event querying

✅ **Enterprise UI**
- Professional dashboard with statistics
- Advanced filtering and search
- Real-time event monitoring
- Responsive design

✅ **API-First Architecture**
- RESTful API endpoints
- WebSocket for real-time events
- Comprehensive filtering options
- Pagination support

---

## 📋 Prerequisites

### Required Software
- **Operating System**: Linux (Ubuntu 20.04+ recommended)
- **Database**: PostgreSQL 13+
- **CUDA**: 11.8+ (for GPU acceleration)
- **GStreamer**: 1.20+
- **Node.js**: 18+
- **Python**: 3.11+

### Hardware Requirements
- **GPU**: NVIDIA GPU with CUDA support (recommended)
- **RAM**: 8GB minimum, 16GB recommended
- **CPU**: 4+ cores recommended
- **Storage**: 50GB+ for images and database

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Camera Streams                           │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│              C++ GStreamer Worker (Port 8080)                   │
│  - Face Detection (SCRFD + TensorRT)                            │
│  - Face Tracking (IoU-based, 60s timeout)                       │
│  - Unique Tracking IDs                                          │
│  - CompreFace Integration                                       │
└─────────────────────────────────────────────────────────────────┘
                              │
                    ┌─────────┴─────────┐
                    │                   │
                    ▼                   ▼
          ┌─────────────────┐  ┌─────────────────┐
          │   WebSocket     │  │   HTTP API      │
          │   (Port 8080)   │  │   (Port 8080)   │
          └─────────────────┘  └─────────────────┘
                    │                   │
                    └─────────┬─────────┘
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│              FastAPI Backend (Port 8002)                        │
│  - Event Handlers                                               │
│  - Database Storage                                             │
│  - REST API Endpoints                                           │
│  - WebSocket Relay                                              │
└─────────────────────────────────────────────────────────────────┘
                              │
                    ┌─────────┴─────────┐
                    │                   │
                    ▼                   ▼
          ┌─────────────────┐  ┌─────────────────┐
          │   PostgreSQL    │  │   Face Crops    │
          │   Database      │  │   Storage       │
          └─────────────────┘  └─────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│              Next.js Frontend (Port 3000)                       │
│  - Face Detections Dashboard                                    │
│  - Real-time Event Monitoring                                   │
│  - Advanced Filtering                                           │
│  - Analytics & Reports                                          │
└─────────────────────────────────────────────────────────────────┘
```

---

## 🔧 Installation

### 1. Clone Repository

```bash
git clone <your-repo-url>
cd optimized_pipeline
```

### 2. Database Setup

```bash
# Install PostgreSQL
sudo apt update
sudo apt install postgresql postgresql-contrib

# Create database
sudo -u postgres psql
CREATE DATABASE face_tracking_db;
CREATE USER face_tracking_user WITH PASSWORD 'your_secure_password';
GRANT ALL PRIVILEGES ON DATABASE face_tracking_db TO face_tracking_user;
\q

# Configure connection
cd backend
cp .env.example .env
# Edit .env with your database credentials
```

### 3. Backend Setup

```bash
cd backend

# Create virtual environment
python3.11 -m venv venv
source venv/bin/activate

# Install dependencies
pip install -r requirements.txt

# Run migrations
alembic upgrade head

# Verify table creation
python3 -c "
import asyncio
from sqlalchemy import text
from app.db.base import async_session

async def check():
    async with async_session() as db:
        result = await db.execute(text('SELECT COUNT(*) FROM face_detection_events'))
        print(f'✅ Database ready. Events: {result.scalar()}')

asyncio.run(check())
"
```

### 4. C++ Worker Setup

```bash
cd ../gstreamer_worker

# Install dependencies
sudo apt install \
    build-essential cmake \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libopencv-dev \
    nvidia-cuda-toolkit

# Build
./build.sh

# Verify build
ls -lh build/gstreamer_worker_api
```

### 5. Frontend Setup

```bash
cd ../frontend-ui

# Install dependencies
npm install

# Configure API URL
echo "NEXT_PUBLIC_API_URL=http://localhost:8002" > .env.local

# Build (optional)
npm run build
```

---

## 🚀 Starting the System

### Option 1: Development Mode

**Terminal 1 - C++ Worker:**
```bash
cd gstreamer_worker/build
./gstreamer_worker_api --port 8080
```

**Terminal 2 - Backend:**
```bash
cd backend
source venv/bin/activate
uvicorn app.main:app --host 0.0.0.0 --port 8002 --reload
```

**Terminal 3 - Frontend:**
```bash
cd frontend-ui
npm run dev
```

### Option 2: Production Mode (Systemd)

Create systemd service files:

**1. C++ Worker Service** (`/etc/systemd/system/face-tracking-worker.service`):
```ini
[Unit]
Description=Face Tracking C++ Worker
After=network.target

[Service]
Type=simple
User=www-data
WorkingDirectory=/path/to/gstreamer_worker/build
ExecStart=/path/to/gstreamer_worker/build/gstreamer_worker_api --port 8080
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

**2. Backend Service** (`/etc/systemd/system/face-tracking-backend.service`):
```ini
[Unit]
Description=Face Tracking Backend API
After=network.target postgresql.service

[Service]
Type=simple
User=www-data
WorkingDirectory=/path/to/backend
Environment="PATH=/path/to/backend/venv/bin"
ExecStart=/path/to/backend/venv/bin/uvicorn app.main:app --host 0.0.0.0 --port 8002
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

**3. Frontend Service** (`/etc/systemd/system/face-tracking-frontend.service`):
```ini
[Unit]
Description=Face Tracking Frontend
After=network.target

[Service]
Type=simple
User=www-data
WorkingDirectory=/path/to/frontend-ui
Environment="PATH=/usr/bin:/usr/local/bin"
Environment="NODE_ENV=production"
ExecStart=/usr/bin/npm start
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

**Enable and start services:**
```bash
sudo systemctl daemon-reload
sudo systemctl enable face-tracking-worker
sudo systemctl enable face-tracking-backend
sudo systemctl enable face-tracking-frontend
sudo systemctl start face-tracking-worker
sudo systemctl start face-tracking-backend
sudo systemctl start face-tracking-frontend

# Check status
sudo systemctl status face-tracking-*
```

---

## 📊 Using the System

### Accessing the Interface

1. **Frontend Dashboard**: http://localhost:3000
2. **Face Detections Page**: http://localhost:3000/face-detections
3. **API Documentation**: http://localhost:8002/docs

### Face Detections Dashboard

The enterprise face detections dashboard provides:

#### **Statistics Overview**
- Total detections count
- Recognized vs unknown faces
- Average tracking duration
- Average recognition attempts
- Success rate percentages

#### **Advanced Filtering**
- **Search**: Filter by tracking ID, subject name, or camera
- **Recognition Status**: View all/recognized/unknown faces only
- **Camera Selection**: Filter by specific camera
- **Date Range**: View events within date ranges (API level)

#### **Event Cards**
Each detection shows:
- Face crop image with confidence score
- Subject name (or "Unknown Person")
- Recognition status badge
- Camera name
- Tracking duration
- Recognition attempts (0-3)
- Timestamp
- Tracking ID

#### **Real-time Updates**
- Auto-refresh capability
- Live event stream (via WebSocket)
- Instant notification of new detections

### API Usage

#### List Face Detection Events
```bash
# Get all events
curl "http://localhost:8002/api/v1/events/faces?limit=10"

# Filter by recognition status
curl "http://localhost:8002/api/v1/events/faces?is_recognized=true"

# Filter by camera
curl "http://localhost:8002/api/v1/events/faces?camera_id=Camera%201"

# Combine filters
curl "http://localhost:8002/api/v1/events/faces?camera_id=Camera%201&is_recognized=false&limit=20"

# With date range
curl "http://localhost:8002/api/v1/events/faces?start_date=2025-01-01T00:00:00&end_date=2025-01-31T23:59:59"
```

#### Get Specific Event
```bash
# By tracking ID
curl "http://localhost:8002/api/v1/events/faces/{tracking_id}"
```

#### Response Format
```json
{
  "events": [
    {
      "id": "uuid",
      "tracking_id": "a1b2c3d4e5f6g7h8...",
      "camera_id": "Camera 1",
      "subject": "John Doe",
      "is_recognized": true,
      "confidence": 0.87,
      "recognition_attempts": 2,
      "tracking_duration_seconds": 45.3,
      "is_first_detection": true,
      "image_url": "/face_crops/Camera_1_1234567890_face1_87.jpg",
      "created_at": "2025-01-28T18:08:55.078Z",
      "updated_at": "2025-01-28T18:09:40.123Z"
    }
  ],
  "total": 1,
  "skip": 0,
  "limit": 10
}
```

---

## 🔍 Monitoring & Troubleshooting

### Log Locations

```bash
# C++ Worker Logs
# Check stdout/stderr or systemd journal
sudo journalctl -u face-tracking-worker -f

# Backend Logs
sudo journalctl -u face-tracking-backend -f
# Or check application logs
tail -f backend/logs/app.log

# Frontend Logs
sudo journalctl -u face-tracking-frontend -f
```

### Common Issues

#### ❌ Multiple face_detected events for same person

**Cause**: Old C++ worker binary is running
**Solution**:
```bash
# Rebuild C++ worker
cd gstreamer_worker
./build.sh

# Restart service
sudo systemctl restart face-tracking-worker

# Verify new tracking IDs in logs
sudo journalctl -u face-tracking-worker -f | grep "tracking_id"
```

#### ❌ Events not appearing in database

**Cause**: Backend not receiving events or database connection issue
**Solution**:
```bash
# Check backend logs
tail -f backend/logs/app.log | grep "face_detected"

# Verify database connection
cd backend && python3 -c "
from app.db.base import async_session
import asyncio
async def test():
    async with async_session() as db:
        print('✅ Database connected')
asyncio.run(test())
"

# Check table exists
psql -d face_tracking_db -c "SELECT COUNT(*) FROM face_detection_events;"
```

#### ❌ Face crops not displaying

**Cause**: Incorrect image URL path or permissions
**Solution**:
```bash
# Check face crops directory
ls -la gstreamer_worker/face_crops/

# Fix permissions
chmod 755 gstreamer_worker/face_crops
chmod 644 gstreamer_worker/face_crops/*.jpg

# Verify backend can access
curl http://localhost:8002/face_crops/Camera_1_xxx.jpg
```

#### ❌ Frontend shows "Failed to fetch events"

**Cause**: Backend not running or wrong API URL
**Solution**:
```bash
# Check backend is running
curl http://localhost:8002/api/v1/events/faces

# Verify frontend .env.local
cat frontend-ui/.env.local
# Should show: NEXT_PUBLIC_API_URL=http://localhost:8002

# Restart frontend
npm run dev
```

### Performance Monitoring

```bash
# Database query performance
psql -d face_tracking_db -c "
SELECT
    camera_id,
    COUNT(*) as events,
    AVG(tracking_duration_seconds) as avg_duration
FROM face_detection_events
GROUP BY camera_id;
"

# C++ Worker statistics
curl http://localhost:8080/api/statistics

# Backend health check
curl http://localhost:8002/health
```

---

## 📈 Scaling & Optimization

### Database Optimization

```sql
-- Add indexes for common queries
CREATE INDEX idx_face_events_camera_created
ON face_detection_events(camera_id, created_at DESC);

CREATE INDEX idx_face_events_subject_created
ON face_detection_events(subject, created_at DESC)
WHERE is_recognized = true;

-- Archive old events (monthly)
CREATE TABLE face_detection_events_archive_2025_01
AS SELECT * FROM face_detection_events
WHERE created_at >= '2025-01-01'
AND created_at < '2025-02-01';
```

### Face Crop Storage

```bash
# Set up daily cleanup (cron)
crontab -e

# Add: Clean face crops older than 30 days
0 2 * * * find /path/to/gstreamer_worker/face_crops -mtime +30 -delete
```

### Load Balancing

For multiple cameras:

1. Run multiple C++ worker instances (different ports)
2. Use nginx as reverse proxy
3. Configure backend to connect to multiple workers

---

## 🔐 Security Considerations

### Database Security

```sql
-- Restrict database user permissions
REVOKE ALL ON DATABASE face_tracking_db FROM PUBLIC;
GRANT CONNECT ON DATABASE face_tracking_db TO face_tracking_user;
GRANT SELECT, INSERT, UPDATE ON face_detection_events TO face_tracking_user;
```

### API Security

```python
# Add authentication middleware in backend/app/main.py
from fastapi import Security, HTTPException
from fastapi.security import APIKeyHeader

API_KEY = "your-secret-key"
api_key_header = APIKeyHeader(name="X-API-Key")

def verify_api_key(api_key: str = Security(api_key_header)):
    if api_key != API_KEY:
        raise HTTPException(status_code=403, detail="Invalid API key")
    return api_key

# Add to protected routes
@app.get("/api/v1/events/faces", dependencies=[Depends(verify_api_key)])
```

### Network Security

```bash
# Configure firewall
sudo ufw allow 3000/tcp  # Frontend (restrict to internal network)
sudo ufw allow 8002/tcp  # Backend API (restrict to internal network)
sudo ufw allow 8080/tcp  # Worker API (localhost only recommended)
sudo ufw enable
```

---

## 📝 Maintenance

### Regular Tasks

**Daily:**
- Monitor log files for errors
- Check disk space for face crops
- Verify all services running

**Weekly:**
- Review database size and performance
- Analyze face detection statistics
- Update CompreFace profiles

**Monthly:**
- Archive old events
- Clean old face crops
- Update dependencies
- Review security logs

### Backup Strategy

```bash
# Automated database backup script
#!/bin/bash
BACKUP_DIR="/backups/face-tracking"
DATE=$(date +%Y%m%d_%H%M%S)

# Backup database
pg_dump -U face_tracking_user face_tracking_db | \
    gzip > "$BACKUP_DIR/db_backup_$DATE.sql.gz"

# Backup face crops (incremental)
rsync -av --link-dest="$BACKUP_DIR/latest" \
    /path/to/gstreamer_worker/face_crops/ \
    "$BACKUP_DIR/face_crops_$DATE/"
ln -snf "$BACKUP_DIR/face_crops_$DATE" "$BACKUP_DIR/latest"

# Keep only last 30 days
find "$BACKUP_DIR" -name "db_backup_*" -mtime +30 -delete
find "$BACKUP_DIR" -name "face_crops_*" -mtime +30 -exec rm -rf {} \;
```

---

## 📚 Additional Resources

- **Architecture Docs**: [FACE_TRACKING_IMPROVEMENTS.md](FACE_TRACKING_IMPROVEMENTS.md)
- **API Reference**: http://localhost:8002/docs (Swagger UI)
- **Test Script**: [test_face_tracking.sh](test_face_tracking.sh)

---

## 🎯 Support

For issues or questions:
1. Check logs for error messages
2. Review troubleshooting section
3. Check API documentation
4. Create GitHub issue with logs and description

---

**System Version**: 2.0.0
**Last Updated**: January 2025
**License**: Enterprise License
