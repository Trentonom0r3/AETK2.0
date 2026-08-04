#pragma once
#include <AE_Effect.h>
#include <aetk/core/suite.hpp>
#include <string>
#include <cstring>

namespace aetk::effect {

/**
 * @brief Type-safe external dependency check modes requested by After Effects.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Replaces raw C enum values for external dependency checks with a type-safe `std::uint8_t` enum class.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
enum class dependency_check_type : std::uint8_t {
    none = PF_DepCheckType_NONE,
    all = PF_DepCheckType_ALL_DEPENDENCIES,
    missing = PF_DepCheckType_MISSING_DEPENDENCIES
};

/**
 * @brief High-level wrapper for PF_Cmd_GET_EXTERNAL_DEPENDENCIES.
 * 
 * @details Handles the allocation of host memory for dependency reports during AE project collections or render preparation.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, reporting external dependencies (e.g. assets, missing fonts) during `PF_Cmd_GET_EXTERNAL_DEPENDENCIES` requires manually allocating string handles on the host, locking handles, copying byte arrays, unlocking, and assigning the handle to `dependencies_strH`. `aetk::effect::dependency_context` provides an exceptionally clean `set_report(const std::string&)` method that encapsulates all PICA suite handle operations seamlessly.
 *
 * @warning <b>Memory & Lifecycles:</b> The allocated handle is owned and eventually released by the After Effects host. This class utilizes `aetk::core::suite<PF_HandleSuite1>` to perform host-side handle operations.
 */
class dependency_context {
public:
    /**
     * @brief Dependency context constructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simple parameter promotion.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param in_data Input struct parameter.
     * @param extra Raw dependency extra parameters.
     */
    dependency_context(PF_InData* in_data, PF_ExtDependenciesExtra* extra)
        : m_pica(in_data->pica_basicP), m_extra(extra) {}

    /**
     * @brief Retrieve the dependency check mode requested by the host.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean enum translation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Bounded dependency check type.
     */
    dependency_check_type check_type() const {
        return static_cast<dependency_check_type>(m_extra->check_type);
    }

    /**
     * @brief Set the dependency report string.
     * 
     * AE expects a NULL-terminated string inside a PF_Handle.
     * AETK handles the host allocation and copying for you.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Elegant RAII-based string copying to host.
     *
     * @warning <b>Memory & Lifecycles:</b> Allocates a host handle which is taken ownership of by AE.
     *
     * @param report Target report string.
     */
    void set_report(const std::string& report) {
        if (report.empty()) return;

        aetk::core::suite<PF_HandleSuite1> s(m_pica);
        A_long size = static_cast<A_long>(report.size() + 1);
        
        PF_Handle msgH = s->host_new_handle(size);
        if (msgH) {
            char* ptr = static_cast<char*>(s->host_lock_handle(msgH));
            if (ptr) {
                std::memcpy(ptr, report.c_str(), size);
                s->host_unlock_handle(msgH);
                m_extra->dependencies_strH = msgH;
            }
        }
    }

private:
    SPBasicSuite* m_pica;
    PF_ExtDependenciesExtra* m_extra;
};

} // namespace aetk::effect
