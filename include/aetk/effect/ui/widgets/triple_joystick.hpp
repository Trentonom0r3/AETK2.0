#pragma once

#include <aetk/effect/context/context.hpp>
#include <aetk/effect/ui/widget.hpp>
#include <aetk/effect/ui/widgets/joystick_data.hpp>
#include <aetk/effect/ui/widgets/joystick_shape.hpp>
#include <aetk/effect/ui/widgets/slider_data.hpp>
#include <aetk/effect/ui/widgets/text_input.hpp>
#include <aetk/effect/ui/widgets/triple_joystick_data.hpp>
#include <aetk/ui/theme.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>

namespace aetk::effect::ui {

struct axis_mapping {
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

class triple_joystick_text_input : public text_input {
public:
    int well_idx;
    int axis_idx;
    std::function<float()> get_value_fn;
    axis_mapping mapping;

    triple_joystick_text_input(int well, int axis, std::function<float()> get_fn,
        axis_mapping map_val, change_callback cb)
        : text_input("", std::move(cb))
        , well_idx(well)
        , axis_idx(axis)
        , get_value_fn(std::move(get_fn))
        , mapping(map_val) {
    }

protected:
    virtual void on_focus_gained() override {
        float norm_val = get_value_fn();
        float phys_val = mapping.to_physical(norm_val);
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f", phys_val);
        text = buf;

        text_input::on_focus_gained();
    }
};

/**
 * @brief A professional side-by-side triple joystick console trackball widget.
 *
 * @details Manages three independent coordinate pairs inside a single arbitrary
 * parameter. Optional mode binding can dim and lock inactive wells, but the
 * widget also works as a fully generic three-pad controller when no mode key is
 * supplied.
 */
class triple_joystick_pad : public widget {
public:
    using data_type = triple_joystick_data;

    /**
     * @brief Styling options for the triple joystick board.
     *
     * @details The board defaults to circular wells to preserve existing
     * behavior, but can opt into square wells for finer access to corner
     * combinations and full-range diagonal travel.
     */
    struct options {
        joystick_shape shape = joystick_shape::circle;
        float response_exponent = 1.0f;
        float preferred_width = 500.0f;
        float preferred_height = 150.0f;
        float well_radius = 26.0f;
        std::array<joystick_data, 3> defaults;
        std::array<std::string, 3> well_labels;
        std::array<std::function<std::string(float, float)>, 3> value_formatters;
        std::array<std::array<axis_mapping, 2>, 3> axis_ranges;
        std::array<std::array<std::function<std::string(float)>, 2>, 3> axis_formatters;

        options()
            : defaults { joystick_data { 0.0f, 0.0f }, joystick_data { 0.0f, 0.0f },
                joystick_data { 0.0f, 0.0f } }
            , well_labels { "Pad A", "Pad B", "Pad C" } {
            for (int i = 0; i < 3; ++i) {
                axis_ranges[i][0] = { -1.0f, 1.0f };
                axis_ranges[i][1] = { -1.0f, 1.0f };

                axis_formatters[i][0] = [](float val) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "X:%.2f", val);
                    return std::string(buf);
                };
                axis_formatters[i][1] = [](float val) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "Y:%.2f", val);
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
        options& set_value_formatter(
            size_t idx, std::function<std::string(float, float)> formatter) {
            if (idx < value_formatters.size()) {
                value_formatters[idx] = std::move(formatter);
            }
            return *this;
        }
        options& set_axis_range(
            size_t well_idx, size_t axis_idx, float min_val, float max_val) {
            if (well_idx < 3 && axis_idx < 2) {
                axis_ranges[well_idx][axis_idx] = { min_val, max_val };
            }
            return *this;
        }
        options& set_axis_formatter(size_t well_idx, size_t axis_idx,
            std::function<std::string(float)> formatter) {
            if (well_idx < 3 && axis_idx < 2) {
                axis_formatters[well_idx][axis_idx] = std::move(formatter);
            }
            return *this;
        }
    };

    static data_type get_default_data(const std::string& /*label*/ = "",
        const std::string& /*mode_key*/ = "", const options& opt = options()) {
        return { opt.defaults[0], opt.defaults[1], opt.defaults[2] };
    }

    std::string text;
    triple_joystick_data data;
    int32_t m_param_index = 0;
    std::string m_mode_key;
    options m_opts;
    int active_mode = -1; // -1 = all wells active

    bool pressed = false;
    int pressed_well = -1; // 0 = Silhouette, 1 = HSV, 2 = YOLO

    float visual_sil_x = 0.0f, visual_sil_y = 0.0f;
    float visual_hsv_x = 0.0f, visual_hsv_y = 0.0f;
    float visual_yolo_x = 0.0f, visual_yolo_y = 0.0f;

    float m_raw_sil_x = 0.0f, m_raw_sil_y = 0.0f;
    float m_raw_hsv_x = 0.0f, m_raw_hsv_y = 0.0f;
    float m_raw_yolo_x = 0.0f, m_raw_yolo_y = 0.0f;
    float m_last_mouse_x = 0.0f;
    float m_last_mouse_y = 0.0f;

    std::unique_ptr<text_input> m_inputs[3][2];

    float m_title_x = 0.0f;
    float m_title_w = 0.0f;
    int m_title_hover_axis = -1; // -1 = none, 0 = X, 1 = Y
    bool m_title_drag_active = false;
    int m_title_drag_axis = -1; // 0 = X, 1 = Y
    float m_title_initial_x = 0.0f;
    float m_title_initial_val = 0.0f;

