#pragma once

#include <aetk/core/error.hpp>
#include <aetk/core/log.hpp>
#include <aetk/core/types.hpp>
#include <aetk/ui/message.hpp>


#include <AE_GeneralPlug.h>
#include <SPBasic.h>
#include <SPSuites.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>


namespace aetk::aegp {

/**
 * @brief Thread-safe proxy wrapper around `SPBasicSuite` PICA allocation hooks.
 *
 * @details This class wraps the basic Adobe PICA suite registry (`SPBasicSuite`),
 * providing a lightweight interface to check out and release host-supplied
 * suites at runtime. It acts as the operational handle during standard AEGP lifecycle
 * callbacks.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In raw C SDK plugins, developers must manage a
 * global `SPBasicSuite` pointer or instantiate a bulky `AEGP_SuiteHandler` helper object
 * on the stack. `aetk::aegp::context` encapsulates suite access into a simple,
 * object-oriented interface, converting manual HRESULT/`A_Err` checks into standard C++
 * exceptions automatically when a required suite is missing or incompatible.
 *
 * @warning <b>Memory & Lifecycles:</b> Do not cache returned raw suite pointers
 * permanently. Host suites are dynamically versioned and can become stale if the context
 * is closed or the host shuts down. Always balance calls to `acquire_suite` with
 * `release_suite`, or prefer high-level RAII mechanisms like `aetk::core::suite` which
 * automatically decrements the host reference count via `ReleaseSuite` when it goes out
 * of scope.
 */
class context {
public:
    /**
     * @brief Constructor for the context wrapper.
     *
     * @details Initializes the context proxy wrapper with a raw SPBasicSuite pointer.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Wraps raw PICA initialization boundaries
     * safely.
     *
     * @warning <b>Memory & Lifecycles:</b> Does not take ownership of the basic suite
     * pointer.
     *
     * @param pica_basic The basic suite pointer from the host.
     */
    explicit context(SPBasicSuite* pica_basic)
        : m_pica_basic(pica_basic) {
    }

    /**
     * @brief Access the raw SPBasicSuite pointer.
     *
     * @details Provides direct access to the underlying PICA suite pointer managed by
     * this context.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Exposes the underlying C pointer for
     * interfacing with raw C APIs.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return The underlying SPBasicSuite raw pointer.
     */
    SPBasicSuite* pica_basic() const {
        return m_pica_basic;
    }

    /**
     * @brief Dynamic retrieval of a host-defined PICA suite.
     *
     * @details Queries the core PICA registry (`SPBasicSuite`) to retrieve a pointer
     * to the requested API suite at the specified major version.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Instead of calling
     * `m_pica_basic->AcquireSuite(name, version, &ptr)` and checking for non-zero error
     * codes, this templated method automatically casts the pointer to the target suite
     * class (`SuiteT`) and triggers an AETK diagnostic check. If the host doesn't support
     * the requested version, an exception is thrown.
     *
     * @warning <b>Memory & Lifecycles:</b> Every successful call to `acquire_suite`
     * increments the reference count of the suite on the host. It MUST be matched by a
     * call to `release_suite` with identical parameters, or wrapped in
     * `aetk::core::suite` (which automatically decrements the host reference count via
     * `ReleaseSuite` when the wrapper goes out of scope), otherwise a suite resource leak
     * occurs.
     *
     * @tparam SuiteT The exact C-struct interface of the suite (e.g. `AEGP_CompSuite12`).
     * @param name The global unique string identifying the suite (e.g. `kAEGPCompSuite`).
     * @param version The requested suite version (e.g. `kAEGPCompSuiteVersion12`).
     * @return A valid pointer to the requested suite struct interface.
     */
    template <typename SuiteT>
    SuiteT* acquire_suite(const char* name, int32_t version) const {
        const void* suite = nullptr;
        core::check_err(
            static_cast<PF_Err>(m_pica_basic->AcquireSuite(name, version, &suite)));
        return static_cast<SuiteT*>(const_cast<void*>(suite));
    }

