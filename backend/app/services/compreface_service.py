"""
CompreFace Service
Provides functionality to interact with CompreFace API for fetching subject images
"""
import httpx
import logging
from typing import List, Dict, Optional
from pydantic import BaseModel

logger = logging.getLogger(__name__)


class CompreFaceImage(BaseModel):
    """CompreFace image reference"""
    image_id: str
    subject: str


class CompreFaceImagesResponse(BaseModel):
    """Response from CompreFace faces API"""
    faces: List[CompreFaceImage]
    page_number: int
    page_size: int
    total_pages: int
    total_elements: int


class CompreFaceService:
    """Service for interacting with CompreFace API"""

    def __init__(self, base_url: str = "http://localhost:8000", api_key: str = ""):
        self.base_url = base_url.rstrip('/')
        self.api_key = api_key
        self.timeout = httpx.Timeout(10.0)

    async def get_subject_images(
        self,
        subject: str,
        page: int = 0,
        size: int = 20
    ) -> Optional[CompreFaceImagesResponse]:
        """
        Fetch all saved images for a specific subject from CompreFace.

        Args:
            subject: Subject ID to fetch images for
            page: Page number (default: 0)
            size: Page size (default: 20)

        Returns:
            CompreFaceImagesResponse or None if request fails
        """
        if not self.api_key:
            logger.warning("CompreFace API key not configured")
            return None

        try:
            url = f"{self.base_url}/api/v1/recognition/faces"
            headers = {"x-api-key": self.api_key}
            params = {
                "subject": subject,
                "page": page,
                "size": size
            }

            async with httpx.AsyncClient(timeout=self.timeout) as client:
                response = await client.get(url, headers=headers, params=params)
                response.raise_for_status()

                data = response.json()
                logger.info(f"Fetched {len(data.get('faces', []))} images for subject '{subject}'")

                return CompreFaceImagesResponse(**data)

        except httpx.HTTPStatusError as e:
            logger.error(f"HTTP error fetching CompreFace images for '{subject}': {e.response.status_code}")
            return None
        except Exception as e:
            logger.error(f"Failed to fetch CompreFace images for '{subject}': {e}")
            return None

    def get_image_url(self, image_id: str) -> str:
        """
        Get the direct URL to download an image from CompreFace.

        Args:
            image_id: UUID of the image

        Returns:
            Direct URL to the image
        """
        return f"{self.base_url}/api/v1/static/{self.api_key}/images/{image_id}"

    async def download_image(self, image_id: str) -> Optional[bytes]:
        """
        Download an image from CompreFace.

        Args:
            image_id: UUID of the image

        Returns:
            Image bytes or None if download fails
        """
        try:
            url = self.get_image_url(image_id)

            async with httpx.AsyncClient(timeout=self.timeout) as client:
                response = await client.get(url)
                response.raise_for_status()

                return response.content

        except Exception as e:
            logger.error(f"Failed to download image {image_id}: {e}")
            return None
