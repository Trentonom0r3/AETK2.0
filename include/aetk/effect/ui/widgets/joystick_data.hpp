#pragma once

namespace aetk::effect::ui {

/**
 * @brief Normalized 2D coordinate container (-1.0 to 1.0) for joysticks.
 *
 * @note <b>AE SDK Paradigm Shift:</b> 2D joystick coordinate state container.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct joystick_data {
    /// Normalized horizontal coordinate (-1.0 to 1.0).
    float x = 0.0f;
    
    /// Normalized vertical coordinate (-1.0 to 1.0).
    float y = 0.0f;

    /**
     * @brief Comparison operator.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Coordinates comparison.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param other Target coordinate to compare.
     * @return True if equal.
     */
    bool operator==(const joystick_data& other) const {
        return x == other.x && y == other.y;
    }

    /**
     * @brief Inequality comparison operator.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Coordinates comparison.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param other Target coordinate to compare.
     * @return True if unequal.
     */
    bool operator!=(const joystick_data& other) const {
        return !(*this == other);
    }
};

} // namespace aetk::effect::ui

// ══════════════════════════════════════════════════════════════════════
//  arb_traits specialization
// ══════════════════════════════════════════════════════════════════════

#include <aetk/core/locale_utils.hpp>
#include <aetk/effect/params/arb_traits.hpp>
#include <cstdio>
#include <cstring>

namespace aetk::effect {

/**
 * @brief Arbitrary traits specialization for joystick data structures.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, tracking active two-dimensional floating-point coordinates (joysticks) requires mapping separate horizontal/vertical parameters and syncing them step-by-step. `aetk::effect::ui::joystick_data` unifies 2D coordinates into a single serializable sequence structure, featuring automated binary flat copies and smooth double precision interpolation across keyframe steps.
 *
 * @warning <b>Memory & Lifecycles:</b> Placement-new allocations and destructors must be handled correctly in all methods. Byte sizes must exactly match structural capacities (`sizeof(T)`) to avoid corrupting sequence blocks.
 */
template <>
struct arb_traits<ui::joystick_data> {
    using T = ui::joystick_data;

    /**
     * @brief Initialize arbitrary data structure.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Placement-new initialization.
     *
     * @warning <b>Memory & Lifecycles:</b> Executes placement-new into host allocations.
     *
     * @param ptr Target memory address.
     */
    static void init(T* ptr) { new (ptr) T(); }

    /**
     * @brief Dispose arbitrary data allocations.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automated destructor call.
     *
     * @warning <b>Memory & Lifecycles:</b> Invokes standard destructor.
     *
     * @param ptr Target memory address.
     */
    static void dispose(T* ptr) { ptr->~T(); }

    /**
     * @brief Copy arbitrary data allocations.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automated copy allocation.
     *
     * @warning <b>Memory & Lifecycles:</b> Performs placement-new copy constructor.
     *
     * @param dst Destination address.
     * @param src Source address.
     */
    static void copy(T* dst, const T* src) { new (dst) T(*src); }

    /**
     * @brief Measures flat buffer size needed for serialization.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Size evaluation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ptr Data pointer.
     * @return Bounded buffer size in bytes.
     */
    static size_t flat_size(const T* ptr) {
        return sizeof(T);
    }

    /**
     * @brief Flatten structures into a binary buffer.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Fast byte copy flattening.
     *
     * @warning <b>Memory & Lifecycles:</b> Safe buffer sizes must equal sizeof(T).
     *
     * @param ptr Data pointer.
     * @param buffer Bounded target buffer.
     * @param size Target size.
     */
    static void flatten(const T* ptr, void* buffer, size_t size) {
        if (size >= sizeof(T)) {
            std::memcpy(buffer, ptr, sizeof(T));
        }
    }

    /**
     * @brief Unflatten binary buffer back into struct fields.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Fast byte copy unflattening.
     *
     * @warning <b>Memory & Lifecycles:</b> Safe buffer sizes must equal sizeof(T).
     *
     * @param ptr Data pointer.
     * @param buffer Bounded source buffer.
     * @param size Source size.
     */
    static void unflatten(T* ptr, const void* buffer, size_t size) {
        if (size >= sizeof(T)) {
            std::memcpy(ptr, buffer, sizeof(T));
        }
    }

    /**
     * @brief Performs standard 2D linear coordinate interpolation.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Linear coordinate interpolation.
     *
     * @warning <b>Memory & Lifecycles:</b> Standard placement-new allocation.
     *
     * @param dst Interpolated target.
     * @param left Source left.
     * @param right Source right.
     * @param t Temporal fraction value.
     */
    static void interpolate(T* dst, const T* left, const T* right, double t) {
        new (dst) T();
        dst->x = left->x + static_cast<float>(t * (right->x - left->x));
        dst->y = left->y + static_cast<float>(t * (right->y - left->y));
    }

    /**
     * @brief Print debugging coordinate values to host logger.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Debugging string printing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ptr Data pointer.
     * @param str Bounded string character target.
     * @param max_len Max string capacity.
     */
    static void print(const T* ptr, char* str, size_t max_len) {
        if (max_len > 0) {
            aetk::core::c_snprintf(str, max_len, "[X: %.2f, Y: %.2f]", ptr->x, ptr->y);
        }
    }

    static size_t print_size(const T* ptr) {
        return 128;
    }

    static bool compare(const T* a, const T* b) {
        return *a == *b;
    }

    static bool scan(T* ptr, const char* str) {
        float x_val = 0.0f, y_val = 0.0f;
        if (aetk::core::c_sscanf(str, "[X: %f, Y: %f]", &x_val, &y_val) == 2) {
            ptr->x = x_val;
            ptr->y = y_val;
            return true;
        }
        return false;
    }
};

} // namespace aetk::effect
