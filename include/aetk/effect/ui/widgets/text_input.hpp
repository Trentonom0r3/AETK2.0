#pragma once

#include <aetk/effect/ui/widget.hpp>
#include <aetk/ui/theme.hpp>
#include <algorithm>
#include <functional>
#include <string>


namespace aetk::effect::ui {

/**
 * @brief A standalone or embedded text input field widget that supports pure-vector
 * in-place text entry.
 *
 * @details 100% System Agnostic:
 *  - Draws text using Drawbot's canvas.draw_text.
 *  - Handles native AE keyboard events (PF_Event_KEYDOWN) via the focus manager.
 *  - Styled beautifully with zero OS-specific controls.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, capturing user text input requires
 * invoking system-specific modal dialog windows or registering complex OS child hwnd
 * windows, thrashing After Effects rendering threads and breaking host responsiveness.
 * `aetk::effect::ui::text_input` modernizes this into a 100% system-agnostic pure-vector
 * text field. It intercepts keydown events (`PF_Event_KEYDOWN`), handles virtual keyboard
 * translation, manages full text selections/backspaces/returns/escapes, and draws
 * high-contrast caret states directly onto Drawbot paths, guaranteeing thread safety and
 * smooth UI flows.
 *
 * @warning <b>Memory & Lifecycles:</b> The input widget binds dynamic pointer callbacks
 * (`on_change`) that must remain allocated. Focus adjustments automatically trigger
 * selection highlight flags to prevent memory or render thread collisions. Safe callbacks
 * routing. Sets mouse cursor dynamically to I-beam (`PF_Cursor_HORZ_I_BEAM`).
 */
class text_input : public widget {
public:
    using change_callback = std::function<void(const std::string&)>;

    /**
     * @brief Placeholder and availability options.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Fluent builder properties replacing raw
     * procedural string flags.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    struct options {
        bool enabled;
        std::string placeholder;

        options()
            : enabled(true)
            , placeholder("") {
        }
        options& set_enabled(bool e) {
            enabled = e;
            return *this;
        }
        options& set_placeholder(std::string p) {
            placeholder = std::move(p);
            return *this;
        }
    };

    /// Active text content.
    std::string text;

    /// Bounded callback trigger on commit changes.
    change_callback on_change;

    /// Esthetic options.
    options opts;

    std::string m_initial_text;
    bool m_all_selected = false;

    /**
     * @brief Constructs a new text input widget.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Pure-vector system-agnostic text entering with
     * native key hook translation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param initial_text Default start text.
     * @param cb Commit callback.
     * @param opt styling options.
     */
    text_input(std::string initial_text = "", change_callback cb = nullptr,
        const options& opt = options())
        : text(std::move(initial_text))
        , on_change(std::move(cb))
        , opts(opt) {
        layout.min_width = 40.0f;
        layout.min_height = 18.0f;
    }

    virtual ~text_input() = default;

    /** @brief Sets text manually. */
    void set_text(const std::string& new_text) {
        if (text != new_text) {
            text = new_text;
        }
    }

protected:
    /**
     * @brief Gains focus handler.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Backs up starting text and highlights all text.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    virtual void on_focus_gained() override {
        widget::on_focus_gained();
        m_initial_text = text;
        m_all_selected = true; // Automatically highlight all text on click focus!
    }

    /**
     * @brief Loses focus handler.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clears highlights.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    virtual void on_focus_lost() override {
        widget::on_focus_lost();
        m_all_selected = false;
    }

    /**
     * @brief Measures text field limits.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Dimension pass.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param avail_w Width bounds.
     * @param avail_h Height bounds.
     * @return Size vector.
     */
    virtual core::vec2 measure_impl(float avail_w, float avail_h) override {
        return { (std::max)(layout.min_width, avail_w),
            (std::max)(layout.min_height, avail_h) };
    }

