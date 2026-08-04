#pragma once

#include <aetk/effect/ui/widget.hpp>
#include <aetk/effect/ui/widgets/slider_data.hpp>
#include <aetk/effect/ui/widgets/text_input.hpp>
#include <aetk/ui/theme.hpp>
#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>


namespace aetk::effect::ui {

// ══════════════════════════════════════════════════════════════════════
//  Slider — Horizontal drag slider with value display
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief A horizontal slider with a draggable thumb.
 *
 * @details Displays a track line with a filled portion and a value label.
 * Click anywhere on the track to snap the value.
 * Drag the thumb for continuous adjustment.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, building a custom slider control
 * requires deep event handling loops, calculating pixel offsets manually, rendering
 * linear lines/rectangular knobs, and integrating separate sub-components for direct
 * value inputs. `aetk::effect::ui::slider` unifies this into a single templated OOD
 * widget. It handles step-snapping, linear normalized tracking, active fill strokes, and
 * embeds a dynamic `text_input` field. Any value alterations automatically synchronize
 * back to persistent parameter sequences (`slider_data<T>`) so changes are locked across
 * host frame requests.
 *
 * @warning <b>Memory & Lifecycles:</b> The slider owns a persistent unique pointer to its
 * child `text_input` component. Numeric modifications are synchronized and committed
 * automatically to After Effects sequence structures via `sync_state_impl` and
 * `commit_state_impl` pipelines. Destroys sub-text inputs cleanly. Integrates mouse
 * cursor shapes (`PF_Cursor_FINGER_POINTER_SCRUB`) automatically.
 *
 * @tparam T Templated scalar value type.
 */
template <typename T = float> class slider : public widget {
public:
    using data_type = slider_data<T>;

    /**
     * @brief Custom aesthetic parameters and text formats.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Fluent builder properties replacing raw
     * procedural slider track flags.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    struct options {
        // Colors
        core::color<> track_color;
        core::color<> active_track_color;
        core::color<> thumb_color;

        // Behavior & Display
        T step;
        std::string display_format;
        bool show_label;

        /**
         * @brief options constructor.
         *
         * @note <b>AE SDK Paradigm Shift:</b> Sets default values for track colors,
         * active colors, and steps.
         *
         * @warning <b>Memory & Lifecycles:</b> None.
         */
        options()
            : track_color(0.0f, 0.0f, 0.0f, 0.0f)
            , active_track_color(0.0f, 0.0f, 0.0f, 0.0f)
            , thumb_color(0.0f, 0.0f, 0.0f, 0.0f)
            , step(T(0))
            , display_format(std::is_floating_point_v<T> ? "%.2f" : "%d")
            , show_label(true) {
        }

        // Builder methods
        options& set_track_color(core::color<> c) {
            track_color = c;
            return *this;
        }
        options& set_active_track_color(core::color<> c) {
            active_track_color = c;
            return *this;
        }
        options& set_thumb_color(core::color<> c) {
            thumb_color = c;
            return *this;
        }
        options& set_step(T s) {
            step = s;
            return *this;
        }
        options& set_display_format(std::string fmt) {
            display_format = std::move(fmt);
            return *this;
        }
        options& set_show_label(bool show) {
            show_label = show;
            return *this;
        }
    };

    /**
     * @brief Fluent state defaults for setup.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Fluent state defaults.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Slider default data.
     */
    static data_type get_default_data(const std::string& /*label*/, T /*min_v*/,
        T /*max_v*/, T default_v, std::function<void(T)> /*callback*/ = nullptr,
        const options& /*opt*/ = options()) {
        data_type d;
        d.value = default_v;
        return d;
    }

    /// Display slider name.
    std::string text;

    T min_val;
    T max_val;
    T value;

    /// Value change callback.
    std::function<void(T)> on_change;

    /// Styling options.
    options opts;

    /// Bounded parameter index.
    int32_t m_param_index = 0;

