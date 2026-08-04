#pragma once

#include <aetk/effect/ui/widget.hpp>
#include <aetk/ui/theme.hpp>
#include <aetk/effect/ui/widgets/button_data.hpp>
#include <functional>

namespace aetk::effect::ui {

// ══════════════════════════════════════════════════════════════════════
//  Button — Clickable rectangle with text label
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief A clickable button with hover and press visual states.
 * 
 * @details Invokes a callback when clicked. Renders with rounded rect background
 * and centered text. Visual states:
 *  - Normal:  bg_light fill
 *  - Hovered: accent fill (lighter)
 *  - Pressed: accent_pressed fill
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, custom Drawbot button controls must be hardcoded and manually rendered frame-by-frame, writing custom Bezier drawing loops, tracking mouse states (`PF_Cursor_FINGER_POINTER`), and managing click triggers procedurally. `aetk::effect::ui::button` shifts this paradigm to a modern, declarative widget structure. It provides automatic 3D bevel highlighting, inset shadows, custom image icon support, and fluid, customizable corner-rounded rectangular path generators. Furthermore, it supports an optional persistent toggle mode which automatically synchronizes the active button state with a sequence parameter arbitrary block (`button_data`), removing manual parameter checkouts during render loops.
 *
 * @warning <b>Memory & Lifecycles:</b> The button is an inspector-oriented widget that binds dynamic pointer callbacks (`on_press`). The referenced callback object must remain in memory while the button executes. Toggle status modifications automatically trigger sequence updates on the host arbitrary parameter mapping. Active states synchronize with arbitrary sequence parameters via `sync_state_impl` and `commit_state_impl` calls.
 */
class button : public widget {
public:
    using data_type = button_data;

    /**
     * @brief Customizable builder options for button aesthetics and behavior.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Fluent builder properties replacing raw procedural drawing flags.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    struct options {
        // Colors (optional, fallbacks to theme if default/empty)
        core::color<> bg_color;         // 0 alpha = use theme
        core::color<> text_color;
        core::color<> hover_color;
        core::color<> pressed_color;
        core::color<> active_color;

        // Layout
        float corner_radius;
        float min_width;
        float padding;

        // Behavior
        bool toggle_mode;
        bool shadow;

        // Image Icon
        DRAWBOT_ImageRef image;

        /**
         * @brief Standard constructor initializing options defaults.
         *
         * @note <b>AE SDK Paradigm Shift:</b> Replaces direct struct value assignments.
         *
         * @warning <b>Memory & Lifecycles:</b> None.
         */
        options()
            : bg_color(0.0f, 0.0f, 0.0f, 0.0f),
              text_color(0.0f, 0.0f, 0.0f, 0.0f),
              hover_color(0.0f, 0.0f, 0.0f, 0.0f),
              pressed_color(0.0f, 0.0f, 0.0f, 0.0f),
              active_color(0.0f, 0.0f, 0.0f, 0.0f),
              corner_radius(2.0f),
              min_width(40.0f),
              padding(8.0f),
              toggle_mode(false),
              shadow(true),
              image(nullptr) {}

        // Builder methods
        options& set_shadow(bool s)               { shadow = s; return *this; }
        options& set_bg_color(core::color<> c)      { bg_color = c; return *this; }
        options& set_text_color(core::color<> c)    { text_color = c; return *this; }
        options& set_hover_color(core::color<> c)   { hover_color = c; return *this; }
        options& set_pressed_color(core::color<> c) { pressed_color = c; return *this; }
        options& set_active_color(core::color<> c)  { active_color = c; return *this; }
        options& set_corner_radius(float r)       { corner_radius = r; return *this; }
        options& set_min_width(float w)           { min_width = w; return *this; }
        options& set_toggle_mode(bool t)          { toggle_mode = t; return *this; }
        options& set_image(DRAWBOT_ImageRef i)    { image = i; return *this; }
    };

    /**
     * @brief Fluent state defaults for setup.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Fluent state defaults.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Button default data.
     */
    static data_type get_default_data(const std::string& /*label*/, const std::function<void(bool)>& /*callback*/ = nullptr, const options& /*opt*/ = options()) {
        return {};
    }

    /// Button text label.
    std::string text;
    
    /// Bounded callback trigger on pressed.
    std::function<void(bool)> on_press;
    
    /// Esthetic styling options.
    options opts;
    
    /// Toggle active status flag.
    bool active = false; // Used for toggle_mode
    
    /// Bounded parameter index identifier.
    int32_t m_param_index = 0;

