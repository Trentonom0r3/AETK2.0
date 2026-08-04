#pragma once

#include <Param_Utils.h>
#include <aetk/core/premiere_compat.hpp>
#include <aetk/effect/context/context.hpp>
#include <aetk/effect/params/param_callbacks.hpp>
#include <string>

namespace aetk::effect {

struct params_setup_context;
namespace ui {
    class widget_registry;
}

/**
 * @brief Builder helper for registering parameter configurations.
 *
 * @details Enables registering dynamic event callbacks on the parameter inline
 * (via `.on_change(cb)`).
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, registering parameter
 * metadata is a procedural process using raw macros.
 * `aetk::effect::param_builder` is a clean builder pattern wrapper that enables
 * registering dynamic event callbacks on the parameter inline (via
 * `.on_change(cb)`).
 *
 * @warning <b>Memory & Lifecycles:</b> The builder retains a reference to the
 * setup context. It must only be instantiated during setup registration phases.
 */
struct param_builder {
    /// Active setup context reference.
    const params_setup_context& ctx;

    /// Unique parameter string layout name.
    std::string name;

    /// Parameter index.
    int index;

    /**
     * @brief Builder constructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simple property mapping.
     *
     * @warning <b>Memory & Lifecycles:</b> Binds context reference.
     *
     * @param ctx Active setup context.
     * @param n Unique parameter string name.
     * @param idx Parameter index.
     */
    param_builder(const params_setup_context& ctx, std::string n, int idx)
        : ctx(ctx)
        , name(std::move(n))
        , index(idx) {
    }

    /**
     * @brief Dynamically registers parameter change callback.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct inline event subscription.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param cb Target change callback.
     * @return Reference to this builder.
     */
    param_builder& on_change(param_change_callback_t cb);

    /**
     * @brief Assigns a custom key string for easier and safer lookup.
     *
     * @param key Unique parameter identifier key.
     * @return Reference to this builder.
     */
    param_builder& set_key(std::string key);

    /**
     * @brief Assigns a custom compile-time integer/enum key for safer lookup.
     *
     * @tparam KeyEnum Enum or integer type.
     * @param key Unique parameter identifier key.
     * @return Reference to this builder.
     */
    template <typename KeyEnum> param_builder& set_key(KeyEnum key);
};

/**
 * @brief Special builder pattern for custom arbitrary data parameter
 * registration.
 *
 * @details Provides fluent methods for event triggers and keyframe
 * interpolators.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Special builder pattern for custom
 * arbitrary data parameter registration, providing fluent methods for event
 * triggers and keyframe interpolators.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 *
 * @tparam T The custom arbitrary data type.
 */
template <typename T> struct arb_param_builder : public param_builder {
    arb_param_builder(const param_builder& base)
        : param_builder(base) {
    }
    arb_param_builder(const params_setup_context& ctx, std::string n, int idx)
        : param_builder(ctx, std::move(n), idx) {
    }

    /**
     * @brief Register change callback.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Fluent builder change callback.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param cb Target change callback.
     * @return Reference to this builder.
     */
    arb_param_builder<T>& on_change(param_change_callback_t cb) {
        param_builder::on_change(std::move(cb));
        return *this;
    }

    /**
     * @brief Assigns a custom key string for easier and safer lookup.
     *
     * @param key Unique parameter identifier key.
     * @return Reference to this builder.
     */
    arb_param_builder<T>& set_key(std::string key) {
        param_builder::set_key(std::move(key));
        return *this;
    }

    arb_param_builder<T>& set_tooltip(std::string text);

    /**
     * @brief Assigns a custom compile-time integer/enum key for safer lookup.
     *
     * @tparam KeyEnum Enum or integer type.
     * @param key Unique parameter identifier key.
     * @return Reference to this builder.
     */
    template <typename KeyEnum> arb_param_builder<T>& set_key(KeyEnum key) {
        param_builder::set_key(key);
        return *this;
    }

    /**
     * @brief Register custom interpolation lambda.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Binds functional interpolators
     * directly.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param func Target interpolation callback.
     * @return Reference to this builder.
     */
    arb_param_builder<T>& on_interpolate(
        std::function<void(T*, const T*, const T*, double)> func) {
        A_short arb_id = static_cast<A_short>(core::hash_string(name) & 0x7FFF);
        arb_data_registry::register_interpolator<T>(arb_id, std::move(func));
        return *this;
    }
};

/**
 * @brief RAII collapsible parameter topic groups layout coordinator.
 *
 * @details Replaces procedural GROUP_START/GROUP_END calls with exception-safe
 * stack lifetimes.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, creating collapsible
 * parameter groups requires manually adding separate `PF_Param_GROUP_START` and
 * `PF_Param_GROUP_END` parameters, leaving risk for misaligned layout closures.
 * `aetk::effect::param_group` utilizes the RAII pattern to automatically close
 * groups at scope exit.
 *
 * @warning <b>Memory & Lifecycles:</b> Destructor does not throw exceptions.
 * Must only be instantiated during setup registration phases.
 */
struct param_group {
    /// Active setup context.
    const params_setup_context& ctx;

