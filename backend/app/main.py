"""
FastAPI Main Application - Camera Management System MVP
"""
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from contextlib import asynccontextmanager
from sqlalchemy import select
import logging
import subprocess
import asyncio
import time
import os
from pathlib import Path

from app.core.config import settings
from app.db.base import get_db
from app.models.camera import Camera
from app.services.worker_client import WorkerClient, WorkerCameraConfig

logger = logging.getLogger(__name__)

# Global worker process
worker_process = None


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
            fastapi_url = f"http://localhost:{settings.API_PORT}"

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


async def sync_cameras_to_worker():
    """Synchronize cameras from database to worker"""
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
                    worker_config = WorkerCameraConfig(
                        camera_id=camera.camera_id,
                        rtsp_url=camera.rtsp_url,
                        username=camera.username,
                        password=camera.password,
                        protocols=camera.protocols,
                        latency_ms=camera.latency_ms,
                        target_fps=camera.target_fps,
                        enable_display=camera.enable_display,
                        use_nvidia_decoder=camera.use_nvidia_decoder
                    )
                    await worker.add_camera(worker_config)
                    logger.info(f"Synced camera {camera.camera_id} to worker")
                    synced_count += 1

                    # If camera was running before, start it
                    if camera.is_running:
                        logger.info(f"Restarting camera {camera.camera_id} (was running before)")
                        await worker.start_camera(camera.camera_id)
                        started_count += 1
                except Exception as e:
                    logger.error(f"Failed to sync camera {camera.camera_id}: {e}")

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

    logger.info("=" * 60)
    logger.info("FastAPI Application Ready")
    logger.info("=" * 60)

    yield

    # Shutdown
    logger.info("=" * 60)
    logger.info("FastAPI Application Shutting Down")
    logger.info("=" * 60)

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

# Configure CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.CORS_ORIGINS,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

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


# Import and include routers
from app.api.routes import cameras

app.include_router(
    cameras.router,
    prefix=f"{settings.API_PREFIX}/cameras",
    tags=["cameras"]
)

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(
        "app.main:app",
        host=settings.API_HOST,
        port=settings.API_PORT,
        reload=settings.DEBUG,
    )
