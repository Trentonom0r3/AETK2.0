#pragma once

#include <vector>
#include <string>
#include <cstring>
#include <type_traits>
#include <stdexcept>

namespace aetk::effect::serialization {

// ══════════════════════════════════════════════════════════════════════
//  Type Traits & Helpers
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Check if T has a void serialize(Archive&) method.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Compile-time detection of serialization compatibility.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
template <typename T, typename Archive, typename = void>
struct has_serialize : std::false_type {};

template <typename T, typename Archive>
struct has_serialize<T, Archive, std::void_t<decltype(std::declval<T>().serialize(std::declval<Archive&>()))>> : std::true_type {};

// ══════════════════════════════════════════════════════════════════════
//  Size Archive (Measures byte size needed for flattening)
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Measures byte size needed for flattening arbitrary types.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, saving and restoring parameter state (like arbitrary data structures) requires direct buffer byte operations and manually calculating size constraints, which leads to memory buffer overflows or corrupt stream indices. `aetk::effect::serialization` introduces a modern, type-safe streaming interface using overloaded `operator&`. It automatically calculates size limits via `size_archive`, serializes structs to host bytes via `binary_oarchive`, and decodes binary streams via `binary_iarchive` without any complex pointer offsets.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
class size_archive {
    size_t m_size = 0;
public:
    /**
     * @brief Get calculated binary byte length.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Returns total calculated byte length.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Accumulated byte size.
     */
    size_t size() const { return m_size; }

    /**
     * @brief Accumulates the serialized size of the given value.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Handles trivial copyable values or dynamic serialize callbacks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @tparam T The value type.
     * @param val Reference to the value.
     * @return Reference to this archive.
     */
    template <typename T>
    size_archive& operator&(const T& val) {
        if constexpr (has_serialize<T, size_archive>::value) {
            const_cast<T&>(val).serialize(*this);
        } else if constexpr (std::is_trivially_copyable_v<T>) {
            m_size += sizeof(T);
        } else {
            static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable or implement serialize()");
        }
        return *this;
    }

    /**
     * @brief Accumulates the size of a std::vector.
     *
     * @tparam T Vector item type.
     * @param vec Reference to vector.
     * @return Reference to this archive.
     */
    template <typename T>
    size_archive& operator&(const std::vector<T>& vec) {
        m_size += sizeof(size_t); // vector size
        for (const auto& item : vec) {
            *this & item;
        }
        return *this;
    }

    /**
     * @brief Accumulates the size of a std::string.
     *
     * @param str Reference to string.
     * @return Reference to this archive.
     */
    size_archive& operator&(const std::string& str) {
        m_size += sizeof(size_t) + str.size();
        return *this;
    }
};

// ══════════════════════════════════════════════════════════════════════
//  Output Archive (Flattens data into a byte buffer)
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Serializes dynamic types into a raw byte buffer.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, saving and restoring parameter state (like arbitrary data structures) requires direct buffer byte operations and manually calculating size constraints, which leads to memory buffer overflows or corrupt stream indices. `aetk::effect::serialization` introduces a modern, type-safe streaming interface using overloaded `operator&`. It automatically calculates size limits via `size_archive`, serializes structs to host bytes via `binary_oarchive`, and decodes binary streams via `binary_iarchive` without any complex pointer offsets.
 *
 * @warning <b>Memory & Lifecycles:</b> The output archive operates directly on raw host memory buffers. It is critical that the buffer lifetime encompasses the lifetime of the serialization action. If the capacity is exceeded, an exception is thrown.
 */
class binary_oarchive {
    char* m_buffer;
    size_t m_capacity;
    size_t m_offset = 0;

public:
    /**
     * @brief Write archive constructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Binds write stream destinations.
     *
     * @warning <b>Memory & Lifecycles:</b> Safe buffer pointer wrapping.
     *
     * @param buffer Output raw buffer.
     * @param capacity Maximum capacity in bytes.
     */
    binary_oarchive(void* buffer, size_t capacity) 
        : m_buffer(static_cast<char*>(buffer)), m_capacity(capacity) {}

