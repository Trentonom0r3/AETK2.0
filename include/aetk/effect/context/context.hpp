#pragma once

#include <AE_Effect.h>
#include <AE_EffectCB.h>
#include <AE_EffectCBSuites.h>
#include <AE_EffectGPUSuites.h>
#include <AE_EffectPixelFormat.h>
#include <AE_EffectUI.h>
#include <AE_GeneralPlug.h>
#include <aetk/core/error.hpp>
#include <aetk/core/hash.hpp>
#include <aetk/core/log.hpp>
#include <aetk/core/premiere_compat.hpp>
#include <aetk/core/suite.hpp>
#include <aetk/core/types.hpp>
#include <aetk/effect/gpu.hpp>
#include <aetk/effect/params/param.hpp>
#include <aetk/effect/params/param_callbacks.hpp>
#include <aetk/effect/params/param_modifier.hpp>
#include <aetk/effect/pixel/smart_world.hpp>
#include <aetk/ui/drawbot.hpp>
#include <functional>
#include <string_view>
#include <type_traits>

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

namespace aetk::effect {
using aetk::core::pixel_range;

template <typename T>
struct is_param_key : std::bool_constant<std::is_enum_v<T>
                          || std::is_convertible_v<T, std::string_view>> { };

/**
 * @brief Consolidates and wraps all host callback parameters safely.
 *
 * @details Consolides `in_data`, `out_data`, `params`, `output`, and `extra`
 * fields into a unified object, providing temporal parameter checkouts, suite
 * acquisitions, and high-performance pixel iterations.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw C SDK, callback parameters
 * (`in_data`, `out_data`, `params`, `output`) are passed as separate naked
 * arguments, making state-sharing and execution coordination difficult.
 * `aetk::effect::context` consolidates these structures into a unified,
 * exception-safe object, exposing clean abstractions for time mapping,
 * parameter checkouts, suite acquisitions, and high-performance pixel
 * iterations.
 *
 * @warning <b>Memory & Lifecycles:</b> The context does not own any underlying
 * After Effects handles or pointers. It acts as a lightweight stack-allocated
 * wrapper that must not outlive the lifespan of the host callback. The template
 * `iterate` wraps high-performance iteration suites which balance their
 * checkouts automatically.
 */
class context {
public:
    virtual ~context() = default;
    virtual bool is_gpu() const {
        return false;
    }

    /**
     * @brief Base context constructor.
     *
     * @details Consolidated stack parameters.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Wraps raw stack parameters.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param cmd Command code passed by AE.
     * @param in_data Input struct parameter.
     * @param out_data Output struct parameter.
     * @param params Host parameter array pointers.
     * @param output Output pixel world surface.
     * @param extra Custom parameter structures.
     */
    context(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[],
        PF_LayerDef* output, void* extra)
        : m_cmd(cmd)
        , m_in_data(in_data)
        , m_out_data(out_data)
        , m_params(params)
        , m_output(output)
        , m_extra(extra) {
    }

    /**
     * @brief Get active host command code.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Modern OOP representations of C
     * structures.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Underlying `PF_Cmd`.
     */
    PF_Cmd cmd() const {
        return m_cmd;
    }

    /**
     * @brief Get current timeline timestamp.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Modern OOP representations of C
     * structures.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Bounded time object.
     */
    core::time current_time() const {
        return { m_in_data->current_time, m_in_data->time_scale };
    }

    /**
     * @brief Get total timeline duration time.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Modern OOP representations of C
     * structures.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Bounded time object.
     */
    core::time total_time() const {
        return { m_in_data->total_time, m_in_data->time_scale };
    }

    /**
     * @brief Get step size time per frame.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Modern OOP representations of C
     * structures.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Bounded time object.
     */
    core::time time_step() const {
        return { m_in_data->time_step, m_in_data->time_scale };
    }

    /**
     * @brief Get raw sequence data handle.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Modern OOP representations of C
     * structures.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Underlying `PF_Handle`.
     */
    PF_Handle sequence_data() const {
        return m_in_data->sequence_data;
    }

    /**
     * @brief Get raw global data handle.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Modern OOP representations of C
     * structures.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Underlying `PF_Handle`.
     */
    PF_Handle global_data() const {
        return m_in_data->global_data;
    }

    /**
     * @brief Access and lock sequence data for this plugin instance.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw `host_lock_handle`
     * invocations.
     *
     * @warning <b>Memory & Lifecycles:</b> The caller is responsible for
     * unlocking this handle (which is normally managed by
     * `mutable_sequence_data`).
     *
     * @tparam T The dynamic sequence data type.
     * @return Typed sequence data pointer.
     */
    template <typename T> T* sequence_data_ptr() const {
        if (!m_in_data->sequence_data)
            return nullptr;
        return reinterpret_cast<T*>(
            (*m_in_data->utils->host_lock_handle)(m_in_data->sequence_data));
    }

    /**
     * @brief Get current frame number index.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe exposure of inner parameters.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Active frame index.
     */
    int32_t current_frame() const {
        return m_in_data->current_time / m_in_data->time_step;
    }

    /**
     * @brief Get total frame count.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe exposure of inner parameters.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Total frames.
     */
    int32_t total_frames() const {
        return m_in_data->total_time / m_in_data->time_step;
    }

    /**
     * @brief Get input layer width.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe exposure of inner parameters.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Width in pixels.
     */
    int32_t width() const {
        return m_in_data->width;
    }

    /**
     * @brief Get input layer height.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe exposure of inner parameters.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Height in pixels.
     */
    int32_t height() const {
        return m_in_data->height;
    }

    /**
     * @brief Detect whether the host application is Premiere Pro.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Premiere Pro uses different pixel
     * byte ordering (BGRA vs ARGB), lacks AEGP suites, and doesn't support
     * SmartFX. This helper enables host-aware branching at the plugin level.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return True if the host is Premiere Pro, false for After Effects.
     */
    bool is_premiere() const {
        return m_in_data && (m_in_data->appl_id == 'PrMr');
    }

    // Internal accessors
    PF_InData* in_data_ptr() const {
        return m_in_data;
    }
    PF_OutData* out_data_ptr() const {
        return m_out_data;
    }
    PF_ParamDef** params_ptr() const {
        return m_params;
    }

    /**
     * @brief Helper to acquire parameter modifiers.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Type-safe modifier instantiation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @tparam ParamT The dynamic parameter type.
     * @tparam KeyT Key type (const char*, std::string, std::string_view, or enum).
     * @param key Unique key identifier (enum or string).
     * @return Instantiated parameter modifier.
     */
    template <typename ParamT, typename KeyT,
        typename = std::enable_if_t<is_param_key<KeyT>::value>>
    ParamT param_modifier(KeyT key) const {
        int32_t index = -1;
        if constexpr (std::is_pointer_v<std::decay_t<KeyT>>
            && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<std::decay_t<KeyT>>>,
                char>) {
            if (m_index_lookup)
                index = m_index_lookup(key);
        } else if constexpr (std::is_same_v<std::decay_t<KeyT>, std::string>
            || std::is_same_v<std::decay_t<KeyT>, std::string_view>) {
            if (m_index_lookup)
                index = m_index_lookup(std::string(key).c_str());
        } else {
            if (m_int_index_lookup) {
                index = m_int_index_lookup(static_cast<int32_t>(key));
            }
            if (index < 0) {
                index = static_cast<int32_t>(key);
            }
        }
        return param_modifier_by_index<ParamT>(index);
    }

    PF_LayerDef* output_ptr() const {
        return m_output;
    }
    void* extra_ptr() const {
        return m_extra;
    }

    /**
     * @brief Access the output world (legacy render).
     *
     * @note <b>AE SDK Paradigm Shift:</b> OOP wrapper for raw drawing outputs.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Instantiated smart world surface.
     */
    smart_world output() const {
        return smart_world(m_output, m_in_data, smart_world::ownership::NONE);
    }

    /**
     * @brief Set the message displayed in the After Effects dialog.
     *
     * Used in on_do_dialog or on_about to present information to the user.
     * Automatically sets the PF_OutFlag_DISPLAY_ERROR_MESSAGE flag.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard error return message wrapper.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param msg Message string.
     */
    void set_dialog_response(const char* msg) const {
        if (m_in_data && m_out_data) {
            m_in_data->utils->ansi.sprintf(m_out_data->return_msg, msg);
            m_out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
        }
    }

    float scale_factor_x() const {
        if (!in_data_ptr() || in_data_ptr()->downsample_x.den == 0)
            return 1.0f;
        return static_cast<float>(in_data_ptr()->downsample_x.num)
            / static_cast<float>(in_data_ptr()->downsample_x.den);
    }

    float scale_factor_y() const {
        if (!in_data_ptr() || in_data_ptr()->downsample_y.den == 0)
            return 1.0f;
        return static_cast<float>(in_data_ptr()->downsample_y.num)
            / static_cast<float>(in_data_ptr()->downsample_y.den);
    }

    /**
     * @brief Dynamic index lookup callback.
     */
    int32_t (*m_index_lookup)(const char*) = nullptr;

    /**
     * @brief Dynamic integer/enum index lookup callback.
     */
    int32_t (*m_int_index_lookup)(int32_t) = nullptr;

    /**
     * @brief Parameter access by name, custom string key, or enum
     * identifier. Eliminates the need for hardcoded indices or index enums.
     *
     * @tparam ParamT The target parameter wrapper type.
     * @tparam KeyT Key type (const char*, std::string, std::string_view, or enum).
     * @param key Unique key identifier or display name.
     * @return Bounded parameter wrapper instance.
     */
    template <typename ParamT, typename KeyT,
        typename = std::enable_if_t<is_param_key<KeyT>::value>>
    ParamT param(KeyT key) const {
        if constexpr (std::is_pointer_v<std::decay_t<KeyT>>
            && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<std::decay_t<KeyT>>>,
                char>) {
            if (!m_index_lookup)
                throw core::exception(
                    PF_Err_BAD_CALLBACK_PARAM, "Index lookup not available");
            int32_t index = m_index_lookup(key);
            if (index < 0) {
                throw core::exception(PF_Err_BAD_CALLBACK_PARAM,
                    std::string("Parameter not found: ") + key);
            }
            return param_by_index<ParamT>(index);
        } else if constexpr (std::is_same_v<std::decay_t<KeyT>, std::string>
            || std::is_same_v<std::decay_t<KeyT>, std::string_view>) {
            if (!m_index_lookup)
                throw core::exception(
                    PF_Err_BAD_CALLBACK_PARAM, "Index lookup not available");
            int32_t index = m_index_lookup(std::string(key).c_str());
            if (index < 0) {
                throw core::exception(PF_Err_BAD_CALLBACK_PARAM,
                    std::string("Parameter not found: ") + std::string(key));
            }
            return param_by_index<ParamT>(index);
        } else {
            int32_t index = -1;
            if (m_int_index_lookup) {
                index = m_int_index_lookup(static_cast<int32_t>(key));
            }
            if (index < 0) {
                index = static_cast<int32_t>(key);
            }
            return param_by_index<ParamT>(index);
        }
    }

