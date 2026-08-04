#pragma once

#include <aetk/core/locale_utils.hpp>
#include <aetk/core/math.hpp>
#include <aetk/effect/context/context.hpp>
#include <aetk/effect/params/arb_traits.hpp>
#include <aetk/effect/params/serialization.hpp>
#include <aetk/effect/ui/widget.hpp>
#include <aetk/ui/theme.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace aetk::effect::ui {

// ══════════════════════════════════════════════════════════════════════
//  HUD Shape Point & Data
// ══════════════════════════════════════════════════════════════════════

struct hud_shape_point {
  float x = 0.0f; // Range: [-1.0, 1.0]
  float y = 0.0f; // Range: [-1.0, 1.0]

  template <typename Archive> void serialize(Archive &ar) { ar & x & y; }

  bool operator==(const hud_shape_point &other) const {
    return x == other.x && y == other.y;
  }
};

struct hud_shape_data {
  std::vector<hud_shape_point> points;
  int dragging_index = -1;

  hud_shape_data() {
    // Default to a 4-point rectangle
    points.push_back({-0.7f, -0.7f});
    points.push_back({0.7f, -0.7f});
    points.push_back({0.7f, 0.7f});
    points.push_back({-0.7f, 0.7f});
    dragging_index = -1;
  }

  template <typename Archive> void serialize(Archive &ar) {
    ar & points & dragging_index;
  }

  bool operator==(const hud_shape_data &other) const {
    return points == other.points && dragging_index == other.dragging_index;
  }

  bool operator!=(const hud_shape_data &other) const {
    return !(*this == other);
  }
};

// Rounded rectangle path helper for buttons
inline drawbot::path create_rounded_rect_path_local(drawbot::supplier &supplier,
                                                    float x, float y, float w,
                                                    float h, float r) {
  auto builder = supplier.create_path();
  builder.move_to(x + r, y);
  builder.line_to(x + w - r, y);
  builder.bezier_to(core::vec2{x + w - r / 2, y}, core::vec2{x + w, y + r / 2},
                    core::vec2{x + w, y + r});
  builder.line_to(x + w, y + h - r);
  builder.bezier_to(core::vec2{x + w, y + h - r / 2},
                    core::vec2{x + w - r / 2, y + h},
                    core::vec2{x + w - r, y + h});
  builder.line_to(x + r, y + h);
  builder.bezier_to(core::vec2{x + r / 2, y + h}, core::vec2{x, y + h - r / 2},
                    core::vec2{x, y + h - r});
  builder.line_to(x, y + r);
  builder.bezier_to(core::vec2{x, y + r / 2}, core::vec2{x + r / 2, y},
                    core::vec2{x + r, y});
  builder.close();
  return builder.build();
}

// ══════════════════════════════════════════════════════════════════════
//  Interactive Shape Designer Widget
// ══════════════════════════════════════════════════════════════════════

class shape_designer : public widget {
public:
  using data_type = hud_shape_data;

  static data_type get_default_data() { return data_type(); }

  int32_t m_param_index = 0;
  hud_shape_data m_shape;

  // Interaction states
  bool m_dragging_node = false;
  int m_hovered_button = -1;
  int m_hovered_node = -1;

  enum class action_type { none, grab, add, remove, preset };
  action_type m_pending_action = action_type::none;
  int m_pending_index = -1;
  int m_pending_preset = -1;
  hud_shape_point m_pending_point = {};
  hud_shape_point m_pending_drag_pos = {};
  bool m_has_pending_drag = false;
  bool m_pending_release = false;

  shape_designer(int32_t param_idx) : m_param_index(param_idx) {
    layout.min_width = 300.0f;
  }

  virtual core::vec2 measure_impl(float avail_w, float /*avail_h*/) override {
    return {(std::max)(avail_w, 300.0f), 160.0f};
  }

