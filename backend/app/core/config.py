"""
Application configuration
"""
from typing import List
from pydantic_settings import BaseSettings
from pydantic import AnyHttpUrl, validator


class Settings(BaseSettings):
    """Application settings"""

    # Application
    APP_NAME: str = "GStreamer Camera Manager"
    APP_VERSION: str = "1.0.0"
    DEBUG: bool = False
    ENVIRONMENT: str = "production"

    # API
    API_HOST: str = "0.0.0.0"
    API_PORT: int = 8000
    API_PREFIX: str = "/api/v1"

    # Database
    DATABASE_URL: str
    DATABASE_ECHO: bool = False

    # Security
    SECRET_KEY: str
    ALGORITHM: str = "HS256"
    ACCESS_TOKEN_EXPIRE_MINUTES: int = 30

    # CORS
    CORS_ORIGINS: List[str] = ["http://localhost:3000"]

    @validator("CORS_ORIGINS", pre=True)
    def assemble_cors_origins(cls, v):
        if isinstance(v, str):
            return [i.strip() for i in v.split(",")]
        return v

    # C++ Worker
    WORKER_HOST: str = "localhost"
    WORKER_PORT: int = 8081
    WORKER_WS_PORT: int = 8082  # WebSocket on separate port for production
    WORKER_API_URL: str = "http://localhost:8081"
    WORKER_WS_URL: str = "ws://localhost:8082/ws"
    WORKER_EXECUTABLE: str = "../gstreamer_worker/build/gstreamer_worker_api"
    WORKER_AUTO_START: bool = True
    WORKER_AUTO_RESTART: bool = True

    # CompreFace
    COMPREFACE_URL: str
    COMPREFACE_API_KEY: str

    # Redis
    REDIS_URL: str = "redis://localhost:6379/0"

    # File Upload
    UPLOAD_DIR: str = "./uploads"
    MAX_UPLOAD_SIZE: int = 10485760  # 10MB

    # Logging
    LOG_LEVEL: str = "INFO"
    LOG_FILE: str = "logs/app.log"

    class Config:
        env_file = ".env"
        case_sensitive = True


settings = Settings()
