"""
Webhook API routes
"""
from typing import List, Optional
from uuid import UUID
from fastapi import APIRouter, Depends, HTTPException, status, Form
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select, func
from datetime import datetime
import httpx
import json

from app.db.base import get_db
from app.models.webhook import Webhook

router = APIRouter()


@router.get("/", response_model=List[dict])
async def list_webhooks(
    skip: int = 0,
    limit: int = 100,
    is_active: Optional[bool] = None,
    db: AsyncSession = Depends(get_db)
):
    """
    List all webhooks with optional filtering
    """
    query = select(Webhook)

    if is_active is not None:
        query = query.where(Webhook.is_active == is_active)

    query = query.offset(skip).limit(limit).order_by(Webhook.created_at.desc())

    result = await db.execute(query)
    webhooks = result.scalars().all()

    return [
        {
            "id": str(w.id),
            "name": w.name,
            "description": w.description,
            "url": w.url,
            "method": w.method,
            "is_active": w.is_active,
            "auth_type": w.auth_type,
            "event_types": w.event_types or [],
            "camera_ids": w.camera_ids or [],
            "watchlist_ids": w.watchlist_ids or [],
            "timeout_seconds": w.timeout_seconds,
            "retry_enabled": w.retry_enabled,
            "retry_max_attempts": w.retry_max_attempts,
            "total_calls": w.total_calls,
            "successful_calls": w.successful_calls,
            "failed_calls": w.failed_calls,
            "last_called_at": w.last_called_at.isoformat() if w.last_called_at else None,
            "last_status_code": w.last_status_code,
            "last_error": w.last_error,
            "created_at": w.created_at.isoformat() if w.created_at else None,
            "updated_at": w.updated_at.isoformat() if w.updated_at else None,
            "created_by": w.created_by,
        }
        for w in webhooks
    ]


@router.get("/{webhook_id}", response_model=dict)
async def get_webhook(
    webhook_id: UUID,
    db: AsyncSession = Depends(get_db)
):
    """
    Get a specific webhook by ID
    """
    result = await db.execute(
        select(Webhook).where(Webhook.id == webhook_id)
    )
    webhook = result.scalar_one_or_none()

    if not webhook:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Webhook {webhook_id} not found"
        )

    return {
        "id": str(webhook.id),
        "name": webhook.name,
        "description": webhook.description,
        "url": webhook.url,
        "method": webhook.method,
        "is_active": webhook.is_active,
        "auth_type": webhook.auth_type,
        "auth_header": webhook.auth_header,
        "auth_value": "***" if webhook.auth_value else None,  # Don't expose credentials
        "event_types": webhook.event_types or [],
        "camera_ids": webhook.camera_ids or [],
        "watchlist_ids": webhook.watchlist_ids or [],
        "headers": webhook.headers or {},
        "timeout_seconds": webhook.timeout_seconds,
        "retry_enabled": webhook.retry_enabled,
        "retry_max_attempts": webhook.retry_max_attempts,
        "retry_backoff_seconds": webhook.retry_backoff_seconds,
        "payload_template": webhook.payload_template,
        "total_calls": webhook.total_calls,
        "successful_calls": webhook.successful_calls,
        "failed_calls": webhook.failed_calls,
        "last_called_at": webhook.last_called_at.isoformat() if webhook.last_called_at else None,
        "last_status_code": webhook.last_status_code,
        "last_error": webhook.last_error,
        "extra_metadata": webhook.extra_metadata,
        "created_at": webhook.created_at.isoformat() if webhook.created_at else None,
        "updated_at": webhook.updated_at.isoformat() if webhook.updated_at else None,
        "created_by": webhook.created_by,
    }


