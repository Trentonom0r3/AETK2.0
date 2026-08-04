#pragma once

#include <aetk/core/context.hpp>
#include <aetk/core/error.hpp>
#include <aetk/core/suite.hpp>
#include <aetk/core/types.hpp>
#include <aetk/effect/pixel/smart_world.hpp>

#include <AE_Effect.h>
#include <AE_GeneralPlug.h>

#include <cstdint>
#include <optional>
#include <utility>

namespace aetk::effect {
using aetk::core::pixel_range;

/**
 * @brief Base class for all parameter wrappers.
 *
 * @details This class handles both "viewing" a parameter from a host-provided
 * array and "owning" a parameter that was checked out via the checkout_param
 * callback.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, accessing parameters
 * requires manually calling `PF_CHECKOUT_PARAM` with temporal steps and scale
 * variables, returning a raw struct `PF_ParamDef`, which must then be manually
 * checked back in using `PF_CHECKIN_PARAM` to prevent host leaks.
 * `aetk::effect::param_base` provides a robust, move-only RAII context wrapper
 * that automates the checkout/check-in lifecycle in its constructor/destructor
 * pairs cleanly.
 *
 * @warning <b>Memory & Lifecycles:</b> The wrapper manages the scoped checkout
 * lifecycle of host-owned memory buffers. Upon destruction, it automatically
 * invokes `PF_CHECKIN_PARAM`. Standard copy operations are strictly disabled to
 * prevent double-free handle errors.
 */
class param_base {
public:
    /**
     * @brief Construct from an existing PF_ParamDef (no ownership).
     *
     * @note <b>AE SDK Paradigm Shift:</b> Non-owning parameter definition
     * wrapper.
     *
     * @warning <b>Memory & Lifecycles:</b> Binds an external parameter without
     * acquiring checkout ownership.
     *
     * @param def Raw parameter pointer.
     * @param in_data Input struct parameter.
     * @param index The zero-based parameter index.
     */
    explicit param_base(
        PF_ParamDef* def, PF_InData* in_data = nullptr, int32_t index = -1)
        : m_def(def)
        , m_in_data(in_data)
        , m_index(index) {
    }

    /**
     * @brief Construct via implicit checkout (acquires ownership).
     *
     * @note <b>AE SDK Paradigm Shift:</b> Instantly checks out the parameter at
     * the active time.
     *
     * @warning <b>Memory & Lifecycles:</b> Acquires checkout ownership of the
     * parameter. AE host requires checking in the param when rendering completes.
     *
     * @param in_data The host input data containing the checkout callback.
     * @param index The zero-based parameter index.
     */
    param_base(PF_InData* in_data, int32_t index)
        : m_in_data(in_data)
        , m_index(index) {
        if (m_in_data) {
            PF_Err err = PF_CHECKOUT_PARAM(m_in_data, static_cast<PF_ParamIndex>(index),
                m_in_data->current_time, m_in_data->time_step, m_in_data->time_scale,
                &m_owned_def);
            if (err == PF_Err_NONE) {
                m_def = &m_owned_def;
            }
        }
    }

    /**
     * @brief Construct via implicit bypass (no-op checkout).
     *
     * @note <b>AE SDK Paradigm Shift:</b> Event bypass.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param in_data Input struct parameter.
     * @param index Parameter index.
     * @param skip_checkout Parameter skip token.
     */
    param_base(PF_InData* in_data, int32_t index, bool skip_checkout)
        : m_in_data(in_data)
        , m_index(index) {
        // No-op constructor for events where checkout is forbidden
    }

