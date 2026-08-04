#pragma once

#include <aetk/core/types.hpp>
#include <cmath>
#include <vector>

namespace aetk::core::math {

// ══════════════════════════════════════════════════════════════════════
//  Catmull-Rom Spline Interpolation
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Evaluate a Catmull-Rom spline segment between p1 and p2.
 *
 * @details Given 4 control points (p0, p1, p2, p3), evaluates the smooth
 * curve at parameter t ∈ [0, 1] between p1 and p2.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Provides high-level Catmull-Rom spline calculations, allowing standard curve editors or lookups to run smoothly without procedural logic blocks.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 *
 * @param p0 Leftmost virtual control point.
 * @param p1 Left segment boundary point.
 * @param p2 Right segment boundary point.
 * @param p3 Rightmost virtual control point.
 * @param t Linear offset parameter in range [0, 1].
 * @return Interpolated float value.
 */
inline float catmull_rom(float p0, float p1, float p2, float p3, float t) {
  return 0.5f * ((2.0f * p1) + (-p0 + p2) * t +
                 (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t * t +
                 (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t * t * t);
}

/**
 * @brief Evaluate a full Catmull-Rom spline at a given x position.
 *
 * @details Points must be sorted by x. Returns interpolated y for x ∈ [0, 1].
 * Works with any type that has .x and .y float members.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Evaluates sorted point collections using Catmull-Rom splines, providing automated boundary clamping.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 *
 * @tparam PointT  Point type with float x, y members
 * @param pts      Sorted array of control points
 * @param x        Query position in [0, 1]
 * @return         Interpolated y value
 */
template <typename PointT>
float evaluate_catmull_rom(const std::vector<PointT> &pts, float x) {
  if (pts.empty())
    return 0.0f;
  if (pts.size() == 1)
    return pts[0].y;

  x = (std::max)(0.0f, (std::min)(1.0f, x));

  // Find the segment containing x
  int n = (int)pts.size();
  int seg = 0;
  for (int i = 0; i < n - 1; ++i) {
    if (x >= pts[i].x && x <= pts[i + 1].x) {
      seg = i;
      break;
    }
    if (i == n - 2)
      seg = i; // clamp to last segment
  }

  // Boundary-clamped indices
  int i0 = (std::max)(seg - 1, 0);
  int i1 = seg;
  int i2 = (std::min)(seg + 1, n - 1);
  int i3 = (std::min)(seg + 2, n - 1);

  // Compute local t within this segment
  float seg_width = pts[i2].x - pts[i1].x;
  float t = (seg_width > 0.0001f) ? (x - pts[i1].x) / seg_width : 0.0f;

  return catmull_rom(pts[i0].y, pts[i1].y, pts[i2].y, pts[i3].y, t);
}

/**
 * @brief Evaluate a linear interpolation at a given x position.
 *
 * @details Points must be sorted by x. Returns linearly interpolated y.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Provides simple, exception-safe piecewise linear interpolation over sorted point arrays.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 *
 * @tparam PointT Point type with float x, y members
 * @param pts Sorted control points array.
 * @param x Query location in range [0, 1].
 * @return Interpolated float value.
 */
template <typename PointT>
float evaluate_linear(const std::vector<PointT> &pts, float x) {
  if (pts.empty())
    return 0.0f;
  if (pts.size() == 1)
    return pts[0].y;

  x = (std::max)(0.0f, (std::min)(1.0f, x));
  int n = (int)pts.size();

  // Clamp to endpoints
  if (x <= pts[0].x)
    return pts[0].y;
  if (x >= pts[n - 1].x)
    return pts[n - 1].y;

  // Find segment
  for (int i = 0; i < n - 1; ++i) {
    if (x >= pts[i].x && x <= pts[i + 1].x) {
      float seg_w = pts[i + 1].x - pts[i].x;
      float t = (seg_w > 0.0001f) ? (x - pts[i].x) / seg_w : 0.0f;
      return pts[i].y + t * (pts[i + 1].y - pts[i].y);
    }
  }
  return pts[n - 1].y;
}

// ══════════════════════════════════════════════════════════════════════
//  Graph Topology
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Disjoint Set Union (DSU) graph data structure.
 *
 * @details Used for cycle detection and connected component tracking.
 * Commonly used to build Minimum Spanning Trees (MST) via Kruskal's algorithm.
 */
struct DSU {
    std::vector<int> parent;
    
    DSU(int n) {
        parent.resize(n);
        for (int i = 0; i < n; i++) parent[i] = i;
    }
    
    int find(int i) {
        if (parent[i] == i)
            return i;
        return parent[i] = find(parent[i]);
    }
    
    bool unite(int i, int j) {
        int root_i = find(i);
        int root_j = find(j);
        if (root_i != root_j) {
            parent[root_i] = root_j;
            return true;
        }
        return false;
    }
};

} // namespace aetk::core::math
