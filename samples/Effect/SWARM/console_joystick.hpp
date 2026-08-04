#pragma once

#include "console_joystick_data.hpp"
#include <aetk/core/locale_utils.hpp>
#include <aetk/effect/context/context.hpp>
#include <aetk/effect/ui/widget.hpp>
#include <aetk/effect/ui/widgets/joystick_data.hpp>
#include <aetk/effect/ui/widgets/joystick_shape.hpp>
#include <aetk/effect/ui/widgets/slider_data.hpp>
#include <aetk/ui/theme.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace aetk::effect::ui {

struct console_axis_mapping {
    float min_val;
    float max_val;

    float to_physical(float norm) const {
        return min_val + (norm + 1.0f) * 0.5f * (max_val - min_val);
    }

    float to_normalized(float phys) const {
        if (max_val <= min_val)
            return 0.0f;
        return (phys - min_val) / (max_val - min_val) * 2.0f - 1.0f;
    }
};

/**
 * @brief Consolidated SWARM Console Joystick Pad (Full-Width).
 *
 * @details Features:
 *  - Full-width 2D grid well with 3D sphere knob
 *  - Title-level X/Y scrubbing controls
 *  - Stacked 2-row HUD readout in bottom-left corner
 *  - Context menu expands inline downward from slide-out tooltip badge
 *  - Slightly expanded well height (170px) to comfortably fit bottom menu
 */
class console_joystick_pad : public widget {
public:
    using data_type = console_joystick_data;

    struct options {
        joystick_shape shape = joystick_shape::square;
        float response_exponent = 1.6f;
        float preferred_width = 500.0f;
        float preferred_height = 170.0f;
        float well_radius = 26.0f;
        std::array<joystick_data, 3> defaults;
        std::array<std::string, 3> well_labels;
        std::array<std::function<std::string(float, float)>, 3> value_formatters;
        std::array<std::array<console_axis_mapping, 2>, 3> axis_ranges;
        std::array<std::array<std::function<std::string(float)>, 2>, 3> axis_formatters;

        options()
            : defaults { joystick_data { 0.0f, 0.0f }, joystick_data { 0.0f, 0.0f },
                joystick_data { 0.0f, 0.0f } }
            , well_labels { "Silhouette", "HSV Keyer", "YOLOv8 AI" } {
            for (int i = 0; i < 3; ++i) {
                axis_ranges[i][0] = { -1.0f, 1.0f };
                axis_ranges[i][1] = { -1.0f, 1.0f };

                axis_formatters[i][0] = [](float val) {
                    char buf[32];
                    aetk::core::c_snprintf(buf, sizeof(buf), "X: %.1f", val);
                    return std::string(buf);
                };
                axis_formatters[i][1] = [](float val) {
                    char buf[32];
                    aetk::core::c_snprintf(buf, sizeof(buf), "Y: %.1f", val);
                    return std::string(buf);
                };
            }
        }

        options& set_shape(joystick_shape s) {
            shape = s;
            return *this;
        }
        options& set_response_exponent(float exponent) {
            response_exponent = exponent;
            return *this;
        }
        options& set_preferred_size(float width, float height) {
            preferred_width = width;
            preferred_height = height;
            return *this;
        }
        options& set_well_radius(float radius) {
            well_radius = radius;
            return *this;
        }
        options& set_default_value(size_t idx, const joystick_data& value) {
            if (idx < defaults.size()) {
                defaults[idx] = value;
            }
            return *this;
        }
        options& set_well_label(size_t idx, std::string label) {
            if (idx < well_labels.size()) {
                well_labels[idx] = std::move(label);
            }
            return *this;
        }
        options& set_value_formatter(size_t idx, std::function<std::string(float, float)> formatter) {
            if (idx < value_formatters.size()) {
                value_formatters[idx] = std::move(formatter);
            }
            return *this;
        }
        options& set_axis_range(size_t well_idx, size_t axis_idx, float min_val, float max_val) {
            if (well_idx < axis_ranges.size() && axis_idx < 2) {
                axis_ranges[well_idx][axis_idx] = { min_val, max_val };
            }
            return *this;
        }
        options& set_axis_formatter(size_t well_idx, size_t axis_idx, std::function<std::string(float)> formatter) {
            if (well_idx < axis_formatters.size() && axis_idx < 2) {
                axis_formatters[well_idx][axis_idx] = std::move(formatter);
            }
            return *this;
        }
    };

    int m_param_index;
    std::string text;
    std::string m_mode_key;
    options m_opts;
    data_type data;

    int active_mode = 2; // Default to YOLOv8 AI (2)
    bool pressed = false;

    // Visual coordinates
    float visual_sil_x = 0.0f, visual_sil_y = 0.0f;
    float visual_hsv_x = 0.0f, visual_hsv_y = 0.0f;
    float visual_yolo_x = 0.0f, visual_yolo_y = 0.0f;

    // Title scrubbing state
    float m_title_x = 0.0f;
    float m_title_w = 0.0f;
    int m_title_hover_axis = -1;
    int m_title_drag_axis = -1;
    bool m_title_drag_active = false;
    float m_title_initial_x = 0.0f;
    float m_title_initial_val = 0.0f;

