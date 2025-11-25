#!/bin/bash

# Full Zero-Copy NVMM Implementation Script
# This applies all changes for enterprise-grade zero-copy pipeline

set -e  # Exit on error

cd /home/ahamdan/Desktop/development/gstreamer/optimized_pipeline

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo "=========================================="
echo "  ZERO-COPY NVMM IMPLEMENTATION"
echo "=========================================="
echo ""

# Source CUDA environment
export CUDA_HOME=/usr/local/cuda-12.6
export PATH=/usr/local/cuda-12.6/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/cuda-12.6/lib64:$LD_LIBRARY_PATH

echo -e "${YELLOW}Phase 1: Backing up current files...${NC}"

BACKUP_DIR="zero_copy_backup_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BACKUP_DIR"

# Backup files we'll modify
cp gstreamer_worker/src/camera_stream.cpp "$BACKUP_DIR/"
cp gstreamer_worker/src/motion_detector.cpp "$BACKUP_DIR/"
cp gstreamer_worker/include/motion_detector.h "$BACKUP_DIR/"

echo -e "${GREEN}✓ Backup created: $BACKUP_DIR/${NC}"

echo ""
echo -e "${YELLOW}Phase 2: Applying code changes...${NC}"

# We'll apply the changes directly via the script
# The actual file modifications will be done by the user or me in the next step

echo -e "${YELLOW}Creating patch files for review...${NC}"

# Create motion_detector.h patch
cat > "$BACKUP_DIR/motion_detector_h.patch" << 'PATCH_EOF'
Add this method to motion_detector.h after line 70:

    /**
     * @brief Process a video frame for motion detection (GPU input - zero-copy)
     * @param d_frame Input frame on GPU (CUDA GpuMat)
     * @return true if motion was detected in this frame
     */
    bool process_frame_cuda(const cv::cuda::GpuMat& d_frame);
PATCH_EOF

echo -e "${GREEN}✓ Created motion_detector_h.patch${NC}"

echo ""
echo -e "${YELLOW}Phase 3: Ready for implementation${NC}"
echo ""
echo "Next steps:"
echo "  1. Review the patches in: $BACKUP_DIR/"
echo "  2. Apply code changes (I'll help with this)"
echo "  3. Rebuild worker"
echo "  4. Test with cameras"
echo ""
echo "Files backed up to: $BACKUP_DIR/"
echo ""
