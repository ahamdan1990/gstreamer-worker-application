"""
Webhook Service for sending events to third-party applications
"""
import logging
import asyncio
from typing import Dict, Any, List, Optional
from datetime import datetime
import httpx
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select

from app.db.base import async_session
from app.models.webhook import Webhook
from app.models.profile import Profile
from app.models.watchlist import Watchlist

logger = logging.getLogger(__name__)


class WebhookService:
    """
    Service for sending webhook notifications to external systems
    """

    def __init__(self):
        self.max_concurrent_requests = 10  # Limit concurrent webhook requests
        self.semaphore = asyncio.Semaphore(self.max_concurrent_requests)

    async def send_person_recognized_event(
        self,
        event_data: Dict[str, Any],
        camera_id: str,
        subject: str,
        similarity: float,
        profile_id: Optional[str] = None
    ):
        """
        Send person_recognized event to all matching webhooks

        Args:
            event_data: Raw event data from worker
            camera_id: Camera identifier
            subject: Person name/ID
            similarity: Recognition similarity score
            profile_id: Profile UUID if available
        """
        async with async_session() as db:
            # Get active webhooks that match this event type
            result = await db.execute(
                select(Webhook).where(Webhook.is_active == True)
            )
            webhooks = result.scalars().all()

            # Get profile details if available
            profile_details = None
            watchlists = []
            if profile_id:
                profile_result = await db.execute(
                    select(Profile).where(Profile.id == profile_id)
                )
                profile = profile_result.scalar_one_or_none()
                if profile:
                    profile_details = {
                        "id": str(profile.id),
                        "name": profile.name,
                        "external_id": profile.external_id,
                        "email": profile.email,
                        "department": profile.department,
                        "tags": profile.tags or [],
                        "notes": profile.notes,
                        "total_detections": profile.total_detections,
                        "avg_recognition_similarity": profile.avg_recognition_similarity,
                        "first_seen_at": profile.first_seen_at.isoformat() if profile.first_seen_at else None,
                        "last_seen_at": profile.last_seen_at.isoformat() if profile.last_seen_at else None,
                        "last_seen_camera_id": profile.last_seen_camera_id,
                    }

                    # Get watchlists this person belongs to
                    if profile.watchlists:
                        for watchlist in profile.watchlists:
                            watchlists.append({
                                "id": str(watchlist.id),
                                "name": watchlist.name,
                                "description": watchlist.description,
                                "alert_enabled": watchlist.alert_enabled,
                                "priority": watchlist.priority
                            })

            # Build comprehensive payload
            payload = {
                "event_type": "person_recognized",
                "event_id": f"person_{datetime.utcnow().timestamp()}",
                "timestamp": datetime.utcnow().isoformat(),
                "camera": {
                    "id": camera_id,
                    "name": camera_id  # Could be enhanced with camera name from DB
                },
                "person": {
                    "subject": subject,
                    "similarity": similarity,
                    "confidence_percentage": round(similarity * 100, 2),
                    "profile": profile_details
                },
                "watchlists": watchlists,
                "detection": {
                    "dwell_time_seconds": event_data.get("dwell_time_seconds", 0),
                    "is_new_person": event_data.get("is_new_person", False),
                    "is_new_at_camera": event_data.get("is_new_at_camera", False),
                    "detection_confidence": event_data.get("detection_confidence", 0),
                },
                "image": {
                    "url": event_data.get("image_url", ""),
                    "timestamp": event_data.get("event_time"),
                },
                "metadata": {
                    "other_cameras": event_data.get("other_cameras", []),
                    "system_name": "Face Recognition System",
                    "version": "2.0.0"
                },
                "raw_event": event_data  # Include full raw event data
            }

            # Filter and send to matching webhooks
            tasks = []
            for webhook in webhooks:
                if self._should_send_webhook(webhook, "person_recognized", camera_id, watchlists):
                    tasks.append(
                        self._send_webhook_async(webhook, payload, db)
                    )

            if tasks:
                logger.info(f"Sending person_recognized event to {len(tasks)} webhook(s)")
                await asyncio.gather(*tasks, return_exceptions=True)

    async def send_face_detected_event(
        self,
        event_data: Dict[str, Any],
        camera_id: str,
        tracking_id: str,
        subject: str,
        is_recognized: bool,
        similarity: float,
        profile_id: Optional[str] = None
    ):
        """
        Send face_detected event to all matching webhooks

        Args:
            event_data: Raw event data from worker
            camera_id: Camera identifier
            tracking_id: Unique tracking ID
            subject: Person name/ID or "unknown"
            is_recognized: Whether face was recognized
            similarity: Recognition similarity score
            profile_id: Profile UUID if available
        """
        async with async_session() as db:
            # Get active webhooks
            result = await db.execute(
                select(Webhook).where(Webhook.is_active == True)
            )
            webhooks = result.scalars().all()

            # Get profile details if recognized
            profile_details = None
            watchlists = []
            if is_recognized and profile_id:
                profile_result = await db.execute(
                    select(Profile).where(Profile.id == profile_id)
                )
                profile = profile_result.scalar_one_or_none()
                if profile:
                    profile_details = {
                        "id": str(profile.id),
                        "name": profile.name,
                        "external_id": profile.external_id,
                        "tags": profile.tags or [],
                    }

                    if profile.watchlists:
                        for watchlist in profile.watchlists:
                            watchlists.append({
                                "id": str(watchlist.id),
                                "name": watchlist.name,
                                "alert_enabled": watchlist.alert_enabled,
                            })

            # Build payload
            payload = {
                "event_type": "face_detected",
                "event_id": f"face_{tracking_id}",
                "timestamp": datetime.utcnow().isoformat(),
                "camera": {
                    "id": camera_id,
                    "name": camera_id
                },
                "tracking": {
                    "tracking_id": tracking_id,
                    "duration_seconds": event_data.get("tracking_duration_seconds", 0),
                    "recognition_attempts": event_data.get("recognition_attempts", 0),
                    "is_first_detection": event_data.get("is_first_detection", True)
                },
                "recognition": {
                    "is_recognized": is_recognized,
                    "subject": subject,
                    "similarity": similarity,
                    "confidence_percentage": round(similarity * 100, 2) if is_recognized else 0,
                    "profile": profile_details
                },
                "watchlists": watchlists,
                "image": {
                    "url": event_data.get("image_url", ""),
                    "confidence": event_data.get("confidence", 0)
                },
                "metadata": {
                    "system_name": "Face Recognition System",
                    "version": "2.0.0"
                },
                "raw_event": event_data
            }

            # Filter and send
            tasks = []
            for webhook in webhooks:
                if self._should_send_webhook(webhook, "face_detected", camera_id, watchlists):
                    tasks.append(
                        self._send_webhook_async(webhook, payload, db)
                    )

            if tasks:
                logger.info(f"Sending face_detected event to {len(tasks)} webhook(s)")
                await asyncio.gather(*tasks, return_exceptions=True)

    def _should_send_webhook(
        self,
        webhook: Webhook,
        event_type: str,
        camera_id: str,
        watchlists: List[Dict[str, Any]]
    ) -> bool:
        """
        Check if webhook should be sent based on filters

        Args:
            webhook: Webhook configuration
            event_type: Type of event
            camera_id: Camera ID
            watchlists: List of watchlists

        Returns:
            True if webhook should be sent
        """
        # Check event type filter
        if webhook.event_types and event_type not in webhook.event_types:
            return False

        # Check camera filter
        if webhook.camera_ids and camera_id not in webhook.camera_ids:
            return False

        # Check watchlist filter
        if webhook.watchlist_ids:
            watchlist_ids = [w["id"] for w in watchlists]
            if not any(wl_id in watchlist_ids for wl_id in webhook.watchlist_ids):
                return False

        return True

    async def _send_webhook_async(
        self,
        webhook: Webhook,
        payload: Dict[str, Any],
        db: AsyncSession
    ):
        """
        Send webhook request asynchronously with retry logic

        Args:
            webhook: Webhook configuration
            payload: Event payload
            db: Database session
        """
        async with self.semaphore:  # Limit concurrent requests
            # Apply custom payload template if configured
            if webhook.payload_template:
                # Merge template with payload
                final_payload = {**webhook.payload_template, **payload}
            else:
                final_payload = payload

            # Build headers
            headers = {"Content-Type": "application/json"}
            if webhook.headers:
                headers.update(webhook.headers)

            # Add authentication
            if webhook.auth_type and webhook.auth_value:
                if webhook.auth_type == "bearer":
                    headers["Authorization"] = f"Bearer {webhook.auth_value}"
                elif webhook.auth_type == "api_key" and webhook.auth_header:
                    headers[webhook.auth_header] = webhook.auth_value
                elif webhook.auth_type == "basic":
                    headers["Authorization"] = f"Basic {webhook.auth_value}"
                elif webhook.auth_type == "custom" and webhook.auth_header:
                    headers[webhook.auth_header] = webhook.auth_value

            # Send request with retries
            attempts = 0
            max_attempts = webhook.retry_max_attempts if webhook.retry_enabled else 1
            last_error = None

            while attempts < max_attempts:
                attempts += 1
                try:
                    async with httpx.AsyncClient(timeout=webhook.timeout_seconds) as client:
                        if webhook.method.upper() == "POST":
                            response = await client.post(
                                webhook.url,
                                json=final_payload,
                                headers=headers
                            )
                        elif webhook.method.upper() == "PUT":
                            response = await client.put(
                                webhook.url,
                                json=final_payload,
                                headers=headers
                            )
                        elif webhook.method.upper() == "PATCH":
                            response = await client.patch(
                                webhook.url,
                                json=final_payload,
                                headers=headers
                            )
                        else:
                            logger.error(f"Unsupported HTTP method: {webhook.method}")
                            return

                    # Update statistics
                    webhook.total_calls += 1
                    webhook.last_called_at = datetime.utcnow()
                    webhook.last_status_code = response.status_code

                    if 200 <= response.status_code < 300:
                        webhook.successful_calls += 1
                        webhook.last_error = None
                        await db.commit()
                        logger.info(f"Webhook '{webhook.name}' sent successfully (status: {response.status_code})")
                        return  # Success!
                    else:
                        # Non-2xx status code
                        last_error = f"HTTP {response.status_code}: {response.text[:200]}"
                        logger.warning(f"Webhook '{webhook.name}' returned {response.status_code} (attempt {attempts}/{max_attempts})")

                        if attempts < max_attempts:
                            await asyncio.sleep(webhook.retry_backoff_seconds)
                        else:
                            # Final attempt failed
                            webhook.failed_calls += 1
                            webhook.last_error = last_error
                            await db.commit()

                except httpx.TimeoutException:
                    last_error = f"Timeout after {webhook.timeout_seconds} seconds"
                    logger.warning(f"Webhook '{webhook.name}' timed out (attempt {attempts}/{max_attempts})")

                    if attempts < max_attempts:
                        await asyncio.sleep(webhook.retry_backoff_seconds)
                    else:
                        webhook.total_calls += 1
                        webhook.failed_calls += 1
                        webhook.last_called_at = datetime.utcnow()
                        webhook.last_error = last_error
                        await db.commit()

                except Exception as e:
                    last_error = str(e)[:500]
                    logger.error(f"Webhook '{webhook.name}' error: {e} (attempt {attempts}/{max_attempts})")

                    if attempts < max_attempts:
                        await asyncio.sleep(webhook.retry_backoff_seconds)
                    else:
                        webhook.total_calls += 1
                        webhook.failed_calls += 1
                        webhook.last_called_at = datetime.utcnow()
                        webhook.last_error = last_error
                        await db.commit()


# Global webhook service instance
webhook_service = WebhookService()
