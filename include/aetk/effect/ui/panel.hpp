#pragma once

#include <aetk/effect/context/context.hpp>
#include <aetk/effect/ui/widget.hpp>
#include <aetk/ui/theme.hpp>
#include <chrono>

namespace aetk::effect::ui {

// ══════════════════════════════════════════════════════════════════════
//  Panel — Root UI dispatcher
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief The root panel that bridges the AETK UI framework to AE's event system.
 *
 * @details Manages:
 *  - Coordinate caching (ox/oy from the DRAW event frame)
 *  - Theme loading from AE's native colors
 *  - Event dispatch (DRAW → paint tree, CLICK/DRAG → DOM-style bubbling)
 *  - Active widget tracking for drag continuity
 *  - Hover tracking and cursor changes during ADJUST_CURSOR
 *
 * Usage in your plugin's on_event:
 *   static ui::panel my_panel(my_root_widget);
 *   my_panel.dispatch(ctx);
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, capturing custom interactive UI
 * panel events requires writing large, complex C-style nested switches in your effect's
 * event handler (`PF_Cmd_EVENT`), managing absolute screen space coordinate offsets
 * (`current_frame.left/top`), querying native colors from basic PICA suites, and manually
 * checking in custom cursors (`PF_CursorType`). `aetk::effect::ui::panel` acts as a
 * unified OOP root dispatcher bridging After Effects to your custom Drawbot widget
 * hierarchy. It maps screen coordinates automatically, tracks drag states across
 * rendering frames, manages cursor switches on hover, and routes drawing, keyboard, and
 * click events via highly optimized, predictable DOM-style event bubbles.
 *
 * @warning <b>Memory & Lifecycles:</b> The panel references a non-owning raw pointer to
 * your widget tree root (`m_root`). The caller must ensure that `m_root` remains
 * allocated and is not disposed while the panel processes dispatch events. Custom theme
 * loading caches PICA graphics colors, which are safe across sequence life limits.
 */
class panel {
public:
    /**
     * @brief Constructs the root UI panel.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Binds raw root layouts to custom panel
     * instances.
     *
     * @warning <b>Memory & Lifecycles:</b> The referenced root widget must outlive the
     * panel.
     *
     * @param root_widget Pointer to the root widget layout.
     */
    explicit panel(widget* root_widget)
        : m_root(root_widget) {
    }

    /**
     * @brief Single entry point — replaces all manual on_event logic.
     *
     * Call this from your plugin's on_event handler. The panel handles
     * coordinate mapping, event dispatch, and AE flag management.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces large procedural switch-cases with
     * dynamic virtual dispatch.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context from the host.
     */
    void dispatch(const interaction_context& ctx) {
        auto type = ctx.type();

        switch (type) {
        case interaction_context::event_type::draw: {
            PF_Rect frame = { };
            if (ctx.window() == interaction_context::window_type::effect) {
                if (ctx.event_extra()->effect_win.area == PF_EA_CONTROL) {
                    frame = ctx.event_extra()->effect_win.current_frame;
                } else if (ctx.event_extra()->effect_win.area == PF_EA_PARAM_TITLE) {
                    frame = ctx.event_extra()->effect_win.param_title_frame;
                }
            }
            handle_draw(ctx, frame);
            break;
        }
        case interaction_context::event_type::click:
            handle_click(ctx);
            break;
        case interaction_context::event_type::drag:
            handle_drag(ctx);
            break;
        case interaction_context::event_type::adjust_cursor:
            handle_adjust_cursor(ctx);
            break;
        case interaction_context::event_type::keydown:
            handle_keydown(ctx);
            break;
        case interaction_context::event_type::mouse_exited:
            handle_mouse_exited(ctx);
            break;
        default:
            break;
        }
    }

    /** @brief Bounded panel width in pixels. */
    float width() const {
        return m_width;
    }

    /** @brief Bounded panel height in pixels. */
    float height() const {
        return m_height;
    }

    /** @brief Horizontal screen space offset. */
    float ox() const {
        return m_cached_ox;
    }

    /** @brief Vertical screen space offset. */
    float oy() const {
        return m_cached_oy;
    }

    /** @brief Reference to cached UI drawing theme. */
    const theme& current_theme() const {
        return m_theme;
    }

    /** @brief Reference to the active drag target widget. */
    widget* active_widget() const {
        return m_active_widget;
    }

