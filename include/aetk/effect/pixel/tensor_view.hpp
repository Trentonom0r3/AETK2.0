#pragma once

#ifndef __CUDACC__
#include <AE_Effect.h>
#include <AE_EffectPixelFormat.h>
#include <AE_EffectGPUSuites.h>
#include <aetk/core/error.hpp>
#include <aetk/core/suite.hpp>
#endif

#include <array>
#include <cstring>
#include <stdexcept>
#include <algorithm>

#if defined(AETK_ENABLE_CUDA) || defined(AETK_CUDA_SUPPORT) || defined(__CUDACC__)
#include <cuda_runtime.h>
#endif

#include <aetk/effect/pixel/device_kind.hpp>

namespace aetk::effect {

#ifndef __CUDACC__
class context;
class smart_world;
#endif

/**
 * @brief A lightweight, non-owning multidimensional tensor view, mirroring std::mdspan.
 *
 * @details Wraps raw pixel buffers (like After Effects' `PF_EffectWorld` or GPU/CUDA buffers)
 * into a multi-dimensional array mapping. For uncompressed pixel formats, the tensor maps
 * a Rank-3 shape of `[Height, Width, 4]` (Alpha, Red, Green, Blue interleaved).
 *
 * ### ⚠️ Crucial Data Layout & Value Scaling Rules:
 *
 * Depending on the backing `PF_PixelFormat` and host application (Premiere Pro vs. After Effects),
 * the template parameters and values represent:
 *
 * 1. **8bpc integer formats (`PF_PixelFormat_ARGB32`, `PF_PixelFormat_BGRA32`)**:
 *    - Use **`T = A_u_char`** (equivalent to `unsigned char`).
 *    - **Value range**: `0` to `255` (where `255` is white).
 *    - **Channel mapping**:
 *      - ARGB: `view(y, x, 0)` is Alpha, `1` is Red, `2` is Green, `3` is Blue.
 *      - BGRA: `view(y, x, 0)` is Blue, `1` is Green, `2` is Red, `3` is Alpha.
 *
 * 2. **16bpc integer formats (`PF_PixelFormat_ARGB64`)**:
 *    - Use **`T = A_u_short`** (equivalent to `unsigned short`).
 *    - **Value range**: `0` to `32768` (where `32768` is white, leaving headroom up to `65535`).
 *    - **Channel mapping**: `view(y, x, 0)` is Alpha, `1` is Red, `2` is Green, `3` is Blue.
 *
 * 3. **32bpc float formats (`PF_PixelFormat_ARGB128`, `PF_PixelFormat_GPU_BGRA128`)**:
 *    - Use **`T = float`**.
 *    - **Value range**: `0.0f` to `1.0f` (where `1.0f` is white). Values can exceed `1.0f` (HDR).
 *    - **Channel mapping**:
 *      - ARGB: `view(y, x, 0)` is Alpha, `1` is Red, `2` is Green, `3` is Blue.
 *      - BGRA: `view(y, x, 0)` is Blue, `1` is Green, `2` is Red, `3` is Alpha.
 *
 * @tparam T Channel data type (e.g. float, A_u_char, A_u_short).
 * @tparam Rank Multi-dimensional rank (dimension count, typically 3).
 * @tparam Dev Target device residency (cpu or cuda).
 */
template <typename T, size_t Rank, device_kind Dev = device_kind::cpu>
class tensor_view {
    T* m_data = nullptr;
    size_t m_shape[Rank > 0 ? Rank : 1]{};
    ptrdiff_t m_strides[Rank > 0 ? Rank : 1]{};

public:
    #if defined(__CUDACC__)
    __host__ __device__
    #endif
    tensor_view() : m_data(nullptr) {
        for (size_t i = 0; i < (Rank > 0 ? Rank : 1); ++i) {
            m_shape[i] = 0;
            m_strides[i] = 0;
        }
    }

    #if defined(__CUDACC__)
    __host__ __device__
    #endif
    tensor_view(T* data, const size_t* shape, const ptrdiff_t* strides)
        : m_data(data) {
        for (size_t i = 0; i < Rank; ++i) {
            m_shape[i] = shape[i];
            m_strides[i] = strides[i];
        }
    }

    /**
     * @brief Indexing operator for Rank-3 tensors (e.g. [y, x, c] or [c, y, x]).
     */
    #if defined(__CUDACC__)
    __host__ __device__
    #endif
    inline T& operator()(size_t i0, size_t i1, size_t i2) const {
        static_assert(Rank == 3, "Rank-3 indexing operator requires Rank == 3");
        return m_data[i0 * m_strides[0] + i1 * m_strides[1] + i2 * m_strides[2]];
    }

    /**
     * @brief Indexing operator for Rank-2 tensors (e.g. [y, x] or grayscale layouts).
     */
    #if defined(__CUDACC__)
    __host__ __device__
    #endif
    inline T& operator()(size_t i0, size_t i1) const {
        static_assert(Rank == 2, "Rank-2 indexing operator requires Rank == 2");
        return m_data[i0 * m_strides[0] + i1 * m_strides[1]];
    }

    /**
     * @brief Access the raw data pointer.
     */
    #if defined(__CUDACC__)
    __host__ __device__
    #endif
    T* data_ptr() const { return m_data; }

    /**
     * @brief Retrieve dimension size.
     */
    #if defined(__CUDACC__)
    __host__ __device__
    #endif
    size_t shape(size_t dim) const { return m_shape[dim]; }

    /**
     * @brief Retrieve dimension stride.
     */
    #if defined(__CUDACC__)
    __host__ __device__
    #endif
    ptrdiff_t stride(size_t dim) const { return m_strides[dim]; }