    /**
     * @brief Construct via explicit temporal checkout (acquires ownership).
     *
     * Uses PF_CHECKOUT_PARAM to fetch the parameter exactly as it evaluates at
     * the specified timestamp. This is critical for effects that need to look
     * forwards or backwards in time (like temporal blending or motion blur).
     *
     * @note <b>AE SDK Paradigm Shift:</b> Checks out the parameter exactly as it
     * evaluates at the specified timestamp.
     *
     * @warning <b>Memory & Lifecycles:</b> Acquires temporal checkout ownership.
     * AE host requires checking in the param when rendering completes.
     *
     * @param in_data The host input data containing the checkout callback.
     * @param index The zero-based parameter index.
     * @param time The specific timestamp to fetch the parameter at.
     * @param time_step The step size for the timestamp.
     * @param time_scale The scale of the timestamp.
     */
    param_base(PF_InData* in_data, int32_t index, A_long time, A_long time_step,
        A_u_long time_scale)
        : m_in_data(in_data)
        , m_index(index) {
        if (m_in_data) {
            PF_Err err = PF_CHECKOUT_PARAM(m_in_data, static_cast<PF_ParamIndex>(index),
                time, time_step, time_scale, &m_owned_def);
            if (err == PF_Err_NONE) {
                m_def = &m_owned_def;
            }
        }
    }

    param_base(const param_base&) = delete;
    param_base& operator=(const param_base&) = delete;

    /**
     * @brief Move constructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe move semantics.
     *
     * @warning <b>Memory & Lifecycles:</b> Transfers parameter checkout ownership
     * cleanly.
     *
     * @param other Rvalue parameter base.
     */
    param_base(param_base&& other) noexcept
        : m_def(other.m_def)
        , m_in_data(other.m_in_data)
        , m_owned_def(other.m_owned_def)
        , m_index(other.m_index) {
        if (m_def == &other.m_owned_def) {
            m_def = &m_owned_def;
        }
        other.m_def = nullptr;
        other.m_in_data = nullptr;
        other.m_index = -1;
    }

    /**
     * @brief Move assignment operator.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe move semantics.
     *
     * @warning <b>Memory & Lifecycles:</b> Transfers parameter checkout ownership
     * cleanly.
     *
     * @param other Rvalue parameter base.
     * @return Reference to this parameter wrapper.
     */
    param_base& operator=(param_base&& other) noexcept {
        if (this != &other) {
            checkin();
            m_def = other.m_def;
            m_in_data = other.m_in_data;
            m_owned_def = other.m_owned_def;
            m_index = other.m_index;
            if (m_def == &other.m_owned_def) {
                m_def = &m_owned_def;
            }
            other.m_def = nullptr;
            other.m_in_data = nullptr;
            other.m_index = -1;
        }
        return *this;
    }

    /**
     * @brief Parameter destructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> RAII parameter check-in.
     *
     * @warning <b>Memory & Lifecycles:</b> Calls `PF_CHECKIN_PARAM`
     * automatically. Never throws exceptions.
     */
    ~param_base() {
        checkin();
    }

    /**
     * @brief Check if the parameter contains a valid host definition.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw C union bitwise queries
     * with type-safe properties.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return True if valid.
     */
    bool valid() const noexcept {
        return m_def != nullptr;
    }

    /**
     * @brief Access underlying AE parameter type enum.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw C union bitwise queries
     * with type-safe properties.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Parameter type enum.
     */
    PF_ParamType type() const noexcept {
        return m_def ? m_def->param_type : static_cast<PF_ParamType>(PF_Param_RESERVED);
    }

    /**
     * @brief Mark the parameter as changed so AE records the new value.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw C union bitwise queries
     * with type-safe properties.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void mark_changed() {
        if (m_def)
            m_def->uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE;
    }

    /**
     * @brief Retrieve underlying raw parameter pointer.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Raw pointer accessor.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Raw PF_ParamDef pointer.
     */
    PF_ParamDef* def_ptr() const {
        return m_def;
    }

    /**
     * @brief Retrieve display name of the parameter.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw C union bitwise queries
     * with type-safe properties.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Parameter name.
     */
    const char* name() const noexcept {
        return m_def ? m_def->PF_DEF_NAME : "";
    }

    /**
     * @brief Retrieve parameter option flags.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw C union bitwise queries
     * with type-safe properties.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Parameter flags.
     */
    PF_ParamFlags flags() const noexcept {
        return m_def ? m_def->flags : PF_ParamFlag_NONE;
    }