  virtual void paint_impl(drawbot::canvas &canvas, drawbot::supplier &supplier,
                          const theme &t) override {
    // 1. Fill background
    canvas.fill_rect(bounds.x, bounds.y, bounds.w, bounds.h, t.bg);

    // 2. Compute grid canvas layout (centered on the left side)
    float grid_size = 130.0f;
    float canvas_cx = bounds.x + 10.0f + grid_size / 2.0f;
    float canvas_cy = bounds.y + 15.0f + grid_size / 2.0f;
    float grid_x = canvas_cx - grid_size / 2.0f;
    float grid_y = canvas_cy - grid_size / 2.0f;

    // Draw grid background
    canvas.fill_rect(grid_x, grid_y, grid_size, grid_size, t.bg_light);

    // Draw grid lines
    auto grid_pen = supplier.create_pen(t.border, 1.0f);
    // Center crosshairs
    canvas.stroke_path(supplier.create_path()
                           .move_to(canvas_cx, grid_y)
                           .line_to(canvas_cx, grid_y + grid_size)
                           .build(),
                       grid_pen);
    canvas.stroke_path(supplier.create_path()
                           .move_to(grid_x, canvas_cy)
                           .line_to(grid_x + grid_size, canvas_cy)
                           .build(),
                       grid_pen);

    // Map shape coordinates to pixels
    auto to_pixels = [&](float nx, float ny) -> core::vec2 {
      float px = canvas_cx + nx * (grid_size / 2.0f);
      float py = canvas_cy + ny * (grid_size / 2.0f);
      return {px, py};
    };

    // Draw shape polygon outline
    if (m_shape.points.size() >= 3) {
      auto path_b = supplier.create_path();
      core::vec2 start_pt = to_pixels(m_shape.points[0].x, m_shape.points[0].y);
      path_b.move_to(start_pt.x, start_pt.y);
      for (size_t i = 1; i < m_shape.points.size(); ++i) {
        core::vec2 p = to_pixels(m_shape.points[i].x, m_shape.points[i].y);
        path_b.line_to(p.x, p.y);
      }
      path_b.close();

      auto shape_pen = supplier.create_pen(t.accent, 2.0f);
      canvas.stroke_path(path_b.build(), shape_pen);
    }

    // Draw vertices (nodes)
    float node_radius = 4.0f;
    for (size_t i = 0; i < m_shape.points.size(); ++i) {
      core::vec2 p = to_pixels(m_shape.points[i].x, m_shape.points[i].y);
      core::color<> n_color = t.accent;

      if (m_shape.dragging_index == (int)i) {
        n_color =
            core::color<>(1.0f, 1.0f, 0.8f, 0.0f); // Bright yellow for dragging
      } else if (m_hovered_node == (int)i) {
        n_color = core::color<>(1.0f, 1.0f, 1.0f, 1.0f); // White for hover
      }

      canvas.fill_rect(p.x - node_radius, p.y - node_radius, node_radius * 2.0f,
                       node_radius * 2.0f, n_color);
      auto node_border_path =
          supplier.create_path()
              .add_rect(p.x - node_radius, p.y - node_radius,
                        node_radius * 2.0f, node_radius * 2.0f)
              .build();
      canvas.stroke_path(node_border_path, supplier.create_pen(t.border, 1.0f));
    }

    // 3. Render Title & Preset Buttons on the right
    float btn_x = bounds.x + 165.0f;
    float btn_w = bounds.w - 175.0f;

    auto font_title = supplier.create_font(9.0f);
    auto font_btn = supplier.create_font(10.0f);
    auto brush_text = supplier.create_brush(t.text_dim);

    canvas.draw_text("VECTOR PRESETS", font_title, brush_text,
                     core::vec2(btn_x + 2.0f, bounds.y + 16.0f));

    const char *labels[] = {"Rectangle", "Ellipse", "Hexagon", "Reticle"};
    for (int i = 0; i < 4; ++i) {
      float btn_y = bounds.y + 24.0f + i * 30.0f;
      bool hovered = (m_hovered_button == i);

      core::color<> bg_color = hovered ? t.bg_light : t.bg;
      core::color<> border_color = hovered ? t.accent : t.border;
      core::color<> text_color = hovered ? t.text : t.text_dim;

      auto btn_path = create_rounded_rect_path_local(supplier, btn_x, btn_y,
                                                     btn_w, 24.0f, 4.0f);
      canvas.fill_path(btn_path, supplier.create_brush(bg_color));
      canvas.stroke_path(btn_path, supplier.create_pen(border_color, 1.0f));

      canvas.draw_text(labels[i], font_btn, supplier.create_brush(text_color),
                       core::vec2(btn_x + 10.0f, btn_y + 16.0f));
    }
  }

