# Setup Guide: Adding Third-Party Cameras

This guide explains how to add cameras from different vendors (Hikvision, Dahua, Axis, etc.) to the GStreamer Worker system.

## Quick Start

### Step 1: Identify Your Camera's RTSP URL

Different camera manufacturers use different RTSP URL formats:

| Manufacturer | RTSP URL Format | Example |
|-------------|-----------------|---------|
| **Hikvision** | `rtsp://IP:554/Streaming/Channels/CHANNEL` | `rtsp://192.168.1.100:554/Streaming/Channels/101` |
| **Dahua** | `rtsp://IP:554/cam/realmonitor?channel=X&subtype=Y` | `rtsp://192.168.1.101:554/cam/realmonitor?channel=1&subtype=0` |
| **Axis** | `rtsp://IP/axis-media/media.amp` | `rtsp://192.168.1.102/axis-media/media.amp` |
| **Foscam** | `rtsp://IP:554/videoMain` | `rtsp://192.168.1.103:554/videoMain` |
| **Amcrest** | `rtsp://IP:554/cam/realmonitor?channel=X` | `rtsp://192.168.1.104:554/cam/realmonitor?channel=1` |
| **Generic/ONVIF** | `rtsp://IP:554/stream1` | `rtsp://192.168.1.105:554/stream1` |

**Common RTSP Ports:**
- Main stream (high quality): Port 554, Channel 101
- Sub stream (lower quality): Port 554, Channel 102

### Step 2: Test Camera Connectivity

Before adding to configuration, test the RTSP URL:

```bash
# Test with gst-launch-1.0
gst-launch-1.0 rtspsrc location="rtsp://username:password@IP:554/path" \
    protocols=tcp latency=200 ! fakesink

# Or use VLC
vlc rtsp://username:password@IP:554/path
```

### Step 3: Create Configuration File

Create or edit `config/cameras.json`:

```json
{
  "manager": {
    "log_level": "INFO",
    "enable_metrics": true,
    "metrics_interval": 30.0
  },
  "cameras": [
    {
      "camera_id": "unique_camera_id",
      "rtsp_url": "rtsp://192.168.1.100:554/path",
      "username": "admin",
      "password": "password123",
      "protocols": "tcp",
      "latency_ms": 150,
      "target_fps": 12,
      "enable_display": true,
      "use_nvidia_decoder": true
    }
  ]
}
```

### Step 4: Run with Configuration

```bash
cd build
./gstreamer_worker_multi --config ../config/cameras.json
```

## Detailed Configuration Options

### Required Fields

```json
{
  "camera_id": "front_door",        // Unique identifier for this camera
  "rtsp_url": "rtsp://192.168.1.100:554/stream"  // RTSP stream URL
}
```

### Optional Fields (with defaults)

```json
{
  "username": "admin",                      // RTSP username
  "password": "password",                   // RTSP password
  "protocols": "tcp",                       // "tcp", "udp", or "tcp+udp"
  "latency_ms": 150,                       // Buffer latency in milliseconds
  "drop_on_latency": true,                 // Drop frames if latency exceeded
  "target_fps": 12,                        // Target framerate

  "enable_display": true,                  // Show live video window
  "display_sync": false,                   // Sync to clock (false = lower latency)

  "auto_reconnect": true,                  // Enable automatic reconnection
  "max_reconnect_attempts": -1,            // -1 = infinite retries
  "reconnect_initial_delay": 1.0,          // Initial reconnect delay (seconds)
  "reconnect_max_delay": 60.0,             // Max reconnect delay (seconds)
  "reconnect_backoff_multiplier": 2.0,     // Exponential backoff multiplier

  "health_check_interval": 5.0,            // Health check interval (seconds)
  "max_consecutive_errors": 5,             // Max errors before stopping

  "use_nvidia_decoder": true               // Use NVIDIA hardware acceleration
}
```

## Vendor-Specific Examples

### Example 1: Hikvision Cameras