    triple_joystick_pad(int32_t param_idx, std::string label, std::string mode_key = "",
        const options& opt = options())
        : m_param_index(param_idx)
        , text(std::move(label))
        , m_mode_key(std::move(mode_key))
        , m_opts(opt) {
        layout.min_height = m_opts.preferred_height;

        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 2; ++j) {
                auto get_fn = [this, i, j]() -> float {
                    if (i == 0)
                        return (j == 0) ? visual_sil_x : visual_sil_y;
                    if (i == 1)
                        return (j == 0) ? visual_hsv_x : visual_hsv_y;
                    return (j == 0) ? visual_yolo_x : visual_yolo_y;
                };

                auto cb_fn = [this, i, j](const std::string& str) {
                    float val;
                    size_t start_pos = str.find_first_of("0123456789.-,");
                    if (start_pos != std::string::npos
                        && aetk::core::c_sscanf(str.c_str() + start_pos, "%f", &val) == 1) {
                        const axis_mapping& mapping = m_opts.axis_ranges[i][j];
                        val = (std::clamp)(val, mapping.min_val, mapping.max_val);
                        float norm = mapping.to_normalized(val);

                        if (i == 0) {
                            if (j == 0) {
                                data.silhouette.x = norm;
                                visual_sil_x = norm;
                            } else {
                                data.silhouette.y = norm;
                                visual_sil_y = norm;
                            }
                        } else if (i == 1) {
                            if (j == 0) {
                                data.hsv.x = norm;
                                visual_hsv_x = norm;
                            } else {
                                data.hsv.y = norm;
                                visual_hsv_y = norm;
                            }
                        } else if (i == 2) {
                            if (j == 0) {
                                data.yolo.x = norm;
                                visual_yolo_x = norm;
                            } else {
                                data.yolo.y = norm;
                                visual_yolo_y = norm;
                            }
                        }
                    }
                };

                m_inputs[i][j] = std::make_unique<triple_joystick_text_input>(
                    i, j, get_fn, m_opts.axis_ranges[i][j], cb_fn);
            }
        }
    }

