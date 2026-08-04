#pragma once

#include <aetk/effect/ui/widget.hpp>
#include <aetk/ui/theme.hpp>
#include <aetk/effect/ui/widgets/joystick_data.hpp>
#include <aetk/effect/ui/widgets/joystick_shape.hpp>
#include <aetk/effect/ui/widgets/text_input.hpp>
#include <aetk/effect/context/context.hpp>
#include <algorithm>
#include <cmath>
#include <memory>

namespace aetk::effect::ui {

/**
 * @brief A 2D draggable joystick pad with simulated physical resistance and spring-back centring.
 * 
 * @details Controls two values (X and Y), mapped from -1.0 to 1.0.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, capturing simultaneous 2D coordinates via a thumbstick well requires coding deep mouse capture loops, implementing bound clamping manually, rendering the control in PICA Drawbot, and embedding individual sub-widgets to edit the values numerically. `aetk::effect::ui::joystick_pad` encapsulates this into a beautiful, drop-in dual-axis pad control. It supports both circular and square travel shapes, user-adjustable damping resistance, and integrated `text_input` fields to edit X/Y fields directly. On release, it can snap back to the center visually while preserving sequence value state data persistent locks.
 *
 * @warning <b>Memory & Lifecycles:</b> The pad manages the unique pointer lifetimes of two persistent `text_input` fields (`val_input_x`, `val_input_y`). Values sync automatically via `sync_state_impl` and `commit_state_impl` passes to and from After Effects arbitrary parameter sequence states. Destroys sub-text controls cleanly. Integrates cursor changes to `PF_Cursor_FINGER_POINTER_SCRUB` automatically.
 */
class joystick_pad : public widget {
public:
    using data_type = joystick_data;

    /**
     * @brief Custom aesthetic parameters and damping factors.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Fluent builder properties replacing raw procedural well rendering flags.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    struct options {
        /// Damping factor (0.0f = no resistance, 1.0f = fully locked).
        float resistance = 0.3f;
        
        /// Spring visual knob back to center on release.
        bool snap_to_center = true;

        /// Travel boundary for the joystick well.
        joystick_shape shape = joystick_shape::circle;

        /// Response shaping exponent for finer control near the center.
        float response_exponent = 1.0f;

        /**
         * @brief options constructor.
         *
         * @note <b>AE SDK Paradigm Shift:</b> Sets default values for resistance and snapping.
         *
         * @warning <b>Memory & Lifecycles:</b> None.
         */
        options() : resistance(0.3f), snap_to_center(true), shape(joystick_shape::circle), response_exponent(1.0f) {}

        options& set_resistance(float r) { resistance = r; return *this; }
        options& set_snap_to_center(bool s) { snap_to_center = s; return *this; }
        options& set_shape(joystick_shape s) { shape = s; return *this; }
        options& set_response_exponent(float exponent) { response_exponent = exponent; return *this; }
    };

    /**
     * @brief Fluent state defaults for setup.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Fluent state defaults.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Joystick default data.
     */
    static data_type get_default_data(const std::string& /*label*/ = "", const options& /*opt*/ = options()) {
        return { 0.0f, 0.0f };
    }

    /// Pad text label.
    std::string text;
    
    /// Joystick active data coordinate.
    joystick_data data;
    
    /// Bounded parameter index identifier.
    int32_t m_param_index = 0;
    
    /// Pad resistance styling options.
    options opts;

    float visual_x = 0.0f;
    float visual_y = 0.0f;

