#!/bin/bash
# Build script for GStreamer Worker

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  GStreamer Worker - Build Script${NC}"
echo -e "${GREEN}========================================${NC}"

# Parse arguments
BUILD_TYPE="Release"
CLEAN=false
INSTALL=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --install)
            INSTALL=true
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --debug     Build in debug mode (default: Release)"
            echo "  --clean     Clean build directory before building"
            echo "  --install   Install after building"
            echo "  --help      Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf build
fi

# Create build directory
mkdir -p build
cd build

# Configure
echo -e "${YELLOW}Configuring with CMake (${BUILD_TYPE})...${NC}"
cmake .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE}

# Build
echo -e "${YELLOW}Building...${NC}"
make -j$(nproc)

# Install if requested
if [ "$INSTALL" = true ]; then
    echo -e "${YELLOW}Installing...${NC}"
    sudo make install
fi

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Build completed successfully!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Executable: build/gstreamer_worker"
echo ""
echo "Run with:"
echo "  cd build"
echo "  ./gstreamer_worker --help"
echo ""
