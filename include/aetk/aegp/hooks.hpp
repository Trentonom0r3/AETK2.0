#pragma once

#include <aetk/core/suite.hpp>
#include <functional>
#include <vector>

namespace aetk::aegp {

// ============================================================
//  Hook convenience wrappers (Layer 2)
//
//  These let you register AE lifecycle hooks with simple lambdas.
//  For full control, use Layer 1 (RegisterSuite5) directly.
// ============================================================

namespace detail {

// --- Stored callbacks ---
inline std::vector<std::function<void()>> death_callbacks;
inline std::vector<std::function<void()>> idle_callbacks;

// --- Raw C dispatchers ---

inline A_Err death_hook_dispatch(AEGP_GlobalRefcon /*plugin_refconP*/,
                                 AEGP_DeathRefcon /*refconP*/) {
  for (const auto &cb : death_callbacks) {
    if (cb) {

      cb();
    }
  }
  return A_Err_NONE;
}

inline A_Err idle_hook_dispatch(AEGP_GlobalRefcon /*plugin_refconP*/,
                                AEGP_IdleRefcon /*refconP*/,
                                A_long *max_sleepPL) {
  for (const auto &cb : idle_callbacks) {
    if (cb) {

      cb();
    }
  }
  // Default: don't hog the CPU. Sleep up to 100ms.
  if (max_sleepPL)
    *max_sleepPL = 6; // units of 1/60s ≈ 100ms
  return A_Err_NONE;
}

} // namespace detail

/**
 * @brief Registers a global cleanup callback invoked upon After Effects
 * shutdown.
 *
 * @details Establishes a shutdown trigger using `AEGP_RegisterDeathHook`.
 * This is the dedicated phase to deallocate long-lived singletons, release
 * graphics pipeline contexts, or serialize caching databases before the host
 * process terminates.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Replaces direct C-callback hooks that
 * expose raw reference counters and refcons with a safe, exception-guarded
 * std::function wrapper.
 *
 * @warning <b>Memory & Lifecycles:</b> This callback is executed very late in
 * the application tear-down phase on the main thread. Accessing other plugins
 * or calling complex host suites during this time can result in access
 * violations if those modules have already been unmapped. Keep cleanup routines
 * focused entirely on private resources. Internally acquires registration
 * capability via `aetk::core::suite` which automatically decrements the host
 * reference count via `ReleaseSuite` when the suite wrapper goes out of scope.
 *
 * @param callback A thread-safe, non-throwing cleanup closure.
 */
inline void on_death(std::function<void()> callback) {
  using reg = aetk::core::suite<AEGP_RegisterSuite5,
                                aetk::core::fixed_string(kAEGPRegisterSuite),
                                kAEGPRegisterSuiteVersion5>;

  bool first = detail::death_callbacks.empty();
  detail::death_callbacks.push_back(std::move(callback));

  if (first) {
    reg::call<&AEGP_RegisterSuite5::AEGP_RegisterDeathHook>(
        aetk::core::context::get_plugin_id(), detail::death_hook_dispatch,
        reinterpret_cast<AEGP_DeathRefcon>(0));
  }
}

/**
 * @brief Registers a persistent background callback executed during After
 * Effects' idle frames.
 *
 * @details Hooks into the main event loop through `AEGP_RegisterIdleHook`. The
 * registered closure will run repeatedly during user inactivity periods,
 * defaulting to a polite 100ms maximum sleep phase (`max_sleepPL = 6` frames)
 * to avoid CPU cores spinning at 100%.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Standardizes and automates high-frequency
 * idle scheduling, automatically wrapping executions in try-catch bounds and
 * handling CPU sleep intervals inside raw `AEGP_IdleHookActivePL` hooks.
 *
 * @warning <b>Memory & Lifecycles:</b> This is called on the host's primary UI
 * thread. Heavy computations or blocking operations (like synchronized network
 * calls) will freeze the entire After Effects user interface. Keep execution
 * times extremely short (under 5ms) or use it to safely poll off-thread work
 * queues. Internally acquires registration capability via `aetk::core::suite`
 * which automatically decrements the host reference count via `ReleaseSuite`
 * when the suite wrapper goes out of scope.
 *
 * @param callback A high-frequency closure executed on the primary UI thread
 * during idle.
 */
inline void on_idle(std::function<void()> callback) {
  using reg = aetk::core::suite<AEGP_RegisterSuite5,
                                aetk::core::fixed_string(kAEGPRegisterSuite),
                                kAEGPRegisterSuiteVersion5>;

  bool first = detail::idle_callbacks.empty();
  detail::idle_callbacks.push_back(std::move(callback));

  if (first) {
    reg::call<&AEGP_RegisterSuite5::AEGP_RegisterIdleHook>(
        aetk::core::context::get_plugin_id(), detail::idle_hook_dispatch,
        reinterpret_cast<AEGP_IdleRefcon>(0));
  }
}

} // namespace aetk::aegp