    /**
     * @brief Constructs a new joystick pad.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Unified NVI-patterned two-dimensional widget with child text coordinate inputs.
     *
     * @warning <b>Memory & Lifecycles:</b> Spawns and takes unique ownership of two text input widgets.
     *
     * @param param_idx Bounded parameter index.
     * @param label Display label.
     * @param opt Custom styling options.
     */
    joystick_pad(int32_t param_idx, std::string label, const options& opt = options())
        : m_param_index(param_idx), text(std::move(label)), opts(opt) {
        layout.min_width = 100.0f;
        layout.min_height = 100.0f;

        val_input_x = std::make_unique<text_input>("0.00", [this](const std::string& str) {
            float parsed = 0.0f;
            if (aetk::core::c_sscanf(str.c_str(), "%f", &parsed) == 1) {
                data.x = std::clamp(parsed, -1.0f, 1.0f);
                visual_x = data.x;
                on_joystick_moved(data.x, data.y);
            }
            update_input_texts();
        });

        val_input_y = std::make_unique<text_input>("0.00", [this](const std::string& str) {
            float parsed = 0.0f;
            if (aetk::core::c_sscanf(str.c_str(), "%f", &parsed) == 1) {
                data.y = std::clamp(parsed, -1.0f, 1.0f);
                visual_y = data.y;
                on_joystick_moved(data.x, data.y);
            }
            update_input_texts();
        });
    }

protected:
    /**
     * @brief Measures layout dimensions ensuring square ratios.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounded square ratios.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param avail_w Width bounds.
     * @param avail_h Height bounds.
     * @return Size vector.
     */
    virtual core::vec2 measure_impl(float avail_w, float avail_h) override {
        return { avail_w, avail_w }; // Maintain square aspect ratio
    }

    /**
     * @brief Paints the joystick well, crosshairs, spherical 3D knob, highlights, and text labels.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Custom 3D circle and crosshair Drawbot rendering.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param canvas Drawbot canvas.
     * @param supplier Drawbot supplier.
     * @param t Theme color definitions.
     */
    virtual void paint_impl(drawbot::canvas& canvas, drawbot::supplier& supplier, const theme& t) override {
        // Draw flat background behind the well
        canvas.fill_rect(bounds.x, bounds.y, bounds.w, bounds.h, t.bg);

        float cx = bounds.x + bounds.w * 0.5f;
        float cy = bounds.y + bounds.h * 0.58f;
        float radius = (std::min)(bounds.w, bounds.h) * 0.38f;

        if (radius <= 0.0f) return;

        // Path generator for clean circles using 4 quadrant Beziers
        auto make_circle_path = [&](float ccx, float ccy, float cr) {
            float k = cr * 0.55228475f;
            return supplier.create_path()
                .move_to(ccx, ccy - cr) // top center
                .bezier_to(core::vec2(ccx + k, ccy - cr), core::vec2(ccx + cr, ccy - k), core::vec2(ccx + cr, ccy)) // top-right
                .bezier_to(core::vec2(ccx + cr, ccy + k), core::vec2(ccx + k, ccy + cr), core::vec2(ccx, ccy + cr)) // bottom-right
                .bezier_to(core::vec2(ccx - k, ccy + cr), core::vec2(ccx - cr, ccy + k), core::vec2(ccx - cr, ccy)) // bottom-left
                .bezier_to(core::vec2(ccx - cr, ccy - k), core::vec2(ccx - k, ccy - cr), core::vec2(ccx, ccy - cr)) // top-left
                .build();
        };

        auto make_square_path = [&](float ccx, float ccy, float half_extent) {
            float left = ccx - half_extent;
            float right = ccx + half_extent;
            float top = ccy - half_extent;
            float bottom = ccy + half_extent;
            float corner = half_extent * 0.28f;

            return supplier.create_path()
                .move_to(left + corner, top)
                .line_to(right - corner, top)
                .bezier_to(core::vec2(right - corner * 0.45f, top), core::vec2(right, top + corner * 0.45f), core::vec2(right, top + corner))
                .line_to(right, bottom - corner)
                .bezier_to(core::vec2(right, bottom - corner * 0.45f), core::vec2(right - corner * 0.45f, bottom), core::vec2(right - corner, bottom))
                .line_to(left + corner, bottom)
                .bezier_to(core::vec2(left + corner * 0.45f, bottom), core::vec2(left, bottom - corner * 0.45f), core::vec2(left, bottom - corner))
                .line_to(left, top + corner)
                .bezier_to(core::vec2(left, top + corner * 0.45f), core::vec2(left + corner * 0.45f, top), core::vec2(left + corner, top))
                .close()
                .build();
        };

        // 1. Draw base well
        auto well_path = (opts.shape == joystick_shape::square)
            ? make_square_path(cx, cy, radius)
            : make_circle_path(cx, cy, radius);
        core::color<> well_bg = core::color<>(t.bg.red * 0.75f, t.bg.green * 0.75f, t.bg.blue * 0.75f);
        canvas.fill_path(well_path, supplier.create_brush(well_bg));
        canvas.stroke_path(well_path, supplier.create_pen(t.border, 1.2f));

        // 2. Draw Faded Crosshairs inside the well
        auto axis_pen = supplier.create_pen(core::color<>(0.4, t.border.red, t.border.green, t.border.blue), 1.0f);
        auto h_path = supplier.create_path().move_to(cx - radius, cy).line_to(cx + radius, cy).build();
        canvas.stroke_path(h_path, axis_pen);

        auto v_path = supplier.create_path().move_to(cx, cy - radius).line_to(cx, cy + radius).build();
        canvas.stroke_path(v_path, axis_pen);

        // 3. Draw Spherical 3D Thumbstick Knob (Radius = 12px)
        float knob_r = 12.0f;
        float px = cx + visual_x * radius;
        float py = cy + visual_y * radius;

        // Knob Shadow (shifted slightly down-right)
        auto knob_shadow_path = make_circle_path(px + 1.0f, py + 1.5f, knob_r);
        canvas.fill_path(knob_shadow_path, supplier.create_brush(core::color<>(0.35, 0.0, 0.0, 0.0)));

        // Knob Main Body
        auto knob_path = make_circle_path(px, py, knob_r);
        core::color<> knob_color = (pressed || hovered) ? t.handle_active : t.accent;
        canvas.fill_path(knob_path, supplier.create_brush(knob_color));
        canvas.stroke_path(knob_path, supplier.create_pen(t.border, 1.0f));

        // Knob Spherical 3D Bevel Overlay
        auto highlight_pen = supplier.create_pen(core::color<>(0.25, 1.0, 1.0, 1.0), 1.0f);
        auto highlight_path = supplier.create_path()
            .move_to(px - knob_r * 0.5f, py - knob_r * 0.2f)
            .bezier_to(core::vec2(px - knob_r * 0.4f, py - knob_r * 0.6f), core::vec2(px - knob_r * 0.1f, py - knob_r * 0.7f), core::vec2(px, py - knob_r * 0.7f))
            .build();
        canvas.stroke_path(highlight_path, highlight_pen);

        // Central Grip Concentric Circle
        auto grip_path = make_circle_path(px, py, 4.0f);
        auto grip_pen = supplier.create_pen(core::color<>(0.3, 0.0, 0.0, 0.0), 1.0f);
        canvas.stroke_path(grip_path, grip_pen);

        // 4. Draw Text Label with current values
        if (supplier.supports_text() && !text.empty()) {
            auto f = supplier.create_font(t.font_size);
            float well_x = cx - radius;
            float well_y = cy - radius;
            
            // Draw Name
            auto name_brush = supplier.create_brush(t.text);
            canvas.draw_text(text, f, name_brush, core::vec2(well_x, well_y - 20.0f), kDRAWBOT_TextAlignment_Left);
            
            // Draw static X: and Y: labels
            auto blue_brush = supplier.create_brush(t.accent);
            canvas.draw_text("X:", f, blue_brush, core::vec2(well_x, well_y - 8.0f), kDRAWBOT_TextAlignment_Left);
            canvas.draw_text("Y:", f, blue_brush, core::vec2(well_x + 52.0f, well_y - 8.0f), kDRAWBOT_TextAlignment_Left);

            // Paint the persistent text inputs
            if (val_input_x && val_input_y) {
                update_input_texts();
                val_input_x->paint(canvas, supplier, t);
                val_input_y->paint(canvas, supplier, t);
            }
        }
    }

