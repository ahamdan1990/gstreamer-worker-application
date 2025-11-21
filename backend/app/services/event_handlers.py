"""
Event Handlers for WebSocket Events from C++ Worker
Processes real-time events and updates state cache
"""
import logging
from typing import Dict, Any
from app.services.state_cache import camera_state_cache

logger = logging.getLogger(__name__)


class EventHandler:
    """
    Handles WebSocket events from C++ worker.
    Updates cache and triggers any additional actions (webhooks, notifications, etc.)
    """

    def __init__(self):
        self.handlers = {
            "connected": self.handle_connected,
            "worker_started": self.handle_worker_started,
            "worker_heartbeat": self.handle_worker_heartbeat,
            "camera_started": self.handle_camera_started,
            "camera_stopped": self.handle_camera_stopped,
            "camera_reconnecting": self.handle_camera_reconnecting,
            "camera_error": self.handle_camera_error,
            "camera_status": self.handle_camera_status,
            "motion_detected": self.handle_motion_detected,
            "fps_drop": self.handle_fps_drop,
            "pipeline_crashed": self.handle_pipeline_crashed,
            "pipeline_recovered": self.handle_pipeline_recovered,
        }

        # Statistics
        self.events_processed = 0
        self.events_by_type: Dict[str, int] = {}

    async def handle_event(self, event: Dict[str, Any]):
        """
        Dispatch event to appropriate handler

        Args:
            event: Event dictionary with 'event_type', 'data', optional 'camera_id'
        """
        event_type = event.get("event_type", "unknown")
        self.events_processed += 1
        self.events_by_type[event_type] = self.events_by_type.get(event_type, 0) + 1

        handler = self.handlers.get(event_type)
        if handler:
            try:
                await handler(event)
            except Exception as e:
                logger.error(f"Error handling event {event_type}: {e}", exc_info=True)
        else:
            logger.warning(f"No handler for event type: {event_type}")

    # WebSocket Connection Events

    async def handle_connected(self, event: Dict[str, Any]):
        """Handle WebSocket connected event"""
        data = event.get("data", {})
        logger.info(f"WebSocket connected to worker: {data.get('message', 'Connected')}")

    # Worker System Events

    async def handle_worker_started(self, event: Dict[str, Any]):
        """Handle worker_started event"""
        data = event.get("data", {})
        logger.info(f"Worker started - Version: {data.get('version')}, PID: {data.get('pid')}")

        await camera_state_cache.update_worker_state({
            "status": "started",
            "version": data.get("version"),
            "pid": data.get("pid")
        })

    async def handle_worker_heartbeat(self, event: Dict[str, Any]):
        """Handle worker_heartbeat event"""
        data = event.get("data", {})

        await camera_state_cache.update_worker_state({
            "status": "healthy",
            "uptime_seconds": data.get("uptime_seconds", 0),
            "total_cameras": data.get("total_cameras", 0),
            "running_cameras": data.get("running_cameras", 0)
        })

        logger.debug(f"Worker heartbeat - Uptime: {data.get('uptime_seconds', 0):.1f}s, "
                    f"Cameras: {data.get('running_cameras', 0)}/{data.get('total_cameras', 0)}")

    # Camera Lifecycle Events

    async def handle_camera_started(self, event: Dict[str, Any]):
        """Handle camera_started event"""
        camera_id = event.get("camera_id")
        data = event.get("data", {})

        if not camera_id:
            logger.error("camera_started event missing camera_id")
            return

        logger.info(f"Camera started: {camera_id}")

        await camera_state_cache.update_camera(camera_id, {
            "state": "RUNNING",
            "is_running": True,
            "rtsp_url": data.get("rtsp_url")
        })

    async def handle_camera_stopped(self, event: Dict[str, Any]):
        """Handle camera_stopped event"""
        camera_id = event.get("camera_id")
        data = event.get("data", {})

        if not camera_id:
            logger.error("camera_stopped event missing camera_id")
            return

        logger.info(f"Camera stopped: {camera_id} - Reason: {data.get('reason', 'unknown')}")

        await camera_state_cache.update_camera(camera_id, {
            "state": "STOPPED",
            "is_running": False,
            "stop_reason": data.get("reason"),
            "uptime_seconds": data.get("uptime_seconds", 0)
        })

    async def handle_camera_reconnecting(self, event: Dict[str, Any]):
        """Handle camera_reconnecting event"""
        camera_id = event.get("camera_id")
        data = event.get("data", {})

        if not camera_id:
            logger.error("camera_reconnecting event missing camera_id")
            return

        logger.warning(f"Camera reconnecting: {camera_id} - "
                      f"Attempt {data.get('attempt')}/{data.get('max_attempts')}")

        await camera_state_cache.update_camera(camera_id, {
            "state": "RECONNECTING",
            "is_running": False,
            "reconnect_attempt": data.get("attempt"),
            "max_reconnect_attempts": data.get("max_attempts")
        })

    async def handle_camera_error(self, event: Dict[str, Any]):
        """Handle camera_error event"""
        camera_id = event.get("camera_id")
        data = event.get("data", {})

        if not camera_id:
            logger.error("camera_error event missing camera_id")
            return

        error_msg = data.get("error_message", "Unknown error")
        logger.error(f"Camera error: {camera_id} - {error_msg}")

        await camera_state_cache.update_camera(camera_id, {
            "state": "ERROR",
            "is_running": False,
            "error_message": error_msg,
            "error_code": data.get("error_code")
        })

    async def handle_camera_status(self, event: Dict[str, Any]):
        """Handle camera_status update event"""
        camera_id = event.get("camera_id")
        data = event.get("data", {})

        if not camera_id:
            logger.error("camera_status event missing camera_id")
            return

        await camera_state_cache.update_camera(camera_id, {
            "state": data.get("state"),
            "metrics": data.get("metrics", {})
        })

    # Motion Detection Events

    async def handle_motion_detected(self, event: Dict[str, Any]):
        """Handle motion_detected event"""
        camera_id = event.get("camera_id")
        data = event.get("data", {})

        if not camera_id:
            logger.error("motion_detected event missing camera_id")
            return

        logger.info(f"Motion detected: {camera_id} - "
                   f"Area: {data.get('motion_area')}px, "
                   f"Confidence: {data.get('confidence', 0):.2f}")

        # Note: Motion events could be stored in database here
        # For now, just log them. The C++ worker also sends HTTP webhooks.

    # Performance Events

    async def handle_fps_drop(self, event: Dict[str, Any]):
        """Handle fps_drop event"""
        camera_id = event.get("camera_id")
        data = event.get("data", {})

        if not camera_id:
            logger.error("fps_drop event missing camera_id")
            return

        logger.warning(f"FPS drop: {camera_id} - "
                      f"{data.get('current_fps', 0):.1f}/{data.get('target_fps', 0):.1f} FPS "
                      f"({data.get('drop_percentage', 0):.1f}% drop)")

    # Pipeline Events

    async def handle_pipeline_crashed(self, event: Dict[str, Any]):
        """Handle pipeline_crashed event"""
        camera_id = event.get("camera_id")
        data = event.get("data", {})

        if not camera_id:
            logger.error("pipeline_crashed event missing camera_id")
            return

        error = data.get("error", "Unknown error")
        logger.error(f"Pipeline crashed: {camera_id} - {error}")

        await camera_state_cache.update_camera(camera_id, {
            "state": "ERROR",
            "is_running": False,
            "error_message": f"Pipeline crashed: {error}"
        })

    async def handle_pipeline_recovered(self, event: Dict[str, Any]):
        """Handle pipeline_recovered event"""
        camera_id = event.get("camera_id")
        data = event.get("data", {})

        if not camera_id:
            logger.error("pipeline_recovered event missing camera_id")
            return

        recovery_time = data.get("recovery_time_seconds", 0)
        logger.info(f"Pipeline recovered: {camera_id} - "
                   f"Recovery time: {recovery_time:.2f}s")

        await camera_state_cache.update_camera(camera_id, {
            "state": "RUNNING",
            "is_running": True,
            "last_recovery_time": recovery_time
        })

    def get_stats(self) -> Dict[str, Any]:
        """Get event handler statistics"""
        return {
            "events_processed": self.events_processed,
            "events_by_type": dict(self.events_by_type)
        }


# Global event handler instance
event_handler = EventHandler()