    // Context menus and icon hover states
    int m_active_menu = -1;      // -1: none, 0: Output Mode, 1: Detect Style, 2: Spawning Mode
    int m_hovered_button = -1;   // -1: none, 0: Output Mode, 1: Detect Style, 2: Spawning Mode
    int m_hovered_menu_item = -1;

    bounds_rect m_btn_bounds[3];
    bounds_rect m_tooltip_bounds[3];
    bounds_rect m_menu_container_bounds;
    std::vector<bounds_rect> m_menu_item_bounds;

    console_joystick_pad(int32_t param_idx, std::string label = "", std::string mode_key = "", options opts = options())
        : m_param_index(param_idx)
        , text(std::move(label))
        , m_mode_key(std::move(mode_key))
        , m_opts(std::move(opts)) {
        layout.min_width = m_opts.preferred_width;
        layout.min_height = m_opts.preferred_height;
    }

    static data_type get_default_data(std::string label = "", std::string mode_key = "", options opts = options()) {
        data_type d;
        d.silhouette = opts.defaults[0];
        d.hsv = opts.defaults[1];
        d.yolo = opts.defaults[2];
        d.output_mode = 0;
        d.detect_style = 2;
        d.spawning_mode = 0;
        return d;
    }

protected:
    virtual aetk::core::vec2 measure_impl(float avail_w, float) override {
        return { (std::max)(avail_w, layout.min_width), layout.min_height };
    }

    virtual int32_t cursor_type_impl() const override {
        return PF_Cursor_FINGER_POINTER;
    }

    // ══════════════════════════════════════════════════════════════════
    //  Title-Level Dragging & Rendering (X/Y Scrubbing in Title Row)
    // ══════════════════════════════════════════════════════════════════

    virtual void paint_title_impl(drawbot::canvas& canvas, drawbot::supplier& supplier,
        const theme& t, float x, float y, float w, float h) override {
        m_title_x = x;
        m_title_w = w;
        if (supplier.supports_text() && active_mode >= 0 && active_mode < 3) {
            float nx = (active_mode == 0) ? visual_sil_x : ((active_mode == 1) ? visual_hsv_x : visual_yolo_x);
            float ny = (active_mode == 0) ? visual_sil_y : ((active_mode == 1) ? visual_hsv_y : visual_yolo_y);
            float px = m_opts.axis_ranges[active_mode][0].to_physical(nx);
            float py = m_opts.axis_ranges[active_mode][1].to_physical(ny);

            char x_buf[32] = { 0 };
            char y_buf[32] = { 0 };
            aetk::core::c_snprintf(x_buf, sizeof(x_buf), "%.1f", px);
            aetk::core::c_snprintf(y_buf, sizeof(y_buf), "%.1f", py);

            auto font = supplier.create_font(t.font_size * 0.85f);
            auto label_brush = supplier.create_brush(t.text_dim);

            core::color<> x_color = t.accent;
            core::color<> y_color = t.accent;

            if (m_title_drag_active) {
                if (m_title_drag_axis == 0) x_color = t.accent_pressed;
                else y_color = t.accent_pressed;
            } else {
                if (m_title_hover_axis == 0) x_color = t.handle_active;
                else if (m_title_hover_axis == 1) y_color = t.handle_active;
            }

            auto x_brush = supplier.create_brush(x_color);
            auto y_brush = supplier.create_brush(y_color);

            float ty = y + h * 0.5f + t.font_size * 0.28f;

            canvas.draw_text("X:", font, label_brush, core::vec2(x + 4.0f, ty), kDRAWBOT_TextAlignment_Left);
            canvas.draw_text(x_buf, font, x_brush, core::vec2(x + 22.0f, ty), kDRAWBOT_TextAlignment_Left);

            canvas.draw_text("Y:", font, label_brush, core::vec2(x + 76.0f, ty), kDRAWBOT_TextAlignment_Left);
            canvas.draw_text(y_buf, font, y_brush, core::vec2(x + 94.0f, ty), kDRAWBOT_TextAlignment_Left);
        }
    }

    virtual bool hit_test_title(float lx, float ly) const override {
        if (active_mode < 0 || active_mode >= 3) return false;
        float dx = lx - m_title_x;
        if (dx >= 22.0f && dx <= 72.0f) return true;
        if (dx >= 94.0f && dx <= 144.0f) return true;
        return false;
    }

    virtual bool on_title_hover_move(float lx, float ly) override {
        if (active_mode < 0 || active_mode >= 3) return false;
        float dx = lx - m_title_x;
        int old_hover = m_title_hover_axis;
        if (dx >= 22.0f && dx <= 72.0f) m_title_hover_axis = 0;
        else if (dx >= 94.0f && dx <= 144.0f) m_title_hover_axis = 1;
        else m_title_hover_axis = -1;
        return m_title_hover_axis != old_hover;
    }

