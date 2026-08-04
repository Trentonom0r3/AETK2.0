#pragma once

#include <aetk/effect/ui/widget.hpp>
#include <aetk/effect/ui/curve_data.hpp>
#include <aetk/ui/theme.hpp>
#include <aetk/effect/context/context.hpp>
#include <aetk/effect/ui/widgets/button.hpp>
#include <aetk/effect/ui/widgets/curve_editor.hpp>

#include <memory>
#include <vector>
#include <string>

namespace aetk::effect::ui {

// ══════════════════════════════════════════════════════════════════════
//  Curve Group Widget (Multi-Curve Editor)
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief A multi-channel tabbed curve editor widget.
 * 
 * @details Aggregates multiple curve_editor instances underneath a horizontal row of square tab selectors, 
 * overlaying inactive channel splines at 30% opacity, while routing focus, click, and drag interactions 
 * recursively down to the selected active channel.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, building a multi-channel curve group (e.g. RGB Curves tabbed editor) requires writing massive, nested custom event handlers, hardcoding button positions, and managing active overlay states procedurally. `aetk::effect::ui::curve_group` shifts this paradigm to a unified OOP control. It aggregates multiple `curve_editor` instances underneath a horizontal row of square tab selectors, overlaying inactive channel splines at 30% opacity, while routing focus, click, and drag interactions recursively down to the selected active channel. Persisted states are synchronized with a sequence parameter arbitrary block (`multi_curve_data`) automatically.
 *
 * @warning <b>Memory & Lifecycles:</b> The curve group owns the unique pointers of the square selector tab buttons (`m_tabs`). All states are synchronized with persistent parameter sequences during `sync_state_impl` and `commit_state_impl` calls, preventing out-of-sync frames. Mismatched channel allocations are automatically resized and adjusted back to configured bounds during state syncs.
 */
class curve_group : public widget {
public:
    using data_type = multi_curve_data;

    /**
     * @brief Customizable builder configuration for tab height and padding.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Fluent layout configuration replacing raw procedural hardcoded size margins.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    struct options {
        struct channel_def {
            std::string name;
            curve_editor::options opts;
        };
        std::vector<channel_def> channels;

        float tab_height;
        float padding;

        /**
         * @brief options constructor.
         *
         * @note <b>AE SDK Paradigm Shift:</b> Sets default values for tab heights and padding.
         *
         * @warning <b>Memory & Lifecycles:</b> None.
         */
        options()
            : tab_height(24.0f),
              padding(4.0f) {}

        options& add_channel(std::string name, curve_editor::options opt = curve_editor::options()) {
            channels.push_back({std::move(name), opt});
            return *this;
        }

        options& set_tab_height(float h) { tab_height = h; return *this; }
        options& set_padding(float p) { padding = p; return *this; }
    };

    /**
     * @brief Fluent state defaults for setup.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Fluent state defaults.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param opt Custom styling options.
     * @return Bounded default multi curve data.
     */
    static data_type get_default_data(const options& opt = options()) {
        data_type d;
        for (const auto& ch : opt.channels) {
            d.channels.push_back(curve_editor::get_default_data(ch.opts));
        }
        return d;
    }

    /**
     * @brief Constructs a new multi-channel tabbed curve group editor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Modern NVI-patterned multi-channel curve widget with recursive sub-widget event routing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param param_idx Bounded parameter index.
     * @param opt Styling and channel configurations.
     */
    explicit curve_group(int32_t param_idx, options opt = options())
        : m_param_index(param_idx), m_opts(std::move(opt))
    {
        // Calculate max layout
        layout.min_height = m_opts.tab_height + m_opts.padding;
        float max_editor_height = 0.0f;

        for (size_t i = 0; i < m_opts.channels.size(); ++i) {
            const auto& ch = m_opts.channels[i];
            
            // Create tab button
            button::options btn_opts;
            btn_opts.set_toggle_mode(true)
                    .set_corner_radius(4.0f)
                    .set_shadow(true);
                    
            // Take just the first letter to fit the square button (e.g. "M", "R", "G", "B")
            // Or if they use an image, it will override.
            std::string label = ch.name.empty() ? "" : ch.name.substr(0, 1);
            auto btn = std::make_unique<button>(0, label, nullptr, btn_opts);
            m_tabs.push_back(std::move(btn));

            m_editors.emplace_back(0, ch.opts);

            if (ch.opts.height > max_editor_height) {
                max_editor_height = ch.opts.height;
            }
        }

        layout.min_height += max_editor_height;
        m_active_idx = 0;
    }

