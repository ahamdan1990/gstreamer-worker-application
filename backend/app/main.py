"""
FastAPI Main Application - Camera Management System MVP
"""
from typing import Optional
from fastapi import FastAPI, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from fastapi.staticfiles import StaticFiles
from contextlib import asynccontextmanager
from sqlalchemy import select
import logging
import subprocess
import asyncio
import time
import os
from pathlib import Path
from datetime import datetime
import traceback

from app.core.config import settings
from app.db.base import get_db
from app.models.camera import Camera
from app.services.worker_client import WorkerClient, WorkerCameraConfig
from app.schemas.event import MotionEventWebhook
from app.services.websocket_client import WorkerWebSocketClient
from app.services.event_handlers import event_handler
from app.services.state_cache import camera_state_cache

# Create logs directory if it doesn't exist (relative to backend directory)
log_dir = Path(__file__).parent.parent / 'logs'
log_dir.mkdir(exist_ok=True)
log_file = log_dir / 'fastapi.log'

# Configure comprehensive logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s | %(levelname)-8s | %(name)-20s | %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)

logger = logging.getLogger(__name__)

# Also log to file
file_handler = logging.FileHandler(str(log_file))
file_handler.setFormatter(logging.Formatter(
    '%(asctime)s | %(levelname)-8s | %(name)-20s | %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
))
logger.addHandler(file_handler)

# Global worker process and state
worker_process = None
ws_client: Optional[WorkerWebSocketClient] = None
sync_lock = asyncio.Lock()  # Prevent concurrent sync operations
monitor_task: Optional[asyncio.Task] = None
should_monitor = True