    /**
     * @brief Retrieve parameter UI flags.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw C union bitwise queries
     * with type-safe properties.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Parameter UI flags.
     */
    PF_ParamUIFlags ui_flags() const noexcept {
        return m_def ? m_def->ui_flags : PF_PUI_NONE;
    }

    /**
     * @brief Hide the parameter in the Effect Controls panel.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw bitmask alterations with a
     * host-aware update.
     */
    void hide() {
        if (!m_def)
            return;
        m_def->ui_flags |= PF_PUI_INVISIBLE;

        bool has_aegp = false;
#ifndef AETK_PREMIERE_COMPAT
        AEGP_PluginID aegp_id = 0;
        aegp_id = aetk::core::context::plugin_id.load();
        if (aegp_id != 0 && m_in_data && m_in_data->pica_basicP) {
            const AEGP_DynamicStreamSuite4* dss_ptr = nullptr;
            if (m_in_data->pica_basicP->AcquireSuite(kAEGPDynamicStreamSuite,
                    kAEGPDynamicStreamSuiteVersion4, (const void**)&dss_ptr)
                == PF_Err_NONE) {
                m_in_data->pica_basicP->ReleaseSuite(
                    kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion4);
                has_aegp = true;
            }
        }

        if (has_aegp && m_in_data) {
            aetk::core::suite<AEGP_PFInterfaceSuite1> pfi(m_in_data->pica_basicP);
            aetk::core::suite<AEGP_StreamSuite2> ss(m_in_data->pica_basicP);
            aetk::core::suite<AEGP_DynamicStreamSuite4> dss(m_in_data->pica_basicP);
            aetk::core::suite<AEGP_EffectSuite4> es(m_in_data->pica_basicP);

            AEGP_EffectRefH meH = nullptr;
            pfi->AEGP_GetNewEffectForEffect(aegp_id, m_in_data->effect_ref, &meH);
            if (meH) {
                AEGP_StreamRefH streamH = nullptr;
                aetk::core::check_err(
                    ss->AEGP_GetNewEffectStreamByIndex(aegp_id, meH, m_index, &streamH));
                if (streamH) {
                    aetk::core::check_err(dss->AEGP_SetDynamicStreamFlag(
                        streamH, AEGP_DynStreamFlag_HIDDEN, FALSE, TRUE));
                    aetk::core::check_err(ss->AEGP_DisposeStream(streamH));
                }
                aetk::core::check_err(es->AEGP_DisposeEffect(meH));
            }
        }
#endif

        if (!has_aegp) {
            if (m_in_data && m_in_data->pica_basicP) {
                aetk::core::suite<PF_ParamUtilsSuite3> s(m_in_data->pica_basicP);
                aetk::core::check_err(
                    s->PF_UpdateParamUI(m_in_data->effect_ref, m_index, m_def));
            }
        }
    }

    /**
     * @brief Show the parameter in the Effect Controls panel.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw bitmask alterations with a
     * host-aware update.
     */
    void show() {
        if (!m_def)
            return;
        m_def->ui_flags &= ~PF_PUI_INVISIBLE;

        bool has_aegp = false;
#ifndef AETK_PREMIERE_COMPAT
        AEGP_PluginID aegp_id = 0;
        aegp_id = aetk::core::context::plugin_id.load();
        if (aegp_id != 0 && m_in_data && m_in_data->pica_basicP) {
            const AEGP_DynamicStreamSuite4* dss_ptr = nullptr;
            if (m_in_data->pica_basicP->AcquireSuite(kAEGPDynamicStreamSuite,
                    kAEGPDynamicStreamSuiteVersion4, (const void**)&dss_ptr)
                == PF_Err_NONE) {
                m_in_data->pica_basicP->ReleaseSuite(
                    kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion4);
                has_aegp = true;
            }
        }

        if (has_aegp && m_in_data) {
            aetk::core::suite<AEGP_PFInterfaceSuite1> pfi(m_in_data->pica_basicP);
            aetk::core::suite<AEGP_StreamSuite2> ss(m_in_data->pica_basicP);
            aetk::core::suite<AEGP_DynamicStreamSuite4> dss(m_in_data->pica_basicP);
            aetk::core::suite<AEGP_EffectSuite4> es(m_in_data->pica_basicP);

            AEGP_EffectRefH meH = nullptr;
            pfi->AEGP_GetNewEffectForEffect(aegp_id, m_in_data->effect_ref, &meH);
            if (meH) {
                AEGP_StreamRefH streamH = nullptr;
                ss->AEGP_GetNewEffectStreamByIndex(aegp_id, meH, m_index, &streamH);
                if (streamH) {
                    dss->AEGP_SetDynamicStreamFlag(
                        streamH, AEGP_DynStreamFlag_HIDDEN, FALSE, FALSE);
                    ss->AEGP_DisposeStream(streamH);
                }
                es->AEGP_DisposeEffect(meH);
            }
        }
#endif

        if (!has_aegp) {
            if (m_in_data && m_in_data->pica_basicP) {
                aetk::core::suite<PF_ParamUtilsSuite3> s(m_in_data->pica_basicP);
                aetk::core::check_err(
                    s->PF_UpdateParamUI(m_in_data->effect_ref, m_index, m_def));
            }
        }
    }

