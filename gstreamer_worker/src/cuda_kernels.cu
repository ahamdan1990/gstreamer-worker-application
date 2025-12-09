#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <stdint.h>

/**
 * @brief CUDA kernel for zero-copy face detection preprocessing
 *
 * Converts BGR HWC format to RGB CHW format with normalization in a single pass.
 * This eliminates CPU↔GPU transfers and the terrible triple-nested loop.
 *
 * Input: BGR image in HWC format (Height × Width × Channels)
 * Output: RGB normalized tensor in CHW format (Channels × Height × Width)
 * Normalization: (pixel / 128.0) - 1.0 → range [-1, 1] for SCRFD model
 */
__global__ void preprocess_face_detection_kernel(
    const uint8_t* __restrict__ input,   // Input: BGR HWC [H, W, 3]
    float* __restrict__ output,          // Output: RGB CHW normalized [3, H, W]
    const int height,
    const int width
) {
    // Calculate global thread index
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;

    // Check bounds
    if (x >= width || y >= height) {
        return;
    }

    // Calculate input index (HWC format: row-major BGR)
    const int input_idx = (y * width + x) * 3;

    // Load BGR values
    const uint8_t b = input[input_idx + 0];
    const uint8_t g = input[input_idx + 1];
    const uint8_t r = input[input_idx + 2];

    // Calculate output indices (CHW format: channel-first RGB)
    const int hw = height * width;
    const int output_base = y * width + x;

    // Convert to float, normalize, and store in RGB order (CHW)
    // Normalization: (pixel / 128.0) - 1.0
    output[0 * hw + output_base] = (float)r * (1.0f / 255.0f);  // R channel
    output[1 * hw + output_base] = (float)g * (1.0f / 255.0f);   // G channel
    output[2 * hw + output_base] = (float)b * (1.0f / 255.0f);  // B channel
}

/**
 * @brief Host function to launch the preprocessing kernel
 *
 * @param d_input Input image on GPU (BGR, uint8, HWC format)
 * @param d_output Output tensor on GPU (RGB, float32, CHW format, normalized)
 * @param height Image height
 * @param width Image width
 * @param stream CUDA stream for async execution (optional)
 */
extern "C" void launch_preprocess_face_detection(
    const uint8_t* d_input,
    float* d_output,
    int height,
    int width,
    cudaStream_t stream = 0
) {
    // Use 16x16 thread blocks (good for image processing)
    dim3 block_size(16, 16);
    dim3 grid_size(
        (width + block_size.x - 1) / block_size.x,
        (height + block_size.y - 1) / block_size.y
    );

    // Launch kernel
    preprocess_face_detection_kernel<<<grid_size, block_size, 0, stream>>>(
        d_input,
        d_output,
        height,
        width
    );

    // Check for launch errors (synchronous for debugging, can be removed in production)
#ifdef DEBUG
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        fprintf(stderr, "CUDA kernel launch failed: %s\n", cudaGetErrorString(err));
    }
#endif
}

/**
 * @brief Alternative kernel with coalesced memory access pattern
 *
 * This version processes in a way that ensures coalesced global memory access,
 * which can be faster on some GPU architectures.
 */
__global__ void preprocess_face_detection_kernel_coalesced(
    const uint8_t* __restrict__ input,
    int input_step,                     // 🔴 NEW: row stride in bytes
    float* __restrict__ output,
    const int height,
    const int width
) {
    const int tid = blockIdx.x * blockDim.x + threadIdx.x;
    const int total_pixels = height * width;

    if (tid >= total_pixels) {
        return;
    }

    const int y = tid / width;
    const int x = tid % width;

    // Respect pitch/step (GpuMat rows are not guaranteed to be tightly packed)
    const int input_idx = y * input_step + x * 3;

    const uint8_t b = input[input_idx + 0];
    const uint8_t g = input[input_idx + 1];
    const uint8_t r = input[input_idx + 2];

    const int hw = total_pixels;

    // ✅ SCRFD-style normalization: (pixel - 127.5) / 128  → ~[-1, 1]
    const float mean   = 127.5f;
    const float invstd = 1.0f / 128.0f;

    output[0 * hw + tid] = (static_cast<float>(r) - mean) * invstd; // R
    output[1 * hw + tid] = (static_cast<float>(g) - mean) * invstd; // G
    output[2 * hw + tid] = (static_cast<float>(b) - mean) * invstd; // B
}

extern "C" void launch_preprocess_face_detection_coalesced(
    const uint8_t* d_input,
    int input_step,            // 🔴 NEW
    float* d_output,
    int height,
    int width,
    cudaStream_t stream
) {
    const int total_pixels = height * width;
    const int block_size   = 256;
    const int grid_size    = (total_pixels + block_size - 1) / block_size;

    preprocess_face_detection_kernel_coalesced<<<grid_size, block_size, 0, stream>>>(
        d_input,
        input_step,
        d_output,
        height,
        width
    );

#ifdef DEBUG
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        fprintf(stderr, "CUDA kernel launch failed: %s\n", cudaGetErrorString(err));
    }
#endif
}