    /**
     * @brief Opens parameter layout group.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw C group opening macros.
     *
     * @warning <b>Memory & Lifecycles:</b> Safe layout transaction.
     *
     * @param context Active setup context.
     * @param name Group name.
     * @param flags Group parameter flags.
     */
    param_group(const params_setup_context& context, const char* name, int32_t flags = 0);

    /**
     * @brief Automatically closes the layout group.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe RAII closure.
     *
     * @warning <b>Memory & Lifecycles:</b> Destructor never throws exceptions.
     */
    ~param_group();
};

/**
 * @brief Context wrapper for parameter setup callbacks.
 *
 * @details Consolidation context dispatched during `PF_Cmd_PARAMS_SETUP`.
 * Provides helper methods (`add_checkbox`, `add_slider`, `add_arbitrary`,
 * `add_popup`) that encapsulate bulky AEFX macros and exception-safe pointer
 * math seamlessly.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Consolidation context dispatched during
 * `PF_Cmd_PARAMS_SETUP`. Provides helper methods (`add_checkbox`, `add_slider`,
 * `add_arbitrary`, `add_popup`) that encapsulate bulky AEFX macros and
 * exception-safe pointer math seamlessly.
 *
 * @warning <b>Memory & Lifecycles:</b> The context does not own parameter
 * definitions. Handles allocated for arbitrary defaults are owned by AE.
 * Arbitrary data is incompatible with Premiere Pro, checked via compile-time
 * assertions.
 */
struct params_setup_context : public context {
    using register_cb_func = void (*)(const std::string&, int, param_change_callback_t);
    register_cb_func reg_func = nullptr;

    using register_key_func = void (*)(const std::string&, int);
    register_key_func reg_key_func = nullptr;

    using register_int_key_func = void (*)(int32_t, int);
    register_int_key_func reg_int_key_func = nullptr;

    mutable bool custom_ui_registered = false;

    /**
     * @brief Base setup context wrapper.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Consolidated wrapper representation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Parent base context.
     * @param reg Target callback registry pointer.
     * @param reg_key Target custom string key registry pointer.
     * @param reg_int_key Target custom integer key registry pointer.
     */
    params_setup_context(const context& ctx, register_cb_func reg = nullptr,
        register_key_func reg_key = nullptr, register_int_key_func reg_int_key = nullptr)
        : context(ctx)
        , reg_func(reg)
        , reg_key_func(reg_key)
        , reg_int_key_func(reg_int_key) {
    }

    void increment_param_count() const {
        m_out_data->num_params++;
    }
    int32_t current_count() const {
        return m_out_data->num_params;
    }