    /**
     * @brief Access the default input layer parameter (which resides at index 0).
     *
     * @tparam ParamT The target parameter wrapper type (defaults to layer_param).
     * @return The input layer parameter wrapper instance.
     */
    template <typename ParamT = layer_param> ParamT input_param() const {
        return param_by_index<ParamT>(0);
    }

    /**
     * @brief Value retrieval helper for float sliders.
     */
    template <typename KeyT, typename = std::enable_if_t<is_param_key<KeyT>::value>>
    double float_val(KeyT key) const {
        auto p = param<param_base>(key);
        if (p.valid() && p.def_ptr()) {
            if (p.type() == PF_Param_FLOAT_SLIDER) {
                return p.def_ptr()->u.fs_d.value;
            }
        }
        return 0.0;
    }

    /**
     * @brief Value retrieval helper for sliders and popups (handles type
     * dynamically).
     */
    template <typename KeyT, typename = std::enable_if_t<is_param_key<KeyT>::value>>
    int32_t int_val(KeyT key) const {
        auto p = param<param_base>(key);
        if (p.valid() && p.def_ptr()) {
            if (p.type() == PF_Param_POPUP) {
                return static_cast<int32_t>(p.def_ptr()->u.pd.value);
            }
            return static_cast<int32_t>(p.def_ptr()->u.sd.value);
        }
        return 0;
    }

    /**
     * @brief Value retrieval helper for checkboxes.
     */
    template <typename KeyT, typename = std::enable_if_t<is_param_key<KeyT>::value>>
    bool bool_val(KeyT key) const {
        auto p = param<param_base>(key);
        if (p.valid() && p.def_ptr()) {
            if (p.type() == PF_Param_CHECKBOX) {
                return p.def_ptr()->u.bd.value != 0;
            }
        }
        return false;
    }

    /**
     * @brief Value retrieval helper for colors.
     */
    template <pixel_range Range = pixel_range::tkfloat, typename KeyT,
        typename = std::enable_if_t<is_param_key<KeyT>::value>>
    core::color<Range> color_val(KeyT key) const {
        auto p = param<param_base>(key);
        if (p.valid() && p.def_ptr()) {
            if (p.type() == PF_Param_COLOR) {
                if constexpr (Range == pixel_range::tkfloat) {
                    constexpr double inv_255 = 1.0 / 255.0;
                    double alpha
                        = static_cast<double>(p.def_ptr()->u.cd.value.alpha) * inv_255;
                    if (alpha == 0.0) {
                        alpha = 1.0;
                    }
                    return { alpha,
                        static_cast<double>(p.def_ptr()->u.cd.value.red) * inv_255,
                        static_cast<double>(p.def_ptr()->u.cd.value.green) * inv_255,
                        static_cast<double>(p.def_ptr()->u.cd.value.blue) * inv_255 };
                } else {
                    double alpha = static_cast<double>(p.def_ptr()->u.cd.value.alpha);
                    if (alpha == 0.0) {
                        alpha = 255.0;
                    }
                    return { alpha, static_cast<double>(p.def_ptr()->u.cd.value.red),
                        static_cast<double>(p.def_ptr()->u.cd.value.green),
                        static_cast<double>(p.def_ptr()->u.cd.value.blue) };
                }
            }
        }
        if constexpr (Range == pixel_range::tkfloat) {
            AETK_DEBUG("ctx.color_val, Range::tkfloat, parameter wasn't valid (or didn't "
                       "match)");
            return { 1.0, 0.0, 0.0, 0.0 };
        } else {
            AETK_DEBUG("ctx.color_val, Range::tkuint8, parameter wasn't valid (or didn't "
                       "match)");
            return { 255.0, 0.0, 0.0, 0.0 };
        }
    }

    /**
     * @brief Value retrieval helper for 2D points.
     */
    template <typename KeyT, typename = std::enable_if_t<is_param_key<KeyT>::value>>
    core::vec2 point_val(KeyT key) const {
        auto p = param<param_base>(key);
        if (p.valid() && p.def_ptr()) {
            if (p.type() == PF_Param_POINT) {
                return core::vec2::from_fixed(
                    p.def_ptr()->u.td.x_value, p.def_ptr()->u.td.y_value);
            }
        }
        return { 0.0f, 0.0f };
    }

    /**
     * @brief Value retrieval helper for 3D points.
     */
    template <typename KeyT, typename = std::enable_if_t<is_param_key<KeyT>::value>>
    core::vec3 point_3d_val(KeyT key) const {
        auto p = param<param_base>(key);
        if (p.valid() && p.def_ptr()) {
            if (p.type() == PF_Param_POINT_3D) {
                return { p.def_ptr()->u.point3d_d.x_value,
                    p.def_ptr()->u.point3d_d.y_value, p.def_ptr()->u.point3d_d.z_value };
            }
        }
        return { 0.0f, 0.0f, 0.0f };
    }

    /**
     * @brief Value retrieval helper for arbitrary data params (returns RAII
     * locked_arbitrary).
     */
    template <typename T, typename KeyT,
        typename = std::enable_if_t<is_param_key<KeyT>::value>>
    locked_arbitrary<T> arb_val(KeyT key) const {
        return locked_arbitrary<T>(param<arbitrary_param<T>>(key));
    }

    /**
     * @brief High-level RAII suite acquisition.
     *
     * Usage:
     *   auto duck = ctx.get_suite<DuckSuite1>("AEGP Duck Suite", 1);
     *   duck->Quack(1);
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automated RAII suite acquisition
     * wrapper.
     *
     * @warning <b>Memory & Lifecycles:</b> Safe RAII wrapper mapping
     * `AcquireSuite` / `ReleaseSuite` lifetimes.
     *
     * @tparam T The suite C struct interface.
     * @param name Unique global suite name.
     * @param version Major version index.
     * @return RAII suite manager.
     */
    template <typename T> core::suite<T> get_suite(const char* name, int version) const {
        return core::suite<T>(::aetk::core::context::get_basic_suite(), name, version);
    }

    /**
     * @brief High-level RAII suite acquisition (default name/version).
     */
    template <typename T> core::suite<T> get_suite() const {
        return core::suite<T>(::aetk::core::context::get_basic_suite());
    }

