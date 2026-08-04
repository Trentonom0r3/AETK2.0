#pragma once

#include <aetk/effect/params/serialization.hpp>
#include <cstring>
#include <vector>

namespace aetk::effect {

/**
 * @brief Traits class to handle arbitrary data serialization and lifecycle.
 * 
 * @details Users can specialize this struct for their custom types to override 
 * default behavior. The default implementation uses the serialization archive.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, custom parameters (Arbitrary Data) require registering callback functions for memory allocation, copy constructors, flattening (saving to project file), unflattening (loading from project file), frame interpolation, and UI text descriptions. `aetk::effect::arb_traits` consolidates these procedural callbacks into a single type-safe C++ traits structure. By utilizing template specialization, developers can customize lifecycle and serialization behaviors for custom types seamlessly.
 *
 * @warning <b>Memory & Lifecycles:</b> Traits manage the instantiation and destruction of complex arbitrary types. For dynamic allocation stability, `init` and `copy` perform placement-new constructions directly inside memory pools pre-allocated by the AE host.
 *
 * @tparam T The custom arbitrary data type.
 */
template <typename T>
struct arb_traits {
    /**
     * @brief Constructs a new instance.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw C pointer zero-initialization with clean C++ placement-new.
     *
     * @warning <b>Memory & Lifecycles:</b> Invokes the default constructor of `T` using placement-new on pre-allocated host memory.
     *
     * @param ptr Target allocation pointer.
     */
    static void init(T* ptr) {
        new (ptr) T();
    }

    /**
     * @brief Destroys the instance.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Custom destructor invocation.
     *
     * @warning <b>Memory & Lifecycles:</b> Manually invokes the destructor `~T()` without releasing the host-managed buffer.
     *
     * @param ptr Target pointer.
     */
    static void dispose(T* ptr) {
        ptr->~T();
    }

    /**
     * @brief Clones the instance.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Type-safe copy.
     *
     * @warning <b>Memory & Lifecycles:</b> Placement-new copy constructor.
     *
     * @param dst Destination pointer.
     * @param src Source instance pointer.
     */
    static void copy(T* dst, const T* src) {
        new (dst) T(*src);
    }

    /**
     * @brief Calculate binary size of serialized instance.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Streamlines sizing math.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ptr Target instance.
     * @return Serialized size in bytes.
     */
    static size_t flat_size(const T* ptr) {
        serialization::size_archive ar;
        ar & *ptr;
        return ar.size();
    }

    /**
     * @brief Serialize state to binary stream.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw byte copy macros with structured serialization.
     *
     * @warning <b>Memory & Lifecycles:</b> Writes into pre-allocated host memory buffer.
     *
     * @param ptr Target instance to serialize.
     * @param buffer Output host buffer.
     * @param size Maximum buffer size.
     */
    static void flatten(const T* ptr, void* buffer, size_t size) {
        serialization::binary_oarchive ar(buffer, size);
        ar & *ptr;
    }

    /**
     * @brief Deserialize state from binary stream.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Placement-new followed by stream decoding.
     *
     * @warning <b>Memory & Lifecycles:</b> Constructs type `T` on host buffer before restoring fields.
     *
     * @param ptr Destination pointer.
     * @param buffer Input host buffer.
     * @param size Buffer size.
     */
    static void unflatten(T* ptr, const void* buffer, size_t size) {
        serialization::binary_iarchive ar(buffer, size);
        new (ptr) T(); // Construct
        ar & *ptr;     // Then read into it
    }

    /**
     * @brief Interpolate between two keyframes.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard time interpolation step.
     *
     * @warning <b>Memory & Lifecycles:</b> Placement-new construction.
     *
     * @param dst Output destination.
     * @param left Left boundary value.
     * @param right Right boundary value.
     * @param t Interpolation progress [0.0, 1.0].
     */
    static void interpolate(T* dst, const T* left, const T* right, double t) {
        // By default, no smooth interpolation; just snap to nearest or left.
        // A user could specialize this if their type supports blending.
        if (t < 0.5) {
            new (dst) T(*left);
        } else {
            new (dst) T(*right);
        }
    }

    /**
     * @brief String description for AE UI.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe copy to UI string buffer.
     *
     * @warning <b>Memory & Lifecycles:</b> Safe bounded copy.
     *
     * @param ptr Target instance.
     * @param str Destination string buffer.
     * @param max_len Maximum length of destination buffer.
     */
    static void print(const T* ptr, char* str, size_t max_len) {
        if (max_len > 0) {
            std::strncpy(str, "Custom UI", max_len);
            str[max_len - 1] = '\0';
        }
    }

    /**
     * @brief Compare two instances for equality.
     * 
     * @param a Pointer to first instance.
     * @param b Pointer to second instance.
     * @return True if equal, false otherwise.
     */
    static bool compare(const T* a, const T* b) {
        size_t size_a = flat_size(a);
        size_t size_b = flat_size(b);
        if (size_a != size_b) {
            return false;
        }
        std::vector<char> buf_a(size_a);
        std::vector<char> buf_b(size_b);
        flatten(a, buf_a.data(), size_a);
        flatten(b, buf_b.data(), size_b);
        return std::memcmp(buf_a.data(), buf_b.data(), size_a) == 0;
    }

    /**
     * @brief Reports the print size required for text serialization.
     * 
     * @param ptr Target pointer.
     * @return Required buffer size in bytes.
     */
    static size_t print_size(const T* ptr) {
        return 256;
    }

    /**
     * @brief Parses a text representation back into the instance.
     * 
     * @param ptr Target pointer.
     * @param str Null-terminated string representation.
     * @return True if successful, false otherwise.
     */
    static bool scan(T* ptr, const char* str) {
        return false;
    }
};

} // namespace aetk::effect