    /**
     * @brief Retrieve complete shape array.
     */
    #if defined(__CUDACC__)
    __host__ __device__
    #endif
    const size_t* shape() const { return m_shape; }

    /**
     * @brief Retrieve complete strides array.
     */
    #if defined(__CUDACC__)
    __host__ __device__
    #endif
    const ptrdiff_t* strides() const { return m_strides; }
};

#ifndef __CUDACC__
/**
 * @brief Owning multidimensional tensor container, managing buffer allocation lifecycles.
 *
 * @details Allocates and owns memory through After Effects' host allocation APIs
 * (integrates with tracking via SPBasicSuite). Maps the allocated memory layout
 * to a multi-dimensional array mapping.
 *
 * ### ⚠️ Crucial Data Layout & Value Scaling Rules:
 *
 * Depending on the pixel format and host, channels are mapped as:
 * - **8bpc (`T = A_u_char`)**: Value range is `0` to `255` (where `255` is white).
 * - **16bpc (`T = A_u_short`)**: Value range is `0` to `32768` (where `32768` is white).
 * - **32bpc (`T = float`)**: Value range is `0.0f` to `1.0f` (where `1.0f` is white).
 *
 * @tparam T Channel data type (e.g. float, A_u_char, A_u_short).
 * @tparam Rank Multi-dimensional rank (dimension count).
 * @tparam Dev Target device residency.
 */
template <typename T, size_t Rank, device_kind Dev = device_kind::cpu>
class tensor {
    T* m_data = nullptr;
    std::array<size_t, Rank> m_shape{};
    std::array<ptrdiff_t, Rank> m_strides{};

    smart_world m_storage;

    // Raw block allocation details if this tensor allocated raw memory directly
    PF_ProgPtr m_effect_ref = nullptr;
    size_t m_raw_size = 0;
    int m_device_index = 0;
    bool m_avoid_lock = false;
    bool m_owned = false;

    void allocate(PF_ProgPtr effect_ref, size_t bytes, int device_index, bool avoid_lock);
    void cleanup();

    template <typename U, size_t R, device_kind D>
    friend class tensor;

public:
    /**
     * @brief Destructor.
     */
    ~tensor();

    /**
     * @brief Construct a tensor around a pre-allocated smart_world with a specific shape.
     */
    tensor(std::array<size_t, Rank> shape, smart_world&& storage);

    /**
     * @brief Construct an owning CPU tensor without a context.
     * Only available when Dev is device_kind::cpu.
     */
    tensor(std::array<size_t, Rank> shape);

    /**
     * @brief Construct an owning tensor and allocate its backing memory using AE GPUDeviceSuite.
     * 
     * @param shape Shape dimensions in C-order.
     * @param ctx   AETK context wrapper.
     * @param kind  Target device kind (defaults to Dev). The device index is auto-detected
     *              using the GPU device suite based on this device kind.
     */
    tensor(std::array<size_t, Rank> shape, const context& ctx, device_kind kind = Dev, bool avoid_lock = false);

    /**
     * @brief Construct an owning tensor and allocate its backing memory using AE GPUDeviceSuite.
     * 
     * @param shape Shape dimensions in C-order.
     * @param effect_ref   AE effect reference.
     * @param kind  Target device kind (defaults to Dev). The device index is auto-detected
     *              using the GPU device suite based on this device kind.
     */
    tensor(std::array<size_t, Rank> shape, PF_ProgPtr effect_ref, device_kind kind = Dev, bool avoid_lock = false);

    /**
     * @brief Construct a tensor around a pre-existing smart_world.
     * Deduces Rank 1, 2, or 3 shapes/strides based on dimensions of smart_world.
     */
    explicit tensor(smart_world&& storage);

    /**
     * @brief Construct a tensor around an existing raw data pointer.
     */
    tensor(std::array<size_t, Rank> shape, void* data, size_t bytes, bool own = false);

    // Move-only container
    tensor(tensor&& other) noexcept;
    tensor& operator=(tensor&& other) noexcept;

    tensor(const tensor&) = delete;
    tensor& operator=(const tensor&) = delete;

    /**
     * @brief Expose a lightweight non-owning view.
     */
    tensor_view<T, Rank, Dev> view() const {
        return tensor_view<T, Rank, Dev>(m_data, m_shape.data(), m_strides.data());
    }

    /**
     * @brief Retrieve raw backing pointer.
     */
    T* data_ptr() const { return m_data; }

    /**
     * @brief Retrieve dimension shape.
     */
    size_t shape(size_t dim) const { return m_shape[dim]; }

    /**
     * @brief Retrieve dimension stride.
     */
    ptrdiff_t stride(size_t dim) const { return m_strides[dim]; }

    /**
     * @brief Transfer tensor data to a target device framework using captured suites.
     * Only available if the tensor was created with a context.
     */
    template <device_kind TargetDev>
    tensor<T, Rank, TargetDev> to(device_kind kind = TargetDev) const;

    /**
     * @brief Transfer tensor data to a target device framework using a context.
     */
    template <device_kind TargetDev>
    tensor<T, Rank, TargetDev> to(const context& ctx, device_kind kind = TargetDev) const;

    // Declarations for AE world exporting
    smart_world to_world(const context& ctx, short bitdepth = 8) const;
    void copy_to(smart_world& dest) const;
};

// --------------------------------------------------------------------
// PyTorch-style Factory Methods
// --------------------------------------------------------------------

template <typename T, size_t Rank>
inline tensor<T, Rank, device_kind::cpu> zeros(std::array<size_t, Rank> shape) {
    return tensor<T, Rank, device_kind::cpu>(shape);
}

template <typename T, size_t Rank>
tensor<T, Rank, device_kind::cpu_pinned> zeros_pinned(std::array<size_t, Rank> shape, const context& ctx);
#endif

} // namespace aetk::effect
