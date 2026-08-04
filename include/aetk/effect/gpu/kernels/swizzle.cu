#include <aetk/effect/gpu/kernels/swizzle.h>
#include <device_launch_parameters.h>

namespace aetk::effect::draw {

__global__ void swizzle_inplace_kernel(
    float* data,
    int pitch_floats,
    int width,
    int height
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;

    float* pixel = data + y * pitch_floats + x * 4;
    float4* pixel_ptr = reinterpret_cast<float4*>(pixel);
    float4 val = *pixel_ptr;
    
    // Swap Red and Blue
    *pixel_ptr = make_float4(val.z, val.y, val.x, val.w);
}

} // namespace aetk::effect::draw

namespace aetk::effect {

extern "C" void cuda_swizzle_inplace(
    float* data,
    int pitch_bytes,
    int width,
    int height,
    void* stream
) {
    if (width <= 0 || height <= 0) return;
    
    int pitch_floats = pitch_bytes / sizeof(float);

    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);

    cudaStream_t custream = (cudaStream_t)stream;

    aetk::effect::draw::swizzle_inplace_kernel<<<grid, block, 0, custream>>>(
        data, pitch_floats,
        width, height
    );
}

} // namespace aetk::effect
