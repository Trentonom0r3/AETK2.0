#pragma once

#include <aetk/core/suite.hpp>
#include <string>

namespace aetk::core {

// ============================================================
//  MemorySuite — RAII wrapper for AEGP_MemHandle
//
//  Many SDK functions return data via AEGP_MemHandle, which must
//  be freed with AEGP_FreeMemHandle. This class automates that.
// ============================================================

using memory_suite = suite<AEGP_MemorySuite1,
    fixed_string(kAEGPMemorySuite), kAEGPMemorySuiteVersion1>;

/**
 * @brief RAII wrapper for AEGP_MemHandle.
 * 
 * Automatically frees the handle on destruction via AEGP_FreeMemHandle.
 * Provides lock/unlock and convenience conversion to std::string
 * for UTF-16 string handles (the most common use case in the SDK).
 * 
 * @note Most SDK functions that return strings (GetItemName, GetLayerName, etc.)
 *       return UTF-16 encoded AEGP_MemHandle. Use `to_string()` to convert.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, string retrieval or host allocation returns opaque `AEGP_MemHandle` references, which must be manually locked, cast, unlocked, and explicitly freed using `AEGP_FreeMemHandle` to prevent resource leaks. `aetk::core::mem_handle` implements a strict move-only RAII container that automates locking/unlocking, handles UTF-16 BMP-to-UTF-8 conversions, and disposes memory on scope exit.
 *
 * @warning <b>Memory & Lifecycles:</b> The wrapper takes exclusive ownership of the `AEGP_MemHandle`. Copying is disabled to prevent double-free vulnerabilities. Lock references must not outlive the scope of the corresponding `unlock` call. Invokes `aetk::core::suite` to acquire `AEGP_MemorySuite1`, which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
 */
class mem_handle {
public:
    /**
     * @brief Default constructor.
     *
     * @details Initializes an empty null memory handle.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean null initializer.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    mem_handle() : m_handle(nullptr) {}
    
    /**
     * @brief Allocates wrapper around active handle.
     *
     * @details Binds the raw handle to the RAII container.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Promotes raw pointers to RAII.
     *
     * @warning <b>Memory & Lifecycles:</b> Takes exclusive ownership of the input handle.
     *
     * @param h Raw host memory handle pointer.
     */
    explicit mem_handle(AEGP_MemHandle h) : m_handle(h) {}

    // No copy
    mem_handle(const mem_handle&) = delete;
    mem_handle& operator=(const mem_handle&) = delete;

    // Move
    mem_handle(mem_handle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    mem_handle& operator=(mem_handle&& other) noexcept {
        if (this != &other) {
            free();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    ~mem_handle() { free(); }

    /**
     * @brief Get raw handle pointer.
     *
     * @details Accesses the wrapped `AEGP_MemHandle`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Exposes the underlying C pointer.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Raw memory handle value.
     */
    AEGP_MemHandle get() const { return m_handle; }
    
    /**
     * @brief Get address of the raw handle pointer.
     *
     * @details Accesses address for populating from host calls.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Facilitates out-pointer population.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Address of the internal handle variable.
     */
    AEGP_MemHandle* get_ptr() { return &m_handle; }
    
    /**
     * @brief Get reference to the raw handle.
     *
     * @details Direct reference access.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct C handle reference.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Internal variable reference.
     */
    AEGP_MemHandle& get_ref() { return m_handle; }
    
    /**
     * @brief Verify if handle is active.
     *
     * @details Evaluates if pointer has non-null value.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw null checks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return True if initialized.
     */
    bool is_valid() const { return m_handle != nullptr; }

    /**
     * @brief Locks the handle and returns a raw pointer to the data.
     *
     * @details Pins and returns a direct pointer to raw address data via `AEGP_LockMemHandle`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automates locking and address casts.
     *
     * @warning <b>Memory & Lifecycles:</b> You must invoke `unlock()` once finished accessing memory. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when the suite goes out of scope.
     *
     * @return Void pointer addressing raw memory space.
     */
    void* lock() {
        void* ptr = nullptr;
        memory_suite::call<&AEGP_MemorySuite1::AEGP_LockMemHandle>(m_handle, &ptr);
        return ptr;
    }

    /**
     * @brief Unlocks the memory handle.
     *
     * @details Unpins memory access via `AEGP_UnlockMemHandle`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe unlocking wrapper.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when the suite goes out of scope.
     */
    void unlock() {
        memory_suite::call<&AEGP_MemorySuite1::AEGP_UnlockMemHandle>(m_handle);
    }

    /**
     * @brief Converts a UTF-16 AEGP_MemHandle to a std::string (UTF-8).
     * 
     * @details Performs full BMP character mappings from UTF-16 arrays to standard strings, handling locks and unlocks implicitly.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces manual UTF-16 loop parsing boilerplates with automated BMP mappings.
     *
     * @warning <b>Memory & Lifecycles:</b> Automatically locks and unlocks the memory internally. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when the suite goes out of scope.
     *
     * @return UTF-8 encoded string.
     */
    std::string to_string();

private:
    void free() {
        if (m_handle) {
                            memory_suite::call<&AEGP_MemorySuite1::AEGP_FreeMemHandle>(m_handle);
            
            m_handle = nullptr;
        }
    }

    AEGP_MemHandle m_handle;
};

/**
 * @brief RAII scoped lock helper for mem_handle.
 * 
 * Automatically locks the mem_handle on construction and unlocks it on destruction.
 */
class scoped_mem_handle_lock {
public:
    explicit scoped_mem_handle_lock(mem_handle& handle) : m_handle(handle) {
        m_ptr = m_handle.lock();
    }

    ~scoped_mem_handle_lock() {
        if (m_ptr) {
            m_handle.unlock();
        }
    }

    // Disable copy
    scoped_mem_handle_lock(const scoped_mem_handle_lock&) = delete;
    scoped_mem_handle_lock& operator=(const scoped_mem_handle_lock&) = delete;

    void* get() const noexcept { return m_ptr; }
    explicit operator bool() const noexcept { return m_ptr != nullptr; }

private:
    mem_handle& m_handle;
    void* m_ptr = nullptr;
};

inline std::string mem_handle::to_string() {
    if (!m_handle) return "";

    scoped_mem_handle_lock lock(*this);
    auto* utf16 = static_cast<const A_UTF16Char*>(lock.get());
    if (!utf16) return "";

    // Find length
    size_t len = 0;
    while (utf16[len] != 0) ++len;

    // Simple UTF-16 to ASCII/UTF-8 conversion
    // (handles BMP characters — sufficient for most AE names)
    std::string result;
    result.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        auto ch = utf16[i];
        if (ch < 0x80) {
            result.push_back(static_cast<char>(ch));
        } else if (ch < 0x800) {
            result.push_back(static_cast<char>(0xC0 | (ch >> 6)));
            result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xE0 | (ch >> 12)));
            result.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        }
    }

    return result;
}

} // namespace aetk::core