@router.post("/", response_model=dict, status_code=status.HTTP_201_CREATED)
async def create_webhook(
    name: str = Form(...),
    url: str = Form(...),
    description: Optional[str] = Form(None),
    method: str = Form("POST"),
    auth_type: Optional[str] = Form(None),
    auth_header: Optional[str] = Form(None),
    auth_value: Optional[str] = Form(None),
    event_types: Optional[str] = Form(None),  # JSON array as string
    camera_ids: Optional[str] = Form(None),  # JSON array as string
    watchlist_ids: Optional[str] = Form(None),  # JSON array as string
    headers: Optional[str] = Form(None),  # JSON object as string
    timeout_seconds: int = Form(10),
    retry_enabled: bool = Form(True),
    retry_max_attempts: int = Form(3),
    retry_backoff_seconds: int = Form(5),
    payload_template: Optional[str] = Form(None),  # JSON object as string
    created_by: Optional[str] = Form(None),
    db: AsyncSession = Depends(get_db)
):
    """
    Create a new webhook
    """
    # Parse JSON fields
    parsed_event_types = []
    if event_types:
        try:
            parsed_event_types = json.loads(event_types)
        except json.JSONDecodeError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid JSON for event_types"
            )

    parsed_camera_ids = []
    if camera_ids:
        try:
            parsed_camera_ids = json.loads(camera_ids)
        except json.JSONDecodeError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid JSON for camera_ids"
            )

    parsed_watchlist_ids = []
    if watchlist_ids:
        try:
            parsed_watchlist_ids = json.loads(watchlist_ids)
        except json.JSONDecodeError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid JSON for watchlist_ids"
            )

    parsed_headers = {}
    if headers:
        try:
            parsed_headers = json.loads(headers)
        except json.JSONDecodeError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid JSON for headers"
            )

    parsed_payload_template = None
    if payload_template:
        try:
            parsed_payload_template = json.loads(payload_template)
        except json.JSONDecodeError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid JSON for payload_template"
            )

    # Check if name already exists
    result = await db.execute(
        select(Webhook).where(Webhook.name == name)
    )
    if result.scalar_one_or_none():
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail=f"Webhook with name '{name}' already exists"
        )

    webhook = Webhook(
        name=name,
        description=description,
        url=url,
        method=method,
        is_active=True,
        auth_type=auth_type,
        auth_header=auth_header,
        auth_value=auth_value,  # TODO: Encrypt in production
        event_types=parsed_event_types,
        camera_ids=parsed_camera_ids,
        watchlist_ids=parsed_watchlist_ids,
        headers=parsed_headers,
        timeout_seconds=timeout_seconds,
        retry_enabled=retry_enabled,
        retry_max_attempts=retry_max_attempts,
        retry_backoff_seconds=retry_backoff_seconds,
        payload_template=parsed_payload_template,
        total_calls=0,
        successful_calls=0,
        failed_calls=0,
        created_by=created_by
    )

    db.add(webhook)
    await db.commit()
    await db.refresh(webhook)

    return {
        "id": str(webhook.id),
        "name": webhook.name,
        "description": webhook.description,
        "url": webhook.url,
        "method": webhook.method,
        "is_active": webhook.is_active,
        "auth_type": webhook.auth_type,
        "event_types": webhook.event_types or [],
        "camera_ids": webhook.camera_ids or [],
        "watchlist_ids": webhook.watchlist_ids or [],
        "timeout_seconds": webhook.timeout_seconds,
        "retry_enabled": webhook.retry_enabled,
        "total_calls": webhook.total_calls,
        "created_at": webhook.created_at.isoformat() if webhook.created_at else None,
        "created_by": webhook.created_by,
    }