    /**
     * @brief Constructs a new slider widget.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Modern NVI-patterned horizontal slider widget
     * with recursive text-field coordinate searches.
     *
     * @warning <b>Memory & Lifecycles:</b> Spawns and takes unique ownership of a text
     * input component.
     *
     * @param param_idx Bounded parameter index.
     * @param label Slider label name.
     * @param min_v Minimum value limit.
     * @param max_v Maximum value limit.
     * @param default_v Default start value.
     * @param callback Value alteration callback.
     * @param opt Custom styling options.
     */
    slider(int32_t param_idx, std::string label, T min_v, T max_v, T default_v,
        std::function<void(T)> callback = nullptr, const options& opt = options())
        : m_param_index(param_idx)
        , text(std::move(label))
        , min_val(min_v)
        , max_val(max_v)
        , value(default_v)
        , on_change(std::move(callback))
        , opts(opt) {
        layout.min_height = 18.0f;
        // Default step for integers is 1 if not specified
        if constexpr (std::is_integral_v<T>) {
            if (opts.step == 0)
                opts.step = 1;
        }

        val_input = std::make_unique<text_input>("", [this](const std::string& str) {
            double parsed = 0.0;
            if (aetk::core::c_sscanf(str.c_str(), "%lf", &parsed) == 1) {
                T new_val = static_cast<T>(parsed);
                new_val = (std::clamp)(new_val, min_val, max_val);
                if (new_val != value) {
                    value = new_val;
                    if (on_change)
                        on_change(value);
                    on_slider_changed(value);
                }
            }
            update_input_text();
        });
    }

protected:
    /**
     * @brief Measures slider dimensions.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Layout size passing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param avail_w Width bounds.
     * @return Size vector.
     */
    virtual core::vec2 measure_impl(float avail_w, float) override {
        return { avail_w, layout.min_height };
    }

    /**
     * @brief Paints the track, active strokes, compact fader thumb, and text inputs.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Custom 2D slider track and fader knob Drawbot
     * rendering.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param canvas Drawbot canvas.
     * @param supplier Drawbot supplier.
     * @param t Theme color parameters.
     */
    virtual void paint_impl(
        drawbot::canvas& canvas, drawbot::supplier& supplier, const theme& t) override {
        float track_y = bounds.y + bounds.h * 0.5f;

        // Normalized position (0..1)
        float norm = 0.0f;
        if (max_val > min_val) {
            norm = static_cast<float>(value - min_val)
                / static_cast<float>(max_val - min_val);
        }

        // Beautiful AE Effect Panel spacing
        float label_w = bounds.w * 0.35f;
        float value_w = bounds.w * 0.15f;

        float track_left = bounds.x + label_w + 8.0f;
        float track_right = bounds.x + bounds.w - value_w - 8.0f;
        float track_w = track_right - track_left;
        float thumb_x = track_left + norm * track_w;

        // Colors
        core::color<> t_color
            = opts.track_color.alpha > 0.0f ? opts.track_color : t.border;
        core::color<> a_color
            = opts.active_track_color.alpha > 0.0f ? opts.active_track_color : t.accent;
        core::color<> th_color = opts.thumb_color.alpha > 0.0f
            ? opts.thumb_color
            : ((pressed || hovered) ? t.handle_active : t.handle);

        // Draw track background (neat 2px native look)
        auto track_pen = supplier.create_pen(t_color, 2.0f);
        canvas.stroke_path(supplier.create_path()
                               .move_to(track_left, track_y)
                               .line_to(track_right, track_y)
                               .build(),
            track_pen);

        // Draw filled portion (2px native look)
        if (norm > 0.001f) {
            auto fill_pen = supplier.create_pen(a_color, 2.0f);
            canvas.stroke_path(supplier.create_path()
                                   .move_to(track_left, track_y)
                                   .line_to(thumb_x, track_y)
                                   .build(),
                fill_pen);
        }

        // Draw thumb (premium compact AE fader knob style)
        float thumb_w = 4.0f;
        float thumb_h = 12.0f;
        canvas.fill_rect(thumb_x - thumb_w * 0.5f, track_y - thumb_h * 0.5f, thumb_w,
            thumb_h, th_color);

        // Text baseline vertically centered
        float text_y = bounds.y + bounds.h * 0.5f + t.font_size * 0.3f;

        // Draw label (on the left)
        if (!text.empty()) {
            auto font = supplier.create_font(t.font_size);
            auto text_brush = supplier.create_brush(t.text);
            canvas.draw_text(text, font, text_brush, core::vec2(bounds.x + 4.0f, text_y),
                kDRAWBOT_TextAlignment_Left);
        }

        // Draw the persistent text input (on the right)
        if (opts.show_label && val_input) {
            update_input_text();
            val_input->paint(canvas, supplier, t);
        }
    }