    /**
     * @brief Disable parameter interactions.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw bitmask alterations.
     */
    void disable() {
        if (m_def)
            m_def->ui_flags |= PF_PUI_DISABLED;
    }

    /**
     * @brief Enable parameter interactions.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw bitmask alterations.
     */
    void enable() {
        if (m_def)
            m_def->ui_flags &= ~PF_PUI_DISABLED;
    }

    /**
     * @brief Expand parameter layout group twirls.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw bitmask alterations.
     */
    void twirl_open() {
        if (m_def)
            m_def->flags &= ~PF_ParamFlag_COLLAPSE_TWIRLY;
    }

    /**
     * @brief Collapse parameter layout group twirls.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw bitmask alterations.
     */
    void twirl_closed() {
        if (m_def)
            m_def->flags |= PF_ParamFlag_COLLAPSE_TWIRLY;
    }

    /**
     * @brief Commits modified parameter properties back to the host UI.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Streamlines parameter updates.
     */
    void commit() {
        if (m_def && m_in_data && m_in_data->pica_basicP) {
            aetk::core::suite<PF_ParamUtilsSuite3> s(m_in_data->pica_basicP);
            aetk::core::check_err(
                s->PF_UpdateParamUI(m_in_data->effect_ref, m_index, m_def));
        }
    }

protected:
    /**
     * @brief Checks the parameter back into the host.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Underlying check-in transaction
     * wrapper.
     *
     * @warning <b>Memory & Lifecycles:</b> Invokes `PF_CHECKIN_PARAM` if owning.
     */
    void checkin() noexcept {
        if (m_in_data && m_def == &m_owned_def) {
            PF_CHECKIN_PARAM(m_in_data, &m_owned_def);
            m_in_data = nullptr;
            m_def = nullptr;
        }
    }

    PF_ParamDef* m_def = nullptr;
    PF_InData* m_in_data = nullptr;
    PF_ParamDef m_owned_def { };
    int32_t m_index = -1;
};

/**
 * @brief Wrapper for standard Slider parameters (A_long).
 *
 * @note <b>AE SDK Paradigm Shift:</b> Type-safe integer slider parameter.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
class slider_param : public param_base {
public:
    using param_base::param_base;

    /**
     * @brief Get integer value.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean slider query.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Long slider value.
     */
    int32_t value() const noexcept {
        return m_def ? static_cast<int32_t>(m_def->u.sd.value) : 0;
    }

    /**
     * @brief Assign a new integer value.
     */
    void set_value(int32_t val) {
        if (m_def) {
            m_def->u.sd.value = val;
            m_def->uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE;
        }
    }
};