@router.patch("/{webhook_id}", response_model=dict)
async def update_webhook(
    webhook_id: UUID,
    name: Optional[str] = Form(None),
    description: Optional[str] = Form(None),
    url: Optional[str] = Form(None),
    method: Optional[str] = Form(None),
    is_active: Optional[bool] = Form(None),
    auth_type: Optional[str] = Form(None),
    auth_header: Optional[str] = Form(None),
    auth_value: Optional[str] = Form(None),
    event_types: Optional[str] = Form(None),
    camera_ids: Optional[str] = Form(None),
    watchlist_ids: Optional[str] = Form(None),
    headers: Optional[str] = Form(None),
    timeout_seconds: Optional[int] = Form(None),
    retry_enabled: Optional[bool] = Form(None),
    retry_max_attempts: Optional[int] = Form(None),
    retry_backoff_seconds: Optional[int] = Form(None),
    payload_template: Optional[str] = Form(None),
    db: AsyncSession = Depends(get_db)
):
    """
    Update a webhook
    """
    result = await db.execute(
        select(Webhook).where(Webhook.id == webhook_id)
    )
    webhook = result.scalar_one_or_none()

    if not webhook:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Webhook {webhook_id} not found"
        )

    # Update fields
    if name is not None:
        webhook.name = name
    if description is not None:
        webhook.description = description
    if url is not None:
        webhook.url = url
    if method is not None:
        webhook.method = method
    if is_active is not None:
        webhook.is_active = is_active
    if auth_type is not None:
        webhook.auth_type = auth_type
    if auth_header is not None:
        webhook.auth_header = auth_header
    if auth_value is not None:
        webhook.auth_value = auth_value

    # Update JSON fields
    if event_types is not None:
        try:
            webhook.event_types = json.loads(event_types)
        except json.JSONDecodeError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid JSON for event_types"
            )

    if camera_ids is not None:
        try:
            webhook.camera_ids = json.loads(camera_ids)
        except json.JSONDecodeError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid JSON for camera_ids"
            )

    if watchlist_ids is not None:
        try:
            webhook.watchlist_ids = json.loads(watchlist_ids)
        except json.JSONDecodeError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid JSON for watchlist_ids"
            )

    if headers is not None:
        try:
            webhook.headers = json.loads(headers)
        except json.JSONDecodeError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid JSON for headers"
            )

    if payload_template is not None:
        try:
            webhook.payload_template = json.loads(payload_template)
        except json.JSONDecodeError:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="Invalid JSON for payload_template"
            )

    if timeout_seconds is not None:
        webhook.timeout_seconds = timeout_seconds
    if retry_enabled is not None:
        webhook.retry_enabled = retry_enabled
    if retry_max_attempts is not None:
        webhook.retry_max_attempts = retry_max_attempts
    if retry_backoff_seconds is not None:
        webhook.retry_backoff_seconds = retry_backoff_seconds

    await db.commit()
    await db.refresh(webhook)

    return {
        "id": str(webhook.id),
        "name": webhook.name,
        "description": webhook.description,
        "url": webhook.url,
        "method": webhook.method,
        "is_active": webhook.is_active,
        "auth_type": webhook.auth_type,
        "event_types": webhook.event_types or [],
        "camera_ids": webhook.camera_ids or [],
        "watchlist_ids": webhook.watchlist_ids or [],
        "timeout_seconds": webhook.timeout_seconds,
        "retry_enabled": webhook.retry_enabled,
        "total_calls": webhook.total_calls,
        "successful_calls": webhook.successful_calls,
        "failed_calls": webhook.failed_calls,
        "last_called_at": webhook.last_called_at.isoformat() if webhook.last_called_at else None,
        "created_at": webhook.created_at.isoformat() if webhook.created_at else None,
        "updated_at": webhook.updated_at.isoformat() if webhook.updated_at else None,
    }


@router.delete("/{webhook_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_webhook(
    webhook_id: UUID,
    db: AsyncSession = Depends(get_db)
):
    """
    Delete a webhook
    """
    result = await db.execute(
        select(Webhook).where(Webhook.id == webhook_id)
    )
    webhook = result.scalar_one_or_none()

    if not webhook:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Webhook {webhook_id} not found"
        )

    await db.delete(webhook)
    await db.commit()