  // Math helper to calculate distance from point to segment
  static float distance_to_segment_local(core::vec2 C, core::vec2 A,
                                         core::vec2 B, float &out_t) {
    core::vec2 AB = B - A;
    core::vec2 AC = C - A;
    float ab_len_sq = AB.x * AB.x + AB.y * AB.y;
    if (ab_len_sq < 1e-6f) {
      out_t = 0.0f;
      return std::sqrt(AC.x * AC.x + AC.y * AC.y);
    }
    float t = (AC.x * AB.x + AC.y * AB.y) / ab_len_sq;
    t = std::clamp(t, 0.0f, 1.0f);
    out_t = t;
    core::vec2 proj = A + AB * t;
    float dx = C.x - proj.x;
    float dy = C.y - proj.y;
    return std::sqrt(dx * dx + dy * dy);
  }

  virtual bool on_click_impl(float lx, float ly, uint32_t mods) override {
    // 1. Check preset buttons click
    float btn_x = bounds.x + 165.0f;
    float btn_w = bounds.w - 175.0f;

    for (int i = 0; i < 4; ++i) {
      float btn_y = bounds.y + 24.0f + i * 30.0f;
      if (lx >= btn_x && lx <= btn_x + btn_w && ly >= btn_y &&
          ly <= btn_y + 24.0f) {
        m_pending_action = action_type::preset;
        m_pending_preset = i;
        return true;
      }
    }

    // 2. Check grid interactions
    float grid_size = 130.0f;
    float canvas_cx = bounds.x + 10.0f + grid_size / 2.0f;
    float canvas_cy = bounds.y + 15.0f + grid_size / 2.0f;
    float grid_x = canvas_cx - grid_size / 2.0f;
    float grid_y = canvas_cy - grid_size / 2.0f;

    if (lx < grid_x || lx > grid_x + grid_size || ly < grid_y ||
        ly > grid_y + grid_size) {
      return false;
    }

    auto to_pixels = [&](float nx, float ny) -> core::vec2 {
      float px = canvas_cx + nx * (grid_size / 2.0f);
      float py = canvas_cy + ny * (grid_size / 2.0f);
      return {px, py};
    };

    // Check if click hit an existing vertex node
    float hit_radius = 8.0f;
    int hit_index = -1;
    for (size_t i = 0; i < m_shape.points.size(); ++i) {
      core::vec2 p = to_pixels(m_shape.points[i].x, m_shape.points[i].y);
      float dx = lx - p.x;
      float dy = ly - p.y;
      if (std::sqrt(dx * dx + dy * dy) <= hit_radius) {
        hit_index = (int)i;
        break;
      }
    }

    bool is_ctrl = (mods & PF_Mod_CMD_CTRL_KEY) != 0;

    if (hit_index != -1) {
      if (is_ctrl) {
        // Delete point (minimum of 3 points)
        if (m_shape.points.size() > 3) {
          m_pending_action = action_type::remove;
          m_pending_index = hit_index;
          m_dragging_node = true;
          return true;
        }
      } else {
        // Grab point to drag
        m_pending_action = action_type::grab;
        m_pending_index = hit_index;
        m_dragging_node = true;
        return true;
      }
    } else {
      // Click to insert a new node on the closest segment
      core::vec2 click_pt = {lx, ly};
      float min_dist = 99999.0f;
      int insert_idx = -1;
      float best_t = 0.0f;

      size_t num_pts = m_shape.points.size();
      for (size_t i = 0; i < num_pts; ++i) {
        core::vec2 A = to_pixels(m_shape.points[i].x, m_shape.points[i].y);
        core::vec2 B = to_pixels(m_shape.points[(i + 1) % num_pts].x,
                                 m_shape.points[(i + 1) % num_pts].y);
        float t = 0.0f;
        float dist = distance_to_segment_local(click_pt, A, B, t);
        if (dist < min_dist) {
          min_dist = dist;
          insert_idx = (int)i;
          best_t = t;
        }
      }

      // Enforce segment hit within 16 pixels
      if (insert_idx != -1 && min_dist <= 16.0f) {
        // Compute new point coordinates in normalized space
        hud_shape_point p_a = m_shape.points[insert_idx];
        hud_shape_point p_b = m_shape.points[(insert_idx + 1) % num_pts];
        hud_shape_point new_p;
        new_p.x = p_a.x + best_t * (p_b.x - p_a.x);
        new_p.y = p_a.y + best_t * (p_b.y - p_a.y);

        m_pending_action = action_type::add;
        m_pending_index = insert_idx + 1;
        m_pending_point = new_p;
        m_dragging_node = true;
        return true;
      }
    }

    return false;
  }

