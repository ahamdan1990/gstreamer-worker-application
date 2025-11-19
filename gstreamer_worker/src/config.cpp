#include "config.h"
#include <sstream>

namespace gstreamer_worker {

std::string CameraConfig::get_rtsp_location() const {
    if (!username.empty() && !password.empty()) {
        // Extract protocol and host from URL
        size_t proto_pos = rtsp_url.find("://");
        if (proto_pos != std::string::npos) {
            std::string protocol = rtsp_url.substr(0, proto_pos);
            std::string rest = rtsp_url.substr(proto_pos + 3);
            return protocol + "://" + username + ":" + password + "@" + rest;
        }
    }
    return rtsp_url;
}

bool CameraConfig::validate() const {
    if (camera_id.empty()) {
        return false;
    }

    if (rtsp_url.empty()) {
        return false;
    }

    if (rtsp_url.find("rtsp://") != 0) {
        return false;
    }

    if (target_fps <= 0 || target_fps > 60) {
        return false;
    }

    if (latency_ms < 0 || latency_ms > 5000) {
        return false;
    }

    return true;
}

bool PipelineManagerConfig::validate() const {
    if (metrics_interval <= 0.0) {
        return false;
    }

    return true;
}

} // namespace gstreamer_worker