    /**
     * @brief Click handler snapping values to clicked horizontals.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Snaps coordinate ratios to values.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal coordinate.
     * @param ly Vertical coordinate.
     * @return True if click registered.
     */
    virtual bool on_click_impl(float lx, float ly, uint32_t) override {
        if (!enabled)
            return false;

        pressed = true;
        update_value_from_x(lx);
        return true;
    }

    /**
     * @brief Drag handler updating value from drag offsets.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Horizontal coordinate dragging updates.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @return True if drag registered.
     */
    virtual bool on_drag_impl(float lx, float ly, uint32_t) override {
        if (!pressed)
            return false;
        update_value_from_x(lx);
        return true;
    }

    /**
     * @brief Performs layout alignments of the right text box.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Absolute coordinates for right input text.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param x Horizontal pixel position.
     * @param y Vertical pixel position.
     * @param w Layout width limit.
     * @param h Layout height limit.
     */
    virtual void do_layout_impl(float x, float y, float w, float h) override {
        float value_w = bounds.w * 0.15f;
        float edit_x = bounds.x + bounds.w - value_w - 4.0f;
        float edit_y = bounds.y + (bounds.h - 18.0f) * 0.5f;
        float edit_w = value_w;
        float edit_h = 18.0f;
        if (val_input) {
            val_input->do_layout(edit_x, edit_y, edit_w, edit_h);
        }
    }

    /**
     * @brief Traverses active widgets down to the input fields first.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Z-order traversal check to numeric input text.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @return Widget reference.
     */
    virtual widget* find_widget_at_impl(float lx, float ly) override {
        if (opts.show_label && val_input) {
            if (auto* hit = val_input->find_widget_at(lx, ly)) {
                return hit;
            }
        }
        return widget::find_widget_at_impl(lx, ly);
    }