    /**
     * @brief Registers integer slider parameter.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Type-safe parameter registration
     * builders, automatically calculating compile-time FNV-1a hashes for
     * parameter IDs.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param name Bounded parameter name.
     * @param min Minimum bounds.
     * @param max Maximum bounds.
     * @param def Default value.
     * @return Parameter setup builder.
     */
    param_builder add_int_slider(
        const char* name, int32_t min, int32_t max, int32_t def) const {
        PF_ParamDef def_p;
        AEFX_CLR_STRUCT(def_p);
        def_p.param_type = PF_Param_SLIDER;
        PF_STRNNCPY(def_p.PF_DEF_NAME, name, sizeof(def_p.PF_DEF_NAME));
        def_p.u.sd.valid_min = min;
        def_p.u.sd.valid_max = max;
        def_p.u.sd.slider_min = min;
        def_p.u.sd.slider_max = max;
        def_p.u.sd.value = def_p.u.sd.dephault = def;
        def_p.uu.id = static_cast<A_long>(core::hash_string(name));

        ::aetk::core::check_err(PF_ADD_PARAM(in_data_ptr(), -1, &def_p),
            "Failed to add integer slider parameter");
        int32_t idx = current_count();
        register_callback(name, idx, nullptr);
        increment_param_count();
        return { *this, name, idx };
    }

    /**
     * @brief Registers button parameter.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Type-safe parameter registration
     * builders, automatically calculating compile-time FNV-1a hashes for
     * parameter IDs.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param name Bounded parameter name.
     * @return Parameter setup builder.
     */
    param_builder add_button(const char* name, int32_t flags = PF_ParamFlag_SUPERVISE,
        int32_t ui_flags = PF_PUI_NONE) const {
        PF_ParamDef def_p;
        AEFX_CLR_STRUCT(def_p);
        def_p.param_type = PF_Param_BUTTON;
        PF_STRNNCPY(def_p.PF_DEF_NAME, name, sizeof(def_p.PF_DEF_NAME));
        def_p.u.button_d.u.namesptr = name;
        def_p.uu.id = static_cast<A_long>(core::hash_string(name));
        def_p.flags |= flags;
        def_p.ui_flags |= ui_flags;

        ::aetk::core::check_err(
            PF_ADD_PARAM(in_data_ptr(), -1, &def_p), "Failed to add button parameter");
        int32_t idx = current_count();
        register_callback(name, idx, nullptr);
        increment_param_count();
        return { *this, name, idx };
    }

    /**
     * @brief Registers layer parameter.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Type-safe parameter registration
     * builders, automatically calculating compile-time FNV-1a hashes for
     * parameter IDs.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param name Bounded parameter name.
     * @param def_val Default selection value.
     * @return Parameter setup builder.
     */
    param_builder add_layer(
        const char* name, int32_t def_val = PF_LayerDefault_MYSELF) const {
        PF_ParamDef def_p;
        AEFX_CLR_STRUCT(def_p);
        def_p.param_type = PF_Param_LAYER;
        PF_STRNNCPY(def_p.PF_DEF_NAME, name, sizeof(def_p.PF_DEF_NAME));
        def_p.u.ld.dephault = def_val;
        def_p.uu.id = static_cast<A_long>(core::hash_string(name));

        ::aetk::core::check_err(
            PF_ADD_PARAM(in_data_ptr(), -1, &def_p), "Failed to add layer parameter");
        int32_t idx = current_count();
        register_callback(name, idx, nullptr);
        increment_param_count();
        return { *this, name, idx };
    }

    /**
     * @brief Registers dropdown popup parameters.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Type-safe parameter registration
     * builders, automatically calculating compile-time FNV-1a hashes for
     * parameter IDs.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param name Parameter name.
     * @param num_choices Choices item count.
     * @param def Default selected option.
     * @param choices_str Pipeline-delimited list of choice names.
     * @param flags Parameter option flags.
     * @param ui_flags UI parameters flags.
     * @return Parameter setup builder.
     */
    param_builder add_popup(const char* name, int num_choices, int def,
        const char* choices_str, int32_t flags = PF_ParamFlag_NONE,
        int32_t ui_flags = PF_PUI_NONE) const {
        PF_ParamDef def_p;
        AEFX_CLR_STRUCT(def_p);
        def_p.param_type = PF_Param_POPUP;
        PF_STRNNCPY(def_p.PF_DEF_NAME, name, sizeof(def_p.PF_DEF_NAME));
        def_p.u.pd.num_choices = static_cast<short>(num_choices);
        def_p.u.pd.dephault = def;
        def_p.u.pd.value = def;
        def_p.u.pd.u.PF_DEF_NAMESPTR = choices_str;
        def_p.uu.id = static_cast<A_long>(core::hash_string(name));
        def_p.flags |= flags;
        def_p.ui_flags |= ui_flags;

        ::aetk::core::check_err(
            PF_ADD_PARAM(in_data_ptr(), -1, &def_p), "Failed to add popup parameter");
        int32_t idx = current_count();
        register_callback(name, idx, nullptr);
        increment_param_count();
        return { *this, name, idx };
    }