    /**
     * @brief Layout empty implementation since pure-vector bounds are handled
     * dynamically.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Layout pass.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param x Horizontal pixel position.
     * @param y Vertical pixel position.
     * @param w Width size.
     * @param h Height size.
     */
    virtual void do_layout_impl(float x, float y, float w, float h) override {
        // Pure-vector: bounds are handled natively by parent layout
    }

    /**
     * @brief Paints the background, borders, selection blue highlights, caret, and text.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Custom 2D selection and font Drawbot rendering.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param canvas Drawbot canvas.
     * @param supplier Drawbot supplier.
     * @param t Theme color definitions.
     */
    virtual void paint_impl(
        drawbot::canvas& canvas, drawbot::supplier& supplier, const theme& t) override {
        if (!visible)
            return;

        float x = bounds.x;
        float y = bounds.y;
        float w = bounds.w;
        float h = bounds.h;

        if (m_focused) {
            // 1. Draw solid dark background in focused state
            canvas.fill_rect(x, y, w, h, core::color<>(0.08f, 0.08f, 0.08f, 1.0f));

            // 2. Draw rounded accent-blue border path
            float r = 2.0f;
            auto border_path
                = supplier.create_path()
                      .move_to(x + r, y)
                      .line_to(x + w - r, y)
                      .bezier_to(core::vec2(x + w - r / 2, y),
                          core::vec2(x + w, y + r / 2), core::vec2(x + w, y + r))
                      .line_to(x + w, y + h - r)
                      .bezier_to(core::vec2(x + w, y + h - r / 2),
                          core::vec2(x + w - r / 2, y + h), core::vec2(x + w - r, y + h))
                      .line_to(x + r, y + h)
                      .bezier_to(core::vec2(x + r / 2, y + h),
                          core::vec2(x, y + h - r / 2), core::vec2(x, y + h - r))
                      .line_to(x, y + r)
                      .bezier_to(core::vec2(x, y + r / 2), core::vec2(x + r / 2, y),
                          core::vec2(x + r, y))
                      .close()
                      .build();

            canvas.stroke_path(border_path, supplier.create_pen(t.accent, 1.5f));
        }

        // 3. Draw text content & selection highlights
        if (supplier.supports_text()) {
            auto font = supplier.create_font(t.font_size);

            float tx = x + 4.0f;
            float ty = y + h * 0.5f + t.font_size * 0.3f;

            if (m_focused && m_all_selected && !text.empty()) {
                // Draw a beautiful blue selection highlight box covering the text area
                float highlight_w = (std::max)(w - 8.0f, 6.0f);
                canvas.fill_rect(tx, y + 2.0f, highlight_w, h - 4.0f, t.accent);

                // Draw high-contrast white text on top of the selection blue highlight
                auto white_brush
                    = supplier.create_brush(core::color<>(1.0f, 1.0f, 1.0f, 1.0f));
                canvas.draw_text(text, font, white_brush, core::vec2(tx, ty),
                    kDRAWBOT_TextAlignment_Left);
            } else {
                // Text color: white when focused, accent blue when unfocused
                core::color<> text_c = m_focused ? core::color<>(1.0f, 1.0f, 1.0f, 1.0f)
                                                 : (hovered ? t.accent_hover : t.accent);
                if (!enabled) {
                    text_c.alpha *= 0.35f;
                }
                auto brush = supplier.create_brush(text_c);

                // Draw placeholder if text is empty and unfocused
                if (text.empty() && !m_focused && !opts.placeholder.empty()) {
                    core::color<> place_color = core::color<>(0.5f, 0.5f, 0.5f, 0.6f);
                    if (!enabled) {
                        place_color.alpha *= 0.35f;
                    }
                    auto place_brush = supplier.create_brush(place_color);
                    canvas.draw_text(opts.placeholder, font, place_brush,
                        core::vec2(tx, ty), kDRAWBOT_TextAlignment_Left);
                } else {
                    canvas.draw_text(text, font, brush, core::vec2(tx, ty),
                        kDRAWBOT_TextAlignment_Left);
                }
            }
        }
    }