    /**
     * @brief Show host-native color picker dialog.
     *
     * @param title Title of the color picker window.
     * @param initial_color Starting color.
     * @return The picked color if confirmed, std::nullopt otherwise.
     */
    std::optional<core::color<>> show_color_picker(
        const std::string& title, const core::color<>& initial_color) const {
        PF_PixelFloat sample = { 1.0f, (float)initial_color.red,
            (float)initial_color.green, (float)initial_color.blue };
        PF_PixelFloat result = { };
        auto app = get_suite<PFAppSuite6>(kPFAppSuite, kPFAppSuiteVersion6);
        if (app.ptr()) {
            PF_Err err = app.ptr()->PF_AppColorPickerDialog(
                title.c_str(), &sample, TRUE, &result);
            if (err == PF_Err_NONE) {
                return core::color<>(1.0, result.red, result.green, result.blue);
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Query composition background color.
     */
    core::color<> get_comp_background_color() const {
        AEGP_LayerH layerH = nullptr;
        AEGP_CompH compH = nullptr;
        AEGP_ColorVal bg_color { };

        auto pf_interface_suite = get_suite<AEGP_PFInterfaceSuite1>(
            kAEGPPFInterfaceSuite, kAEGPPFInterfaceSuiteVersion1);
        auto layer_suite
            = get_suite<AEGP_LayerSuite8>(kAEGPLayerSuite, kAEGPLayerSuiteVersion8);
        auto comp_suite
            = get_suite<AEGP_CompSuite10>(kAEGPCompSuite, kAEGPCompSuiteVersion10);

        pf_interface_suite->AEGP_GetEffectLayer(in_data_ptr()->effect_ref, &layerH);
        layer_suite->AEGP_GetLayerParentComp(layerH, &compH);
        comp_suite->AEGP_GetCompBGColor(compH, &bg_color);

        return core::color<>(bg_color);
    }

    /**
     * @brief Acquire a suite and show an alert if it fails.
     *
     * Matches the behavior of SDK AEFX_SuiteScoper with an error string.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Wraps AE alert scoper callbacks.
     *
     * @warning <b>Memory & Lifecycles:</b> Safe RAII acquisition.
     *
     * @tparam T The suite C struct interface.
     * @param name Unique global suite name.
     * @param version Major version index.
     * @param message Message to display in alert dialog on failure.
     * @return RAII suite manager.
     */
    template <typename T>
    core::suite<T> get_suite_with_alert(
        const char* name, int version, const char* message) const {
        try {
            return core::suite<T>(
                ::aetk::core::context::get_basic_suite(), name, version);
        } catch (const std::exception&) {
            set_dialog_response(message);
            throw; // Re-throw so the plugin can handle it
        }
    }

    /**
     * @brief Temporal checkout: Fetch a parameter at a specific explicit time.
     *
     * This is required when fetching layer parameters or evaluating animated
     * properties at frames other than the current one. The returned parameter is
     * automatically checked in when the ParamT wrapper goes out of scope.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simplifies timeline temporal checkouts.
     *
     * @warning <b>Memory & Lifecycles:</b> Balanced automatically via
     * `PF_CHECKIN_PARAM`.
     *
     * @tparam ParamT The target parameter wrapper type.
     * @param name The string name of the parameter.
     * @param time The specific timestamp to fetch.
     * @param time_step The step size for the timestamp.
     * @param time_scale The scale of the timestamp.
     * @return Bounded parameter wrapper instance.
     */
    template <typename ParamT, typename KeyT,
        typename = std::enable_if_t<is_param_key<KeyT>::value>>
    ParamT param_at_time(
        KeyT key, int32_t time, int32_t time_step, uint32_t time_scale) const {
        if constexpr (std::is_pointer_v<std::decay_t<KeyT>>
            && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<std::decay_t<KeyT>>>,
                char>) {
            if (!m_index_lookup)
                throw core::exception(
                    PF_Err_BAD_CALLBACK_PARAM, "Index lookup not available");
            int32_t index = m_index_lookup(key);
            if (index < 0) {
                throw core::exception(PF_Err_BAD_CALLBACK_PARAM,
                    std::string("Parameter not found: ") + key);
            }
            return param_at_time_by_index<ParamT>(index, time, time_step, time_scale);
        } else if constexpr (std::is_same_v<std::decay_t<KeyT>, std::string>
            || std::is_same_v<std::decay_t<KeyT>, std::string_view>) {
            if (!m_index_lookup)
                throw core::exception(
                    PF_Err_BAD_CALLBACK_PARAM, "Index lookup not available");
            int32_t index = m_index_lookup(std::string(key).c_str());
            if (index < 0) {
                throw core::exception(PF_Err_BAD_CALLBACK_PARAM,
                    std::string("Parameter not found: ") + std::string(key));
            }
            return param_at_time_by_index<ParamT>(index, time, time_step, time_scale);
        } else {
            int32_t index = -1;
            if (m_int_index_lookup) {
                index = m_int_index_lookup(static_cast<int32_t>(key));
            }
            if (index < 0) {
                index = static_cast<int32_t>(key);
            }
            return param_at_time_by_index<ParamT>(index, time, time_step, time_scale);
        }
    }

    template <typename ParamT, typename KeyT,
        typename = std::enable_if_t<is_param_key<KeyT>::value>>
    ParamT param_at_offset(KeyT key, int32_t frame_offset) const {
        int32_t offset_time
            = m_in_data->current_time + (frame_offset * m_in_data->time_step);
        return param_at_time<ParamT>(
            key, offset_time, m_in_data->time_step, m_in_data->time_scale);
    }

    /**
     * @brief Multi-threaded pixel iterator.
     *
     * @details Selects appropriate iterate suite based on world pixel formats
     * (Float vs 8-bit), invoking wrapped callbacks.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Elevates low-level function-pointer
     * iterations into type-safe C++ modern lambdas.
     *
     * @warning <b>Memory & Lifecycles:</b> Fully thread-safe, avoiding global
     * variable mutations for MFR rendering compatibility.
     *
     * @tparam PixelT Target pixel structure format (e.g. `PF_Pixel8` or
     * `PF_PixelFloat`).
     * @tparam Func Callback functor.
     * @param src Source pixel surface.
     * @param dst Destination pixel surface.
     * @param func Functor parameter.
     */
    /**
     * @brief Raw parallel execution thread worker.
     */
    template <typename Func> void iterate_generic(A_long iterations, Func&& func) const {
        aetk::effect::iterate_generic(m_in_data, iterations, std::forward<Func>(func));
    }

    /**
     * @brief Parallel loop executor utilizing the host's native iterate suite.
     */
    template <typename Func> void parallel_for(int32_t iterations, Func&& func) const {
        aetk::effect::parallel_for(m_in_data, iterations, std::forward<Func>(func));
    }

    /**
     * @brief Context-aware pixel checkout.
     *
     * In SmartFX rendering, this uses the smart render callbacks to check out input
     * layers. In classic rendering, this checks out layer parameters using
     * PF_CHECKOUT_PARAM.
     */
    smart_world checkout_pixels(int32_t checkout_id = 0) const {
        if ((m_cmd == PF_Cmd_SMART_RENDER || m_cmd == PF_Cmd_SMART_RENDER_GPU)
            && m_extra) {
            auto* sr_extra = reinterpret_cast<PF_SmartRenderExtra*>(m_extra);
            PF_EffectWorld* w = nullptr;
            ::aetk::core::check_err(sr_extra->cb->checkout_layer_pixels(
                m_in_data->effect_ref, checkout_id, &w));
            return smart_world(w, m_in_data, smart_world::ownership::LAYER_PIXELS,
                sr_extra->cb, checkout_id);
        } else {
            return smart_world(m_in_data, checkout_id);
        }
    }

    smart_world checkout_pixels(const char* name) const {
        if (!m_index_lookup)
            throw core::exception(
                PF_Err_BAD_CALLBACK_PARAM, "Index lookup not available");
        int32_t index = m_index_lookup(name);
        if (index < 0)
            throw core::exception(
                PF_Err_BAD_CALLBACK_PARAM, std::string("Parameter not found: ") + name);
        return checkout_pixels(index);
    }

    smart_world checkout_pixels(const std::string& name) const {
        return checkout_pixels(name.c_str());
    }

    template <typename EnumT, typename = std::enable_if_t<std::is_enum_v<EnumT>>>
    smart_world checkout_pixels(EnumT key) const {
        if (!m_int_index_lookup)
            throw core::exception(
                PF_Err_BAD_CALLBACK_PARAM, "Integer index lookup not available");
        int32_t index = m_int_index_lookup(static_cast<int32_t>(key));
        if (index < 0)
            throw core::exception(PF_Err_BAD_CALLBACK_PARAM, "Parameter key not found");
        return checkout_pixels(index);
    }

    /**
     * @brief Context-aware output checkout.
     *
     * In SmartFX rendering, this uses the smart render callbacks to check out output
     * pixels. In classic rendering, this returns a non-owning smart_world wrapper of the
     * output buffer.
     */
    smart_world checkout_output() const {
        if ((m_cmd == PF_Cmd_SMART_RENDER || m_cmd == PF_Cmd_SMART_RENDER_GPU)
            && m_extra) {
            auto* sr_extra = reinterpret_cast<PF_SmartRenderExtra*>(m_extra);
            PF_EffectWorld* w = nullptr;
            ::aetk::core::check_err(
                sr_extra->cb->checkout_output(m_in_data->effect_ref, &w));
            return smart_world(w, m_in_data, smart_world::ownership::NONE);
        } else {
            return smart_world(m_output, m_in_data, smart_world::ownership::NONE);
        }
    }

    /** @brief Access raw params array (if available in the current command). */
    PF_ParamDef** raw_params() const {
        return m_params;
    }

protected:
    template <typename ParamT>
    ParamT param_at_time_by_index(
        int32_t index, int32_t time, int32_t time_step, uint32_t time_scale) const {
        return ParamT(m_in_data, index, time, time_step, time_scale);
    }

    template <typename ParamT>
    ParamT param_at_offset_by_index(int32_t index, int32_t frame_offset) const {
        int32_t offset_time
            = m_in_data->current_time + (frame_offset * m_in_data->time_step);
        return param_at_time_by_index<ParamT>(
            index, offset_time, m_in_data->time_step, m_in_data->time_scale);
    }

    PF_InData* m_in_data;
    PF_OutData* m_out_data;
    PF_ParamDef** m_params;
    PF_LayerDef* m_output;
    void* m_extra;
    PF_Cmd m_cmd;
    int device_idx = 0;

private:
    template <typename ParamT> ParamT param_by_index(int32_t index) const {
        if (m_params && m_params[index]) {
            return ParamT(m_params[index], m_in_data, index);
        }

        // If we are in an event (especially DRAW), PF_CHECKOUT_PARAM is forbidden.
        // If m_params is NULL, we MUST NOT fall back to checkout.
        if (m_cmd == PF_Cmd_EVENT) {
            return ParamT(m_in_data, index, true /* skip checkout */);
        }

        return ParamT(m_in_data, index);
    }

    template <typename ParamT> ParamT param_modifier_by_index(int32_t index) const {
        return ParamT(m_params[index], index, m_in_data);
    }
};

enum class field : int32_t {
    frame = PF_Field_FRAME,
    upper = PF_Field_UPPER,
    lower = PF_Field_LOWER
};

enum class channel_mask : int8_t {
    alpha = PF_ChannelMask_ALPHA,
    red = PF_ChannelMask_RED,
    green = PF_ChannelMask_GREEN,
    blue = PF_ChannelMask_BLUE,
    rgb = PF_ChannelMask_RED | PF_ChannelMask_GREEN | PF_ChannelMask_BLUE,
    argb = PF_ChannelMask_ARGB
};

inline constexpr channel_mask operator|(channel_mask a, channel_mask b) {
    return static_cast<channel_mask>(static_cast<int8_t>(a) | static_cast<int8_t>(b));
}

inline constexpr channel_mask operator&(channel_mask a, channel_mask b) {
    return static_cast<channel_mask>(static_cast<int8_t>(a) & static_cast<int8_t>(b));
}

inline constexpr channel_mask operator^(channel_mask a, channel_mask b) {
    return static_cast<channel_mask>(static_cast<int8_t>(a) ^ static_cast<int8_t>(b));
}

inline constexpr channel_mask operator~(channel_mask a) {
    return static_cast<channel_mask>(~static_cast<int8_t>(a));
}

inline channel_mask& operator|=(channel_mask& a, channel_mask b) {
    a = a | b;
    return a;
}

inline channel_mask& operator&=(channel_mask& a, channel_mask b) {
    a = a & b;
    return a;
}

inline channel_mask& operator^=(channel_mask& a, channel_mask b) {
    a = a ^ b;
    return a;
}

/**
 * @brief Modern type-safe wrapper for PF_RenderRequest.
 *
 * @details Simplifies setting and querying render request rectangles, field
 * options, channel masks, and alpha preservation flags using a fluent builder
 * interface.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Replaces raw struct initialization and
 * bitmasks with a clean builder pattern.
 */
class render_request {
public:
    render_request() {
        std::memset(&m_req, 0, sizeof(m_req));
        m_req.channel_mask = PF_ChannelMask_ARGB;
    }

    render_request(const PF_RenderRequest& raw)
        : m_req(raw) {
    }

    render_request(const core::lrect& r, channel_mask mask = channel_mask::argb,
        field fld = field::frame, bool preserve = false) {
        std::memset(&m_req, 0, sizeof(m_req));
        m_req.rect = r.to_pf();
        m_req.channel_mask = static_cast<PF_ChannelMask>(mask);
        m_req.field = static_cast<PF_Field>(fld);
        m_req.preserve_rgb_of_zero_alpha = preserve ? TRUE : FALSE;
    }

    render_request(const core::rect& r, channel_mask mask = channel_mask::argb,
        field fld = field::frame, bool preserve = false) {
        std::memset(&m_req, 0, sizeof(m_req));
        m_req.rect = core::lrect(r).to_pf();
        m_req.channel_mask = static_cast<PF_ChannelMask>(mask);
        m_req.field = static_cast<PF_Field>(fld);
        m_req.preserve_rgb_of_zero_alpha = preserve ? TRUE : FALSE;
    }

    render_request& with_rect(const core::lrect& r) {
        m_req.rect = r.to_pf();
        return *this;
    }

    render_request& with_rect(const core::rect& r) {
        m_req.rect = core::lrect(r).to_pf();
        return *this;
    }

    render_request& with_channel_mask(channel_mask mask) {
        m_req.channel_mask = static_cast<PF_ChannelMask>(mask);
        return *this;
    }

    render_request& with_field(field fld) {
        m_req.field = static_cast<PF_Field>(fld);
        return *this;
    }

    render_request& with_preserve_rgb_of_zero_alpha(bool preserve) {
        m_req.preserve_rgb_of_zero_alpha = preserve ? TRUE : FALSE;
        return *this;
    }

    render_request& expand_rect(int32_t dx, int32_t dy) {
        core::lrect r = rect();
        r.left -= dx;
        r.right += dx;
        r.top -= dy;
        r.bottom += dy;
        return with_rect(r);
    }

    render_request& expand_rect(
        int32_t left, int32_t top, int32_t right, int32_t bottom) {
        core::lrect r = rect();
        r.left -= left;
        r.top -= top;
        r.right += right;
        r.bottom += bottom;
        return with_rect(r);
    }

    render_request& intersect_rect(const core::lrect& other) {
        core::lrect r = rect();
        r.left = (std::max)(r.left, other.left);
        r.top = (std::max)(r.top, other.top);
        r.right = (std::min)(r.right, other.right);
        r.bottom = (std::min)(r.bottom, other.bottom);
        if (r.left > r.right)
            r.right = r.left;
        if (r.top > r.bottom)
            r.bottom = r.top;
        return with_rect(r);
    }

    render_request& intersect_rect(const core::rect& other) {
        return intersect_rect(core::lrect(other));
    }

    core::lrect rect() const {
        return core::lrect(m_req.rect);
    }
    channel_mask channel_mask() const {
        return static_cast<aetk::effect::channel_mask>(m_req.channel_mask);
    }
    field field() const {
        return static_cast<aetk::effect::field>(m_req.field);
    }
    bool preserve_rgb_of_zero_alpha() const {
        return m_req.preserve_rgb_of_zero_alpha != 0;
    }

    const PF_RenderRequest& to_pf() const {
        return m_req;
    }
    operator const PF_RenderRequest&() const {
        return m_req;
    }

    PF_RenderRequest& to_pf() {
        return m_req;
    }
    operator PF_RenderRequest&() {
        return m_req;
    }

private:
    PF_RenderRequest m_req;
};

/**
 * @brief Wrapped checkout result. Exposes only what matters.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Binds raw `PF_CheckoutResult` bounds.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct checkout_result {
    /// Resulting valid bounding rectangle.
    core::lrect result_rect;

    /// Max possible bounding rectangle.
    core::lrect max_result_rect;

    /// Solid rendering flag.
    bool solid = false;

    checkout_result() = default;

    /**
     * @brief Checkout result wrapper.
     *
     * @param raw Raw host checkout struct.
     */
    explicit checkout_result(const PF_CheckoutResult& raw)
        : result_rect(raw.result_rect)
        , max_result_rect(raw.max_result_rect)
        , solid(raw.solid != 0) {
    }

    int32_t width() const {
        return result_rect.width();
    }
    int32_t height() const {
        return result_rect.height();
    }
};

// ══════════════════════════════════════════════════════════════════════
//  Specialized Contexts
// ══════════════════════════════════════════════════════════════════════

struct user_changed_param_context;

/**
 * @brief Context wrapper for PF_Cmd_SMART_PRE_RENDER.
 *
 * @details SmartFX pre-render context. Manages bounding rect unions
 * (`union_rects`), allocates pre-render data parameters, and GPU execution
 * permissions.
 *
 * @note <b>AE SDK Paradigm Shift:</b> SmartFX pre-render context. Manages
 * bounding rect unions (`union_rects`), allocates pre-render dynamic
 * parameters, and configures GPU execution permissions.
 *
 * @warning <b>Memory & Lifecycles:</b> Allocates `pre_render_data` memory which
 * must be garbage collected using custom delete callbacks. Statically asserts
 * in Premiere Pro compatibility frameworks.
 */
struct pre_render_context : public context {
    /**
     * @brief Pre-render setup constructor.
     *
     * @tparam C Parent context type.
     * @param ctx Parent base context.
     */
    template <typename C = context>
    pre_render_context(const C& ctx)
        : context(ctx)
        , m_pr_extra(static_cast<PF_PreRenderExtra*>(ctx.extra_ptr())) {
#ifdef AETK_PREMIERE_COMPAT
        static_assert(aetk::core::always_false<C>::value,
            "AETK Error: SmartFX (pre_render_context) is an After Effects "
            "exclusive feature and incompatible with Premiere Pro.");
#endif
    }

    /**
     * @brief Checkout a layer and auto-propagate rects to output.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automates standard output rect
     * expansion union calculations.
     *
     * @warning <b>Memory & Lifecycles:</b> Bound checkouts are balanced by
     * SmartFX pipelines.
     *
     * @param index Parameter layer index.
     * @param checkout_id Unique checkout reference.
     * @return Bounded checkout result parameters.
     */
    checkout_result checkout_layer(int32_t index, int32_t checkout_id = 0) const {
        PF_CheckoutResult raw { };
        ::aetk::core::check_err(
            m_pr_extra->cb->checkout_layer(in_data_ptr()->effect_ref, index, checkout_id,
                &m_pr_extra->input->output_request, in_data_ptr()->current_time,
                in_data_ptr()->time_step, in_data_ptr()->time_scale, &raw));

        union_rects(raw.result_rect);
        return checkout_result(raw);
    }

    /**
     * @brief Checkout a layer with a custom render_request.
     *
     * @param index Parameter layer index.
     * @param req Custom render request.
     * @param checkout_id Unique checkout reference.
     * @return Bounded checkout result parameters.
     */
    checkout_result checkout_layer(
        int32_t index, const render_request& req, int32_t checkout_id = 0) const {
        PF_CheckoutResult raw { };
        ::aetk::core::check_err(m_pr_extra->cb->checkout_layer(in_data_ptr()->effect_ref,
            index, checkout_id, &req.to_pf(), in_data_ptr()->current_time,
            in_data_ptr()->time_step, in_data_ptr()->time_scale, &raw));

        union_rects(raw.result_rect);
        return checkout_result(raw);
    }

    /**
     * @brief Checkout a layer at an explicit timeline offset.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automates standard output rect
     * expansion union calculations.
     *
     * @warning <b>Memory & Lifecycles:</b> Bound checkouts are balanced by
     * SmartFX pipelines.
     *
     * @param index Parameter layer index.
     * @param checkout_id Unique checkout reference.
     * @param frame_offset Relative frame offset duration.
     * @return Bounded checkout result parameters.
     */
    checkout_result checkout_layer_at_offset(
        int32_t index, int32_t checkout_id, int32_t frame_offset) const {
        PF_CheckoutResult raw { };
        int32_t time
            = in_data_ptr()->current_time + (frame_offset * in_data_ptr()->time_step);
        ::aetk::core::check_err(m_pr_extra->cb->checkout_layer(in_data_ptr()->effect_ref,
            index, checkout_id, &m_pr_extra->input->output_request, time,
            in_data_ptr()->time_step, in_data_ptr()->time_scale, &raw));

        union_rects(raw.result_rect);
        return checkout_result(raw);
    }

    /**
     * @brief Checkout a layer at an explicit timeline offset with a custom
     * request.
     *
     * @param index Parameter layer index.
     * @param req Custom render request.
     * @param checkout_id Unique checkout reference.
     * @param frame_offset Relative frame offset duration.
     * @return Bounded checkout result parameters.
     */
    checkout_result checkout_layer_at_offset(int32_t index, const render_request& req,
        int32_t checkout_id, int32_t frame_offset) const {
        PF_CheckoutResult raw { };
        int32_t time
            = in_data_ptr()->current_time + (frame_offset * in_data_ptr()->time_step);
        ::aetk::core::check_err(m_pr_extra->cb->checkout_layer(in_data_ptr()->effect_ref,
            index, checkout_id, &req.to_pf(), time, in_data_ptr()->time_step,
            in_data_ptr()->time_scale, &raw));

        union_rects(raw.result_rect);
        return checkout_result(raw);
    }

    /** @brief Fetch the default incoming output request. */
    render_request output_request() const {
        return render_request(m_pr_extra->input->output_request);
    }

    /** @brief Retrieve the output result rectangle. */
    core::lrect result_rect() const {
        return core::lrect(m_pr_extra->output->result_rect);
    }

    /** @brief Set/override the output result rectangle. */
    void set_result_rect(const core::lrect& r) const {
        m_pr_extra->output->result_rect = r.to_pf();
    }

    /** @brief Retrieve the output max result rectangle. */
    core::lrect max_result_rect() const {
        return core::lrect(m_pr_extra->output->max_result_rect);
    }

    /** @brief Set/override the output max result rectangle. */
    void set_max_result_rect(const core::lrect& r) const {
        m_pr_extra->output->max_result_rect = r.to_pf();
    }

    /** @brief Sets theRETURNS_EXTRA_PIXELS output rendering flag. */
    void set_returns_extra_pixels(bool returns = true) const {
        if (returns) {
            m_pr_extra->output->flags |= PF_RenderOutputFlag_RETURNS_EXTRA_PIXELS;
        } else {
            m_pr_extra->output->flags &= ~PF_RenderOutputFlag_RETURNS_EXTRA_PIXELS;
        }
    }

    /** @brief Expands result_rect and max_result_rect on all sides. */
    void expand_output_bounds(
        int32_t left, int32_t top, int32_t right, int32_t bottom) const {
        m_pr_extra->output->result_rect.left -= left;
        m_pr_extra->output->result_rect.top -= top;
        m_pr_extra->output->result_rect.right += right;
        m_pr_extra->output->result_rect.bottom += bottom;

        m_pr_extra->output->max_result_rect.left -= left;
        m_pr_extra->output->max_result_rect.top -= top;
        m_pr_extra->output->max_result_rect.right += right;
        m_pr_extra->output->max_result_rect.bottom += bottom;
    }

    /**
     * @brief Mix in custom dependency values into the host's cache guid.
     *
     * @tparam T dependency state value type.
     */
    template <typename T> void mix_guid(const T& value) const {
        if (m_pr_extra && m_pr_extra->cb && m_pr_extra->cb->GuidMixInPtr
            && reinterpret_cast<uintptr_t>(m_pr_extra->cb->GuidMixInPtr)
                != 0xabababababababab) {
            m_pr_extra->cb->GuidMixInPtr(in_data_ptr()->effect_ref,
                static_cast<A_long>(sizeof(T)),
                const_cast<void*>(reinterpret_cast<const void*>(&value)));
        }
    }

    /** @brief Mix in raw byte blocks into the host's cache guid. */
    void mix_guid_raw(const void* data, int32_t bytes) const {
        if (m_pr_extra && m_pr_extra->cb && m_pr_extra->cb->GuidMixInPtr
            && reinterpret_cast<uintptr_t>(m_pr_extra->cb->GuidMixInPtr)
                != 0xabababababababab) {
            m_pr_extra->cb->GuidMixInPtr(in_data_ptr()->effect_ref,
                static_cast<A_long>(bytes), const_cast<void*>(data));
        }
    }

    /**
     * @brief Flags that GPU computations are enabled.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard GPU render capability flag.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void enable_gpu_render() const {
        m_pr_extra->output->flags |= PF_RenderOutputFlag_GPU_RENDER_POSSIBLE;
    }

    /**
     * @brief Save custom pre-render data parameters.
     *
     * @details Binds deletion lambdas to guarantee leak-free cleanup.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Elegant type-safe cleanup registration.
     *
     * @warning <b>Memory & Lifecycles:</b> Pre-render data is cleaned up by After
     * Effects using the provided deletion callback.
     *
     * @tparam T Pre-render data structure type.
     * @param data Raw pointer of type `T` to save.
     */
    template <typename T> void set_pre_render_data(T* data) const {
        m_pr_extra->output->pre_render_data = data;
        m_pr_extra->output->delete_pre_render_data_func
            = [](void* ptr) { delete static_cast<T*>(ptr); };
    }

    template <typename T>
    void set_pre_render_data(T* data, void (*deleter)(void*)) const {
        m_pr_extra->output->pre_render_data = data;
        m_pr_extra->output->delete_pre_render_data_func = deleter;
    }

    int32_t current_frame() const {
        if (in_data_ptr()->time_step == 0)
            return 0;
        return in_data_ptr()->current_time / in_data_ptr()->time_step;
    }

    int16_t bitdepth() const {
        return m_pr_extra->input->bitdepth;
    }

    /** @brief Force a specific GPU framework to be used for the current frame
     * rendering. */
    void set_what_gpu(PF_GPU_Framework fw) const {
        if (m_pr_extra && m_pr_extra->input) {
            m_pr_extra->input->what_gpu = fw;
        }
    }

    /** @brief Get the current GPU device index. */
    int32_t gpu_device_index() const {
        if (m_pr_extra && m_pr_extra->input) {
            return m_pr_extra->input->device_index;
        }
        return -1;
    }

    /** @brief Check if a specific GPU framework is available for the current frame
     * rendering. */
    bool has_framework(PF_GPU_Framework fw) const {
        if (m_pr_extra && m_pr_extra->input) {
            return (m_pr_extra->input->what_gpu == fw);
        }
        return false;
    }

private:
    void union_rects(const PF_LRect& next) const {
        PF_LRect& current = m_pr_extra->output->result_rect;
        if (current.left == 0 && current.right == 0 && current.top == 0
            && current.bottom == 0) {
            current = next;
        } else {
            current.left = (std::min)(current.left, next.left);
            current.top = (std::min)(current.top, next.top);
            current.right = (std::max)(current.right, next.right);
            current.bottom = (std::max)(current.bottom, next.bottom);
        }
        m_pr_extra->output->max_result_rect = current;
    }

    PF_PreRenderExtra* m_pr_extra;
};

using render_context = context;

/**
 * @brief Context wrapper for PF_Cmd_SMART_RENDER.
 *
 * @details SmartFX render-time context wrapper. Replaces legacy stack
 * iterations with 32-bit float SmartFX world checkouts.
 *
 * @note <b>AE SDK Paradigm Shift:</b> SmartFX render-time context wrapper.
 * Replaces legacy stack iterations with 32-bit float SmartFX world checkouts.
 *
 * @warning <b>Memory & Lifecycles:</b> Statically blocked in Premiere Pro
 * compatibility modes.
 */
struct smart_render_context : public context {
    /**
     * @brief Smart render setup constructor.
     *
     * @tparam C Parent context type.
     * @param ctx Parent base context.
     * @param extra Raw host smart render parameters.
     */
    template <typename C = context>
    smart_render_context(const C& ctx, PF_SmartRenderExtra* extra)
        : context(ctx)
        , m_sr_extra(extra) {
    }

    /**
     * @brief Classic render fallback constructor.
     *
     * @tparam C Parent context type.
     * @param ctx Parent base context.
     */
    template <typename C = context>
    smart_render_context(const C& ctx)
        : context(ctx)
        , m_sr_extra(nullptr) {
    }

    PF_SmartRenderExtra* extra() const {
        return m_sr_extra;
    }

    bool is_gpu() const override {
        return cmd() == PF_Cmd_SMART_RENDER_GPU;
    }

    /**
     * @brief Fetch pre-render metadata allocations.
     *
     * @note <b>AE SDK Paradigm Shift:</b> SmartFX environmental accessors.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @tparam T Custom metadata structure.
     * @return Pre-render struct pointer.
     */
    template <typename T> T* pre_render_data() const {
        return m_sr_extra ? static_cast<T*>(m_sr_extra->input->pre_render_data) : nullptr;
    }

    int16_t bitdepth() const {
        if (m_sr_extra) {
            return m_sr_extra->input->bitdepth;
        }
        if (output_ptr() && in_data_ptr() && in_data_ptr()->pica_basicP) {
            aetk::core::suite<PF_WorldSuite2> world_suite(in_data_ptr()->pica_basicP);
            PF_PixelFormat format = PF_PixelFormat_INVALID;
            if (world_suite->PF_GetPixelFormat(output_ptr(), &format) == PF_Err_NONE) {
                if (format == PF_PixelFormat_ARGB128)
                    return 32;
                if (format == PF_PixelFormat_ARGB64)
                    return 16;
                return 8;
            }
        }
        return 8;
    }
    uint32_t device_index() const {
        return m_sr_extra ? m_sr_extra->input->device_index : 0;
    }
    const void* gpu_data() const {
        return (m_sr_extra && m_sr_extra->input) ? m_sr_extra->input->gpu_data : nullptr;
    }
    PF_GPU_Framework what_gpu() const {
        return (m_sr_extra && m_sr_extra->input) ? m_sr_extra->input->what_gpu
                                                 : PF_GPU_Framework_NONE;
    }

    /**
     * @brief Calculates placement offset for centering an input world in an
     * expanded output world.
     *
     * @details Incorporates native SDK output_origin with manual size-based
     * centering fallbacks to prevent clipping.
     *
     * @param input Input source world reference.
     * @param output Output target world reference.
     * @return Center offset position coordinate vectors.
     */
    core::vec2 placement_offset(
        const smart_world& input, const smart_world& output) const {
        A_long offset_x = (output.width() - input.width()) / 2;
        A_long offset_y = (output.height() - input.height()) / 2;
        A_long actual_x = (in_data_ptr()->output_origin_x != 0)
            ? in_data_ptr()->output_origin_x
            : offset_x;
        A_long actual_y = (in_data_ptr()->output_origin_y != 0)
            ? in_data_ptr()->output_origin_y
            : offset_y;
        return { static_cast<double>(actual_x), static_cast<double>(actual_y) };
    }

private:
    PF_SmartRenderExtra* m_sr_extra;
};

// ── Interaction & Events ──────────────────────────────────────────────

/**
 * @brief Context wrapper for PF_Cmd_EVENT custom UI rendering and mouse
 * interactions.
 *
 * @details Consolidates screen coordinate translations, modifier keyboard
 * states, coordinate conversions, and Drawbot canvases.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Consolidated Event handler wrapper.
 * Simplifies event mapping (click, drag, KeyDown, adjust cursor),
 * local-to-screen coordinate translations, and custom Drawbot canvas
 * acquisitions.
 *
 * @warning <b>Memory & Lifecycles:</b> Custom UI overlays are incompatible with
 * Premiere Pro.
 */
struct interaction_context : public context {
    /**
     * @brief Event context constructor.
     *
     * @param ctx Parent base context.
     */
    interaction_context(const context& ctx)
        : context(ctx)
        , m_event_extra(static_cast<PF_EventExtra*>(ctx.extra_ptr())) {
    }

    enum class event_type : std::uint8_t {
        none,
        new_context,
        activate,
        click,
        drag,
        draw,
        deactivate,
        close_context,
        idle,
        keydown_obsolete,
        adjust_cursor,
        keydown,
        mouse_exited
    };

    enum class window_type : std::int8_t {
        none = -1,
        comp = 0,
        layer = 1,
        effect = 2,
        preview = 3
    };

    /**
     * @brief Resolve the event type.
     *
     * @note <b>AE SDK Paradigm Shift:</b> OOP event wrappers.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Event type code.
     */
    event_type type() const {
        switch (m_event_extra->e_type) {
        case PF_Event_NEW_CONTEXT:
            return event_type::new_context;
        case PF_Event_ACTIVATE:
            return event_type::activate;
        case PF_Event_DO_CLICK:
            return event_type::click;
        case PF_Event_DRAG:
            return event_type::drag;
        case PF_Event_DRAW:
            return event_type::draw;
        case PF_Event_DEACTIVATE:
            return event_type::deactivate;
        case PF_Event_CLOSE_CONTEXT:
            return event_type::close_context;
        case PF_Event_IDLE:
            return event_type::idle;
        case PF_Event_KEYDOWN_OBSOLETE:
            return event_type::keydown_obsolete;
        case PF_Event_ADJUST_CURSOR:
            return event_type::adjust_cursor;
        case PF_Event_KEYDOWN:
            return event_type::keydown;
        case PF_Event_MOUSE_EXITED:
            return event_type::mouse_exited;
        default:
            return event_type::none;
        }
    }

    /**
     * @brief Resolve the active window type.
     *
     * @note <b>AE SDK Paradigm Shift:</b> OOP event wrappers.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Bounded window type.
     */
    window_type window() const {
        if (m_event_extra->contextH && *m_event_extra->contextH) {
            switch ((**m_event_extra->contextH).w_type) {
            case PF_Window_COMP:
                return window_type::comp;
            case PF_Window_LAYER:
                return window_type::layer;
            case PF_Window_EFFECT:
                return window_type::effect;
            case PF_Window_PREVIEW:
                return window_type::preview;
            default:
                break;
            }
        }
        return window_type::none;
    }

    /**
     * @brief Screen space point coordinates.
     *
     * @note <b>AE SDK Paradigm Shift:</b> OOP event wrappers.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Screen point vector.
     */
    core::vec2 screen_point() const {
        auto t = type();
        if (t == event_type::click || t == event_type::drag) {
            return core::vec2(m_event_extra->u.do_click.screen_point.h,
                m_event_extra->u.do_click.screen_point.v);
        } else if (t == event_type::adjust_cursor) {
            return core::vec2(m_event_extra->u.adjust_cursor.screen_point.h,
                m_event_extra->u.adjust_cursor.screen_point.v);
        } else if (t == event_type::keydown) {
            return core::vec2(m_event_extra->u.key_down.screen_point.h,
                m_event_extra->u.key_down.screen_point.v);
        }
        return core::vec2(0.0f, 0.0f);
    }

    /**
     * @brief The parameter index for the Custom UI being drawn/interacted with.
     *
     * Returns >0 if this is a per-parameter Custom UI (PF_PUI_CONTROL).
     * Returns 0 if this is a global Custom UI (PF_OutFlag_CUSTOM_UI).
     *
     * @note <b>AE SDK Paradigm Shift:</b> OOP event wrappers.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Bounded index value.
     */
    int32_t param_index() const {
        if (m_event_extra->contextH && *m_event_extra->contextH
            && (**m_event_extra->contextH).w_type == PF_Window_EFFECT) {
            return m_event_extra->effect_win.index;
        }
        return 0;
    }

    /**
     * @brief Convert a screen point to local UI coordinates.
     *
     * @note <b>AE SDK Paradigm Shift:</b> OOP event wrappers.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Local point vector.
     */
    core::vec2 local_point() const {
        auto sp = screen_point();
        auto t = type();
        if (t == event_type::click || t == event_type::drag
            || t == event_type::adjust_cursor || t == event_type::keydown) {
            if (window() == window_type::effect) {
                return { static_cast<float>(
                             sp.x - m_event_extra->effect_win.current_frame.left),
                    static_cast<float>(
                        sp.y - m_event_extra->effect_win.current_frame.top) };
            }
            return sp;
        }
        return { 0, 0 };
    }

    // ── Coordinate Mapping Callbacks ──────────────────────────────────

    bool has_callbacks() const {
        return m_event_extra != nullptr;
    }

    /**
     * @brief Convert layer source pixel coordinates to active window frame
     * coordinates.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simplifies complicated
     * fixed-point 16.16 mapping calculations.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param layer_pt Input layer coordinates vector.
     * @return Window frame coordinates vector.
     */
    core::vec2 source_to_frame(const core::vec2& layer_pt) const {
        if (!has_callbacks() || !m_event_extra->cbs.source_to_frame)
            return layer_pt;
        PF_FixedPoint pt = { static_cast<A_long>(layer_pt.x * 65536.0f),
            static_cast<A_long>(layer_pt.y * 65536.0f) };
        m_event_extra->cbs.source_to_frame(
            m_event_extra->cbs.refcon, m_event_extra->contextH, &pt);
        return { static_cast<float>(pt.x) / 65536.0f,
            static_cast<float>(pt.y) / 65536.0f };
    }

    /**
     * @brief Convert window frame coordinates to layer source pixel coordinates.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simplifies complicated
     * fixed-point 16.16 mapping calculations.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param frame_pt Input frame coordinates vector.
     * @return Layer coordinates vector.
     */
    core::vec2 frame_to_source(const core::vec2& frame_pt) const {
        if (!has_callbacks() || !m_event_extra->cbs.frame_to_source)
            return frame_pt;
        PF_FixedPoint pt = { static_cast<A_long>(frame_pt.x * 65536.0f),
            static_cast<A_long>(frame_pt.y * 65536.0f) };
        m_event_extra->cbs.frame_to_source(
            m_event_extra->cbs.refcon, m_event_extra->contextH, &pt);
        return { static_cast<float>(pt.x) / 65536.0f,
            static_cast<float>(pt.y) / 65536.0f };
    }

    /**
     * @brief Convert layer coordinates to compositions coordinates.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simplifies complicated
     * fixed-point 16.16 mapping calculations.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param layer_pt Input layer coordinates vector.
     * @param time Timestamp index.
     * @param scale Divisor scale.
     * @return Composition coordinates vector.
     */
    core::vec2 layer_to_comp(
        const core::vec2& layer_pt, A_long time = -1, A_long scale = -1) const {
        if (!has_callbacks() || !m_event_extra->cbs.layer_to_comp)
            return layer_pt;
        PF_FixedPoint pt = { static_cast<A_long>(layer_pt.x * 65536.0f),
            static_cast<A_long>(layer_pt.y * 65536.0f) };
        A_long t = (time == -1) ? in_data_ptr()->current_time : time;
        A_long s = (scale == -1) ? in_data_ptr()->time_scale : scale;
        m_event_extra->cbs.layer_to_comp(
            m_event_extra->cbs.refcon, m_event_extra->contextH, t, s, &pt);
        return { static_cast<float>(pt.x) / 65536.0f,
            static_cast<float>(pt.y) / 65536.0f };
    }

    /**
     * @brief Convert composition coordinates to layer coordinates.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simplifies complicated
     * fixed-point 16.16 mapping calculations.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param comp_pt Input composition coordinates vector.
     * @param time Timestamp index.
     * @param scale Divisor scale.
     * @return Layer coordinates vector.
     */
    core::vec2 comp_to_layer(
        const core::vec2& comp_pt, A_long time = -1, A_long scale = -1) const {
        if (!has_callbacks() || !m_event_extra->cbs.comp_to_layer)
            return comp_pt;
        PF_FixedPoint pt = { static_cast<A_long>(comp_pt.x * 65536.0f),
            static_cast<A_long>(comp_pt.y * 65536.0f) };
        A_long t = (time == -1) ? in_data_ptr()->current_time : time;
        A_long s = (scale == -1) ? in_data_ptr()->time_scale : scale;
        m_event_extra->cbs.comp_to_layer(
            m_event_extra->cbs.refcon, m_event_extra->contextH, t, s, &pt);
        return { static_cast<float>(pt.x) / 65536.0f,
            static_cast<float>(pt.y) / 65536.0f };
    }

    /**
     * @brief Get keyboard modifier mask.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard state properties.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Modifiers bitmask.
     */
    uint32_t modifiers() const {
        if (type() == event_type::click || type() == event_type::drag)
            return m_event_extra->u.do_click.modifiers;
        if (type() == event_type::adjust_cursor)
            return m_event_extra->u.adjust_cursor.modifiers;
        return 0;
    }

    core::vec2 origin() const {
        return { static_cast<float>(m_event_extra->effect_win.current_frame.left),
            static_cast<float>(m_event_extra->effect_win.current_frame.top) };
    }

    int32_t width() const {
        return m_event_extra->effect_win.current_frame.right
            - m_event_extra->effect_win.current_frame.left;
    }

    int32_t height() const {
        return m_event_extra->effect_win.current_frame.bottom
            - m_event_extra->effect_win.current_frame.top;
    }

    bool shift_down() const {
        return (modifiers() & PF_Mod_SHIFT_KEY) != 0;
    }
    bool ctrl_down() const {
        return (modifiers() & PF_Mod_CMD_CTRL_KEY) != 0;
    }
    bool alt_down() const {
        return (modifiers() & PF_Mod_OPT_ALT_KEY) != 0;
    }

    /**
     * @brief Access the Drawbot canvas for custom UI drawing.
     * Only valid during event_type::draw.
     *
     * @details Queries `PF_EffectCustomUISuite2` (with version 1 fallbacks) to
     * get the drawing reference.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automated drawing reference
     * acquisition.
     *
     * @warning <b>Memory & Lifecycles:</b> Returns an RAII
     * `aetk::ui::drawbot::canvas` wrapper.
     *
     * @return Canvas drawing manager.
     */
    ::aetk::ui::drawbot::canvas canvas() const {
        DRAWBOT_DrawRef draw_ref = nullptr;

        // 1. Try modern suite call (preferred by SDK samples)
        auto custom_ui = get_suite<PF_EffectCustomUISuite2>(
            kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion2);
        if (custom_ui.ptr()) {
            custom_ui->PF_GetDrawingReference(m_event_extra->contextH, &draw_ref);
        }

        // 2. Fallback to reserved field if suite call failed or returned null
        if (!draw_ref && m_event_extra->contextH && m_event_extra->contextH[0]) {
            draw_ref = m_event_extra->contextH[0]->reserved_drawref;
        }

        return { draw_ref };
    }

    /**
     * @brief Get the rect that needs updating during a draw event.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard action hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Update bounding rectangle.
     */
    core::rect update_rect() const {
        return core::rect(m_event_extra->u.draw.update_rect);
    }

    /**
     * @brief Request that AE sends drag events after this click.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard action hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void request_drag() const {
        if (type() == event_type::click) {
            m_event_extra->u.do_click.send_drag = true;
        }
    }

    /**
     * @brief Tell AE that the event was handled and shouldn't propagate.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard action hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param handled If true, sets Event Handled bit flag.
     */
    void set_handled(bool handled = true) const {
        if (handled)
            m_event_extra->evt_out_flags |= PF_EO_HANDLED_EVENT;
        else
            m_event_extra->evt_out_flags &= ~PF_EO_HANDLED_EVENT;
    }

    /**
     * @brief Request a UI refresh (e.g. after data changes).
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard action hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void set_refresh_ui() const {
        m_out_data->out_flags |= PF_OutFlag_REFRESH_UI;
    }

    /**
     * @brief Invalidate a portion of the custom UI (or the whole thing if rect is
     * null).
     *
     * @details Calls `PF_InvalidateRect` on `PFAppSuite6`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct redraw trigger.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param r Optional bounding rectangle.
     */
    void invalidate(const core::rect* r = nullptr) const {
        if (m_cmd != PF_Cmd_EVENT || !m_event_extra) {
            return;
        }

        // Request a redraw by setting the out flags. This is safe and valid for all
        // events.
        m_event_extra->evt_out_flags |= PF_EO_UPDATE_NOW;

        // Only invoke the host PF_InvalidateRect suite call during event types
        // where the AE host officially permits it (e.g. click, drag, idle).
        // Prohibited event types (like DRAW, keydown, cursor adjustment, exited,
        // close, deactivate) will trigger an After Effects internal verification
        // failure dialog if we call it.
        bool can_call_invalidate_rect = false;
        if (m_event_extra->e_type == PF_Event_DO_CLICK) {
            can_call_invalidate_rect = true;
        } else if (m_event_extra->e_type == PF_Event_DRAG) {
            if (!m_event_extra->u.do_click.last_time) {
                can_call_invalidate_rect = true;
            }
        } else if (m_event_extra->e_type == PF_Event_IDLE) {
            can_call_invalidate_rect = true;
        } else if (m_event_extra->e_type == PF_Event_NEW_CONTEXT
            || m_event_extra->e_type == PF_Event_ACTIVATE) {
            can_call_invalidate_rect = true;
        }

        if (can_call_invalidate_rect) {
            auto app = get_suite<PFAppSuite6>(kPFAppSuite, kPFAppSuiteVersion6);
            if (r) {
                PF_Rect pr = { (A_long)r->left, (A_long)r->top, (A_long)r->right,
                    (A_long)r->bottom };
                app->PF_InvalidateRect(m_event_extra->contextH, &pr);
            } else {
                app->PF_InvalidateRect(m_event_extra->contextH, nullptr);
            }
        }
    }

    PF_EventExtra* event_extra() const {
        return m_event_extra;
    }

    // ── High-level Arb Data Access ───────────────────────────────────

    /**
     * @brief RAII lock for arbitrary data parameters during UI events.
     *
     * @details Locks the parameter arbitrary data handle upon construction, and
     * unlocks upon destruction.
     *
     * Usage:
     *   auto lock = ctx.arb_data<curve_data>(1); // param index
     *   if (lock) {
     *       auto& data = *lock;
     *       data.points.push_back({0.5f, 0.5f});
     *       lock.mark_changed(); // signals AE that the value was modified
     *   }
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw C lock and unlock
     * boilerplates during custom UI click/drag updates.
     *
     * @warning <b>Memory & Lifecycles:</b> Unlocks handles automatically on
     * destruction.
     *
     * @tparam T Arbitrary data structure type.
     */
    template <typename T> class arb_lock {
    public:
        /**
         * @brief Lock constructor.
         *
         * @param ctx Parent event context.
         * @param param_index Arbitrary parameter index.
         */
        arb_lock(const interaction_context& ctx, int32_t param_index)
            : m_utils(ctx.in_data_ptr()->utils)
            , m_param(nullptr)
            , m_ptr(nullptr)
            , m_handle(nullptr) {
            auto** params = ctx.raw_params();
            if (params && params[param_index]) {
                m_param = params[param_index];
                m_handle = m_param->u.arb_d.value;
                if (m_handle) {
                    m_ptr = reinterpret_cast<T*>(m_utils->host_lock_handle(m_handle));
                }
            }
        }

        /**
         * @brief Safe lock release.
         */
        ~arb_lock() {
            if (m_handle && m_ptr) {
                m_utils->host_unlock_handle(m_handle);
            }
        }

        // Non-copyable, non-movable
        arb_lock(const arb_lock&) = delete;
        arb_lock& operator=(const arb_lock&) = delete;

        explicit operator bool() const {
            return m_ptr != nullptr;
        }
        T& operator*() const {
            return *m_ptr;
        }
        T* operator->() const {
            return m_ptr;
        }
        T* get() const {
            return m_ptr;
        }

        /**
         * @brief Signal AE that the arbitrary data value was modified.
         *
         * @note <b>AE SDK Paradigm Shift:</b> Standard changed parameter
         * notification.
         *
         * @warning <b>Memory & Lifecycles:</b> None.
         */
        void mark_changed() const {
            if (m_param)
                m_param->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
        }

    private:
        PF_UtilCallbacks* m_utils;
        PF_ParamDef* m_param;
        T* m_ptr;
        PF_Handle m_handle;
    };

    /**
     * @brief Lock and access an arbitrary data parameter by index. Returns an
     * RAII lock.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Returns RAII lock.
     *
     * @warning <b>Memory & Lifecycles:</b> Balanced on destruction.
     *
     * @tparam T Arbitrary data structure type.
     * @param param_index Parameter index.
     * @return Bounded RAII lock.
     */
    template <typename T> arb_lock<T> arb_data(int32_t param_index) const {
        return arb_lock<T>(*this, param_index);
    }

    /**
     * @brief Returns true if this is the final drag event (mouse released).
     *
     * @note <b>AE SDK Paradigm Shift:</b> OOP event wrappers.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return True if drag ends.
     */
    bool is_drag_end() const {
        if (type() == event_type::drag) {
            return m_event_extra->u.do_click.last_time != 0;
        }
        return false;
    }

    /**
     * @brief Mark a parameter as changed by index.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard action hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param param_index Target parameter index.
     */
    void mark_param_changed(int32_t param_index) const {
        auto** params = raw_params();
        if (params && params[param_index]) {
            params[param_index]->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
        }
    }

    PF_EventExtra* event_extra_ptr() const {
        return m_event_extra;
    }

protected:
    PF_EventExtra* m_event_extra;
};

/**
 * @brief Context wrapper for parameter change notifications.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Focus context representing param change
 * hooks.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct param_change_context : public context {
    /**
     * @brief Change constructor.
     *
     * @param ctx Parent base context.
     */
    param_change_context(const context& ctx)
        : context(ctx)
        , m_change_extra(static_cast<PF_UserChangedParamExtra*>(ctx.extra_ptr())) {
    }

    int32_t param_index() const {
        return m_change_extra->param_index;
    }

private:
    PF_UserChangedParamExtra* m_change_extra;
};

/**
 * @brief Context wrapper for PF_Cmd_USER_CHANGED_PARAM.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Context helper dispatched during UI
 * parameter mutations.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct user_changed_param_context : public context {
    /**
     * @brief User change constructor.
     *
     * @param ctx Parent base context.
     * @param extra Raw host parameters.
     * @param params Writable param array pointer.
     */
    user_changed_param_context(
        const context& ctx, PF_UserChangedParamExtra* extra, PF_ParamDef** params)
        : context(ctx)
        , m_uc_extra(extra)
        , m_uc_params(params) {
    }