    /**
     * @brief Registers dynamic custom arbitrary data parameters.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simplifies complex arbitrary parameter
     * initialization, handle allocations, and registry binding.
     *
     * @warning <b>Memory & Lifecycles:</b> Allocates a default host handle.
     * Statically blocked in Premiere Pro compatibility frameworks.
     *
     * @tparam T The custom arbitrary data type.
     * @param name Parameter name.
     * @param default_val Optional default value struct pointer.
     * @param flags Parameter option flags.
     * @param ui_flags UI parameters flags.
     * @param ui_width Parameter width.
     * @param ui_height Parameter height.
     * @return Arbitrary parameter setup builder.
     */
    template <typename T>
    arb_param_builder<T> add_arbitrary(const char* name, const T* default_val = nullptr,
        int32_t flags = PF_ParamFlag_NONE, int32_t ui_flags = PF_PUI_NO_ECW_UI,
        short ui_width = 0, short ui_height = 0) const {
#ifdef AETK_PREMIERE_COMPAT
        static_assert(aetk::core::always_false<T>::value,
            "AETK Error: Arbitrary Data parameters are After Effects "
            "exclusives and incompatible with Premiere Pro.");
#endif
        PF_ParamDef def_p;
        AEFX_CLR_STRUCT(def_p);
        def_p.param_type = PF_Param_ARBITRARY_DATA;
        PF_STRNNCPY(def_p.PF_DEF_NAME, name, sizeof(def_p.PF_DEF_NAME));

        // We use a 15-bit truncated hash for arbitrary data ID, as AE only supports
        // A_short in PF_ArbParamsExtra.
        A_short arb_id = static_cast<A_short>(core::hash_string(name) & 0x7FFF);
        def_p.uu.id = arb_id;
        def_p.u.arb_d.id = arb_id;
        def_p.flags |= flags;
        def_p.ui_flags |= ui_flags;
        def_p.ui_width = ui_width;
        def_p.ui_height = ui_height;

        // Allocate a default handle — AE requires this to initialize the parameter.
        // Without it, params[N]->u.arb_d.value will always be NULL.
        auto* utils = in_data_ptr()->utils;
        PF_Handle dephault_h = utils->host_new_handle(sizeof(T));
        if (dephault_h) {
            T* ptr = reinterpret_cast<T*>(utils->host_lock_handle(dephault_h));
            if (ptr) {
                if (default_val) {
                    arb_traits<T>::copy(ptr, default_val);
                } else {
                    arb_traits<T>::init(ptr);
                }
                utils->host_unlock_handle(dephault_h);
            }
        }
        def_p.u.arb_d.dephault = dephault_h;
        def_p.u.arb_d.value = nullptr;
        def_p.u.arb_d.pad = 0;

        // Register into the global arb_data_registry
        arb_data_registry::register_type<T>(arb_id);

        ::aetk::core::check_err(
            PF_ADD_PARAM(in_data_ptr(), -1, &def_p), "Failed to add arbitrary parameter");
        int32_t idx = current_count();
        register_callback(name, idx, nullptr);
        increment_param_count();
        return { *this, name, idx };
    }

