#pragma once

#include <aetk/core/log.hpp>
#include <aetk/core/suite.hpp>
#include <functional>
#include <unordered_map>
#include <string>

namespace aetk::aegp {

// ============================================================
//  Menu location constants
// ============================================================

namespace menu {
    constexpr AEGP_MenuID window    = AEGP_Menu_WINDOW;
    constexpr AEGP_MenuID file      = AEGP_Menu_FILE;
    constexpr AEGP_MenuID edit      = AEGP_Menu_EDIT;
    constexpr AEGP_MenuID comp      = AEGP_Menu_COMPOSITION;
    constexpr AEGP_MenuID layer     = AEGP_Menu_LAYER;
    constexpr AEGP_MenuID effect    = AEGP_Menu_EFFECT;
    constexpr AEGP_MenuID animation = AEGP_Menu_ANIMATION;
}

namespace hook_priority {
    constexpr AEGP_HookPriority before_ae = AEGP_HP_BeforeAE;
    constexpr AEGP_HookPriority after_ae  = AEGP_HP_AfterAE;
}

// ============================================================
//  Command options (Layer 2.5 — structured customization)
// ============================================================

/**
 * @brief Options configuration structure for custom AEGP menu commands.
 * 
 * @details Encapsulates high-level callbacks and registration parameters for adding 
 * native After Effects menu items. It allows declarative setup using C++20 designated 
 * initializers.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, custom menu items are registered through 
 * a complex sequence of suite calls (`AEGP_GetUniqueCommand` and `AEGP_InsertMenuCommand`) 
 * coupled with a global, monolithic command hook dispatcher callback. This structure 
 * and its associated `add_command` helpers abstract this entire process into scoped, 
 * localized C++ `std::function` closures.
 *
 * @warning <b>Memory & Lifecycles:</b> All callback functions (`on_execute` and `on_can_enable`) 
 * are stored inside a global, persistent registry. If you capture pointers or local variables 
 * by reference within these lambdas, ensure those captured objects remain allocated for the 
 * entire duration of the host process's lifecycle. Callback lambdas wrap the raw `AEGP_CommandHook` 
 * and `AEGP_UpdateMenuHook` typedef interfaces.
 */
struct command_options {
    /// Called when the user clicks the menu item.
    std::function<void()> on_execute;

    /// Called during menu update. Return true to enable, false to grey out.
    /// If null, the command is always enabled.
    std::function<bool()> on_can_enable = nullptr;

    /// Hook priority relative to After Effects' own handling.
    AEGP_HookPriority priority = AEGP_HP_BeforeAE;

    /// Menu insertion position. Use AEGP_MENU_INSERT_SORTED for alphabetical.
    A_long insert_after = AEGP_MENU_INSERT_SORTED;
};

// ============================================================
//  Command registry (internal — manages the dispatch table)
// ============================================================

namespace detail {

    /**
     * @brief Internal command entry details.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Wraps raw AEGP command configuration properties inside a modern structured class.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    struct command_entry {
        std::function<void()> on_execute;
        std::function<bool()> on_can_enable;
        AEGP_Command          id;
    };

    /**
     * @brief Singleton registry that maps AEGP_Command IDs to callbacks.
     * 
     * @details This is the "engine" behind add_command(). It registers a single
     * global CommandHook and UpdateMenuHook with AE, then dispatches
     * to the appropriate lambda when a command fires.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Provides a centralized database to hold command entries dynamically rather than registering multiple C hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> Returns a reference to a static map. Memory is persisted across the entire process lifetime. Entries should only be written from the main thread during initialization.
     */
    inline std::unordered_map<AEGP_Command, command_entry>& get_commands() {
        static std::unordered_map<AEGP_Command, command_entry> commands;
        return commands;
    }

    inline bool hooks_registered = false;

    // --- Raw C callbacks that AE calls ---

    /**
     * @brief Command hook dispatcher for After Effects menu events.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Adapts raw `AEGP_CommandHook` signature callbacks into type-safe modern C++ lambdas.
     *
     * @warning <b>Memory & Lifecycles:</b> Executed on the primary UI thread. Catches all exceptions to prevent them from propagating past the C boundary. Custom logic relies on global commands.
     */
    inline A_Err command_hook_dispatch(
        AEGP_GlobalRefcon   /*plugin_refconPV*/,
        AEGP_CommandRefcon  /*refconPV*/,
        AEGP_Command        command,
        AEGP_HookPriority   /*hook_priority*/,
        A_Boolean           /*already_handledB*/,
        A_Boolean*          handledPB)
    {
        auto& cmds = get_commands();
        auto it = cmds.find(command);
        if (it != cmds.end()) {
            try {
                it->second.on_execute();
            } catch (const std::exception& e) {
                AETK_ERROR("[AEGP Command] Exception during execute callback: {}", e.what());
            }
            *handledPB = TRUE;
        }
        return A_Err_NONE;
    }