  virtual bool on_drag_impl(float lx, float ly, uint32_t /*mods*/) override {
    if (!m_dragging_node) {
      return false;
    }

    float grid_size = 130.0f;
    float canvas_cx = bounds.x + 10.0f + grid_size / 2.0f;
    float canvas_cy = bounds.y + 15.0f + grid_size / 2.0f;

    // Map back to normalized [-1.0, 1.0] coordinates
    float nx = (lx - canvas_cx) / (grid_size / 2.0f);
    float ny = (ly - canvas_cy) / (grid_size / 2.0f);

    m_pending_drag_pos.x = std::clamp(nx, -1.0f, 1.0f);
    m_pending_drag_pos.y = std::clamp(ny, -1.0f, 1.0f);
    m_has_pending_drag = true;
    return true;
  }

  virtual void on_release_impl() override {
    m_dragging_node = false;
    m_pending_release = true;
  }

  virtual bool on_hover_move_impl(float lx, float ly) override {
    int old_button = m_hovered_button;
    int old_node = m_hovered_node;

    m_hovered_button = -1;
    m_hovered_node = -1;

    // Check buttons hover
    float btn_x = bounds.x + 165.0f;
    float btn_w = bounds.w - 175.0f;
    for (int i = 0; i < 4; ++i) {
      float btn_y = bounds.y + 24.0f + i * 30.0f;
      if (lx >= btn_x && lx <= btn_x + btn_w && ly >= btn_y &&
          ly <= btn_y + 24.0f) {
        m_hovered_button = i;
        break;
      }
    }

    // Check nodes hover
    if (m_hovered_button == -1) {
      float grid_size = 130.0f;
      float canvas_cx = bounds.x + 10.0f + grid_size / 2.0f;
      float canvas_cy = bounds.y + 15.0f + grid_size / 2.0f;

      auto to_pixels = [&](float nx, float ny) -> core::vec2 {
        return {canvas_cx + nx * (grid_size / 2.0f),
                canvas_cy + ny * (grid_size / 2.0f)};
      };

      float hit_radius = 8.0f;
      for (size_t i = 0; i < m_shape.points.size(); ++i) {
        core::vec2 p = to_pixels(m_shape.points[i].x, m_shape.points[i].y);
        float dx = lx - p.x;
        float dy = ly - p.y;
        if (std::sqrt(dx * dx + dy * dy) <= hit_radius) {
          m_hovered_node = (int)i;
          break;
        }
      }
    }

    return m_hovered_button != old_button || m_hovered_node != old_node;
  }

  virtual void on_hover_exit_impl() override {
    m_hovered_button = -1;
    m_hovered_node = -1;
  }

  virtual void sync_state_impl(const interaction_context &ctx) override {
    if (m_param_index <= 0)
      return;
    auto lock = ctx.arb_data<hud_shape_data>(m_param_index);
    if (lock) {
      m_shape = *lock;
    }
  }

  virtual void commit_state_impl(const interaction_context &ctx) override {
    if (m_param_index <= 0)
      return;
    auto lock = ctx.arb_data<hud_shape_data>(m_param_index);
    if (lock) {
      bool changed = false;
      apply_to(*lock, changed);
      if (changed) {
        lock.mark_changed();
        ctx.mark_param_changed(m_param_index);
      }
    }
  }

  virtual int32_t cursor_type_impl() const override { return 0; }

