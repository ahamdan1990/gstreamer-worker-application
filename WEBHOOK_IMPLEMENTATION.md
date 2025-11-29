# Webhook Implementation Guide

## Overview

The webhook system is now fully implemented and integrated into the face recognition system. Webhooks automatically send real-time event notifications to third-party applications when persons are recognized or faces are detected.

## Features

### Event Types

1. **`person_recognized`** - Triggered when a person is recognized by the system
   - Includes full profile information
   - Watchlist assignments
   - Recognition similarity score
   - Camera information
   - Detection statistics (dwell time, new person flags, etc.)
   - Face crop image URL

2. **`face_detected`** - Triggered for all face detections (recognized or unknown)
   - Tracking information (tracking_id, duration, attempts)
   - Recognition status and confidence
   - Profile information (if recognized)
   - Face crop image URL

### Webhook Payload Example

**Person Recognized Event:**
```json
{
  "event_type": "person_recognized",
  "event_id": "person_1732901234.567",
  "timestamp": "2025-11-29T12:34:56.789Z",
  "camera": {
    "id": "camera_01",
    "name": "camera_01"
  },
  "person": {
    "subject": "john_doe_abc123",
    "similarity": 0.92,
    "confidence_percentage": 92.0,
    "profile": {
      "id": "uuid-profile-id",
      "name": "John Doe",
      "external_id": "EMP-001",
      "email": "john.doe@example.com",
      "department": "Engineering",
      "tags": ["employee", "vip"],
      "notes": "Regular employee",
      "total_detections": 45,
      "avg_recognition_similarity": 0.89,
      "first_seen_at": "2025-01-15T10:00:00Z",
      "last_seen_at": "2025-11-29T12:34:56Z",
      "last_seen_camera_id": "camera_01"
    }
  },
  "watchlists": [
    {
      "id": "uuid-watchlist-id",
      "name": "Employees",
      "description": "All registered employees",
      "alert_enabled": false,
      "priority": "medium"
    }
  ],
  "detection": {
    "dwell_time_seconds": 5.2,
    "is_new_person": false,
    "is_new_at_camera": false,
    "detection_confidence": 0.85
  },
  "image": {
    "url": "/face_crops/camera_01_1732901234567_face1.jpg",
    "timestamp": "2025-11-29T12:34:56Z"
  },
  "metadata": {
    "other_cameras": ["camera_02", "camera_03"],
    "system_name": "Face Recognition System",
    "version": "2.0.0"
  },
  "raw_event": { /* Full raw event data from C++ worker */ }
}
```

### Webhook Configuration Options

- **URL**: Target endpoint for webhook delivery
- **Method**: HTTP method (POST, PUT, PATCH)
- **Event Types**: Filter by event type (`person_recognized`, `face_detected`)
- **Camera IDs**: Filter by specific cameras
- **Watchlist IDs**: Only send events for persons in specific watchlists
- **Authentication**:
  - Bearer token
  - API key (custom header)
  - Basic authentication
  - Custom header authentication
- **Retry Settings**:
  - Enable/disable retries
  - Max retry attempts (default: 3)
  - Backoff seconds between retries (default: 5s)
- **Timeout**: Request timeout in seconds (default: 10s)
- **Custom Headers**: Add custom HTTP headers
- **Payload Template**: Merge custom fields with event payload

### Statistics Tracked

- Total calls
- Successful calls
- Failed calls
- Success rate percentage
- Last called timestamp
- Last status code
- Last error message

## Testing the Webhook System

### 1. Start the Test Receiver

```bash
# Run the test webhook receiver on port 8888
python3 test_webhook_receiver.py

# Or use a custom port
python3 test_webhook_receiver.py 9000
```

### 2. Create a Webhook via API

```bash
curl -X POST http://localhost:8000/api/v1/webhooks/ \
  -F "name=Test Webhook" \
  -F "url=http://localhost:8888/webhook" \
  -F "method=POST" \
  -F "event_types=[\"person_recognized\",\"face_detected\"]" \
  -F "timeout_seconds=10" \
  -F "retry_enabled=true" \
  -F "retry_max_attempts=3"
```

### 3. Create a Webhook via UI

1. Navigate to the **Webhooks** tab in the frontend
2. Click "Add Webhook" or "New Webhook"
3. Fill in the form:
   - **Name**: Test Webhook
   - **URL**: `http://localhost:8888/webhook`
   - **Method**: POST
   - **Event Types**: Select `person_recognized` and/or `face_detected`
   - **Active**: Enabled
4. Save the webhook

### 4. Test the Webhook

**Option A: Use the Test Button**
- In the webhooks UI, click "Test" on your webhook
- Check the test receiver console for the test payload

**Option B: Trigger Real Events**
- Ensure a camera is running with face detection enabled
- Walk in front of the camera or show a registered person's photo
- Check the test receiver console for real-time webhook events

### 5. Monitor Webhook Statistics

```bash
# Get webhook statistics
curl http://localhost:8000/api/v1/webhooks/{webhook_id}/statistics
```

Or view in the UI:
- Go to Webhooks tab
- Click on a webhook to view detailed statistics
- See success rate, total calls, last error, etc.

