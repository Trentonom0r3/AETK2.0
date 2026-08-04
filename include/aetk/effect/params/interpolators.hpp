#pragma once

#include <cmath>

/**
 * @brief Namespace containing standard interpolation utilities for arbitrary data types.
 */
namespace aetk::effect::interpolators {

/**
 * @brief Interpolates basic numerical structs by linearly blending each member.
 * Requirements: The struct must support scalar addition and scalar multiplication.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, custom parameters like arbitrary data (e.g. keyframes) require custom registration of interpolation functions with function pointers. `aetk::effect::interpolators` provides type-safe, compile-time templates that make it simple to blend structures or hold states during timeline frame rendering.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 *
 * @tparam T Numerical structure type supporting standard addition and subtraction operators.
 * @param dst Output destination pointer.
 * @param left Left keyframe boundary value.
 * @param right Right keyframe boundary value.
 * @param t Interpolation progress factor [0.0, 1.0].
 */
template <typename T>
inline void linear(T* dst, const T* left, const T* right, double t) {
    *dst = (*left) + ((*right) - (*left)) * static_cast<float>(t);
}

/**
 * @brief Interpolates two floats linearly.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, custom parameters like arbitrary data (e.g. keyframes) require custom registration of interpolation functions with function pointers. `aetk::effect::interpolators` provides type-safe, compile-time templates that make it simple to blend structures or hold states during timeline frame rendering.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 *
 * @param dst Output destination pointer.
 * @param left Left boundary float value.
 * @param right Right boundary float value.
 * @param t Interpolation progress factor [0.0, 1.0].
 */
inline void linear_float(float* dst, const float* left, const float* right, double t) {
    *dst = *left + static_cast<float>((*right - *left) * t);
}

/**
 * @brief Default interpolator that simply holds the left value until t > 0.5.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, custom parameters like arbitrary data (e.g. keyframes) require custom registration of interpolation functions with function pointers. `aetk::effect::interpolators` provides type-safe, compile-time templates that make it simple to blend structures or hold states during timeline frame rendering.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 *
 * @tparam T Custom arbitrary parameter structure type.
 * @param dst Output destination pointer.
 * @param left Left boundary value.
 * @param right Right boundary value.
 * @param t Interpolation progress factor [0.0, 1.0].
 */
template <typename T>
inline void hold(T* dst, const T* left, const T* right, double t) {
    if (t < 0.5) {
        *dst = *left;
    } else {
        *dst = *right;
    }
}

} // namespace aetk::effect::interpolators
