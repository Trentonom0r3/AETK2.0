#pragma once
#include <AE_Effect.h>
#include <AE_EffectCBSuites.h>
#include <aetk/ui/message.hpp>

namespace my_plugin::licensing {

// Returns true if the plugin should continue executing, false to halt.
inline PF_Err check_and_intercept(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data) {
    return PF_Err_NONE; // Open-source version has no license checks
}

inline void setup_ui_button(PF_InData* in_data) {
    if (!in_data || !in_data->effect_ref) return;
    if (in_data->appl_id != 'PrMr') {
        PF_EffectUISuite1* effect_ui = nullptr;
        if (in_data->pica_basicP) {
            in_data->pica_basicP->AcquireSuite(
                kPFEffectUISuite, kPFEffectUISuiteVersion1, (const void**)&effect_ui);
            if (effect_ui) {
                effect_ui->PF_SetOptionsButtonName(in_data->effect_ref, "Register");
                in_data->pica_basicP->ReleaseSuite(
                    kPFEffectUISuite, kPFEffectUISuiteVersion1);
            }
        }
    }
}

inline void show_registration_dialog(PF_InData* in_data) {
    aetk::ui::alert("SWARM v1.0.0\nLicense Status: Registered / Active", "SWARM License Registration");
}

inline void append_about_info(PF_InData* in_data, PF_OutData* out_data) {
    if (!out_data) return;
    snprintf(out_data->return_msg, sizeof(out_data->return_msg), "SWARM v1.0.0\rCyberpunk object tracking and plexus network visualization.\r\rRegistered");
}

inline bool should_draw_watermark() {
    return false; // Never watermark the open-source version
}

} // namespace my_plugin::licensing