## Implementation Details

### Files Modified/Created

1. **`backend/app/services/webhook_service.py`** (Created)
   - `WebhookService` class with async webhook delivery
   - `send_person_recognized_event()` method
   - `send_face_detected_event()` method
   - Retry logic with exponential backoff
   - Authentication support
   - Event/camera/watchlist filtering

2. **`backend/app/services/event_handlers.py`** (Modified)
   - Integrated webhook service
   - Calls webhook on `person_recognized` events ([event_handlers.py:395-403](backend/app/services/event_handlers.py#L395-L403))
   - Calls webhook on `face_detected` events ([event_handlers.py:507-517](backend/app/services/event_handlers.py#L507-L517))

3. **`backend/app/api/routes/webhooks.py`** (Existing)
   - CRUD operations for webhook management
   - Test endpoint for webhook validation
   - Statistics endpoint

4. **`test_webhook_receiver.py`** (Created)
   - Simple HTTP server for testing webhook delivery
   - Logs all received webhook payloads

### Integration Flow

```
Face Detection → C++ Worker → WebSocket → Event Handler → Webhook Service → Third-party App
```

1. **C++ Worker** detects/recognizes a face
2. **WebSocket** sends event to FastAPI backend
3. **Event Handler** processes event and stores in database
4. **Webhook Service** queries active webhooks with matching filters
5. **HTTP Request** sent to configured webhook URLs with full event payload
6. **Retry Logic** attempts delivery up to max_attempts on failure
7. **Statistics** updated (success/failure counts, last error, etc.)

## Security Considerations

### Current Implementation

- Authentication credentials stored in database (cleartext)
- HTTPS recommended for production webhook URLs
- Webhook URLs validated on creation

### Production Recommendations

1. **Encrypt webhook credentials** in database using encryption keys
2. **Use HTTPS** for all webhook URLs
3. **Implement webhook signing** (HMAC signatures) for payload verification
4. **Rate limiting** on webhook deliveries
5. **IP whitelisting** for webhook sources
6. **Audit logging** for all webhook deliveries

## Troubleshooting

### Webhooks Not Firing

1. **Check webhook is active**: Verify `is_active = true`
2. **Check event type filter**: Ensure event type is in `event_types` array
3. **Check camera filter**: Ensure camera_id matches `camera_ids` filter (or filter is empty)
4. **Check watchlist filter**: Ensure person is in one of the `watchlist_ids` (or filter is empty)
5. **Check backend logs**: Look for webhook errors in FastAPI logs

### Webhook Delivery Failures

1. **Check URL is reachable**: Test with `curl http://your-webhook-url`
2. **Check timeout settings**: Increase timeout if remote server is slow
3. **Check authentication**: Verify auth credentials are correct
4. **Check retry settings**: Enable retries for transient failures
5. **View webhook statistics**: Check `last_error` field for details

### High Latency

1. **Enable concurrent sending**: Webhooks use semaphore for concurrency (max 10)
2. **Reduce timeout**: Lower timeout for faster failure detection
3. **Disable retries**: Turn off retries for non-critical webhooks
4. **Use async receivers**: Ensure webhook receiver responds quickly (< 1s)

## Example Use Cases

### 1. Slack Notifications

Send alerts to Slack when VIP persons are detected:
- URL: `https://hooks.slack.com/services/YOUR/SLACK/WEBHOOK`
- Event Types: `person_recognized`
- Watchlist Filter: VIP watchlist ID
- Payload Template: Format for Slack incoming webhooks

### 2. Access Control System

Trigger door unlock when authorized person detected:
- URL: `https://access-control.example.com/api/unlock`
- Event Types: `person_recognized`
- Watchlist Filter: Authorized employees watchlist
- Authentication: Bearer token

### 3. Analytics Platform

Send all detection events to analytics system:
- URL: `https://analytics.example.com/api/events`
- Event Types: `person_recognized`, `face_detected`
- No filters (send all events)
- Custom headers for API key

### 4. Security Monitoring

Alert security team when unknown persons detected:
- URL: `https://security-ops.example.com/api/alerts`
- Event Types: `face_detected`
- Filter: Only unknown faces (handled in receiver logic)
- High priority delivery

## API Reference

See [backend/app/api/routes/webhooks.py](backend/app/api/routes/webhooks.py) for complete API documentation.

### Key Endpoints

- `GET /api/v1/webhooks/` - List all webhooks
- `POST /api/v1/webhooks/` - Create webhook
- `GET /api/v1/webhooks/{id}` - Get webhook details
- `PATCH /api/v1/webhooks/{id}` - Update webhook
- `DELETE /api/v1/webhooks/{id}` - Delete webhook
- `POST /api/v1/webhooks/{id}/test` - Test webhook delivery
- `GET /api/v1/webhooks/{id}/statistics` - Get webhook stats

## Next Steps

1. **Test webhook delivery** with test receiver script
2. **Configure production webhooks** for your use case
3. **Monitor webhook statistics** for delivery issues
4. **Implement webhook signing** for production security
5. **Add custom integrations** (Slack, Teams, custom apps)