    /**
     * @brief Constructs a new clickable button.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Modern NVI-patterned button UI widget with unified PICA/Drawbot coordinate mappings.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param param_idx Bounded parameter index.
     * @param label Button label.
     * @param callback Click callback.
     * @param opt Styling options.
     */
    explicit button(int32_t param_idx, std::string label, std::function<void(bool)> callback = nullptr, const options& opt = options())
        : m_param_index(param_idx), text(std::move(label)), on_press(std::move(callback)), opts(opt) {
        layout.min_height = 18.0f;
        layout.min_width = opts.min_width;
        // Limit max width by default to keep standard buttons compact and AE-native
        float est_w = (std::max)(opts.min_width, (float)text.size() * 7.0f + opts.padding * 2.0f);
        layout.max_width = est_w + 16.0f;
    }

protected:
    /**
     * @brief Measures button dimension limits.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Estimated width bounds.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param avail_w Available width.
     * @return Bounded vector size.
     */
    virtual core::vec2 measure_impl(float avail_w, float) override {
        // Estimate width from text length, including balanced padding on both sides
        float est_w = (std::max)(opts.min_width, (float)text.size() * 7.0f + opts.padding * 2.0f);
        return { (std::min)(est_w, avail_w), layout.min_height };
    }

    /**
     * @brief Paints the button bevels, borders, shadows, and text.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Custom 3D rounded path drawing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param canvas Drawbot canvas.
     * @param supplier Drawbot supplier.
     * @param t Theme color parameters.
     */
    virtual void paint_impl(drawbot::canvas& canvas, drawbot::supplier& supplier, const theme& t) override {
        // Choose fill color based on state and options
        core::color<> fill;
        if (opts.toggle_mode && active) {
            fill = opts.active_color.alpha > 0.0f ? opts.active_color : t.accent_pressed;
        } else if (pressed) {
            fill = opts.pressed_color.alpha > 0.0f ? opts.pressed_color : t.accent_pressed;
        } else if (hovered) {
            fill = opts.hover_color.alpha > 0.0f ? opts.hover_color : t.accent_hover;
        } else {
            fill = opts.bg_color.alpha > 0.0f ? opts.bg_color : t.bg_light;
        }

        float x = bounds.x;
        float y = bounds.y;
        float w = bounds.w;
        float h = bounds.h;
        float r = opts.corner_radius;

        // Path generator for clean rounded rectangles
        auto make_round_rect_path = [&](float px, float py, float pw, float ph, float pr) {
            pr = (std::min)({pr, pw * 0.5f, ph * 0.5f});
            if (pr <= 0.0f) {
                return supplier.create_path().add_rect(px, py, pw, ph).build();
            }
            float k = pr * 0.55228475f;
            return supplier.create_path()
                .move_to(px + pr, py)
                .line_to(px + pw - pr, py)
                .bezier_to({px + pw - pr + k, py}, {px + pw, py + pr - k}, {px + pw, py + pr})
                .line_to(px + pw, py + ph - pr)
                .bezier_to({px + pw, py + ph - pr + k}, {px + pw - pr + k, py + ph}, {px + pw - pr, py + ph})
                .line_to(px + pr, py + ph)
                .bezier_to({px + pr - k, py + ph}, {px, py + ph - pr + k}, {px, py + ph - pr})
                .line_to(px, py + pr)
                .bezier_to({px, py + pr - k}, {px + pr - k, py}, {px + pr, py})
                .build();
        };

        // Draw shadow shifted by 1px if requested
        if (opts.shadow && !pressed) {
            auto shadow_path = make_round_rect_path(x + 0.5f, y + 1.0f, w, h, r);
            canvas.fill_path(shadow_path, supplier.create_brush(core::color<>(0.0f, 0.0f, 0.0f, 0.25f)));
        }

        // Draw button background
        auto bg_path = make_round_rect_path(x, y, w, h, r);
        canvas.fill_path(bg_path, supplier.create_brush(fill));

        // 3D Bevel Highlights and Inset Shadows
        if (w > 2.0f * r && h > 2.0f) {
            bool inset = pressed || (opts.toggle_mode && active);
            core::color<> top_color = inset ? core::color<>(0.0f, 0.0f, 0.0f, 0.25f) : core::color<>(1.0f, 1.0f, 1.0f, 0.15f);
            core::color<> bottom_color = inset ? core::color<>(1.0f, 1.0f, 1.0f, 0.1f) : core::color<>(0.0f, 0.0f, 0.0f, 0.2f);

            // Top highlight/shadow bevel line
            auto top_pen = supplier.create_pen(top_color, 1.0f);
            auto top_path = supplier.create_path()
                .move_to(x + r, y + 1.0f)
                .line_to(x + w - r, y + 1.0f)
                .build();
            canvas.stroke_path(top_path, top_pen);

            // Bottom highlight/shadow bevel line
            auto bottom_pen = supplier.create_pen(bottom_color, 1.0f);
            auto bottom_path = supplier.create_path()
                .move_to(x + r, y + h - 1.0f)
                .line_to(x + w - r, y + h - 1.0f)
                .build();
            canvas.stroke_path(bottom_path, bottom_pen);
        }

        // Draw crisp border outline
        auto border_pen = supplier.create_pen(t.border, 1.0f);
        canvas.stroke_path(bg_path, border_pen);

        // Draw icon image or text centering
        if (opts.image) {
            float img_x = x + w * 0.5f - 12.0f;
            float img_y = y + h * 0.5f - 12.0f;
            canvas.draw_image(opts.image, img_x, img_y);
        } else if (!text.empty()) {
            auto font = supplier.create_font(t.font_size);
            core::color<> text_c = opts.text_color.alpha > 0.0f ? opts.text_color : core::color<>(1.0f, 1.0f, 1.0f, 1.0f);
            auto brush = supplier.create_brush(text_c);

            float cx = x + w * 0.5f;
            float cy = y + h * 0.5f + t.font_size * 0.3f; // baseline offset

            canvas.draw_text(text, font, brush, core::vec2(cx, cy), kDRAWBOT_TextAlignment_Center);
        }
    }

