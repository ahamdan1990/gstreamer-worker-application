"""
Event Handlers for WebSocket Events from C++ Worker
Processes real-time events and updates state cache
"""
import logging
from typing import Dict, Any, Optional
from datetime import datetime
from pathlib import Path
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from app.services.state_cache import camera_state_cache
from app.db.base import async_session
from app.models.profile import Profile
from app.models.person_event import PersonEvent
from app.models.face_detection_event import FaceDetectionEvent
from app.services.webhook_service import webhook_service

logger = logging.getLogger(__name__)


def convert_absolute_path_to_url(absolute_path: Optional[str]) -> Optional[str]:
    """
    Convert absolute file path to relative URL for static file serving.

    Example:
        /home/user/backend/face_crops/camera_123.jpg -> /face_crops/camera_123.jpg
        ./face_crops/camera_123.jpg -> /face_crops/camera_123.jpg

    Args:
        absolute_path: Absolute or relative file path

    Returns:
        Relative URL path or None if input is None/empty
    """
    if not absolute_path:
        return None

    try:
        # Extract just the filename
        path = Path(absolute_path)
        filename = path.name

        # Return relative URL for static file server
        return f"/face_crops/{filename}"
    except Exception as e:
        logger.error(f"Failed to convert path '{absolute_path}': {e}")
        return None


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
            "person_recognized": self.handle_person_recognized,
            "face_detected": self.handle_face_detected,
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
            # logger.warning(f"No handler for event type: {event_type}")
            logger.warning(".")

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

    # Person Recognition Events

    async def handle_person_recognized(self, event: Dict[str, Any]):
        """
        Handle person_recognized event from C++ worker

        1. Find or auto-create profile
        2. Store PersonEvent in database
        3. Update profile statistics
        """
        camera_id = event.get("camera_id")
        data = event.get("data", {})

        if not camera_id:
            logger.error("person_recognized event missing camera_id")
            return

        subject = data.get("subject")
        if not subject:
            logger.error("person_recognized event missing subject")
            return

        similarity = data.get("similarity", 0.0)
        detection_confidence = data.get("detection_confidence", 0.0)
        is_new_person = data.get("is_new_person", False)
        is_new_at_camera = data.get("is_new_at_camera", False)
        dwell_time_seconds = data.get("dwell_time_seconds", 0.0)
        other_cameras = data.get("other_cameras", [])

        # Convert absolute file path to relative URL
        image_url = convert_absolute_path_to_url(data.get("image_url"))

        logger.info(f"Person recognized: {subject} at {camera_id} "
                   f"(similarity: {similarity:.2f}, new_person: {is_new_person})")

        try:
            async with async_session() as db:
                # Find or create profile
                result = await db.execute(
                    select(Profile).where(Profile.compreface_subject_id == subject)
                )
                profile = result.scalar_one_or_none()

                if not profile:
                    # Auto-create profile for unknown person
                    logger.info(f"Auto-creating profile for subject: {subject}")
                    profile = Profile(
                        name=f"Person {subject[:8]}",  # Use first 8 chars of subject ID
                        compreface_subject_id=subject,
                        description="Auto-created from face recognition",
                        is_active=True,
                        auto_created=True,
                        alert_on_recognition=False,
                        total_detections=0,
                        avg_recognition_similarity=0.0,
                        images=[],
                        tags=["auto-created"]
                    )
                    db.add(profile)
                    await db.flush()  # Get profile.id

                # Create PersonEvent
                person_event = PersonEvent(
                    profile_id=profile.id,
                    camera_id=camera_id,
                    subject=subject,
                    similarity=similarity,
                    detection_confidence=detection_confidence,
                    is_new_person=is_new_person,
                    is_new_at_camera=is_new_at_camera,
                    dwell_time_seconds=dwell_time_seconds,
                    other_cameras=other_cameras,
                    image_url=image_url,  # Face crop image path
                    worker_metadata=data,  # Store full event data
                    alert_sent=False,
                    acknowledged=False
                )
                db.add(person_event)

                # Update profile statistics
                profile.total_detections += 1
                profile.last_seen_at = datetime.utcnow()
                profile.last_seen_camera_id = camera_id

                # Update rolling average similarity
                if profile.avg_recognition_similarity == 0.0:
                    profile.avg_recognition_similarity = similarity
                else:
                    # Exponential moving average (weight recent detections more)
                    alpha = 0.1  # Smoothing factor
                    profile.avg_recognition_similarity = (
                        alpha * similarity +
                        (1 - alpha) * profile.avg_recognition_similarity
                    )

                await db.commit()

                logger.info(f"Stored PersonEvent for {subject} (total: {profile.total_detections})")

                # Send webhook notification (runs outside DB transaction)
                try:
                    await webhook_service.send_person_recognized_event(
                        event_data=data,
                        camera_id=camera_id,
                        subject=subject,
                        similarity=similarity,
                        profile_id=str(profile.id)
                    )
                except Exception as webhook_error:
                    logger.error(f"Error sending webhook for person_recognized: {webhook_error}", exc_info=True)

        except Exception as e:
            logger.error(f"Error handling person_recognized event: {e}", exc_info=True)

    async def handle_face_detected(self, event: Dict[str, Any]):
        """
        Handle face_detected event with tracking information

        - Saves to database with tracking ID
        - Converts image paths to relative URLs
        - Updates existing tracking session if recognized
        """
        camera_id = event.get("camera_id")
        data = event.get("data", {})

        if not camera_id:
            logger.error("face_detected event missing camera_id")
            return

        # Extract tracking information
        tracking_id = data.get("tracking_id", "")
        subject = data.get("subject", "unknown")
        is_recognized = data.get("is_recognized", False)
        recognition_attempts = data.get("recognition_attempts", 0)
        tracking_duration = data.get("tracking_duration_seconds", 0.0)
        is_first_detection = data.get("is_first_detection", True)
        detection_confidence = data.get("confidence", 0.0)
        recognition_similarity = data.get("similarity", 0.0)
        # Use similarity for recognized faces, detection confidence for unknown faces
        confidence = recognition_similarity if is_recognized else detection_confidence
        detection_count = data.get("detection_count", 0)

        # Convert absolute file path to relative URL
        image_url = convert_absolute_path_to_url(data.get("image_url"))

        # Log the detection with tracking info
        logger.info(f"Face detected at {camera_id} "
                   f"(tracking_id: {tracking_id[:8]}..., "
                   f"subject: {subject}, "
                   f"recognized: {is_recognized}, "
                   f"attempts: {recognition_attempts}/3, "
                   f"duration: {tracking_duration:.1f}s, "
                   f"first: {is_first_detection})")

        try:
            async with async_session() as db:
                # Check if we already have an event for this tracking_id
                from sqlalchemy import select
                result = await db.execute(
                    select(FaceDetectionEvent).where(
                        FaceDetectionEvent.tracking_id == tracking_id
                    )
                )
                existing_event = result.scalar_one_or_none()

                if existing_event:
                    # Update existing event with new information
                    existing_event.subject = subject
                    existing_event.is_recognized = is_recognized
                    existing_event.confidence = confidence  # Update with recognition similarity if recognized
                    existing_event.recognition_attempts = recognition_attempts
                    existing_event.tracking_duration_seconds = tracking_duration
                    existing_event.updated_at = datetime.utcnow()
                    existing_event.worker_metadata = data  # Update full event data

                    logger.info(f"Updated face detection event for tracking_id {tracking_id[:8]}... "
                              f"(subject: {subject}, recognized: {is_recognized})")
                else:
                    # Create new event
                    face_event = FaceDetectionEvent(
                        tracking_id=tracking_id,
                        camera_id=camera_id,
                        confidence=confidence,
                        subject=subject,
                        is_recognized=is_recognized,
                        recognition_attempts=recognition_attempts,
                        tracking_duration_seconds=tracking_duration,
                        is_first_detection=is_first_detection,
                        image_url=image_url,
                        worker_metadata=data  # Store full event data
                    )
                    db.add(face_event)

                    logger.info(f"Stored face detection event for tracking_id {tracking_id[:8]}...")

                await db.commit()

                # Send webhook notification for face detection
                # Get profile_id if recognized
                profile_id = None
                if is_recognized and subject != "unknown":
                    try:
                        profile_result = await db.execute(
                            select(Profile).where(Profile.compreface_subject_id == subject)
                        )
                        profile = profile_result.scalar_one_or_none()
                        if profile:
                            profile_id = str(profile.id)
                    except Exception as profile_error:
                        logger.warning(f"Could not fetch profile for webhook: {profile_error}")

                # Send webhook (runs outside DB transaction)
                try:
                    await webhook_service.send_face_detected_event(
                        event_data=data,
                        camera_id=camera_id,
                        tracking_id=tracking_id,
                        subject=subject,
                        is_recognized=is_recognized,
                        similarity=recognition_similarity,
                        profile_id=profile_id
                    )
                except Exception as webhook_error:
                    logger.error(f"Error sending webhook for face_detected: {webhook_error}", exc_info=True)

        except Exception as e:
            logger.error(f"Error handling face_detected event: {e}", exc_info=True)

    def get_stats(self) -> Dict[str, Any]:
        """Get event handler statistics"""
        return {
            "events_processed": self.events_processed,
            "events_by_type": dict(self.events_by_type)
        }


# Global event handler instance
event_handler = EventHandler()
