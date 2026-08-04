#pragma once

/**
 * @file utility.hpp
 * @brief High-level wrappers for AEGP utility functions and dialog management.
 */

#include <aetk/core/suite.hpp>

namespace aetk::core {

/**
 * @brief Provides high-level access to After Effects Utility functions.
 * Wraps AEGP_UtilitySuite6.
 *
 * @details Encapsulates global messaging structures, alerts, and transactional undo grouping operations.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In raw plugins, displaying alerts or handling undo groupings requires manual stack checkout of `AEGP_UtilitySuite6` and passing dynamic plugin IDs. `aetk::core::utility` encapsulates this suite through clean static wrappers, querying context states automatically.
 *
 * @warning <b>Memory & Lifecycles:</b> The wrapper relies internally on `aetk::core::suite` to acquire and release `AEGP_UtilitySuite6`, which automatically decrements the host reference count via `ReleaseSuite` when the suite goes out of scope.
 */
class utility {
private:
    using utility_suite = suite<AEGP_UtilitySuite6, fixed_string(kAEGPUtilitySuite), kAEGPUtilitySuiteVersion6>;

public:
    /**
     * @brief Displays a dialog box with the plugin's name and the provided info string.
     *
     * @details Displays modular informational popups using `AEGP_ReportInfo`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct wrapper over `AEGP_ReportInfo` with automatic ID query.
     *
     * @warning <b>Memory & Lifecycles:</b> Block execution on the primary thread. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when the suite goes out of scope.
     *
     * @param info The message to display.
     */
    static void report_info(const char* info) {
        utility_suite::call<&AEGP_UtilitySuite6::AEGP_ReportInfo>(context::get_plugin_id(), info);
    }

    /**
     * @brief Starts an Undo Group in the AE timeline.
     * 
     * @details Commences standard transactional boundaries for undo tracking via `AEGP_StartUndoGroup`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Starts transactional undo blocks cleanly.
     *
     * @warning <b>Memory & Lifecycles:</b> MUST be balanced with a call to `end_undo_group()`. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when the suite goes out of scope.
     *
     * @param name The name of the action that will appear in the Undo menu.
     */
    static void start_undo_group(const char* name) {
        utility_suite::call<&AEGP_UtilitySuite6::AEGP_StartUndoGroup>(name);
    }

    /**
     * @brief Ends the current Undo Group.
     *
     * @details Finalizes the active timeline transaction via `AEGP_EndUndoGroup`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Concludes the active undo transaction.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when the suite goes out of scope.
     */
    static void end_undo_group() {
        utility_suite::call<&AEGP_UtilitySuite6::AEGP_EndUndoGroup>();
    }
};

} // namespace aetk::core
