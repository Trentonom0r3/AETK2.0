#pragma once

#include <aetk/effect/ui/widget.hpp>
#include <algorithm>

namespace aetk::effect::ui {

// ══════════════════════════════════════════════════════════════════════
//  vstack — Vertical flexbox layout container
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Lays children out vertically (top to bottom).
 * 
 * @details Fixed-size children (flex == 0) get their measured height.
 * Remaining space is distributed among flex > 0 children proportionally.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, laying out custom UI elements requires manual hardcoded absolute coordinate computation, rendering them fragile to interface resizing or custom DPI scalings. `aetk::effect::ui::vstack`, `hstack`, and `spacer` provide modern, declarative flexbox-inspired layout containers. Instead of absolute positions, they implement two-pass dimension measurements, distributing remaining flex space proportionally across visible children.
 *
 * @warning <b>Memory & Lifecycles:</b> Flex layouts are container widgets that manage child references. They do not acquire exclusive pointer ownership themselves, delegating memory lifecycles to their parent panels or views.
 */
class vstack : public container {
public:
    /// Spacing gap between children in pixels.
    float gap = 4.0f;

    /**
     * @brief vstack constructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Vertical gap sizing setup.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param gap_px Spacing gap between children in pixels.
     */
    explicit vstack(float gap_px = 4.0f) : gap(gap_px) {}

protected:
    /**
     * @brief Measures layout dimension bounds.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Dynamic size pass.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param avail_w Available width bounds.
     * @param avail_h Available height bounds.
     * @return Bounded size vector.
     */
    core::vec2 measure_impl(float avail_w, float avail_h) override {
        float total_h = 0;
        float max_w = 0;
        int visible_count = 0;

        for (auto& child : children) {
            if (!child || !child->visible) continue;
            auto sz = child->measure(avail_w, avail_h);
            float child_h = (std::max)((float)sz.y, child->layout.min_height);
            if (child->layout.max_height > 0.0f) {
                child_h = (std::min)(child_h, child->layout.max_height);
            }
            total_h += child_h;

            float child_w = (std::max)((float)sz.x, child->layout.min_width);
            if (child->layout.max_width > 0.0f) {
                child_w = (std::min)(child_w, child->layout.max_width);
            }
            max_w = (std::max)(max_w, child_w);
            visible_count++;
        }

        if (visible_count > 1) {
            total_h += gap * (visible_count - 1);
        }

        return { max_w, total_h };
    }

    /**
     * @brief Performs vertical layout alignments.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Two-pass flex spacing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param x Layout horizontal coordinate.
     * @param y Layout vertical coordinate.
     * @param w Layout width dimension.
     * @param h Layout height dimension.
     */
    void do_layout_impl(float x, float y, float w, float h) override {
        // Pass 1: Measure fixed children, sum flex factors
        float fixed_total = 0;
        float flex_total = 0;
        int visible_count = 0;

        for (auto& child : children) {
            if (!child || !child->visible) continue;
            if (child->layout.flex <= 0) {
                auto sz = child->measure(w, h);
                fixed_total += (std::clamp)((float)sz.y, child->layout.min_height, child->layout.max_height);
            } else {
                flex_total += child->layout.flex;
            }
            visible_count++;
        }

        float gap_total = (visible_count > 1) ? gap * (visible_count - 1) : 0;
        float remaining = (std::max)(0.0f, h - fixed_total - gap_total);

        // Pass 2: Assign positions
        float cursor_y = y;
        for (auto& child : children) {
            if (!child || !child->visible) continue;

            float child_h;
            if (child->layout.flex > 0 && flex_total > 0) {
                child_h = remaining * (child->layout.flex / flex_total);
            } else {
                auto sz = child->measure(w, h);
                child_h = (std::clamp)((float)sz.y, child->layout.min_height, child->layout.max_height);
            }

            child_h = (std::clamp)(child_h, child->layout.min_height, child->layout.max_height);

            float child_w = w;
            if (child->layout.max_width > 0.0f) {
                child_w = (std::min)(child_w, child->layout.max_width);
            }
            child_w = (std::max)(child_w, child->layout.min_width);

            child->do_layout(x, cursor_y, child_w, child_h);
            cursor_y += child_h + gap;
        }
    }
};

// ══════════════════════════════════════════════════════════════════════
//  hstack — Horizontal flexbox layout container
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Lays children out horizontally (left to right).
 * 
 * @details Same flex distribution logic as vstack, but on the X axis.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, laying out custom UI elements requires manual hardcoded absolute coordinate computation, rendering them fragile to interface resizing or custom DPI scalings. `aetk::effect::ui::vstack`, `hstack`, and `spacer` provide modern, declarative flexbox-inspired layout containers. Instead of absolute positions, they implement two-pass dimension measurements, distributing remaining flex space proportionally across visible children.
 *
 * @warning <b>Memory & Lifecycles:</b> Flex layouts are container widgets that manage child references. They do not acquire exclusive pointer ownership themselves, delegating memory lifecycles to their parent panels or views.
 */
class hstack : public container {
public:
    /// Spacing gap between children in pixels.
    float gap = 4.0f;

