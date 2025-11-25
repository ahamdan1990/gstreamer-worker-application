cmake_minimum_required(VERSION 3.16)
project(GStreamerWorker VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# Find packages
find_package(PkgConfig REQUIRED)

# GStreamer
pkg_check_modules(GSTREAMER REQUIRED
    gstreamer-1.0>=1.14
    gstreamer-video-1.0>=1.14
    gstreamer-rtsp-1.0>=1.14
    gstreamer-app-1.0>=1.14
)

# GLib
pkg_check_modules(GLIB REQUIRED glib-2.0>=2.56)

# OpenCV with CUDA support
find_package(OpenCV 4.0 REQUIRED
    COMPONENTS
        core
        imgproc
        imgcodecs
        video
        cudabgsegm
        cudaimgproc
        cudawarping
        cudafilters
        cudaarithm
)

if(OpenCV_FOUND)
    message(STATUS "OpenCV version: ${OpenCV_VERSION}")
    message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
    message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")

    # Check if CUDA modules are available
    if(OpenCV_CUDA_VERSION)
        message(STATUS "OpenCV CUDA support: YES (${OpenCV_CUDA_VERSION})")
    else()
        message(WARNING "OpenCV found but CUDA support not detected. Motion detection will use CPU.")
    endif()
else()
    message(FATAL_ERROR "OpenCV not found. Please install OpenCV with CUDA support.")
endif()

# Threads
find_package(Threads REQUIRED)

# ONNX Runtime (for face detection)
set(ONNXRUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime")
set(ONNXRUNTIME_INCLUDE_DIRS "${ONNXRUNTIME_ROOT}/include")
set(ONNXRUNTIME_LIBRARIES "${ONNXRUNTIME_ROOT}/lib/libonnxruntime.so")

if(EXISTS "${ONNXRUNTIME_INCLUDE_DIRS}" AND EXISTS "${ONNXRUNTIME_LIBRARIES}")
    message(STATUS "ONNX Runtime found: ${ONNXRUNTIME_ROOT}")
    message(STATUS "  Include: ${ONNXRUNTIME_INCLUDE_DIRS}")
    message(STATUS "  Library: ${ONNXRUNTIME_LIBRARIES}")
else()
    message(WARNING "ONNX Runtime not found in third_party/onnxruntime")
    message(WARNING "Face detection will not be available")
endif()

# Enable FetchContent for downloading dependencies
include(FetchContent)

# nlohmann/json (header-only)
# Try to find system package first, fallback to fetching
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann/json not found, fetching from GitHub...")
    FetchContent_Declare(json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(json)
endif()

# cpp-httplib (header-only HTTP server)
FetchContent_Declare(httplib
    URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.14.3.tar.gz
)
FetchContent_MakeAvailable(httplib)

# IXWebSocket (WebSocket client/server library)
FetchContent_Declare(ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG v11.4.5
)
set(USE_TLS ON CACHE BOOL "Enable TLS support for IXWebSocket")
set(USE_ZLIB ON CACHE BOOL "Enable ZLIB compression for IXWebSocket")
FetchContent_MakeAvailable(ixwebsocket)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${GSTREAMER_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    ${ONNXRUNTIME_INCLUDE_DIRS}
)

# Link directories
link_directories(
    ${GSTREAMER_LIBRARY_DIRS}
    ${GLIB_LIBRARY_DIRS}
)

# Source files
set(SOURCES
    src/camera_stream.cpp
    src/pipeline_manager.cpp
    src/config.cpp
    src/config_loader.cpp
    src/logger.cpp
    src/motion_detector.cpp
    src/face_detector.cpp
    src/websocket_server.cpp
    src/event_broadcaster.cpp
)

# API Server sources
set(API_SOURCES
    src/api_server.cpp
)

# Headers
set(HEADERS
    include/camera_stream.h
    include/pipeline_manager.h
    include/config.h
    include/config_loader.h
    include/logger.h
    include/types.h
    include/api_server.h
    include/motion_detector.h
    include/face_detector.h
    include/websocket_server.h
    include/event_broadcaster.h
)

# Create library
add_library(gstreamer_worker_lib STATIC ${SOURCES} ${HEADERS})
target_link_libraries(gstreamer_worker_lib
    ${GSTREAMER_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${OpenCV_LIBS}
    ${ONNXRUNTIME_LIBRARIES}
    Threads::Threads
    nlohmann_json::nlohmann_json
    ixwebsocket
)

# Main executable (single camera)
add_executable(gstreamer_worker src/main.cpp)
target_link_libraries(gstreamer_worker
    gstreamer_worker_lib
    ${GSTREAMER_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${OpenCV_LIBS}
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# Multi-camera executable (with config file support)
add_executable(gstreamer_worker_multi src/main_multi.cpp)
target_link_libraries(gstreamer_worker_multi
    gstreamer_worker_lib
    ${GSTREAMER_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${OpenCV_LIBS}
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# API Server executable (with REST API + WebSocket support)
add_executable(gstreamer_worker_api src/main_api.cpp ${API_SOURCES})
target_link_libraries(gstreamer_worker_api
    gstreamer_worker_lib
    ${GSTREAMER_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${OpenCV_LIBS}
    Threads::Threads
    nlohmann_json::nlohmann_json
    httplib::httplib
    ixwebsocket
)

# Install targets
install(TARGETS gstreamer_worker gstreamer_worker_multi gstreamer_worker_api
    RUNTIME DESTINATION bin
)

# Print configuration
message(STATUS "")
message(STATUS "GStreamer Worker Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  GStreamer: ${GSTREAMER_VERSION}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
