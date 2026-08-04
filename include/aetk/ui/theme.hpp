#pragma once

#include <aetk/core/types.hpp>
#include <aetk/core/suite.hpp>
#include <AE_EffectSuites.h>

namespace aetk::ui {

using color = aetk::core::color<aetk::core::pixel_range::tkfloat>;

/**
 * @brief Holds the active color scheme for widget rendering.
 *
 * @details Populated from AE's native theme via PFAppSuite6::PF_AppGetColor,
 * or falls back to sensible dark-mode defaults if the suite is unavailable.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, custom draw styling requires hardcoded color bytes or procedural suite fetches that crash if the version is incompatible. `aetk::ui::theme` implements a query system that matches the host appearance across versions or uses safe fallbacks.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct theme {
    color bg;               // Panel background
    color bg_light;         // Slightly lighter background (cards, wells)
    color text;             // Primary text color
    color text_dim;         // Secondary/disabled text
    color accent;           // Interactive highlight (buttons, sliders)
    color accent_hover;     // Lighter accent for hover state
    color accent_pressed;   // Accent for pressed state
    color grid;             // Grid lines, separators
    color handle;           // Draggable handle (normal)
    color handle_active;    // Draggable handle (active/dragging)
    color border;           // Widget borders
    
    // Overlay Theme Suite properties (used for path strokes and vertices natively)
    color overlay_foreground;
    color overlay_shadow;
    float overlay_stroke_width = 1.0f;
    float overlay_vertex_size = 5.0f;
    core::vec2 overlay_shadow_offset = {1.0f, 1.0f};

    float font_size = 11.0f;
    float padding = 4.0f;
    float corner_radius = 2.0f;

    /**
     * @brief Build a theme from AE's native app colors.
     *
     * @details Queries PFAppSuite6 for panel background, text, and accent colors.
     * Falls back to default_dark() if the suite is unavailable.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces procedural APP theme requests with dynamically constructed values.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param pica Pointer to the PICA basic suite.
     * @return Resolved theme structure.
     */
    static theme from_ae(SPBasicSuite* pica) {
        theme t = default_dark(); // Start with defaults

        if (!pica) return t;

        try {
            core::suite<PFAppSuite6> app(pica, kPFAppSuite, kPFAppSuiteVersion6);

            // Helper: convert PF_App_Color (A_u_short 0-65535) to color (float 0-1)
            auto to_color = [](const PF_App_Color& c) -> color {
                constexpr float inv_65535 = 1.0f / 65535.0f;
                return {
                    1.0f,
                    c.red * inv_65535,
                    c.green * inv_65535,
                    c.blue * inv_65535
                };
            };

            PF_App_Color ae_color;

            // Panel background
            if (app->PF_AppGetBgColor(&ae_color) == PF_Err_NONE) {
                t.bg = to_color(ae_color);
            }

            // Text on lighter bg
            if (app->PF_AppGetColor(PF_App_Color_TEXT_ON_LIGHTER_BG, &ae_color) == PF_Err_NONE) {
                t.text = to_color(ae_color);
            }

            // Accent (hot text)
            if (app->PF_AppGetColor(PF_App_Color_HOT_TEXT, &ae_color) == PF_Err_NONE) {
                t.accent = to_color(ae_color);
                // Derive hover/pressed variants
                t.accent_hover = color(
                    (std::min)(1.0f, (float)t.accent.red * 1.2f),
                    (std::min)(1.0f, (float)t.accent.green * 1.2f),
                    (std::min)(1.0f, (float)t.accent.blue * 1.2f)
                );
            }

            if (app->PF_AppGetColor(PF_App_Color_HOT_TEXT_PRESSED, &ae_color) == PF_Err_NONE) {
                t.accent_pressed = to_color(ae_color);
            }

            // Shadow color (for widget wells/backgrounds — darker tone matching AE's native UI)
            if (app->PF_AppGetColor(PF_App_Color_SHADOW, &ae_color) == PF_Err_NONE) {
                t.bg_light = to_color(ae_color);
            }

            // Dark caption text (for dimmed/secondary text)
            if (app->PF_AppGetColor(PF_App_Color_DARK_CAPTION_TEXT, &ae_color) == PF_Err_NONE) {
                t.text_dim = to_color(ae_color);
            }

            // Derive grid from background (slightly brighter)
            t.grid = color(
                (std::min)(1.0f, (float)t.bg.red + 0.08f),
                (std::min)(1.0f, (float)t.bg.green + 0.08f),
                (std::min)(1.0f, (float)t.bg.blue + 0.08f)
            );
        } catch (const std::exception&) {
        }

        // Try to fetch specific Custom UI Overlay Theme properties (for drawing paths natively)
        try {
            core::suite<PF_EffectCustomUIOverlayThemeSuite1> overlay(pica, kPFEffectCustomUIOverlayThemeSuite, kPFEffectCustomUIOverlayThemeSuiteVersion1);
            
            DRAWBOT_ColorRGBA fg, shadow;
            if (overlay->PF_GetPreferredForegroundColor(&fg) == PF_Err_NONE) {
                t.overlay_foreground = color(fg.alpha, fg.red, fg.green, fg.blue);
            }
            if (overlay->PF_GetPreferredShadowColor(&shadow) == PF_Err_NONE) {
                t.overlay_shadow = color(shadow.alpha, shadow.red, shadow.green, shadow.blue);
            }
            
            float stroke_w, vertex_s;
            if (overlay->PF_GetPreferredStrokeWidth(&stroke_w) == PF_Err_NONE) {
                t.overlay_stroke_width = stroke_w;
            }
            if (overlay->PF_GetPreferredVertexSize(&vertex_s) == PF_Err_NONE) {
                t.overlay_vertex_size = vertex_s;
            }
            
            A_LPoint shadow_offset;
            if (overlay->PF_GetPreferredShadowOffset(&shadow_offset) == PF_Err_NONE) {
                t.overlay_shadow_offset = core::vec2(shadow_offset.x, shadow_offset.y);
            }
        } catch (const std::exception&) {
        }

        return t;
    }

    /**
     * @brief Sensible dark-mode defaults matching AE's standard appearance.
     *
     * @details Populates colors matching AE's carbon/charcoal interface.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces hardcoded fallback colors with an encapsulated function.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Initialized theme structure.
     */
    static theme default_dark() {
        theme t;
        t.bg              = color(0.10f, 0.10f, 0.10f);
        t.bg_light        = color(0.15f, 0.15f, 0.15f);
        t.text            = color(0.68f, 0.68f, 0.68f);
        t.text_dim        = color(0.40f, 0.40f, 0.40f);
        t.accent          = color(0.30f, 0.55f, 0.90f);
        t.accent_hover    = color(0.35f, 0.60f, 0.95f);
        t.accent_pressed  = color(0.22f, 0.42f, 0.72f);
        t.grid            = color(0.16f, 0.16f, 0.16f);
        t.handle          = color(1.00f, 0.80f, 0.00f);
        t.handle_active   = color(1.00f, 0.50f, 0.00f);
        t.border          = color(0.20f, 0.20f, 0.20f);
        t.font_size       = 11.0f;
        t.padding         = 4.0f;
        t.corner_radius   = 2.0f;
        return t;
    }
};

} // namespace aetk::ui