protected:
    virtual core::vec2 measure_impl(float avail_w, float /*avail_h*/) override {
        return { avail_w, m_opts.preferred_height };
    }

    virtual void do_layout_impl(float x, float y, float w, float h) override {
        if (active_mode >= 0 && active_mode < 3) {
            // Single large well layout
            float margin_x = 4.0f;
            float margin_top = 0.0f;
            float margin_bottom = 0.0f;
            float grid_x = bounds.x + margin_x;
            float grid_y = bounds.y + margin_top;
            float grid_w = bounds.w - 2.0f * margin_x;
            float grid_h = bounds.h - margin_top - margin_bottom;

            float box_h = 16.0f;
            float box_w = 130.0f;

            // Hide all other inputs
            for (int i = 0; i < 3; ++i) {
                if (i != active_mode) {
                    if (m_inputs[i][0])
                        m_inputs[i][0]->visible = false;
                    if (m_inputs[i][1])
                        m_inputs[i][1]->visible = false;
                }
            }

            if (m_inputs[active_mode][0]) {
                m_inputs[active_mode][0]->visible = true;
                m_inputs[active_mode][0]->do_layout(
                    grid_x + 6.0f, grid_y + grid_h - 2.0f * box_h - 4.0f, box_w, box_h);
            }
            if (m_inputs[active_mode][1]) {
                m_inputs[active_mode][1]->visible = true;
                m_inputs[active_mode][1]->do_layout(
                    grid_x + 6.0f, grid_y + grid_h - box_h - 2.0f, box_w, box_h);
            }
        } else {
            // Fallback to original side-by-side layout
            for (int i = 0; i < 3; ++i) {
                if (m_inputs[i][0])
                    m_inputs[i][0]->visible = true;
                if (m_inputs[i][1])
                    m_inputs[i][1]->visible = true;
            }

            float well_y = bounds.y + bounds.h * 0.46f;
            float radius = m_opts.well_radius;

            float cx1 = bounds.x + bounds.w * 0.18f;
            float cx2 = bounds.x + bounds.w * 0.50f;
            float cx3 = bounds.x + bounds.w * 0.82f;

            float cx_arr[3] = { cx1, cx2, cx3 };

            for (int i = 0; i < 3; ++i) {
                float cx = cx_arr[i];
                float box_h = 12.0f;
                float box_w = 54.0f;
                float box_x = cx - box_w * 0.5f;

                if (m_inputs[i][0]) {
                    m_inputs[i][0]->do_layout(
                        box_x, well_y + radius + 4.0f, box_w, box_h);
                }
                if (m_inputs[i][1]) {
                    m_inputs[i][1]->do_layout(
                        box_x, well_y + radius + 4.0f + box_h + 2.0f, box_w, box_h);
                }
            }
        }
    }

    virtual widget* find_widget_at_impl(float lx, float ly) override {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 2; ++j) {
                if (m_inputs[i][j] && m_inputs[i][j]->visible
                    && m_inputs[i][j]->enabled) {
                    if (auto* hit = m_inputs[i][j]->find_widget_at(lx, ly)) {
                        return hit;
                    }
                }
            }
        }
        return widget::find_widget_at_impl(lx, ly);
    }

    virtual void paint_impl(
        drawbot::canvas& canvas, drawbot::supplier& supplier, const theme& t) override {
        // Draw flat background
        canvas.fill_rect(bounds.x, bounds.y, bounds.w, bounds.h, t.bg);

        // Draw the console header title (only in three-well fallback mode)
        if (supplier.supports_text() && !text.empty() && active_mode < 0) {
            auto font_hdr = supplier.create_font(t.font_size);
            canvas.draw_text(text, font_hdr, supplier.create_brush(t.text),
                core::vec2(bounds.x + 8.0f, bounds.y + 14.0f),
                kDRAWBOT_TextAlignment_Left);
        }

        auto make_circle_path = [&](float ccx, float ccy, float cr) {
            float k = cr * 0.55228475f;
            return supplier.create_path()
                .move_to(ccx, ccy - cr)
                .bezier_to(core::vec2(ccx + k, ccy - cr), core::vec2(ccx + cr, ccy - k),
                    core::vec2(ccx + cr, ccy))
                .bezier_to(core::vec2(ccx + cr, ccy + k), core::vec2(ccx + k, ccy + cr),
                    core::vec2(ccx, ccy + cr))
                .bezier_to(core::vec2(ccx - k, ccy + cr), core::vec2(ccx - cr, ccy + k),
                    core::vec2(ccx - cr, ccy))
                .bezier_to(core::vec2(ccx - cr, ccy - k), core::vec2(ccx - k, ccy - cr),
                    core::vec2(ccx, ccy - cr))
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
                .bezier_to(core::vec2(right - corner * 0.45f, top),
                    core::vec2(right, top + corner * 0.45f),
                    core::vec2(right, top + corner))
                .line_to(right, bottom - corner)
                .bezier_to(core::vec2(right, bottom - corner * 0.45f),
                    core::vec2(right - corner * 0.45f, bottom),
                    core::vec2(right - corner, bottom))
                .line_to(left + corner, bottom)
                .bezier_to(core::vec2(left + corner * 0.45f, bottom),
                    core::vec2(left, bottom - corner * 0.45f),
                    core::vec2(left, bottom - corner))
                .line_to(left, top + corner)
                .bezier_to(core::vec2(left, top + corner * 0.45f),
                    core::vec2(left + corner * 0.45f, top),
                    core::vec2(left + corner, top))
                .close()
                .build();
        };

        if (active_mode >= 0 && active_mode < 3) {
            // Single large well mode
            float margin_x = 4.0f;
            float margin_top = 0.0f;
            float margin_bottom = 0.0f;
            float grid_x = bounds.x + margin_x;
            float grid_y = bounds.y + margin_top;
            float grid_w = bounds.w - 2.0f * margin_x;
            float grid_h = bounds.h - margin_top - margin_bottom;

            float grid_cx = grid_x + grid_w * 0.5f;
            float grid_cy = grid_y + grid_h * 0.5f;

            // Draw rounded rectangular grid background outline
            auto make_rounded_rect_path
                = [&](float rx, float ry, float rw, float rh, float corner) {
                      float left = rx;
                      float right = rx + rw;
                      float top = ry;
                      float bottom = ry + rh;

                      return supplier.create_path()
                          .move_to(left + corner, top)
                          .line_to(right - corner, top)
                          .bezier_to(core::vec2(right - corner * 0.45f, top),
                              core::vec2(right, top + corner * 0.45f),
                              core::vec2(right, top + corner))
                          .line_to(right, bottom - corner)
                          .bezier_to(core::vec2(right, bottom - corner * 0.45f),
                              core::vec2(right - corner * 0.45f, bottom),
                              core::vec2(right - corner, bottom))
                          .line_to(left + corner, bottom)
                          .bezier_to(core::vec2(left + corner * 0.45f, bottom),
                              core::vec2(left, bottom - corner * 0.45f),
                              core::vec2(left, bottom - corner))
                          .line_to(left, top + corner)
                          .bezier_to(core::vec2(left, top + corner * 0.45f),
                              core::vec2(left + corner * 0.45f, top),
                              core::vec2(left + corner, top))
                          .close()
                          .build();
                  };

            core::color<> bg_well { 1.0f, 45.0f / 255.0f, 45.0f / 255.0f,
                45.0f / 255.0f };
            auto grid_path = make_rounded_rect_path(grid_x, grid_y, grid_w, grid_h, 6.0f);
            canvas.fill_path(grid_path, supplier.create_brush(bg_well));
            canvas.stroke_path(grid_path,
                supplier.create_pen(core::color<> { 1.0f, 70.0f / 255.0f, 70.0f / 255.0f,
                                        70.0f / 255.0f },
                    1.0f));

            // Draw faint grid lines (20 vertical, 10 horizontal divisions)
            auto grid_pen = supplier.create_pen(
                core::color<>(0.25f, 70.0f / 255.0f, 70.0f / 255.0f, 70.0f / 255.0f),
                1.0f);

            for (int k = 1; k < 20; ++k) {
                float lx_line = grid_x + grid_w * k / 20.0f;
                if (std::abs(lx_line - grid_cx) < 1.0f)
                    continue;
                auto v_line = supplier.create_path()
                                  .move_to(lx_line, grid_y)
                                  .line_to(lx_line, grid_y + grid_h)
                                  .build();
                canvas.stroke_path(v_line, grid_pen);
            }

            for (int k = 1; k < 10; ++k) {
                float ly_line = grid_y + grid_h * k / 10.0f;
                if (std::abs(ly_line - grid_cy) < 1.0f)
                    continue;
                auto h_line = supplier.create_path()
                                  .move_to(grid_x, ly_line)
                                  .line_to(grid_x + grid_w, ly_line)
                                  .build();
                canvas.stroke_path(h_line, grid_pen);
            }

            // Draw prominent center crosshair lines
            auto center_pen = supplier.create_pen(
                core::color<>(0.60f, 70.0f / 255.0f, 70.0f / 255.0f, 70.0f / 255.0f),
                1.2f);

            auto center_v = supplier.create_path()
                                .move_to(grid_cx, grid_y)
                                .line_to(grid_cx, grid_y + grid_h)
                                .build();
            canvas.stroke_path(center_v, center_pen);

            auto center_h = supplier.create_path()
                                .move_to(grid_x, grid_cy)
                                .line_to(grid_x + grid_w, grid_cy)
                                .build();
            canvas.stroke_path(center_h, center_pen);

            // Draw uppercase Mode Label in a custom trapezoidal badge centered at the top
            // grid border
            if (supplier.supports_text()) {
                std::string mode_lbl = m_opts.well_labels[active_mode];
                std::string badge_text = mode_lbl + " TRACKING";
                for (auto& c : badge_text) {
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                }

                auto font_title = supplier.create_font(t.font_size * 0.82f);
                float char_w = t.font_size * 0.52f;
                float text_w = badge_text.length() * char_w;
                float badge_w = text_w + 24.0f;
                float badge_h = 16.0f;
                float badge_x = grid_cx - badge_w * 0.5f;
                float badge_y = grid_y - badge_h * 0.5f;

                // Create the trapezoidal path
                auto badge_path = supplier.create_path()
                                      .move_to(badge_x + 6.0f, badge_y)
                                      .line_to(badge_x + badge_w - 6.0f, badge_y)
                                      .line_to(badge_x + badge_w, badge_y + badge_h)
                                      .line_to(badge_x, badge_y + badge_h)
                                      .close()
                                      .build();

                // Fill with a dark background to block grid lines, and stroke the border
                core::color<> badge_bg = t.bg;
                badge_bg.red *= 0.4f;
                badge_bg.green *= 0.4f;
                badge_bg.blue *= 0.4f;
                canvas.fill_path(badge_path, supplier.create_brush(badge_bg));
                canvas.stroke_path(badge_path, supplier.create_pen(t.border, 1.0f));

                // Draw the text inside the badge
                core::color<> label_col = t.text;
                canvas.draw_text(badge_text, font_title, supplier.create_brush(label_col),
                    core::vec2(grid_cx, badge_y + badge_h * 0.5f + t.font_size * 0.3f),
                    kDRAWBOT_TextAlignment_Center);
            }

            // Active mode coordinates
            float nx = (active_mode == 0)
                ? visual_sil_x
                : ((active_mode == 1) ? visual_hsv_x : visual_yolo_x);
            float ny = (active_mode == 0)
                ? visual_sil_y
                : ((active_mode == 1) ? visual_hsv_y : visual_yolo_y);

            float px = grid_cx + nx * (grid_w * 0.5f);
            float py = grid_cy + ny * (grid_h * 0.5f);

            float knob_r = 18.0f;

            // Sphere Shadow
            auto shadow_path = make_circle_path(px + 1.2f, py + 1.8f, knob_r);
            canvas.fill_path(shadow_path,
                supplier.create_brush(core::color<>(0.3f, 0.0f, 0.0f, 0.0f)));

            // 3D Sphere Highlight Shading
            for (int step = 0; step < 10; ++step) {
                float t_step = step / 9.0f;
                float r = knob_r * (1.0f - t_step * 0.7f);
                float ox = px - t_step * knob_r * 0.25f;
                float oy = py - t_step * knob_r * 0.25f;

                float val = 0.22f + t_step * 0.68f;
                core::color<> metal_col(1.0f, val, val, val);
                metal_col.red = metal_col.red * 0.82f + t.accent.red * 0.18f;
                metal_col.green = metal_col.green * 0.82f + t.accent.green * 0.18f;
                metal_col.blue = metal_col.blue * 0.82f + t.accent.blue * 0.18f;

                auto step_path = make_circle_path(ox, oy, r);
                canvas.fill_path(step_path, supplier.create_brush(metal_col));
            }

            // Border ring of knob
            auto knob_border_path = make_circle_path(px, py, knob_r);
            canvas.stroke_path(knob_border_path, supplier.create_pen(t.border, 1.2f));

            // Outer ring around the sphere
            float ring_r = knob_r + 4.0f;
            auto outer_ring_path = make_circle_path(px, py, ring_r);
            core::color<> ring_col = (pressed) ? t.handle_active : t.accent;
            ring_col.alpha = 0.5f;
            canvas.stroke_path(outer_ring_path, supplier.create_pen(ring_col, 1.2f));

            // Glowing dot
            float dot_angle = std::atan2(ny, nx);
            if (nx == 0.0f && ny == 0.0f) {
                dot_angle = -0.5f;
            }
            float dot_x = px + std::cos(dot_angle) * ring_r;
            float dot_y = py + std::sin(dot_angle) * ring_r;
            float dot_r = 3.5f;

            core::color<> glow_col = t.handle_active;
            glow_col.alpha = 0.25f;
            canvas.fill_path(make_circle_path(dot_x, dot_y, dot_r + 2.0f),
                supplier.create_brush(glow_col));

            glow_col.alpha = 1.0f;
            canvas.fill_path(
                make_circle_path(dot_x, dot_y, dot_r), supplier.create_brush(glow_col));
            canvas.fill_path(make_circle_path(dot_x, dot_y, 1.2f),
                supplier.create_brush(core::color<>(1.0f, 1.0f, 1.0f, 1.0f)));

        } else {
            // Centers and radius for three wells
            float well_y = bounds.y + bounds.h * 0.46f;
            float radius = m_opts.well_radius;

            float cx1 = bounds.x + bounds.w * 0.18f; // Silhouette
            float cx2 = bounds.x + bounds.w * 0.50f; // HSV
            float cx3 = bounds.x + bounds.w * 0.82f; // YOLO

            auto draw_well = [&](int well_idx, std::string label, float cx, float vx,
                                 float vy) {
                bool is_active = (active_mode < 0 || active_mode == well_idx);

                // Generate paths
                auto well_path = (m_opts.shape == joystick_shape::square)
                    ? make_square_path(cx, well_y, radius)
                    : make_circle_path(cx, well_y, radius);

                // Determine opacity-based colors
                core::color<> bg_well { 1.0f, 45.0f / 255.0f, 45.0f / 255.0f,
                    45.0f / 255.0f };
                core::color<> border_col { 1.0f, 70.0f / 255.0f, 70.0f / 255.0f,
                    70.0f / 255.0f };
                core::color<> axis_col { 0.25f, 70.0f / 255.0f, 70.0f / 255.0f,
                    70.0f / 255.0f };

                core::color<> label_col = t.text;
                core::color<> knob_color = t.accent;

                if (!is_active) {
                    // Dim colors if well is inactive
                    border_col.alpha *= 0.35f;
                    label_col.alpha *= 0.40f;
                    knob_color = border_col;
                    bg_well.alpha *= 0.50f;
                    axis_col.alpha *= 0.35f;
                } else {
                    knob_color = (pressed) ? t.handle_active : t.accent;
                }

                // 1. Fill base well
                canvas.fill_path(well_path, supplier.create_brush(bg_well));
                canvas.stroke_path(well_path, supplier.create_pen(border_col, 1.0f));

                // 2. Faded crosshairs inside the well
                auto axis_pen = supplier.create_pen(axis_col, 1.0f);
                auto h_path = supplier.create_path()
                                  .move_to(cx - radius, well_y)
                                  .line_to(cx + radius, well_y)
                                  .build();
                canvas.stroke_path(h_path, axis_pen);
                auto v_path = supplier.create_path()
                                  .move_to(cx, well_y - radius)
                                  .line_to(cx, well_y + radius)
                                  .build();
                canvas.stroke_path(v_path, axis_pen);

                // 3. Draw thumbstick knob
                float knob_r = 7.0f;
                float px = cx + vx * radius;
                float py = well_y + vy * radius;
                auto knob_path = make_circle_path(px, py, knob_r);

                // Knob shadow
                auto shadow_path = make_circle_path(px + 0.8f, py + 1.0f, knob_r);
                canvas.fill_path(shadow_path,
                    supplier.create_brush(core::color<>(0.25f, 0.0f, 0.0f, 0.0f)));
                // Knob main
                canvas.fill_path(knob_path, supplier.create_brush(knob_color));
                canvas.stroke_path(knob_path, supplier.create_pen(border_col, 1.0f));

                // Central Grip Concentric Circle
                auto grip_path = make_circle_path(px, py, 2.0f);
                canvas.stroke_path(grip_path,
                    supplier.create_pen(core::color<>(0.2f, 0.0f, 0.0f, 0.0f), 1.0f));

                // 4. Draw texts
                if (supplier.supports_text()) {
                    auto font_title = supplier.create_font(t.font_size);

                    // Mode name label
                    canvas.draw_text(label, font_title, supplier.create_brush(label_col),
                        core::vec2(cx, well_y - radius - 15.0f),
                        kDRAWBOT_TextAlignment_Center);
                }
            };

            // Draw three trackballs
            draw_well(0, m_opts.well_labels[0], cx1, visual_sil_x, visual_sil_y);
            draw_well(1, m_opts.well_labels[1], cx2, visual_hsv_x, visual_hsv_y);
            draw_well(2, m_opts.well_labels[2], cx3, visual_yolo_x, visual_yolo_y);
        }

        // Draw the text input fields below the wells
        update_input_texts();
        for (int i = 0; i < 3; ++i) {
            if (m_inputs[i][0])
                m_inputs[i][0]->paint(canvas, supplier, t);
            if (m_inputs[i][1])
                m_inputs[i][1]->paint(canvas, supplier, t);
        }
    }

    virtual bool on_click_impl(float lx, float ly, uint32_t mods) override {
        if (!enabled)
            return false;

        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 2; ++j) {
                if (m_inputs[i][j] && m_inputs[i][j]->visible && m_inputs[i][j]->hit_test(lx, ly)) {
                    return m_inputs[i][j]->on_click(lx, ly, mods);
                }
            }
        }

        if (active_mode >= 0 && active_mode < 3) {
            float margin_x = 4.0f;
            float margin_top = 0.0f;
            float margin_bottom = 0.0f;
            float grid_x = bounds.x + margin_x;
            float grid_y = bounds.y + margin_top;
            float grid_w = bounds.w - 2.0f * margin_x;
            float grid_h = bounds.h - margin_top - margin_bottom;

            if (lx >= grid_x - 5.0f && lx <= grid_x + grid_w + 5.0f && ly >= grid_y - 5.0f
                && ly <= grid_y + grid_h + 5.0f) {
                pressed = true;
                pressed_well = active_mode;
                m_last_mouse_x = lx;
                m_last_mouse_y = ly;

                float grid_cx = grid_x + grid_w * 0.5f;
                float grid_cy = grid_y + grid_h * 0.5f;

                if (mods & PF_Mod_CMD_CTRL_KEY) {
                    // Fine-tune: do not snap, initialize raw coords to current data
                    if (active_mode == 0) {
                        m_raw_sil_x = data.silhouette.x;
                        m_raw_sil_y = data.silhouette.y;
                    } else if (active_mode == 1) {
                        m_raw_hsv_x = data.hsv.x;
                        m_raw_hsv_y = data.hsv.y;
                    } else if (active_mode == 2) {
                        m_raw_yolo_x = data.yolo.x;
                        m_raw_yolo_y = data.yolo.y;
                    }
                } else {
                    update_large_well_from_point(
                        lx, ly, grid_cx, grid_cy, grid_w, grid_h, active_mode);
                }
                return true;
            }
            return false;
        }

        float well_y = bounds.y + bounds.h * 0.46f;
        float radius = m_opts.well_radius;

        float cx1 = bounds.x + bounds.w * 0.18f;
        float cx2 = bounds.x + bounds.w * 0.50f;
        float cx3 = bounds.x + bounds.w * 0.82f;

        auto test_click = [&](float cx, int well_idx) {
            float dx = lx - cx;
            float dy = ly - well_y;
            if (is_point_inside_well(dx, dy, radius, 5.0f)
                && (active_mode < 0 || active_mode == well_idx)) {
                pressed = true;
                pressed_well = well_idx;
                m_last_mouse_x = lx;
                m_last_mouse_y = ly;

                if (mods & PF_Mod_CMD_CTRL_KEY) {
                    // Fine-tune: do not snap
                    if (well_idx == 0) {
                        m_raw_sil_x = data.silhouette.x;
                        m_raw_sil_y = data.silhouette.y;
                    } else if (well_idx == 1) {
                        m_raw_hsv_x = data.hsv.x;
                        m_raw_hsv_y = data.hsv.y;
                    } else if (well_idx == 2) {
                        m_raw_yolo_x = data.yolo.x;
                        m_raw_yolo_y = data.yolo.y;
                    }
                } else {
                    update_well_from_point(lx, ly, cx, well_y, radius, well_idx);
                }
                return true;
            }
            return false;
        };

        if (test_click(cx1, 0))
            return true;
        if (test_click(cx2, 1))
            return true;
        if (test_click(cx3, 2))
            return true;

        return false;
    }

    virtual bool on_drag_impl(float lx, float ly, uint32_t mods) override {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 2; ++j) {
                if (m_inputs[i][j] && m_inputs[i][j]->is_focused()) {
                    return m_inputs[i][j]->on_drag(lx, ly, mods);
                }
            }
        }

        if (!pressed || pressed_well < 0)
            return false;
        if (active_mode >= 0 && pressed_well != active_mode)
            return false;

        float dlx = lx - m_last_mouse_x;
        float dly = ly - m_last_mouse_y;
        m_last_mouse_x = lx;
        m_last_mouse_y = ly;

        float speed_multiplier = (mods & PF_Mod_CMD_CTRL_KEY) ? 0.1f : 1.0f;

        if (active_mode >= 0 && active_mode < 3) {
            float margin_x = 4.0f;
            float margin_top = 0.0f;
            float margin_bottom = 0.0f;
            float grid_w = bounds.w - 2.0f * margin_x;
            float grid_h = bounds.h - margin_top - margin_bottom;

            float rx = grid_w * 0.5f;
            float ry = grid_h * 0.5f;

            if (rx <= 0.0f || ry <= 0.0f)
                return false;

            float dnx = (dlx / rx) * speed_multiplier;
            float dny = (dly / ry) * speed_multiplier;

            float& rx_ref = (pressed_well == 0)
                ? m_raw_sil_x
                : ((pressed_well == 1) ? m_raw_hsv_x : m_raw_yolo_x);
            float& ry_ref = (pressed_well == 0)
                ? m_raw_sil_y
                : ((pressed_well == 1) ? m_raw_hsv_y : m_raw_yolo_y);

            rx_ref += dnx;
            ry_ref += dny;

            clamp_normalized_position(rx_ref, ry_ref);

            float nx = rx_ref;
            float ny = ry_ref;
            apply_response_curve(nx, ny);

            if (pressed_well == 0) {
                data.silhouette.x = nx;
                data.silhouette.y = ny;
                visual_sil_x = nx;
                visual_sil_y = ny;
            } else if (pressed_well == 1) {
                data.hsv.x = nx;
                data.hsv.y = ny;
                visual_hsv_x = nx;
                visual_hsv_y = ny;
            } else if (pressed_well == 2) {
                data.yolo.x = nx;
                data.yolo.y = ny;
                visual_yolo_x = nx;
                visual_yolo_y = ny;
            }
            return true;
        }

        float well_y = bounds.y + bounds.h * 0.46f;
        float radius = m_opts.well_radius;
        if (radius <= 0.0f)
            return false;

        float dnx = (dlx / radius) * speed_multiplier;
        float dny = (dly / radius) * speed_multiplier;

        float& rx_ref = (pressed_well == 0)
            ? m_raw_sil_x
            : ((pressed_well == 1) ? m_raw_hsv_x : m_raw_yolo_x);
        float& ry_ref = (pressed_well == 0)
            ? m_raw_sil_y
            : ((pressed_well == 1) ? m_raw_hsv_y : m_raw_yolo_y);

        rx_ref += dnx;
        ry_ref += dny;

        clamp_normalized_position(rx_ref, ry_ref);

        float nx = rx_ref;
        float ny = ry_ref;
        apply_response_curve(nx, ny);

        if (pressed_well == 0) {
            data.silhouette.x = nx;
            data.silhouette.y = ny;
            visual_sil_x = nx;
            visual_sil_y = ny;
        } else if (pressed_well == 1) {
            data.hsv.x = nx;
            data.hsv.y = ny;
            visual_hsv_x = nx;
            visual_hsv_y = ny;
        } else if (pressed_well == 2) {
            data.yolo.x = nx;
            data.yolo.y = ny;
            visual_yolo_x = nx;
            visual_yolo_y = ny;
        }
        return true;
    }

    virtual void on_release_impl() override {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 2; ++j) {
                if (m_inputs[i][j])
                    m_inputs[i][j]->on_release();
            }
        }
        pressed = false;
        pressed_well = -1;
    }

    virtual bool on_key_impl(const interaction_context& ctx) override {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 2; ++j) {
                if (m_inputs[i][j] && m_inputs[i][j]->is_focused()) {
                    return m_inputs[i][j]->on_key(ctx);
                }
            }
        }
        return false;
    }

    virtual void sync_state_impl(const interaction_context& ctx) override {
        auto lock = ctx.arb_data<triple_joystick_data>(m_param_index);
        if (lock) {
            data = *lock;
            if (!pressed) {
                visual_sil_x = data.silhouette.x;
                visual_sil_y = data.silhouette.y;
                visual_hsv_x = data.hsv.x;
                visual_hsv_y = data.hsv.y;
                visual_yolo_x = data.yolo.x;
                visual_yolo_y = data.yolo.y;

                m_raw_sil_x = data.silhouette.x;
                m_raw_sil_y = data.silhouette.y;
                m_raw_hsv_x = data.hsv.x;
                m_raw_hsv_y = data.hsv.y;
                m_raw_yolo_x = data.yolo.x;
                m_raw_yolo_y = data.yolo.y;
            }
        }

        if (!m_mode_key.empty()) {
            auto mode_param = ctx.param<
                aetk::effect::arbitrary_param<aetk::effect::ui::slider_data<int>>>(
                m_mode_key);
            aetk::effect::locked_arbitrary<aetk::effect::ui::slider_data<int>> mode_lock(
                mode_param);
            if (mode_lock) {
                active_mode = mode_lock->value;
            }
        } else {
            active_mode = -1;
        }

        for (int i = 0; i < 3; ++i) {
            bool is_active = (active_mode < 0 || active_mode == i);
            if (m_inputs[i][0])
                m_inputs[i][0]->enabled = is_active;
            if (m_inputs[i][1])
                m_inputs[i][1]->enabled = is_active;
        }

        update_input_texts();
    }

    virtual void commit_state_impl(const interaction_context& ctx) override {
        auto lock = ctx.arb_data<triple_joystick_data>(m_param_index);
        if (lock && (*lock != data)) {
            *lock = data;
            ctx.mark_param_changed(m_param_index);
        }
    }

    virtual int32_t cursor_type_impl() const override {
        return PF_Cursor_FINGER_POINTER;
    }

    virtual void paint_title_impl(drawbot::canvas& canvas, drawbot::supplier& supplier,
        const theme& t, float x, float y, float w, float h) override {
        m_title_x = x;
        m_title_w = w;
        if (supplier.supports_text()) {
            if (active_mode >= 0 && active_mode < 3) {
                float nx = (active_mode == 0)
                    ? visual_sil_x
                    : ((active_mode == 1) ? visual_hsv_x : visual_yolo_x);
                float ny = (active_mode == 0)
                    ? visual_sil_y
                    : ((active_mode == 1) ? visual_hsv_y : visual_yolo_y);
                float px = m_opts.axis_ranges[active_mode][0].to_physical(nx);
                float py = m_opts.axis_ranges[active_mode][1].to_physical(ny);

                char x_buf[32] = { 0 };
                char y_buf[32] = { 0 };
                snprintf(x_buf, sizeof(x_buf), "%.1f", px);
                snprintf(y_buf, sizeof(y_buf), "%.1f", py);

                auto font = supplier.create_font(t.font_size * 0.85f);
                auto label_brush = supplier.create_brush(t.text_dim);

                core::color<> x_color = t.accent;
                core::color<> y_color = t.accent;

                if (m_title_drag_active) {
                    if (m_title_drag_axis == 0)
                        x_color = t.accent_pressed;
                    else
                        y_color = t.accent_pressed;
                } else {
                    if (m_title_hover_axis == 0)
                        x_color = t.handle_active;
                    else if (m_title_hover_axis == 1)
                        y_color = t.handle_active;
                }

                auto x_brush = supplier.create_brush(x_color);
                auto y_brush = supplier.create_brush(y_color);

                float ty = y + h * 0.5f + t.font_size * 0.28f;

                canvas.draw_text("X:", font, label_brush, core::vec2(x + 4.0f, ty),
                    kDRAWBOT_TextAlignment_Left);
                canvas.draw_text(x_buf, font, x_brush, core::vec2(x + 22.0f, ty),
                    kDRAWBOT_TextAlignment_Left);

                canvas.draw_text("Y:", font, label_brush, core::vec2(x + 76.0f, ty),
                    kDRAWBOT_TextAlignment_Left);
                canvas.draw_text(y_buf, font, y_brush, core::vec2(x + 94.0f, ty),
                    kDRAWBOT_TextAlignment_Left);
            } else {
                float px0 = m_opts.axis_ranges[0][0].to_physical(visual_sil_x);
                float py0 = m_opts.axis_ranges[0][1].to_physical(visual_sil_y);
                float px1 = m_opts.axis_ranges[1][0].to_physical(visual_hsv_x);
                float py1 = m_opts.axis_ranges[1][1].to_physical(visual_hsv_y);
                float px2 = m_opts.axis_ranges[2][0].to_physical(visual_yolo_x);
                float py2 = m_opts.axis_ranges[2][1].to_physical(visual_yolo_y);
                char buf[256] = { 0 };
                snprintf(buf, sizeof(buf), "A:(%.0f,%.0f) B:(%.0f,%.0f) C:(%.0f,%.0f)",
                    px0, py0, px1, py1, px2, py2);

                auto font = supplier.create_font(t.font_size * 0.85f);
                auto brush = supplier.create_brush(t.text_dim);
                float ty = y + h * 0.5f + t.font_size * 0.28f;
                canvas.draw_text(buf, font, brush, core::vec2(x + 4.0f, ty),
                    kDRAWBOT_TextAlignment_Left);
            }
        }
    }

    virtual bool hit_test_title(float lx, float ly) const override {
        if (active_mode < 0 || active_mode >= 3)
            return false;
        float dx = lx - m_title_x;
        if (dx >= 22.0f && dx <= 72.0f) {
            return true;
        }
        if (dx >= 94.0f && dx <= 144.0f) {
            return true;
        }
        return false;
    }

    virtual bool on_title_hover_move(float lx, float ly) override {
        if (active_mode < 0 || active_mode >= 3)
            return false;
        float dx = lx - m_title_x;
        int old_hover = m_title_hover_axis;
        if (dx >= 22.0f && dx <= 72.0f) {
            m_title_hover_axis = 0;
        } else if (dx >= 94.0f && dx <= 144.0f) {
            m_title_hover_axis = 1;
        } else {
            m_title_hover_axis = -1;
        }
        return m_title_hover_axis != old_hover;
    }

    virtual void on_hover_exit_impl() override {
        m_title_hover_axis = -1;
    }

    virtual bool on_title_click_impl(float lx, float ly, uint32_t mods) override {
        if (!enabled || active_mode < 0 || active_mode >= 3)
            return false;
        float dx = lx - m_title_x;
        if (dx >= 22.0f && dx <= 72.0f) {
            m_title_drag_axis = 0;
            m_title_drag_active = true;
            m_title_initial_x = lx;
            float nx = (active_mode == 0)
                ? visual_sil_x
                : ((active_mode == 1) ? visual_hsv_x : visual_yolo_x);
            m_title_initial_val = m_opts.axis_ranges[active_mode][0].to_physical(nx);
            return true;
        }
        if (dx >= 94.0f && dx <= 144.0f) {
            m_title_drag_axis = 1;
            m_title_drag_active = true;
            m_title_initial_x = lx;
            float ny = (active_mode == 0)
                ? visual_sil_y
                : ((active_mode == 1) ? visual_hsv_y : visual_yolo_y);
            m_title_initial_val = m_opts.axis_ranges[active_mode][1].to_physical(ny);
            return true;
        }
        return false;
    }

    virtual bool on_title_drag_impl(float lx, float ly, uint32_t mods) override {
        if (!m_title_drag_active || active_mode < 0 || active_mode >= 3)
            return false;

        float dx = lx - m_title_initial_x;
        const auto& range_info = m_opts.axis_ranges[active_mode][m_title_drag_axis];
        double range = static_cast<double>(range_info.max_val - range_info.min_val);
        if (range <= 0.0)
            range = 2.0;

        double speed = range / 300.0; // 300px for full sweep
        if (mods & PF_Mod_SHIFT_KEY) {
            speed *= 10.0;
        } else if (mods & PF_Mod_CMD_CTRL_KEY) {
            speed *= 0.1;
        }

        double phys_val = m_title_initial_val + dx * speed;
        phys_val = (std::clamp)(phys_val, static_cast<double>(range_info.min_val),
            static_cast<double>(range_info.max_val));

        float norm = range_info.to_normalized(static_cast<float>(phys_val));

        if (m_title_drag_axis == 0) {
            if (active_mode == 0) {
                data.silhouette.x = norm;
                visual_sil_x = norm;
            } else if (active_mode == 1) {
                data.hsv.x = norm;
                visual_hsv_x = norm;
            } else if (active_mode == 2) {
                data.yolo.x = norm;
                visual_yolo_x = norm;
            }
        } else {
            if (active_mode == 0) {
                data.silhouette.y = norm;
                visual_sil_y = norm;
            } else if (active_mode == 1) {
                data.hsv.y = norm;
                visual_hsv_y = norm;
            } else if (active_mode == 2) {
                data.yolo.y = norm;
                visual_yolo_y = norm;
            }
        }

        update_input_texts();
        return true;
    }

    virtual void on_title_release_impl() override {
        m_title_drag_active = false;
    }

    void update_input_texts() {
        auto format_value
            = [&](int well_idx, int axis_idx, float norm_val, text_input* input) {
                  if (!input || input->is_focused())
                      return;

                  const axis_mapping& mapping = m_opts.axis_ranges[well_idx][axis_idx];
                  float phys_val = mapping.to_physical(norm_val);
                  std::string formatted_str
                      = m_opts.axis_formatters[well_idx][axis_idx](phys_val);

                  if (active_mode >= 0) {
                      std::string axis_name = "";
                      const char* axis_letter = (axis_idx == 0) ? "x" : "y";
                      char val_buf[32];

                      if (well_idx == 0) {
                          axis_name = (axis_idx == 0) ? "Threshold" : "Area";
                      } else if (well_idx == 1) {
                          axis_name = (axis_idx == 0) ? "Hue" : "Sat/Val";
                      } else if (well_idx == 2) {
                          axis_name = (axis_idx == 0) ? "Max Detections" : "Min Area";
                      }
                      snprintf(val_buf, sizeof(val_buf), "%.0f", phys_val);

                      formatted_str = axis_name + " : " + axis_letter + " = " + val_buf;
                  }

                  input->set_text(formatted_str);
              };

        format_value(0, 0, visual_sil_x, m_inputs[0][0].get());
        format_value(0, 1, visual_sil_y, m_inputs[0][1].get());
        format_value(1, 0, visual_hsv_x, m_inputs[1][0].get());
        format_value(1, 1, visual_hsv_y, m_inputs[1][1].get());
        format_value(2, 0, visual_yolo_x, m_inputs[2][0].get());
        format_value(2, 1, visual_yolo_y, m_inputs[2][1].get());
    }

