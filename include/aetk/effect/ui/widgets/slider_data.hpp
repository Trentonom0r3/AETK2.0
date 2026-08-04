#pragma once

#include <aetk/core/locale_utils.hpp>
#include <aetk/effect/params/arb_traits.hpp>
#include <aetk/effect/params/serialization.hpp>
#include <cmath>
#include <cstdio>

namespace aetk::effect::ui {

// ══════════════════════════════════════════════════════════════════════
//  Slider Data — serializable state for custom sliders
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Serializable state for custom sliders.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Custom slider state container.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 *
 * @tparam T Templated scalar value type.
 */
template <typename T>
struct slider_data {
    /// Slider scalar value.
    T value = T(0);

    /**
     * @brief Serializes the slider value.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Structured streaming.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @tparam Archive Streaming archive type.
     * @param ar Archive reference.
     */
    template <typename Archive>
    void serialize(Archive& ar) {
        ar & value;
    }

    bool operator==(const slider_data& other) const {
        return value == other.value;
    }

    bool operator!=(const slider_data& other) const {
        return !(*this == other);
    }
};

} // namespace aetk::effect::ui

// ══════════════════════════════════════════════════════════════════════
//  arb_traits specialization
// ══════════════════════════════════════════════════════════════════════

namespace aetk::effect {

/**
 * @brief Arbitrary traits specialization for slider data structures.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, custom sliders must be procedural and serialize their state step-by-step using separate floating-point or integer param types. `aetk::effect::ui::slider_data` unifies templated slider states directly into a type-safe serializable container. Specialized `arb_traits` automate interpolation, automatically selecting double-precision floating point interpolation or rounding methods for integer values.
 *
 * @warning <b>Memory & Lifecycles:</b> Placement-new allocations and destructors must be handled correctly in all methods.
 *
 * @tparam U Bounded scalar type template.
 */
template <typename U>
struct arb_traits<ui::slider_data<U>> {
    using T = ui::slider_data<U>;

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
        serialization::size_archive ar;
        const_cast<T*>(ptr)->serialize(ar);
        return ar.size();
    }

    /**
     * @brief Flatten structures into a binary buffer.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Structured flattening.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ptr Data pointer.
     * @param buffer Bounded target buffer.
     * @param size Target size.
     */
    static void flatten(const T* ptr, void* buffer, size_t size) {
        serialization::binary_oarchive ar(buffer, size);
        const_cast<T*>(ptr)->serialize(ar);
    }

    /**
     * @brief Unflatten binary buffer back into struct fields.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Structured unflattening.
     *
     * @warning <b>Memory & Lifecycles:</b> Standard placement-new allocation.
     *
     * @param ptr Data pointer.
     * @param buffer Bounded source buffer.
     * @param size Source size.
     */
    static void unflatten(T* ptr, const void* buffer, size_t size) {
        serialization::binary_iarchive ar(buffer, size);
        new (ptr) T();
        ptr->serialize(ar);
    }

    /**
     * @brief Performs standard scalar interpolation.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Interpolate sliding values using rounded snapping for integers.
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
        if constexpr (std::is_floating_point_v<U>) {
            dst->value = left->value + static_cast<U>(t * (right->value - left->value));
        } else {
            // For integers, round the interpolation
            double interp = static_cast<double>(left->value) + t * static_cast<double>(right->value - left->value);
            dst->value = static_cast<U>(std::round(interp));
        }
    }

    /**
     * @brief Print debugging scalar values to host logger.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Print floating point or integer string descriptions.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ptr Data pointer.
     * @param str Bounded string character target.
     * @param max_len Max string capacity.
     */
    static void print(const T* ptr, char* str, size_t max_len) {
        if (max_len > 0) {
            if constexpr (std::is_floating_point_v<U>) {
                aetk::core::c_snprintf(str, max_len, "%.2f", static_cast<double>(ptr->value));
            } else {
                aetk::core::c_snprintf(str, max_len, "%d", static_cast<int>(ptr->value));
            }
        }
    }

    static size_t print_size(const T* ptr) {
        return 128;
    }

    static bool compare(const T* a, const T* b) {
        return *a == *b;
    }

    static bool scan(T* ptr, const char* str) {
        if constexpr (std::is_floating_point_v<U>) {
            double val = 0.0;
            if (aetk::core::c_sscanf(str, "%lf", &val) == 1) {
                ptr->value = static_cast<U>(val);
                return true;
            }
        } else {
            int val = 0;
            if (aetk::core::c_sscanf(str, "%d", &val) == 1) {
                ptr->value = static_cast<U>(val);
                return true;
            }
        }
        return false;
    }
};

} // namespace aetk::effect