    /**
     * @brief Registers color parameter.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Type-safe parameter registration
     * builders, automatically calculating compile-time FNV-1a hashes for
     * parameter IDs.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param name Parameter name.
     * @param r Red value [0, 255].
     * @param g Green value [0, 255].
     * @param b Blue value [0, 255].
     * @return Parameter setup builder.
     */
    param_builder add_color(const char* name, uint8_t r, uint8_t g, uint8_t b) const {
        PF_ParamDef def_p;
        AEFX_CLR_STRUCT(def_p);
        def_p.param_type = PF_Param_COLOR;
        PF_STRNNCPY(def_p.PF_DEF_NAME, name, sizeof(def_p.PF_DEF_NAME));
        def_p.u.cd.value.red = r;
        def_p.u.cd.value.green = g;
        def_p.u.cd.value.blue = b;
        def_p.u.cd.value.alpha = 255;
        def_p.u.cd.dephault = def_p.u.cd.value;
        def_p.uu.id = static_cast<A_long>(core::hash_string(name));

        ::aetk::core::check_err(
            PF_ADD_PARAM(in_data_ptr(), -1, &def_p), "Failed to add color parameter");
        int32_t idx = current_count();
        register_callback(name, idx, nullptr);
        increment_param_count();
        return { *this, name, idx };
    }

    /**
     * @brief Registers checkbox parameter.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Type-safe parameter registration
     * builders, automatically calculating compile-time FNV-1a hashes for
     * parameter IDs.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param name Parameter name.
     * @param def Default boolean selection.
     * @param caption Alternate display name.
     * @param flags Option parameter flags.
     * @param ui_flags Option UI flags.
     * @return Parameter setup builder.
     */
    param_builder add_checkbox(const char* name, bool def, const char* caption = nullptr,
        int32_t flags = PF_ParamFlag_NONE, int32_t ui_flags = PF_PUI_NONE) const {
        PF_ParamDef def_p;
        AEFX_CLR_STRUCT(def_p);
        def_p.param_type = PF_Param_CHECKBOX;
        PF_STRNNCPY(def_p.PF_DEF_NAME, name, sizeof(def_p.PF_DEF_NAME));
        def_p.u.bd.value = def_p.u.bd.dephault = def;
        def_p.u.bd.u.PF_DEF_NAMEPTR = caption ? caption : name;
        def_p.uu.id = static_cast<A_long>(core::hash_string(name));
        def_p.flags |= flags;
        def_p.ui_flags |= ui_flags;

        ::aetk::core::check_err(
            PF_ADD_PARAM(in_data_ptr(), -1, &def_p), "Failed to add checkbox parameter");
        int32_t idx = current_count();
        register_callback(name, idx, nullptr);
        increment_param_count();
        return { *this, name, idx };
    }

    /**
     * @brief Registers floating-point slider parameter.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Type-safe parameter registration
     * builders, automatically calculating compile-time FNV-1a hashes for
     * parameter IDs.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param name Parameter name.
     * @param min Minimum range value.
     * @param max Maximum range value.
     * @param def Default value.
     * @param flags Option parameter flags.
     * @param ui_flags Option UI flags.
     * @return Parameter setup builder.
     */
    param_builder add_slider(const char* name, float min, float max, float def,
        int32_t flags = PF_ParamFlag_NONE, int32_t ui_flags = PF_PUI_NONE) const {
        PF_ParamDef def_p;
        AEFX_CLR_STRUCT(def_p);
        def_p.param_type = PF_Param_FLOAT_SLIDER;
        PF_STRNNCPY(def_p.PF_DEF_NAME, name, sizeof(def_p.PF_DEF_NAME));
        def_p.u.fs_d.valid_min = min;
        def_p.u.fs_d.valid_max = max;
        def_p.u.fs_d.slider_min = min;
        def_p.u.fs_d.slider_max = max;
        def_p.u.fs_d.value = def_p.u.fs_d.dephault = def;
        def_p.u.fs_d.precision = 1;
        def_p.u.fs_d.display_flags = 0;
        def_p.uu.id = static_cast<A_long>(core::hash_string(name));
        def_p.flags |= flags;
        def_p.ui_flags |= ui_flags;

        ::aetk::core::check_err(PF_ADD_PARAM(in_data_ptr(), -1, &def_p),
            "Failed to add float slider parameter");
        int32_t idx = current_count();
        register_callback(name, idx, nullptr);
        increment_param_count();
        return { *this, name, idx };
    }