@router.post("/{webhook_id}/test", response_model=dict)
async def test_webhook(
    webhook_id: UUID,
    db: AsyncSession = Depends(get_db)
):
    """
    Test a webhook by sending a sample payload
    """
    result = await db.execute(
        select(Webhook).where(Webhook.id == webhook_id)
    )
    webhook = result.scalar_one_or_none()

    if not webhook:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Webhook {webhook_id} not found"
        )

    # Build test payload
    test_payload = {
        "event_type": "webhook_test",
        "webhook_id": str(webhook.id),
        "webhook_name": webhook.name,
        "timestamp": datetime.utcnow().isoformat(),
        "test_data": {
            "camera_id": "test_camera",
            "subject": "test_subject",
            "similarity": 0.95,
            "message": "This is a test webhook call from the system"
        }
    }

    # Use payload template if provided
    if webhook.payload_template:
        test_payload = {**webhook.payload_template, **test_payload}

    # Build headers
    request_headers = {"Content-Type": "application/json"}
    if webhook.headers:
        request_headers.update(webhook.headers)

    # Add authentication
    if webhook.auth_type and webhook.auth_value:
        if webhook.auth_type == "bearer":
            request_headers["Authorization"] = f"Bearer {webhook.auth_value}"
        elif webhook.auth_type == "api_key" and webhook.auth_header:
            request_headers[webhook.auth_header] = webhook.auth_value
        elif webhook.auth_type == "basic":
            request_headers["Authorization"] = f"Basic {webhook.auth_value}"
        elif webhook.auth_type == "custom" and webhook.auth_header:
            request_headers[webhook.auth_header] = webhook.auth_value

    # Make HTTP request
    try:
        async with httpx.AsyncClient(timeout=webhook.timeout_seconds) as client:
            if webhook.method.upper() == "POST":
                response = await client.post(
                    webhook.url,
                    json=test_payload,
                    headers=request_headers
                )
            elif webhook.method.upper() == "PUT":
                response = await client.put(
                    webhook.url,
                    json=test_payload,
                    headers=request_headers
                )
            elif webhook.method.upper() == "PATCH":
                response = await client.patch(
                    webhook.url,
                    json=test_payload,
                    headers=request_headers
                )
            else:
                raise HTTPException(
                    status_code=status.HTTP_400_BAD_REQUEST,
                    detail=f"Unsupported HTTP method: {webhook.method}"
                )

        # Update webhook stats
        webhook.total_calls += 1
        webhook.last_called_at = datetime.utcnow()
        webhook.last_status_code = response.status_code

        if 200 <= response.status_code < 300:
            webhook.successful_calls += 1
            webhook.last_error = None
            await db.commit()

            return {
                "success": True,
                "message": "Webhook test successful",
                "status_code": response.status_code,
                "response_body": response.text[:500] if response.text else None  # Limit response size
            }
        else:
            webhook.failed_calls += 1
            webhook.last_error = f"HTTP {response.status_code}: {response.text[:200]}"
            await db.commit()

            return {
                "success": False,
                "message": f"Webhook returned non-success status code",
                "status_code": response.status_code,
                "response_body": response.text[:500] if response.text else None
            }

    except httpx.TimeoutException:
        webhook.total_calls += 1
        webhook.failed_calls += 1
        webhook.last_called_at = datetime.utcnow()
        webhook.last_error = f"Timeout after {webhook.timeout_seconds} seconds"
        await db.commit()

        raise HTTPException(
            status_code=status.HTTP_408_REQUEST_TIMEOUT,
            detail=f"Webhook request timed out after {webhook.timeout_seconds} seconds"
        )

    except Exception as e:
        webhook.total_calls += 1
        webhook.failed_calls += 1
        webhook.last_called_at = datetime.utcnow()
        webhook.last_error = str(e)[:500]
        await db.commit()

        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Webhook test failed: {str(e)}"
        )


@router.get("/{webhook_id}/statistics", response_model=dict)
async def get_webhook_statistics(
    webhook_id: UUID,
    db: AsyncSession = Depends(get_db)
):
    """
    Get statistics for a webhook
    """
    result = await db.execute(
        select(Webhook).where(Webhook.id == webhook_id)
    )
    webhook = result.scalar_one_or_none()

    if not webhook:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail=f"Webhook {webhook_id} not found"
        )

    success_rate = 0.0
    if webhook.total_calls > 0:
        success_rate = (webhook.successful_calls / webhook.total_calls) * 100

    return {
        "webhook_id": str(webhook.id),
        "name": webhook.name,
        "total_calls": webhook.total_calls,
        "successful_calls": webhook.successful_calls,
        "failed_calls": webhook.failed_calls,
        "success_rate": round(success_rate, 2),
        "last_called_at": webhook.last_called_at.isoformat() if webhook.last_called_at else None,
        "last_status_code": webhook.last_status_code,
        "last_error": webhook.last_error,
        "is_active": webhook.is_active,
        "event_types": webhook.event_types or [],
        "retry_enabled": webhook.retry_enabled,
    }
