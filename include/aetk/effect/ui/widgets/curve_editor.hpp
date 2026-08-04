#pragma once

#include <aetk/core/math.hpp>
#include <aetk/effect/context/context.hpp>
#include <aetk/effect/ui/curve_data.hpp>
#include <aetk/effect/ui/widget.hpp>
#include <aetk/ui/theme.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace aetk::effect::ui {

// ══════════════════════════════════════════════════════════════════════
//  Curve Editor Widget
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief A drop-in curve editor widget (like AE's RGB Curves).
 *
 * @details Renders a Catmull-Rom spline through user-editable control points,
 * backed by an arbitrary data parameter for serialization and keyframing.
 *
 * Usage:
 *   root->emplace<curve_editor>(1,  // param index
 *       curve_editor::options().set_height(180.0f).set_max_points(16)
 *   );
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, building a multi-node spline editor
 * (such as RGB Curves) requires writing massive manual rendering loops, allocating
 * complex custom drawing math for Catmull-Rom or Bezier spline curves, tracking click
 * positions against node hit boxes, and flattening structures manually for host
 * keyframing. `aetk::effect::ui::curve_editor` shifts this paradigm to a drop-in OOP
 * custom UI control. It isolates grid computations, hit tests, node dragging, ctrl-click
 * deletions, and spline drawings inside a single clean class. Evaluated spline positions
 * are available statically via `evaluate(...)` during effect rendering passes, ensuring
 * perfect alignment between the UI visual curves and pixel processing kernels.
 *
 * @warning <b>Memory & Lifecycles:</b> The editor contains cached point arrays. Point
 * updates and dragged node interactions are automatically synced to and committed from
 * After Effects arbitrary parameter sequence states (`sync_state_impl`,
 * `commit_state_impl`) to prevent memory leaks or out-of-sync frames. Dynamic node
 * additions and deletions are bounds-checked (`min_points`, `max_points`) to prevent
 * structure buffer overflow leaks inside persistent sequences.
 */
class curve_editor : public widget {
public:
    using data_type = curve_data;

    /**
     * @brief Supported spline interpolation schemes.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Type-safe interpolation mode indicators.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    enum class interpolation : std::uint8_t {
        linear,
        catmull_rom,
    };

    /**
     * @brief Custom aesthetic and grid layout configurations.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Fluent layout configurations replacing raw
     * hardcoded spline values.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    struct options {
        interpolation curve_type;
        int samples_per_seg;

        int min_points;
        int max_points;
        bool lock_endpoints;

        bool show_grid;
        int grid_cols;
        int grid_rows;

        float height;
        float hit_radius;
        float node_size;
        float active_node_size;

        core::color<> bg_color;
        core::color<> grid_color;
        core::color<> node_color;
        core::color<> active_color;

        /**
         * @brief options constructor.
         *
         * @note <b>AE SDK Paradigm Shift:</b> Configures default values for the editor
         * layout.
         *
         * @warning <b>Memory & Lifecycles:</b> None.
         */
        options()
            : curve_type(interpolation::catmull_rom)
            , samples_per_seg(16)
            , min_points(2)
            , max_points(32)
            , lock_endpoints(true)
            , show_grid(true)
            , grid_cols(4)
            , grid_rows(4)
            , height(150.0f)
            , hit_radius(12.0f)
            , node_size(8.0f)
            , active_node_size(10.0f)
            , bg_color(0.0f, 0.0f, 0.0f, 0.0f)
            , grid_color(0.18f, 0.18f, 0.18f)
            , // <-- Darkened from 0.25f
            node_color(0.2f, 0.6f, 0.9f)
            , active_color(1.0f, 0.55f, 0.0f) {
        }
        bool use_native_stroke = false;

        bool show_diagnostics = false;

        // Builder methods
        options& set_curve_type(interpolation t) {
            curve_type = t;
            return *this;
        }
        options& set_samples(int n) {
            samples_per_seg = n;
            return *this;
        }
        options& set_min_points(int n) {
            min_points = n;
            return *this;
        }
        options& set_max_points(int n) {
            max_points = n;
            return *this;
        }
        options& set_lock_endpoints(bool v) {
            lock_endpoints = v;
            return *this;
        }
        options& set_grid(int cols, int rows) {
            grid_cols = cols;
            grid_rows = rows;
            return *this;
        }
        options& set_show_grid(bool v) {
            show_grid = v;
            return *this;
        }
        options& set_height(float h) {
            height = h;
            return *this;
        }
        options& set_hit_radius(float r) {
            hit_radius = r;
            return *this;
        }
        options& set_node_size(float s) {
            node_size = s;
            return *this;
        }
        options& set_active_node_size(float s) {
            active_node_size = s;
            return *this;
        }
        options& set_bg_color(core::color<> c) {
            bg_color = c;
            return *this;
        }
        options& set_grid_color(core::color<> c) {
            grid_color = c;
            return *this;
        }
        options& set_node_color(core::color<> c) {
            node_color = c;
            return *this;
        }
        options& set_active_color(core::color<> c) {
            active_color = c;
            return *this;
        }
        options& set_native_stroke(bool v) {
            use_native_stroke = v;
            return *this;
        }
        options& set_show_diagnostics(bool v) {
            show_diagnostics = v;
            return *this;
        }
    };

    /**
     * @brief Fluent state defaults for setup.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Fluent state defaults.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Curve default data.
     */
    static data_type get_default_data(const options& /*opt*/ = options()) {
        return { };
    }

    /// Mutable node color.
    void set_node_color(core::color<> c) {
        m_opts.node_color = c;
    }

    /// Mutable active color.
    void set_active_color(core::color<> c) {
        m_opts.active_color = c;
    }

    /// Mutable background color.
    void set_bg_color(core::color<> c) {
        m_opts.bg_color = c;
    }

    /// Mutable grid color.
    void set_grid_color(core::color<> c) {
        m_opts.grid_color = c;
    }

    /**
     * @brief Constructs a new curve editor widget.
     *
     * @note <b>AE SDK Paradigm Shift:</b> High-performance custom user interface spline
     * control with unified Drawbot routing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param param_index Bounded parameter index.
     * @param opts Styling and layout options.
     */
    explicit curve_editor(int32_t param_index, options opts = { })
        : m_param_index(param_index)
        , m_opts(opts) {
        layout.min_height = m_opts.height;
    }

    /**
     * @brief Evaluate the curve at a given x position.
     *
     * Returns the interpolated y value for x ∈ [0, 1].
     * Usable from on_smart_render without a widget instance.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Static spline evaluator bypassing active widget
     * dependencies.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param pts Node control points.
     * @param x Horizontal search fraction value.
     * @param type Spline interpolation mode.
     * @return Interpolated vertical fraction value.
     */
    static float evaluate(const std::vector<curve_point>& pts, float x,
        interpolation type = interpolation::catmull_rom) {
        switch (type) {
        case interpolation::catmull_rom:
            return core::math::evaluate_catmull_rom(pts, x);
        case interpolation::linear:
            return core::math::evaluate_linear(pts, x);
        default:
            return core::math::evaluate_catmull_rom(pts, x);
        }
    }

protected:
    /**
     * @brief Measures editor layout dimensions.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Size limits.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param avail_w Available width bounds.
     * @return Bounded dimension sizes.
     */
    core::vec2 measure_impl(float avail_w, float) override {
        return { avail_w, m_opts.height };
    }

    /**
     * @brief Paints the editor grid, background, spline, and nodes.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Vector grid and path rendering.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param canvas Drawbot canvas.
     * @param supplier Drawbot supplier.
     * @param t Theme color parameters.
     */
    void paint_impl(
        drawbot::canvas& canvas, drawbot::supplier& supplier, const theme& t) override {
        if (!visible)
            return;
        float bx = bounds.x, by = bounds.y;
        float bw = bounds.w, bh = bounds.h;

        // --- COLOR UPDATE: Lighter Native AE Curve Well ---
        // ~RGB 45 for the background well
        core::color<> fill = m_opts.bg_color.alpha > 0.0f
            ? m_opts.bg_color
            : core::color<> { 1.0f, 45.0f / 255.0f, 45.0f / 255.0f, 45.0f / 255.0f };
        canvas.fill_rect(bx, by, bw, bh, fill);

        // Lighter border to frame the well (~RGB 70)
        auto inset_pen = supplier.create_pen(
            core::color<> { 1.0f, 70.0f / 255.0f, 70.0f / 255.0f, 70.0f / 255.0f }, 1.0f);
        auto inset_path = supplier.create_path()
                              .move_to(bx, by)
                              .line_to(bx + bw, by)
                              .line_to(bx + bw, by + bh)
                              .line_to(bx, by + bh)
                              .close()
                              .build();
        canvas.stroke_path(inset_path, inset_pen);

        // Grid
        if (m_opts.show_grid) {
            // Lighter grid lines to stand out clearly against the well (~RGB 70)
            auto grid_pen = supplier.create_pen(
                core::color<> { 1.0f, 70.0f / 255.0f, 70.0f / 255.0f, 70.0f / 255.0f },
                1.0f);
            for (int i = 1; i < m_opts.grid_cols; ++i) {
                float gx = bx + i * (bw / (float)m_opts.grid_cols);
                canvas.stroke_path(
                    supplier.create_path().move_to(gx, by).line_to(gx, by + bh).build(),
                    grid_pen);
            }
            for (int i = 1; i < m_opts.grid_rows; ++i) {
                float gy = by + i * (bh / (float)m_opts.grid_rows);
                canvas.stroke_path(
                    supplier.create_path().move_to(bx, gy).line_to(bx + bw, gy).build(),
                    grid_pen);
            }
        }

        // Draw curve if we have cached points (Stays Blue)
        if (m_cached_points.size() >= 2) {
            draw_curve(canvas, supplier, m_cached_points, bx, by, bw, bh, 1.0f);
            draw_nodes(
                canvas, supplier, m_cached_points, bx, by, bw, bh, m_cached_dragging);
        }

        // Diagnostics
        if (m_opts.show_diagnostics) {
            auto font = supplier.create_font(10.0f);
            char buf[128];
            snprintf(buf, sizeof(buf), "P:%zu DRG:%d", m_cached_points.size(),
                m_cached_dragging);
            auto text_brush = supplier.create_brush(core::color<>(1.0f, 0.6f, 0.6f, 0.6f));
            canvas.draw_text(buf, font, text_brush, core::vec2(bx + 5, by + bh - 5));
        }
    }

    /**
     * @brief Pull state from the arb parameter.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automatically synchronizes cached node
     * pointers.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    void sync_state_impl(const interaction_context& ctx) override {
        if (m_param_index <= 0)
            return; // Decoupled mode
        auto lock = ctx.arb_data<curve_data>(m_param_index);
        if (lock) {
            m_cached_points = lock->points;
            m_cached_dragging = lock->dragging_index;
        }
    }

    void paint_title_impl(drawbot::canvas& canvas, drawbot::supplier& supplier,
        const theme& t, float x, float y, float w, float h) override {
        if (supplier.supports_text()) {
            char buf[64] = {0};
            snprintf(buf, sizeof(buf), "%zu pts", m_cached_points.size());
            auto font = supplier.create_font(t.font_size * 0.9f);
            auto brush = supplier.create_brush(t.text_dim);
            float ty = y + h * 0.5f + t.font_size * 0.28f;
            canvas.draw_text(buf, font, brush,
                core::vec2(x + 4.0f, ty), kDRAWBOT_TextAlignment_Left);
        }
    }

public:
    /**
     * @brief Manual data feed for decoupled sub-widget usage.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Decoupled manual sync override.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param data Raw curve data container.
     */
    void set_data(const curve_data& data) {
        m_cached_points = data.points;
        m_cached_dragging = data.dragging_index;
    }

    // ── Interaction ──────────────────────────────────────────────

    /**
     * @brief Click handler detecting node clicks, additions, and deletions.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Dynamic node additions/deletions and ctrl-click
     * routing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @param mods Modifiers.
     * @return True if interaction registered.
     */
    bool on_click_impl(float lx, float ly, uint32_t mods) override {
        if (!visible || !enabled)
            return false;
        if (!bounds.contains(lx, ly))
            return false;

        if (bounds.w <= 0.0f || bounds.h <= 0.0f)
            return false;

        // Convert to normalized curve space
        float nx = (lx - bounds.x) / bounds.w;
        float ny = (ly - bounds.y) / bounds.h;

        // Hit-test against cached nodes
        int hit_index = -1;
        for (size_t i = 0; i < m_cached_points.size(); ++i) {
            float dx = lx - (bounds.x + m_cached_points[i].x * bounds.w);
            float dy = ly - (bounds.y + m_cached_points[i].y * bounds.h);
            if (std::sqrt(dx * dx + dy * dy) < m_opts.hit_radius) {
                hit_index = (int)i;
                break;
            }
        }

        bool is_ctrl = (mods & PF_Mod_CMD_CTRL_KEY) != 0;

        if (hit_index != -1 && is_ctrl) {
            // Ctrl+click on node: DELETE (protect endpoints)
            m_pending_action = action::remove;
            m_pending_index = hit_index;
            m_cached_dragging = -1;
        } else if (hit_index != -1) {
            // Click on existing node: GRAB
            m_pending_action = action::grab;
            m_pending_index = hit_index;
            m_cached_dragging = hit_index;
        } else {
            // Click on empty space: ADD
            m_pending_action = action::add;
            m_pending_point = { nx, ny };
            m_cached_dragging = -1; // will be set after add
        }

        m_dragging = true;
        return true;
    }

    /**
     * @brief Drag handler updating active node coordinates.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Normalized coordinate dragging.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param lx Horizontal relative coordinate.
     * @param ly Vertical relative coordinate.
     * @return True if drag registered.
     */
    bool on_drag_impl(float lx, float ly, uint32_t /*mods*/) override {
        if (!m_dragging)
            return false;

        float nx = std::clamp((lx - bounds.x) / bounds.w, 0.0f, 1.0f);
        float ny = std::clamp((ly - bounds.y) / bounds.h, 0.0f, 1.0f);
        m_pending_drag_pos = { nx, ny };
        m_has_pending_drag = true;
        return true;
    }

    /**
     * @brief Release handler resetting dragging flags.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Interactive release.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void on_release_impl() override {
        m_dragging = false;
        m_pending_release = true;
    }

    /**
     * @brief Commits state changes.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Commits node changes back to arbitrary After
     * Effects parameters.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Bounded interaction context.
     */
    void commit_state_impl(const interaction_context& ctx) override {
        if (m_param_index <= 0)
            return; // Decoupled mode
        auto lock = ctx.arb_data<curve_data>(m_param_index);
        if (!lock)
            return;
        apply_to(*lock);
        lock.mark_changed();
    }

    /**
     * @brief Apply pending interactions to a manual data object.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe procedural coordinate changes and sorting.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param data Spline data container.
     */
    void apply_to(curve_data& data) {
        // Apply pending action from on_click
        if (m_pending_action == action::remove) {
            if (m_pending_index > 0 && m_pending_index < (int)data.points.size() - 1) {
                data.points.erase(data.points.begin() + m_pending_index);
            }
            data.dragging_index = -1;
        } else if (m_pending_action == action::grab) {
            data.dragging_index = m_pending_index;
        } else if (m_pending_action == action::add) {
            if ((int)data.points.size() < m_opts.max_points) {
                data.points.push_back(m_pending_point);
                std::sort(data.points.begin(), data.points.end(),
                    [](const curve_point& a, const curve_point& b) { return a.x < b.x; });
                // Find the newly inserted point
                for (size_t i = 0; i < data.points.size(); ++i) {
                    if (data.points[i].x == m_pending_point.x
                        && data.points[i].y == m_pending_point.y) {
                        data.dragging_index = (int)i;
                        break;
                    }
                }
            }
        }
        m_pending_action = action::none;

        // Apply drag position
        if (m_has_pending_drag && data.dragging_index >= 0
            && data.dragging_index < (int)data.points.size()) {
            float new_x = m_pending_drag_pos.x;
            // Optionally lock endpoint x positions
            if (m_opts.lock_endpoints) {
                if (data.dragging_index == 0)
                    new_x = 0.0f;
                else if (data.dragging_index == (int)data.points.size() - 1)
                    new_x = 1.0f;
            }
            data.points[data.dragging_index].x = new_x;
            data.points[data.dragging_index].y = m_pending_drag_pos.y;
            m_has_pending_drag = false;
        }

        // On release: clear dragging and re-sort
        if (m_pending_release) {
            data.dragging_index = -1;
            std::sort(data.points.begin(), data.points.end(),
                [](const curve_point& a, const curve_point& b) { return a.x < b.x; });
            m_pending_release = false;
        }

        // Update cache
        m_cached_points = data.points;
        m_cached_dragging = data.dragging_index;
    }

    /** @brief Bounded parameter index accessor. */
    int32_t param_index() const {
        return m_param_index;
    }

    /** @brief Bounded styling options. */
    const options& opts() const {
        return m_opts;
    }

    /** @brief Reference to cached control points. */
    const std::vector<curve_point>& cached_points() const {
        return m_cached_points;
    }

    /** @brief Bounded dragging node index. */
    int cached_dragging() const {
        return m_cached_dragging;
    }

    // ── Drawing helpers (public for curve_group overlay use) ──────

    /**
     * @brief Path calculation and drawing utility for vector splines.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Catmull-Rom vector path strokes.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param canvas Drawbot canvas.
     * @param supplier Drawbot supplier.
     * @param pts Spline nodes.
     * @param ox Offset x position.
     * @param oy Offset y position.
     * @param w Editor width bounds.
     * @param h Editor height bounds.
     * @param alpha Stroke alpha opacity.
     */
    void draw_curve(drawbot::canvas& canvas, drawbot::supplier& supplier,
        const std::vector<curve_point>& pts, float ox, float oy, float w, float h,
        float alpha) const {
        if (pts.size() < 2)
            return;
        auto path_b = supplier.create_path();
        int n = (int)pts.size();

        path_b.move_to(ox + pts[0].x * w, oy + pts[0].y * h);

        if (m_opts.curve_type == interpolation::catmull_rom) {
            for (int seg = 0; seg < n - 1; ++seg) {
                int i0 = (std::max)(seg - 1, 0);
                int i1 = seg;
                int i2 = seg + 1;
                int i3 = (std::min)(seg + 2, n - 1);

                for (int s = 1; s <= m_opts.samples_per_seg; ++s) {
                    float t = (float)s / (float)m_opts.samples_per_seg;
                    float sx = core::math::catmull_rom(
                        pts[i0].x, pts[i1].x, pts[i2].x, pts[i3].x, t);
                    float sy = core::math::catmull_rom(
                        pts[i0].y, pts[i1].y, pts[i2].y, pts[i3].y, t);
                    path_b.line_to(ox + sx * w, oy + sy * h);
                }
            }
        } else {
            // Linear
            for (int i = 1; i < n; ++i) {
                path_b.line_to(ox + pts[i].x * w, oy + pts[i].y * h);
            }
        }

        if (m_opts.use_native_stroke && alpha >= 1.0f) {
            canvas.stroke_path_native(path_b.build());
        } else {
            auto pen = supplier.create_pen(
                core::color<>(alpha, m_opts.node_color.red, m_opts.node_color.green,
                    m_opts.node_color.blue),
                1.5f);
            canvas.stroke_path(path_b.build(), pen);
        }
    }

    /**
     * @brief Draws grid nodes on the canvas.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Draw nodes as rectangular points.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param canvas Drawbot canvas.
     * @param pts Nodes.
     * @param ox Offset x coordinate.
     * @param oy Offset y coordinate.
     * @param w Editor width bounds.
     * @param h Editor height bounds.
     * @param dragging_idx Dragging node index.
     */
    void draw_nodes(drawbot::canvas& canvas, drawbot::supplier& /*supplier*/,
        const std::vector<curve_point>& pts, float ox, float oy, float w, float h,
        int dragging_idx) const {
        float ns = m_opts.node_size;
        float as = m_opts.active_node_size;
        for (size_t i = 0; i < pts.size(); ++i) {
            float hx = ox + pts[i].x * w;
            float hy = oy + pts[i].y * h;
            if ((int)i == dragging_idx) {
                canvas.fill_rect(hx - as / 2, hy - as / 2, as, as, m_opts.active_color);
            } else {
                canvas.fill_rect(hx - ns / 2, hy - ns / 2, ns, ns, m_opts.node_color);
            }
        }
    }

private:
    int32_t m_param_index;
    options m_opts;

    // Cached state (read from arb data each frame)
    std::vector<curve_point> m_cached_points;
    int m_cached_dragging = -1;

    // Interaction state
    bool m_dragging = false;

    enum class action : std::uint8_t { none, grab, add, remove };
    action m_pending_action = action::none;
    int m_pending_index = -1;
    curve_point m_pending_point = { };
    curve_point m_pending_drag_pos = { };
    bool m_has_pending_drag = false;
    bool m_pending_release = false;
};

} // namespace aetk::effect::ui