private:
    bool is_point_inside_well(
        float dx, float dy, float radius, float padding = 0.0f) const {
        if (m_opts.shape == joystick_shape::square) {
            return std::abs(dx) <= radius + padding && std::abs(dy) <= radius + padding;
        }

        float dist = std::sqrt(dx * dx + dy * dy);
        return dist <= radius + padding;
    }

    void clamp_normalized_position(float& nx, float& ny) const {
        if (m_opts.shape == joystick_shape::square) {
            nx = (std::clamp)(nx, -1.0f, 1.0f);
            ny = (std::clamp)(ny, -1.0f, 1.0f);
            return;
        }

        float len = std::sqrt(nx * nx + ny * ny);
        if (len > 1.0f) {
            nx /= len;
            ny /= len;
        }
    }

    void apply_response_curve(float& nx, float& ny) const {
        const float exponent = (std::clamp)(m_opts.response_exponent, 0.25f, 4.0f);
        if (std::abs(exponent - 1.0f) <= 0.001f) {
            return;
        }

        if (m_opts.shape == joystick_shape::square) {
            nx = std::copysign(std::pow(std::abs(nx), exponent), nx);
            ny = std::copysign(std::pow(std::abs(ny), exponent), ny);
            return;
        }

        const float len = std::sqrt(nx * nx + ny * ny);
        if (len <= 0.0001f) {
            return;
        }

        const float shaped_len = std::pow(len, exponent);
        const float scale = shaped_len / len;
        nx *= scale;
        ny *= scale;
    }

    void update_well_from_point(
        float lx, float ly, float cx, float cy, float r, int well_idx) {
        float nx = (lx - cx) / r;
        float ny = (ly - cy) / r;

        clamp_normalized_position(nx, ny);

        if (well_idx == 0) {
            m_raw_sil_x = nx;
            m_raw_sil_y = ny;
        } else if (well_idx == 1) {
            m_raw_hsv_x = nx;
            m_raw_hsv_y = ny;
        } else if (well_idx == 2) {
            m_raw_yolo_x = nx;
            m_raw_yolo_y = ny;
        }

        apply_response_curve(nx, ny);

        if (well_idx == 0) {
            data.silhouette.x = nx;
            data.silhouette.y = ny;
            visual_sil_x = nx;
            visual_sil_y = ny;
        } else if (well_idx == 1) {
            data.hsv.x = nx;
            data.hsv.y = ny;
            visual_hsv_x = nx;
            visual_hsv_y = ny;
        } else if (well_idx == 2) {
            data.yolo.x = nx;
            data.yolo.y = ny;
            visual_yolo_x = nx;
            visual_yolo_y = ny;
        }
    }

    void update_large_well_from_point(
        float lx, float ly, float cx, float cy, float w, float h, int well_idx) {
        float nx = (lx - cx) / (w * 0.5f);
        float ny = (ly - cy) / (h * 0.5f);

        clamp_normalized_position(nx, ny);

        if (well_idx == 0) {
            m_raw_sil_x = nx;
            m_raw_sil_y = ny;
        } else if (well_idx == 1) {
            m_raw_hsv_x = nx;
            m_raw_hsv_y = ny;
        } else if (well_idx == 2) {
            m_raw_yolo_x = nx;
            m_raw_yolo_y = ny;
        }

        apply_response_curve(nx, ny);

        if (well_idx == 0) {
            data.silhouette.x = nx;
            data.silhouette.y = ny;
            visual_sil_x = nx;
            visual_sil_y = ny;
        } else if (well_idx == 1) {
            data.hsv.x = nx;
            data.hsv.y = ny;
            visual_hsv_x = nx;
            visual_hsv_y = ny;
        } else if (well_idx == 2) {
            data.yolo.x = nx;
            data.yolo.y = ny;
            visual_yolo_x = nx;
            visual_yolo_y = ny;
        }
    }
};

} // namespace aetk::effect::ui