    /**
     * @brief Registers 2D Point parameter.
     *
     * @param name Parameter name.
     * @param def_x Default X value (percentage, e.g. 50.0f).
     * @param def_y Default Y value (percentage, e.g. 50.0f).
     * @param restrict_bounds Bounded restrict selection.
     * @param flags Option parameter flags.
     * @param ui_flags Option UI flags.
     * @return Parameter setup builder.
     */
    param_builder add_point2d(const char* name, float def_x = 50.0f, float def_y = 50.0f,
        bool restrict_bounds = false, int32_t flags = PF_ParamFlag_NONE,
        int32_t ui_flags = PF_PUI_NONE) const {
        PF_ParamDef def_p;
        AEFX_CLR_STRUCT(def_p);
        def_p.param_type = PF_Param_POINT;
        PF_STRNNCPY(def_p.PF_DEF_NAME, name, sizeof(def_p.PF_DEF_NAME));
        def_p.u.td.restrict_bounds = restrict_bounds ? TRUE : FALSE;
        def_p.u.td.x_value = def_p.u.td.x_dephault = FLOAT2FIX(def_x);
        def_p.u.td.y_value = def_p.u.td.y_dephault = FLOAT2FIX(def_y);
        def_p.uu.id = static_cast<A_long>(core::hash_string(name));
        def_p.flags |= flags;
        def_p.ui_flags |= ui_flags;

        ::aetk::core::check_err(
            PF_ADD_PARAM(in_data_ptr(), -1, &def_p), "Failed to add 2D point parameter");
        int32_t idx = current_count();
        register_callback(name, idx, nullptr);
        increment_param_count();
        return { *this, name, idx };
    }

    /**
     * @brief Registers Angle parameter.
     *
     * @param name Parameter name.
     * @param def Default angle value.
     * @param flags Option parameter flags.
     * @param ui_flags Option UI flags.
     * @return Parameter setup builder.
     */
    param_builder add_angle(const char* name, float def = 0.0f, int32_t flags = PF_ParamFlag_NONE,
        int32_t ui_flags = PF_PUI_NONE) const {
        PF_ParamDef def_p;
        AEFX_CLR_STRUCT(def_p);
        def_p.param_type = PF_Param_ANGLE;
        PF_STRNNCPY(def_p.PF_DEF_NAME, name, sizeof(def_p.PF_DEF_NAME));
        def_p.u.ad.value = def_p.u.ad.dephault = static_cast<A_long>(def * 65536.0f);
        def_p.uu.id = static_cast<A_long>(core::hash_string(name));
        def_p.flags |= flags;
        def_p.ui_flags |= ui_flags;

        ::aetk::core::check_err(
            PF_ADD_PARAM(in_data_ptr(), -1, &def_p), "Failed to add angle parameter");
        int32_t idx = current_count();
        register_callback(name, idx, nullptr);
        increment_param_count();
        return { *this, name, idx };
    }

    /**
     * @brief Registers 3D Point parameter.
     *
     * @param name Parameter name.
     * @param def_x Default X value.
     * @param def_y Default Y value.
     * @param def_z Default Z value.
     * @param flags Option parameter flags.
     * @param ui_flags Option UI flags.
     * @return Parameter setup builder.
     */
    param_builder add_point3d(const char* name, float def_x = 0.0f, float def_y = 0.0f,
        float def_z = 0.0f, int32_t flags = PF_ParamFlag_NONE,
        int32_t ui_flags = PF_PUI_NONE) const {
        PF_ParamDef def_p;
        AEFX_CLR_STRUCT(def_p);
        def_p.param_type = PF_Param_POINT_3D;
        PF_STRNNCPY(def_p.PF_DEF_NAME, name, sizeof(def_p.PF_DEF_NAME));
        def_p.u.point3d_d.x_value = def_p.u.point3d_d.x_dephault = def_x;
        def_p.u.point3d_d.y_value = def_p.u.point3d_d.y_dephault = def_y;
        def_p.u.point3d_d.z_value = def_p.u.point3d_d.z_dephault = def_z;
        def_p.uu.id = static_cast<A_long>(core::hash_string(name));
        def_p.flags |= flags;
        def_p.ui_flags |= ui_flags;

        ::aetk::core::check_err(
            PF_ADD_PARAM(in_data_ptr(), -1, &def_p), "Failed to add 3D point parameter");
        int32_t idx = current_count();
        register_callback(name, idx, nullptr);
        increment_param_count();
        return { *this, name, idx };
    }

