#pragma once

#include <aetk/effect/ui/layout.hpp>
#include <aetk/effect/ui/widgets/accordion_data.hpp>
#include <aetk/ui/theme.hpp>

namespace aetk::effect::ui {

/**
 * @brief A collapsible container with a header row.
 * 
 * @details Inherits from vstack but intercepts measure and layout to hide children
 * when collapsed. State is persisted via a hidden arbitrary parameter.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, creating interactive, collapsible container menus (Accordions) requires complex layout logic, manually tracking screen-space mouse coordinates during clicks, drawing custom path vector triangles via PICA Drawbot, and managing persistent show/hide variables across plugin frames. `aetk::effect::ui::accordion` automates this collapsibility by overriding standard flexbox `measure` and `layout` steps. It intercepts coordinates, lazily hides child components when collapsed, renders custom vector expand arrows on-the-fly, and synchronizes its state automatically with a persistent parameter arbitrary block (`accordion_data`) so that collapsed groups persist across project saves.
 *
 * @warning <b>Memory & Lifecycles:</b> The accordion inherits from `vstack`, managing dynamic child widgets using `std::unique_ptr` arrays. State values (`expanded`) are automatically marshaled to and from After Effects parameter sequences via `sync_state_impl` and `commit_state_impl` calls.
 */
class accordion : public vstack {
public:
    using data_type = accordion_data;

    /**
     * @brief Returns default accordion data structure.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Fluent state defaults.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param default_expanded Default expanded state status.
     * @return Accordion default data.
     */
    static data_type get_default_data(const std::string& /*label*/, bool default_expanded = true) {
        return { default_expanded };
    }

    /// Header text label.
    std::string text;
    
    /// Expansion status flag.
    bool expanded;
    
    /// Bounded arbitrary parameter index.
    int32_t m_param_index;
    
    /// Header row height in pixels.
    float header_height = 24.0f;
    
    bool header_hovered = false;
    bool header_pressed = false;

    /**
     * @brief Constructs a new collapsible accordion container.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Collapsible stack setup.
     *
     * @warning <b>Memory & Lifecycles:</b> Binds a persistent parameter index.
     *
     * @param param_idx Bounded parameter index.
     * @param label Accordion display label.
     * @param default_expanded Expansion state status.
     */
    accordion(int32_t param_idx, std::string label, bool default_expanded = true)
        : vstack(), m_param_index(param_idx), text(std::move(label)), expanded(default_expanded) {
    }

protected:
    /**
     * @brief Measures accordion layout dimensions.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounded overrides that automatically suppress child layout computations when collapsed.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param avail_w Available width bounds.
     * @param avail_h Available height bounds.
     * @return Bounded size vector.
     */
    core::vec2 measure_impl(float avail_w, float avail_h) override {
        if (!expanded) {
            return { avail_w, header_height };
        }
        // If expanded, measure children
        auto sz = vstack::measure_impl(avail_w, avail_h);
        return { sz.x, sz.y + header_height + gap };
    }

    /**
     * @brief Performs vertical layout alignments.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounded overrides that automatically suppress child layout computations when collapsed.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param x Layout horizontal coordinate.
     * @param y Layout vertical coordinate.
     * @param w Layout width dimension.
     * @param h Layout height dimension.
     */
    void do_layout_impl(float x, float y, float w, float h) override {
        if (!expanded) return;

        // Layout children starting below the header
        float children_y = y + header_height + gap;
        float children_h = h - header_height - gap;
        vstack::do_layout_impl(x, children_y, w, children_h);
    }