    /**
     * @brief Syncs state parameters.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Auto-syncs persistent slider scalar value.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    virtual void sync_state_impl(const interaction_context& ctx) override {
        auto lock = ctx.arb_data<slider_data<T>>(m_param_index);
        if (lock) {
            value = lock->value;
            update_input_text();
        }
    }

    /**
     * @brief Commits state changes.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Commits scalar changes back to After Effects
     * parameters.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    virtual void commit_state_impl(const interaction_context& ctx) override {
        auto lock = ctx.arb_data<slider_data<T>>(m_param_index);
        if (lock && lock->value != value) {
            lock->value = value;
            ctx.mark_param_changed(m_param_index);
        }
    }

    /**
     * @brief Release handler.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Release reset.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    virtual void on_release_impl() override {
        pressed = false;
        scrubbing = false;
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
        return PF_Cursor_FINGER_POINTER_SCRUB;
    }

    virtual void paint_title_impl(drawbot::canvas& canvas, drawbot::supplier& supplier,
        const theme& t, float x, float y, float w, float h) override {
        m_title_x = x;
        if (supplier.supports_text()) {
            char buf[64] = { 0 };
            if constexpr (std::is_floating_point_v<T>) {
                aetk::core::c_snprintf(buf, sizeof(buf), opts.display_format.c_str(),
                    static_cast<double>(value));
            } else {
                aetk::core::c_snprintf(buf, sizeof(buf), opts.display_format.c_str(),
                    static_cast<int>(value));
            }

            std::string val_str = buf;
            if (text.find('%') != std::string::npos
                && val_str.find('%') == std::string::npos) {
                val_str += "%";
            }

            auto font = supplier.create_font(t.font_size * 0.9f);
            auto brush = supplier.create_brush(
                m_title_drag_active ? t.accent_pressed : t.accent);
            float ty = y + h * 0.5f + t.font_size * 0.28f;
            canvas.draw_text(val_str, font, brush, core::vec2(x + 4.0f, ty),
                kDRAWBOT_TextAlignment_Left);
        }
    }

    virtual bool hit_test_title(float lx, float ly) const override {
        float dx = lx - m_title_x;
        return dx >= 4.0f && dx <= 80.0f;
    }

    virtual bool on_title_click_impl(float lx, float ly, uint32_t mods) override {
        if (!enabled)
            return false;
        float dx = lx - m_title_x;
        if (dx < 4.0f || dx > 80.0f)
            return false;

        m_title_drag_active = true;
        m_title_initial_x = lx;
        m_title_initial_val = value;
        return true;
    }

    virtual bool on_title_drag_impl(float lx, float ly, uint32_t mods) override {
        if (!m_title_drag_active)
            return false;
        float dx = lx - m_title_initial_x;

        double speed = 1.0;
        if constexpr (std::is_floating_point_v<T>) {
            double range = static_cast<double>(max_val - min_val);
            if (range <= 0.0)
                range = 100.0;
            speed = range / 300.0; // 300px for full sweep
        } else {
            speed = 0.5; // 1 unit per 2 pixels
        }

        if (mods & PF_Mod_SHIFT_KEY) {
            speed *= 10.0;
        } else if (mods & PF_Mod_CMD_CTRL_KEY) {
            speed *= 0.1;
        }

        T new_val;
        if constexpr (std::is_floating_point_v<T>) {
            new_val = static_cast<T>(m_title_initial_val + dx * speed);
            if (opts.step > 0) {
                new_val = std::round(new_val / opts.step) * opts.step;
            }
        } else {
            new_val = static_cast<T>(std::round(m_title_initial_val + dx * speed));
            if (opts.step > 0) {
                new_val
                    = static_cast<T>(std::round(static_cast<double>(new_val) / opts.step))
                    * opts.step;
            }
        }

        new_val = (std::clamp)(new_val, min_val, max_val);
        if (new_val != value) {
            value = new_val;
            if (on_change)
                on_change(value);
            on_slider_changed(value);
        }
        return true;
    }

    virtual void on_title_release_impl() override {
        m_title_drag_active = false;
    }

protected:
    /**
     * @brief Safe subclass event hook (NVI Expansion).
     *
     * @note <b>AE SDK Paradigm Shift:</b> Slider value change callback.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param val New slider value.
     */
    virtual void on_slider_changed(T val) {
    }

private:
    std::unique_ptr<text_input> val_input;
    bool scrubbing = false;
    bool has_dragged = false;
    float initial_x = 0.0f;
    T initial_val = { };
    bool m_title_drag_active = false;
    float m_title_initial_x = 0.0f;
    T m_title_initial_val = { };
    float m_title_x = 0.0f;

    void update_input_text() {
        if (!val_input)
            return;
        if (val_input->is_focused())
            return;

        char buf[64] = { 0 };
        if constexpr (std::is_floating_point_v<T>) {
            aetk::core::c_snprintf(buf, sizeof(buf), opts.display_format.c_str(),
                static_cast<double>(value));
        } else {
            aetk::core::c_snprintf(
                buf, sizeof(buf), opts.display_format.c_str(), static_cast<int>(value));
        }
        val_input->set_text(buf);
    }

    void update_value_from_x(float lx) {
        float label_w = bounds.w * 0.35f;
        float value_w = bounds.w * 0.15f;

        float track_left = bounds.x + label_w + 8.0f;
        float track_right = bounds.x + bounds.w - value_w - 8.0f;
        float track_w = track_right - track_left;

        if (track_w <= 0.0f)
            return;

        float norm = (std::clamp)((lx - track_left) / track_w, 0.0f, 1.0f);
        T new_val;

        if constexpr (std::is_floating_point_v<T>) {
            new_val = min_val + norm * (max_val - min_val);
            if (opts.step > 0) {
                new_val = std::round(new_val / opts.step) * opts.step;
            }
        } else {
            new_val = static_cast<T>(std::round(min_val + norm * (max_val - min_val)));
            if (opts.step > 0) {
                new_val
                    = static_cast<T>(std::round(static_cast<double>(new_val) / opts.step))
                    * opts.step;
            }
        }

        new_val = (std::clamp)(new_val, min_val, max_val);

        if (new_val != value) {
            value = new_val;
            if (on_change)
                on_change(value);
            on_slider_changed(value);
        }
    }
};

} // namespace aetk::effect::ui