    /**
     * @brief Performs internal layout positioning of the text input components.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Absolute layouts inside custom wells.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param x Horizontal pixel position.
     * @param y Vertical pixel position.
     * @param w Layout width limit.
     * @param h Layout height limit.
     */
    virtual void do_layout_impl(float x, float y, float w, float h) override {
        float cx = bounds.x + bounds.w * 0.5f;
        float cy = bounds.y + bounds.h * 0.58f;
        float radius = (std::min)(bounds.w, bounds.h) * 0.38f;
        float well_x = cx - radius;
        float well_y = cy - radius;

        if (val_input_x) {
            val_input_x->do_layout(well_x + 14.0f, well_y - 12.0f, 36.0f, 15.0f);
        }
        if (val_input_y) {
            val_input_y->do_layout(well_x + 68.0f, well_y - 12.0f, 36.0f, 15.0f);
        }
    }

    /**
     * @brief Traverses active widgets down to the input fields first.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Z-order routing to text inputs.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @return Target widget.
     */
    virtual widget* find_widget_at_impl(float lx, float ly) override {
        if (val_input_x) {
            if (auto* hit = val_input_x->find_widget_at(lx, ly)) {
                return hit;
            }
        }
        if (val_input_y) {
            if (auto* hit = val_input_y->find_widget_at(lx, ly)) {
                return hit;
            }
        }
        return widget::find_widget_at_impl(lx, ly);
    }

