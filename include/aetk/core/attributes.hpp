#pragma once

#include <aetk/effect/pixel/device_kind.hpp>

// =====================================================================
//  AETK Development Status Attributes
// =====================================================================

/**
 * @brief Attribute indicating that a feature has been fully tested.
 */
#ifndef AETK_TESTED
#define AETK_TESTED [[maybe_unused]]
#endif

/**
 * @brief Attribute indicating that a feature is untested and should be verified.
 */
#ifndef AETK_UNTESTED
#define AETK_UNTESTED [[maybe_unused]]
#endif

/**
 * @brief Attribute indicating that a feature is under active development.
 */
#ifndef AETK_IN_PROGRESS
#define AETK_IN_PROGRESS [[maybe_unused]]
#endif

/**
 * @brief Attribute marking deprecated interfaces to emit compiler warnings.
 */
#ifndef AETK_DEPRECATED
#define AETK_DEPRECATED [[deprecated]]
#endif

// =====================================================================
//  AETK Device safety & occupancy constraints
// =====================================================================

namespace aetk::core {

/**
 * @brief Helper traits to determine residency characteristics of a device kind.
 * 
 * @tparam Dev The device residency model.
 */
template <aetk::effect::device_kind Dev>
struct device_traits {
    /// True if the device represents CPU-backed storage.
    static constexpr bool is_cpu = (Dev == aetk::effect::device_kind::cpu || Dev == aetk::effect::device_kind::cpu_pinned);
    /// True if the device represents GPU-accelerated storage (CUDA, Metal, D3D12, OpenCL).
    static constexpr bool is_gpu = (Dev == aetk::effect::device_kind::cuda || Dev == aetk::effect::device_kind::metal || Dev == aetk::effect::device_kind::d3d12 || Dev == aetk::effect::device_kind::opencl);
};

// C++20 concepts to check device location
#if __cplusplus >= 202002L
/**
 * @brief Constraint enforcing that a device residency model is CPU-backed.
 */
template <aetk::effect::device_kind Dev>
concept CPUResident = device_traits<Dev>::is_cpu;

/**
 * @brief Constraint enforcing that a device residency model is GPU-accelerated.
 */
template <aetk::effect::device_kind Dev>
concept GPUResident = device_traits<Dev>::is_gpu;
#endif

} // namespace aetk::core
