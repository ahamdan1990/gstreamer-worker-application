# MVP Quick Start Guide

## 🚀 Get Your Camera System Running in 5 Minutes

This MVP includes:
- ✅ Camera management (add/remove/control)
- ✅ Real-time status updates
- ✅ Event logging
- ✅ Production-ready C++ GStreamer worker
- ✅ REST API with auto-docs
- ✅ Docker deployment

## Prerequisites

- Docker & Docker Compose
- NVIDIA GPU with drivers (for hardware acceleration)
- NVIDIA Container Toolkit
- Your RTSP camera URLs

## Quick Start

### 1. Setup Environment

```bash
cd backend
cp .env.example .env
# Edit .env with your settings
```

### 2. Start Everything with Docker

```bash
docker-compose up -d
```

This starts:
- PostgreSQL (port 5432)
- Redis (port 6379)
- FastAPI Backend (port 8000)
- C++ GStreamer Worker (port 8081)

### 3. Access the System

- **API Documentation**: http://localhost:8000/docs
- **API Base URL**: http://localhost:8000/api/v1

### 4. Add Your First Camera

**Via API** (using Swagger UI at http://localhost:8000/docs):

```json
POST /api/v1/cameras
{
  "camera_id": "front_door",
  "name": "Front Door Camera",
  "rtsp_url": "rtsp://username:password@192.168.0.219/stream",
  "target_fps": 12,
  "latency_ms": 150,
  "enable_display": true
}
```

**Via curl**:

```bash
curl -X POST "http://localhost:8000/api/v1/cameras" \
  -H "Content-Type: application/json" \
  -d '{
    "camera_id": "front_door",
    "name": "Front Door Camera",
    "rtsp_url": "rtsp://service:WSS4Sec$$@192.168.0.219/stream",
    "target_fps": 12
  }'
```

### 5. Start the Camera Stream

```bash
curl -X POST "http://localhost:8000/api/v1/cameras/{camera_id}/start"
```

### 6. Check Status

```bash
curl "http://localhost:8000/api/v1/cameras/{camera_id}/status"
```

## Development Setup (Without Docker)

### 1. Install Dependencies

```bash
# Backend
cd backend
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

# C++ Worker (if not using Docker)
cd ../gstreamer_worker
./build.sh
```

### 2. Setup Database

```bash
# Start PostgreSQL
docker run -d \
  --name camera-postgres \
  -e POSTGRES_PASSWORD=postgres \
  -e POSTGRES_DB=camera_manager \
  -p 5432:5432 \
  postgres:15

# Run migrations
cd backend
alembic upgrade head
```

### 3. Start Services

```bash
# Terminal 1: FastAPI
cd backend
uvicorn app.main:app --reload --port 8000

# Terminal 2: C++ Worker
cd gstreamer_worker/build
./gstreamer_worker_multi --config ../config/cameras.json
```

## API Endpoints

### Cameras

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/cameras` | List all cameras |
| POST | `/api/v1/cameras` | Create camera |
| GET | `/api/v1/cameras/{id}` | Get camera details |
| PUT | `/api/v1/cameras/{id}` | Update camera |
| DELETE | `/api/v1/cameras/{id}` | Delete camera |
| POST | `/api/v1/cameras/{id}/start` | Start stream |
| POST | `/api/v1/cameras/{id}/stop` | Stop stream |
| GET | `/api/v1/cameras/{id}/status` | Get status |

### Events

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/events` | List events (paginated) |
| GET | `/api/v1/events/{id}` | Get event details |

### System

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/system/health` | Health check |
| GET | `/api/v1/system/status` | System status |

## Testing with Your Cameras

### Example: Add Multiple Cameras

```python
import requests

API_URL = "http://localhost:8000/api/v1"

cameras = [
    {
        "camera_id": "front_door",
        "name": "Front Door",
        "rtsp_url": "rtsp://user:pass@192.168.0.219/stream"
    },
    {
        "camera_id": "back_door",
        "name": "Back Door",
        "rtsp_url": "rtsp://user:pass@192.168.0.220/stream"
    }
]

for camera in cameras:
    # Create camera
    response = requests.post(f"{API_URL}/cameras", json=camera)
    print(f"Created: {response.json()}")

    # Start stream
    camera_id = camera["camera_id"]
    response = requests.post(f"{API_URL}/cameras/{camera_id}/start")
    print(f"Started: {response.json()}")
```

## Troubleshooting

### Database Connection Error

```bash
# Check if PostgreSQL is running
docker ps | grep postgres

# Check connection
psql -h localhost -U postgres -d camera_manager
```

### C++ Worker Not Starting

```bash
# Check GStreamer installation
gst-inspect-1.0 --version

# Check NVIDIA plugins
gst-inspect-1.0 nvv4l2decoder

# Test camera URL
gst-launch-1.0 rtspsrc location="rtsp://..." ! fakesink
```

### Camera Won't Connect

1. Verify RTSP URL with VLC
2. Check firewall/network access
3. Verify credentials
4. Check logs: `docker-compose logs backend`

## Next Steps

### Add Features

1. **Profiles & Face Recognition**
   - Uncomment profile routes
   - Integrate CompreFace
   - Add image upload

2. **Watchlists**
   - Uncomment watchlist routes
   - Configure alerts
   - Add webhook delivery

3. **Frontend**
   - React dashboard (coming next)
   - Real-time WebSocket updates
   - Camera grid view

4. **Production Deployment**
   - Add HTTPS (nginx reverse proxy)
   - Configure authentication
   - Set up monitoring
   - Add backup/recovery

## Support

- **API Docs**: http://localhost:8000/docs
- **Health Check**: http://localhost:8000/api/v1/system/health
- **Logs**: `docker-compose logs -f`

## What's Included in MVP

✅ **Backend**:
- FastAPI with async PostgreSQL
- Camera CRUD operations
- Real-time status tracking
- Event logging
- Auto-generated API docs

✅ **C++ Worker**:
- Production-ready pipeline manager
- Auto-reconnection
- Health monitoring
- Multi-camera support
- NVIDIA acceleration

✅ **Docker**:
- Complete stack in containers
- Easy deployment
- Development & production configs

## What's Coming Next

- 🔄 React frontend dashboard
- 🔄 WebSocket real-time updates
- 🔄 Profile management (CompreFace)
- 🔄 Watchlists & alerts
- 🔄 Webhook integrations
- 🔄 Authentication & roles

**You have a solid foundation!** The architecture supports all planned features - just uncomment and expand as needed.
