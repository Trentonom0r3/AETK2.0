#pragma once

#include "error.hpp"
#include <AE_GeneralPlug.h>
#include <SPBasic.h>
#include <atomic>

namespace aetk::core {

/**
 * @brief Hidden global context for AETK.
 *
 * Stores the SPBasicSuite* and AEGP_PluginID so that all suite
 * wrappers can access them without manual passing.
 *
 * This lives in `core` because both AEGP and Effect plugins
 * need access to SPBasicSuite for suite acquisition.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, `SPBasicSuite` and
 * `AEGP_PluginID` must be passed manually through all internal function
 * parameters, or stored in brittle static globals inside every plugin.
 * `aetk::core::context` provides a unified, thread-safe global registry that
 * manages these parameters behind the scenes, allowing all high-level wrappers
 * to query PICA suites dynamically.
 *
 * @warning <b>Memory & Lifecycles:</b> The context is initialized automatically
 * upon plugin startup. Querying or acquiring suites before `init` has been
 * executed will throw a runtime exception.
 */
namespace context {
    /// Global pointer to the core PICA suites registry SPBasicSuite.
    inline std::atomic<SPBasicSuite*> sp_basic_suite { nullptr };

    /// Global After Effects dynamic GP plugin identifier.
    inline std::atomic<AEGP_PluginID> plugin_id { 0 };

    /// Global process-wide flag representing whether we are running in Premiere Pro.
    inline std::atomic<bool> is_premiere_host { false };

    inline void set_is_premiere(bool premiere) {
        is_premiere_host.store(premiere);
    }

    inline bool is_premiere() {
        return is_premiere_host.load();
    }

    /**
     * @brief Context initialization helper.
     *
     * @details Sets the dynamic basic suite registry pointer and AEGP identifier
     * variables.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Thread-safe initialization boundary.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param basic_suite Core basic suite registry.
     * @param id dynamic plugin ID.
     */
    inline void init(SPBasicSuite* basic_suite, AEGP_PluginID id) {
        sp_basic_suite.store(basic_suite);
        plugin_id.store(id);
    }

    /**
     * @brief Safe basic suite query helper.
     *
     * @details Resolves the basic suite registry and throws if the pointer has not
     * been initialized.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automated exception trigger when context
     * is queried before initialization.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Raw `SPBasicSuite` pointer.
     */
    inline SPBasicSuite* get_basic_suite() {
        SPBasicSuite* pica = sp_basic_suite.load();
        if (!pica) {
            throw exception(PF_Err_OUT_OF_MEMORY,
                "SPBasicSuite is null. Was aetk::aegp::plugin or "
                "aetk::effect::plugin initialized?");
        }
        return pica;
    }

    /**
     * @brief Safe plugin ID query helper.
     *
     * @details Resolves the unique identifier and throws if query is executed
     * prematurely.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean diagnostic verification.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Dynamic `AEGP_PluginID` value.
     */
    inline AEGP_PluginID get_plugin_id() {
        AEGP_PluginID pid = plugin_id.load();
        if (!pid) {
            throw exception(PF_Err_OUT_OF_MEMORY,
                "AEGP_PluginID is 0. Was aetk::aegp::plugin initialized?");
        }
        return pid;
    }
} // namespace context

} // namespace aetk::core