  void apply_to(hud_shape_data &data, bool &out_changed) {
    out_changed = false;

    if (m_pending_action == action_type::remove) {
      if (m_pending_index >= 0 && m_pending_index < (int)data.points.size() &&
          data.points.size() > 3) {
        data.points.erase(data.points.begin() + m_pending_index);
        out_changed = true;
      }
      data.dragging_index = -1;
      m_pending_action = action_type::none;
    } else if (m_pending_action == action_type::grab) {
      data.dragging_index = m_pending_index;
      m_pending_action = action_type::none;
      out_changed = true;
    } else if (m_pending_action == action_type::add) {
      if (data.points.size() < 32) {
        data.points.insert(data.points.begin() + m_pending_index,
                           m_pending_point);
        data.dragging_index = m_pending_index;
        out_changed = true;
      }
      m_pending_action = action_type::none;
    } else if (m_pending_action == action_type::preset) {
      apply_preset_points(data.points, m_pending_preset);
      data.dragging_index = -1;
      m_pending_action = action_type::none;
      out_changed = true;
    }

    if (m_has_pending_drag && data.dragging_index >= 0 &&
        data.dragging_index < (int)data.points.size()) {
      float nx = std::clamp(m_pending_drag_pos.x, -1.0f, 1.0f);
      float ny = std::clamp(m_pending_drag_pos.y, -1.0f, 1.0f);
      if (data.points[data.dragging_index].x != nx ||
          data.points[data.dragging_index].y != ny) {
        data.points[data.dragging_index].x = nx;
        data.points[data.dragging_index].y = ny;
        out_changed = true;
      }
      m_has_pending_drag = false;
    }

    if (m_pending_release) {
      if (data.dragging_index != -1) {
        data.dragging_index = -1;
        out_changed = true;
      }
      m_pending_release = false;
    }

    m_shape = data;
  }

  static void apply_preset_points(std::vector<hud_shape_point> &points,
                                  int preset_idx) {
    points.clear();

    if (preset_idx == 0) {
      // Rectangle
      points.push_back({-0.7f, -0.7f});
      points.push_back({0.7f, -0.7f});
      points.push_back({0.7f, 0.7f});
      points.push_back({-0.7f, 0.7f});
    } else if (preset_idx == 1) {
      // Ellipse (16 points)
      constexpr int n_pts = 16;
      constexpr float pi = 3.14159265f;
      for (int i = 0; i < n_pts; ++i) {
        float angle = i * 2.0f * pi / n_pts;
        points.push_back({0.7f * std::cos(angle), 0.7f * std::sin(angle)});
      }
    } else if (preset_idx == 2) {
      // Hexagon (6 points)
      constexpr int n_pts = 6;
      constexpr float pi = 3.14159265f;
      for (int i = 0; i < n_pts; ++i) {
        float angle = i * 2.0f * pi / n_pts;
        points.push_back({0.7f * std::cos(angle), 0.7f * std::sin(angle)});
      }
    } else if (preset_idx == 3) {
      // Reticle (4-pointed star outline)
      points.push_back({0.0f, -0.8f});
      points.push_back({0.15f, -0.2f});
      points.push_back({0.8f, 0.0f});
      points.push_back({0.15f, 0.2f});
      points.push_back({0.0f, 0.8f});
      points.push_back({-0.15f, 0.2f});
      points.push_back({-0.8f, 0.0f});
      points.push_back({-0.15f, -0.2f});
    }
  }
};

} // namespace aetk::effect::ui