    /**
     * @brief hstack constructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Horizontal gap sizing setup.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param gap_px Spacing gap between children in pixels.
     */
    explicit hstack(float gap_px = 4.0f) : gap(gap_px) {}

protected:
    /**
     * @brief Measures layout dimension bounds.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Dynamic size pass.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param avail_w Available width bounds.
     * @param avail_h Available height bounds.
     * @return Bounded size vector.
     */
    core::vec2 measure_impl(float avail_w, float avail_h) override {
        float total_w = 0;
        float max_h = 0;
        int visible_count = 0;

        for (auto& child : children) {
            if (!child || !child->visible) continue;
            auto sz = child->measure(avail_w, avail_h);
            float child_w = (std::max)((float)sz.x, child->layout.min_width);
            if (child->layout.max_width > 0.0f) {
                child_w = (std::min)(child_w, child->layout.max_width);
            }
            total_w += child_w;

            float child_h = (std::max)((float)sz.y, child->layout.min_height);
            if (child->layout.max_height > 0.0f) {
                child_h = (std::min)(child_h, child->layout.max_height);
            }
            max_h = (std::max)(max_h, child_h);
            visible_count++;
        }

        if (visible_count > 1) {
            total_w += gap * (visible_count - 1);
        }

        return { total_w, max_h };
    }

    /**
     * @brief Performs horizontal layout alignments.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Two-pass flex spacing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param x Layout horizontal coordinate.
     * @param y Layout vertical coordinate.
     * @param w Layout width dimension.
     * @param h Layout height dimension.
     */
    void do_layout_impl(float x, float y, float w, float h) override {
        float fixed_total = 0;
        float flex_total = 0;
        int visible_count = 0;

        for (auto& child : children) {
            if (!child || !child->visible) continue;
            if (child->layout.flex <= 0) {
                auto sz = child->measure(w, h);
                fixed_total += (std::clamp)((float)sz.x, child->layout.min_width, child->layout.max_width);
            } else {
                flex_total += child->layout.flex;
            }
            visible_count++;
        }

        float gap_total = (visible_count > 1) ? gap * (visible_count - 1) : 0;
        float remaining = (std::max)(0.0f, w - fixed_total - gap_total);

        float cursor_x = x;
        for (auto& child : children) {
            if (!child || !child->visible) continue;

            float child_w;
            if (child->layout.flex > 0 && flex_total > 0) {
                child_w = remaining * (child->layout.flex / flex_total);
            } else {
                auto sz = child->measure(w, h);
                child_w = (std::clamp)((float)sz.x, child->layout.min_width, child->layout.max_width);
            }

            child_w = (std::clamp)(child_w, child->layout.min_width, child->layout.max_width);

            float child_h = h;
            if (child->layout.max_height > 0.0f) {
                child_h = (std::min)(child_h, child->layout.max_height);
            }
            child_h = (std::max)(child_h, child->layout.min_height);

            child->do_layout(cursor_x, y, child_w, child_h);
            cursor_x += child_w + gap;
        }
    }
};

// ══════════════════════════════════════════════════════════════════════
//  spacer — Invisible flex-absorbing widget
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief An invisible widget that absorbs remaining flex space.
 * 
 * @details Usage: stack->emplace<spacer>() between fixed-size widgets
 * to push them apart (like CSS justify-content: space-between).
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, laying out custom UI elements requires manual hardcoded absolute coordinate computation, rendering them fragile to interface resizing or custom DPI scalings. `aetk::effect::ui::vstack`, `hstack`, and `spacer` provide modern, declarative flexbox-inspired layout containers. Instead of absolute positions, they implement two-pass dimension measurements, distributing remaining flex space proportionally across visible children.
 *
 * @warning <b>Memory & Lifecycles:</b> Spacers are container child widgets. They do not own or allocate active memories.
 */
class spacer : public widget {
public:
    /**
     * @brief spacer constructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Set flex to 1 by default.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    spacer() { layout.flex = 1; }

protected:
    /**
     * @brief Invisible layout size helper.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Zero size metrics.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Zero vector.
     */
    core::vec2 measure_impl(float, float) override { return { 0, 0 }; }

    /**
     * @brief Invisible painting helper.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Empty canvas override.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void paint_impl(drawbot::canvas&, drawbot::supplier&, const theme&) override {
        // Intentionally empty — spacers are invisible
    }
};

} // namespace aetk::effect::ui