    PF_UserChangedParamExtra* extra() const {
        return m_uc_extra;
    }
    int32_t param_index() const {
        return m_uc_extra->param_index;
    }

    /** @brief Direct access to the writable params array for UI manipulation. */
    PF_ParamDef** raw_params() const {
        return m_uc_params;
    }

    template <typename ParamT, typename KeyT,
        typename = std::enable_if_t<is_param_key<KeyT>::value>>
    ParamT param_modifier(KeyT key) const {
        int32_t index = -1;
        if constexpr (std::is_pointer_v<std::decay_t<KeyT>>
            && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<std::decay_t<KeyT>>>,
                char>) {
            if (m_index_lookup)
                index = m_index_lookup(key);
        } else if constexpr (std::is_same_v<std::decay_t<KeyT>, std::string>
            || std::is_same_v<std::decay_t<KeyT>, std::string_view>) {
            if (m_index_lookup)
                index = m_index_lookup(std::string(key).c_str());
        } else {
            if (m_int_index_lookup) {
                index = m_int_index_lookup(static_cast<int32_t>(key));
            }
            if (index < 0) {
                index = static_cast<int32_t>(key);
            }
        }
        return param_modifier_by_index<ParamT>(index);
    }

    /**
     * @brief Commit a modified param back to the AE UI via PF_UpdateParamUI.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Triggers AE param UI updates.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param index Parameter index.
     */
    template <typename KeyT, typename = std::enable_if_t<is_param_key<KeyT>::value>>
    void commit_param(KeyT key) const {
        int32_t index = -1;
        if constexpr (std::is_pointer_v<std::decay_t<KeyT>>
            && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<std::decay_t<KeyT>>>,
                char>) {
            if (m_index_lookup)
                index = m_index_lookup(key);
        } else if constexpr (std::is_same_v<std::decay_t<KeyT>, std::string>
            || std::is_same_v<std::decay_t<KeyT>, std::string_view>) {
            if (m_index_lookup)
                index = m_index_lookup(std::string(key).c_str());
        } else {
            if (m_int_index_lookup) {
                index = m_int_index_lookup(static_cast<int32_t>(key));
            }
            if (index < 0) {
                index = static_cast<int32_t>(key);
            }
        }
        commit_param_by_index(index);
    }