    // ── Widget Overrides ─────────────────────────────────────────

protected:
    /**
     * @brief Measures layout dimensions.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Dynamic size pass.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param avail_w Available width bounds.
     * @param avail_h Available height bounds.
     * @return Size vector.
     */
    core::vec2 measure_impl(float avail_w, float avail_h) override {
        bounds.w = avail_w;
        bounds.h = layout.min_height;
        
        // Layout tabs horizontally (Square buttons)
        if (!m_tabs.empty()) {
            float tab_size = m_opts.tab_height; // Square dimensions
            float cx = bounds.x;
            for (auto& btn : m_tabs) {
                btn->bounds = {cx, bounds.y, tab_size, tab_size};
                cx += tab_size + m_opts.padding;
            }
        }

        // Layout editor below tabs
        if (!m_editors.empty()) {
            m_editors[m_active_idx].bounds = {
                bounds.x, 
                bounds.y + m_opts.tab_height + m_opts.padding, 
                avail_w, 
                m_opts.channels[m_active_idx].opts.height
            };
        }

        return {avail_w, layout.min_height};
    }

    /**
     * @brief Paints tabs, background grids, inactive overlays, and selected active splines.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Recursive child drawing and inactive channel overlays.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param canvas Drawbot canvas.
     * @param supplier Drawbot supplier.
     * @param t Theme configurations.
     */
    void paint_impl(drawbot::canvas& canvas, drawbot::supplier& supplier, const theme& t) override {
        if (!visible) return;

        // Ensure proper sub-widget layout bounds
        measure(bounds.w, bounds.h);

        // Draw tabs
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            m_tabs[i]->set_active(i == m_active_idx);
            m_tabs[i]->paint(canvas, supplier, t);
        }

