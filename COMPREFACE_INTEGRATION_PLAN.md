# CompreFace Integration - Implementation Plan

## Overview
Enterprise-grade face recognition integration with:
- Async recognition queue
- Cross-camera person tracking
- Dwell time analytics
- Production-ready error handling

## Components Created

### 1. CompreFaceClient (`compreface_client.h`)
**Features:**
- ✅ Async HTTP client with thread pool
- ✅ Retry logic with exponential backoff
- ✅ Result caching (60s default)
- ✅ Circuit breaker pattern
- ✅ Performance metrics

**Key Methods:**
- `recognize_async()` - Queue face for recognition (non-blocking)
- `recognize_sync()` - Immediate recognition (blocking)
- `get_cached_result()` - Check cache before API call

### 2. PersonTracker (`person_tracker.h`)
**Features:**
- ✅ Multi-camera person tracking
- ✅ Dwell time calculation per camera
- ✅ Cross-camera movement detection
- ✅ Historical presence data
- ✅ Optional JSON persistence

**Key Methods:**
- `update_person()` - Update with recognition result
- `get_person_presence()` - Get person's current state
- `get_persons_at_camera()` - Who's at this camera now
- `get_cross_camera_persons()` - People across multiple cameras

## Integration Flow

```
Face Detection (SCRFD)
    ↓
Save Face Crop
    ↓
Queue for Recognition (CompreFaceClient)
    ↓
[Worker Thread Pool]
    ↓
HTTP POST to CompreFace API
    ↓
Parse Recognition Result
    ↓
Update PersonTracker
    ↓
Calculate Dwell Time
    ↓
Trigger PersonRecognitionEvent
    ↓
Log/Store/Alert
```

## Configuration

### CompreFace Config (`cameras.json`)
```json
{
  "compreface": {
    "enabled": true,
    "base_url": "http://localhost:8000",
    "api_key": "YOUR_API_KEY",
    "similarity_threshold": 0.7,
    "timeout_ms": 5000,
    "max_retries": 3,
    "cache_duration_seconds": 60
  },
  "person_tracking": {
    "enabled": true,
    "presence_timeout_seconds": 300,
    "same_camera_cooldown_seconds": 10,
    "enable_cross_camera_tracking": true,
    "enable_persistence": true,
    "persistence_path": "./person_tracking.json"
  }
}
```

## Performance Optimizations

### 1. Async Recognition Queue
- Non-blocking: Detection continues while recognition happens
- Thread pool: 4 workers (configurable)
- Queue size: 100 max (prevents memory overflow)

### 2. Result Caching
- Cache duration: 60 seconds default
- Prevents duplicate API calls for same face
- ~80% cache hit rate expected

### 3. HTTP Connection Pooling
- Reuse connections to CompreFace
- Reduces TCP handshake overhead
- ~30% faster than creating new connections

### 4. Circuit Breaker
- Opens after 5 consecutive failures
- Half-open state after 30 seconds
- Prevents API hammering during outages

## Error Handling

### Retry Logic
```
Attempt 1: Immediate
Attempt 2: +1s delay
Attempt 3: +2s delay (exponential backoff)
```

### Failure Modes
1. **Network Error**: Retry with backoff
2. **Timeout**: Retry with longer timeout
3. **API Error (4xx)**: Don't retry, log error
4. **API Error (5xx)**: Retry with backoff
5. **Circuit Open**: Skip, queue for later

## Events & Callbacks

### PersonRecognitionEvent
```cpp
{
  subject: "john_doe",
  camera_id: "front_door",
  similarity: 0.85,
  detection_confidence: 0.92,
  timestamp: "2025-11-25T14:30:00Z",
  is_new_person: false,
  is_new_at_camera: true,
  dwell_time_seconds: 45.2,
  other_cameras: ["back_door", "hallway"]
}
```

### Use Cases
- **Alert**: "John Doe entered front door (45s at camera, also seen at hallway)"
- **Analytics**: "John Doe visited 3 cameras, total time: 2m 30s"
- **Security**: "Unknown person detected at front door"

## Database Schema (Future)

### persons table
```sql
CREATE TABLE persons (
    subject VARCHAR(255) PRIMARY KEY,
    first_seen TIMESTAMP,
    last_seen TIMESTAMP,
    total_detections INT,
    avg_similarity FLOAT
);
```

### appearances table
```sql
CREATE TABLE appearances (
    id SERIAL PRIMARY KEY,
    subject VARCHAR(255),
    camera_id VARCHAR(255),
    first_seen TIMESTAMP,
    last_seen TIMESTAMP,
    detection_count INT,
    dwell_time_seconds FLOAT,
    FOREIGN KEY (subject) REFERENCES persons(subject)
);
```

## Next Steps

### Phase 1: Core Implementation ✅ (Headers Done)
- [x] CompreFaceClient header
- [x] PersonTracker header
- [ ] CompreFaceClient implementation
- [ ] PersonTracker implementation

### Phase 2: Integration
- [ ] Add to CMakeLists.txt
- [ ] Update FaceDetector to use CompreFaceClient
- [ ] Add configuration loading
- [ ] Wire up callbacks

### Phase 3: Testing
- [ ] Unit tests for CompreFaceClient
- [ ] Unit tests for PersonTracker
- [ ] Integration test with mock CompreFace
- [ ] Load testing

### Phase 4: Production Features
- [ ] Add database persistence (PostgreSQL)
- [ ] Add WebSocket event stream
- [ ] Add REST API for person queries
- [ ] Add Prometheus metrics export

## API Endpoints (REST API)

### GET /api/persons
List all tracked persons

### GET /api/persons/{subject}
Get specific person's presence data

### GET /api/cameras/{camera_id}/persons
Get persons currently at camera

### GET /api/persons/cross-camera
Get persons seen at multiple cameras

## Metrics

### CompreFace Client
- `recognition_requests_total` - Total API calls
- `recognition_success_total` - Successful recognitions
- `recognition_failures_total` - Failed API calls
- `recognition_cache_hits_total` - Cache hits
- `recognition_duration_seconds` - API response time histogram
- `recognition_queue_size` - Current queue size

### Person Tracker
- `persons_tracked_total` - Total unique persons
- `persons_currently_tracked` - Active persons
- `person_dwell_time_seconds` - Dwell time histogram
- `cross_camera_movements_total` - Cross-camera detections

## Security Considerations

1. **API Key Management**: Store in env vars, not config files
2. **TLS/HTTPS**: Use HTTPS for CompreFace in production
3. **Rate Limiting**: Limit API calls per second
4. **Data Privacy**: PII - implement retention policies
5. **Access Control**: Restrict person data API access

## Deployment Checklist

- [ ] Set CompreFace API key in environment
- [ ] Configure CompreFace URL (production endpoint)
- [ ] Set similarity threshold (test and tune)
- [ ] Enable persistence (database recommended)
- [ ] Configure retention policy (GDPR compliance)
- [ ] Set up monitoring/alerting
- [ ] Test failover scenarios
- [ ] Load test with expected volume
