"""
In-Memory State Cache for Camera Status and Worker State
Thread-safe cache updated in real-time via WebSocket events
"""
import asyncio
from typing import Dict, Optional, Any
from datetime import datetime, timedelta
import logging

logger = logging.getLogger(__name__)


class CameraStateCache:
    """
    Thread-safe in-memory cache for camera states and metrics.
    Updated in real-time via WebSocket events from the C++ worker.
    """

    def __init__(self, ttl_seconds: int = 300):
        """
        Initialize state cache

        Args:
            ttl_seconds: Time-to-live for cached entries (default: 5 minutes)
        """
        self.cache: Dict[str, Dict[str, Any]] = {}
        self.worker_state: Dict[str, Any] = {}
        self.lock = asyncio.Lock()
        self.ttl = timedelta(seconds=ttl_seconds)

        # Statistics
        self.hits = 0
        self.misses = 0
        self.updates = 0

    async def update_camera(self, camera_id: str, state: Dict[str, Any]):
        """
        Update camera state in cache

        Args:
            camera_id: Camera identifier
            state: Camera state dictionary (should contain 'state', 'metrics', etc.)
        """
        async with self.lock:
            state["_cached_at"] = datetime.utcnow()
            state["_camera_id"] = camera_id
            self.cache[camera_id] = state
            self.updates += 1
            logger.debug(f"Cache updated: {camera_id} (state={state.get('state', 'unknown')})")

    async def get_camera(self, camera_id: str) -> Optional[Dict[str, Any]]:
        """
        Get camera state from cache

        Args:
            camera_id: Camera identifier

        Returns:
            Camera state dictionary or None if not found/expired
        """
        async with self.lock:
            if camera_id not in self.cache:
                self.misses += 1
                logger.debug(f"Cache miss: {camera_id}")
                return None

            state = self.cache[camera_id]
            cached_at = state.get("_cached_at")

            # Check if stale
            if cached_at and datetime.utcnow() - cached_at > self.ttl:
                logger.warning(f"Stale cache for {camera_id} "
                             f"(age: {(datetime.utcnow() - cached_at).total_seconds():.1f}s)")
                self.misses += 1
                return None

            self.hits += 1
            return dict(state)  # Return copy to prevent external modifications

    async def get_all_cameras(self) -> Dict[str, Dict[str, Any]]:
        """
        Get all cached camera states

        Returns:
            Dictionary mapping camera_id to state
        """
        async with self.lock:
            result = {}
            now = datetime.utcnow()

            for camera_id, state in self.cache.items():
                cached_at = state.get("_cached_at")

                # Skip expired entries
                if cached_at and now - cached_at > self.ttl:
                    continue

                result[camera_id] = dict(state)

            return result

    async def delete_camera(self, camera_id: str):
        """
        Remove camera from cache

        Args:
            camera_id: Camera identifier
        """
        async with self.lock:
            if camera_id in self.cache:
                del self.cache[camera_id]
                logger.debug(f"Cache deleted: {camera_id}")

    async def clear_cameras(self):
        """Clear all camera entries from cache"""
        async with self.lock:
            self.cache.clear()
            logger.info("Camera cache cleared")

    async def update_worker_state(self, state: Dict[str, Any]):
        """
        Update worker system state

        Args:
            state: Worker state dictionary (uptime, total_cameras, etc.)
        """
        async with self.lock:
            state["_cached_at"] = datetime.utcnow()
            self.worker_state = state
            logger.debug(f"Worker state updated: {state.get('uptime_seconds', 0):.1f}s uptime")

    async def get_worker_state(self) -> Optional[Dict[str, Any]]:
        """
        Get worker system state

        Returns:
            Worker state dictionary or None if not available/expired
        """
        async with self.lock:
            if not self.worker_state:
                return None

            cached_at = self.worker_state.get("_cached_at")

            # Check if stale
            if cached_at and datetime.utcnow() - cached_at > self.ttl:
                logger.warning("Stale worker state")
                return None

            return dict(self.worker_state)

    async def is_camera_cached(self, camera_id: str) -> bool:
        """
        Check if camera exists in cache (not expired)

        Args:
            camera_id: Camera identifier

        Returns:
            True if camera is cached and not expired
        """
        state = await self.get_camera(camera_id)
        return state is not None

    def get_cache_stats(self) -> Dict[str, Any]:
        """
        Get cache statistics

        Returns:
            Statistics dictionary with hits, misses, hit rate, etc.
        """
        total_requests = self.hits + self.misses
        hit_rate = (self.hits / total_requests * 100) if total_requests > 0 else 0.0

        return {
            "total_cameras_cached": len(self.cache),
            "cache_hits": self.hits,
            "cache_misses": self.misses,
            "cache_hit_rate": round(hit_rate, 2),
            "total_updates": self.updates,
            "ttl_seconds": self.ttl.total_seconds()
        }

    async def cleanup_expired(self):
        """Remove expired entries from cache"""
        async with self.lock:
            now = datetime.utcnow()
            expired = []

            for camera_id, state in self.cache.items():
                cached_at = state.get("_cached_at")
                if cached_at and now - cached_at > self.ttl:
                    expired.append(camera_id)

            for camera_id in expired:
                del self.cache[camera_id]

            if expired:
                logger.info(f"Cleaned up {len(expired)} expired cache entries")


# Global cache instance
camera_state_cache = CameraStateCache(ttl_seconds=300)
