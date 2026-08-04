#pragma once

#include <aetk/effect/ui/widget.hpp>

namespace aetk::effect::ui {

/**
 * @brief Base class for declarative, reusable UI components.
 * 
 * @details Instead of manually managing child widget memory, a component overrides `build()`
 * to return a constructed tree of widgets. The component caches this tree and
 * forwards all layout, drawing, and interaction events to it.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, custom Drawbot interfaces require implementing complex, procedurally managed event cascades and pixel-level hit tests across rendering dispatches. `aetk::effect::ui::component` shifts this paradigm to a modern, declarative, widget-tree composition model. Instead of manually registering child widgets or tracking coordinates, components override `build()` to define a declarative subtree, delegating hit testing, layouts, drawing, and interaction states seamlessly.
 *
 * @warning <b>Memory & Lifecycles:</b> The component owns the unique pointer lifecycle of its declared widget subtree (`m_tree`). Destructor automatically cleans up all child allocations. Rebuild commands (`mark_needs_build`) release the previous subtree, freeing child memory safely.
 */
class component : public widget {
public:
    component() = default;

    /**
     * @brief Build and return the widget tree for this component.
     * 
     * Called automatically during the first measure pass if the tree hasn't been built.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Declarative template builders replacing procedural event maps.
     *
     * @warning <b>Memory & Lifecycles:</b> Must return a unique pointer to a new widget subtree.
     *
     * @return Unique pointer to the root widget of the constructed tree.
     */
    virtual std::unique_ptr<widget> build() = 0;

    /**
     * @brief Manually force the component to rebuild its tree.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Live layout dirtying.
     *
     * @warning <b>Memory & Lifecycles:</b> Instantly resets and disposes the existing subtree memory.
     */
    void mark_needs_build() {
        m_tree.reset();
    }

protected:
    std::unique_ptr<widget> m_tree;

public:
    /**
     * @brief Hit test.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Delegates low-level event flows automatically to the active child tree.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative position.
     * @param ly Vertical relative position.
     * @return True if hit.
     */
    bool hit_test(float lx, float ly) const override {
        if (!visible) return false;
        // ensure_built() is non-const, so we can't call it here directly if component is const,
        // but hit_test is const. We'll assume do_layout or sync_state happened first.
        if (m_tree && m_tree->hit_test(lx, ly)) return true;
        return bounds.contains(lx, ly);
    }

protected:
    /**
     * @brief Measures component dimension bounds.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Delegates low-level event flows automatically to the active child tree.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param avail_w Available width bounds.
     * @param avail_h Available height bounds.
     * @return Vector dimension.
     */
    core::vec2 measure_impl(float avail_w, float avail_h) override {
        ensure_built();
        if (m_tree) {
            auto sz = m_tree->measure(avail_w, avail_h);
            return {
                (std::clamp)(static_cast<float>(sz.x), layout.min_width, layout.max_width),
                (std::clamp)(static_cast<float>(sz.y), layout.min_height, layout.max_height)
            };
        }
        return { 0.0f, 0.0f };
    }

    /**
     * @brief Performs layout operations.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Delegates low-level event flows automatically to the active child tree.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param x Layout horizontal coordinate.
     * @param y Layout vertical coordinate.
     * @param w Layout width dimension.
     * @param h Layout height dimension.
     */
    void do_layout_impl(float x, float y, float w, float h) override {
        ensure_built();
        if (m_tree) {
            m_tree->do_layout(x, y, w, h);
        }
    }

    /**
     * @brief Paints the widget canvas.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Delegates low-level event flows automatically to the active child tree.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param canvas Drawbot canvas.
     * @param supplier Drawbot suppliers.
     * @param t Theme information.
     */
    void paint_impl(drawbot::canvas& canvas, drawbot::supplier& supplier, const theme& t) override {
        ensure_built();
        if (m_tree && m_tree->visible) {
            m_tree->paint(canvas, supplier, t);
        }
    }

    /**
     * @brief Interaction click routing.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Delegates low-level event flows automatically to the active child tree.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @param mods Modifiers.
     * @return True if handled.
     */
    bool on_click_impl(float lx, float ly, uint32_t mods) override {
        if (!m_tree) return false;
        return m_tree->on_click(lx, ly, mods);
    }

    /**
     * @brief Interaction drag routing.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Delegates low-level event flows automatically to the active child tree.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal coordinate.
     * @param ly Vertical coordinate.
     * @param mods Modifiers.
     * @return True if handled.
     */
    bool on_drag_impl(float lx, float ly, uint32_t mods) override {
        if (!m_tree) return false;
        return m_tree->on_drag(lx, ly, mods);
    }

    /**
     * @brief Interaction release routing.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Delegates low-level event flows automatically to the active child tree.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void on_release_impl() override {
        if (m_tree) m_tree->on_release();
    }

    /**
     * @brief Hover enter routing.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Delegates low-level event flows automatically to the active child tree.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void on_hover_enter_impl() override {
        if (m_tree) m_tree->on_hover_enter();
    }

    /**
     * @brief Hover exit routing.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Delegates low-level event flows automatically to the active child tree.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void on_hover_exit_impl() override {
        if (m_tree) m_tree->on_hover_exit();
    }

    /**
     * @brief Cursor style inspect routing.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Delegates low-level event flows automatically to the active child tree.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Cursor type value.
     */
    int32_t cursor_type_impl() const override {
        if (m_tree) return m_tree->cursor_type();
        return 0;
    }

    /**
     * @brief Sync widget states.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Delegates low-level event flows automatically to the active child tree.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    void sync_state_impl(const interaction_context& ctx) override {
        ensure_built();
        if (m_tree) m_tree->sync_state(ctx);
    }

    /**
     * @brief Commit changed states.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Delegates low-level event flows automatically to the active child tree.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    void commit_state_impl(const interaction_context& ctx) override {
        if (m_tree) m_tree->commit_state(ctx);
    }

    /**
     * @brief Deep coordinate widget search routing.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Delegates low-level event flows automatically to the active child tree.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @return Widget reference.
     */
    widget* find_widget_at_impl(float lx, float ly) override {
        if (!visible || !bounds.contains(lx, ly)) return nullptr;
        ensure_built();
        if (m_tree) {
            if (auto* hit = m_tree->find_widget_at(lx, ly)) {
                return hit;
            }
        }
        return this;
    }

private:
    /**
     * @brief Subtree initialization helper.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Lazy layout instantiation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void ensure_built() {
        if (!m_tree) {
            m_tree = build();
        }
    }
};

} // namespace aetk::effect::ui
