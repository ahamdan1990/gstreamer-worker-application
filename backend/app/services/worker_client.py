"""
Worker Client Service
Communicates with the C++ GStreamer Worker API
"""
import httpx
from typing import Dict, List, Optional
from pydantic import BaseModel


class WorkerCameraConfig(BaseModel):
    """Configuration for adding a camera to the worker"""
    camera_id: str
    rtsp_url: str
    username: Optional[str] = None
    password: Optional[str] = None
    protocols: str = "tcp"
    latency_ms: int = 150
    target_fps: int = 12
    enable_display: bool = False  # Disable display in production
    use_nvidia_decoder: bool = True


class WorkerCameraStatus(BaseModel):
    """Camera status from worker"""
    camera_id: str
    state: str
    is_running: bool
    metrics: Dict


class WorkerClient:
    """Client for C++ GStreamer Worker REST API"""

    def __init__(self, base_url: str = "http://localhost:8081"):
        self.base_url = base_url
        self.timeout = httpx.Timeout(10.0, connect=5.0)

    async def health_check(self) -> bool:
        """Check if worker is healthy"""
        try:
            async with httpx.AsyncClient(timeout=self.timeout) as client:
                response = await client.get(f"{self.base_url}/health")
                return response.status_code == 200
        except Exception:
            return False

    async def list_cameras(self) -> List[WorkerCameraStatus]:
        """List all cameras in the worker"""
        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.get(f"{self.base_url}/api/cameras")
            response.raise_for_status()
            data = response.json()
            return [WorkerCameraStatus(**cam) for cam in data]

    async def get_camera_status(self, camera_id: str) -> WorkerCameraStatus:
        """Get status of a specific camera"""
        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.get(
                f"{self.base_url}/api/cameras/{camera_id}/status"
            )
            response.raise_for_status()
            return WorkerCameraStatus(**response.json())

    async def add_camera(self, config: WorkerCameraConfig) -> Dict:
        """Add a camera to the worker"""
        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.post(
                f"{self.base_url}/api/cameras",
                json=config.model_dump()
            )
            response.raise_for_status()
            return response.json()

    async def start_camera(self, camera_id: str) -> Dict:
        """Start a camera stream"""
        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.post(
                f"{self.base_url}/api/cameras/{camera_id}/start"
            )
            response.raise_for_status()
            return response.json()

    async def stop_camera(self, camera_id: str) -> Dict:
        """Stop a camera stream"""
        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.post(
                f"{self.base_url}/api/cameras/{camera_id}/stop"
            )
            response.raise_for_status()
            return response.json()

    async def remove_camera(self, camera_id: str) -> Dict:
        """Remove a camera from the worker"""
        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.delete(
                f"{self.base_url}/api/cameras/{camera_id}"
            )
            response.raise_for_status()
            return response.json()

    async def get_system_status(self) -> Dict:
        """Get overall system status"""
        async with httpx.AsyncClient(timeout=self.timeout) as client:
            response = await client.get(f"{self.base_url}/api/system/status")
            response.raise_for_status()
            return response.json()