    /**
     * @brief Click handler registering pressed state status.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Callback triggers and optional toggle active changes.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @param mods Modifiers.
     * @return True if handled.
     */
    virtual bool on_click_impl(float lx, float ly, uint32_t mods) override {
        if (!enabled) return false;
        pressed = true;
        if (opts.toggle_mode) {
            active = !active;
        }
        if (on_press) on_press(active);

        // Notify custom virtual subclass hook
        on_button_pressed(active);
        return true;
    }

    /**
     * @brief Syncs state parameters.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automated sync from persistent arbitrary button data.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    virtual void sync_state_impl(const interaction_context& ctx) override {
        if (opts.toggle_mode && m_param_index > 0) {
            auto lock = ctx.arb_data<button_data>(m_param_index);
            if (lock) active = lock->active;
        }
    }

public:
    /** @brief Sets button active state manually. */
    void set_active(bool state) {
        active = state;
    }

    virtual bool is_title_only() const override {
        return true;
    }

protected:
    /**
     * @brief User-overridable hook for subclass safety (NVI Expansion).
     *
     * @note <b>AE SDK Paradigm Shift:</b> Pressed event callback.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param active_state New active state status.
     */
    virtual void on_button_pressed(bool active_state) {}

    /**
     * @brief Commits state changes.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Auto-commit toggle changes back to After Effects parameters.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    virtual void commit_state_impl(const interaction_context& ctx) override {
        if (opts.toggle_mode && m_param_index > 0) {
            auto lock = ctx.arb_data<button_data>(m_param_index);
            if (lock && lock->active != active) {
                lock->active = active;
                ctx.mark_param_changed(m_param_index);
            }
        }
    }

    /**
     * @brief Release handler.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Dynamic release reset.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    virtual void on_release_impl() override {
        pressed = false;
    }

    /**
     * @brief Inspects custom cursor type on hover bounds.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Set cursor style to finger pointer.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Cursor type value.
     */
    virtual int32_t cursor_type_impl() const override {
        return PF_Cursor_FINGER_POINTER;
    }