namespace aetk::effect {

template <> struct arb_traits<ui::hud_shape_data> {
  using T = ui::hud_shape_data;

  static void init(T *ptr) { new (ptr) T(); }
  static void dispose(T *ptr) { ptr->~T(); }
  static void copy(T *dst, const T *src) { new (dst) T(*src); }
  static size_t flat_size(const T *ptr) {
    serialization::size_archive ar;
    const_cast<T *>(ptr)->serialize(ar);
    return ar.size();
  }

  static void flatten(const T *ptr, void *buffer, size_t size) {
    serialization::binary_oarchive ar(buffer, size);
    const_cast<T *>(ptr)->serialize(ar);
  }

  static void unflatten(T *ptr, const void *buffer, size_t size) {
    serialization::binary_iarchive ar(buffer, size);
    new (ptr) T();
    ptr->serialize(ar);
  }

  static void interpolate(T *dst, const T *left, const T *right, double t) {
    new (dst) T();
    dst->points.clear();
    dst->dragging_index = -1;

    if (left->points.empty() && right->points.empty()) {
      return;
    }
    if (left->points.empty()) {
      dst->points = right->points;
      return;
    }
    if (right->points.empty()) {
      dst->points = left->points;
      return;
    }

    // If sizes match, direct linear interpolation
    if (left->points.size() == right->points.size()) {
      dst->points.resize(left->points.size());
      for (size_t i = 0; i < left->points.size(); ++i) {
        dst->points[i].x =
            left->points[i].x +
            static_cast<float>(t * (right->points[i].x - left->points[i].x));
        dst->points[i].y =
            left->points[i].y +
            static_cast<float>(t * (right->points[i].y - left->points[i].y));
      }
    } else {
      // Perimeter-resampling morph algorithm
      size_t n_left = left->points.size();
      std::vector<float> dist_left(n_left + 1, 0.0f);
      for (size_t i = 0; i < n_left; ++i) {
        size_t next = (i + 1) % n_left;
        float dx = left->points[next].x - left->points[i].x;
        float dy = left->points[next].y - left->points[i].y;
        dist_left[i + 1] = dist_left[i] + std::sqrt(dx * dx + dy * dy);
      }
      float total_dist_left = dist_left[n_left];

      size_t n_right = right->points.size();
      std::vector<float> dist_right(n_right + 1, 0.0f);
      for (size_t i = 0; i < n_right; ++i) {
        size_t next = (i + 1) % n_right;
        float dx = right->points[next].x - right->points[i].x;
        float dy = right->points[next].y - right->points[i].y;
        dist_right[i + 1] = dist_right[i] + std::sqrt(dx * dx + dy * dy);
      }
      float total_dist_right = dist_right[n_right];

      size_t n_dst = (std::max)(n_left, n_right);
      dst->points.resize(n_dst);

      auto sample_shape = [](const std::vector<ui::hud_shape_point> &pts,
                             const std::vector<float> &dists, float total_dist,
                             float s) -> ui::hud_shape_point {
        if (total_dist <= 1e-5f) {
          return pts[0];
        }
        float target_dist = s * total_dist;
        size_t num_pts = pts.size();
        size_t idx = 0;
        for (size_t i = 0; i < num_pts; ++i) {
          if (target_dist >= dists[i] && target_dist <= dists[i + 1]) {
            idx = i;
            break;
          }
        }
        float seg_len = dists[idx + 1] - dists[idx];
        float local_t = 0.0f;
        if (seg_len > 1e-5f) {
          local_t = (target_dist - dists[idx]) / seg_len;
        }
        size_t next = (idx + 1) % num_pts;
        ui::hud_shape_point p;
        p.x = pts[idx].x + local_t * (pts[next].x - pts[idx].x);
        p.y = pts[idx].y + local_t * (pts[next].y - pts[idx].y);
        return p;
      };

      for (size_t i = 0; i < n_dst; ++i) {
        float s = static_cast<float>(i) / static_cast<float>(n_dst);
        ui::hud_shape_point p_l =
            sample_shape(left->points, dist_left, total_dist_left, s);
        ui::hud_shape_point p_r =
            sample_shape(right->points, dist_right, total_dist_right, s);

        dst->points[i].x = p_l.x + static_cast<float>(t * (p_r.x - p_l.x));
        dst->points[i].y = p_l.y + static_cast<float>(t * (p_r.y - p_l.y));
      }
    }
  }

  static void print(const T *ptr, char *str, size_t max_len) {
    if (max_len > 0) {
      std::string out = "HUDShape:";
      for (size_t i = 0; i < ptr->points.size(); ++i) {
        char pt_buf[64];
        aetk::core::c_snprintf(pt_buf, sizeof(pt_buf), " (%.4f, %.4f)%s", 
                 ptr->points[i].x, ptr->points[i].y,
                 (i + 1 < ptr->points.size() ? "," : ""));
        out += pt_buf;
      }
      std::strncpy(str, out.c_str(), max_len);
      str[max_len - 1] = '\0';
    }
  }

  static size_t print_size(const T *ptr) {
    return 32 + ptr->points.size() * 32;
  }

  static bool compare(const T *a, const T *b) {
    return *a == *b;
  }

  static bool scan(T *ptr, const char *str) {
    if (std::strncmp(str, "HUDShape:", 9) != 0) return false;
    
    std::vector<ui::hud_shape_point> pts;
    const char *p = str + 9;
    float x, y;
    while (true) {
      p = std::strchr(p, '(');
      if (!p) break;
      if (aetk::core::c_sscanf(p, "(%f, %f)", &x, &y) == 2) {
        pts.push_back({x, y});
      }
      p++;
    }
    if (!pts.empty()) {
      ptr->points = std::move(pts);
      ptr->dragging_index = -1;
      return true;
    }
    return false;
  }
};

} // namespace aetk::effect
