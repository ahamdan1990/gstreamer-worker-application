#!/bin/bash

# System Audit for Zero-Copy and Enterprise Production
# This script checks all prerequisites and capabilities

echo "=========================================="
echo "  SYSTEM AUDIT FOR PRODUCTION DEPLOYMENT"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

check_pass() { echo -e "${GREEN}✓${NC} $1"; }
check_fail() { echo -e "${RED}✗${NC} $1"; }
check_warn() { echo -e "${YELLOW}⚠${NC} $1"; }

# Track overall status
CRITICAL_FAILURES=0
WARNINGS=0

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "1. GPU HARDWARE CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if command -v nvidia-smi &> /dev/null; then
    GPU_INFO=$(nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader 2>/dev/null)
    if [ $? -eq 0 ]; then
        check_pass "NVIDIA GPU detected"
        echo "   GPU: $GPU_INFO"

        # Extract memory
        GPU_MEM=$(nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits 2>/dev/null)
        if [ "$GPU_MEM" -lt 2000 ]; then
            check_warn "GPU memory is ${GPU_MEM}MB (recommended: 4GB+)"
            WARNINGS=$((WARNINGS + 1))
        else
            check_pass "GPU memory: ${GPU_MEM}MB (sufficient)"
        fi
    else
        check_fail "nvidia-smi command failed"
        CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
    fi
else
    check_fail "nvidia-smi not found - No NVIDIA GPU or drivers"
    CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "2. CUDA TOOLKIT CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if command -v nvcc &> /dev/null; then
    CUDA_VERSION=$(nvcc --version | grep "release" | awk '{print $5}' | cut -d',' -f1)
    check_pass "CUDA Toolkit installed: version $CUDA_VERSION"

    # Check CUDA path
    if [ -d "$CUDA_HOME" ]; then
        check_pass "CUDA_HOME set: $CUDA_HOME"
    elif [ -d "/usr/local/cuda" ]; then
        check_warn "CUDA_HOME not set, but found at /usr/local/cuda"
        export CUDA_HOME=/usr/local/cuda
        WARNINGS=$((WARNINGS + 1))
    else
        check_fail "CUDA_HOME not set"
        CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
    fi

    # Check for CUDA libraries
    if [ -f "$CUDA_HOME/lib64/libcudart.so" ]; then
        check_pass "CUDA runtime library found"
    else
        check_fail "CUDA runtime library missing"
        CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
    fi
else
    check_fail "nvcc not found - CUDA Toolkit not installed"
    CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "3. OPENCV WITH CUDA CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if pkg-config --exists opencv4; then
    OPENCV_VERSION=$(pkg-config --modversion opencv4)
    check_pass "OpenCV installed: version $OPENCV_VERSION"

    # Check for CUDA support in OpenCV
    CUDA_CHECK=$(python3 -c "import cv2; print(cv2.cuda.getCudaEnabledDeviceCount())" 2>/dev/null)
    if [ "$CUDA_CHECK" -gt 0 ]; then
        check_pass "OpenCV CUDA support: $CUDA_CHECK device(s)"
    else
        check_fail "OpenCV NOT compiled with CUDA support"
        CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
        echo "   → OpenCV needs to be recompiled with CUDA"
    fi
