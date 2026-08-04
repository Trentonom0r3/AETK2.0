#pragma once

#include <aetk/effect/ui/widget.hpp>
#include <aetk/ui/theme.hpp>
#include <aetk/effect/ui/widgets/resizable_data.hpp>
#include <algorithm>

namespace aetk::effect::ui {

/**
 * @brief A container that wraps a single child widget and provides a drag handle to resize it.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, custom interface panels are strictly hardcoded or fixed-size, as writing dynamic parent layouts and manual diagonal cursor dragging (`PF_Cursor_MAGNIFY`) is extremely error-prone. `aetk::effect::ui::resizable` introduces a modular container wrapper. It intercepts the `measure` and `layout` pass of its child, draws a clean triangular resize handle at the bottom-right corner, and manages drag coordinates automatically. The resulting resized dimension is synchronized back to an After Effects sequence arbitrary block (`resizable_data`) to remember custom layouts across frames.
 *
 * @warning <b>Memory & Lifecycles:</b> The resizable widget inherits from `container` and owns child widget hierarchies using `std::unique_ptr` arrays. State dimensions are marshaled to and from After Effects parameter sequences automatically via `sync_state_impl` and `commit_state_impl` loops.
 */
class resizable : public container {
public:
    using data_type = resizable_data;

    /**
     * @brief Returns default resizable data structure.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Fluent state defaults.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Resizable default data.
     */
    static data_type get_default_data(const std::string& /*label*/) {
        return { -1.0f, -1.0f };
    }

    /// Active resizable dimensions.
    resizable_data data;
    
    /// Bounded parameter index.
    int32_t m_param_index = 0;

    bool handle_hovered = false;
    bool handle_pressed = false;

    // To calculate delta
    float drag_start_x = 0;
    float drag_start_y = 0;
    float start_w = 0;
    float start_h = 0;

    /**
     * @brief Constructs a new resizable container wrapper.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Resizable stack wrapper setup.
     *
     * @warning <b>Memory & Lifecycles:</b> Binds a persistent parameter index.
     *
     * @param param_idx Bounded parameter index.
     */
    resizable(int32_t param_idx, std::string /*label*/)
        : m_param_index(param_idx) {
        layout.min_width = 50.0f;
        layout.min_height = 50.0f;
    }

protected:
    /**
     * @brief Measures resizable layout dimensions.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Intercepts layout bounds checks to inject custom resizing bounds.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param avail_w Width bounds.
     * @param avail_h Height bounds.
     * @return Size vector.
     */
    core::vec2 measure_impl(float avail_w, float avail_h) override {
        core::vec2 child_sz = { 0, 0 };
        if (!children.empty() && children[0] && children[0]->visible) {
            child_sz = children[0]->measure(avail_w, avail_h);
        }

        float final_w = (data.width > 0) ? data.width : child_sz.x;
        float final_h = (data.height > 0) ? data.height : child_sz.y;

        final_w = (std::max)(final_w, layout.min_width);
        final_h = (std::max)(final_h, layout.min_height);

        return { final_w, final_h };
    }

    /**
     * @brief Performs layout bounds checking.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Intercepts layout bounds checks to inject custom resizing bounds.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param x Horizontal pixel position.
     * @param y Vertical pixel position.
     * @param w Layout width limit.
     * @param h Layout height limit.
     */
    void do_layout_impl(float x, float y, float w, float h) override {
        float final_w = (data.width > 0) ? data.width : w;
        float final_h = h;
        
        final_w = (std::max)(final_w, layout.min_width);
        
        bounds = { x, y, final_w, final_h };
        if (!children.empty() && children[0]) {
            children[0]->do_layout(x, y, final_w, final_h);
        }
    }

    /**
     * @brief Paints the child widget and resizing corner highlight triangle.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Intercepts layout bounds checks to inject custom resizing bounds.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param canvas Drawbot canvas.
     * @param supplier Drawbot supplier.
     * @param t Theme color definitions.
     */
    void paint_impl(drawbot::canvas& canvas, drawbot::supplier& supplier, const theme& t) override {
        if (!visible) return;

        // Paint child first
        if (!children.empty() && children[0] && children[0]->visible) {
            children[0]->paint(canvas, supplier, t);
        }

        // Draw resize handle at bottom right
        float handle_sz = 12.0f;
        core::rect_f handle_rect = { 
            bounds.x + bounds.w - handle_sz, 
            bounds.y + bounds.h - handle_sz, 
            handle_sz, handle_sz 
        };

        core::color<> h_color = handle_pressed ? t.handle_active : (handle_hovered ? t.accent_hover : t.handle);

        auto p = supplier.create_path()
            .move_to(handle_rect.left, handle_rect.top + handle_sz)
            .line_to(handle_rect.left + handle_sz, handle_rect.top)
            .line_to(handle_rect.left + handle_sz, handle_rect.top + handle_sz)
            .line_to(handle_rect.left, handle_rect.top + handle_sz)
            .build();
            
        canvas.fill_path(p, supplier.create_brush(h_color));
    }

