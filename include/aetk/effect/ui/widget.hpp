#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif
#pragma once

#include <aetk/core/types.hpp>
#include <aetk/ui/drawbot.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cmath>

#include <aetk/ui/theme.hpp>

namespace aetk::effect { struct interaction_context; }
namespace aetk::ui::drawbot { 
    class canvas; 
    class supplier; 
}

namespace aetk::effect::ui {

using theme = ::aetk::ui::theme;
namespace drawbot = ::aetk::ui::drawbot;

// ══════════════════════════════════════════════════════════════════════
//  Layout Properties (Flexbox-like)
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Bounded layout dimension properties.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Flexbox size constraints.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct layout_props {
    float flex = 0;             // flex grow factor (0 = fixed size)
    float min_width = 0;
    float min_height = 0;
    float max_width = 99999.0f;
    float max_height = 99999.0f;
    float padding = 0;
    bool resizable = false;     // is this widget directly resizable?
};

// ══════════════════════════════════════════════════════════════════════
//  Bounds Rectangle
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Bounded layout coordinates.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Position boundaries.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct bounds_rect {
    float x = 0, y = 0, w = 0, h = 0;

    /**
     * @brief Checks if a coordinate point falls inside the boundary.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Coordinate hit checking.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param px Horizontal coordinate.
     * @param py Vertical coordinate.
     * @return True if inside.
     */
    bool contains(float px, float py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

// ══════════════════════════════════════════════════════════════════════
//  Widget Base Class
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Base class for all UI components in the AETK UI framework.
 * 
 * @details Every widget has:
 *  - Layout properties (flex, min/max sizes, padding)
 *  - Computed bounds (set by the parent layout engine)
 *  - Paint, measure, and interaction virtual methods
 *  - Shadow-state hooks for future keyframe support (Phase 2)
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, custom Drawbot user interface items must be procedural, executing raw drawing and event switches. `aetk::effect::ui::widget` modernizes this into a Non-Virtual Interface (NVI) pattern. Instead of writing duplicate hit-tests and coordinate transformations, standard checks are locked in the base class public functions, and children override protected custom implementations (`measure_impl`, `paint_impl`, `on_click_impl`). This ensures coordinate constraints, visible toggles, and state locks are uniformly applied across all layouts.
 *
 * @warning <b>Memory & Lifecycles:</b> The standard base widget contains non-owning inspectors. The child `container` class holds dynamic child widget pointers using `std::unique_ptr` arrays, guaranteeing zero memory leaks during tree disposal passes. Destructors are marked virtual. Holds focus, visibility, and hover state flags.
 */
class widget {
public:
    virtual ~widget() = default;

    /// String identifier for debugging.
    std::string id;

    // Setter for tooltip text
    widget& set_tooltip(std::string text) { m_tooltip = std::move(text); return *this; }

    // Getter for tooltip text
    const std::string& tooltip() const { return m_tooltip; }
    
    /// Widget layout specifications.
    layout_props layout;
    
    /// Computed pixel boundaries.
    bounds_rect bounds;

    bool visible = true;
    bool enabled = true;
    bool hovered = false;
    bool pressed = false;

    // Resizing State
    bool m_is_resizing = false;
    float m_drag_start_x = 0;
    float m_drag_start_y = 0;
    float m_start_w = 0;
    float m_start_h = 0;

    // ── NVI Public Interface ──────────────────────────────────────────

    /**
     * @brief Measures layout dimensions.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounded layout measuring.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param avail_w Available width in pixels.
     * @param avail_h Available height in pixels.
     * @return Bounded dimension vector.
     */
    virtual core::vec2 measure(float avail_w, float avail_h) final {
        return measure_impl(avail_w, avail_h);
    }

    /**
     * @brief Positions the widget in absolute canvas spaces.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounded positioning.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param x Horizontal pixel position.
     * @param y Vertical pixel position.
     * @param w Horizontal width limit.
     * @param h Vertical height limit.
     */
    virtual void do_layout(float x, float y, float w, float h) final {
        bounds = { x, y, w, h };
        do_layout_impl(x, y, w, h);
    }

    /**
     * @brief Paints the widget canvas.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounded graphics drawing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param canvas Drawbot canvas.
     * @param supplier Drawbot suppliers.
     * @param t UI theme parameters.
     */
    virtual void paint(drawbot::canvas& canvas, drawbot::supplier& supplier, const theme& t) final {
        if (!visible) return;
        paint_impl(canvas, supplier, t);
    }

    /**
     * @brief Coordinate hit test checker.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Hit testing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @return True if hit.
     */
    virtual bool hit_test(float lx, float ly) const {
        return visible && bounds.contains(lx, ly);
    }

    /**
     * @brief Click handler.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Interaction routing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal coordinate.
     * @param ly Vertical coordinate.
     * @param mods Modifiers.
     * @return True if handled.
     */
    virtual bool on_click(float lx, float ly, uint32_t mods) final {
        if (!enabled || !visible) return false;
        return on_click_impl(lx, ly, mods);
    }

    /**
     * @brief Drag handler.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Interaction routing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal coordinate.
     * @param ly Vertical coordinate.
     * @param mods Modifiers.
     * @return True if handled.
     */
    virtual bool on_drag(float lx, float ly, uint32_t mods) final {
        if (!enabled || !visible) return false;
        return on_drag_impl(lx, ly, mods);
    }

    /**
     * @brief Release handler.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Interaction routing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    virtual void on_release() final {
        if (!enabled || !visible) return;
        on_release_impl();
    }

    /**
     * @brief Hover enter routing.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Interaction routing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    virtual void on_hover_enter() final {
        hovered = true;
        on_hover_enter_impl();
    }

    /**
     * @brief Hover exit routing.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Interaction routing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    virtual void on_hover_exit() final {
        hovered = false;
        on_hover_exit_impl();
    }

    /**
     * @brief Hover coordinate movement routing.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @return True if redraw is needed.
     */
    virtual bool on_hover_move(float lx, float ly) final {
        if (!enabled || !visible) return false;
        return on_hover_move_impl(lx, ly);
    }

    /**
     * @brief Inspects native cursor styles.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Custom cursor inspections.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Cursor type value.
     */
    virtual int32_t cursor_type() const final {
        return cursor_type_impl();
    }

    /**
     * @brief Commits state changes.
     *
     * @note <b>AE SDK Paradigm Shift:</b> State committing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    virtual void commit_state(const interaction_context& ctx) final {
        commit_state_impl(ctx);
    }

    /**
     * @brief Syncs state parameters.
     *
     * @note <b>AE SDK Paradigm Shift:</b> State syncing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    virtual void sync_state(const interaction_context& ctx) final {
        sync_state_impl(ctx);
    }

    /**
     * @brief Key event handler.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Keyboard input routing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     * @return True if handled.
     */
    virtual bool on_key(const interaction_context& ctx) final {
        if (!enabled || !visible) return false;
        return on_key_impl(ctx);
    }

    bool m_focused = false;
    
    /** @brief Gains focus. */
    virtual void on_focus_gained() { m_focused = true; }
    
    /** @brief Loses focus. */
    virtual void on_focus_lost() { m_focused = false; }
    
    /** @brief Check focus status. */
    bool is_focused() const { return m_focused; }

    /**
     * @brief Find the deepest widget at a given local point.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Deep widget coordinate search.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @return Widget reference.
     */
    virtual widget* find_widget_at(float lx, float ly) final {
        if (!visible) return nullptr;
        return find_widget_at_impl(lx, ly);
    }

    /** @brief Returns true if the widget is title-only and doesn't need a control area (no twirly). */
    virtual bool is_title_only() const { return false; }

    // ── Title-Area NVI (PF_EA_PARAM_TITLE) ────────────────────────────

    /**
     * @brief Paint the widget's inline summary in the parameter title row.
     *
     * @note The title row is always visible even when the twirly is collapsed.
     * Override `paint_title_impl` in subclasses to draw a compact summary.
     *
     * @param canvas Drawbot canvas.
     * @param supplier Drawbot supplier.
     * @param t UI theme.
     * @param x Left edge of the drawable title area (after horiz_offset).
     * @param y Top edge of the title row.
     * @param w Available width.
     * @param h Height of the title row.
     */
    virtual void paint_title(drawbot::canvas& canvas, drawbot::supplier& supplier,
                             const theme& t, float x, float y, float w, float h) final {
        paint_title_impl(canvas, supplier, t, x, y, w, h);
    }

    /** @brief Handle a click in the title area. Return true if handled. */
    virtual bool on_title_click(float lx, float ly, uint32_t mods) final {
        if (!enabled || !visible) return false;
        return on_title_click_impl(lx, ly, mods);
    }

    /** @brief Handle drag continuation in the title area. */
    virtual bool on_title_drag(float lx, float ly, uint32_t mods) final {
        if (!enabled || !visible) return false;
        return on_title_drag_impl(lx, ly, mods);
    }

    /** @brief Handle release after a title-area interaction. */
    virtual void on_title_release() final {
        on_title_release_impl();
    }

    /** @brief Check if a point is within the widget's title-area bounds. */
    virtual bool hit_test_title(float lx, float ly) const { return false; }

    /** @brief Handle mouse movement over the title area. Return true if visual change. */
    virtual bool on_title_hover_move(float lx, float ly) { return false; }

protected:
    // ── Overrideable Implementations ──────────────────────────────────
    virtual bool on_key_impl(const interaction_context& ctx) { return false; }
    virtual core::vec2 measure_impl(float avail_w, float avail_h) = 0;
    virtual void do_layout_impl(float x, float y, float w, float h) {}
    virtual void paint_impl(drawbot::canvas& canvas, drawbot::supplier& supplier, const theme& t) = 0;
    virtual bool on_click_impl(float lx, float ly, uint32_t mods) { return false; }
    virtual bool on_drag_impl(float lx, float ly, uint32_t mods) { return false; }
    virtual void on_release_impl() {}
    virtual void on_hover_enter_impl() {}
    virtual void on_hover_exit_impl() {}
    virtual bool on_hover_move_impl(float lx, float ly) { return false; }
    virtual int32_t cursor_type_impl() const { return 0; /* PF_Cursor_NONE */ }
    virtual void commit_state_impl(const aetk::effect::interaction_context& ctx) {}
    virtual void sync_state_impl(const aetk::effect::interaction_context& ctx) {}
    virtual widget* find_widget_at_impl(float lx, float ly) {
        return hit_test(lx, ly) ? this : nullptr;
    }

    // ── Title-Area Overrideable Implementations ──────────────────────
    virtual void paint_title_impl(drawbot::canvas&, drawbot::supplier&,
                                   const theme&, float, float, float, float) {}
    virtual bool on_title_click_impl(float, float, uint32_t) { return false; }
    virtual bool on_title_drag_impl(float, float, uint32_t) { return false; }
    virtual void on_title_release_impl() {}

private:
    std::string m_tooltip;
};

// ══════════════════════════════════════════════════════════════════════
//  Container — Widget that owns and manages children
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief A widget that contains other widgets.
 * 
 * @details Provides child management with unique_ptr ownership, and default
 * implementations for paint (iterate children) and hit_test
 * (reverse z-order traversal for correct front-to-back clicking).
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, custom Drawbot user interface items must be procedural, executing raw drawing and event switches. `aetk::effect::ui::widget` modernizes this into a Non-Virtual Interface (NVI) pattern. Instead of writing duplicate hit-tests and coordinate transformations, standard checks are locked in the base class public functions, and children override protected custom implementations (`measure_impl`, `paint_impl`, `on_click_impl`). This ensures coordinate constraints, visible toggles, and state locks are uniformly applied across all layouts.
 *
 * @warning <b>Memory & Lifecycles:</b> Manages `std::unique_ptr<widget>` vectors. Assures safe dynamic cleanup.
 */
class container : public widget {
public:
    /// Vector array containing child widget unique pointers.
    std::vector<std::unique_ptr<widget>> children;

    /**
     * @brief Add a child widget. Takes ownership.
     * 
     * @note <b>AE SDK Paradigm Shift:</b> Container hierarchy insertion.
     *
     * @warning <b>Memory & Lifecycles:</b> Dynamic unique_ptr transfer.
     *
     * @tparam T Child widget type.
     * @param child Unique pointer to child.
     * @return Raw pointer to the added widget (for further configuration).
     */
    template <typename T>
    T* add(std::unique_ptr<T> child) {
        T* ptr = child.get();
        children.push_back(std::move(child));
        return ptr;
    }

    /**
     * @brief Construct and add a child widget in-place.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Container hierarchy emplace.
     *
     * @warning <b>Memory & Lifecycles:</b> Dynamic unique_ptr instantiation.
     *
     * @tparam T Child widget type.
     * @tparam Args Forwarding constructor arguments.
     * @param args Bounded arguments.
     * @return Emplaced child raw pointer.
     */
    template <typename T, typename... Args>
    T* emplace(Args&&... args) {
        auto child = std::make_unique<T>(std::forward<Args>(args)...);
        return add(std::move(child));
    }

protected:
    /**
     * @brief Deep coordinate widget search traversal.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Z-order depth traversal.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @return Deepest widget target.
     */
    widget* find_widget_at_impl(float lx, float ly) override {
        if (!visible || !bounds.contains(lx, ly)) return nullptr;

        for (int i = static_cast<int>(children.size()) - 1; i >= 0; --i) {
            auto& child = children[i];
            if (!child || !child->visible) continue;

            if (auto* hit = child->find_widget_at(lx, ly)) {
                return hit;
            }
        }
        return this;
    }

protected:
    /**
     * @brief Paint all visible children.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Dynamic children painting.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param canvas Drawbot canvas.
     * @param supplier Drawbot supplier.
     * @param t Theme color definitions.
     */
    void paint_impl(drawbot::canvas& canvas, drawbot::supplier& supplier, const theme& t) override {
        for (auto& child : children) {
            if (child && child->visible) {
                child->paint(canvas, supplier, t);
            }
        }
    }

    /**
     * @brief Empty container dimension base.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Defer dimension measurements to subclasses.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param avail_w Width limit.
     * @param avail_h Height limit.
     * @return Bounded sizes.
     */
    core::vec2 measure_impl(float avail_w, float avail_h) override {
        return { avail_w, avail_h }; // Containers defer to layout subclasses
    }

    /**
     * @brief Forward state commit down the tree.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Recursive state commit.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    void commit_state_impl(const aetk::effect::interaction_context& ctx) override {
        for (auto& child : children) {
            if (child) child->commit_state(ctx);
        }
    }

    /**
     * @brief Forward state synchronization down the tree.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Recursive state sync.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    void sync_state_impl(const aetk::effect::interaction_context& ctx) override {
        for (auto& child : children) {
            if (child) child->sync_state(ctx);
        }
    }
};

} // namespace aetk::effect::ui