    virtual void paint_title_impl(drawbot::canvas& canvas, drawbot::supplier& supplier,
        const theme& t, float x, float y, float w, float h) override {
        float btn_w = (std::max)(opts.min_width, (float)text.size() * 7.0f + opts.padding * 2.0f);
        float btn_h = h - 4.0f; // 2px margin top/bottom
        float btn_x = x + 4.0f;
        float btn_y = y + 2.0f;

        m_title_btn_bounds = { btn_x, btn_y, btn_w, btn_h };

        // Choose fill color based on state
        core::color<> fill;
        if (opts.toggle_mode && active) {
            fill = opts.active_color.alpha > 0.0f ? opts.active_color : t.accent_pressed;
        } else if (m_title_pressed) {
            fill = opts.pressed_color.alpha > 0.0f ? opts.pressed_color : t.accent_pressed;
        } else if (hovered) {
            fill = opts.hover_color.alpha > 0.0f ? opts.hover_color : t.accent_hover;
        } else {
            fill = opts.bg_color.alpha > 0.0f ? opts.bg_color : t.bg_light;
        }

        auto make_round_rect_path = [&](float px, float py, float pw, float ph, float pr) {
            pr = (std::min)({pr, pw * 0.5f, ph * 0.5f});
            if (pr <= 0.0f) {
                return supplier.create_path().add_rect(px, py, pw, ph).build();
            }
            float k = pr * 0.55228475f;
            return supplier.create_path()
                .move_to(px + pr, py)
                .line_to(px + pw - pr, py)
                .bezier_to({px + pw - pr + k, py}, {px + pw, py + pr - k}, {px + pw, py + pr})
                .line_to(px + pw, py + ph - pr)
                .bezier_to({px + pw, py + ph - pr + k}, {px + pw - pr + k, py + ph}, {px + pw - pr, py + ph})
                .line_to(px + pr, py + ph)
                .bezier_to({px + pr - k, py + ph}, {px, py + ph - pr + k}, {px, py + ph - pr})
                .line_to(px, py + pr)
                .bezier_to({px, py + pr - k}, {px + pr - k, py}, {px + pr, py})
                .build();
        };

        float r = opts.corner_radius;
        if (opts.shadow && !m_title_pressed) {
            auto shadow_path = make_round_rect_path(btn_x + 0.5f, btn_y + 1.0f, btn_w, btn_h, r);
            canvas.fill_path(shadow_path, supplier.create_brush(core::color<>(0.0f, 0.0f, 0.0f, 0.25f)));
        }

        auto bg_path = make_round_rect_path(btn_x, btn_y, btn_w, btn_h, r);
        canvas.fill_path(bg_path, supplier.create_brush(fill));

        if (btn_w > 2.0f * r && btn_h > 2.0f) {
            bool inset = m_title_pressed || (opts.toggle_mode && active);
            core::color<> top_color = inset ? core::color<>(0.0f, 0.0f, 0.0f, 0.25f) : core::color<>(1.0f, 1.0f, 1.0f, 0.15f);
            core::color<> bottom_color = inset ? core::color<>(1.0f, 1.0f, 1.0f, 0.1f) : core::color<>(0.0f, 0.0f, 0.0f, 0.2f);

            auto top_pen = supplier.create_pen(top_color, 1.0f);
            auto top_path = supplier.create_path().move_to(btn_x + r, btn_y + 1.0f).line_to(btn_x + btn_w - r, btn_y + 1.0f).build();
            canvas.stroke_path(top_path, top_pen);

            auto bottom_pen = supplier.create_pen(bottom_color, 1.0f);
            auto bottom_path = supplier.create_path().move_to(btn_x + r, btn_y + btn_h - 1.0f).line_to(btn_x + btn_w - r, btn_y + btn_h - 1.0f).build();
            canvas.stroke_path(bottom_path, bottom_pen);
        }

        auto border_pen = supplier.create_pen(t.border, 1.0f);
        canvas.stroke_path(bg_path, border_pen);

        if (opts.image) {
            float img_x = btn_x + btn_w * 0.5f - 12.0f;
            float img_y = btn_y + btn_h * 0.5f - 12.0f;
            canvas.draw_image(opts.image, img_x, img_y);
        } else if (!text.empty()) {
            auto font = supplier.create_font(t.font_size * 0.9f);
            core::color<> text_c = opts.text_color.alpha > 0.0f ? opts.text_color : core::color<>(1.0f, 1.0f, 1.0f, 1.0f);
            auto brush = supplier.create_brush(text_c);

            float cx = btn_x + btn_w * 0.5f;
            float cy = btn_y + btn_h * 0.5f + t.font_size * 0.28f;
            canvas.draw_text(text, font, brush, core::vec2(cx, cy), kDRAWBOT_TextAlignment_Center);
        }
    }

    virtual bool on_title_click_impl(float lx, float ly, uint32_t mods) override {
        if (!enabled) return false;
        if (lx >= m_title_btn_bounds.x && lx <= m_title_btn_bounds.x + m_title_btn_bounds.w &&
            ly >= m_title_btn_bounds.y && ly <= m_title_btn_bounds.y + m_title_btn_bounds.h) {
            m_title_pressed = true;
            if (opts.toggle_mode) {
                active = !active;
            }
            if (on_press) on_press(active);
            on_button_pressed(active);
            return true;
        }
        return false;
    }

    virtual void on_title_release_impl() override {
        m_title_pressed = false;
    }

    virtual bool hit_test_title(float lx, float ly) const override {
        return (lx >= m_title_btn_bounds.x && lx <= m_title_btn_bounds.x + m_title_btn_bounds.w &&
                ly >= m_title_btn_bounds.y && ly <= m_title_btn_bounds.y + m_title_btn_bounds.h);
    }

private:
    bounds_rect m_title_btn_bounds = {0.0f, 0.0f, 0.0f, 0.0f};
    bool m_title_pressed = false;
};

} // namespace aetk::effect::ui
