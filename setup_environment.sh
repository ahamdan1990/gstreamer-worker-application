#!/bin/bash

# Quick Setup Script - Fix CUDA and Install ONNX Runtime
# Run this to prepare your system for production implementation

set -e  # Exit on error

echo "=========================================="
echo "  ENVIRONMENT SETUP FOR PRODUCTION"
echo "=========================================="
echo ""

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}Step 1: Configuring CUDA environment...${NC}"

# Check if CUDA is already in bashrc
if grep -q "CUDA_HOME=/usr/local/cuda-12.6" ~/.bashrc; then
    echo "  CUDA already configured in ~/.bashrc"
else
    echo "  Adding CUDA to ~/.bashrc..."
    echo '' >> ~/.bashrc
    echo '# CUDA Configuration (added by setup_environment.sh)' >> ~/.bashrc
    echo 'export CUDA_HOME=/usr/local/cuda-12.6' >> ~/.bashrc
    echo 'export PATH=/usr/local/cuda-12.6/bin:$PATH' >> ~/.bashrc
    echo 'export LD_LIBRARY_PATH=/usr/local/cuda-12.6/lib64:$LD_LIBRARY_PATH' >> ~/.bashrc
    echo -e "${GREEN}  ✓ CUDA environment variables added${NC}"
fi

# Apply immediately for this session
export CUDA_HOME=/usr/local/cuda-12.6
export PATH=/usr/local/cuda-12.6/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/cuda-12.6/lib64:$LD_LIBRARY_PATH

# Verify CUDA
if command -v nvcc &> /dev/null; then
    CUDA_VER=$(nvcc --version | grep "release" | awk '{print $5}' | cut -d',' -f1)
    echo -e "${GREEN}  ✓ CUDA Toolkit accessible: $CUDA_VER${NC}"
else
    echo "  ✗ ERROR: nvcc still not found after setting PATH"
    exit 1
fi

echo ""
echo -e "${YELLOW}Step 2: Installing ONNX Runtime (GPU) in backend venv...${NC}"

# Use backend venv
VENV_PATH="/home/ahamdan/Desktop/development/gstreamer/optimized_pipeline/backend/venv"

if [ ! -d "$VENV_PATH" ]; then
    echo "  ✗ ERROR: Backend venv not found at $VENV_PATH"
    exit 1
fi

# Activate venv and install
source "$VENV_PATH/bin/activate"

# Check if already installed
if python -c "import onnxruntime" &> /dev/null; then
    ONNX_VER=$(python -c "import onnxruntime; print(onnxruntime.__version__)")
    echo "  ONNX Runtime already installed: $ONNX_VER"

    # Check for GPU support
    PROVIDERS=$(python -c "import onnxruntime; print(','.join(onnxruntime.get_available_providers()))")
    if echo "$PROVIDERS" | grep -q "CUDAExecutionProvider"; then
        echo -e "${GREEN}  ✓ CUDA provider available${NC}"
    else
        echo "  WARNING: CUDA provider not available, reinstalling..."
        pip install --force-reinstall onnxruntime-gpu==1.16.3
    fi
else
    echo "  Installing onnxruntime-gpu in venv..."
    pip install onnxruntime-gpu==1.16.3

    # Verify installation
    if python -c "import onnxruntime" &> /dev/null; then
        ONNX_VER=$(python -c "import onnxruntime; print(onnxruntime.__version__)")
        echo -e "${GREEN}  ✓ ONNX Runtime installed: $ONNX_VER${NC}"

        PROVIDERS=$(python -c "import onnxruntime; print(','.join(onnxruntime.get_available_providers()))")
        echo "  Available providers: $PROVIDERS"
    else
        echo "  ✗ ERROR: ONNX Runtime installation failed"
        deactivate
        exit 1
    fi
fi

deactivate

echo ""
echo -e "${YELLOW}Step 3: Creating backup of current system...${NC}"

BACKUP_FILE="backup_$(date +%Y%m%d_%H%M%S).tar.gz"
cd /home/ahamdan/Desktop/development/gstreamer/optimized_pipeline

tar -czf "$BACKUP_FILE" \
    gstreamer_worker/src/ \
    gstreamer_worker/include/ \
    backend/app/ \
    --exclude='*.pyc' \
    --exclude='__pycache__' \
    --exclude='build/' \
    2>/dev/null || true

if [ -f "$BACKUP_FILE" ]; then
    BACKUP_SIZE=$(du -h "$BACKUP_FILE" | awk '{print $1}')
    echo -e "${GREEN}  ✓ Backup created: $BACKUP_FILE ($BACKUP_SIZE)${NC}"
else
    echo "  WARNING: Backup creation failed (non-critical)"
fi

echo ""
echo "=========================================="
echo "  SETUP COMPLETE!"
echo "=========================================="
echo ""
echo "✅ CUDA Toolkit configured"
echo "✅ ONNX Runtime installed"
echo "✅ Backup created"
echo ""
echo "Next steps:"
echo "  1. Close and reopen your terminal (or run: source ~/.bashrc)"
echo "  2. Run system audit: ./system_audit.sh"
echo "  3. Verify 0 critical failures"
echo "  4. Review PRODUCTION_IMPLEMENTATION_ROADMAP.md"
echo "  5. Start with Phase 1 (Zero-Copy Quick Win)"
echo ""