    /** @brief Reference to the hovered widget under cursor. */
    widget* hovered_widget() const {
        return m_hovered_widget;
    }

    /** @brief Reference to the active focused keyboard input widget. */
    widget* focused_widget() const {
        return m_focused_widget;
    }

private:
    widget* m_root;
    widget* m_active_widget = nullptr; // widget being dragged
    widget* m_hovered_widget = nullptr; // widget under cursor
    widget* m_focused_widget = nullptr; // widget receiving keyboard input
    bool m_title_active = false; // true when a title-area drag is in progress

    float m_cached_ox = 0;
    float m_cached_oy = 0;
    float m_width = 250;
    float m_height = 150;
    std::chrono::steady_clock::time_point m_hover_start_time
        = std::chrono::steady_clock::now();

    theme m_theme;
    bool m_theme_loaded = false;

    core::vec2 m_mouse_pos_screen { 0.0f, 0.0f };

    // ── Coordinate Mapping ────────────────────────────────────────────

    /**
     * @brief Translates screen coordinates to local panel coordinates.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Auto-offset mapping.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param screen_pt Relative coordinate vector.
     * @return Local coordinate vector.
     */
    core::vec2 to_local(const core::vec2& screen_pt) const {
        return { screen_pt.x - m_cached_ox, screen_pt.y - m_cached_oy };
    }

    // ── Event Handlers ────────────────────────────────────────────────

    /**
     * @brief High-performance drawing handler wrapping canvas transactions.
     *
     * @note <b>AE SDK Paradigm Shift:</b> High-performance internal event handlers
     * wrapping complex PICA transactions.
     *
     * @warning <b>Memory & Lifecycles:</b> Acquires a temporary Canvas handle and
     * releases it safely.
     *
     * @param ctx Bounded interaction context.
     * @param frame Bounded screen space frame dimensions.
     */
    void handle_draw(const interaction_context& ctx, const PF_Rect& frame) {
        // ── Title-Area Drawing (PF_EA_PARAM_TITLE) ──────────────────
        if (ctx.event_extra()
            && ctx.event_extra()->effect_win.area == PF_EA_PARAM_TITLE) {
            // Load theme once (or refresh if needed)
            if (!m_theme_loaded) {
                m_theme = theme::from_ae(::aetk::core::context::get_basic_suite());
                m_theme_loaded = true;
            }

            auto canvas = ctx.canvas();
            if (!canvas.valid())
                return;
            auto supplier = canvas.get_supplier();

            // Under PF_EA_PARAM_TITLE, the drawing frame bounds are passed in frame.
            // But we only want to paint in the space after the parameter name label, i.e.
            // starting from frame.left + horiz_offset. Since AE often passes
            // garbage/uninitialized memory for horiz_offset, we fall back to a safe 150px
            // divider default.
            float offset = 180.0f;
            if (ctx.event_extra()) {
                A_long raw_offset = ctx.event_extra()->effect_win.horiz_offset;
                float max_w = (float)(frame.right - frame.left);
                if (raw_offset > 10 && raw_offset < (max_w - 20.0f)) {
                    offset = (std::max)(180.0f, (float)raw_offset);
                }
            }

            float x = (float)frame.left + offset;
            float y = (float)frame.top;
            float w = (float)(frame.right - frame.left) - offset;
            float h = (float)(frame.bottom - frame.top);

            AETK_TRACE("[handle_draw] left={}, right={}, final_offset={}, x={}, w={}",
                frame.left, frame.right, offset, x, w);

            if (m_root) {
                m_root->paint_title(canvas, supplier, m_theme, x, y, w, h);
            }
            ctx.set_handled();
            return;
        }

        // ── Control-Area Drawing (PF_EA_CONTROL) — existing path ────
        // Update cached dimensions from the reliable DRAW frame
        float cur_w = (float)(frame.right - frame.left);
        float cur_h = (float)(frame.bottom - frame.top);
        if (cur_w > 10)
            m_width = cur_w;
        if (cur_h > 10)
            m_height = cur_h;
        m_cached_ox = (float)frame.left;
        m_cached_oy = (float)frame.top;

        // Load theme once (or refresh if needed)
        if (!m_theme_loaded) {
            m_theme = theme::from_ae(::aetk::core::context::get_basic_suite());
            m_theme_loaded = true;
        }

        auto canvas = ctx.canvas();
        if (!canvas.valid())
            return;
        auto supplier = canvas.get_supplier();

        // Layout the widget tree with 2px padding inset on all sides to avoid overlap
        // with AE's selection outlines
        if (m_root) {
            float layout_w = m_width - 4.0f;
            float layout_h = m_height - 4.0f;
            if (m_root->layout.max_width > 0.0f) {
                layout_w = (std::min)(layout_w, m_root->layout.max_width);
            }
            if (m_root->layout.max_height > 0.0f) {
                layout_h = (std::min)(layout_h, m_root->layout.max_height);
            }
            m_root->do_layout(m_cached_ox + 2.0f, m_cached_oy + 2.0f, layout_w, layout_h);
        }

        // Update hovered widget based on current layout and mouse position
        bool mouse_in_bounds = (m_mouse_pos_screen.x >= m_cached_ox
            && m_mouse_pos_screen.x <= m_cached_ox + m_width
            && m_mouse_pos_screen.y >= m_cached_oy
            && m_mouse_pos_screen.y <= m_cached_oy + m_height);

        widget* current_hover = nullptr;
        if (mouse_in_bounds && m_root) {
            current_hover
                = m_root->find_widget_at(m_mouse_pos_screen.x, m_mouse_pos_screen.y);
        }

        if (current_hover != m_hovered_widget) {
            if (m_hovered_widget)
                m_hovered_widget->on_hover_exit();
            if (current_hover)
                current_hover->on_hover_enter();
            m_hovered_widget = current_hover;
            m_hover_start_time = std::chrono::steady_clock::now();
        }

        // Draw background
        canvas.fill_rect(m_cached_ox, m_cached_oy, m_width, m_height, m_theme.bg);

        // Paint the widget tree
        if (m_root) {
            m_root->paint(canvas, supplier, m_theme);
        }

        // Draw overlay tooltip
        if (m_hovered_widget && !m_hovered_widget->tooltip().empty()
            && m_height >= 20.0f) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_hover_start_time)
                               .count();

