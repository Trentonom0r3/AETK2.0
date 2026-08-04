#pragma once

#include <aetk/effect/ui/widget.hpp>
#include <aetk/effect/ui/widgets/slider_data.hpp>
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace aetk::effect::ui {

/**
 * @brief Segment Selector Widget (Cassette-style buttons).
 *
 * @details A horizontal segmented button group representing a single integer
 * selection state.
 */
class segment_selector : public widget {
public:
    using data_type = slider_data<int>;

    static data_type get_default_data(
        std::vector<std::string> options, int default_val = 0) {
        data_type d;
        d.value = default_val;
        return d;
    }

    int m_param_index;
    std::vector<std::string> m_options;
    int m_selected;
    std::function<void(int)> m_on_change;

    segment_selector(int32_t param_idx, std::vector<std::string> options)
        : segment_selector(param_idx, std::move(options), 0, nullptr) {
    }

    segment_selector(int32_t param_idx, std::vector<std::string> options, int default_val)
        : segment_selector(param_idx, std::move(options), default_val, nullptr) {
    }

    segment_selector(int32_t param_idx, std::vector<std::string> options, int default_val,
        std::function<void(int)> on_change)
        : m_param_index(param_idx)
        , m_options(std::move(options))
        , m_selected(default_val)
        , m_on_change(std::move(on_change)) {
        layout.min_height = 20.0f;
        layout.min_width = 240.0f;
    }

    virtual bool is_title_only() const override {
        return false;
    }

protected:
    virtual aetk::core::vec2 measure_impl(float avail_w, float) override {
        return { avail_w, layout.min_height };
    }

    void draw_segments(drawbot::canvas& canvas, drawbot::supplier& supplier,
        const theme& t, float x, float y, float w, float h, bool is_title) {
        int num_segments = (int)m_options.size();
        if (num_segments == 0)
            return;

        float pad_y = is_title ? 1.0f : 0.0f;
        float draw_y = y + pad_y;
        float draw_h = h - 2.0f * pad_y;

        float segment_w = w / num_segments;

        // Path generator for rounded corner bounds
        auto make_round_rect_path = [&](float px, float py, float pw, float ph,
                                        float pr_tl, float pr_tr, float pr_bl,
                                        float pr_br) {
            float k_tl = pr_tl * 0.55228475f;
            float k_tr = pr_tr * 0.55228475f;
            float k_bl = pr_bl * 0.55228475f;
            float k_br = pr_br * 0.55228475f;
            return supplier.create_path()
                .move_to(px + pr_tl, py)
                .line_to(px + pw - pr_tr, py)
                .bezier_to({ px + pw - pr_tr + k_tr, py }, { px + pw, py + pr_tr - k_tr },
                    { px + pw, py + pr_tr })
                .line_to(px + pw, py + ph - pr_br)
                .bezier_to({ px + pw, py + ph - pr_br + k_br },
                    { px + pw - pr_br + k_br, py + ph }, { px + pw - pr_br, py + ph })
                .line_to(px + pr_bl, py + ph)
                .bezier_to({ px + pr_bl - k_bl, py + ph }, { px, py + ph - pr_bl + k_bl },
                    { px, py + ph - pr_bl })
                .line_to(px, py + pr_tl)
                .bezier_to({ px, py + pr_tl - k_tl }, { px + pr_tl - k_tl, py },
                    { px + pr_tl, py })
                .build();
        };

        for (int i = 0; i < num_segments; ++i) {
            float seg_x = x + i * segment_w;
            float seg_w = (i == num_segments - 1) ? (x + w - seg_x) : segment_w;

            float tl = (i == 0) ? (is_title ? 3.0f : 4.0f) : 0.0f;
            float bl = (i == 0) ? (is_title ? 3.0f : 4.0f) : 0.0f;
            float tr = (i == num_segments - 1) ? (is_title ? 3.0f : 4.0f) : 0.0f;
            float br = (i == num_segments - 1) ? (is_title ? 3.0f : 4.0f) : 0.0f;

            core::color<> fill;
            bool is_active = (i == m_selected);
            bool is_hovered = hovered
                && (is_title ? get_segment_at_rect(m_last_lx, m_title_bounds) == i
                             : get_segment_at(m_last_lx) == i);

            if (is_active) {
                fill = t.accent;
            } else if (is_hovered) {
                fill
                    = core::color<>(1.0f, 80.0f / 255.0f, 80.0f / 255.0f, 80.0f / 255.0f);
            } else {
                fill
                    = core::color<>(1.0f, 51.0f / 255.0f, 51.0f / 255.0f, 51.0f / 255.0f);
            }

            auto path
                = make_round_rect_path(seg_x, draw_y, seg_w, draw_h, tl, tr, bl, br);
            canvas.fill_path(path, supplier.create_brush(fill));

            auto border_pen = supplier.create_pen(
                core::color<>(1.0f, 25.0f / 255.0f, 25.0f / 255.0f, 25.0f / 255.0f),
                1.0f);
            canvas.stroke_path(path, border_pen);

            float text_len = (float)m_options[i].length();
            float max_text_w = seg_w - 4.0f;
            float active_font_size = t.font_size * (is_title ? 0.95f : 1.0f);
            if (text_len > 0.0f) {
                float est_text_w = text_len * active_font_size * 0.70f;
                if (est_text_w > max_text_w) {
                    active_font_size = (std::max)(5.0f, max_text_w / (text_len * 0.70f));
                }
            }

            auto font = supplier.create_font(active_font_size);

            core::color<> text_c = is_active
                ? core::color<>(1.0f, 1.0f, 1.0f, 1.0f)
                : (is_hovered ? core::color<>(1.0f, 0.85f, 0.85f, 0.85f)
                              : core::color<>(1.0f, 0.5f, 0.5f, 0.5f));

            auto brush = supplier.create_brush(text_c);

            float cx = seg_x + seg_w * 0.5f;
            float cy = draw_y + draw_h * 0.5f + active_font_size * 0.3f;

            if (is_active) {
                auto shadow_brush
                    = supplier.create_brush(core::color<>(0.5f, 0.0f, 0.0f, 0.0f));
                canvas.draw_text(m_options[i], font, shadow_brush,
                    core::vec2(cx, cy + 1.0f), kDRAWBOT_TextAlignment_Center);
            }

            canvas.draw_text(m_options[i], font, brush, core::vec2(cx, cy),
                kDRAWBOT_TextAlignment_Center);
        }
    }

    virtual void paint_impl(
        drawbot::canvas& canvas, drawbot::supplier& supplier, const theme& t) override {
        draw_segments(canvas, supplier, t, bounds.x, bounds.y, bounds.w, bounds.h, false);
    }

    virtual bool on_click_impl(float lx, float ly, uint32_t mods) override {
        if (!enabled)
            return false;
        m_last_lx = lx;
        int clicked = get_segment_at(lx);
        if (clicked >= 0 && clicked < (int)m_options.size()) {
            if (clicked != m_selected) {
                m_selected = clicked;
                if (m_on_change)
                    m_on_change(m_selected);
            }
            return true;
        }
        return false;
    }

    virtual bool on_drag_impl(float lx, float ly, uint32_t mods) override {
        return on_click_impl(lx, ly, mods);
    }

    virtual void sync_state_impl(const interaction_context& ctx) override {
        auto lock = ctx.arb_data<slider_data<int>>(m_param_index);
        if (lock) {
            m_selected = lock->value;
        }
    }

    virtual void commit_state_impl(const interaction_context& ctx) override {
        auto lock = ctx.arb_data<slider_data<int>>(m_param_index);
        if (lock && lock->value != m_selected) {
            lock->value = m_selected;
            ctx.mark_param_changed(m_param_index);
        }
    }

    virtual bool on_hover_move_impl(float lx, float ly) override {
        int old_hover_seg = get_segment_at(m_last_lx);
        m_last_lx = lx;
        int new_hover_seg = get_segment_at(lx);
        return (old_hover_seg != new_hover_seg);
    }

    virtual void on_hover_exit_impl() override {
        m_last_lx = -9999.0f;
        m_last_title_lx = -9999.0f;
    }

    virtual void paint_title_impl(drawbot::canvas& canvas, drawbot::supplier& supplier,
        const theme& t, float x, float y, float w, float h) override {
        if (m_selected < 0 || m_selected >= (int)m_options.size())
            return;

        std::string selected_label = m_options[m_selected];

        // Font setup
        float font_size = t.font_size * 0.9f;
        auto font = supplier.create_font(font_size);

        // Estimate character width
        float char_w = font_size * 0.70f;

        // 1. Find the maximum option text width to keep size constant across all options
        float max_text_w = 0.0f;
        size_t longest_len = 1;
        for (const auto& opt : m_options) {
            float opt_w = opt.length() * char_w;
            if (opt_w > max_text_w) {
                max_text_w = opt_w;
            }
            if (opt.length() > longest_len) {
                longest_len = opt.length();
            }
        }

        // 2. Structure layout: Left Arrow (16px) + Gap (4px) + Pill (100px fixed width) +
        // Gap (4px) + Right Arrow (16px)
        float pad_x = 8.0f;
        float pad_y = 2.0f;
        float arrow_area_w = 40.0f; // (16px + 4px gap) * 2

        float pill_w = 100.0f;
        float total_w = pill_w + arrow_area_w;

        // Position it left-aligned starting at the offset
        float badge_x = x + 4.0f;
        float badge_y = y + pad_y;
        float badge_h = h - 2.0f * pad_y;

        // 3. Clamp total width to fit in available space (w - 8.0f) and scale down font
        // if needed
        float max_total_w = w - 8.0f;
        if (total_w > max_total_w) {
            total_w = max_total_w;
            pill_w = total_w - arrow_area_w;
            if (pill_w < 16.0f)
                pill_w = 16.0f;

            // Recalculate font size to fit the longest text inside the shrunk pill
            float available_text_space = pill_w - pad_x * 2.0f;
            if (available_text_space < 8.0f)
                available_text_space = 8.0f;

            font_size = available_text_space / (longest_len * 0.70f);
            font_size = (std::max)(6.5f, (std::min)(font_size, t.font_size * 0.9f));
            font = supplier.create_font(font_size);
        }

        // Cache title bounds for hit testing
        m_title_bounds = { badge_x, badge_y, total_w, badge_h };

        // 4. Coordinates for triangles and pill
        float l_cx = badge_x + 8.0f;
        float l_cy = badge_y + badge_h * 0.5f;

        float pill_x = badge_x + 20.0f;
        float pill_y = badge_y;
        float pill_h = badge_h;

        float r_cx = pill_x + pill_w + 12.0f;
        float r_cy = badge_y + badge_h * 0.5f;

        // Highlight based on mouse hover position
        bool left_hovered = hovered && (m_last_title_lx < pill_x);
        bool right_hovered = hovered && (m_last_title_lx > pill_x + pill_w);
        bool pill_hovered = hovered && !left_hovered && !right_hovered;

        // Draw Left Triangle (◀)
        auto left_path = supplier.create_path()
                             .move_to(l_cx - 3.5f, l_cy)
                             .line_to(l_cx + 2.5f, l_cy - 4.0f)
                             .line_to(l_cx + 2.5f, l_cy + 4.0f)
                             .close()
                             .build();
        core::color<> left_color = left_hovered
            ? core::color<>(1.0f, 1.0f, 1.0f, 1.0f)
            : core::color<>(1.0f, 150.0f / 255.0f, 150.0f / 255.0f, 150.0f / 255.0f);
        canvas.fill_path(left_path, supplier.create_brush(left_color));

        // Draw Right Triangle (▶)
        auto right_path = supplier.create_path()
                              .move_to(r_cx + 3.5f, r_cy)
                              .line_to(r_cx - 2.5f, r_cy - 4.0f)
                              .line_to(r_cx - 2.5f, r_cy + 4.0f)
                              .close()
                              .build();
        core::color<> right_color = right_hovered
            ? core::color<>(1.0f, 1.0f, 1.0f, 1.0f)
            : core::color<>(1.0f, 150.0f / 255.0f, 150.0f / 255.0f, 150.0f / 255.0f);
        canvas.fill_path(right_path, supplier.create_brush(right_color));

        // Draw background pill: rounded rect path
        float pr = 3.0f; // rounded radius
        float pk = pr * 0.55228475f;
        auto pill_path = supplier.create_path()
                             .move_to(pill_x + pr, pill_y)
                             .line_to(pill_x + pill_w - pr, pill_y)
                             .bezier_to({ pill_x + pill_w - pr + pk, pill_y },
                                 { pill_x + pill_w, pill_y + pr - pk },
                                 { pill_x + pill_w, pill_y + pr })
                             .line_to(pill_x + pill_w, pill_y + pill_h - pr)
                             .bezier_to({ pill_x + pill_w, pill_y + pill_h - pr + pk },
                                 { pill_x + pill_w - pr + pk, pill_y + pill_h },
                                 { pill_x + pill_w - pr, pill_y + pill_h })
                             .line_to(pill_x + pr, pill_y + pill_h)
                             .bezier_to({ pill_x + pr - pk, pill_y + pill_h },
                                 { pill_x, pill_y + pill_h - pr + pk },
                                 { pill_x, pill_y + pill_h - pr })
                             .line_to(pill_x, pill_y + pr)
                             .bezier_to({ pill_x, pill_y + pr - pk },
                                 { pill_x + pr - pk, pill_y }, { pill_x + pr, pill_y })
                             .build();

        // Fill color: lighter bg when pill is hovered
        core::color<> bg_color = pill_hovered
            ? core::color<>(1.0f, 95.0f / 255.0f, 95.0f / 255.0f, 95.0f / 255.0f)
            : core::color<>(1.0f, 65.0f / 255.0f, 65.0f / 255.0f, 65.0f / 255.0f);
        canvas.fill_path(pill_path, supplier.create_brush(bg_color));

        // Border color: accent color when hovered, otherwise clean subtle border
        core::color<> border_color = pill_hovered
            ? t.accent
            : core::color<>(1.0f, 90.0f / 255.0f, 90.0f / 255.0f, 90.0f / 255.0f);
        canvas.stroke_path(pill_path, supplier.create_pen(border_color, 1.0f));

        // Draw text: centered inside the pill
        core::color<> text_color = core::color<>(1.0f, 1.0f, 1.0f, 1.0f);
        auto text_brush = supplier.create_brush(text_color);

        float cx = pill_x + pill_w * 0.5f;
        float cy = pill_y + pill_h * 0.5f + font_size * 0.3f;
        canvas.draw_text(selected_label, font, text_brush, core::vec2(cx, cy),
            kDRAWBOT_TextAlignment_Center);
    }

    virtual bool on_title_click_impl(float lx, float ly, uint32_t mods) override {
        if (!m_title_bounds.contains(lx, ly))
            return false;

        int num_options = (int)m_options.size();
        if (num_options <= 1)
            return false;

        // Pill starting x-coord
        float pill_x = m_title_bounds.x + 20.0f;
        float pill_w = m_title_bounds.w - 40.0f;

        // Click left of pill -> cycle backward, click on pill or right -> cycle forward
        if (lx < pill_x) {
            m_selected = (m_selected + num_options - 1) % num_options;
        } else {
            m_selected = (m_selected + 1) % num_options;
        }

        if (m_on_change) {
            m_on_change(m_selected);
        }
        return true;
    }

    virtual bool on_title_drag_impl(float lx, float ly, uint32_t mods) override {
        return false;
    }

    virtual bool hit_test_title(float lx, float ly) const override {
        return m_title_bounds.contains(lx, ly);
    }

    virtual bool on_title_hover_move(float lx, float ly) override {
        float pill_x = m_title_bounds.x + 20.0f;
        float pill_w = m_title_bounds.w - 40.0f;

        bool old_left = (m_last_title_lx < pill_x);
        bool old_right = (m_last_title_lx > pill_x + pill_w);

        m_last_title_lx = lx;
        m_last_title_ly = ly;

        bool new_left = (lx < pill_x);
        bool new_right = (lx > pill_x + pill_w);

        return (old_left != new_left || old_right != new_right || !hovered);
    }

    virtual int32_t cursor_type_impl() const override {
        return PF_Cursor_FINGER_POINTER;
    }

private:
    float m_last_lx = -9999.0f;
    float m_last_title_lx = -9999.0f;
    float m_last_title_ly = -9999.0f;
    bounds_rect m_title_bounds = { 0.0f, 0.0f, 0.0f, 0.0f };

    int get_segment_at_rect(float lx, const bounds_rect& r) const {
        if (r.w <= 0.0f)
            return -1;
        if (lx < r.x || lx > r.x + r.w)
            return -1;
        float local_x = lx - r.x;
        int idx = (int)(local_x / (r.w / m_options.size()));
        return std::clamp(idx, 0, (int)m_options.size() - 1);
    }

    int get_segment_at(float lx) const {
        return get_segment_at_rect(lx, bounds);
    }
};

} // namespace aetk::effect::ui