    /**
     * @brief Request UI refresh from AE.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simple flag wrapper.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void refresh_ui() const {
        out_data_ptr()->out_flags |= PF_OutFlag_REFRESH_UI;
    }

private:
    void commit_param_by_index(int32_t index) const {
        if (!m_uc_params || index < 1)
            return;
        aetk::core::suite<PF_ParamUtilsSuite3> s(
            ::aetk::core::context::get_basic_suite());
        aetk::core::check_err(
            s->PF_UpdateParamUI(in_data_ptr()->effect_ref, index, m_uc_params[index]));
    }

    template <typename ParamT> ParamT param_modifier_by_index(int32_t index) const {
        return ParamT(m_uc_params[index], index, in_data_ptr());
    }

    PF_UserChangedParamExtra* m_uc_extra;
    PF_ParamDef** m_uc_params;
};

/**
 * @brief Context wrapper for PF_Cmd_UPDATE_PARAMS_UI.
 *
 * @details Parameter UI update context. Leverages modern AEGP suites to show,
 * hide, or manipulate parameter hierarchies dynamically.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Parameter UI update context. Leverages
 * modern AEGP suites to show, hide, or manipulate parameter hierarchies
 * dynamically.
 *
 * @warning <b>Memory & Lifecycles:</b> Modifying stream parameters requires
 * balancing stream references.
 */
struct ui_update_context : public context {
    /**
     * @brief UI update constructor.
     *
     * @param ctx Parent base context.
     */
    ui_update_context(const context& ctx)
        : context(ctx) {
    }

