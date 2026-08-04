/**
 * AETK 2.0 Supervisor Sample
 *
 * Demonstrates parameter supervision using AETK's declarative API.
 * Matches the behavior of the SDK's Supervisor sample:
 *   - Mode popup (Basic/Advanced) controls visibility of color, slider, checkbox
 *   - Flavor popup choices change based on mode
 *   - Color updates to preset when flavor changes
 *   - Checkbox resets slider to 50%
 *   - Rendering blends input with the selected color: (input + color) / 2
 */

#include <aetk/effect.hpp>

using namespace aetk::effect;

// ── Constants (matching SDK Supervisor) ────────────────────────────
enum { MODE_BASIC = 1, MODE_ADVANCED, MODE_SIZE = MODE_ADVANCED };

// Flat param indices — no groups, matches original SDK layout exactly
enum {
    SUPER_INPUT = 0,
    SUPER_MODE,
    SUPER_FLAVOR,
    SUPER_COLOR,
    SUPER_SLIDER,
    SUPER_CHECKBOX,
    SUPER_NUM_PARAMS
};

static const char* BASIC_FLAVORS = "Chocolate|Strawberry|Sherbet";
static const char* ADV_FLAVORS   = "Exploding Snaps|Treacle Tart|Diracawl Slices|Butter Beer";

// ── Global data ───────────────────────────────────────────────────
struct supervisor_globals {
    bool          initializedB = false;
    AEGP_PluginID aegp_id = 0;
};

// ── Color presets (matching SDK) ──────────────────────────────────
static aetk::core::color<pixel_range::tkuint8> get_preset_color(A_long flavor, A_long mode) {
    aetk::core::color<pixel_range::tkuint8> c = {255.0, 0.0, 0.0, 0.0}; // black with full alpha default
    if (mode == MODE_BASIC) {
        switch (flavor) {
            case 1: c = {255.0, 136.0,  83.0,  51.0}; break; // Chocolate
            case 2: c = {255.0, 232.0,  21.0,  84.0}; break; // Strawberry
            case 3: c = {255.0, 255.0, 128.0,   0.0}; break; // Sherbet
            default: c = {255.0, 0.0, 255.0, 0.0}; break; // bright green for unmatched
        }
    } else {
        switch (flavor) {
            case 1: c = {255.0, 255.0, 215.0,   0.0}; break; // Exploding Snaps
            case 2: c = {255.0, 101.0,  67.0,  33.0}; break; // Treacle Tart
            case 3: c = {255.0, 139.0,  69.0,  19.0}; break; // Diracawl Slices
            case 4: c = {255.0, 255.0, 223.0,   0.0}; break; // Butter Beer
            default: c = {255.0, 0.0, 0.0, 0.0}; break; // black for unmatched
        }
    }
    return c;
}

// ══════════════════════════════════════════════════════════════════
//  Plugin
// ══════════════════════════════════════════════════════════════════

class supervisor_plugin : public plugin<supervisor_plugin> {
public:

    // ── GLOBAL_SETUP ──────────────────────────────────────────────
    static void on_global_setup(const global_setup_context& ctx) {
        ctx.set_version(5, 11, 0);
        ctx.enable_smart_render();
        ctx.enable_param_supervision();  // Required for UPDATE_PARAMS_UI
        ctx.add_out_flags(PF_OutFlag_DEEP_COLOR_AWARE);
        ctx.add_out_flags2(PF_OutFlag2_FLOAT_COLOR_AWARE);

        supervisor_globals g;
        g.initializedB = TRUE;
        g.aegp_id = ctx.register_with_aegp("AETK Supervisor");
        ctx.set_global_data<supervisor_globals>(g);
    }

