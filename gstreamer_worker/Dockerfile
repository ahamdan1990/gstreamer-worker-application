FROM nvidia/cuda:11.8.0-devel-ubuntu22.04

WORKDIR /app

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-tools \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav \
    gstreamer1.0-rtsp \
    gstreamer1.0-x \
    gstreamer1.0-gl \
    libglib2.0-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy source code
COPY . .

# Build application
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# Expose port for REST API (future feature)
EXPOSE 8081

# Run the multi-camera worker
CMD ["./build/gstreamer_worker_multi", "--config", "config/cameras.json"]
