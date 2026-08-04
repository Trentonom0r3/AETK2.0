#pragma once

/**
 * @file swizzle.h
 * @brief Zero-Pass GPU swizzling for AETK.
 * 
 * @details Include this in your .cu or .metal kernels to handle After Effects' 
 * pixel layout on-the-fly without extra passes.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, After Effects GPU frames (specifically `PF_PixelFormat_GPU_BGRA128`) are stored in BGRA channel order, whereas modern graphic kernels, ML models, and pipelines require standard RGBA textures. Querying or rewriting these channels typically requires an extra copy pass. `aetk_bgra_to_rgba` and `aetk_rgba_to_bgra` resolve this by providing zero-overhead, single-instruction register swizzles that execute on-the-fly inside CUDA or Metal kernels.
 *
 * @warning <b>Memory & Lifecycles:</b> None. These are low-overhead inline register math operations.
 */

#ifdef __CUDACC__

/**
 * @brief Convert BGRA (AE GPU Native) to RGBA during register load.
 * 
 * After Effects GPU worlds (PF_PixelFormat_GPU_BGRA128) store 32-bit floats
 * in the order B, G, R, A. This function swaps B and R.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Zero-overhead register-level swizzle transaction.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 *
 * @param bgra Raw BGRA register value.
 * @return Swizzled RGBA register float4.
 */
__device__ __forceinline__ float4 aetk_bgra_to_rgba(float4 bgra) {
    return make_float4(bgra.z, bgra.y, bgra.x, bgra.w);
}

/**
 * @brief Convert RGBA to BGRA (AE GPU Native) before register store.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Zero-overhead register-level swizzle transaction.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 *
 * @param rgba Raw RGBA register value.
 * @return Swizzled BGRA register float4.
 */
__device__ __forceinline__ float4 aetk_rgba_to_bgra(float4 rgba) {
    return make_float4(rgba.z, rgba.y, rgba.x, rgba.w);
}

#endif // __CUDACC__

#if defined(AETK_ENABLE_CUDA) || defined(AETK_CUDA_SUPPORT) || defined(__CUDACC__)
#include <cuda_runtime.h>
#endif

namespace aetk::effect {

#if defined(AETK_ENABLE_CUDA) || defined(AETK_CUDA_SUPPORT) || defined(__CUDACC__)
extern "C" void cuda_swizzle_inplace(
    float* data,
    int pitch_bytes,
    int width,
    int height,
    void* stream = nullptr
);
#endif

} // namespace aetk::effect

#ifdef __METAL_VERSION__

/**
 * @brief Convert BGRA (AE GPU Native) to RGBA.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Zero-overhead register-level swizzle transaction.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 *
 * @param bgra Raw BGRA register value.
 * @return Swizzled RGBA register float4.
 */
inline float4 aetk_bgra_to_rgba(float4 bgra) {
    return bgra.zyxw;
}

#endif