    /**
     * @brief Registers fixed-point slider parameters.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Type-safe parameter registration
     * builders, automatically calculating compile-time FNV-1a hashes for
     * parameter IDs.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param name Parameter name.
     * @param valid_min Absolute minimum value.
     * @param valid_max Absolute maximum value.
     * @param slider_min Minimum slider value.
     * @param slider_max Maximum slider value.
     * @param def Default value.
     * @param precision Precision decimal digits.
     * @param display_flags Standard display parameters.
     * @param flags Option parameter flags.
     * @param ui_flags Option UI flags.
     * @return Parameter setup builder.
     */
    param_builder add_fixed_slider(const char* name, float valid_min, float valid_max,
        float slider_min, float slider_max, float def, int16_t precision = 1,
        int16_t display_flags = 0, int32_t flags = PF_ParamFlag_NONE,
        int32_t ui_flags = PF_PUI_NONE) const {
        PF_ParamDef def_p;
        AEFX_CLR_STRUCT(def_p);
        def_p.param_type = PF_Param_FIX_SLIDER;
        PF_STRNNCPY(def_p.PF_DEF_NAME, name, sizeof(def_p.PF_DEF_NAME));
        def_p.u.fd.valid_min = INT2FIX(valid_min);
        def_p.u.fd.valid_max = INT2FIX(valid_max);
        def_p.u.fd.slider_min = INT2FIX(slider_min);
        def_p.u.fd.slider_max = INT2FIX(slider_max);
        def_p.u.fd.value = def_p.u.fd.dephault = INT2FIX(def);
        def_p.u.fd.precision = precision;
        def_p.u.fd.display_flags = display_flags;
        def_p.uu.id = static_cast<A_long>(core::hash_string(name));
        def_p.flags |= flags;
        def_p.ui_flags |= ui_flags;

        ::aetk::core::check_err(PF_ADD_PARAM(in_data_ptr(), -1, &def_p),
            "Failed to add fixed slider parameter");
        int32_t idx = current_count();
        register_callback(name, idx, nullptr);
        increment_param_count();
        return { *this, name, idx };
    }

    /**
     * @brief Opens a collapsible parameter topic group block.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean lambda block group wrapper.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param name Group name.
     * @param func Functor block populating the group.
     */
    void add_topic(const char* name,
        const std::function<void(const params_setup_context&)>& func) const {
        param_group group(*this, name);
        func(*this);
    }

    void add_topic(const char* name, int32_t flags,
        const std::function<void(const params_setup_context&)>& func) const {
        param_group group(*this, name, flags);
        func(*this);
    }

    /**
     * @brief Save callback pointer.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Saves registration.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param name Parameter name.
     * @param index Parameter index.
     * @param cb Target change callback.
     */
    void register_callback(
        const std::string& name, int index, param_change_callback_t cb) const {
        if (reg_func)
            reg_func(name, index, std::move(cb));
    }

    /**
     * @brief Registers a custom string key mapping to a parameter index.
     *
     * @param key Custom string key.
     * @param index Parameter index.
     */
    void register_key(const std::string& key, int index) const {
        if (reg_key_func)
            reg_key_func(key, index);
    }

    /**
     * @brief Registers a custom integer/enum key mapping to a parameter index.
     *
     * @param key Custom integer/enum key.
     * @param index Parameter index.
     */
    void register_int_key(int32_t key, int index) const {
        if (reg_int_key_func)
            reg_int_key_func(key, index);
    }