else
    check_fail "OpenCV4 not found"
    CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "4. GSTREAMER CHECK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if command -v gst-inspect-1.0 &> /dev/null; then
    GST_VERSION=$(gst-inspect-1.0 --version | grep "version" | awk '{print $3}')
    check_pass "GStreamer installed: version $GST_VERSION"

    # Check critical plugins
    echo "   Checking essential plugins:"

    # Core plugins
    if gst-inspect-1.0 rtspsrc &> /dev/null; then
        check_pass "  rtspsrc (RTSP source)"
    else
        check_fail "  rtspsrc missing"
        CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
    fi

    if gst-inspect-1.0 appsink &> /dev/null; then
        check_pass "  appsink (frame extraction)"
    else
        check_fail "  appsink missing"
        CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
    fi

    # NVIDIA plugins (critical for zero-copy)
    echo "   Checking NVIDIA hardware plugins:"

    if gst-inspect-1.0 nvv4l2decoder &> /dev/null; then
        check_pass "  nvv4l2decoder (HW decode) - ZERO-COPY CAPABLE"
        HW_DECODE=1
    else
        check_warn "  nvv4l2decoder not found - will use software decode"
        HW_DECODE=0
        WARNINGS=$((WARNINGS + 1))
    fi

    if gst-inspect-1.0 nvvideoconvert &> /dev/null; then
        check_pass "  nvvideoconvert (GPU format conversion) - ZERO-COPY CAPABLE"
        HW_CONVERT=1
    else
        check_warn "  nvvideoconvert not found - will use software conversion"
        HW_CONVERT=0
        WARNINGS=$((WARNINGS + 1))
    fi

    if gst-inspect-1.0 nvv4l2h264enc &> /dev/null; then
        check_pass "  nvv4l2h264enc (HW encode)"
    else
        check_warn "  nvv4l2h264enc not found - software encode only"
        WARNINGS=$((WARNINGS + 1))
    fi

    # Software fallback plugins
    echo "   Checking software fallback plugins:"

    if gst-inspect-1.0 avdec_h264 &> /dev/null; then
        check_pass "  avdec_h264 (SW H.264 decode)"
    else
        check_fail "  avdec_h264 missing"
        CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
    fi

    if gst-inspect-1.0 videoconvert &> /dev/null; then
        check_pass "  videoconvert (SW format conversion)"
    else
        check_fail "  videoconvert missing"
        CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
    fi

else
    check_fail "GStreamer not found"
    CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "5. ONNX RUNTIME CHECK (for Face Detection)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if python3 -c "import onnxruntime" 2>/dev/null; then
    ONNX_VERSION=$(python3 -c "import onnxruntime; print(onnxruntime.__version__)" 2>/dev/null)
    check_pass "ONNX Runtime installed: version $ONNX_VERSION"

    # Check for available providers
    PROVIDERS=$(python3 -c "import onnxruntime; print(','.join(onnxruntime.get_available_providers()))" 2>/dev/null)
    echo "   Available providers: $PROVIDERS"

    if echo "$PROVIDERS" | grep -q "TensorrtExecutionProvider"; then
        check_pass "  TensorRT provider available (BEST performance)"
    else
        check_warn "  TensorRT provider not available"
        WARNINGS=$((WARNINGS + 1))
    fi

    if echo "$PROVIDERS" | grep -q "CUDAExecutionProvider"; then
        check_pass "  CUDA provider available (GOOD performance)"
    else
        check_warn "  CUDA provider not available - will use CPU"
        WARNINGS=$((WARNINGS + 1))
    fi
else
    check_warn "ONNX Runtime not installed (needed for face detection)"
    echo "   → Install with: pip install onnxruntime-gpu"
    WARNINGS=$((WARNINGS + 1))
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "6. SYSTEM RESOURCES"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# CPU info
CPU_CORES=$(nproc)
check_pass "CPU cores: $CPU_CORES"

# RAM info
TOTAL_RAM=$(free -h | awk '/^Mem:/ {print $2}')
AVAIL_RAM=$(free -h | awk '/^Mem:/ {print $7}')
check_pass "RAM: $TOTAL_RAM total, $AVAIL_RAM available"

# Disk space
DISK_AVAIL=$(df -h /home | awk 'NR==2 {print $4}')
check_pass "Disk space available: $DISK_AVAIL"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "7. DEVELOPMENT TOOLS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if command -v cmake &> /dev/null; then
    CMAKE_VERSION=$(cmake --version | head -1 | awk '{print $3}')
    check_pass "CMake installed: version $CMAKE_VERSION"
else
    check_fail "CMake not found"
    CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
fi

if command -v g++ &> /dev/null; then
    GCC_VERSION=$(g++ --version | head -1 | awk '{print $3}')
    check_pass "G++ installed: version $GCC_VERSION"
else
    check_fail "G++ not found"
    CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
fi

if command -v pkg-config &> /dev/null; then
    check_pass "pkg-config available"
else
    check_fail "pkg-config not found"
    CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "8. LIBRARY DEPENDENCIES"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Check for important libraries
if ldconfig -p | grep -q libcudart; then
    check_pass "libcudart (CUDA runtime)"
else
    check_fail "libcudart not found"
    CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
fi