/**
 * @brief Wrapper for Float Slider parameters (double).
 *
 * @note <b>AE SDK Paradigm Shift:</b> Type-safe float slider parameter.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
class float_slider_param : public param_base {
public:
    using param_base::param_base;

    /**
     * @brief Get double slider value.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean float slider query.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Floating point slider value.
     */
    double value() const noexcept {
        return m_def ? m_def->u.fs_d.value : 0.0;
    }

    /**
     * @brief Assign a new float value.
     */
    void set_value(double val) {
        if (m_def) {
            m_def->u.fs_d.value = val;
            m_def->uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE;
        }
    }
};

/**
 * @brief Wrapper for Fixed Slider parameters (float).
 */
class fixed_slider_param : public param_base {
public:
    using param_base::param_base;

    /**
     * @brief Get fixed-point slider value.
     */
    float value() const noexcept {
        return m_def ? static_cast<float>(m_def->u.fd.value) / 65536.0f : 0.0f;
    }

    /**
     * @brief Assign a new fixed-point slider value.
     */
    void set_value(float v) {
        if (m_def) {
            m_def->u.fd.value = static_cast<A_long>(v * 65536.0f);
            m_def->uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE;
        }
    }
};

/**
 * @brief Wrapper for Angle parameters (degrees).
 *
 * @note <b>AE SDK Paradigm Shift:</b> Type-safe angle parameter.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
class angle_param : public param_base {
public:
    using param_base::param_base;

    /**
     * @brief Get rotation angle in degrees.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Converts raw fixed-point values to
     * standard degrees.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Bounded angle float.
     */
    double value() const noexcept {
        return m_def ? static_cast<double>(m_def->u.ad.value) / 65536.0 : 0.0;
    }
};

/**
 * @brief Wrapper for Checkbox parameters (bool).
 *
 * @note <b>AE SDK Paradigm Shift:</b> Type-safe boolean checkbox parameter.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
class checkbox_param : public param_base {
public:
    using param_base::param_base;

    /**
     * @brief Get checkbox selected state.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean checkbox query.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return True if selected.
     */
    bool value() const noexcept {
        return m_def ? m_def->u.bd.value != 0 : false;
    }

    /**
     * @brief Assign a new checkbox state.
     */
    void set_value(bool val) {
        if (m_def) {
            m_def->u.bd.value = val ? TRUE : FALSE;
            m_def->uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE;
        }
    }
};

/**
 * @brief Wrapper for Color parameters.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Type-safe color parameter.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
class color_param : public param_base {
public:
    using param_base::param_base;

    /**
     * @brief Get parameter color.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Translates host color values to AETK
     * normalized colors.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Color structure.
     */
    template <pixel_range Range = pixel_range::tkfloat>
    core::color<Range> value() const noexcept {
        if (!m_def) {
            if constexpr (Range == pixel_range::tkfloat) return { 1.0, 0.0, 0.0, 0.0 };
            else return { 255.0, 0.0, 0.0, 0.0 };
        }
        if constexpr (Range == pixel_range::tkfloat) {
            constexpr double inv_255 = 1.0 / 255.0;
            return { static_cast<double>(m_def->u.cd.value.alpha) * inv_255,
                static_cast<double>(m_def->u.cd.value.red) * inv_255,
                static_cast<double>(m_def->u.cd.value.green) * inv_255,
                static_cast<double>(m_def->u.cd.value.blue) * inv_255 };
        } else {
            return { static_cast<double>(m_def->u.cd.value.alpha),
                static_cast<double>(m_def->u.cd.value.red),
                static_cast<double>(m_def->u.cd.value.green),
                static_cast<double>(m_def->u.cd.value.blue) };
        }
    }

    /**
     * @brief Assign a new color value.
     */
    template <pixel_range Range = pixel_range::tkfloat>
    void set_value(const core::color<Range>& c) {
        if (m_def) {
            if constexpr (Range == pixel_range::tkfloat) {
                m_def->u.cd.value.alpha
                    = static_cast<A_u_char>(std::clamp(c.alpha * 255.0, 0.0, 255.0));
                m_def->u.cd.value.red
                    = static_cast<A_u_char>(std::clamp(c.red * 255.0, 0.0, 255.0));
                m_def->u.cd.value.green
                    = static_cast<A_u_char>(std::clamp(c.green * 255.0, 0.0, 255.0));
                m_def->u.cd.value.blue
                    = static_cast<A_u_char>(std::clamp(c.blue * 255.0, 0.0, 255.0));
            } else {
                m_def->u.cd.value.alpha
                    = static_cast<A_u_char>(std::clamp(c.alpha, 0.0, 255.0));
                m_def->u.cd.value.red
                    = static_cast<A_u_char>(std::clamp(c.red, 0.0, 255.0));
                m_def->u.cd.value.green
                    = static_cast<A_u_char>(std::clamp(c.green, 0.0, 255.0));
                m_def->u.cd.value.blue
                    = static_cast<A_u_char>(std::clamp(c.blue, 0.0, 255.0));
            }
            m_def->uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE;
        }
    }
};