    /**
     * @brief Releases a previously checked-out host suite.
     *
     * @details Decrements the host-side reference count of the target suite.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw `ReleaseSuite` pointer calls with
     * typed template arguments.
     *
     * @warning <b>Memory & Lifecycles:</b> Only call this on suites that were
     * successfully retrieved via `acquire_suite`. Double-releasing or passing invalid
     * pointers will corrupt host memory tables. Recommend using `aetk::core::suite`
     * instead, which automatically decrements the host reference count via `ReleaseSuite`
     * when it goes out of scope.
     *
     * @tparam SuiteT The exact C-struct interface of the suite being released.
     * @param name The global unique string identifying the suite.
     * @param version The version number of the suite interface.
     */
    template <typename SuiteT>
    void release_suite(const char* name, int32_t version) const {
        core::check_err(static_cast<PF_Err>(m_pica_basic->ReleaseSuite(name, version)));
    }

protected:
    SPBasicSuite* m_pica_basic;
};

/**
 * @brief Context provided during the AEGP initialization phase.
 *
 * @details Exposes registration capabilities that are only valid when the AEGP
 * is first loaded by the After Effects host inside its main entry point function.
 * Allows the plugin to advertise custom C-compatible suites to the rest of the
 * application.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Standardizes the registration of developer-defined
 * suites during the execution of `EntryPointFunc`, bypassing manual `SPSuitesSuite` query
 * boilerplates. Allows AETK plugins to act as native suite providers seamlessly.
 *
 * @warning <b>Memory & Lifecycles:</b> Methods like `register_suite` can only be invoked
 * safely during the `EntryPointFunc` execution path on the main thread. Attempting to
 * register suites at render time or from worker threads is undefined behavior.
 */
class entry_context : public context {
public:
    /**
     * @brief Constructor for the entry context.
     *
     * @details Instantiates the entry-specific context with the plugin identifier.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Binds the unique `AEGP_PluginID` cleanly
     * alongside the basic suite handle.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param pica_basic The basic suite pointer from the host.
     * @param id The plugin ID assigned by After Effects.
     */
    entry_context(SPBasicSuite* pica_basic, AEGP_PluginID id)
        : context(pica_basic)
        , m_plugin_id(id) {
    }

    /**
     * @brief Access the assigned plugin identifier.
     *
     * @details Returns the unique ID allocated to the plugin by the After Effects host.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Encapsulates raw `AEGP_PluginID` retrieval.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return The AEGP_PluginID of the current plugin.
     */
    AEGP_PluginID plugin_id() const {
        return m_plugin_id;
    }

    /**
     * @brief Registers a custom, C-compatible PICA suite with the After Effects host.
     *
     * @details Publishes a struct of function pointers to the global PICA registry,
     * allowing other plugins (both AETK-based and raw C SDK plugins) to acquire and
     * call your plugin's exported procedures.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automates the multi-step
     * `SPSuitesSuite::AddSuite` call, managing transient references to the suites suite
     * cleanly behind the scenes.
     *
     * @warning <b>Memory & Lifecycles:</b> The `implementation` pointer must remain valid
     * and allocated for the entire lifetime of the host process. Typically, this points
     * to a static structure. The helper internally relies on `aetk::core::suite` to
     * safely manage the lifetime of `SPSuitesSuite`, which automatically decrements the
     * host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @tparam SuiteT The custom suite structure definition.
     * @param name The unique name string to identify this custom suite.
     * @param version The version number of the custom suite interface.
     * @param implementation A pointer to the static struct holding the function pointers.
     */
    template <typename SuiteT>
    void register_suite(const char* name, int32_t version, SuiteT* implementation) const {
        SPSuitesSuite* suites_suite
            = acquire_suite<SPSuitesSuite>(kSPSuitesSuite, kSPSuitesSuiteVersion);
        if (suites_suite) {
            SPSuiteRef suite_ref = nullptr;
            core::check_err(static_cast<PF_Err>(suites_suite->AddSuite(
                kSPRuntimeSuiteList, 0, name, version, 1, implementation, &suite_ref)));
            release_suite<SPSuitesSuite>(kSPSuitesSuite, kSPSuitesSuiteVersion);
        }
    }

private:
    AEGP_PluginID m_plugin_id;
};

/**
 * @brief Base CRTP class template representing an After Effects General Plugin (AEGP).
 *
 * @details Implements the standard AEGP initialization entry point using the Curiously
 * Recurring Template Pattern (CRTP). It manages transition boundaries from C-linkage
 * callback conventions to object-oriented modern C++ exceptions and context models.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Replaces the raw global setup entry point function
 * (`EntryPointFunc`) with a declarative template. Handles all unhandled AETK exceptions
 * at the C-boundary, translating them back to standard `A_Err` codes to prevent host
 * crashes.
 *
 * @warning <b>Memory & Lifecycles:</b> This class template manages transition boundaries
 * from C-linkage callback conventions to object-oriented modern C++ contexts. The entry
 * context `aetk::aegp::entry_context` is created and managed safely on the stack during
 * the `EntryPointFunc` execution.
 *
 * @tparam T The derived plugin class defining the `static void on_entry(const
 * entry_context&)` hook.
 */
template <typename T> class plugin {
public:
    /**
     * @brief Virtual destructor.
     *
     * @details Enforces virtual cleanup behaviors for AEGP plugin subclasses.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Adds standard modern OOP polymorphic cleanup
     * capabilities.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    virtual ~plugin() = default;

    /**
     * @brief Main static effect entry point.
     *
     * @details Standard entry execution layer that translates raw C entry calls into
     * modern CRTP subclass dispatches.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Acts as the primary boundary manager,
     * transforming raw stack initializations into structured context wrappers and
     * managing C++ exceptions safely.
     *
     * @warning <b>Memory & Lifecycles:</b> Stack context is created and owned during the
     * lifetime of this call.
     *
     * @param pica_basicP The core basic suite pointer.
     * @param major_versionL Major version number.
     * @param minor_versionL Minor version number.
     * @param aegp_plugin_id Dynamic plugin identifier.
     * @param global_refconP Plugin global reference storage pointer.
     * @return Raw C status code (A_Err).
     */
    static A_Err effect_main(struct SPBasicSuite* pica_basicP, A_long major_versionL,
        A_long minor_versionL, AEGP_PluginID aegp_plugin_id,
        AEGP_GlobalRefcon* global_refconP) {
        try {
            ::aetk::core::context::init(pica_basicP, aegp_plugin_id);
            ::aetk::core::context::set_is_premiere(false);
            entry_context ctx(pica_basicP, aegp_plugin_id);
            T::on_entry(ctx);
            return A_Err_NONE;
        } catch (const core::exception& e) {
            AETK_ERROR("[AEGP Entry] Exception: {}", e.what());
            return static_cast<A_Err>(e.code());
        } catch (const std::exception& e) {
            AETK_ERROR("[AEGP Entry] std::exception: {}", e.what());
            return A_Err_GENERIC;
        }
    }
};

} // namespace aetk::aegp

