# GStreamer Camera Management System

**Production-ready camera management with facial recognition, watchlists, and real-time monitoring.**

## 🎯 What This Is

A complete, production-ready system for managing multiple RTSP camera streams with:
- Real-time video processing (C++ GStreamer with NVIDIA acceleration)
- REST API for camera management (FastAPI)
- Event logging and webhooks
- Future-ready for facial recognition (CompreFace integration)
- Watchlists and alerts
- Web dashboard (React - coming soon)

## 🏗️ Architecture

```
React Frontend (Port 3000) [Coming Soon]
    ↓
FastAPI Backend (Port 8000)
    ↓
PostgreSQL + Redis
    ↓
C++ GStreamer Worker (Port 8081)
    → RTSP Cameras (Live Streams)
```

## 🚀 Quick Start (5 Minutes)

See **[MVP_QUICKSTART.md](MVP_QUICKSTART.md)** for detailed setup instructions.

```bash
# 1. Copy environment file
cp .env.example .env

# 2. Start everything with Docker
docker-compose up -d

# 3. Access API docs
open http://localhost:8000/docs

# 4. Add your first camera (via API)
curl -X POST "http://localhost:8000/api/v1/cameras" \
  -H "Content-Type: application/json" \
  -d '{
    "camera_id": "front_door",
    "name": "Front Door",
    "rtsp_url": "rtsp://user:pass@192.168.0.219/stream",
    "target_fps": 12
  }'
```

## 📚 Documentation

- **[MVP_QUICKSTART.md](MVP_QUICKSTART.md)** - Get started in 5 minutes
- **[PRODUCTION_IMPLEMENTATION_PLAN.md](PRODUCTION_IMPLEMENTATION_PLAN.md)** - Complete system design
- **[ARCHITECTURE.md](gstreamer_worker/ARCHITECTURE.md)** - Technical architecture
- **[SETUP_GUIDE.md](gstreamer_worker/SETUP_GUIDE.md)** - Third-party camera integration

## ✨ Features

### Current (MVP)
- ✅ Camera CRUD operations via REST API
- ✅ Start/stop/restart camera streams
- ✅ Real-time status monitoring
- ✅ Event logging
- ✅ Production-ready C++ GStreamer worker
- ✅ Auto-reconnection with exponential backoff
- ✅ Health monitoring
- ✅ NVIDIA hardware acceleration
- ✅ Docker deployment
- ✅ Auto-generated API documentation

### Coming Soon
- 🔄 React dashboard with live camera grid
- 🔄 WebSocket for real-time updates
- 🔄 Profile management (CompreFace integration)
- 🔄 Watchlists with alert rules
- 🔄 Webhook delivery to third-party systems
- 🔄 Authentication & authorization
- 🔄 Advanced analytics

## 📁 Project Structure

```
├── backend/              # FastAPI application
│   ├── app/
│   │   ├── api/         # API routes
│   │   ├── models/      # Database models
│   │   ├── schemas/     # Pydantic schemas
│   │   └── core/        # Config, security
│   └── requirements.txt
├── gstreamer_worker/     # C++ GStreamer worker
│   ├── src/             # C++ source code
│   ├── include/         # Header files
│   └── config/          # Camera configurations
├── frontend/             # React app (coming)
└── docker-compose.yml    # Complete stack
```

## 🛠️ Technology Stack

- **Backend**: FastAPI (Python 3.11+)
- **Database**: PostgreSQL 15
- **Cache/Queue**: Redis 7
- **Worker**: C++ 17 with GStreamer 1.24
- **Frontend**: React + TypeScript (planned)
- **Deployment**: Docker + docker-compose

## 🎮 API Endpoints

### Cameras
- `GET /api/v1/cameras` - List all cameras
- `POST /api/v1/cameras` - Create camera
- `GET /api/v1/cameras/{id}` - Get camera
- `PUT /api/v1/cameras/{id}` - Update camera
- `DELETE /api/v1/cameras/{id}` - Delete camera
- `POST /api/v1/cameras/{id}/start` - Start stream
- `POST /api/v1/cameras/{id}/stop` - Stop stream
- `GET /api/v1/cameras/{id}/status` - Get status

### Events
- `GET /api/v1/events` - List events
- `GET /api/v1/events/{id}` - Get event

### System
- `GET /api/v1/system/health` - Health check
- `GET /api/v1/system/status` - System status

**Full API documentation**: http://localhost:8000/docs

## 🔧 Development

### Prerequisites
- Docker & Docker Compose
- NVIDIA GPU + drivers (for hardware acceleration)
- NVIDIA Container Toolkit
- Python 3.11+ (for local development)
- CMake 3.16+ (for C++ worker)

### Local Development Setup

```bash
# Backend
cd backend
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
uvicorn app.main:app --reload

# C++ Worker
cd gstreamer_worker
./build.sh
./build/gstreamer_worker_multi --config config/cameras.json
```

## 🚢 Production Deployment

1. Set production environment variables
2. Use docker-compose for deployment
3. Configure nginx reverse proxy for HTTPS
4. Set up monitoring (Prometheus/Grafana)
5. Configure backup/recovery

See deployment guide (coming soon).

## 🤝 Contributing

This is a production system under active development. Features are added incrementally based on the roadmap.

## 📝 License

MIT License - See LICENSE file

## 🆘 Support

- API Documentation: http://localhost:8000/docs
- Health Check: http://localhost:8000/health
- Issues: GitHub Issues (if public repo)

## 🎯 Roadmap

### Phase 1: MVP (Current)
- [x] Core camera management
- [x] C++ GStreamer worker
- [x] REST API
- [x] Docker deployment

### Phase 2: Web Interface
- [ ] React dashboard
- [ ] Real-time WebSocket
- [ ] Camera grid view
- [ ] Event timeline

### Phase 3: Face Recognition
- [ ] CompreFace integration
- [ ] Profile management
- [ ] Watchlists
- [ ] Alert system

### Phase 4: Advanced Features
- [ ] Webhooks & integrations
- [ ] Authentication & roles
- [ ] Analytics dashboard
- [ ] Mobile app (optional)

---

**Built with ❤️ using FastAPI + GStreamer + React**