            if (elapsed < 2800) {
                float tooltip_alpha = 1.0f;
                if (elapsed > 2000) {
                    tooltip_alpha = 1.0f - (static_cast<float>(elapsed - 2000) / 800.0f);
                    // Request another redraw to animate the fade out smoothly
                    ctx.invalidate();
                }

                if (supplier.supports_text()) {
                    float f_size = m_theme.font_size - 1.0f;
                    if (m_height < 45.0f) {
                        f_size = (std::min)(f_size,
                            8.5f); // Slightly smaller font for short panels
                    }
                    auto font = supplier.create_font(f_size);
                    std::string text = m_hovered_widget->tooltip();

                    // Character width estimate for text bounds (sans-serif)
                    float char_w = f_size * 0.75f;
                    float pad_x = 8.0f;
                    float pad_y = (m_height < 45.0f) ? 3.0f : 6.0f;
                    float line_spacing = 3.0f;
                    float line_h = f_size + line_spacing;

                    // Max width of the tooltip box is the panel width minus margin
                    float max_box_w = m_width - 24.0f;
                    size_t max_chars
                        = static_cast<size_t>((std::max)(15.0f, max_box_w / char_w));

                    std::vector<std::string> lines;
                    if (m_height < 45.0f) {
                        lines.push_back(truncate_text(text, max_chars));
                    } else {
                        lines = wrap_text(text, max_chars);
                    }

                    if (!lines.empty()) {
                        float max_line_len = 0.0f;
                        for (const auto& line : lines) {
                            float len = line.length() * char_w;
                            if (len > max_line_len)
                                max_line_len = len;
                        }

                        float tip_w = max_line_len + pad_x * 2.0f;
                        float tip_h = lines.size() * line_h - line_spacing + pad_y * 2.0f;

                        // Position tooltip centered horizontally relative to the mouse,
                        // placed slightly above or below it
                        float tx = m_mouse_pos_screen.x - tip_w * 0.5f;
                        float ty = m_mouse_pos_screen.y - tip_h
                            - 12.0f; // Default: above the mouse cursor

                        // If it goes above the top of the panel, place it below the
                        // cursor instead
                        if (ty < m_cached_oy + 4.0f) {
                            ty = m_mouse_pos_screen.y + 20.0f;
                        }

                        // Constrain horizontally to remain within the panel borders (with
                        // a 4px margin)
                        if (tx < m_cached_ox + 4.0f) {
                            tx = m_cached_ox + 4.0f;
                        }
                        if (tx + tip_w > m_cached_ox + m_width - 4.0f) {
                            tx = m_cached_ox + m_width - 4.0f - tip_w;
                        }

                        // Constrain vertically to remain within the panel borders
                        if (ty < m_cached_oy + 4.0f) {
                            ty = m_cached_oy + 4.0f;
                        }
                        if (ty + tip_h > m_cached_oy + m_height - 4.0f) {
                            ty = (std::max)(m_cached_oy + 4.0f,
                                m_cached_oy + m_height - 4.0f - tip_h);
                        }

                        // Draw drop shadow
                        canvas.fill_rect(tx + 1.0f, ty + 1.0f, tip_w, tip_h,
                            core::color<>(0.3f * tooltip_alpha, 0.0f, 0.0f, 0.0f));

                        // Draw background (premium dark semi-transparent glass)
                        canvas.fill_rect(tx, ty, tip_w, tip_h,
                            core::color<>(0.95f * tooltip_alpha, 0.08f, 0.08f, 0.08f));

                        // Draw border (thin themed border)
                        core::color<> border_col = m_theme.border;
                        border_col.alpha = 0.8f * tooltip_alpha;
                        auto border_pen = supplier.create_pen(border_col, 1.0f);
                        canvas.stroke_path(supplier.create_path()
                                               .move_to(tx, ty)
                                               .line_to(tx + tip_w, ty)
                                               .line_to(tx + tip_w, ty + tip_h)
                                               .line_to(tx, ty + tip_h)
                                               .close()
                                               .build(),
                            border_pen);

                        // Draw high-contrast text lines
                        auto text_brush = supplier.create_brush(
                            core::color<>(tooltip_alpha, 1.0f, 1.0f, 1.0f));
                        for (size_t i = 0; i < lines.size(); ++i) {
                            float ly_pos = ty + pad_y + i * line_h + f_size * 0.8f;
                            canvas.draw_text(lines[i], font, text_brush,
                                core::vec2(tx + pad_x, ly_pos),
                                kDRAWBOT_TextAlignment_Left);
                        }
                    }
                }
            }
        }