/**
 * @brief Wrapper for Popup (Dropdown) parameters.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Type-safe dropdown popup param.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
class popup_param : public param_base {
public:
    using param_base::param_base;

    /**
     * @brief Get selected dropdown choice index.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean popup query.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return 1-based index selection.
     */
    int32_t value() const noexcept {
        return m_def ? static_cast<int32_t>(m_def->u.pd.value) : 0;
    }

    /**
     * @brief Assign a new dropdown index value.
     */
    void set_value(int32_t val) {
        if (m_def) {
            m_def->u.pd.value = val;
            m_def->uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE;
        }
    }

    /**
     * @brief Set choices items string dynamically.
     */
    void set_choices(const char* choices_str, int32_t num_choices) {
        if (m_def) {
            m_def->u.pd.u.PF_DEF_NAMESPTR = choices_str;
            m_def->u.pd.num_choices = num_choices;
        }
    }
};

/**
 * @brief Wrapper for Layer (World) parameters.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Type-safe layer parameter.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
class layer_param : public param_base {
public:
    using param_base::param_base;

    /**
     * @brief Access the pixels of the layer.
     *
     * Returns a non-owning smart_world view of the layer parameter's buffer.
     * Note: For SmartFX, use `ctx.checkout_pixels()` instead.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Easily binds layer parameter structures
     * to reusable `aetk::effect::smart_world` instances.
     *
     * @warning <b>Memory & Lifecycles:</b> The returned `smart_world` is a
     * non-owning view of the layer's pixels and must not outlive the
     * `layer_param` checkout.
     *
     * @return Non-owning smart world pixels view.
     */
    smart_world world() const noexcept {
        return smart_world(&m_def->u.ld, m_in_data, smart_world::ownership::NONE);
    }
};

/**
 * @brief Wrapper for 2D Point parameters.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Type-safe 2D point parameter.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
class point_param : public param_base {
public:
    using param_base::param_base;

    /**
     * @brief Get 2D point coordinates.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Translates fixed-point coordinates into
     * standard floats.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Point vector structure.
     */
    core::vec2 value() const noexcept {
        return core::vec2::from_fixed(m_def->u.td.x_value, m_def->u.td.y_value);
    }
};

/**
 * @brief Wrapper for 3D Point parameters.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Type-safe 3D point parameter.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
class point_3d_param : public param_base {
public:
    using param_base::param_base;

    /**
     * @brief Get 3D point coordinates.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct access to 3D point floating
     * values.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Point3D vector structure.
     */
    core::vec3 value() const noexcept {
        return { m_def->u.point3d_d.x_value, m_def->u.point3d_d.y_value,
            m_def->u.point3d_d.z_value };
    }
};

/**
 * @brief Wrapper for Arbitrary Data parameters.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Type-safe custom arbitrary data parameter
 * wrapper, encapsulating low-level handle locks and unlocking mechanisms
 * seamlessly.
 *
 * @warning <b>Memory & Lifecycles:</b> Manages handle locking states on
 * host-owned allocations. Users must pair locked pointers with explicit
 * `release` calls before checkout frames close.
 *
 * @tparam T Must be a POD struct.
 */