    /**
     * @brief Click handler.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Well-shape-aware hit tests.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @param mods Modifiers.
     * @return True if inside radial bounds.
     */
    virtual bool on_click_impl(float lx, float ly, uint32_t mods) override {
        if (!enabled) return false;

        if (val_input_x && val_input_x->hit_test(lx, ly)) {
            return val_input_x->on_click(lx, ly, mods);
        }
        if (val_input_y && val_input_y->hit_test(lx, ly)) {
            return val_input_y->on_click(lx, ly, mods);
        }

        float cx = bounds.x + bounds.w * 0.5f;
        float cy = bounds.y + bounds.h * 0.58f;
        float radius = (std::min)(bounds.w, bounds.h) * 0.38f;

        float dx = lx - cx;
        float dy = ly - cy;

        if (is_point_inside_well(dx, dy, radius, 5.0f)) {
            pressed = true;
            m_last_mouse_x = lx;
            m_last_mouse_y = ly;

            if (mods & PF_Mod_CMD_CTRL_KEY) {
                // Fine-tune mode: start from current visual position without snapping
                if (opts.snap_to_center) {
                    m_raw_x = 0.0f;
                    m_raw_y = 0.0f;
                } else {
                    m_raw_x = data.x;
                    m_raw_y = data.y;
                }
            } else {
                // Normal mode: snap to clicked position
                float nx = dx / radius;
                float ny = dy / radius;
                
                // Apply resistance: scale down displacement
                float res_factor = 1.0f - (std::clamp)(opts.resistance, 0.0f, 1.0f);
                nx *= res_factor;
                ny *= res_factor;
                
                clamp_normalized_position(nx, ny);
                m_raw_x = nx;
                m_raw_y = ny;
            }

            update_value_from_raw();
            return true;
        }
        return false;
    }

    virtual bool on_drag_impl(float lx, float ly, uint32_t mods) override {
        if (val_input_x && val_input_x->is_focused()) {
            return val_input_x->on_drag(lx, ly, mods);
        }
        if (val_input_y && val_input_y->is_focused()) {
            return val_input_y->on_drag(lx, ly, mods);
        }

        if (!pressed) return false;

        float radius = (std::min)(bounds.w, bounds.h) * 0.38f;
        if (radius <= 0.0f) return false;

        float dlx = lx - m_last_mouse_x;
        float dly = ly - m_last_mouse_y;

        m_last_mouse_x = lx;
        m_last_mouse_y = ly;

        float speed_multiplier = (mods & PF_Mod_CMD_CTRL_KEY) ? 0.1f : 1.0f;

        float dnx = (dlx / radius) * speed_multiplier;
        float dny = (dly / radius) * speed_multiplier;

        // Apply resistance on the delta
        float res_factor = 1.0f - (std::clamp)(opts.resistance, 0.0f, 1.0f);
        dnx *= res_factor;
        dny *= res_factor;

        m_raw_x += dnx;
        m_raw_y += dny;

        clamp_normalized_position(m_raw_x, m_raw_y);
        update_value_from_raw();
        return true;
    }

    virtual void on_release_impl() override {
        if (val_input_x) val_input_x->on_release();
        if (val_input_y) val_input_y->on_release();
        pressed = false;
        scrubbing = false;
        if (opts.snap_to_center) {
            visual_x = 0.0f;
            visual_y = 0.0f;
        }
    }

    virtual bool on_key_impl(const interaction_context& ctx) override {
        if (val_input_x && val_input_x->is_focused()) {
            return val_input_x->on_key(ctx);
        }
        if (val_input_y && val_input_y->is_focused()) {
            return val_input_y->on_key(ctx);
        }
        return false;
    }