```json
{
  "cameras": [
    {
      "camera_id": "hikvision_front",
      "rtsp_url": "rtsp://192.168.1.100:554/Streaming/Channels/101",
      "username": "admin",
      "password": "Hik12345",
      "protocols": "tcp",
      "latency_ms": 200,
      "target_fps": 15,
      "enable_display": true
    },
    {
      "camera_id": "hikvision_back",
      "rtsp_url": "rtsp://192.168.1.101:554/Streaming/Channels/102",
      "username": "admin",
      "password": "Hik12345",
      "protocols": "tcp",
      "latency_ms": 150,
      "target_fps": 10,
      "enable_display": true
    }
  ]
}
```

**Hikvision Notes:**
- Channel 101 = Main stream (high quality)
- Channel 102 = Sub stream (lower quality, better for processing)
- Default port: 554
- Some models use `/h264/ch1/main/av_stream` format

### Example 2: Dahua Cameras

```json
{
  "cameras": [
    {
      "camera_id": "dahua_parking",
      "rtsp_url": "rtsp://192.168.1.110:554/cam/realmonitor?channel=1&subtype=0",
      "username": "admin",
      "password": "Dahua123",
      "protocols": "tcp",
      "latency_ms": 150,
      "target_fps": 12,
      "enable_display": true
    }
  ]
}
```

**Dahua Notes:**
- `subtype=0` = Main stream
- `subtype=1` = Sub stream
- Default credentials often: admin/admin

### Example 3: Axis Cameras

```json
{
  "cameras": [
    {
      "camera_id": "axis_lobby",
      "rtsp_url": "rtsp://192.168.1.120/axis-media/media.amp",
      "username": "root",
      "password": "Axis456",
      "protocols": "tcp",
      "latency_ms": 100,
      "target_fps": 20,
      "enable_display": true
    }
  ]
}
```

**Axis Notes:**
- Default username is usually "root"
- Add `?resolution=640x480` for specific resolution
- Generally lowest latency of major vendors

### Example 4: Mixed Vendor Setup

```json
{
  "manager": {
    "log_level": "INFO",
    "enable_metrics": true,
    "metrics_interval": 60.0
  },
  "cameras": [
    {
      "camera_id": "hikvision_front_door",
      "rtsp_url": "rtsp://192.168.1.100:554/Streaming/Channels/101",
      "username": "admin",
      "password": "hik123",
      "latency_ms": 200,
      "target_fps": 15
    },
    {
      "camera_id": "dahua_back_door",
      "rtsp_url": "rtsp://192.168.1.101:554/cam/realmonitor?channel=1&subtype=0",
      "username": "admin",
      "password": "dahua123",
      "latency_ms": 150,
      "target_fps": 12
    },
    {
      "camera_id": "axis_parking_lot",
      "rtsp_url": "rtsp://192.168.1.102/axis-media/media.amp",
      "username": "root",
      "password": "axis456",
      "latency_ms": 100,
      "target_fps": 20
    },
    {
      "camera_id": "foscam_warehouse",
      "rtsp_url": "rtsp://192.168.1.103:554/videoMain",
      "username": "admin",
      "password": "foscam789",
      "latency_ms": 150,
      "target_fps": 10
    }
  ]
}
```

## Step-by-Step Integration Process

### 1. Discovery Phase

```bash
# Scan your network for cameras (requires nmap)
sudo nmap -p 554,8554 192.168.1.0/24

# Check if RTSP port is open
nc -zv 192.168.1.100 554

# Try common URLs with VLC or ffplay
vlc rtsp://admin:admin@192.168.1.100:554/stream
```

### 2. Credential Management

**Option A: Embedded in URL** (less secure, but simple)
```json
"rtsp_url": "rtsp://username:password@192.168.1.100:554/stream"
```

**Option B: Separate fields** (recommended)
```json
"rtsp_url": "rtsp://192.168.1.100:554/stream",
"username": "admin",
"password": "secure_password"
```

### 3. Performance Tuning

**For Reliable Connection** (prioritize stability):
```json
{
  "protocols": "tcp",
  "latency_ms": 200,
  "drop_on_latency": false,
  "reconnect_max_delay": 120.0
}
```

**For Low Latency** (prioritize speed):
```json
{
  "protocols": "tcp",
  "latency_ms": 50,
  "drop_on_latency": true,
  "display_sync": false
}
```

