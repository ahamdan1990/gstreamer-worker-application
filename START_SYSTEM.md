# GStreamer Camera Manager - Simplified Start Guide

## 🚀 One Command to Rule Them All

FastAPI now **manages everything** - just start FastAPI and it handles the rest!

## Quick Start

```bash
./START.sh
```

That's it! FastAPI will:
1. ✅ Check PostgreSQL is running
2. ✅ Start C++ Worker automatically
3. ✅ Wait for worker to be healthy
4. ✅ Sync all cameras from database
5. ✅ Start cameras that were running before
6. ✅ Monitor and manage everything

## What FastAPI Manages

```
┌─────────────────────────────────────────┐
│         FastAPI (Process Manager)       │
│              Port 8002                  │
├─────────────────────────────────────────┤
│  ┌─────────────────────────────────┐   │
│  │   C++ Worker (subprocess)       │   │
│  │   - Auto-started on startup     │   │
│  │   - Auto-stopped on shutdown    │   │
│  │   - Health monitored            │   │
│  └─────────────────────────────────┘   │
│                 │                        │
│                 ├──► PostgreSQL         │
│                 └──► Camera Pipelines   │
└─────────────────────────────────────────┘
```

## Manual Start (if preferred)

```bash
cd backend
source venv/bin/activate
uvicorn app.main:app --host 0.0.0.0 --port 8002
```

## Stopping the System

Just press **Ctrl+C** in the FastAPI terminal.

FastAPI will:
1. Gracefully stop the worker (SIGTERM)
2. Wait up to 5 seconds for clean shutdown
3. Force kill if needed (SIGKILL)
4. Clean up all resources

## Configuration

Edit `backend/.env`:

```env
# Worker Management
WORKER_EXECUTABLE=../gstreamer_worker/build/gstreamer_worker_api
WORKER_AUTO_START=True          # Auto-start worker on FastAPI startup
WORKER_AUTO_RESTART=True        # Auto-restart worker if it crashes (future)
WORKER_HOST=localhost
WORKER_PORT=8081
```

## Startup Sequence

When you run FastAPI, this happens:

```
1. FastAPI starts
   ↓
2. FastAPI spawns worker subprocess
   └─► ./gstreamer_worker_api --port 8081 --fastapi-url http://localhost:8002
   ↓
3. Worker starts API server
   ↓
4. Worker notifies FastAPI: POST /api/v1/worker/ready
   ↓
5. FastAPI loads cameras from PostgreSQL
   ↓
6. FastAPI syncs cameras to worker
   ↓
7. FastAPI starts cameras that were running
   ↓
8. System ready! 🎉
```

## Logs You'll See

```
============================================================
FastAPI Application Starting
============================================================
INFO: Starting C++ worker process: ../gstreamer_worker/build/gstreamer_worker_api
INFO: Worker process started with PID: 12345
INFO: Worker is healthy and ready
INFO: Worker ready notification received, triggering camera sync...
INFO: Synchronizing cameras from database to worker...
INFO: Found 1 cameras in database
INFO: Synced camera test_camera_1 to worker
INFO: Restarting camera test_camera_1 (was running before)
INFO: Camera synchronization complete: 1 synced, 1 started
============================================================
FastAPI Application Ready
============================================================
```

## Benefits of This Approach

✅ **Single Command** - No more managing multiple terminals
✅ **Automatic Lifecycle** - Worker starts with FastAPI, stops with FastAPI
✅ **Graceful Shutdown** - Proper cleanup of all resources
✅ **Simple Deployment** - Just deploy FastAPI, it handles the rest
✅ **Process Supervision** - FastAPI monitors worker health
✅ **No Manual Steps** - Everything is automatic

## API Endpoints

### FastAPI (Port 8002)
- `GET  /health` - Health check
- `GET  /api/v1/cameras/` - List cameras
- `POST /api/v1/cameras/` - Create camera
- `POST /api/v1/cameras/{id}/start` - Start camera
- `POST /api/v1/cameras/{id}/stop` - Stop camera
- `DELETE /api/v1/cameras/{id}` - Delete camera
- `POST /api/v1/worker/ready` - Worker callback (automatic)
- `POST /api/v1/sync` - Manual sync (backup)

### C++ Worker (Port 8081)
Managed automatically by FastAPI - no need to interact directly!

## Disabling Auto-Start

If you want to manage the worker manually:

```env
# In backend/.env
WORKER_AUTO_START=False
```

Then start worker separately:
```bash
cd gstreamer_worker/build
./gstreamer_worker_api --port 8081 --fastapi-url http://localhost:8002
```

## Troubleshooting

**Worker not starting:**
- Check `WORKER_EXECUTABLE` path in `.env`
- Check worker was built: `ls gstreamer_worker/build/gstreamer_worker_api`
- Check logs for errors

**Port already in use:**
- Kill existing processes: `pkill -f gstreamer_worker_api`
- Change port in `.env`

**Cameras not syncing:**
- Check PostgreSQL is running: `docker ps | grep postgres`
- Check worker health: `curl http://localhost:8081/health`
- Manually trigger sync: `curl -X POST http://localhost:8002/api/v1/sync`

## Production Deployment

For production, use a process manager like systemd or Docker:

### Option 1: Systemd Service

```ini
[Unit]
Description=GStreamer Camera Manager
After=network.target postgresql.service

[Service]
Type=simple
User=your-user
WorkingDirectory=/path/to/backend
ExecStart=/path/to/venv/bin/uvicorn app.main:app --host 0.0.0.0 --port 8002
Restart=always

[Install]
WantedBy=multi-user.target
```

### Option 2: Docker Compose

```yaml
version: '3.8'
services:
  app:
    build: ./backend
    ports:
      - "8002:8002"
    depends_on:
      - postgres
    environment:
      - WORKER_AUTO_START=True
```

## Next Steps

- [ ] Add automatic worker restart on crash
- [ ] Add worker health monitoring endpoint
- [ ] Add worker log streaming to FastAPI
- [ ] Add React frontend
- [ ] Integrate CompreFace
- [ ] Add webhooks and events