        ctx.set_handled();
    }

    void update_layout_from_event(const interaction_context& ctx) {
        if (!m_root)
            return;
        if (ctx.window() == interaction_context::window_type::effect
            && ctx.event_extra()) {
            const auto& frame = ctx.event_extra()->effect_win.current_frame;
            float cur_w = (float)(frame.right - frame.left);
            float cur_h = (float)(frame.bottom - frame.top);
            if (cur_w > 10)
                m_width = cur_w;
            if (cur_h > 10)
                m_height = cur_h;
            m_cached_ox = (float)frame.left;
            m_cached_oy = (float)frame.top;
        }
        float layout_w = m_width - 4.0f;
        float layout_h = m_height - 4.0f;
        if (m_root->layout.max_width > 0.0f) {
            layout_w = (std::min)(layout_w, m_root->layout.max_width);
        }
        if (m_root->layout.max_height > 0.0f) {
            layout_h = (std::min)(layout_h, m_root->layout.max_height);
        }
        m_root->do_layout(m_cached_ox + 2.0f, m_cached_oy + 2.0f, layout_w, layout_h);
    }

    /**
     * @brief Interaction click routing.
     *
     * @note <b>AE SDK Paradigm Shift:</b> High-performance internal event handlers
     * wrapping complex PICA transactions.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    void handle_click(const interaction_context& ctx) {
        if (!m_root)
            return;

        // ── Title-Area Click ────────────────────────────────────────
        if (ctx.event_extra()
            && ctx.event_extra()->effect_win.area == PF_EA_PARAM_TITLE) {
            auto screen_pt = ctx.screen_point();
            m_mouse_pos_screen = { (float)screen_pt.x, (float)screen_pt.y };
            uint32_t mods = ctx.event_extra()->u.do_click.modifiers;

            m_active_widget = nullptr;
            if (m_root
                && m_root->on_title_click((float)screen_pt.x, (float)screen_pt.y, mods)) {
                m_active_widget = m_root;
                m_title_active = true;
            }
            ctx.request_drag(); // Always request drag so we get release events
            ctx.set_handled();
            ctx.invalidate();
            return;
        }

        // ── Control-Area Click — existing path ──────────────────────
        update_layout_from_event(ctx);

        auto screen_pt = ctx.screen_point();
        m_mouse_pos_screen = { (float)screen_pt.x, (float)screen_pt.y };
        uint32_t mods = ctx.event_extra()->u.do_click.modifiers;

        // Find the deepest widget at this point
        m_active_widget = nullptr;

        auto* hit = m_root->find_widget_at((float)screen_pt.x, (float)screen_pt.y);

        widget* old_focused = m_focused_widget;
        if (hit && hit->enabled) {
            m_focused_widget = hit;
            if (hit->on_click((float)screen_pt.x, (float)screen_pt.y, mods)) {
                m_active_widget = hit;
            }
        } else {
            m_focused_widget = nullptr;
        }

        if (m_focused_widget != old_focused) {
            if (old_focused)
                old_focused->on_focus_lost();
            if (m_focused_widget)
                m_focused_widget->on_focus_gained();
            ctx.invalidate();
        }

        ctx.request_drag(); // Always request drag so we get release events
        ctx.set_handled();
        ctx.invalidate();
    }

    /**
     * @brief Interaction drag routing.
     *
     * @note <b>AE SDK Paradigm Shift:</b> High-performance internal event handlers
     * wrapping complex PICA transactions.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    void handle_drag(const interaction_context& ctx) {
        // ── Title-Area Drag ─────────────────────────────────────────
        if (ctx.event_extra()
            && ctx.event_extra()->effect_win.area == PF_EA_PARAM_TITLE) {
            auto screen_pt = ctx.screen_point();
            m_mouse_pos_screen = { (float)screen_pt.x, (float)screen_pt.y };
            uint32_t mods = ctx.event_extra()->u.do_click.modifiers;

            if (m_active_widget && m_title_active) {
                m_active_widget->on_title_drag(
                    (float)screen_pt.x, (float)screen_pt.y, mods);
            }

            // Check for release (last_time)
            if (ctx.event_extra()->u.do_click.last_time) {
                if (m_active_widget && m_title_active) {
                    m_active_widget->on_title_release();
                    m_active_widget = nullptr;
                    m_title_active = false;
                }
            }

            ctx.set_handled();
            ctx.invalidate();
            return;
        }

        // ── Control-Area Drag — existing path ───────────────────────
        update_layout_from_event(ctx);

        auto screen_pt = ctx.screen_point();
        m_mouse_pos_screen = { (float)screen_pt.x, (float)screen_pt.y };
        uint32_t mods = ctx.event_extra()->u.do_click.modifiers;

        if (m_active_widget && m_active_widget->enabled) {
            m_active_widget->on_drag((float)screen_pt.x, (float)screen_pt.y, mods);
        }

        // Check for release (last_time)
        if (ctx.event_extra()->u.do_click.last_time) {
            if (m_active_widget) {
                m_active_widget->on_release();
                m_active_widget = nullptr;
            }
        }

        ctx.set_handled();
        ctx.invalidate();
    }

    /**
     * @brief Keyboard event routing.
     *
     * @note <b>AE SDK Paradigm Shift:</b> High-performance internal event handlers
     * wrapping complex PICA transactions.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    void handle_keydown(const interaction_context& ctx) {
        if (m_focused_widget && m_focused_widget->enabled) {
            PF_KeyCode key = ctx.event_extra()->u.key_down.keycode;
            bool is_ctrl = !PF_KEYCODE_IS_PRINTABLE(key);
            PF_ControlCode ctrl = is_ctrl
                ? static_cast<PF_ControlCode>(PF_KEYCODE_GET_CONTROL_CODE(key))
                : PF_ControlCode_Unknown;

            if (m_focused_widget->on_key(ctx)) {
                ctx.set_handled();
                ctx.set_refresh_ui();
            }

            if (is_ctrl
                && (ctrl == PF_ControlCode_Return || ctrl == PF_ControlCode_Enter
                    || ctrl == PF_ControlCode_Escape)) {
                m_focused_widget->on_focus_lost();
                m_focused_widget = nullptr;
                ctx.set_refresh_ui();
            }
        }
    }

    void handle_mouse_exited(const interaction_context& ctx) {
        if (m_hovered_widget) {
            m_hovered_widget->on_hover_exit();
            m_hovered_widget = nullptr;
        }
        ctx.set_handled();
        ctx.invalidate();
    }

    /**
     * @brief Adjust cursor style routing on hover bounds.
     *
     * @note <b>AE SDK Paradigm Shift:</b> High-performance internal event handlers
     * wrapping complex PICA transactions.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    void handle_adjust_cursor(const interaction_context& ctx) {
        if (!m_root)
            return;

        // ── Title-Area Adjust Cursor ────────────────────────────────
        if (ctx.event_extra()
            && ctx.event_extra()->effect_win.area == PF_EA_PARAM_TITLE) {
            auto screen_pt = ctx.screen_point();
            m_mouse_pos_screen = { (float)screen_pt.x, (float)screen_pt.y };

            bool is_hit = m_root
                && m_root->hit_test_title((float)screen_pt.x, (float)screen_pt.y);
            if (m_root) {
                if (is_hit) {
                    if (m_hovered_widget != m_root) {
                        if (m_hovered_widget)
                            m_hovered_widget->on_hover_exit();
                        m_root->on_hover_enter();
                        m_hovered_widget = m_root;
                        ctx.invalidate();
                    }
                    if (m_root->on_title_hover_move(
                            (float)screen_pt.x, (float)screen_pt.y)) {
                        ctx.invalidate();
                    }
                    int32_t cursor = PF_Cursor_ARROW;
                    int32_t widget_cursor = m_root->cursor_type();
                    if (widget_cursor > 0) {
                        cursor = widget_cursor;
                    }
                    ctx.event_extra()->u.adjust_cursor.set_cursor
                        = static_cast<PF_CursorType>(cursor);
                    ctx.set_handled();
                } else {
                    if (m_hovered_widget == m_root) {
                        m_root->on_hover_exit();
                        m_hovered_widget = nullptr;
                        ctx.invalidate();
                    }
                }
            }
            return;
        }

        update_layout_from_event(ctx);

        auto screen_pt = ctx.screen_point();
        m_mouse_pos_screen = { (float)screen_pt.x, (float)screen_pt.y };

        widget* new_hover
            = m_root->find_widget_at((float)screen_pt.x, (float)screen_pt.y);

        // Handle hover enter/exit transitions
        if (new_hover != m_hovered_widget) {
            if (m_hovered_widget)
                m_hovered_widget->on_hover_exit();
            if (new_hover)
                new_hover->on_hover_enter();
            m_hovered_widget = new_hover;
            m_hover_start_time = std::chrono::steady_clock::now();
            ctx.invalidate(); // Redraw for hover visual feedback
        }

        // Notify the hovered widget of local coordinate movement
        if (m_hovered_widget) {
            if (m_hovered_widget->on_hover_move((float)screen_pt.x, (float)screen_pt.y)) {
                ctx.invalidate(); // Redraw for internal widget hover visual updates
            }
        }

        // Set cursor based on hovered widget, default to PF_Cursor_ARROW
        int32_t cursor = PF_Cursor_ARROW;
        if (m_hovered_widget) {
            int32_t widget_cursor = m_hovered_widget->cursor_type();
            if (widget_cursor > 0) {
                cursor = widget_cursor;
            }
        }
        ctx.event_extra()->u.adjust_cursor.set_cursor
            = static_cast<PF_CursorType>(cursor);
        ctx.set_handled();
    }

    std::vector<std::string> wrap_text(const std::string& text, size_t max_chars) const {
        std::vector<std::string> lines;
        if (text.empty())
            return lines;
        size_t limit = (max_chars < 5) ? 5 : max_chars;

        size_t start = 0;
        while (start < text.length()) {
            if (text.length() - start <= limit) {
                lines.push_back(text.substr(start));
                break;
            }

            size_t end = start + limit;
            size_t space = text.find_last_of(" \t\r\n", end);
            if (space != std::string::npos && space > start) {
                lines.push_back(text.substr(start, space - start));
                start = space + 1;
            } else {
                lines.push_back(text.substr(start, limit));
                start += limit;
            }
        }
        return lines;
    }

    std::string truncate_text(const std::string& text, size_t max_chars) const {
        if (text.length() <= max_chars)
            return text;
        if (max_chars <= 3)
            return text.substr(0, max_chars);
        return text.substr(0, max_chars - 3) + "...";
    }
};

} // namespace aetk::effect::ui