    /**
     * @brief Paints the accordion header and expanded children.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounded overrides that automatically suppress child layout computations when collapsed.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param canvas Drawbot canvas.
     * @param supplier Drawbot supplier.
     * @param t Theme color definitions.
     */
    void paint_impl(drawbot::canvas& canvas, drawbot::supplier& supplier, const theme& t) override {

        // Draw Header Background
        core::color<> bg_color = header_pressed ? t.accent_pressed : (header_hovered ? t.bg_light : t.bg);
        canvas.fill_rect(bounds.x, bounds.y, bounds.w, header_height, bg_color);

        // Draw expand/collapse icon (triangle)
        float cx = bounds.x + 12.0f;
        float cy = bounds.y + header_height * 0.5f;
        auto icon_pen = supplier.create_pen(t.text, 1.5f);
        auto icon_brush = supplier.create_brush(t.text);

        if (expanded) {
            // Down-pointing triangle
            auto p = supplier.create_path()
                .move_to(cx - 4, cy - 2)
                .line_to(cx + 4, cy - 2)
                .line_to(cx, cy + 3)
                .line_to(cx - 4, cy - 2)
                .build();
            canvas.fill_path(p, icon_brush);
        } else {
            // Right-pointing triangle
            auto p = supplier.create_path()
                .move_to(cx - 2, cy - 4)
                .line_to(cx + 3, cy)
                .line_to(cx - 2, cy + 4)
                .line_to(cx - 2, cy - 4)
                .build();
            canvas.fill_path(p, icon_brush);
        }

        // Draw Text
        if (supplier.supports_text()) {
            auto f = supplier.create_font(t.font_size);
            auto b = supplier.create_brush(t.text);
            canvas.draw_text(text, f, b, { bounds.x + 24.0f, bounds.y + header_height - 6.0f });
        }

        // Paint children
        if (expanded) {
            for (auto& child : children) {
                if (child && child->visible) {
                    child->paint(canvas, supplier, t);
                }
            }
        }
    }

    /**
     * @brief Coordinate hit test.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounded overrides that automatically suppress child layout computations when collapsed.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @return True if hit.
     */
    bool hit_test(float lx, float ly) const override {
        return visible && bounds.contains(lx, ly);
    }

    /**
     * @brief Deep coordinate widget search traversal.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounded overrides that automatically suppress child layout computations when collapsed.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @return Deepest widget target.
     */
    widget* find_widget_at_impl(float lx, float ly) override {
        if (!visible || !bounds.contains(lx, ly)) return nullptr;

        if (expanded && ly >= bounds.y + header_height) {
            // Check children
            for (int i = static_cast<int>(children.size()) - 1; i >= 0; --i) {
                auto& child = children[i];
                if (!child || !child->visible) continue;
                if (auto* hit = child->find_widget_at(lx, ly)) {
                    return hit;
                }
            }
        }

        // Hit the header
        return this;
    }

    /**
     * @brief Click handler.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounded overrides that automatically suppress child layout computations when collapsed.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @param mods Modifiers.
     * @return True if click registered.
     */
    bool on_click_impl(float lx, float ly, uint32_t mods) override {
        if (ly < bounds.y + header_height) {
            header_pressed = true;
            return true;
        }
        return false;
    }

    /**
     * @brief Release handler toggling expanded state status.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounded overrides that automatically suppress child layout computations when collapsed.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void on_release_impl() override {
        if (header_pressed) {
            expanded = !expanded;
            header_pressed = false;
            on_accordion_toggled(expanded);
        }
    }

    /**
     * @brief Hover enter routing.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounded overrides that automatically suppress child layout computations when collapsed.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void on_hover_enter_impl() override {
        header_hovered = true;
    }

    /**
     * @brief Hover exit routing.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounded overrides that automatically suppress child layout computations when collapsed.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void on_hover_exit_impl() override {
        header_hovered = false;
        header_pressed = false;
    }

    /**
     * @brief Syncs state parameters.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounded overrides that automatically suppress child layout computations when collapsed.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    void sync_state_impl(const interaction_context& ctx) override {
        auto lock = ctx.arb_data<accordion_data>(m_param_index);
        if (lock) expanded = lock->expanded;
        vstack::sync_state_impl(ctx);
    }

    /**
     * @brief Commits state changes.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounded overrides that automatically suppress child layout computations when collapsed.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    void commit_state_impl(const interaction_context& ctx) override {
        auto lock = ctx.arb_data<accordion_data>(m_param_index);
        if (lock && lock->expanded != expanded) {
            lock->expanded = expanded;
            ctx.mark_param_changed(m_param_index);
        }
        vstack::commit_state_impl(ctx);
    }

protected:
    /**
     * @brief Safe subclass event hook for collapsible changes.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Expansion event callback.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param expanded_state New expansion state flag status.
     */
    virtual void on_accordion_toggled(bool expanded_state) {}
};

} // namespace aetk::effect::ui
