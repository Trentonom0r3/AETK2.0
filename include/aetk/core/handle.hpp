#pragma once

#include <AE_Effect.h>
#include <AE_EffectCB.h>

namespace aetk::core {

/**
 * @brief Base class for "borrowed" AE handles. 
 * 
 * Borrowed handles are not owned by the plugin and should not be freed.
 * They represent objects owned by the After Effects project.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, borrowed pointers (like `AEGP_ItemH` or `AEGP_CompH`) are passed as untyped `void*` parameters, with no safety or structural grouping. `aetk::core::borrowed` provides an abstraction layer that wraps borrowed pointers with simple validation helpers.
 *
 * @warning <b>Memory & Lifecycles:</b> The wrapper does not own or free the underlying handle. Handles are owned by the After Effects host.
 *
 * @tparam Traits Type traits structure containing the target raw handle definition.
 */
template <typename Traits>
class borrowed {
public:
    /// Raw handle type from traits.
    using handle_type = typename Traits::type;

    /**
     * @brief Default constructor.
     *
     * @details Binds a null pointer to the wrapper.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard null wrapper.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    borrowed() : m_handle(nullptr) {}
    
    /**
     * @brief Handle wrapper constructor.
     *
     * @details Explicitly initializes the borrowed handle wrapper with a raw pointer.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Binds raw handles to OOP wrappers.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param h Raw handle pointer.
     */
    explicit borrowed(handle_type h) : m_handle(h) {}

    /**
     * @brief Get raw handle.
     *
     * @details Returns the raw handle pointer.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe pointer retrieval.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Raw handle type.
     */
    handle_type get() const { return m_handle; }
    
    /**
     * @brief Get address of the raw handle pointer.
     *
     * @details Returns the raw pointer address.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard address extraction.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Address of the internal handle variable.
     */
    handle_type* get_ptr() { return &m_handle; }
    
    /**
     * @brief Check if handle is valid.
     *
     * @details Evaluates if pointer is non-null.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Ergonomic boolean check.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    explicit operator bool() const { return m_handle != nullptr; }
    
    /**
     * @brief Check if handle is valid.
     *
     * @details Alternate validity checker.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard validity validation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return True if non-null.
     */
    bool is_valid() const { return m_handle != nullptr; }

protected:
    handle_type m_handle;
};

/**
 * @brief RAII wrapper for locking and unlocking AE Handles.
 * 
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, locking a `PF_Handle` requires using standard macros (like `PF_LOCK_HANDLE`) that rely on a local variable named exactly `in_data`. This helper bypasses macro restrictions by calling raw function pointers (`(*(in_data)->utils->host_lock_handle)(h)`) directly, wrapping the allocation in a strict, move-only RAII scope.
 *
 * @warning <b>Memory & Lifecycles:</b> The locker requires `in_data` to remain valid for the duration of the lock scope. Destruction automatically triggers `host_unlock_handle` to prevent memory lockups on the host.
 *
 * @tparam T The type of data stored in the handle.
 */
template <typename T>
class handle_lock {
public:
    /**
     * @brief Locks the raw handle.
     *
     * @details Pins dynamic memory and casts address pointers using callback suite handlers.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bypasses standard `PF_LOCK_HANDLE` macros to avoid stack variable dependencies.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param handle The AE handle to lock.
     * @param in_data Pointer to PF_InData (required for locking).
     */
    handle_lock(PF_Handle handle, PF_InData* in_data) : m_handle(handle), m_in_data(in_data) {
        if (m_handle && m_in_data) {
            // Using the raw callback from PF_UtilCallbacks
            m_ptr = reinterpret_cast<T*>((*(m_in_data)->utils->host_lock_handle)(m_handle));
        }
    }

    /**
     * @brief Unlocks the handle on destruction.
     *
     * @details Unpins address pages using suite unlock calls.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automatic scope unpinning.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    ~handle_lock() {
        if (m_handle && m_in_data) {
            (*(m_in_data)->utils->host_unlock_handle)(m_handle);
        }
    }

    // No copy allowed.
    handle_lock(const handle_lock&) = delete;
    handle_lock& operator=(const handle_lock&) = delete;

    // Move allowed.
    handle_lock(handle_lock&& other) noexcept
        : m_handle(other.m_handle), m_in_data(other.m_in_data), m_ptr(other.m_ptr) {
        other.m_handle = nullptr;
        other.m_in_data = nullptr;
        other.m_ptr = nullptr;
    }

    handle_lock& operator=(handle_lock&& other) noexcept {
        if (this != &other) {
            if (m_handle && m_in_data) (*(m_in_data)->utils->host_unlock_handle)(m_handle);
            m_handle = other.m_handle;
            m_in_data = other.m_in_data;
            m_ptr = other.m_ptr;
            other.m_handle = nullptr;
            other.m_in_data = nullptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    /**
     * @brief Direct member accessor.
     *
     * @details Direct pointer member dereference.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Smart pointer member mapping.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Typed pointer.
     */
    T* operator->() noexcept { return m_ptr; }
    
    /**
     * @brief Read-only member accessor.
     *
     * @details Read-only pointer member dereference.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Const-safe smart pointer member mapping.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Const typed pointer.
     */
    const T* operator->() const noexcept { return m_ptr; }
    
    /**
     * @brief Value dereferencer.
     *
     * @details Returns reference to typed memory.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct value dereference.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Reference to the typed value.
     */
    T& operator*() noexcept { return *m_ptr; }
    
    /**
     * @brief Const value dereferencer.
     *
     * @details Returns read-only reference to typed memory.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Const-safe value dereference.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Const reference to the typed value.
     */
    const T& operator*() const noexcept { return *m_ptr; }
    
    /**
     * @brief Check if locked address is valid.
     *
     * @details Returns true if locked address is non-null.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard allocation validation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    explicit operator bool() const noexcept { return m_ptr != nullptr; }

private:
    PF_Handle m_handle = nullptr;
    PF_InData* m_in_data = nullptr;
    T* m_ptr = nullptr;
};

} // namespace aetk::core