template <typename T> class arbitrary_param : public param_base {
public:
    using param_base::param_base;

    /**
     * @brief Constructs a viewing arbitrary wrapper.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Non-owning instantiation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param in_data Input struct parameter.
     * @param index Parameter index identifier.
     * @param skip_checkout Skip checkout token.
     */
    arbitrary_param(PF_InData* in_data, int32_t index, bool skip_checkout)
        : param_base(in_data, index, skip_checkout)
        , m_index(index) {
    }

    /**
     * @brief Locks and returns the custom type pointer.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw C handle locking with
     * compile-time type casts.
     *
     * @warning <b>Memory & Lifecycles:</b> Locks host memory handles. Must be
     * released with `release()` after use.
     *
     * @return Typed arbitrary pointer.
     */
    T* value() const noexcept {
        PF_Handle handle = nullptr;
        if (this->m_def) {
            handle = this->m_def->u.arb_d.value;
        }

        if (!handle)
            return nullptr;
        return reinterpret_cast<T*>((*this->m_in_data->utils->host_lock_handle)(handle));
    }

    /**
     * @brief Unlocks the arbitrary handle.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Streamlines raw handle unlocking.
     *
     * @warning <b>Memory & Lifecycles:</b> Safe unlocking of the host handle.
     */
    void release() const noexcept {
        PF_Handle handle = nullptr;
        if (this->m_def) {
            handle = this->m_def->u.arb_d.value;
        }

        if (handle) {
            (*this->m_in_data->utils->host_unlock_handle)(handle);
        }
    }

private:
    int32_t m_index = 0;
};

/**
 * @brief RAII wrapper for arbitrary_param locks.
 *
 * @details Locks an arbitrary param's underlying handle on construction and
 * safely releases it on destruction.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Safer scope-based resource management for
 * timeline arbitrary data, avoiding handle leaks.
 *
 * @tparam T Arbitrary data structure type.
 */
template <typename T> class locked_arbitrary {
public:
    /**
     * @brief Constructor that binds to an existing parameter reference
     * (non-owning).
     */
    explicit locked_arbitrary(const arbitrary_param<T>& param)
        : m_param_ptr(&param) {
        m_ptr = m_param_ptr->value();
    }

    /**
     * @brief Constructor that takes ownership of an rvalue parameter (owning).
     */
    explicit locked_arbitrary(arbitrary_param<T>&& param)
        : m_owned_param(std::move(param))
        , m_param_ptr(&*m_owned_param) {
        m_ptr = m_param_ptr->value();
    }

    /**
     * @brief Destructor safely releasing the handle.
     */
    ~locked_arbitrary() {
        if (m_ptr && m_param_ptr) {
            m_param_ptr->release();
        }
    }

    // Disable copy
    locked_arbitrary(const locked_arbitrary&) = delete;
    locked_arbitrary& operator=(const locked_arbitrary&) = delete;

    // Enable move
    locked_arbitrary(locked_arbitrary&& other) noexcept
        : m_owned_param(std::move(other.m_owned_param))
        , m_param_ptr(other.m_param_ptr)
        , m_ptr(other.m_ptr) {
        if (m_owned_param) {
            m_param_ptr = &*m_owned_param;
        }
        other.m_ptr = nullptr;
        other.m_param_ptr = nullptr;
    }

    locked_arbitrary& operator=(locked_arbitrary&& other) noexcept {
        if (this != &other) {
            if (m_ptr && m_param_ptr) {
                m_param_ptr->release();
            }
            m_owned_param = std::move(other.m_owned_param);
            m_param_ptr = other.m_param_ptr;
            if (m_owned_param) {
                m_param_ptr = &*m_owned_param;
            }
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
            other.m_param_ptr = nullptr;
        }
        return *this;
    }

    T* get() const noexcept {
        return m_ptr;
    }
    T* operator->() const noexcept {
        return m_ptr;
    }
    T& operator*() const noexcept {
        return *m_ptr;
    }
    explicit operator bool() const noexcept {
        return m_ptr != nullptr;
    }

private:
    std::optional<arbitrary_param<T>> m_owned_param;
    const arbitrary_param<T>* m_param_ptr = nullptr;
    T* m_ptr = nullptr;
};

} // namespace aetk::effect
