#pragma once
#include <aetk/core/types.hpp>
#include <AE_Effect.h>
#include <initializer_list>
#include <algorithm>

namespace aetk::core {

/**
 * @brief High-level wrapper for a 3x3 convolution kernel.
 *
 * @details Represents a 2D floating-point convolution matrix for image filter math operations.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Replaces raw, uninitialized 1D arrays or hardcoded matrices in classical 3x3 convolution effects with a structured, C++ `initializer_list` construct.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct kernel_3x3 {
    /// Internal raw 3x3 matrix storage.
    float data[3][3];

    /**
     * @brief Null constructor.
     *
     * @details Initializes all values to zero.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safer default values.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    kernel_3x3() {
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 3; ++x)
                data[y][x] = 0.0f;
    }

    /**
     * @brief Initializer list constructor.
     *
     * @details Binds fractional floating point coordinates to matrix locations.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Allows clean, declarative initialization syntax.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param list Standard nested initializer list values.
     */
    kernel_3x3(std::initializer_list<std::initializer_list<float>> list) {
        int y = 0;
        for (auto row : list) {
            if (y < 3) {
                int x = 0;
                for (auto val : row) {
                    if (x < 3) data[y][x] = val;
                    x++;
                }
            }
            y++;
        }
    }

    /**
     * @brief Convert kernel to AE's fixed-point format (A_long).
     *
     * @details Scales and casts fractional values to standard fixed-point representation format.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simplifies manual integer conversion routines.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param out Target 9-element raw output buffer.
     * @param unity_scale The value representing 1.0 (defaults to 16.16 fixed point: 65536).
     */
    void to_ae_fixed(A_long out[9], float unity_scale = 65536.0f) const {
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                out[y * 3 + x] = static_cast<A_long>(data[y][x] * unity_scale);
            }
        }
    }
};

} // namespace aetk::core