    virtual bool on_title_click_impl(float lx, float ly, uint32_t mods) override {
        if (!enabled || active_mode < 0 || active_mode >= 3) return false;
        float dx = lx - m_title_x;
        if (dx >= 22.0f && dx <= 72.0f) {
            m_title_drag_axis = 0;
            m_title_drag_active = true;
            m_title_initial_x = lx;
            float nx = (active_mode == 0) ? visual_sil_x : ((active_mode == 1) ? visual_hsv_x : visual_yolo_x);
            m_title_initial_val = m_opts.axis_ranges[active_mode][0].to_physical(nx);
            return true;
        }
        if (dx >= 94.0f && dx <= 144.0f) {
            m_title_drag_axis = 1;
            m_title_drag_active = true;
            m_title_initial_x = lx;
            float ny = (active_mode == 0) ? visual_sil_y : ((active_mode == 1) ? visual_hsv_y : visual_yolo_y);
            m_title_initial_val = m_opts.axis_ranges[active_mode][1].to_physical(ny);
            return true;
        }
        return false;
    }

    virtual bool on_title_drag_impl(float lx, float ly, uint32_t mods) override {
        if (!m_title_drag_active || active_mode < 0 || active_mode >= 3) return false;

        float dx = lx - m_title_initial_x;
        const auto& range_info = m_opts.axis_ranges[active_mode][m_title_drag_axis];
        double range = static_cast<double>(range_info.max_val - range_info.min_val);
        if (range <= 0.0) range = 2.0;

        double speed = range / 300.0;
        if (mods & PF_Mod_SHIFT_KEY) speed *= 10.0;
        else if (mods & PF_Mod_CMD_CTRL_KEY) speed *= 0.1;

        double phys_val = std::clamp(m_title_initial_val + dx * speed, static_cast<double>(range_info.min_val), static_cast<double>(range_info.max_val));
        float norm = range_info.to_normalized(static_cast<float>(phys_val));

        if (m_title_drag_axis == 0) {
            if (active_mode == 0) { data.silhouette.x = norm; visual_sil_x = norm; }
            else if (active_mode == 1) { data.hsv.x = norm; visual_hsv_x = norm; }
            else { data.yolo.x = norm; visual_yolo_x = norm; }
        } else {
            if (active_mode == 0) { data.silhouette.y = norm; visual_sil_y = norm; }
            else if (active_mode == 1) { data.hsv.y = norm; visual_hsv_y = norm; }
            else { data.yolo.y = norm; visual_yolo_y = norm; }
        }
        return true;
    }

    virtual void on_title_release_impl() override {
        m_title_drag_active = false;
    }

    // ══════════════════════════════════════════════════════════════════
    //  Control Area Painting (Full-Width Well, Floating Buttons, Stacked HUD)
    // ══════════════════════════════════════════════════════════════════

