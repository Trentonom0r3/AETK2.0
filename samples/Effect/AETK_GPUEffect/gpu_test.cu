#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <aetk/effect/pixel/tensor_view.hpp>

using namespace aetk::effect;

__device__ inline float lerp_f(float a, float b, float t) {
    return a + t * (b - a);
}

// CUDA color inversion kernel
// AE GPU worlds use PF_PixelFormat_GPU_BGRA128 (BGRA float4):
// - .x = Blue
// - .y = Green
// - .z = Red
// - .w = Alpha
__global__ void InvertKernel_CUDA(
    const float4* src,
    float4* dst,
    int width,
    int height,
    int stride,
    float invert_ratio
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;

    int idx = y * stride + x;
    float4 pixel = src[idx];
    
    float4 result;
    result.x = lerp_f(pixel.x, 1.0f - pixel.x, invert_ratio); // Blue
    result.y = lerp_f(pixel.y, 1.0f - pixel.y, invert_ratio); // Green
    result.z = lerp_f(pixel.z, 1.0f - pixel.z, invert_ratio); // Red
    result.w = pixel.w; // Alpha
    
    dst[idx] = result;
}

__global__ void InvertKernel_Tensor_CUDA(
    tensor_view<float, 3, device_kind::cuda> src,
    tensor_view<float, 3, device_kind::cuda> dst,
    float invert_ratio
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    int width = src.shape(1);
    int height = src.shape(0);

    if (x >= width || y >= height) return;

    // Channels: 0 = Blue, 1 = Green, 2 = Red, 3 = Alpha
    for (int c = 0; c < 3; ++c) {
        float val = src(y, x, c);
        dst(y, x, c) = lerp_f(val, 1.0f - val, invert_ratio);
    }
    dst(y, x, 3) = src(y, x, 3);
}

extern "C" void launch_invert_kernel_cuda(
    const void* src_ptr,
    void* dst_ptr,
    int width,
    int height,
    int stride,
    float invert_ratio,
    cudaStream_t stream
) {
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);

    InvertKernel_CUDA<<<grid, block, 0, stream>>>(
        static_cast<const float4*>(src_ptr),
        static_cast<float4*>(dst_ptr),
        width, height, stride, invert_ratio
    );
}

extern void launch_invert_kernel_tensor_cuda(
    tensor_view<float, 3, device_kind::cuda> src_view,
    tensor_view<float, 3, device_kind::cuda> dst_view,
    float invert_ratio,
    cudaStream_t stream
) {
    int width = src_view.shape(1);
    int height = src_view.shape(0);

    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);

    InvertKernel_Tensor_CUDA<<<grid, block, 0, stream>>>(
        src_view,
        dst_view,
        invert_ratio
    );
}

#include <aetk/effect/gpu/kernels/swizzle.cu>