        // Overlay rendering for curves
        if (!m_editors.empty()) {
            auto& active_editor = m_editors[m_active_idx];
            float bx = active_editor.bounds.x, by = active_editor.bounds.y;
            float bw = active_editor.bounds.w, bh = active_editor.bounds.h;

            // Background
            canvas.fill_rect(bx, by, bw, bh, active_editor.opts().bg_color);

            // Grid
            if (active_editor.opts().show_grid) {
                auto grid_pen = supplier.create_pen(active_editor.opts().grid_color, 1.0f);
                for (int i = 1; i < active_editor.opts().grid_cols; ++i) {
                    float gx = bx + i * (bw / (float)active_editor.opts().grid_cols);
                    canvas.stroke_path(
                        supplier.create_path().move_to(gx, by).line_to(gx, by + bh).build(),
                        grid_pen);
                }
                for (int i = 1; i < active_editor.opts().grid_rows; ++i) {
                    float gy = by + i * (bh / (float)active_editor.opts().grid_rows);
                    canvas.stroke_path(
                        supplier.create_path().move_to(bx, gy).line_to(bx + bw, gy).build(),
                        grid_pen);
                }
            }

            // Draw Inactive curves (lines only, 30% opacity)
            for (size_t i = 0; i < m_editors.size(); ++i) {
                if (i == m_active_idx) continue;
                if (m_editors[i].cached_points().size() >= 2) {
                    m_editors[i].draw_curve(canvas, supplier, m_editors[i].cached_points(), bx, by, bw, bh, 0.3f);
                }
            }

            // Draw Active curve (full opacity + nodes)
            if (active_editor.cached_points().size() >= 2) {
                active_editor.draw_curve(canvas, supplier, active_editor.cached_points(), bx, by, bw, bh, 1.0f);
                active_editor.draw_nodes(canvas, supplier, active_editor.cached_points(), bx, by, bw, bh, active_editor.cached_dragging());
            }

            // Diagnostics
            if (active_editor.opts().show_diagnostics) {
                auto font = supplier.create_font(10.0f);
                char buf[128];
                sprintf(buf, "P:%zu DRG:%d", active_editor.cached_points().size(), active_editor.cached_dragging());
                auto text_brush = supplier.create_brush(core::color<>(0.6f, 0.6f, 0.6f));
                canvas.draw_text(buf, font, text_brush, core::vec2(bx + 5, by + bh - 5));
            }
        }
    }

    // ── State Sync ───────────────────────────────────────────────

    /**
     * @brief Syncs state parameters.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Recursive child synchronization.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    void sync_state_impl(const interaction_context& ctx) override {
        auto lock = ctx.arb_data<multi_curve_data>(m_param_index);
        if (lock) {
            // Validate data size matches our channel definitions
            if (lock->channels.size() != m_opts.channels.size()) {
                // If the user changed the plugin code, gracefully re-allocate
                lock->channels.resize(m_opts.channels.size());
                lock.mark_changed();
            }

            // Feed data to active editor
            if (!m_editors.empty() && m_active_idx < lock->channels.size()) {
                m_editors[m_active_idx].set_data(lock->channels[m_active_idx]);
            }
        }
    }

    /**
     * @brief Commits state changes.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Recursive state commit.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    void commit_state_impl(const interaction_context& ctx) override {
        auto lock = ctx.arb_data<multi_curve_data>(m_param_index);
        if (!lock) return;

        bool changed = false;

        // Commit curve data changes from the editor
        if (!m_editors.empty() && m_active_idx < lock->channels.size()) {
            // Apply any pending UI interactions from the active editor into the actual data
            m_editors[m_active_idx].apply_to(lock->channels[m_active_idx]);
            changed = true;
        }

        if (changed) {
            lock.mark_changed();
            ctx.mark_param_changed(m_param_index);
        }
    }

    // ── Interaction Routing ──────────────────────────────────────

    /**
     * @brief Click handler routing down to selected active tabs or curves.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Event coordinate intersection and recursive tab routing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param x Horizontal relative coordinate.
     * @param y Vertical relative coordinate.
     * @param mods Modifiers.
     * @return True if click registered.
     */
    bool on_click_impl(float x, float y, uint32_t mods) override {
        if (!enabled || !visible) return false;

        // Route to tabs
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            if (m_tabs[i]->bounds.contains(x, y)) {
                if (m_tabs[i]->on_click(x, y, mods)) {
                    m_active_idx = (int)i; // Switch active channel
                    return true;
                }
            }
        }

        // Route to active editor
        if (!m_editors.empty() && m_editors[m_active_idx].bounds.contains(x, y)) {
            return m_editors[m_active_idx].on_click(x, y, mods);
        }

        return false;
    }

    /**
     * @brief Drag handler routing to the active tab editor spline.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Recursive drag routing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param x Horizontal relative coordinate.
     * @param y Vertical relative coordinate.
     * @param mods Modifiers.
     * @return True if drag registered.
     */
    bool on_drag_impl(float x, float y, uint32_t mods) override {
        if (!enabled || !visible) return false;

        // Tabs don't typically support dragging.
        // Route to active editor
        if (!m_editors.empty()) {
            return m_editors[m_active_idx].on_drag(x, y, mods);
        }
        return false;
    }

    /**
     * @brief Release handler routing down.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Recursive release routing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void on_release_impl() override {
        for (auto& btn : m_tabs) btn->on_release();
        if (!m_editors.empty()) m_editors[m_active_idx].on_release();
    }

    /**
     * @brief Inspects custom cursor type on hover bounds.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Empty cursor routing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Cursor type value.
     */
    int32_t cursor_type_impl() const override {
        // Not currently propagating cursor types from sub-widgets cleanly,
        // but can be added if needed.
        return 0; 
    }

private:
    int32_t m_param_index;
    options m_opts;
    int m_active_idx = 0;

    std::vector<std::unique_ptr<button>> m_tabs;
    std::vector<curve_editor> m_editors;
};

} // namespace aetk::effect::ui