    /**
     * @brief Writes a value to the archive buffer.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Enforces type-safe serialization.
     *
     * @warning <b>Memory & Lifecycles:</b> Throws overflow exceptions if buffer limits are breached.
     *
     * @tparam T The value type.
     * @param val The value to write.
     * @return Reference to this archive.
     */
    template <typename T>
    binary_oarchive& operator&(const T& val) {
        if constexpr (has_serialize<T, binary_oarchive>::value) {
            const_cast<T&>(val).serialize(*this);
        } else if constexpr (std::is_trivially_copyable_v<T>) {
            write(&val, sizeof(T));
        } else {
            static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable or implement serialize()");
        }
        return *this;
    }

    /**
     * @brief Writes a std::vector to the archive.
     *
     * @tparam T Vector item type.
     * @param vec Vector reference.
     * @return Reference to this archive.
     */
    template <typename T>
    binary_oarchive& operator&(const std::vector<T>& vec) {
        size_t s = vec.size();
        write(&s, sizeof(size_t));
        for (const auto& item : vec) {
            *this & item;
        }
        return *this;
    }

    /**
     * @brief Writes a std::string to the archive.
     *
     * @param str String reference.
     * @return Reference to this archive.
     */
    binary_oarchive& operator&(const std::string& str) {
        size_t s = str.size();
        write(&s, sizeof(size_t));
        write(str.data(), s);
        return *this;
    }

private:
    void write(const void* data, size_t size) {
        if (m_offset + size > m_capacity) {
            throw std::runtime_error("binary_oarchive buffer overflow");
        }
        std::memcpy(m_buffer + m_offset, data, size);
        m_offset += size;
    }
};

// ══════════════════════════════════════════════════════════════════════
//  Input Archive (Unflattens data from a byte buffer)
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Deserializes dynamic types from a raw byte buffer.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, saving and restoring parameter state (like arbitrary data structures) requires direct buffer byte operations and manually calculating size constraints, which leads to memory buffer overflows or corrupt stream indices. `aetk::effect::serialization` introduces a modern, type-safe streaming interface using overloaded `operator&`. It automatically calculates size limits via `size_archive`, serializes structs to host bytes via `binary_oarchive`, and decodes binary streams via `binary_iarchive` without any complex pointer offsets.
 *
 * @warning <b>Memory & Lifecycles:</b> The input archive operates directly on raw host memory buffers. It is critical that the buffer lifetime encompasses the lifetime of the deserialization action. If the capacity is exceeded, an exception is thrown.
 */
class binary_iarchive {
    const char* m_buffer;
    size_t m_capacity;
    size_t m_offset = 0;

public:
    /**
     * @brief Read archive constructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Binds read stream sources.
     *
     * @warning <b>Memory & Lifecycles:</b> Safe buffer pointer wrapping.
     *
     * @param buffer Input raw buffer.
     * @param capacity Source capacity in bytes.
     */
    binary_iarchive(const void* buffer, size_t capacity) 
        : m_buffer(static_cast<const char*>(buffer)), m_capacity(capacity) {}

    /**
     * @brief Reads a value from the archive buffer.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Enforces type-safe deserialization.
     *
     * @warning <b>Memory & Lifecycles:</b> Throws overflow exceptions if buffer limits are breached.
     *
     * @tparam T The value type.
     * @param val The value to populate.
     * @return Reference to this archive.
     */
    template <typename T>
    binary_iarchive& operator&(T& val) {
        if constexpr (has_serialize<T, binary_iarchive>::value) {
            val.serialize(*this);
        } else if constexpr (std::is_trivially_copyable_v<T>) {
            read(&val, sizeof(T));
        } else {
            static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable or implement serialize()");
        }
        return *this;
    }

    /**
     * @brief Reads a std::vector from the archive.
     *
     * @tparam T Vector item type.
     * @param vec Vector reference.
     * @return Reference to this archive.
     */
    template <typename T>
    binary_iarchive& operator&(std::vector<T>& vec) {
        size_t s;
        read(&s, sizeof(size_t));
        vec.resize(s);
        for (auto& item : vec) {
            *this & item;
        }
        return *this;
    }

    /**
     * @brief Reads a std::string from the archive.
     *
     * @param str String reference.
     * @return Reference to this archive.
     */
    binary_iarchive& operator&(std::string& str) {
        size_t s;
        read(&s, sizeof(size_t));
        str.resize(s);
        read(str.data(), s);
        return *this;
    }

private:
    void read(void* data, size_t size) {
        if (m_offset + size > m_capacity) {
            throw std::runtime_error("binary_iarchive buffer overflow");
        }
        std::memcpy(data, m_buffer + m_offset, size);
        m_offset += size;
    }
};

} // namespace aetk::effect::serialization
