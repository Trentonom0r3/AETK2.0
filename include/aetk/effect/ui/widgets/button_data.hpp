#pragma once

#include <aetk/effect/params/arb_traits.hpp>
#include <aetk/effect/params/serialization.hpp>
#include <cstdio>

namespace aetk::effect::ui {

// ══════════════════════════════════════════════════════════════════════
//  Button Data — serializable state for toggle buttons
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Serializable state for toggle buttons.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Custom button state container.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct button_data {
    /// Active state status.
    bool active = false;

    /**
     * @brief Serializes the active status flag.
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
        ar & active;
    }

    bool operator==(const button_data& other) const {
        return active == other.active;
    }

    bool operator!=(const button_data& other) const {
        return !(*this == other);
    }
};

} // namespace aetk::effect::ui

// ══════════════════════════════════════════════════════════════════════
//  arb_traits specialization
// ══════════════════════════════════════════════════════════════════════

namespace aetk::effect {

/**
 * @brief Arbitrary traits specialization for button data structures.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, tracking state parameters for custom buttons (such as dynamic press toggles or image modes) requires custom parameter structures and manual byte flattening. `aetk::effect::ui::button_data` integrates custom button active states directly with `arb_traits`, providing automated binary streaming and snap-interpolation at temporal midpoints.
 *
 * @warning <b>Memory & Lifecycles:</b> Placement-new allocations and destructors must be handled correctly in all methods.
 */
template <>
struct arb_traits<ui::button_data> {
    using T = ui::button_data;

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
     * @note <b>AE SDK Paradigm Shift:</b> Streaming size calculation.
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
     * @note <b>AE SDK Paradigm Shift:</b> Automated stream flattening.
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
     * @note <b>AE SDK Paradigm Shift:</b> Placement-new stream unflattening.
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
     * @brief Performs snap-interpolation between button states.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Boolean snapping interpolation.
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
        // Boolean values snap at the halfway point
        *dst = (t < 0.5) ? *left : *right;
    }

    /**
     * @brief Print debugging string to host logger.
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
            snprintf(str, max_len, "%s", ptr->active ? "On" : "Off");
        }
    }

    static size_t print_size(const T* ptr) {
        return 128;
    }

    static bool compare(const T* a, const T* b) {
        return *a == *b;
    }

    static bool scan(T* ptr, const char* str) {
        if (std::strcmp(str, "On") == 0 || std::strcmp(str, "on") == 0 || std::strcmp(str, "1") == 0 || std::strcmp(str, "true") == 0 || std::strcmp(str, "True") == 0) {
            ptr->active = true;
            return true;
        } else if (std::strcmp(str, "Off") == 0 || std::strcmp(str, "off") == 0 || std::strcmp(str, "0") == 0 || std::strcmp(str, "false") == 0 || std::strcmp(str, "False") == 0) {
            ptr->active = false;
            return true;
        }
        return false;
    }
};

} // namespace aetk::effect