async def start_worker_process():
    """Start the C++ worker process"""
    global worker_process

    if not settings.WORKER_AUTO_START:
        logger.info("Worker auto-start is disabled")
        return False

    # Check if executable exists
    worker_path = Path(settings.WORKER_EXECUTABLE)
    if not worker_path.is_absolute():
        # Make path relative to backend directory
        worker_path = Path(__file__).parent.parent / worker_path

    if not worker_path.exists():
        logger.error(f"Worker executable not found at: {worker_path}")
        return False

    try:
        logger.info(f"Starting C++ worker process: {worker_path}")

        # Start worker with FastAPI URL for callback
        fastapi_url = f"http://{settings.API_HOST}:{settings.API_PORT}"
        if settings.API_HOST == "0.0.0.0":
            fastapi_url = f"http://192.168.0.24:{settings.API_PORT}"

        worker_process = subprocess.Popen(
            [
                str(worker_path),
                "--port", str(settings.WORKER_PORT),
                "--fastapi-url", fastapi_url
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1
        )

        logger.info(f"Worker process started with PID: {worker_process.pid}")

        # Start background thread to read worker output
        def read_worker_output():
            """Read and log worker stdout in background"""
            try:
                for line in iter(worker_process.stdout.readline, ''):
                    if line:
                        # Log worker output with [WORKER] prefix
                        logger.info(f"[WORKER] {line.rstrip()}")
            except Exception as e:
                logger.error(f"Error reading worker output: {e}")

        import threading
        output_thread = threading.Thread(target=read_worker_output, daemon=True)
        output_thread.start()

        # Wait for worker to become healthy (with timeout)
        max_wait = 10  # seconds
        start_time = time.time()
        while time.time() - start_time < max_wait:
            try:
                worker = WorkerClient(base_url=settings.WORKER_API_URL)
                if await worker.health_check():
                    logger.info("Worker is healthy and ready")
                    return True
            except Exception:
                pass
            await asyncio.sleep(0.5)

        logger.warning("Worker started but health check timed out")
        return True  # Process started, even if health check failed

    except Exception as e:
        logger.error(f"Failed to start worker process: {e}")
        return False


async def stop_worker_process():
    """Stop the C++ worker process"""
    global worker_process

    if worker_process is None:
        return

    try:
        logger.info(f"Stopping worker process (PID: {worker_process.pid})")

        # Send SIGTERM for graceful shutdown
        worker_process.terminate()

        # Wait up to 5 seconds for graceful shutdown
        try:
            worker_process.wait(timeout=5)
            logger.info("Worker process stopped gracefully")
        except subprocess.TimeoutExpired:
            logger.warning("Worker did not stop gracefully, forcing kill")
            worker_process.kill()
            worker_process.wait()
            logger.info("Worker process killed")

    except Exception as e:
        logger.error(f"Error stopping worker process: {e}")
    finally:
        worker_process = None


async def monitor_worker_health():
    """
    Background task to monitor worker process health and restart if needed.
    Only runs if WORKER_AUTO_RESTART is enabled.
    """
    global worker_process, should_monitor

    if not settings.WORKER_AUTO_RESTART:
        logger.info("Worker auto-restart is disabled")
        return

    logger.info("Starting worker health monitor (checking every 10 seconds)")

    while should_monitor:
        try:
            await asyncio.sleep(10)  # Check every 10 seconds

            if not should_monitor:
                break

            # Check if worker process is still alive
            if worker_process is not None:
                poll_result = worker_process.poll()
                if poll_result is not None:
                    # Process has exited
                    logger.error(f"Worker process died with exit code {poll_result}. Restarting...")

                    # Try to restart
                    success = await start_worker_process()
                    if success:
                        logger.info("Worker successfully restarted")
                        # Wait for worker to initialize
                        await asyncio.sleep(2)
                        # Re-sync cameras
                        await sync_cameras_to_worker()
                        # Restart WebSocket connection
                        if ws_client:
                            await ws_client.stop()
                            await asyncio.sleep(1)
                            await ws_client.start()
                    else:
                        logger.error("Failed to restart worker, will retry on next check")
            else:
                # Worker process object is None, try to start it
                logger.warning("Worker process is None, attempting to start...")
                await start_worker_process()

        except Exception as e:
            logger.error(f"Error in worker health monitor: {e}", exc_info=True)
            # Continue monitoring even if there's an error
            await asyncio.sleep(5)


async def initialize_websocket_client():
    """
    Initialize and start WebSocket client for real-time events from worker.
    Replaces HTTP polling with push-based architecture.
    """
    global ws_client

    # Use dedicated WebSocket URL (production: ws://localhost:8082/ws)
    worker_ws_url = settings.WORKER_WS_URL

    logger.info(f"Initializing WebSocket client: {worker_ws_url}")

    # Create WebSocket client with event handler callback
    ws_client = WorkerWebSocketClient(
        worker_ws_url=worker_ws_url,
        on_event=event_handler.handle_event,
        reconnect_delay=2.0,
        max_reconnect_delay=60.0
    )

    # Start the client (non-blocking)
    await ws_client.start()
    logger.info("WebSocket client started successfully")


async def sync_cameras_to_worker():
    """Synchronize cameras from database to worker"""
    # Prevent concurrent sync operations
    if sync_lock.locked():
        logger.warning("Sync already in progress, skipping duplicate sync request")
        return {"status": "skipped", "message": "Sync already in progress"}

    async with sync_lock:
        logger.info("Synchronizing cameras from database to worker...")
        try:
            worker = WorkerClient(base_url=settings.WORKER_API_URL)

            # Check if worker is healthy
            if not await worker.health_check():
                logger.error("Worker is not healthy, cannot sync cameras")
                return {"status": "error", "message": "Worker not healthy"}

            # Get database session
            async for db in get_db():
                result = await db.execute(select(Camera))
                cameras = result.scalars().all()

                logger.info(f"Found {len(cameras)} cameras in database")

                synced_count = 0
                started_count = 0

                # Sync each camera to worker
                for camera in cameras:
                    try:
                        # Build motion detection config if enabled
                        motion_config = None
                        if camera.motion_detection_enabled and camera.motion_detection_config:
                            from app.services.worker_client import MotionDetectionConfig as WorkerMotionConfig
                            motion_config = WorkerMotionConfig(**camera.motion_detection_config)

                        # Build face detection config if enabled
                        face_config = None
                        if camera.face_detection_enabled and camera.face_detection_config:
                            from app.services.worker_client import FaceDetectionConfig as WorkerFaceConfig
                            face_config = WorkerFaceConfig(**camera.face_detection_config)

                        worker_config = WorkerCameraConfig(
                            camera_id=camera.camera_id,
                            rtsp_url=camera.rtsp_url,
                            username=camera.username,
                            password=camera.password,
                            protocols=camera.protocols,
                            latency_ms=camera.latency_ms,
                            target_fps=camera.target_fps,
                            enable_display=camera.enable_display,
                            use_nvidia_decoder=camera.use_nvidia_decoder,
                            motion_detection=motion_config,
                            face_detection=face_config
                        )
                        # Use add_or_update for idempotent operation
                        await worker.add_or_update_camera(worker_config)
                        logger.info(f"Synced camera {camera.camera_id} to worker")
                        synced_count += 1

                        # If camera was running before, start it
                        if camera.is_running:
                            # Give worker a moment to fully register the camera
                            await asyncio.sleep(0.5)
                            logger.info(f"Restarting camera {camera.camera_id} (was running before)")
                            await worker.start_camera(camera.camera_id)
                            started_count += 1
                    except Exception as e:
                        logger.error(f"Failed to sync camera {camera.camera_id}: {e}", exc_info=True)
                        # Try to extract more details from HTTP errors
                        if hasattr(e, 'response') and hasattr(e.response, 'text'):
                            logger.error(f"Worker response: {e.response.text}")

                break  # Only need one db session

            logger.info(f"Camera synchronization complete: {synced_count} synced, {started_count} started")
            return {
                "status": "success",
                "synced": synced_count,
                "started": started_count,
                "total": len(cameras)
            }

        except Exception as e:
            logger.error(f"Failed to synchronize cameras: {e}")
            return {"status": "error", "message": str(e)}


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Startup and shutdown events"""
    global ws_client, monitor_task, should_monitor

    # Startup: Start worker process
    logger.info("=" * 60)
    logger.info("FastAPI Application Starting")
    logger.info("=" * 60)

    # Start worker process
    await start_worker_process()

    # Wait a moment for worker to fully initialize
    await asyncio.sleep(2)

    # Synchronize cameras from database to worker
    # Note: Worker will call /api/v1/worker/ready which triggers sync
    # But we also sync here in case callback fails
    await sync_cameras_to_worker()

    # Initialize WebSocket client for real-time events
    logger.info("Starting WebSocket client for real-time events...")
    await initialize_websocket_client()

    # Wait for WebSocket to connect
    await asyncio.sleep(1)

    # Start worker health monitor
    should_monitor = True
    monitor_task = asyncio.create_task(monitor_worker_health())

    logger.info("=" * 60)
    logger.info("FastAPI Application Ready")
    logger.info(f"WebSocket connected: {ws_client.is_connected() if ws_client else False}")
    logger.info(f"Worker auto-restart: {settings.WORKER_AUTO_RESTART}")
    logger.info("=" * 60)

    yield

    # Shutdown
    logger.info("=" * 60)
    logger.info("FastAPI Application Shutting Down")
    logger.info("=" * 60)

    # Stop health monitor
    should_monitor = False
    if monitor_task and not monitor_task.done():
        monitor_task.cancel()
        try:
            await monitor_task
        except asyncio.CancelledError:
            pass

    # Stop WebSocket client
    if ws_client:
        logger.info("Stopping WebSocket client...")
        await ws_client.stop()

    # Stop worker process
    await stop_worker_process()

    logger.info("Shutdown complete")


# Initialize FastAPI app
app = FastAPI(
    title=settings.APP_NAME,
    version=settings.APP_VERSION,
    description="Production-ready camera management system with GStreamer",
    docs_url="/docs",
    redoc_url="/redoc",
    lifespan=lifespan,
)


# Global exception handler to log all errors
@app.exception_handler(Exception)
async def global_exception_handler(request: Request, exc: Exception):
    """Catch all exceptions and log full traceback"""
    logger.error(f"Unhandled exception on {request.method} {request.url.path}")
    logger.error(f"Exception type: {type(exc).__name__}")
    logger.error(f"Exception message: {str(exc)}")
    logger.error(f"Full traceback:\n{''.join(traceback.format_exception(type(exc), exc, exc.__traceback__))}")

    return JSONResponse(
        status_code=500,
        content={"detail": "Internal server error", "error": str(exc)}
    )

# Request logging middleware
@app.middleware("http")
async def log_requests(request: Request, call_next):
    """Log all HTTP requests and responses"""
    start_time = time.time()

    # Log request
    logger.info(f"→ {request.method} {request.url.path}")

    # Process request
    response = await call_next(request)

    # Log response
    process_time = (time.time() - start_time) * 1000  # ms
    logger.info(
        f"← {request.method} {request.url.path} "
        f"[{response.status_code}] {process_time:.2f}ms"
    )

    return response


# Configure CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.CORS_ORIGINS,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Mount static files for face crop images
face_crops_dir = Path(__file__).parent.parent / "face_crops"
face_crops_dir.mkdir(exist_ok=True)  # Create directory if it doesn't exist
app.mount("/face_crops", StaticFiles(directory=str(face_crops_dir)), name="face_crops")

# Health check endpoint
@app.get("/health")
async def health_check():
    """Health check endpoint"""
    return {
        "status": "healthy",
        "app": settings.APP_NAME,
        "version": settings.APP_VERSION,
    }

# Root endpoint
@app.get("/")
async def root():
    """Root endpoint with API info"""
    return {
        "message": f"Welcome to {settings.APP_NAME}",
        "version": settings.APP_VERSION,
        "docs": "/docs",
        "api": settings.API_PREFIX,
    }


# Worker callback endpoint
@app.post(f"{settings.API_PREFIX}/worker/ready")
async def worker_ready():
    """
    Called by C++ Worker on startup to trigger camera synchronization.
    This ensures cameras are automatically re-synced when worker restarts.
    """
    logger.info("Worker ready notification received, triggering camera sync...")
    result = await sync_cameras_to_worker()
    return result


# Manual sync endpoint (backup)
@app.post(f"{settings.API_PREFIX}/sync")
async def manual_sync():
    """
    Manually trigger camera synchronization to worker.
    Useful for debugging or manual recovery.
    """
    logger.info("Manual sync triggered...")
    result = await sync_cameras_to_worker()
    return result


# Motion event webhook
@app.post(f"{settings.API_PREFIX}/worker/motion-event")
async def motion_event_webhook(event: MotionEventWebhook):
    """
    Webhook endpoint for motion detection events from C++ Worker.
    Stores motion events in the database for later querying.
    """
    from app.models.event import Event

    logger.info(
        f"Motion event received: camera={event.camera_id}, "
        f"area={event.motion_area}px, contours={event.num_contours}, "
        f"confidence={event.confidence:.2f}"
    )

    try:
        # Get camera UUID from camera_id
        async for db in get_db():
            result = await db.execute(
                select(Camera).where(Camera.camera_id == event.camera_id)
            )
            camera = result.scalar_one_or_none()

            if not camera:
                logger.warning(f"Motion event for unknown camera: {event.camera_id}")
                return {"status": "error", "message": "Camera not found"}

            # Store motion event
            motion_event = Event(
                event_type="motion_detected",
                camera_id=camera.id,
                detected_face=False,
                confidence=event.confidence,
                bounding_box=event.bounding_box,
                extra_metadata={
                    "motion_area": event.motion_area,
                    "num_contours": event.num_contours,
                    "timestamp": event.timestamp
                }
            )

            db.add(motion_event)
            await db.commit()

            logger.info(f"Motion event stored in database for camera {event.camera_id}")
            return {"status": "success", "event_id": str(motion_event.id)}

    except Exception as e:
        logger.error(f"Failed to store motion event: {e}")
        return {"status": "error", "message": str(e)}


# System status endpoint
@app.get(f"{settings.API_PREFIX}/system/status")
async def get_system_status():
    """
    Get system-wide status including total cameras and running count.
    Used by frontend dashboard for overview stats.
    """
    try:
        # Get database session
        async for db in get_db():
            result = await db.execute(select(Camera))
            cameras = result.scalars().all()

            total_cameras = len(cameras)
            running_cameras = sum(1 for cam in cameras if cam.is_running)

            return {
                "running": True,  # System is running if we can respond
                "total_cameras": total_cameras,
                "running_cameras": running_cameras,
                "timestamp": int(time.time())
            }
    except Exception as e:
        logger.error(f"Failed to get system status: {e}")
        return {
            "running": False,
            "total_cameras": 0,
            "running_cameras": 0,
            "timestamp": int(time.time())
        }


# Import and include routers
from app.api.routes import cameras, profiles, tracking, watchlists, webhooks, events, settings as settings_routes

app.include_router(
    cameras.router,
    prefix=f"{settings.API_PREFIX}/cameras",
    tags=["cameras"]
)

app.include_router(
    profiles.router,
    prefix=f"{settings.API_PREFIX}/profiles",
    tags=["profiles"]
)

app.include_router(
    tracking.router,
    prefix=f"{settings.API_PREFIX}/tracking",
    tags=["tracking"]
)

app.include_router(
    watchlists.router,
    prefix=f"{settings.API_PREFIX}/watchlists",
    tags=["watchlists"]
)

app.include_router(
    webhooks.router,
    prefix=f"{settings.API_PREFIX}/webhooks",
    tags=["webhooks"]
)

app.include_router(
    events.router,
    prefix=f"{settings.API_PREFIX}/events",
    tags=["events"]
)

app.include_router(
    settings_routes.router,
    prefix=f"{settings.API_PREFIX}/settings",
    tags=["settings"]
)

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(
        "app.main:app",
        host=settings.API_HOST,
        port=settings.API_PORT,
        reload=settings.DEBUG,
    )