**For High Throughput** (many cameras):
```json
{
  "target_fps": 5,
  "latency_ms": 200,
  "enable_display": false
}
```

### 4. Validation

```bash
# Validate configuration before deploying
./gstreamer_worker_multi --validate-config config/cameras.json

# If valid, deploy
./gstreamer_worker_multi --config config/cameras.json
```

## Troubleshooting

### Camera Not Connecting

**Check 1: Network connectivity**
```bash
ping 192.168.1.100
nc -zv 192.168.1.100 554
```

**Check 2: Credentials**
```bash
# Test with curl
curl -v "rtsp://admin:password@192.168.1.100:554/stream"

# Test with gst-launch-1.0
gst-launch-1.0 rtspsrc location="rtsp://admin:password@192.168.1.100:554/stream" \
    ! fakesink
```

**Check 3: RTSP URL format**
- Consult camera manual
- Check manufacturer's documentation
- Try common patterns for your brand

### Frequent Disconnections

**Solution 1: Increase latency**
```json
"latency_ms": 300
```

**Solution 2: Adjust reconnection**
```json
"reconnect_initial_delay": 2.0,
"reconnect_max_delay": 120.0
```

**Solution 3: Use TCP**
```json
"protocols": "tcp"
```

### Poor Video Quality

**Solution: Check stream type**
- Main stream = High quality, high bandwidth
- Sub stream = Lower quality, lower bandwidth

```json
// Hikvision main stream (high quality)
"rtsp_url": "rtsp://IP:554/Streaming/Channels/101"

// Hikvision sub stream (lower quality)
"rtsp_url": "rtsp://IP:554/Streaming/Channels/102"
```

## Real-World Deployment Checklist

- [ ] All cameras tested individually with VLC/gst-launch-1.0
- [ ] Configuration file validated
- [ ] Network bandwidth sufficient for all streams
- [ ] Credentials stored securely
- [ ] Backup/failover plan documented
- [ ] Monitoring/alerting configured
- [ ] Log rotation enabled
- [ ] Recovery procedures documented

## Adding New Camera (Quick Steps)

1. **Get camera details:**
   - IP address
   - RTSP port (usually 554)
   - Username/password
   - Stream path

2. **Test URL:**
   ```bash
   vlc rtsp://user:pass@IP:554/path
   ```

3. **Add to config:**
   ```json
   {
     "camera_id": "new_camera",
     "rtsp_url": "rtsp://IP:554/path",
     "username": "user",
     "password": "pass"
   }
   ```

4. **Validate:**
   ```bash
   ./gstreamer_worker_multi --validate-config config/cameras.json
   ```

5. **Deploy:**
   ```bash
   ./gstreamer_worker_multi --config config/cameras.json
   ```

## Security Best Practices

1. **Never commit passwords to git:**
   ```bash
   # Add to .gitignore
   config/cameras_production.json
   ```

2. **Use environment variables:**
   ```json
   // Not directly supported yet, but can preprocess config
   "password": "${CAMERA_PASSWORD}"
   ```

3. **Restrict network access:**
   - Use VLANs for camera network
   - Firewall rules limiting access
   - Change default passwords

4. **Regular updates:**
   - Keep camera firmware updated
   - Monitor for security patches
   - Audit access logs

## Performance Guidelines

| Cameras | Recommended Hardware | Notes |
|---------|---------------------|-------|
| 1-5 | Entry NVIDIA GPU (GTX 1650+) | ~10-20% GPU usage per camera |
| 5-15 | Mid-range NVIDIA GPU (RTX 3060+) | Monitor GPU memory |
| 15-30 | High-end NVIDIA GPU (RTX 3080+) | Consider multiple workers |
| 30+ | Multiple GPUs or machines | Distribute load |

## Support

For camera-specific RTSP URLs, consult:
- Camera's web interface (usually under "Network" or "Streaming")
- Manufacturer's RTSP documentation
- [ONVIF Device Manager](https://sourceforge.net/projects/onvifdm/) - can discover RTSP URLs
- Camera manual's network configuration section