if ldconfig -p | grep -q libgstreamer; then
    check_pass "libgstreamer (GStreamer libraries)"
else
    check_fail "libgstreamer not found"
    CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
fi

if pkg-config --exists gstreamer-app-1.0; then
    check_pass "gstreamer-app-1.0 (AppSink support)"
else
    check_fail "gstreamer-app-1.0 not found"
    CRITICAL_FAILURES=$((CRITICAL_FAILURES + 1))
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "9. CURRENT BUILD STATUS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if [ -f "/home/ahamdan/Desktop/development/gstreamer/optimized_pipeline/gstreamer_worker/build/gstreamer_worker_api" ]; then
    check_pass "Worker binary exists"

    # Check if it runs
    if /home/ahamdan/Desktop/development/gstreamer/optimized_pipeline/gstreamer_worker/build/gstreamer_worker_api --help &> /dev/null; then
        check_pass "Worker binary is executable"
    else
        check_warn "Worker binary exists but may need rebuild"
        WARNINGS=$((WARNINGS + 1))
    fi
else
    check_warn "Worker not built yet (normal for new setup)"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SUMMARY"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

echo ""
if [ $CRITICAL_FAILURES -eq 0 ]; then
    echo -e "${GREEN}✓ No critical failures found${NC}"
else
    echo -e "${RED}✗ $CRITICAL_FAILURES critical failure(s) found${NC}"
fi

if [ $WARNINGS -eq 0 ]; then
    echo -e "${GREEN}✓ No warnings${NC}"
else
    echo -e "${YELLOW}⚠ $WARNINGS warning(s) found${NC}"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "ZERO-COPY CAPABILITY ASSESSMENT"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if [ $HW_DECODE -eq 1 ] && [ $HW_CONVERT -eq 1 ]; then
    echo -e "${GREEN}✓ FULL ZERO-COPY CAPABLE${NC}"
    echo "   Your system has all required NVIDIA plugins for zero-copy"
    echo "   Can proceed with Option 1 (Full NVMM Pipeline)"
elif [ $HW_DECODE -eq 0 ] || [ $HW_CONVERT -eq 0 ]; then
    echo -e "${YELLOW}⚠ PARTIAL ZERO-COPY${NC}"
    echo "   Missing NVIDIA plugins - will need software fallback"
    echo "   Recommend: Option 2 (Optimized with one upload)"
else
    echo -e "${RED}✗ NO ZERO-COPY SUPPORT${NC}"
    echo "   No NVIDIA hardware acceleration available"
    echo "   Will use CPU pipeline"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "RECOMMENDED NEXT STEPS"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if [ $CRITICAL_FAILURES -gt 0 ]; then
    echo ""
    echo "🚨 CRITICAL: Fix these issues before proceeding:"
    echo ""
    echo "1. Install missing critical dependencies"
    echo "2. Verify GPU drivers and CUDA installation"
    echo "3. Check OpenCV CUDA compilation"
    echo "4. Re-run this audit script"
else
    echo ""
    echo "✅ System is ready for implementation!"
    echo ""

    if [ $HW_DECODE -eq 1 ] && [ $HW_CONVERT -eq 1 ]; then
        echo "📋 RECOMMENDED IMPLEMENTATION PATH:"
        echo ""
        echo "Phase 1 (Immediate - Low Risk):"
        echo "  1. Remove frame cloning (camera_stream.cpp:676)"
        echo "  2. Add GPU upload instead of clone"
        echo "  3. Expected gain: 50% bandwidth reduction"
        echo ""
        echo "Phase 2 (1-2 weeks - Medium Risk):"
        echo "  4. Implement NVMM zero-copy pipeline"
        echo "  5. Add CUDA buffer mapping"
        echo "  6. Expected gain: 90% bandwidth reduction"
    else
        echo "📋 RECOMMENDED IMPLEMENTATION PATH:"
        echo ""
        echo "Phase 1 (Immediate - Low Risk):"
        echo "  1. Remove frame cloning"
        echo "  2. Use single GPU upload and reuse buffer"
        echo "  3. Expected gain: 50% bandwidth reduction"
        echo ""
        echo "Note: Consider installing gstreamer-plugins-bad for HW acceleration"
    fi
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Audit complete. Save this output for reference."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
