#pragma once

/**
 * @file theme.hpp
 * @brief Host theme integration wrapper for Composition and Layer panel overlays.
 */

#include <AE_EffectCBSuites.h>
#include <adobesdk/DrawbotSuite.h>
#include <aetk/core/suite.hpp>
#include <aetk/effect/comp_ui/context.hpp>
#include <optional>

namespace aetk::effect::comp_ui {

/**
 * @brief Host theme wrapper for Composition/Layer Custom UI overlays.
 * Wraps PF_EffectCustomUIOverlayThemeSuite1 with automatic Premiere Pro fallback.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Custom UI overlays in After Effects automatically
 * adapt to the user's host brightness preferences (dark vs light mode). `comp_ui::theme`
 * queries native host stroke colors, vertex sizes, and drop-shadow parameters, falling
 * back safely under Premiere Pro.
 */
class theme {
public:
    /**
     * @brief Construct comp_ui theme manager.
     */
    explicit theme(const context& ctx)
        : m_ctx(ctx) {
        if (m_ctx.in_data_ptr() && m_ctx.in_data_ptr()->appl_id != 'PrMr') {
            try {
                m_theme_suite.emplace(m_ctx.in_data_ptr()->pica_basicP);
            } catch (const std::exception&) {
                m_theme_suite.reset();
            }
        }
    }

    /**
     * @brief Check if host theme suite is available.
     */
    bool is_supported() const {
        return m_theme_suite.has_value();
    }

    /**
     * @brief Get host preferred foreground color for overlay strokes and text.
     */
    DRAWBOT_ColorRGBA get_preferred_foreground_color() const {
        DRAWBOT_ColorRGBA col = { 1.0f, 1.0f, 1.0f, 1.0f };
        if (m_theme_suite) {
            (*m_theme_suite)->PF_GetPreferredForegroundColor(&col);
        } else {
            col = { 0.9f, 0.9f, 0.9f, 1.0f };
        }
        return col;
    }

    /**
     * @brief Get host preferred shadow color for overlay drop-shadows.
     */
    DRAWBOT_ColorRGBA get_preferred_shadow_color() const {
        DRAWBOT_ColorRGBA col = { 0.0f, 0.0f, 0.0f, 0.6f };
        if (m_theme_suite) {
            (*m_theme_suite)->PF_GetPreferredShadowColor(&col);
        }
        return col;
    }

    /**
     * @brief Get host preferred stroke width.
     */
    float get_preferred_stroke_width() const {
        float w = 1.0f;
        if (m_theme_suite) {
            (*m_theme_suite)->PF_GetPreferredStrokeWidth(&w);
        }
        return w;
    }

    /**
     * @brief Get host preferred vertex handle size (control handle square side length).
     */
    float get_preferred_vertex_size() const {
        float s = 6.0f;
        if (m_theme_suite) {
            (*m_theme_suite)->PF_GetPreferredVertexSize(&s);
        }
        return s;
    }

    /**
     * @brief Stroke a path using host theme colors and drop-shadow.
     */
    void stroke_path(
        DRAWBOT_DrawRef draw_ref, DRAWBOT_PathRef path, bool draw_shadow = true) const {
        if (m_theme_suite && draw_ref && path) {
            (*m_theme_suite)->PF_StrokePath(draw_ref, path, draw_shadow ? TRUE : FALSE);
        }
    }

    /**
     * @brief Fill a path using host theme colors and drop-shadow.
     */
    void fill_path(
        DRAWBOT_DrawRef draw_ref, DRAWBOT_PathRef path, bool draw_shadow = true) const {
        if (m_theme_suite && draw_ref && path) {
            (*m_theme_suite)->PF_FillPath(draw_ref, path, draw_shadow ? TRUE : FALSE);
        }
    }

    /**
     * @brief Fill a vertex handle square centered at specified point using host theme
     * colors.
     */
    void fill_vertex(DRAWBOT_DrawRef draw_ref, const core::vec2& center,
        bool draw_shadow = true) const {
        if (m_theme_suite && draw_ref) {
            A_FloatPoint pt = { center.x, center.y };
            (*m_theme_suite)->PF_FillVertex(draw_ref, &pt, draw_shadow ? TRUE : FALSE);
        }
    }

private:
    const context& m_ctx;
    std::optional<aetk::core::suite<PF_EffectCustomUIOverlayThemeSuite1>> m_theme_suite;
};

using comp_ui_theme = theme;

} // namespace aetk::effect::comp_ui