    /**
     * @brief Register the plugin to receive custom UI events.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard register helper.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param flags OR-ed combination of PF_CustomEFlag constants.
     */
    void register_custom_ui(int32_t flags,
                            A_short comp_w = 0, A_short comp_h = 0,
                            A_short layer_w = 0, A_short layer_h = 0) const {
        PF_CustomUIInfo ci;
        AEFX_CLR_STRUCT(ci);
        ci.events = static_cast<PF_CustomEventFlags>(flags);
        ci.comp_ui_width = comp_w;
        ci.comp_ui_height = comp_h;
        ci.comp_ui_alignment = PF_UIAlignment_NONE;
        ci.layer_ui_width = layer_w;
        ci.layer_ui_height = layer_h;
        ci.layer_ui_alignment = PF_UIAlignment_NONE;
        ci.preview_ui_width = 0;
        ci.preview_ui_height = 0;
        ci.preview_ui_alignment = PF_UIAlignment_NONE;
        custom_ui_registered = true;
        ::aetk::core::check_err(
            PF_REGISTER_UI(in_data_ptr(), &ci), "Failed to register custom UI");
    }

    /**
     * @brief Register the plugin to receive Composition, Layer, and Effect Custom UI events.
     *
     * @param flags Bitwise OR combination of PF_CustomEFlag constants (defaults to COMP | LAYER | EFFECT).
     */
    void register_comp_ui(int32_t flags = PF_CustomEFlag_COMP | PF_CustomEFlag_LAYER | PF_CustomEFlag_EFFECT) const {
        register_custom_ui(flags);
    }

    /**
     * @brief Set the custom name for the "Options..." button.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Renames standard option buttons.
     *
     * @warning <b>Memory & Lifecycles:</b> Fails silently in Premiere Pro.
     *
     * @param name The button label text.
     */
    void set_options_name(const char* name) const {
        aetk::core::suite<PF_EffectUISuite1> ui(::aetk::core::context::get_basic_suite());
        ui->PF_SetOptionsButtonName(in_data_ptr()->effect_ref, name);
    }
};

inline param_group::param_group(
    const params_setup_context& context, const char* name, int32_t flags)
    : ctx(context) {
    PF_ParamDef def_p;
    AEFX_CLR_STRUCT(def_p);
    def_p.param_type = PF_Param_GROUP_START;
    PF_STRNNCPY(def_p.PF_DEF_NAME, name, sizeof(def_p.PF_DEF_NAME));
    def_p.flags |= flags;
    def_p.uu.id = static_cast<A_long>(core::hash_string(name));

    aetk::core::check_err(PF_ADD_PARAM(ctx.in_data_ptr(), -1, &def_p),
        "Failed to add group start parameter");
    ctx.increment_param_count();
}

inline param_group::~param_group() {
    PF_ParamDef def_p;
    AEFX_CLR_STRUCT(def_p);
    def_p.param_type = PF_Param_GROUP_END;

    // We don't check_err in destructors to avoid throwing exceptions
    PF_ADD_PARAM(ctx.in_data_ptr(), -1, &def_p);
    ctx.increment_param_count();
}

inline param_builder& param_builder::on_change(param_change_callback_t cb) {
    ctx.register_callback(name, index, std::move(cb));
    return *this;
}

inline param_builder& param_builder::set_key(std::string key) {
    ctx.register_key(std::move(key), index);
    return *this;
}

template <typename KeyEnum> inline param_builder& param_builder::set_key(KeyEnum key) {
    if constexpr (std::is_pointer_v<KeyEnum>
        && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<KeyEnum>>, char>) {
        ctx.register_key(std::string(key), index);
    } else if constexpr (std::is_same_v<KeyEnum, std::string>) {
        ctx.register_key(key, index);
    } else {
        ctx.register_int_key(static_cast<int32_t>(key), index);
    }
    return *this;
}

} // namespace aetk::effect