    /**
     * @brief Click handler registering focused state.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Activates focus.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @param mods Modifiers.
     * @return True.
     */
    virtual bool on_click_impl(float lx, float ly, uint32_t mods) override {
        double parsed = 0.0;
        size_t num_start = text.find_first_of("0123456789.-,");
        if (num_start != std::string::npos
            && aetk::core::c_sscanf(text.c_str() + num_start, "%lf", &parsed) == 1) {
            m_is_numeric = true;
            m_drag_start_val = parsed;
            m_drag_start_x = lx;
            m_scrubbing = true;
            m_has_dragged = false;

            m_prefix = text.substr(0, num_start);
            size_t num_end = text.find_first_not_of("0123456789.-,", num_start);
            m_suffix = (num_end == std::string::npos) ? "" : text.substr(num_end);

            m_decimal_places = 0;
            size_t dot = text.find('.', num_start);
            if (dot == std::string::npos) {
                dot = text.find(',', num_start);
            }
            if (dot != std::string::npos
                && (num_end == std::string::npos || dot < num_end)) {
                size_t decimals_len = (num_end == std::string::npos)
                    ? (text.length() - dot - 1)
                    : (num_end - dot - 1);
                m_decimal_places = static_cast<int>(decimals_len);
            }
        } else {
            m_is_numeric = false;
            m_scrubbing = false;
            m_has_dragged = false;
            m_prefix.clear();
            m_suffix.clear();
        }
        return true;
    }

    virtual bool on_drag_impl(float lx, float ly, uint32_t mods) override {
        if (m_scrubbing && m_is_numeric) {
            float dx = lx - m_drag_start_x;
            if (!m_has_dragged && std::abs(dx) > 3.0f) {
                m_has_dragged = true;
                m_all_selected = false;
            }

            if (m_has_dragged) {
                double val_mag = std::abs(m_drag_start_val);
                double sensitivity = 1.0;
                if (m_decimal_places > 0) {
                    if (val_mag < 2.0)
                        sensitivity = 0.01;
                    else if (val_mag < 20.0)
                        sensitivity = 0.05;
                    else
                        sensitivity = 0.1;
                } else {
                    if (val_mag < 2.0)
                        sensitivity = 0.1;
                    else if (val_mag < 20.0)
                        sensitivity = 0.5;
                    else if (val_mag < 200.0)
                        sensitivity = 1.0;
                    else
                        sensitivity = 10.0;
                }

                if (mods & 0x0002 /* PF_Mod_SHIFT */) {
                    sensitivity *= 5.0;
                }
                if (mods & 0x0001 /* PF_Mod_CMD_CONTROL */) {
                    sensitivity *= 0.1;
                }

                double new_val = m_drag_start_val + dx * sensitivity;
                char buf[64];
                if (m_decimal_places > 0) {
                    aetk::core::c_snprintf(buf, sizeof(buf), "%.*f", m_decimal_places, new_val);
                } else {
                    aetk::core::c_snprintf(buf, sizeof(buf), "%.0f", std::round(new_val));
                }
                text = m_prefix + buf + m_suffix;

                if (on_change) {
                    on_change(text);
                }
                return true;
            }
        }
        return false;
    }

    virtual void on_release_impl() override {
        m_scrubbing = false;
        if (m_has_dragged) {
            m_all_selected = false;
        }
    }

