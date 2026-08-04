#pragma once

#include <AE_Effect.h>
#include <AE_EffectCB.h>
#include <AE_GeneralPlug.h>
#include <aetk/core/handle.hpp>
#include <aetk/effect/context/context.hpp>

namespace aetk::effect {

/**
 * @brief A temporary RAII lock for initializing a newly allocated state handle.
 *
 * @details Locks the target host handle upon construction and releases it upon
 * destruction.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, locking a newly allocated
 * handle requires manually calling `host_lock_handle`, remembering to pair it
 * with `host_unlock_handle` inside try-catch structures, and handling garbage
 * pointers. `aetk::effect::smart_state_handle` is an RAII helper that
 * automatically locks upon construction and unlocks upon destruction, ensuring
 * exception safety during state allocations.
 *
 * @warning <b>Memory & Lifecycles:</b> The wrapper only manages the temporary
 * lock (unlocks upon destruction) but DOES NOT dispose the handle itself. The
 * handle remains persistent in memory.
 *
 * @tparam T The dynamic state type.
 */
template <typename T> class smart_state_handle {
public:
  /**
   * @brief Lock constructor.
   *
   * @param in_data InData parameter from AE.
   * @param handle Host-allocated PF_Handle.
   */
  smart_state_handle(PF_InData *in_data, PF_Handle handle)
      : m_in_data(in_data), m_handle(handle) {
    if (m_in_data && m_handle) {
      m_ptr = reinterpret_cast<T *>(
          (*(m_in_data)->utils->host_lock_handle)(m_handle));
    }
  }

  /**
   * @brief Safe lock release on scope exit.
   */
  ~smart_state_handle() {
    if (m_in_data && m_handle && m_ptr) {
              (*(m_in_data)->utils->host_unlock_handle)(m_handle);
      
    }
  }

  /**
   * @brief Member dereference operator.
   *
   * @return Locked struct pointer.
   */
  T *operator->() noexcept { return m_ptr; }

  /**
   * @brief Retrieve the raw handle.
   *
   * @return Underlying `PF_Handle`.
   */
  PF_Handle handle() const noexcept { return m_handle; }

  /**
   * @brief Boolean validation check.
   */
  explicit operator bool() const noexcept { return m_ptr != nullptr; }

private:
  PF_InData *m_in_data;
  PF_Handle m_handle;
  T *m_ptr = nullptr;
};

/**
 * @brief Allocate a new AE Handle for a state structure and return a lock for
 * initialization.
 *
 * @details Combines raw host handle allocation, placement-new construction, and
 * structured locking inside an exception-safe templated factory.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Combines raw host handle allocation,
 * placement-new construction, and structured locking inside an exception-safe
 * templated factory.
 *
 * @warning <b>Memory & Lifecycles:</b> Allocates a new dynamic handle using
 * After Effects' memory manager. The caller is responsible for ensuring this
 * handle is eventually disposed via `destroy_state_handle`.
 *
 * @tparam T The dynamic state struct type.
 * @param in_data InData parameter from AE.
 * @param args Parameter pack forwarded directly to the constructor of `T`.
 * @return Smart state handle manager wrapper.
 */
template <typename T, typename... Args>
smart_state_handle<T> new_state_handle(PF_InData *in_data, Args &&...args) {
  if (!in_data) {
    throw core::exception(PF_Err_INTERNAL_STRUCT_DAMAGED,
                          "Null in_data passed to new_state_handle");
  }

  PF_Handle handle = (*(in_data)->utils->host_new_handle)(sizeof(T));
  if (!handle) {
    throw core::exception(PF_Err_OUT_OF_MEMORY,
                          "Failed to allocate state handle");
  }

  smart_state_handle<T> smart(in_data, handle);
  if (smart) {
    new (smart.operator->()) T(std::forward<Args>(args)...);
  }
  return smart;
}

/**
 * @brief Destroy a state handle and call the structure's destructor.
 *
 * @details Explicitly triggers the target object destructor `ptr->~T()` and
 * safely disposes the host handle using `host_dispose_handle`.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Explicitly triggers the target object
 * destructor `ptr->~T()` and safely disposes the host handle using
 * `host_dispose_handle`.
 *
 * @warning <b>Memory & Lifecycles:</b> Safe to call on valid handles, executing
 * cleanup routines in a silent, no-throw block.
 *
 * @tparam T The dynamic state type.
 * @param in_data InData parameter from AE.
 * @param handle Dynamic handle to destroy.
 */
template <typename T>
void destroy_state_handle(PF_InData *in_data, PF_Handle handle) {
  if (in_data && handle) {
          T *ptr =
          reinterpret_cast<T *>((*(in_data)->utils->host_lock_handle)(handle));
      if (ptr) {
        ptr->~T();
        (*(in_data)->utils->host_unlock_handle)(handle);
      }
      (*(in_data)->utils->host_dispose_handle)(handle);
    
  }
}

/**
 * @brief RAII lock for an AE Handle.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Direct type alias to unified generic
 * handle_lock wrappers.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
template <typename T> using lock_handle = core::handle_lock<T>;

// ══════════════════════════════════════════════════════════════════════
//  Sequence Data — MFR-safe access to per-instance persistent state
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Initialize sequence data for a POD-like struct.
 *
 * @details Call this in on_sequence_setup. Allocates a handle, placement-new's
 * T into it, and sets out_data->sequence_data.
 *
 * Usage:
 *   static void on_sequence_setup(const context& ctx) {
 *       aetk::effect::init_sequence_data<my_seq_data>(ctx);
 *   }
 *
 * @note <b>AE SDK Paradigm Shift:</b> Modernized timeline setup helper which
 * automatically allocates sequence data handles and constructs custom
 * structures.
 *
 * @warning <b>Memory & Lifecycles:</b> Allocates dynamic handles on host memory
 * which must be balanced in setdowns.
 *
 * @tparam T The dynamic sequence data type.
 * @param ctx Target setup context.
 * @param args Parameter pack forwarded directly to the constructor of `T`.
 */
template <typename T, typename... Args>
void init_sequence_data(const context &ctx, Args &&...args) {
  PF_InData *in_data = ctx.in_data_ptr();
  PF_Handle handle = (*(in_data)->utils->host_new_handle)(sizeof(T));
  if (!handle) {
    throw core::exception(PF_Err_OUT_OF_MEMORY,
                          "Failed to allocate sequence data");
  }

  T *ptr = reinterpret_cast<T *>((*(in_data)->utils->host_lock_handle)(handle));
  if (ptr) {
    new (ptr) T(std::forward<Args>(args)...);
    (*(in_data)->utils->host_unlock_handle)(handle);
  }

  ctx.out_data_ptr()->sequence_data = handle;
}

/**
 * @brief Dispose sequence data. Call in on_sequence_setdown.
 *
 * @details Safe execution wrapper that releases sequence storage.
 *
 * Usage:
 *   static void on_sequence_setdown(const context& ctx) {
 *       aetk::effect::dispose_sequence_data<my_seq_data>(ctx);
 *   }
 *
 * @note <b>AE SDK Paradigm Shift:</b> Safe execution wrapper that releases
 * sequence storage.
 *
 * @warning <b>Memory & Lifecycles:</b> Disposes of active handles to avoid
 * memory leaks.
 *
 * @tparam T The dynamic sequence data type.
 * @param ctx Target setdown context.
 */
template <typename T> void dispose_sequence_data(const context &ctx) {
  PF_InData *in_data = ctx.in_data_ptr();
  PF_Handle handle = in_data->sequence_data;
  if (handle) {
    destroy_state_handle<T>(in_data, handle);
    ctx.out_data_ptr()->sequence_data = nullptr;
  }
}

/**
 * @brief Read-only access to sequence data. MFR-safe via
 * PF_EffectSequenceDataSuite.
 *
 * @details Under Multi-Frame Rendering (MFR), `in_data->sequence_data` is NULL
 * during render threads (Smart Render callbacks). Accessing it directly results
 * in crashes. `aetk::effect::const_sequence_data` resolves this by querying
 * `PF_EffectSequenceDataSuite1` to acquire read-only instance states
 * thread-safely, with automatic fallbacks for classical rendering pipelines.
 *
 * Use this in on_smart_render or any render-time context where
 * in_data->sequence_data is NULL under threaded rendering.
 *
 * Usage:
 *   auto seq = aetk::effect::get_sequence_data<my_seq_data>(ctx);
 *   if (seq) {
 *       float val = seq->my_cached_value;
 *   }
 *
 * @note <b>AE SDK Paradigm Shift:</b> Under Multi-Frame Rendering (MFR),
 * `in_data->sequence_data` is NULL during render threads (Smart Render
 * callbacks). Accessing it directly results in crashes.
 * `aetk::effect::const_sequence_data` resolves this by querying
 * `PF_EffectSequenceDataSuite1` to acquire read-only instance states
 * thread-safely, with automatic fallbacks for classical rendering pipelines.
 *
 * @warning <b>Memory & Lifecycles:</b> Locks the target sequence state during
 * the scope of the class wrapper. Automatically unlocks via
 * `host_unlock_handle` if falling back to classic handle checkouts.
 *
 * @tparam T The dynamic sequence data type.
 */
template <typename T> class const_sequence_data {
public:
  /**
   * @brief Safe MFR sequence lock.
   *
   * @param ctx Target render/smart-render context.
   */
  explicit const_sequence_data(const context &ctx) {
    // First try the MFR-safe suite
          PF_EffectSequenceDataSuite1 *suite = nullptr;
      auto err = ctx.in_data_ptr()->pica_basicP->AcquireSuite(
          kPFEffectSequenceDataSuite, kPFEffectSequenceDataSuiteVersion1,
          (const void **)&suite);
      if (err == PF_Err_NONE && suite) {
        PF_ConstHandle handle = nullptr;
        err = suite->PF_GetConstSequenceData(ctx.in_data_ptr()->effect_ref,
                                             &handle);
        if (err == PF_Err_NONE && handle) {
          m_ptr = reinterpret_cast<const T *>(*handle);
        }
        ctx.in_data_ptr()->pica_basicP->ReleaseSuite(
            kPFEffectSequenceDataSuite, kPFEffectSequenceDataSuiteVersion1);
      }
    

    // Fallback: try in_data->sequence_data directly (non-MFR contexts)
    if (!m_ptr && ctx.in_data_ptr()->sequence_data) {
      m_ptr = reinterpret_cast<const T *>(
          (*(ctx.in_data_ptr())->utils->host_lock_handle)(
              ctx.in_data_ptr()->sequence_data));
      m_needs_unlock = true;
      m_in_data = ctx.in_data_ptr();
    }
  }

  /**
   * @brief Releases locks on exit.
   */
  ~const_sequence_data() {
    if (m_needs_unlock && m_in_data && m_ptr) {
      (*(m_in_data)->utils->host_unlock_handle)(m_in_data->sequence_data);
    }
  }

  const T *operator->() const noexcept { return m_ptr; }
  const T &operator*() const noexcept { return *m_ptr; }
  explicit operator bool() const noexcept { return m_ptr != nullptr; }
  const T *get() const noexcept { return m_ptr; }

  // Non-copyable
  const_sequence_data(const const_sequence_data &) = delete;
  const_sequence_data &operator=(const const_sequence_data &) = delete;

private:
  const T *m_ptr = nullptr;
  bool m_needs_unlock = false;
  PF_InData *m_in_data = nullptr;
};

/**
 * @brief Mutable access to sequence data. Only valid in setup/setdown contexts.
 *
 * @details Lock manager providing mutable access during sequence setup/setdown.
 *
 * Usage:
 *   static void on_sequence_setup(const context& ctx) {
 *       aetk::effect::init_sequence_data<my_seq_data>(ctx);
 *       auto seq = aetk::effect::mutable_sequence_data<my_seq_data>(ctx);
 *       seq->my_value = 42;
 *   }
 *
 * @note <b>AE SDK Paradigm Shift:</b> Lock manager providing mutable access
 * during sequence setup/setdown.
 *
 * @warning <b>Memory & Lifecycles:</b> Locks handles on construction,
 * guaranteeing clean `host_unlock_handle` callbacks on scope exit.
 *
 * @tparam T The dynamic sequence data type.
 */
template <typename T> class mutable_sequence_data {
public:
  /**
   * @brief Mutable sequence lock.
   *
   * @param ctx Target setup/setdown context.
   */
  explicit mutable_sequence_data(const context &ctx) {
    PF_Handle handle = ctx.in_data_ptr()->sequence_data;
    if (!handle)
      handle = ctx.out_data_ptr()->sequence_data;
    if (handle) {
      m_ptr = reinterpret_cast<T *>(
          (*(ctx.in_data_ptr())->utils->host_lock_handle)(handle));
      m_in_data = ctx.in_data_ptr();
      m_handle = handle;
    }
  }

  /**
   * @brief Releases locks on exit.
   */
  ~mutable_sequence_data() {
    if (m_in_data && m_handle && m_ptr) {
      (*(m_in_data)->utils->host_unlock_handle)(m_handle);
    }
  }

  T *operator->() noexcept { return m_ptr; }
  T &operator*() noexcept { return *m_ptr; }
  explicit operator bool() const noexcept { return m_ptr != nullptr; }
  T *get() noexcept { return m_ptr; }

  // Non-copyable
  mutable_sequence_data(const mutable_sequence_data &) = delete;
  mutable_sequence_data &operator=(const mutable_sequence_data &) = delete;

private:
  T *m_ptr = nullptr;
  PF_InData *m_in_data = nullptr;
  PF_Handle m_handle = nullptr;
};

// ══════════════════════════════════════════════════════════════════════
//  Global Data — Shared state across all instances
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Lock manager for shared global data.
 *
 * @details Safe RAII wrapper for shared global data, locking and unlocking
 * automatically.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Safe RAII wrapper for shared global data,
 * locking and unlocking automatically.
 *
 * @warning <b>Memory & Lifecycles:</b> Balances lock references.
 *
 * @tparam T The dynamic global state type.
 */
template <typename T> class global_data {
public:
  /**
   * @brief Global lock constructor.
   *
   * @param ctx Target context reference.
   */
  explicit global_data(const context &ctx) {
    m_handle = ctx.in_data_ptr()->global_data;
    if (m_handle) {
      m_ptr = reinterpret_cast<T *>(
          (*(ctx.in_data_ptr())->utils->host_lock_handle)(m_handle));
      m_in_data = ctx.in_data_ptr();
    }
  }

  /**
   * @brief Releases locks on exit.
   */
  ~global_data() {
    if (m_in_data && m_handle && m_ptr) {
      (*(m_in_data)->utils->host_unlock_handle)(m_handle);
    }
  }

  T *operator->() noexcept { return m_ptr; }
  T &operator*() noexcept { return *m_ptr; }
  explicit operator bool() const noexcept { return m_ptr != nullptr; }

private:
  T *m_ptr = nullptr;
  PF_InData *m_in_data = nullptr;
  PF_Handle m_handle = nullptr;
};

/**
 * @brief Convenience helper to lock sequence data.
 *
 * Usage: auto seq = aetk::effect::get_sequence_data<my_data>(ctx);
 *
 * @note <b>AE SDK Paradigm Shift:</b> Replaces raw handle checking.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 *
 * @tparam T The dynamic sequence data type.
 * @param ctx Target render/smart-render context.
 * @return Locked read-only sequence data reference.
 */
template <typename T>
const_sequence_data<T> get_sequence_data(const context &ctx) {
  return const_sequence_data<T>(ctx);
}

} // namespace aetk::effect
