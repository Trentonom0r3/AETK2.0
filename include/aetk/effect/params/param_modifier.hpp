#include <AE_Macros.h>
#pragma once

#include <AE_Effect.h>
#include <AE_EffectCB.h>
#include <aetk/core/types.hpp>
#include <aetk/core/suite.hpp>
#include <aetk/effect/params/param.hpp>
#include <AE_GeneralPlug.h>
#include <string>

namespace aetk::effect {

/**
 * @brief Base class for modifying parameter UI states dynamically.
 * 
 * Redefined as a compatibility wrapper inheriting from unified param_base.
 */
class param_modifier : public param_base {
public:
    param_modifier(PF_ParamDef* param, int32_t index, PF_InData* in_data)
        : param_base(param, in_data, index) {}
};

/**
 * @brief Dynamic modifier wrapper for dropdown popup choices.
 *
 * Redefined as a compatibility wrapper inheriting from unified popup_param.
 */
class popup_modifier : public popup_param {
public:
    popup_modifier(PF_ParamDef* param, int32_t index, PF_InData* in_data)
        : popup_param(param, in_data, index) {}
};

/**
 * @brief Dynamic modifier wrapper for color picker parameters.
 *
 * Redefined as a compatibility wrapper inheriting from unified color_param.
 */
class color_modifier : public color_param {
public:
    color_modifier(PF_ParamDef* param, int32_t index, PF_InData* in_data)
        : color_param(param, in_data, index) {}
};

/**
 * @brief Dynamic modifier wrapper for floating-point slider parameters.
 *
 * Redefined as a compatibility wrapper inheriting from unified float_slider_param.
 */
class slider_modifier : public float_slider_param {
public:
    slider_modifier(PF_ParamDef* param, int32_t index, PF_InData* in_data)
        : float_slider_param(param, in_data, index) {}
};

/**
 * @brief Dynamic modifier wrapper for fixed-point parameters.
 *
 * Redefined as a compatibility wrapper inheriting from unified fixed_slider_param.
 */
class fixed_slider_modifier : public fixed_slider_param {
public:
    fixed_slider_modifier(PF_ParamDef* param, int32_t index, PF_InData* in_data)
        : fixed_slider_param(param, in_data, index) {}
};

/**
 * @brief Dynamic modifier wrapper for checkbox parameters.
 *
 * Redefined as a compatibility wrapper inheriting from unified checkbox_param.
 */
class checkbox_modifier : public checkbox_param {
public:
    checkbox_modifier(PF_ParamDef* param, int32_t index, PF_InData* in_data)
        : checkbox_param(param, in_data, index) {}
};

/**
 * @brief Dynamic modifier wrapper for arbitrary parameters.
 *
 * Redefined as a compatibility wrapper inheriting from unified arbitrary_param.
 */
template <typename T>
class arbitrary_modifier : public arbitrary_param<T> {
public:
    arbitrary_modifier(PF_ParamDef* param, int32_t index, PF_InData* in_data)
        : arbitrary_param<T>(param, in_data, index) {}
};

} // namespace aetk::effect

