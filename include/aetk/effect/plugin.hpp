#pragma once

#include <AE_Effect.h>
#include <aetk/core/error.hpp>
#include <aetk/core/log.hpp>
#include <aetk/effect/comp_ui.hpp>
#include <aetk/effect/context/audio_context.hpp>
#include <aetk/effect/context/context.hpp>
#include <aetk/effect/licensing.hpp>
#include <aetk/effect/params/dependencies.hpp>
#include <aetk/effect/params/param_callbacks.hpp>
#include <aetk/effect/ui/registration.hpp>

#ifndef AETK_OUT_FLAGS
#define AETK_OUT_FLAGS 0
#endif

#ifndef AETK_OUT_FLAGS2
#define AETK_OUT_FLAGS2 0
#endif

#ifndef AETK_CODE_VERSION
#define AETK_CODE_VERSION 0
#endif

namespace aetk::effect {

template <typename Class>
concept has_on_smart_render_with_bool = requires(const smart_render_context& ctx,
    bool is_gpu) { Class::on_smart_render(ctx, is_gpu); };

/**
 * @brief Base class for After Effects effect plugins.
 *
 * @details Uses CRTP (Curiously Recurring Template Pattern) to provide a static
 * entry point with automatic dispatch and Zero-Return error handling.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, `EffectMain` is a
 * monolithic C switch-case block returning `PF_Err` statuses directly. Errors
 * are difficult to propagate exception-safely, and boilerplate is duplicated
 * for every command hook. `aetk::effect::plugin` leverages CRTP to automate
 * host setup, initialize the `SPBasicSuite` context safely, wrap parameter
 * index structures, and translate C++ runtime exceptions into host status codes
 * automatically.
 *
 * @warning <b>Memory & Lifecycles:</b> The CRTP base class maintains no static
 * state itself (ensuring MFR safety), but instantiates a stack-allocated
 * `context` on each call. If an exception occurs, the framework intercepts it
 * and returns standard `PF_Err` statuses cleanly, preventing host-side crashes.
 *
 * @tparam T The derived plugin class.
 */
template <typename T> class plugin {
public:
    /**
     * @brief The primary entry point called by the After Effects host.
     *
     * @details Intercepts raw `EffectMain` commands, sets up the PICA basic suite
     * context registry, dispatches to specific overridden handlers, and wraps
     * exceptions.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Elevates raw C status codes and switch
     * commands into structured, type-safe C++ handler dispatches.
     *
     * @warning <b>Memory & Lifecycles:</b> `in_data` and `out_data` are
     * stack-allocated by AE and become stale after the callback returns. The
     * framework guarantees that the local `context` wraps these parameters only
     * during the call scope. Wraps the raw `EntryPointFunc` signature.
     *
     * @param cmd Standard host command selector.
     * @param in_data Input struct parameter from AE.
     * @param out_data Output struct parameter for AE.
     * @param params Host parameter array pointers.
     * @param output Output pixel world surface.
     * @param extra Custom parameter structures based on cmd.
     * @return `PF_Err` status code.
     */
    static PF_Err effect_main(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data,
        PF_ParamDef* params[], PF_LayerDef* output, void* extra) {
        constexpr auto is_base_smart_render =
            []<typename F>(F f, void (*base_f)(const smart_render_context&)) {
                if constexpr (std::is_same_v<F, void (*)(const smart_render_context&)>) {
                    return f == base_f;
                } else {
                    return false;
                }
            };

        constexpr auto is_base_pre_render
            = []<typename F>(F f, void (*base_f)(const pre_render_context&)) {
                  if constexpr (std::is_same_v<F, void (*)(const pre_render_context&)>) {
                      return f == base_f;
                  } else {
                      return false;
                  }
              };

        constexpr bool overrides_smart_render
            = !is_base_smart_render(&T::on_smart_render, &plugin<T>::on_smart_render);
        constexpr bool overrides_smart_render_gpu = !is_base_smart_render(
            &T::on_smart_render_gpu, &plugin<T>::on_smart_render_gpu);
        constexpr bool overrides_pre_render
            = !is_base_pre_render(&T::on_pre_render, &plugin<T>::on_pre_render);

        constexpr bool is_smart_plugin = overrides_smart_render
            || overrides_smart_render_gpu || has_on_smart_render_with_bool<T>;

        static_assert(!is_smart_plugin || overrides_pre_render,
            "AETK ERROR: SmartFX plugins must override on_pre_render(const "
            "pre_render_context&) to initialize max_result_rect.");

        if (!in_data || !out_data)
            return PF_Err_INTERNAL_STRUCT_DAMAGED;

        try {
            if (in_data->pica_basicP) {
                ::aetk::core::context::init(in_data->pica_basicP, 0);
                ::aetk::core::context::set_is_premiere(in_data->appl_id == 'PrMr');
            }

            context ctx(cmd, in_data, out_data, params, output, extra);
            ctx.m_index_lookup = [](const char* name) {
                auto it = param_callback_registry<T>::index_by_key.find(name);
                if (it != param_callback_registry<T>::index_by_key.end()) {
                    return it->second;
                }
                auto it_name = param_callback_registry<T>::index_by_name.find(name);
                return (it_name != param_callback_registry<T>::index_by_name.end())
                    ? it_name->second
                    : -1;
            };
            ctx.m_int_index_lookup = [](int32_t key) {
                auto it = param_callback_registry<T>::index_by_int_key.find(key);
                return (it != param_callback_registry<T>::index_by_int_key.end())
                    ? it->second
                    : -1;
            };

            switch (cmd) {
            case PF_Cmd_ABOUT:
                T::on_about(ctx);
                break;
            case PF_Cmd_GLOBAL_SETUP:
                T::on_global_setup(global_setup_context(ctx));
                break;
            case PF_Cmd_GLOBAL_SETDOWN:
                T::on_global_setdown(ctx);
                break;
            case PF_Cmd_PARAMS_SETUP: {
                ctx.out_data_ptr()->num_params = 1; // Start at 1 (input layer)
                if (out_data) {
                    out_data->out_flags |= AETK_OUT_FLAGS;
                    out_data->out_flags2 |= AETK_OUT_FLAGS2;
                }
                params_setup_context pctx(ctx,
                    param_callback_registry<T>::register_callback,
                    param_callback_registry<T>::register_key,
                    param_callback_registry<T>::register_int_key);
                T::on_params_setup(pctx);
                if ((ctx.out_data_ptr()->out_flags & PF_OutFlag_CUSTOM_UI) && !pctx.custom_ui_registered) {
                    PF_CustomUIInfo ci;
                    AEFX_CLR_STRUCT(ci);
                    ci.events = PF_CustomEFlag_EFFECT;
                    // Ignore error since some hosts might not support it (e.g. Premiere
                    // Pro)
                    PF_REGISTER_UI(in_data, &ci);
                }
                break;
            }
            case PF_Cmd_FRAME_SETUP:
                T::on_frame_setup(frame_setup_context(ctx));
                break;
            case PF_Cmd_SEQUENCE_SETUP:
                T::on_sequence_setup(sequence_setup_context(ctx));
                break;
            case PF_Cmd_SEQUENCE_RESETUP:
                T::on_sequence_resetup(sequence_setup_context(ctx));
                break;
            case PF_Cmd_SEQUENCE_SETDOWN:
                T::on_sequence_setdown(sequence_setdown_context(ctx));
                break;
            case PF_Cmd_SEQUENCE_FLATTEN:
                T::on_sequence_flatten(flattened_sequence_context(ctx));
                break;
            case PF_Cmd_GET_FLATTENED_SEQUENCE_DATA:
                T::on_get_flattened_sequence_data(flattened_sequence_context(ctx));
                break;
            case PF_Cmd_RENDER:
                if (in_data && output && params && params[0]) {
                    if (in_data->utils && in_data->utils->copy) {
                        in_data->utils->copy(in_data->effect_ref, &params[0]->u.ld, output, NULL, NULL);
                    }
                }
                T::on_render(ctx);
                break;
#ifndef AETK_PREMIERE_COMPAT
            case PF_Cmd_SMART_PRE_RENDER:
                T::on_pre_render(pre_render_context(ctx));
                break;
            case PF_Cmd_SMART_RENDER:
                if constexpr (has_on_smart_render_with_bool<T>) {
                    T::on_smart_render(smart_render_context(
                                           ctx, static_cast<PF_SmartRenderExtra*>(extra)),
                        false);
                } else {
                    T::on_smart_render(smart_render_context(
                        ctx, static_cast<PF_SmartRenderExtra*>(extra)));
                }
                break;
            case PF_Cmd_SMART_RENDER_GPU:
                if constexpr (has_on_smart_render_with_bool<T>) {
                    T::on_smart_render(smart_render_context(
                                           ctx, static_cast<PF_SmartRenderExtra*>(extra)),
                        true);
                } else {
                    T::on_smart_render_gpu(smart_render_context(
                        ctx, static_cast<PF_SmartRenderExtra*>(extra)));
                }
                break;
#endif
            case PF_Cmd_GPU_DEVICE_SETUP:
                T::on_gpu_device_setup(gpu_device_setup_context(ctx));
                break;
            case PF_Cmd_GPU_DEVICE_SETDOWN:
                T::on_gpu_device_setdown(gpu_device_setdown_context(ctx));
                break;
            case PF_Cmd_ARBITRARY_CALLBACK:
                return arb_data_registry::invoke(
                    in_data, out_data, static_cast<PF_ArbParamsExtra*>(extra));
            case PF_Cmd_EVENT: {
                interaction_context ictx(ctx);
                if (ictx.window() == interaction_context::window_type::comp
                    || ictx.window() == interaction_context::window_type::layer) {
                    ::aetk::effect::comp_ui::context octx(ictx);
                    T::on_comp_ui(octx);
                } else {
                    ::aetk::effect::ui::widget_registry::dispatch(ictx);
                    T::on_event(ictx);
                }
                break;
            }
            case PF_Cmd_USER_CHANGED_PARAM: {
                user_changed_param_context uctx(
                    ctx, static_cast<PF_UserChangedParamExtra*>(extra), params);
                param_callback_registry<T>::invoke(uctx.param_index(), uctx);
                T::on_user_changed_param(uctx);
                break;
            }
            case PF_Cmd_UPDATE_PARAMS_UI: {
                ui_update_context uctx(ctx);
                T::on_ui_update(uctx);
                ::aetk::effect::ui::widget_registry::refresh_registered_widgets(uctx);
                break;
            }
            case PF_Cmd_DO_DIALOG:
                T::on_do_dialog(ctx);
                break;
            case PF_Cmd_GET_EXTERNAL_DEPENDENCIES: {
                dependency_context dctx(
                    in_data, static_cast<PF_ExtDependenciesExtra*>(extra));
                T::on_get_dependencies(dctx);
                break;
            }
            case PF_Cmd_QUERY_DYNAMIC_FLAGS: {
                T::on_query_dynamic_flags(query_dynamic_flags_context(ctx));
                break;
            }
            case PF_Cmd_AUDIO_SETUP: {
                audio_setup_context actx(in_data, out_data, params);
                T::on_audio_setup(actx);
                break;
            }
            case PF_Cmd_AUDIO_RENDER: {
                audio_render_context actx(in_data, out_data, params, output);
                T::on_audio_render(actx);
                break;
            }
            case PF_Cmd_AUDIO_SETDOWN: {
                audio_setdown_context actx(in_data, out_data);
                T::on_audio_setdown(actx);
                break;
            }
            case PF_Cmd_RESERVED3: {
                T::on_reserved3(ctx);
                break;
            }
            case PF_Cmd_COMPLETELY_GENERAL: {
                completely_general_context cctx(ctx, extra);
                T::on_completely_general(cctx);
                break;
            }
            default:
                break;
            }
            return PF_Err_NONE;
        } catch (const ::aetk::core::exception& e) {
            if (e.code() == PF_Interrupt_CANCEL || e.code() == 10007) {
                AETK_DEBUG(
                    "[EffectMain] Render cancelled or interrupted (code: {})", e.code());
                return e.code();
            }
            if (e.code() == PF_Err_INTERNAL_STRUCT_DAMAGED) {
                AETK_ERROR("[EffectMain] Internal Struct Damaged, returning as Out of "
                           "Memory: {}",
                    e.what());
                return PF_Err_OUT_OF_MEMORY;
            }
            AETK_ERROR("[EffectMain] aetk::core::exception: {}", e.what());
            return e.code();
        } catch (const std::exception& e) {
            AETK_ERROR("[EffectMain] std::exception: {}", e.what());
            return PF_Err_OUT_OF_MEMORY;
        }
    }

    /**
     * @brief Handler hook for PF_Cmd_ABOUT.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Wrapper context representing current call state.
     */
    static void on_about(const context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_GLOBAL_SETUP.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Setup context wrappers.
     */
    static void on_global_setup(const global_setup_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_GLOBAL_SETDOWN.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Wrapper context representing current call state.
     */
    static void on_global_setdown(const context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_PARAMS_SETUP.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Setup context wrappers.
     */
    static void on_params_setup(const params_setup_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_SEQUENCE_SETUP.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Setup context wrappers.
     */
    static void on_sequence_setup(const sequence_setup_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_SEQUENCE_RESETUP.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Setup context wrappers.
     */
    static void on_sequence_resetup(const sequence_setup_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_SEQUENCE_SETDOWN.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Setup context wrappers.
     */
    static void on_sequence_setdown(const sequence_setdown_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_SEQUENCE_FLATTEN.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Setup context wrappers.
     */
    static void on_sequence_flatten(const flattened_sequence_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_GET_FLATTENED_SEQUENCE_DATA.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Setup context wrappers.
     */
    static void on_get_flattened_sequence_data(const flattened_sequence_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_RENDER.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Wrapper context representing current call state.
     */
    static void on_render(const context& ctx) {
        if constexpr (has_on_smart_render_with_bool<T>) {
            T::on_smart_render(smart_render_context(ctx), false);
        } else {
            T::on_smart_render(smart_render_context(ctx));
        }
    }

    /**
     * @brief Handler hook for PF_Cmd_SMART_PRE_RENDER.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Setup context wrappers.
     */
    static void on_pre_render(const pre_render_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_SMART_RENDER.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Setup context wrappers.
     */
    static void on_smart_render(const smart_render_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_SMART_RENDER_GPU.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Setup context wrappers.
     */
    static void on_smart_render_gpu(const smart_render_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_GPU_DEVICE_SETUP.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Setup context wrappers.
     */
    static void on_gpu_device_setup(const gpu_device_setup_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_GPU_DEVICE_SETDOWN.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Setup context wrappers.
     */
    static void on_gpu_device_setdown(const gpu_device_setdown_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_EVENT.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Setup context wrappers.
     */
    static void on_event(const interaction_context& ctx) {
    }

    /**
     * @brief Handler hook for Composition (PF_Window_COMP) and Layer (PF_Window_LAYER) Custom UI events.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation for Comp/Layer viewport UI overlay callbacks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Comp UI context wrapper containing coordinate transformations and overlay tools.
     */
    static void on_comp_ui(const ::aetk::effect::comp_ui::context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_USER_CHANGED_PARAM.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Setup context wrappers.
     */
    static void on_user_changed_param(const user_changed_param_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_UPDATE_PARAMS_UI.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Setup context wrappers.
     */
    static void on_ui_update(const ui_update_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_DO_DIALOG.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Wrapper context representing current call state.
     */
    static void on_do_dialog(const context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_GET_EXTERNAL_DEPENDENCIES.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Setup context wrappers.
     */
    static void on_get_dependencies(dependency_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_QUERY_DYNAMIC_FLAGS.
     */
    static void on_query_dynamic_flags(const query_dynamic_flags_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_RESERVED3.
     */
    static void on_reserved3(const context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_COMPLETELY_GENERAL.
     */
    static void on_completely_general(completely_general_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_FRAME_SETUP.
     */
    static void on_frame_setup(const frame_setup_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_AUDIO_SETUP.
     */
    static void on_audio_setup(audio_setup_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_AUDIO_RENDER.
     */
    static void on_audio_render(audio_render_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_AUDIO_SETDOWN.
     */
    static void on_audio_setdown(audio_setdown_context& ctx) {
    }

    /**
     * @brief Handler hook for PF_Cmd_ARBITRARY_CALLBACK.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP encapsulation of classical C
     * command hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Wrapper context representing current call state.
     * @param extra Custom parameter structures based on cmd.
     * @return `PF_Err` status code.
     */
};

// ============================================================
//  Entry Point Macro
// ============================================================

#ifdef _WIN32
#define AETK_EXPORT __declspec(dllexport)
#else
#define AETK_EXPORT __attribute__((visibility("default")))
#endif

/**
 * @brief Macro defining standard C Entry Point linkage EffectMain.
 *
 * @details Automates initial version setup, flag assignments, and dispatches to
 * plugin CRTP context frameworks.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Replaces manual DLL export setups and
 * default flag initialization boilerplates with an elegant one-liner.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
#define AETK_EFFECT_MAIN(PLUGIN_CLASS)                                                   \
    extern "C" AETK_EXPORT PF_Err EffectMain(PF_Cmd cmd, PF_InData* in_data,             \
        PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output, void* extra) { \
        if (cmd == PF_Cmd_GLOBAL_SETUP && out_data) {                                    \
            out_data->my_version = AETK_CODE_VERSION;                                    \
            out_data->out_flags |= AETK_OUT_FLAGS;                                       \
            out_data->out_flags2 |= AETK_OUT_FLAGS2;                                     \
            aetk::effect::licensing::setup_ui_button(in_data);                           \
        }                                                                                \
        if (PF_Err err = aetk::effect::licensing::check_and_intercept(cmd, in_data, out_data)) { \
            return err;                                                                  \
        }                                                                                \
        return PLUGIN_CLASS::effect_main(cmd, in_data, out_data, params, output, extra); \
    }

} // namespace aetk::effect