    /**
     * @brief Access global data stored during GLOBAL_SETUP.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Modern type-safe global state getter.
     *
     * @warning <b>Memory & Lifecycles:</b> Caller must unlock.
     *
     * @tparam T Global state struct type.
     * @return Locked global state pointer.
     */
    template <typename T> T* global_data() const {
        if (!out_data_ptr()->global_data)
            return nullptr;
        return reinterpret_cast<T*>(
            (*in_data_ptr()->utils->host_lock_handle)(out_data_ptr()->global_data));
    }

    /**
     * @brief Commit a modified param copy back to the AE UI.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard update trigger.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param index Parameter index.
     * @param param_copy Copied parameters pointer.
     */
    void commit_param(int32_t index, PF_ParamDef* param_copy) const {
        aetk::core::suite<PF_ParamUtilsSuite3> s(
            ::aetk::core::context::get_basic_suite());
        s->PF_UpdateParamUI(in_data_ptr()->effect_ref, index, param_copy);
    }

    /**
     * @brief Hide or show a parameter using AEGP DynamicStreamSuite.
     *
     * @details Queries `AEGP_PFInterfaceSuite1`, `AEGP_StreamSuite2`,
     * `AEGP_DynamicStreamSuite4`, and `AEGP_EffectSuite4` to toggle the dynamic
     * `AEGP_DynStreamFlag_HIDDEN` flag.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Completely hides parameter details
     * programmatically, bypassing bulky SDK macro suites.
     *
     * @warning <b>Memory & Lifecycles:</b> Safely acquires and disposes all
     * intermediate stream/effect handles using `AEGP_DisposeStream` and
     * `AEGP_DisposeEffect`, guaranteeing leak-free stream transactions.
     *
     * @param aegp_id  The plugin's AEGP_PluginID obtained from
     * register_with_aegp().
     * @param index    The parameter index to hide/show.
     * @param hidden   TRUE to hide, FALSE to show.
     */
    void set_stream_hidden(AEGP_PluginID aegp_id, int32_t index, A_Boolean hidden) const {
        aetk::core::suite<AEGP_PFInterfaceSuite1> pfi(
            ::aetk::core::context::get_basic_suite());
        aetk::core::suite<AEGP_StreamSuite2> ss(::aetk::core::context::get_basic_suite());
        aetk::core::suite<AEGP_DynamicStreamSuite4> dss(
            ::aetk::core::context::get_basic_suite());
        aetk::core::suite<AEGP_EffectSuite4> es(::aetk::core::context::get_basic_suite());

        AEGP_EffectRefH meH = nullptr;
        pfi->AEGP_GetNewEffectForEffect(aegp_id, in_data_ptr()->effect_ref, &meH);
        if (meH) {
            AEGP_StreamRefH streamH = nullptr;
            ss->AEGP_GetNewEffectStreamByIndex(aegp_id, meH, index, &streamH);
            if (streamH) {
                dss->AEGP_SetDynamicStreamFlag(
                    streamH, AEGP_DynStreamFlag_HIDDEN, FALSE, hidden);
                ss->AEGP_DisposeStream(streamH);
            }
            es->AEGP_DisposeEffect(meH);
        }
    }