    // ── PARAMS_SETUP (flat — no groups) ───────────────────────────
    static void on_params_setup(const params_setup_context& ctx) {
        auto update_color = [](const user_changed_param_context& uctx) {
            aetk::core::color<pixel_range::tkuint8> preset = get_preset_color(uctx.int_val(SUPER_FLAVOR), uctx.int_val(SUPER_MODE));
            
            auto color = uctx.param_modifier<color_modifier>(SUPER_COLOR);
            color.set_value<pixel_range::tkuint8>(preset);
            color.commit();
            
            uctx.refresh_ui();
        };

        ctx.add_popup("Mode", MODE_SIZE, MODE_BASIC, "Basic|Advanced",
            PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP,
            PF_PUI_STD_CONTROL_ONLY)
            .on_change(update_color);

        ctx.add_popup("Flavor", 3, 1, BASIC_FLAVORS,
            PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP,
            PF_PUI_STD_CONTROL_ONLY)
            .on_change(update_color);

        ctx.add_color("Overlay Color", 136, 83, 51);

        ctx.add_fixed_slider("Intensity", 0.0f, 100.0f, 0.0f, 100.0f, 28.0f, 1, 1,
            PF_ParamFlag_EXCLUDE_FROM_HAVE_INPUTS_CHANGED);

        ctx.add_checkbox("Reset Slider", false, "Snap to 50%",
            PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY,
            PF_PUI_STD_CONTROL_ONLY)
            .on_change([](const user_changed_param_context& uctx) {
                if (uctx.bool_val(SUPER_CHECKBOX)) {
                    auto slider = uctx.param_modifier<fixed_slider_modifier>(SUPER_SLIDER);
                    slider.set_value(50.0f);
                    slider.commit();
                    
                    auto checkbox = uctx.param_modifier<checkbox_modifier>(SUPER_CHECKBOX);
                    checkbox.set_value(false);
                }
            });

    }



    // ── UPDATE_PARAMS_UI (visibility via AEGP streams) ────────────
    static void on_ui_update(ui_update_context& ctx) {
        auto* globP = ctx.global_data<supervisor_globals>();
        if (!globP) return;

        int mode_val = ctx.int_val(SUPER_MODE);
        A_Boolean hide_them = (mode_val == MODE_BASIC);

        // Update popup choices based on mode
        auto flavor = ctx.param_modifier<popup_modifier>(SUPER_FLAVOR);
        if (mode_val == MODE_ADVANCED) {
            flavor.set_choices(ADV_FLAVORS, 4);
        } else {
            flavor.set_choices(BASIC_FLAVORS, 3);
        }
        flavor.commit();

        if (mode_val == MODE_ADVANCED) {
            auto slider = ctx.param_modifier<fixed_slider_modifier>(SUPER_SLIDER);
            slider.twirl_open();
            slider.commit();
        }

        // Use host-aware param_modifier to hide/show advanced-only params
        auto color = ctx.param_modifier<color_modifier>(SUPER_COLOR);
        auto slider_val = ctx.param_modifier<fixed_slider_modifier>(SUPER_SLIDER);
        auto checkbox = ctx.param_modifier<checkbox_modifier>(SUPER_CHECKBOX);

        if (hide_them) {
            color.hide();
            slider_val.hide();
            checkbox.hide();
        } else {
            color.show();
            slider_val.show();
            checkbox.show();
        }
        color.commit();
        slider_val.commit();
        checkbox.commit();

        ctx.force_rerender();
        ctx.set_refresh_ui();
    }

    // ── PRE_RENDER ────────────────────────────────────────────────
    static void on_pre_render(const pre_render_context& ctx) {
        ctx.checkout_layer(SUPER_INPUT, SUPER_INPUT);
    }

    // ── SMART_RENDER ──────────────────────────────────────────────
    static void on_smart_render(const smart_render_context& ctx) {
        auto input  = ctx.checkout_pixels(SUPER_INPUT);
        auto output = ctx.checkout_output();

        // Get color param value using the ergonomic context helper
        auto cval = ctx.color_val<pixel_range::tkuint8>(SUPER_COLOR);

        // Blend: (input + color) / 2 per pixel
        input.iterate<pixel_range::tkuint8>(output,
            [&](int32_t x, int32_t y, aetk::core::color<pixel_range::tkuint8>& c) {
                c.red   = (c.red   + cval.red) / 2.0;
                c.green = (c.green + cval.green) / 2.0;
                c.blue  = (c.blue  + cval.blue) / 2.0;
                c.alpha = 255.0;
            });
    }

};

AETK_EFFECT_MAIN(supervisor_plugin)