    virtual void paint_impl(drawbot::canvas& canvas, drawbot::supplier& supplier, const theme& t) override {
        canvas.fill_rect(bounds.x, bounds.y, bounds.w, bounds.h, t.bg);

        float grid_x = bounds.x + 4.0f;
        float grid_y = bounds.y + 2.0f;
        float grid_w = bounds.w - 8.0f;
        float grid_h = bounds.h - 4.0f;
        float grid_cx = grid_x + grid_w * 0.5f;
        float grid_cy = grid_y + grid_h * 0.5f;

        auto make_rounded_rect_path = [&](float rx, float ry, float rw, float rh, float corner) {
            float left = rx, right = rx + rw, top = ry, bottom = ry + rh;
            return supplier.create_path()
                .move_to(left + corner, top)
                .line_to(right - corner, top)
                .bezier_to({ right - corner * 0.45f, top }, { right, top + corner * 0.45f }, { right, top + corner })
                .line_to(right, bottom - corner)
                .bezier_to({ right, bottom - corner * 0.45f }, { right - corner * 0.45f, bottom }, { right - corner, bottom })
                .line_to(left + corner, bottom)
                .bezier_to({ left + corner * 0.45f, bottom }, { left, bottom - corner * 0.45f }, { left, bottom - corner })
                .line_to(left, top + corner)
                .bezier_to({ left, top + corner * 0.45f }, { left + corner * 0.45f, top }, { left + corner, top })
                .close()
                .build();
        };

        core::color<> bg_well { 1.0f, 45.0f / 255.0f, 45.0f / 255.0f, 45.0f / 255.0f };
        auto grid_path = make_rounded_rect_path(grid_x, grid_y, grid_w, grid_h, 6.0f);
        canvas.fill_path(grid_path, supplier.create_brush(bg_well));
        canvas.stroke_path(grid_path, supplier.create_pen(core::color<> { 1.0f, 70.0f / 255.0f, 70.0f / 255.0f, 70.0f / 255.0f }, 1.0f));

        // Grid lines
        auto grid_pen = supplier.create_pen(core::color<>(0.25f, 70.0f / 255.0f, 70.0f / 255.0f, 70.0f / 255.0f), 1.0f);
        for (int k = 1; k < 20; ++k) {
            float lx = grid_x + grid_w * k / 20.0f;
            canvas.stroke_path(supplier.create_path().move_to(lx, grid_y).line_to(lx, grid_y + grid_h).build(), grid_pen);
        }
        for (int k = 1; k < 10; ++k) {
            float ly = grid_y + grid_h * k / 10.0f;
            canvas.stroke_path(supplier.create_path().move_to(grid_x, ly).line_to(grid_x + grid_w, ly).build(), grid_pen);
        }

        // Center crosshair
        auto center_pen = supplier.create_pen(core::color<>(0.60f, 70.0f / 255.0f, 70.0f / 255.0f, 70.0f / 255.0f), 1.2f);
        canvas.stroke_path(supplier.create_path().move_to(grid_cx, grid_y).line_to(grid_cx, grid_y + grid_h).build(), center_pen);
        canvas.stroke_path(supplier.create_path().move_to(grid_x, grid_cy).line_to(grid_x + grid_w, grid_cy).build(), center_pen);

        // Header Mode Badge
        if (supplier.supports_text() && active_mode >= 0 && active_mode < 3) {
            std::string mode_lbl = m_opts.well_labels[active_mode];
            std::string badge_text = mode_lbl + " TRACKING";
            for (auto& c : badge_text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

            auto font_title = supplier.create_font(t.font_size * 0.82f);
            float text_w = badge_text.length() * (t.font_size * 0.52f);
            float badge_w = text_w + 24.0f;
            float badge_h = 16.0f;
            float badge_x = grid_cx - badge_w * 0.5f;
            float badge_y = grid_y - badge_h * 0.5f;

            auto badge_path = supplier.create_path()
                                  .move_to(badge_x + 6.0f, badge_y)
                                  .line_to(badge_x + badge_w - 6.0f, badge_y)
                                  .line_to(badge_x + badge_w, badge_y + badge_h)
                                  .line_to(badge_x, badge_y + badge_h)
                                  .close()
                                  .build();

            core::color<> badge_bg = t.bg;
            badge_bg.red *= 0.4f; badge_bg.green *= 0.4f; badge_bg.blue *= 0.4f;
            canvas.fill_path(badge_path, supplier.create_brush(badge_bg));
            canvas.stroke_path(badge_path, supplier.create_pen(t.border, 1.0f));

            canvas.draw_text(badge_text, font_title, supplier.create_brush(t.text),
                core::vec2(grid_cx, badge_y + badge_h * 0.5f + t.font_size * 0.3f),
                kDRAWBOT_TextAlignment_Center);
        }

        // Stacked 2-row read-only HUD info overlay in bottom-left corner
        if (supplier.supports_text() && active_mode >= 0 && active_mode < 3) {
            float nx = (active_mode == 0) ? visual_sil_x : ((active_mode == 1) ? visual_hsv_x : visual_yolo_x);
            float ny = (active_mode == 0) ? visual_sil_y : ((active_mode == 1) ? visual_hsv_y : visual_yolo_y);

            std::string row1_text, row2_text;
            if (active_mode == 0) { // Silhouette
                float threshold_val = (nx + 1.0f) * 122.5f + 5.0f;
                float min_area_val = (ny + 1.0f) * 495.0f + 10.0f;
                char b1[64], b2[64];
                aetk::core::c_snprintf(b1, sizeof(b1), "Threshold: %.0f", threshold_val);
                aetk::core::c_snprintf(b2, sizeof(b2), "Min Area: %.0f px", min_area_val);
                row1_text = b1; row2_text = b2;
            } else if (active_mode == 1) { // HSV Keyer
                float hue_range = (nx + 1.0f) * 72.5f + 35.0f;
                float sat_val = (ny + 1.0f) * 12.5f + 175.0f;
                char b1[64], b2[64];
                aetk::core::c_snprintf(b1, sizeof(b1), "Hue Range: %.0f\xC2\xB0", hue_range);
                aetk::core::c_snprintf(b2, sizeof(b2), "Sat/Val: %.0f", sat_val);
                row1_text = b1; row2_text = b2;
            } else { // YOLOv8 AI
                float max_det = (nx + 1.0f) * 249.5f + 1.0f;
                float min_area_val = (ny + 1.0f) * 4950.0f + 100.0f;
                char b1[64], b2[64];
                aetk::core::c_snprintf(b1, sizeof(b1), "Max Detections: %.0f", max_det);
                aetk::core::c_snprintf(b2, sizeof(b2), "Min Area: %.0f px", min_area_val);
                row1_text = b1; row2_text = b2;
            }

            auto hud_font = supplier.create_font(t.font_size * 0.96f);
            core::color<> hud_c(0.75f, 180.0f / 255.0f, 220.0f / 255.0f, 240.0f / 255.0f);

            // Row 1
            canvas.draw_text(row1_text, hud_font, supplier.create_brush(hud_c),
                core::vec2(grid_x + 10.0f, grid_y + grid_h - 24.0f),
                kDRAWBOT_TextAlignment_Left);
            // Row 2
            canvas.draw_text(row2_text, hud_font, supplier.create_brush(hud_c),
                core::vec2(grid_x + 10.0f, grid_y + grid_h - 8.0f),
                kDRAWBOT_TextAlignment_Left);
        }

        // Active mode coordinates for 3D sphere knob
        float nx = (active_mode == 0) ? visual_sil_x : ((active_mode == 1) ? visual_hsv_x : visual_yolo_x);
        float ny = (active_mode == 0) ? visual_sil_y : ((active_mode == 1) ? visual_hsv_y : visual_yolo_y);

        float px = grid_cx + nx * (grid_w * 0.45f);
        float py = grid_cy + ny * (grid_h * 0.42f);
        float knob_r = 16.0f;

        auto make_circle_path = [&](float ccx, float ccy, float cr) {
            float k = cr * 0.55228475f;
            return supplier.create_path()
                .move_to(ccx, ccy - cr)
                .bezier_to({ ccx + k, ccy - cr }, { ccx + cr, ccy - k }, { ccx + cr, ccy })
                .bezier_to({ ccx + cr, ccy + k }, { ccx + k, ccy + cr }, { ccx, ccy + cr })
                .bezier_to({ ccx - k, ccy + cr }, { ccx - cr, ccy + k }, { ccx - cr, ccy })
                .bezier_to({ ccx - cr, ccy - k }, { ccx - k, ccy - cr }, { ccx, ccy - cr })
                .build();
        };

        // Metallic knob body
        for (int step = 0; step < 8; ++step) {
            float t_step = step / 7.0f;
            float r = knob_r * (1.0f - t_step * 0.7f);
            float val = 0.22f + t_step * 0.68f;
            core::color<> metal_col(1.0f, val, val, val);
            metal_col.red = metal_col.red * 0.82f + t.accent.red * 0.18f;
            metal_col.green = metal_col.green * 0.82f + t.accent.green * 0.18f;
            metal_col.blue = metal_col.blue * 0.82f + t.accent.blue * 0.18f;
            canvas.fill_path(make_circle_path(px - t_step * knob_r * 0.2f, py - t_step * knob_r * 0.2f, r), supplier.create_brush(metal_col));
        }

        float ring_r = knob_r + 4.0f;
        core::color<> ring_col = (pressed) ? t.handle_active : t.accent;
        ring_col.alpha = 0.6f;
        canvas.stroke_path(make_circle_path(px, py, ring_r), supplier.create_pen(ring_col, 1.2f));

        // ══════════════════════════════════════════════════════════════════
        //  Floating Mode Icon Buttons (State-Specific Custom Vector Icons)
        // ══════════════════════════════════════════════════════════════════
        float btn_r = 14.0f;
        float btn_x = grid_x + grid_w - 24.0f;
        float btn_y_offsets[3] = { grid_cy - 44.0f, grid_cy - 4.0f, grid_cy + 36.0f };

        std::vector<std::string> opts_output = { "Composite", "Transparent", "Matte" };
        std::vector<std::string> opts_detect = { "Dark", "Bright", "Auto" };
        std::vector<std::string> opts_spawning = { "Centroid", "Grid Fill", "Contour Outline" };

        for (int b = 0; b < 3; ++b) {
            float bx = btn_x;
            float by = btn_y_offsets[b];

            m_btn_bounds[b] = { bx - btn_r, by - btn_r, btn_r * 2.0f, btn_r * 2.0f };

            bool is_hov = (m_hovered_button == b);
            bool is_active_menu = (m_active_menu == b);

            core::color<> disc_bg = (is_hov || is_active_menu)
                ? core::color<>(0.95f, 60.0f / 255.0f, 60.0f / 255.0f, 60.0f / 255.0f)
                : core::color<>(0.80f, 35.0f / 255.0f, 35.0f / 255.0f, 35.0f / 255.0f);
            canvas.fill_path(make_circle_path(bx, by, btn_r), supplier.create_brush(disc_bg));

            core::color<> disc_bc = (is_hov || is_active_menu) ? t.accent : core::color<>(0.8f, 90.0f / 255.0f, 90.0f / 255.0f, 90.0f / 255.0f);
            canvas.stroke_path(make_circle_path(bx, by, btn_r), supplier.create_pen(disc_bc, 1.2f));

            core::color<> icon_c = (is_hov || is_active_menu) ? core::color<>(1.0f, 1.0f, 1.0f, 1.0f) : core::color<>(1.0f, 0.85f, 0.85f, 0.85f);

            if (b == 0) {
                // ── Button 0: Output Mode Icons ──
                if (data.output_mode == 0) {
                    canvas.stroke_path(supplier.create_path().move_to(bx - 5.0f, by - 4.0f).line_to(bx + 5.0f, by - 4.0f).build(), supplier.create_pen(icon_c, 1.5f));
                    canvas.stroke_path(supplier.create_path().move_to(bx - 5.0f, by).line_to(bx + 5.0f, by).build(), supplier.create_pen(icon_c, 1.5f));
                    canvas.stroke_path(supplier.create_path().move_to(bx - 5.0f, by + 4.0f).line_to(bx + 5.0f, by + 4.0f).build(), supplier.create_pen(icon_c, 1.5f));
                } else if (data.output_mode == 1) {
                    auto rect_p = make_rounded_rect_path(bx - 5.0f, by - 5.0f, 10.0f, 10.0f, 1.0f);
                    canvas.stroke_path(rect_p, supplier.create_pen(icon_c, 1.2f));
                    canvas.fill_path(supplier.create_path().move_to(bx - 4.0f, by - 4.0f).line_to(bx, by - 4.0f).line_to(bx, by).line_to(bx - 4.0f, by).close().build(), supplier.create_brush(icon_c));
                    canvas.fill_path(supplier.create_path().move_to(bx, by).line_to(bx + 4.0f, by).line_to(bx + 4.0f, by + 4.0f).line_to(bx, by + 4.0f).close().build(), supplier.create_brush(icon_c));
                } else {
                    auto fill_p = make_rounded_rect_path(bx - 5.0f, by - 5.0f, 10.0f, 10.0f, 1.0f);
                    canvas.fill_path(fill_p, supplier.create_brush(icon_c));
                }
            } else if (b == 1) {
                // ── Button 1: Detect Style Icons ──
                if (data.detect_style == 0) {
                    auto moon_p = supplier.create_path()
                                      .move_to(bx + 1.0f, by - 5.0f)
                                      .bezier_to({ bx - 5.0f, by - 5.0f }, { bx - 5.0f, by + 5.0f }, { bx + 1.0f, by + 5.0f })
                                      .bezier_to({ bx - 2.0f, by + 2.5f }, { bx - 2.0f, by - 2.5f }, { bx + 1.0f, by - 5.0f })
                                      .close()
                                      .build();
                    canvas.fill_path(moon_p, supplier.create_brush(icon_c));
                } else if (data.detect_style == 1) {
                    canvas.fill_path(make_circle_path(bx, by, 3.0f), supplier.create_brush(icon_c));
                    for (int r_idx = 0; r_idx < 6; ++r_idx) {
                        float ang = r_idx * 1.04719755f;
                        float rx1 = bx + std::cos(ang) * 4.5f;
                        float ry1 = by + std::sin(ang) * 4.5f;
                        float rx2 = bx + std::cos(ang) * 6.5f;
                        float ry2 = by + std::sin(ang) * 6.5f;
                        canvas.stroke_path(supplier.create_path().move_to(rx1, ry1).line_to(rx2, ry2).build(), supplier.create_pen(icon_c, 1.0f));
                    }
                } else {
                    auto half_p = supplier.create_path()
                                      .move_to(bx, by - 5.0f)
                                      .bezier_to({ bx + 4.0f, by - 5.0f }, { bx + 4.0f, by + 5.0f }, { bx, by + 5.0f })
                                      .close()
                                      .build();
                    canvas.fill_path(half_p, supplier.create_brush(icon_c));
                    canvas.stroke_path(make_circle_path(bx, by, 5.0f), supplier.create_pen(icon_c, 1.0f));
                }
            } else if (b == 2) {
                // ── Button 2: Spawning Mode Icons ──
                if (data.spawning_mode == 0) {
                    canvas.fill_path(make_circle_path(bx, by, 2.0f), supplier.create_brush(icon_c));
                    canvas.stroke_path(supplier.create_path().move_to(bx - 6.0f, by).line_to(bx - 3.0f, by).build(), supplier.create_pen(icon_c, 1.2f));
                    canvas.stroke_path(supplier.create_path().move_to(bx + 3.0f, by).line_to(bx + 6.0f, by).build(), supplier.create_pen(icon_c, 1.2f));
                    canvas.stroke_path(supplier.create_path().move_to(bx, by - 6.0f).line_to(bx, by - 3.0f).build(), supplier.create_pen(icon_c, 1.2f));
                    canvas.stroke_path(supplier.create_path().move_to(bx, by + 3.0f).line_to(bx, by + 6.0f).build(), supplier.create_pen(icon_c, 1.2f));
                } else if (data.spawning_mode == 1) {
                    for (int gx = -4; gx <= 4; gx += 4) {
                        for (int gy = -4; gy <= 4; gy += 4) {
                            canvas.fill_path(make_circle_path(bx + gx, by + gy, 1.2f), supplier.create_brush(icon_c));
                        }
                    }
                } else {
                    auto box_p = make_rounded_rect_path(bx - 5.0f, by - 5.0f, 10.0f, 10.0f, 1.5f);
                    canvas.stroke_path(box_p, supplier.create_pen(icon_c, 1.2f));
                }
            }

            // Draw Hover Tooltip Badge (when hovered and menu closed)
            if (is_hov && m_active_menu < 0 && supplier.supports_text()) {
                std::string tip_txt;
                if (b == 0) tip_txt = "Output: " + opts_output[data.output_mode];
                else if (b == 1) tip_txt = "Detect: " + opts_detect[data.detect_style];
                else if (b == 2) tip_txt = "Spawning: " + opts_spawning[data.spawning_mode];

                float font_sz = t.font_size * 0.8f;
                auto tip_font = supplier.create_font(font_sz);
                float tip_w = tip_txt.length() * (font_sz * 0.6f) + 12.0f;
                float tip_h = 18.0f;
                float tip_x = bx - btn_r - tip_w - 6.0f;
                float tip_y = by - tip_h * 0.5f;

                m_tooltip_bounds[b] = { tip_x, tip_y, tip_w + btn_r + 6.0f, tip_h };

                auto tip_p = make_rounded_rect_path(tip_x, tip_y, tip_w, tip_h, 3.0f);
                canvas.fill_path(tip_p, supplier.create_brush(core::color<>(0.95f, 25.0f / 255.0f, 25.0f / 255.0f, 25.0f / 255.0f)));
                canvas.stroke_path(tip_p, supplier.create_pen(t.accent, 1.0f));

                canvas.draw_text(tip_txt, tip_font, supplier.create_brush(core::color<>(1.0f, 1.0f, 1.0f, 1.0f)),
                    core::vec2(tip_x + tip_w * 0.5f, tip_y + tip_h * 0.5f + font_sz * 0.3f),
                    kDRAWBOT_TextAlignment_Center);
            }
        }

        // ══════════════════════════════════════════════════════════════════
        //  Inline Context Popup Menu (Opens Inline with Tooltip & Expands Down)
        // ══════════════════════════════════════════════════════════════════
        if (m_active_menu >= 0 && m_active_menu < 3 && supplier.supports_text()) {
            int b = m_active_menu;
            float bx = btn_x;
            float by = btn_y_offsets[b];

            const std::vector<std::string>& menu_opts = (b == 0) ? opts_output : ((b == 1) ? opts_detect : opts_spawning);
            int selected_val = (b == 0) ? data.output_mode : ((b == 1) ? data.detect_style : data.spawning_mode);

            float font_sz = t.font_size * 0.85f;
            auto menu_font = supplier.create_font(font_sz);
            float item_h = 18.0f;
            float max_opt_w = 60.0f;

            for (const auto& opt : menu_opts) {
                float opt_w = opt.length() * (font_sz * 0.65f);
                if (opt_w > max_opt_w) max_opt_w = opt_w;
            }

            float menu_w = max_opt_w + 24.0f;
            float menu_h = menu_opts.size() * (item_h + 1.0f) + 4.0f;
            float menu_x = bx - btn_r - menu_w - 6.0f;
            
            // Align menu top exactly with top of tooltip badge, expanding downward!
            float menu_y = by - 9.0f;
            if (menu_y + menu_h > grid_y + grid_h - 2.0f) {
                menu_y = grid_y + grid_h - menu_h - 2.0f; // Soft clamp to stay within well
            }

            m_menu_container_bounds = { menu_x, menu_y, menu_w + btn_r + 6.0f, menu_h };

            // Menu Container
            auto menu_bg_path = make_rounded_rect_path(menu_x, menu_y, menu_w, menu_h, 4.0f);
            canvas.fill_path(menu_bg_path, supplier.create_brush(core::color<>(0.98f, 30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f)));
            canvas.stroke_path(menu_bg_path, supplier.create_pen(t.accent, 1.0f));

            m_menu_item_bounds.resize(menu_opts.size());

            // Menu Items
            for (int j = 0; j < (int)menu_opts.size(); ++j) {
                float iy = menu_y + 2.0f + j * (item_h + 1.0f);
                m_menu_item_bounds[j] = { menu_x + 2.0f, iy, menu_w - 4.0f, item_h };

                bool is_item_hov = (m_hovered_menu_item == j);
                bool is_selected = (selected_val == j);

                if (is_item_hov || is_selected) {
                    core::color<> item_bg = is_selected ? t.accent : core::color<>(0.8f, 60.0f / 255.0f, 60.0f / 255.0f, 60.0f / 255.0f);
                    if (!is_selected && is_item_hov) item_bg = core::color<>(0.9f, 80.0f / 255.0f, 80.0f / 255.0f, 80.0f / 255.0f);
                    canvas.fill_path(make_rounded_rect_path(menu_x + 3.0f, iy, menu_w - 6.0f, item_h, 2.0f), supplier.create_brush(item_bg));
                }

                if (is_selected) {
                    canvas.fill_path(make_circle_path(menu_x + 9.0f, iy + item_h * 0.5f, 2.5f), supplier.create_brush(core::color<>(1.0f, 1.0f, 1.0f, 1.0f)));
                }

                core::color<> text_c = (is_selected || is_item_hov) ? core::color<>(1.0f, 1.0f, 1.0f, 1.0f) : core::color<>(1.0f, 0.8f, 0.8f, 0.8f);
                canvas.draw_text(menu_opts[j], menu_font, supplier.create_brush(text_c),
                    core::vec2(menu_x + 18.0f, iy + item_h * 0.5f + font_sz * 0.3f),
                    kDRAWBOT_TextAlignment_Left);
            }
        }
    }

    // ══════════════════════════════════════════════════════════════════
    //  Interaction Logic (Instant Click-to-Cycle, Hover-to-Tooltip-to-Menu)
    // ══════════════════════════════════════════════════════════════════

    virtual bool on_click_impl(float lx, float ly, uint32_t mods) override {
        // 1. Check if clicking inside an active open context menu item
        if (m_active_menu >= 0) {
            for (int j = 0; j < (int)m_menu_item_bounds.size(); ++j) {
                if (m_menu_item_bounds[j].contains(lx, ly)) {
                    if (m_active_menu == 0) data.output_mode = j;
                    else if (m_active_menu == 1) data.detect_style = j;
                    else if (m_active_menu == 2) data.spawning_mode = j;

                    m_active_menu = -1;
                    m_hovered_menu_item = -1;
                    return true;
                }
            }
            // Clicked outside context menu -> dismiss menu
            m_active_menu = -1;
        }

        // 2. Check if clicking floating icon buttons -> Instant Click-to-Cycle!
        for (int b = 0; b < 3; ++b) {
            if (m_btn_bounds[b].contains(lx, ly)) {
                if (b == 0) data.output_mode = (data.output_mode + 1) % 3;
                else if (b == 1) data.detect_style = (data.detect_style + 1) % 3;
                else if (b == 2) data.spawning_mode = (data.spawning_mode + 1) % 3;

                m_active_menu = -1; // Close any open menu on click
                return true;
            }
        }

        // 3. Check if clicking inside well area to drag knob
        float grid_x = bounds.x + 4.0f;
        float grid_y = bounds.y + 2.0f;
        float grid_w = bounds.w - 8.0f;
        float grid_h = bounds.h - 4.0f;

        if (lx >= grid_x && lx <= grid_x + grid_w && ly >= grid_y && ly <= grid_y + grid_h) {
            pressed = true;
            update_knob_from_mouse(lx, ly);
            return true;
        }

        return false;
    }

    virtual bool on_drag_impl(float lx, float ly, uint32_t mods) override {
        if (pressed) {
            update_knob_from_mouse(lx, ly);
            return true;
        }
        return false;
    }

    virtual void on_release_impl() override {
        pressed = false;
    }

    virtual bool on_hover_move_impl(float lx, float ly) override {
        int old_btn = m_hovered_button;
        int old_menu_item = m_hovered_menu_item;
        int old_menu = m_active_menu;

        int new_hov_btn = -1;
        m_hovered_menu_item = -1;

        // Check menu item hover
        if (m_active_menu >= 0) {
            for (int j = 0; j < (int)m_menu_item_bounds.size(); ++j) {
                if (m_menu_item_bounds[j].contains(lx, ly)) {
                    m_hovered_menu_item = j;
                    break;
                }
            }
        }

        // Check icon button hover
        for (int b = 0; b < 3; ++b) {
            if (m_btn_bounds[b].contains(lx, ly)) {
                new_hov_btn = b;
                break;
            }
        }

        if (new_hov_btn != -1) {
            m_hovered_button = new_hov_btn;
        } else if (m_hovered_button >= 0) {
            // Check if mouse moved onto the slide-out tooltip badge -> Transform into Context Menu!
            if (m_tooltip_bounds[m_hovered_button].contains(lx, ly)) {
                m_active_menu = m_hovered_button;
            } else if (m_active_menu >= 0 && m_menu_container_bounds.contains(lx, ly)) {
                // Mouse is inside open context menu bounds -> keep menu open
            } else {
                // Mouse moved away from button, tooltip, and menu -> dismiss menu & tooltip
                m_active_menu = -1;
                m_hovered_button = -1;
            }
        }

        return (old_btn != m_hovered_button || old_menu_item != m_hovered_menu_item || old_menu != m_active_menu);
    }

    virtual void on_hover_exit_impl() override {
        if (m_hovered_button != -1 || m_hovered_menu_item != -1 || m_active_menu != -1) {
            m_hovered_button = -1;
            m_hovered_menu_item = -1;
            m_active_menu = -1;
        }
        m_title_hover_axis = -1;
    }

    virtual void sync_state_impl(const interaction_context& ctx) override {
        auto lock = ctx.arb_data<data_type>(m_param_index);
        if (lock) {
            data = *lock;
            if (!pressed && !m_title_drag_active) {
                visual_sil_x = data.silhouette.x; visual_sil_y = data.silhouette.y;
                visual_hsv_x = data.hsv.x; visual_hsv_y = data.hsv.y;
                visual_yolo_x = data.yolo.x; visual_yolo_y = data.yolo.y;
            }
        }

        if (!m_mode_key.empty()) {
            auto mode_param = ctx.param<aetk::effect::arbitrary_param<aetk::effect::ui::slider_data<int>>>(m_mode_key);
            aetk::effect::locked_arbitrary<aetk::effect::ui::slider_data<int>> mode_lock(mode_param);
            if (mode_lock) {
                active_mode = mode_lock->value;
            }
        } else {
            active_mode = 2;
        }
    }

    virtual void commit_state_impl(const interaction_context& ctx) override {
        auto lock = ctx.arb_data<data_type>(m_param_index);
        if (lock && *lock != data) {
            *lock = data;
            ctx.mark_param_changed(m_param_index);
        }
    }

private:
    void update_knob_from_mouse(float lx, float ly) {
        float grid_x = bounds.x + 4.0f;
        float grid_y = bounds.y + 2.0f;
        float grid_w = bounds.w - 8.0f;
        float grid_h = bounds.h - 4.0f;
        float grid_cx = grid_x + grid_w * 0.5f;
        float grid_cy = grid_y + grid_h * 0.5f;

        float norm_x = (lx - grid_cx) / (grid_w * 0.45f);
        float norm_y = (ly - grid_cy) / (grid_h * 0.42f);

        norm_x = std::clamp(norm_x, -1.0f, 1.0f);
        norm_y = std::clamp(norm_y, -1.0f, 1.0f);

        if (active_mode == 0) {
            data.silhouette.x = norm_x; data.silhouette.y = norm_y;
            visual_sil_x = norm_x; visual_sil_y = norm_y;
        } else if (active_mode == 1) {
            data.hsv.x = norm_x; data.hsv.y = norm_y;
            visual_hsv_x = norm_x; visual_hsv_y = norm_y;
        } else {
            data.yolo.x = norm_x; data.yolo.y = norm_y;
            visual_yolo_x = norm_x; visual_yolo_y = norm_y;
        }
    }
};

} // namespace aetk::effect::ui