    /**
     * @brief Syncs state parameters.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Syncs sequence coordinates automatically.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    virtual void sync_state_impl(const interaction_context& ctx) override {
        auto lock = ctx.arb_data<joystick_data>(m_param_index);
        if (lock) {
            data = *lock;
            if (!pressed && !scrubbing) {
                if (opts.snap_to_center) {
                    visual_x = 0.0f;
                    visual_y = 0.0f;
                    m_raw_x = 0.0f;
                    m_raw_y = 0.0f;
                } else {
                    visual_x = data.x;
                    visual_y = data.y;
                    m_raw_x = data.x;
                    m_raw_y = data.y;
                }
            }
            update_input_texts();
        }
    }

    /**
     * @brief Commits state changes.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Commits coordinates back to After Effects parameters.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    virtual void commit_state_impl(const interaction_context& ctx) override {
        auto lock = ctx.arb_data<joystick_data>(m_param_index);
        if (lock && (*lock != data)) {
            *lock = data;
            ctx.mark_param_changed(m_param_index);
        }
    }

    /**
     * @brief Inspects custom cursor type on hover bounds.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Set cursor to fingerscrub icon.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Cursor type value.
     */
    virtual int32_t cursor_type_impl() const override {
        return PF_Cursor_FINGER_POINTER;
    }

protected:
    /**
     * @brief Safe subclass event hook (NVI Expansion).
     *
     * @note <b>AE SDK Paradigm Shift:</b> Normalized movement coordinate callback.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param x Horizontal normalized position.
     * @param y Vertical normalized position.
     */
    virtual void on_joystick_moved(float x, float y) {}

private:
    std::unique_ptr<text_input> val_input_x;
    std::unique_ptr<text_input> val_input_y;
    bool scrubbing = false;
    bool has_dragged = false;
    float initial_x = 0.0f;
    float initial_y = 0.0f;
    float initial_val_x = 0.0f;
    float initial_val_y = 0.0f;
    float m_raw_x = 0.0f;
    float m_raw_y = 0.0f;
    float m_last_mouse_x = 0.0f;
    float m_last_mouse_y = 0.0f;

    void update_input_texts() {
        if (val_input_x && !val_input_x->is_focused()) {
            char buf[32];
            aetk::core::c_snprintf(buf, sizeof(buf), "%.2f", data.x);
            val_input_x->set_text(buf);
        }
        if (val_input_y && !val_input_y->is_focused()) {
            char buf[32];
            aetk::core::c_snprintf(buf, sizeof(buf), "%.2f", data.y);
            val_input_y->set_text(buf);
        }
    }

    bool is_point_inside_well(float dx, float dy, float radius, float padding = 0.0f) const {
        if (opts.shape == joystick_shape::square) {
            return std::abs(dx) <= radius + padding && std::abs(dy) <= radius + padding;
        }

        float dist = std::sqrt(dx * dx + dy * dy);
        return dist <= radius + padding;
    }

    void clamp_normalized_position(float& nx, float& ny) const {
        if (opts.shape == joystick_shape::square) {
            nx = (std::clamp)(nx, -1.0f, 1.0f);
            ny = (std::clamp)(ny, -1.0f, 1.0f);
            return;
        }

        float length = std::sqrt(nx * nx + ny * ny);
        if (length > 1.0f) {
            nx /= length;
            ny /= length;
        }
    }

    void apply_response_curve(float& nx, float& ny) const {
        const float exponent = (std::clamp)(opts.response_exponent, 0.25f, 4.0f);
        if (std::abs(exponent - 1.0f) <= 0.001f) {
            return;
        }

        if (opts.shape == joystick_shape::square) {
            nx = std::copysign(std::pow(std::abs(nx), exponent), nx);
            ny = std::copysign(std::pow(std::abs(ny), exponent), ny);
            return;
        }

        const float length = std::sqrt(nx * nx + ny * ny);
        if (length <= 0.0001f) {
            return;
        }

        const float shaped_length = std::pow(length, exponent);
        const float scale = shaped_length / length;
        nx *= scale;
        ny *= scale;
    }

    void update_value_from_raw() {
        float nx = m_raw_x;
        float ny = m_raw_y;

        apply_response_curve(nx, ny);

        data.x = nx;
        data.y = ny;
        visual_x = nx;
        visual_y = ny;

        // Trigger dynamic subclass hook
        on_joystick_moved(data.x, data.y);
    }
};

} // namespace aetk::effect::ui