    /**
     * @brief Force AE to re-render after UI changes.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simple flag wrapper.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void force_rerender() const {
        out_data_ptr()->out_flags |= PF_OutFlag_FORCE_RERENDER;
    }

    /**
     * @brief Request a UI refresh of the Effect Controls Window.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Required in Premiere Pro after param
     * visibility changes (hide/show), since it does not automatically redraw
     * the ECW after `PF_UpdateParamUI` calls. In After Effects this flag is
     * redundant but harmless.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void set_refresh_ui() const {
        out_data_ptr()->out_flags |= PF_OutFlag_REFRESH_UI;
    }
};

// --------------------------------------------------------------------
// Out-of-line implementations of tensor/zeros_pinned that require context
// --------------------------------------------------------------------

template <typename T, size_t Rank, device_kind Dev>
tensor<T, Rank, Dev>::tensor(
    std::array<size_t, Rank> shape, const context& ctx, device_kind kind, bool avoid_lock)
    : m_shape(shape) {
    if (kind != Dev) {
        throw std::runtime_error(
            "Constructor kind parameter must match Dev template parameter");
    }
    size_t total_elements = 1;
    for (size_t s : m_shape)
        total_elements *= s;
    size_t bytes = total_elements * sizeof(T);

    // Compute contiguous strides (C-order)
    ptrdiff_t stride = 1;
    for (int i = static_cast<int>(Rank) - 1; i >= 0; --i) {
        m_strides[i] = stride;
        stride *= m_shape[i];
    }

    PF_InData* in_data = ctx.in_data_ptr();
    if (!in_data) {
        throw std::runtime_error("Context has null PF_InData pointer");
    }
    auto effect_ref = in_data->effect_ref;

    allocate(effect_ref, bytes, -1, ctx.is_gpu() || avoid_lock);
}

template <typename T, size_t Rank, device_kind Dev>
template <device_kind TargetDev>
tensor<T, Rank, TargetDev> tensor<T, Rank, Dev>::to(
    const context& ctx, device_kind kind) const {
    PF_InData* in_data = ctx.in_data_ptr();
    if (!in_data) {
        throw std::runtime_error("to() requires valid context");
    }

    size_t total_elements = 1;
    for (size_t s : m_shape)
        total_elements *= s;
    size_t bytes = total_elements * sizeof(T);

    tensor<T, Rank, TargetDev> dst(m_shape, ctx, TargetDev, ctx.is_gpu() || m_avoid_lock);

    if constexpr (Dev == device_kind::cuda
        && (TargetDev == device_kind::cpu || TargetDev == device_kind::cpu_pinned)) {
#if defined(AETK_ENABLE_CUDA) || defined(AETK_CUDA_SUPPORT) || defined(__CUDACC__)
        size_t height = m_shape[0];
        size_t width = m_shape[1];
        size_t channels = (Rank == 3) ? m_shape[2] : 1;
        ptrdiff_t src_pitch = m_strides[0] * sizeof(T);
        ptrdiff_t dst_pitch = bytes / height;

        if (cudaMemcpy2D(dst.data_ptr(), dst_pitch, m_data, src_pitch,
                width * channels * sizeof(T), height, cudaMemcpyDeviceToHost)
            != cudaSuccess) {
            throw std::runtime_error(
                "cudaMemcpy2D failed during device-to-host transfer");
        }
#else
        throw std::runtime_error("CUDA support is not enabled in this build");
#endif
    } else if constexpr ((Dev == device_kind::cpu || Dev == device_kind::cpu_pinned)
        && TargetDev == device_kind::cuda) {
#if defined(AETK_ENABLE_CUDA) || defined(AETK_CUDA_SUPPORT) || defined(__CUDACC__)
        size_t height = m_shape[0];
        size_t width = m_shape[1];
        size_t channels = (Rank == 3) ? m_shape[2] : 1;
        ptrdiff_t src_pitch = m_strides[0] * sizeof(T);
        ptrdiff_t dst_pitch = bytes / height;

        if (cudaMemcpy2D(dst.data_ptr(), dst_pitch, m_data, src_pitch,
                width * channels * sizeof(T), height, cudaMemcpyHostToDevice)
            != cudaSuccess) {
            throw std::runtime_error(
                "cudaMemcpy2D failed during host-to-device transfer");
        }
#else
        throw std::runtime_error("CUDA support is not enabled in this build");
#endif
    } else {
        std::memcpy(dst.data_ptr(), m_data, bytes);
    }

    return dst;
}

template <typename T, size_t Rank, device_kind Dev>
smart_world tensor<T, Rank, Dev>::to_world(const context& ctx, short bitdepth) const {
    static_assert(Rank == 3, "to_world requires Rank-3 tensor [height, width, channels]");
    size_t height = m_shape[0];
    size_t width = m_shape[1];

    smart_world world(ctx.in_data_ptr(), width, height, bitdepth, false);
    this->copy_to(world);
    return world;
}

template <typename T, size_t Rank>
inline tensor<T, Rank, device_kind::cpu_pinned> zeros_pinned(
    std::array<size_t, Rank> shape, const context& ctx) {
    return tensor<T, Rank, device_kind::cpu_pinned>(shape, ctx);
}

} // namespace aetk::effect

#include <aetk/effect/context/setup_contexts.hpp>
#include <aetk/effect/params/param_setup.hpp>
