#pragma once

#include <AE_Effect.h>
#include <AE_EffectCBSuites.h>
#include <cstdio>

#if __has_include(<aetk/effect/licensing_private.hpp>)
#include <aetk/effect/licensing_private.hpp>
#elif __has_include("licensing_private.hpp")
#include "licensing_private.hpp"
#endif

// Fallback stub definition if licensing_private.hpp did not define namespace aetk::effect::licensing
#ifndef LIC_PRODUCT_NAME

namespace aetk::effect::licensing {

/**
 * @brief Intercepts After Effects plugin commands for licensing verification.
 * 
 * Default stub: Always returns PF_Err_NONE (unlicensed / free mode).
 */
inline PF_Err check_and_intercept(PF_Cmd, PF_InData*, PF_OutData*) {
    return PF_Err_NONE;
}

/**
 * @brief Configures options button label in Effect Controls panel.
 * 
 * Default stub: No-op.
 */
inline void setup_ui_button(PF_InData*) {}

/**
 * @brief Displays registration/activation dialog.
 * 
 * Default stub: No-op.
 */
inline void show_registration_dialog(PF_InData*, const char* = "1.0.0") {}

/**
 * @brief Formats plugin About box information.
 * 
 * Default stub: Appends basic plugin name and description.
 */
inline void append_about_info(PF_InData*, PF_OutData* out_data, const char* plugin_name, const char* desc) {
    if (out_data) {
        snprintf(out_data->return_msg, sizeof(out_data->return_msg), "%s\r%s", plugin_name, desc);
    }
}

/**
 * @brief Determines if trial watermark overlay should be rendered.
 * 
 * Default stub: Returns false (no watermark).
 */
inline bool should_draw_watermark() {
    return false;
}

} // namespace aetk::effect::licensing

#endif
