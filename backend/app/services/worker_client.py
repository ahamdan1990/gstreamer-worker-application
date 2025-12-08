"""
Worker Client Service
Communicates with the C++ GStreamer Worker API
"""
import httpx
from typing import Dict, List, Optional
from pydantic import BaseModel


class MotionDetectionConfig(BaseModel):
    """Motion detection configuration"""
    enabled: bool = False
    algorithm: str = "MOG2_CUDA"
    sensitivity: float = 0.5
    min_contour_area: int = 500
    max_contours: int = 50
    frame_skip: int = 2
    blur_size: int = 21
    history: int = 500
    var_threshold: float = 16.0
    detect_shadows: bool = False
    cooldown_seconds: float = 1.0
    required_frames: int = 3
    max_frame_width: int = 640
    max_frame_height: int = 480
    roi: Optional[Dict] = None


class FaceDetectionConfig(BaseModel):
    """Face detection configuration - all 46 parameters"""
    enabled: bool = False
    # Model Configuration
    model_path: str = "models/scrfd/scrfd_10g_bnkps.onnx"
    input_size: int = 640
    confidence_threshold: float = 0.3
    nms_threshold: float = 0.5
    # Frame Processing
    frame_skip: int = 2
    max_frame_width: int = 1920
    max_frame_height: int = 1080
    # Face Detection Parameters
    min_face_size: float = 0.0003
    max_faces: int = 30
    required_frames: int = 1
    cooldown_seconds: float = 0.3
    # Face Quality Filtering (NEW)
    enable_frontal_face_filter: bool = True
    eye_tilt_threshold: float = 0.3
    min_eye_distance: float = 0.01
    nose_center_offset_threshold: float = 0.3
    eye_to_face_ratio_min: float = 0.25
    eye_to_face_ratio_max: float = 0.65
    enable_min_face_size_filter: bool = True
    min_face_width: int = 80
    min_face_height: int = 80
    # TensorRT/CUDA Settings
    use_tensorrt: bool = True
    use_cuda: bool = True
    max_batch_size: int = 4
    # Face Saving Configuration
    save_faces: bool = True
    save_path: str = "./face_crops"
    save_margin: float = 0.3
    min_save_confidence: float = 0.2
    max_saves_per_event: int = 5
    # Blur Detection
    enable_blur_detection: bool = True
    min_laplacian_variance: float = 250.0
    blur_kernel_size: int = 3
    # Motion Integration
    motion_triggered_detection: bool = True
    motion_detection_cooldown: float = 1.5
    # CompreFace Integration
    enable_compreface: bool = True
    compreface_url: str = "http://localhost:8000"
    compreface_api_key: str = ""
    compreface_subject: str = "unknown"
    compreface_similarity_threshold: float = 0.88
    compreface_timeout_ms: int = 8000
    compreface_max_queue_size: int = 200
    # Visualization Settings
    enable_visualization: bool = True
    draw_landmarks: bool = True
    draw_confidence: bool = True
    box_thickness: int = 2
    font_scale: float = 0.5


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
    motion_detection: Optional[MotionDetectionConfig] = None
    face_detection: Optional[FaceDetectionConfig] = None


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
        # Increased timeout to 120s to allow for TensorRT model compilation on first load
        self.timeout = httpx.Timeout(120.0, connect=5.0)

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
                json=config.model_dump(exclude_none=True)
            )
            response.raise_for_status()
            return response.json()

    async def update_camera(self, camera_id: str, config: WorkerCameraConfig) -> Dict:
        """Update camera configuration in worker by stopping, removing and re-adding it"""
        # Worker doesn't have PUT endpoint, so we stop, remove and re-add
        import asyncio

        # Step 1: Stop the camera first to ensure clean removal
        try:
            await self.stop_camera(camera_id)
            # Give it a moment to fully stop
            await asyncio.sleep(0.2)
        except Exception as e:
            # Camera might not be running or might not exist, continue anyway
            pass

        # Step 2: Remove the camera
        try:
            await self.remove_camera(camera_id)
            # Give it a moment to fully remove
            await asyncio.sleep(0.2)
        except httpx.HTTPStatusError as e:
            if e.response.status_code == 404:
                # Camera doesn't exist, that's okay - we'll add it
                pass
            else:
                # Other errors should propagate
                raise
        except Exception as e:
            # For other exceptions, log but continue (will fail on add if camera still exists)
            import logging
            logging.getLogger(__name__).warning(f"Failed to remove camera {camera_id}: {e}")

        # Step 3: Add with new configuration
        return await self.add_camera(config)

    async def add_or_update_camera(self, config: WorkerCameraConfig) -> Dict:
        """Add camera if doesn't exist, update if it does (idempotent operation)"""
        try:
            # Try to add first
            return await self.add_camera(config)
        except httpx.HTTPStatusError as e:
            if e.response.status_code == 400:
                # Camera already exists, update by removing and re-adding
                return await self.update_camera(config.camera_id, config)
            raise

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