    /**
     * @brief Coordinate hit test checker.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Intercepts layout bounds checks to inject custom resizing bounds.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @return True if inside.
     */
    bool hit_test(float lx, float ly) const override {
        return visible && bounds.contains(lx, ly);
    }

    /**
     * @brief Deep coordinate widget search traversal.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Intercepts layout bounds checks to inject custom resizing bounds.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @return Deepest widget target.
     */
    widget* find_widget_at_impl(float lx, float ly) override {
        if (!visible || !bounds.contains(lx, ly)) return nullptr;

        // Check if hitting handle first
        float handle_sz = 12.0f;
        if (lx >= bounds.x + bounds.w - handle_sz && ly >= bounds.y + bounds.h - handle_sz) {
            return this;
        }

        // Then check child
        if (!children.empty() && children[0] && children[0]->visible) {
            if (auto* hit = children[0]->find_widget_at(lx, ly)) {
                return hit;
            }
        }

        return this;
    }

    /**
     * @brief Click handler registering drag origins on handle.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Intercepts layout bounds checks to inject custom resizing bounds.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @param mods Modifiers.
     * @return True if click registered on handle.
     */
    bool on_click_impl(float lx, float ly, uint32_t mods) override {
        float handle_sz = 12.0f;
        if (lx >= bounds.x + bounds.w - handle_sz && ly >= bounds.y + bounds.h - handle_sz) {
            handle_pressed = true;
            drag_start_x = lx;
            drag_start_y = ly;
            start_w = data.width > 0 ? data.width : bounds.w;
            start_h = data.height > 0 ? data.height : bounds.h;
            return true;
        }
        return false;
    }

    /**
     * @brief Drag handler resizing layout bounds dimensions.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Intercepts layout bounds checks to inject custom resizing bounds.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @param mods Modifiers.
     * @return True if drag registered on handle.
     */
    bool on_drag_impl(float lx, float ly, uint32_t mods) override {
        if (!handle_pressed) return false;

        float dx = lx - drag_start_x;
        float dy = ly - drag_start_y;

        data.width = (std::max)(start_w + dx, layout.min_width);
        data.height = (std::max)(start_h + dy, layout.min_height);
        
        on_resized(data.width, data.height);
        return true;
    }

    /**
     * @brief Release handler.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Intercepts layout bounds checks to inject custom resizing bounds.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void on_release_impl() override {
        handle_pressed = false;
    }

    /**
     * @brief Hover enter routing.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Intercepts layout bounds checks to inject custom resizing bounds.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void on_hover_enter_impl() override {
        handle_hovered = true;
    }

    /**
     * @brief Hover exit routing.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Intercepts layout bounds checks to inject custom resizing bounds.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void on_hover_exit_impl() override {
        handle_hovered = false;
        handle_pressed = false;
    }

    /**
     * @brief Inspects custom cursor type on hover bounds.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Intercepts layout bounds checks to inject custom resizing bounds. Sets diagonal resize cursor proxy (`PF_Cursor_MAGNIFY`).
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Cursor type value.
     */
    int32_t cursor_type_impl() const override {
        if (handle_hovered || handle_pressed) return PF_Cursor_MAGNIFY; // No diagonal resize cursor in AE, magnify acts as a proxy
        return 0;
    }

    /**
     * @brief Syncs state parameters.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Intercepts layout bounds checks to inject custom resizing bounds.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    void sync_state_impl(const interaction_context& ctx) override {
        auto lock = ctx.arb_data<resizable_data>(m_param_index);
        if (lock) {
            data.width = lock->width;
            data.height = lock->height;
        }
        container::sync_state_impl(ctx);
    }

    /**
     * @brief Commits state changes.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Intercepts layout bounds checks to inject custom resizing bounds.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    void commit_state_impl(const interaction_context& ctx) override {
        auto lock = ctx.arb_data<resizable_data>(m_param_index);
        if (lock && (lock->width != data.width || lock->height != data.height)) {
            lock->width = data.width;
            lock->height = data.height;
            ctx.mark_param_changed(m_param_index);
        }
        container::commit_state_impl(ctx);
    }

protected:
    /**
     * @brief Safe subclass event hook for resizing changes.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Resized event callback.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param w New width dimension.
     * @param h New height dimension.
     */
    virtual void on_resized(float w, float h) {}
};

} // namespace aetk::effect::ui