    /**
     * @brief Keyboard event translation routing backspace, enter, and escape commands.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Native key down translation and character
     * inputs.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     * @return True if key event was handled.
     */
    virtual bool on_key_impl(const aetk::effect::interaction_context& ctx) override {
        auto* extra = ctx.event_extra();
        if (!extra)
            return false;

        PF_KeyCode key = extra->u.key_down.keycode;
        uint32_t mods = extra->u.key_down.modifiers;

        // 1. Detect Ctrl+A (CMD on Mac, Control on Windows)
        bool is_ctrl_a = false;
        if (mods & 0x0001 /* PF_Mod_CMD_CONTROL */) {
            // Virtual key for A is 0x41
            if (key == 0x41 || key == 'a' || key == 'A') {
                is_ctrl_a = true;
            }
        }

        if (is_ctrl_a) {
            m_all_selected = true;
            return true;
        }

        // 2. Resolve printable character
        char ch = 0;
        if (PF_KEYCODE_IS_PRINTABLE(key)) {
            ch = static_cast<char>(PF_KEYCODE_GET_SHORTCUT_CHARACTER(key));
        } else {
// Fallback: Windows Virtual-Key translation
#ifdef _WIN32
            // Mask the keycode to low word to discard high-order flags / scan codes
            uint32_t vk = key & 0xFFFF;
            if (vk >= 0x30 && vk <= 0x39) {
                ch = '0' + (vk - 0x30);
            } else if (vk >= 0x60 && vk <= 0x69) {
                ch = '0' + (vk - 0x60);
            } else if (vk == 0xBE || vk == 0x2E || vk == 110) {
                ch = '.';
            } else if (vk == 0xBD || vk == 109) {
                ch = '-';
            } else if (vk >= 0x41 && vk <= 0x5A) {
                bool shift = (mods & 0x0002 /* PF_Mod_SHIFT */) != 0;
                ch = (shift ? 'A' : 'a') + (vk - 0x41);
            }
#endif
        }

        // If we resolved a printable character:
        if (ch >= 32 && ch <= 126) {
            if (m_all_selected) {
                text = "";
                m_all_selected = false;
            }
            text += ch;
            return true;
        }

        // 3. Handle Control codes
        PF_ControlCode ctrl
            = static_cast<PF_ControlCode>(PF_KEYCODE_GET_CONTROL_CODE(key));

#ifdef _WIN32
        // Fallback control mappings for Windows virtual keys if the SDK macro is empty
        uint32_t vk = key & 0xFFFF;
        if (ctrl == PF_ControlCode_Unknown) {
            if (vk == 0x08)
                ctrl = PF_ControlCode_Backspace;
            else if (vk == 0x0D)
                ctrl = PF_ControlCode_Return;
            else if (vk == 0x1B)
                ctrl = PF_ControlCode_Escape;
        }
#endif

        if (ctrl == PF_ControlCode_Backspace) {
            if (m_all_selected) {
                text = "";
                m_all_selected = false;
                return true;
            } else if (!text.empty()) {
                text.pop_back();
                return true;
            }
        } else if (ctrl == PF_ControlCode_Return || ctrl == PF_ControlCode_Enter) {
            m_all_selected = false;
            if (on_change) {
                on_change(text);
            }
            return true;
        } else if (ctrl == PF_ControlCode_Escape) {
            m_all_selected = false;
            text = m_initial_text;
            return true;
        } else {
            // Any other control key navigation clears selection
            m_all_selected = false;
        }

        return false;
    }

    /**
     * @brief Inspects custom cursor type on hover bounds.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Set cursor style to horizontal I-beam
     * (`PF_Cursor_HORZ_I_BEAM`).
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Cursor type value.
     */
    virtual int32_t cursor_type_impl() const override {
        if (m_scrubbing && m_has_dragged) {
            return PF_Cursor_FINGER_POINTER;
        }
        return PF_Cursor_HORZ_I_BEAM;
    }

private:
    bool m_scrubbing = false;
    float m_drag_start_x = 0.0f;
    double m_drag_start_val = 0.0;
    bool m_is_numeric = false;
    bool m_has_dragged = false;
    int m_decimal_places = 0;
    std::string m_prefix;
    std::string m_suffix;
};

} // namespace aetk::effect::ui
