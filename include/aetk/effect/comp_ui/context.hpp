#pragma once

/**
 * @file context.hpp
 * @brief Context wrapper for Composition and Layer panel Custom UI events.
 */

#include <aetk/effect/context/context.hpp>
#include <aetk/ui/drawbot.hpp>
#include <aetk/core/suite.hpp>
#include <AE_EffectCBSuites.h>

namespace aetk::effect::comp_ui {

/**
 * @brief Context wrapper for Composition (PF_Window_COMP) and Layer (PF_Window_LAYER)
 * viewport Custom UI interactions and overlay rendering.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In raw AE SDK, converting between Layer space, Composition space,
 * and Viewport Frame space requires calling C callbacks (`layer_to_comp`, `source_to_frame`, `frame_to_source`,
 * `comp_to_layer`) with fixed-point 16.16 arithmetic. `comp_ui::context` encapsulates these transforms into
 * direct `layer_to_frame` and `frame_to_layer` C++ vector calls.
 */
class context : public interaction_context {
public:
    /**
     * @brief Construct comp_ui context from base interaction_context.
     */
    explicit context(const interaction_context& ictx)
        : interaction_context(ictx) {}

    /**
     * @brief Construct comp_ui context from base context.
     */
    explicit context(const ::aetk::effect::context& ctx)
        : interaction_context(ctx) {}

    /**
     * @brief Convert a point from Layer coordinates [x, y] to Viewport Frame coordinates [x, y].
     *
     * In Comp windows (PF_Window_COMP), layer_to_comp is invoked prior to source_to_frame.
     * In Layer windows (PF_Window_LAYER), source_to_frame is invoked directly.
     *
     * @param layer_pt Vector in layer space pixel coordinates.
     * @return Vector in viewport frame screen space coordinates.
     */
    core::vec2 layer_to_frame(const core::vec2& layer_pt) const {
        if (window() == window_type::comp) {
            core::vec2 comp_pt = layer_to_comp(layer_pt);
            return source_to_frame(comp_pt);
        }
        return source_to_frame(layer_pt);
    }

    /**
     * @brief Convert a Viewport Frame screen coordinate [x, y] back to Layer coordinates [x, y].
     *
     * @param frame_pt Vector in viewport frame screen space coordinates.
     * @return Vector in layer space pixel coordinates.
     */
    core::vec2 frame_to_layer(const core::vec2& frame_pt) const {
        core::vec2 src_pt = frame_to_source(frame_pt);
        if (window() == window_type::comp) {
            return comp_to_layer(src_pt);
        }
        return src_pt;
    }

    /**
     * @brief Check if the current event originated from the Composition panel.
     */
    bool is_comp() const {
        return window() == window_type::comp;
    }

    /**
     * @brief Check if the current event originated from the Layer panel.
     */
    bool is_layer() const {
        return window() == window_type::layer;
    }

    /**
     * @brief Get mouse click/drag position in Viewport Frame screen coordinates.
     */
    core::vec2 frame_mouse_point() const {
        return screen_point();
    }

    /**
     * @brief Get mouse click/drag position converted to Layer space coordinates.
     */
    core::vec2 layer_mouse_point() const {
        return frame_to_layer(screen_point());
    }

    /**
     * @brief Display two lines of text in After Effects' bottom Info Panel.
     *
     * @param line1 First line string.
     * @param line2 Second line string.
     */
    void info_draw_text(const char* line1, const char* line2) const {
        if (m_event_extra && m_event_extra->cbs.info_draw_text) {
            m_event_extra->cbs.info_draw_text(m_event_extra->cbs.refcon, line1, line2);
        }
    }

    /**
     * @brief Display a color swatch in After Effects' bottom Info Panel.
     *
     * @param col ARGB color pixel structure.
     */
    void info_draw_color(const PF_Pixel& col) const {
        if (m_event_extra && m_event_extra->cbs.info_draw_color) {
            m_event_extra->cbs.info_draw_color(m_event_extra->cbs.refcon, col);
        }
    }

    /**
     * @brief Request After Effects to route ongoing mouse drag events following a click.
     *
     * @param send_drag Set true to receive subsequent PF_Event_DRAG messages.
     * @param refcon_0 User handle ID stored across drag continuation calls.
     */
    void request_drag(bool send_drag = true, std::intptr_t refcon_0 = 1) const {
        if (m_event_extra && type() == event_type::click) {
            m_event_extra->u.do_click.send_drag = send_drag ? TRUE : FALSE;
            m_event_extra->u.do_click.continue_refcon[0] = refcon_0;
        }
    }

    /**
     * @brief Get the user state handle ID stored in continue_refcon[0].
     */
    std::intptr_t drag_refcon(std::size_t index = 0) const {
        if (m_event_extra && index < 4 && (type() == event_type::click || type() == event_type::drag)) {
            return m_event_extra->u.do_click.continue_refcon[index];
        }
        return 0;
    }

    /**
     * @brief Set the user state handle ID stored in continue_refcon[index].
     */
    void set_drag_refcon(std::size_t index, std::intptr_t val) const {
        if (m_event_extra && index < 4 && (type() == event_type::click || type() == event_type::drag)) {
            m_event_extra->u.do_click.continue_refcon[index] = val;
        }
    }

    /**
     * @brief Check if the current drag event is the final frame (mouse button released).
     */
    bool is_last_drag_frame() const {
        if (m_event_extra && (type() == event_type::drag || type() == event_type::click)) {
            return m_event_extra->u.do_click.last_time != FALSE;
        }
        return false;
    }

    /**
     * @brief Mark the event output flags as handled to prevent host overdraw or default handling.
     */
    void set_handled() const {
        if (m_event_extra) {
            m_event_extra->evt_out_flags |= PF_EO_HANDLED_EVENT;
        }
    }

    /**
     * @brief Request an immediate Viewport update (redraw).
     * Automatically sets PF_EO_UPDATE_NOW safely without breaking AE InvalidateRect event rules.
     */
    void request_update() const {
        if (m_event_extra) {
            m_event_extra->evt_out_flags |= PF_EO_UPDATE_NOW;
        }
    }
};

using comp_ui_context = context;

} // namespace aetk::effect::comp_ui