#ifdef _WIN32
/**
 * @brief Macro to define dll symbol exports on Windows.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Standardizes symbol export attribute syntax across
 * Windows (MSVC) and macOS/Linux (Clang/GCC).
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
#define AETK_EXPORT __declspec(dllexport)
#else
/**
 * @brief Macro to define dll symbol exports on GCC/Clang platforms.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Standardizes symbol export attribute syntax across
 * Windows (MSVC) and macOS/Linux (Clang/GCC).
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
#define AETK_EXPORT __attribute__((visibility("default")))
#endif

/**
 * @brief Macro to define the AEGP entry point.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Declares and implements the raw `EntryPointFunc` C
 * entry point required by After Effects, passing the call to the modern
 * `plugin::effect_main` template.
 *
 * @warning <b>Memory & Lifecycles:</b> The macro wraps the main `EntryPointFunc`
 * execution path, which initializes the global stack context and sets up the PICA suites.
 * The entry context `aetk::aegp::entry_context` is created and managed safely on the
 * stack during this initialization phase.
 */
#define AETK_AEGP_MAIN(PLUGIN_CLASS)                                                     \
    extern "C" AETK_EXPORT A_Err EntryPointFunc(struct SPBasicSuite* pica_basicP,        \
        A_long major_versionL, A_long minor_versionL, AEGP_PluginID aegp_plugin_id,      \
        AEGP_GlobalRefcon* global_refconP) {                                             \
        return PLUGIN_CLASS::effect_main(pica_basicP, major_versionL, minor_versionL,    \
            aegp_plugin_id, global_refconP);                                             \
    }

/**
 * @brief Helper macro to execute modern logic and return a PICA-compatible A_Err.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Simplifies executing code inside a try-catch block,
 * translating any exception to a raw `A_Err` or `PF_Err` code.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
#define AETK_EXECUTE(CODE)                                                               \
    try {                                                                                \
        CODE;                                                                            \
        return A_Err_NONE;                                                               \
    } catch (const aetk::core::exception& e) {                                           \
        AETK_ERROR("[AEGP Execute] Exception: {}", e.what());                            \
        return static_cast<A_Err>(e.code());                                             \
    } catch (const std::exception& e) {                                                  \
        AETK_ERROR("[AEGP Execute] std::exception: {}", e.what());                       \
        return A_Err_GENERIC;                                                            \
    }