    /**
     * @brief Menu update hook dispatcher for dynamic enablement status.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Adapts raw `AEGP_UpdateMenuHook` signature callbacks into modular lambda update functions.
     *
     * @warning <b>Memory & Lifecycles:</b> Executed on the primary UI thread during After Effects menu rendering. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope, avoiding memory leaks on `AEGP_CommandSuite1` checkouts.
     */
    inline A_Err update_menu_hook_dispatch(
        AEGP_GlobalRefcon       /*plugin_refconPV*/,
        AEGP_UpdateMenuRefcon   /*refconPV*/,
        AEGP_WindowType         /*active_window*/)
    {
        using cmd_suite = aetk::core::suite<AEGP_CommandSuite1,
            aetk::core::fixed_string(kAEGPCommandSuite), kAEGPCommandSuiteVersion1>;

        for (auto& [id, entry] : get_commands()) {
            bool enabled = true;
            if (entry.on_can_enable) {
                try {
                    enabled = entry.on_can_enable();
                } catch (const std::exception& e) {
                    AETK_ERROR("[AEGP Command] Exception during can_enable callback: {}", e.what());
                    enabled = false;
                }
            }
            if (enabled) {
                cmd_suite::call<&AEGP_CommandSuite1::AEGP_EnableCommand>(id);
            } else {
                cmd_suite::call<&AEGP_CommandSuite1::AEGP_DisableCommand>(id);
            }
        }
        return A_Err_NONE;
    }

    /**
     * @brief Registers the global hooks with AE (once).
     * Called automatically by add_command().
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standardizes the registration of both `AEGP_RegisterCommandHook` and `AEGP_RegisterUpdateMenuHook` dynamically.
     *
     * @warning <b>Memory & Lifecycles:</b> Invokes `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope. Must only be run on the UI thread.
     */
    inline void ensure_hooks_registered() {
        if (hooks_registered) return;

        using reg_suite = aetk::core::suite<AEGP_RegisterSuite5,
            aetk::core::fixed_string(kAEGPRegisterSuite), kAEGPRegisterSuiteVersion5>;

        auto pid = aetk::core::context::get_plugin_id();

        reg_suite::call<&AEGP_RegisterSuite5::AEGP_RegisterCommandHook>(
            pid, AEGP_HP_BeforeAE, AEGP_Command_ALL,
            command_hook_dispatch,
            reinterpret_cast<AEGP_CommandRefcon>(0));

        reg_suite::call<&AEGP_RegisterSuite5::AEGP_RegisterUpdateMenuHook>(
            pid, update_menu_hook_dispatch,
            reinterpret_cast<AEGP_UpdateMenuRefcon>(0));

        hooks_registered = true;
    }

} // namespace detail

// ============================================================
//  Public API — add_command()
// ============================================================

/**
 * @brief Registers a menu command with rich behavioral customization.
 * 
 * @details Allocates a globally unique command ID from the host application, inserts 
 * the custom menu item into the designated After Effects menu bar hierarchy, and automatically 
 * hooks it into a safe global dispatcher.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Bypasses the need to write custom boilerplate handlers 
 * inside a monolithic `AEGP_CommandHook` callback. Instead of manually inspecting `command` 
 * integers inside a giant switch block, the developer registers localized execution and 
 * update hooks per menu item.
 *
 * @warning <b>Memory & Lifecycles:</b> Menu items are created during the AEGP initialization phase. 
 * Invoking `add_command` dynamically at render time is illegal and will cause host-side errors.
 * Internally uses `aetk::core::suite` which automatically decrements the host reference count 
 * via `ReleaseSuite` when it goes out of scope to safely load `AEGP_CommandSuite1`.
 *
 * @param name The human-readable string displayed in the menu bar.
 * @param menu_id The parent menu identifier (e.g. `aetk::aegp::menu::window`).
 * @param opts Configuration options for hook routing and enable conditions.
 * @return The raw allocated `AEGP_Command` identifier.
 */
inline AEGP_Command add_command(const char* name, AEGP_MenuID menu_id,
                                command_options opts)
{
    using cmd_suite = aetk::core::suite<AEGP_CommandSuite1,
        aetk::core::fixed_string(kAEGPCommandSuite), kAEGPCommandSuiteVersion1>;

    detail::ensure_hooks_registered();

    AEGP_Command cmd = 0;
    cmd_suite::call<&AEGP_CommandSuite1::AEGP_GetUniqueCommand>(&cmd);
    cmd_suite::call<&AEGP_CommandSuite1::AEGP_InsertMenuCommand>(
        cmd, name, menu_id, opts.insert_after);

    detail::get_commands()[cmd] = detail::command_entry{
        .on_execute    = std::move(opts.on_execute),
        .on_can_enable = std::move(opts.on_can_enable),
        .id            = cmd,
    };

    return cmd;
}

/**
 * @brief Registers a menu command with a simple, direct execution callback.
 * 
 * @details An ergonomic overload of `add_command` for menu items that are always 
 * active and do not require custom enabling logic or specific hook priorities.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Provides the ultimate short-hand wrapper for simple 
 * actions (like launching a dialog window or toggling a global plugin mode).
 *
 * @warning <b>Memory & Lifecycles:</b> Menu items must only be registered during the main initialization thread. 
 * Relies on the standard `add_command` delegation, which internally uses `aetk::core::suite` 
 * to acquire and release `AEGP_CommandSuite1` automatically upon scope exit.
 *
 * @param name The human-readable string displayed in the menu bar.
 * @param menu_id The parent menu identifier.
 * @param callback The function/lambda to execute when the menu item is clicked.
 * @return The raw allocated `AEGP_Command` identifier.
 */
inline AEGP_Command add_command(const char* name, AEGP_MenuID menu_id,
                                std::function<void()> callback)
{
    return add_command(name, menu_id, command_options{
        .on_execute = std::move(callback),
    });
}

} // namespace aetk::aegp
