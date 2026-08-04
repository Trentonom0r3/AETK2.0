#pragma once

#include <aetk/core/types.hpp>
#include <aetk/effect/pixel/smart_world.hpp>
#include <aetk/effect/context/context.hpp>
#include <aetk/effect/pixel/tensor_view.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <memory>

#if defined(AETK_ENABLE_CUDA)
#include <aetk/effect/draw/cuda_canvas.hpp>
#include <cuda_runtime.h>
#include <stdexcept>
#endif

namespace aetk::effect::draw {

#if defined(AETK_ENABLE_CUDA)
class cuda_buffer {
    void* m_ptr = nullptr;
    size_t m_size = 0;

public:
    cuda_buffer() = default;
    ~cuda_buffer() {
        if (m_ptr) {
            cudaFree(m_ptr);
        }
    }

    cuda_buffer(const cuda_buffer&) = delete;
    cuda_buffer& operator=(const cuda_buffer&) = delete;

    cuda_buffer(cuda_buffer&& other) noexcept
        : m_ptr(other.m_ptr)
        , m_size(other.m_size) {
        other.m_ptr = nullptr;
        other.m_size = 0;
    }

    cuda_buffer& operator=(cuda_buffer&& other) noexcept {
        if (this != &other) {
            if (m_ptr) {
                cudaFree(m_ptr);
            }
            m_ptr = other.m_ptr;
            m_size = other.m_size;
            other.m_ptr = nullptr;
            other.m_size = 0;
        }
        return *this;
    }

    void reserve(size_t bytes) {
        if (m_size >= bytes) return;

        if (m_ptr) {
            cudaFree(m_ptr);
            m_ptr = nullptr;
            m_size = 0;
        }

        cudaError_t err = cudaMalloc(&m_ptr, bytes);
        if (err == cudaSuccess) {
            m_size = bytes;
        } else {
            m_ptr = nullptr;
            m_size = 0;
            throw std::runtime_error("cudaMalloc failed");
        }
    }

    void* data() const { return m_ptr; }
    size_t size() const { return m_size; }
};
#endif

template <pixel_range Range = pixel_range::tkfloat>
struct batched_line {
    float x0, y0;
    float x1, y1;
    aetk::core::color<Range> color;
    int thickness;
};

template <pixel_range Range = pixel_range::tkfloat>
struct batched_triangle {
    float x0, y0;
    float x1, y1;
    float x2, y2;
    aetk::core::color<Range> color;
};

inline float get_pixel_aspect_ratio(const smart_world &world) {
  if (world.in_data_ptr()) {
    auto &ar = world.in_data_ptr()->pixel_aspect_ratio;
    if (ar.den > 0) {
      return static_cast<float>(ar.num) / static_cast<float>(ar.den);
    }
  }
  return 1.0f;
}

inline float get_downsample_scale(const smart_world &world) {
  if (world.in_data_ptr()) {
    auto &ds = world.in_data_ptr()->downsample_x;
    if (ds.den > 0) {
      float scale = static_cast<float>(ds.num) / static_cast<float>(ds.den);
      if (scale > 0.0001f) {
        return scale;
      }
    }
  }
  return 1.0f;
}

/**
 * @brief Draw a single pixel onto a smart_world with alpha blending.
 *
 * @param world Destination smart_world.
 * @param x Pixel coordinate x.
 * @param y Pixel coordinate y.
 * @param color Source color value.
 */
template <pixel_range Range = pixel_range::tkfloat>
inline void pixel(smart_world &world, int x, int y,
                  const aetk::core::color<Range> &color) {
#if defined(AETK_ENABLE_CUDA)
  if (world.is_gpu()) {
    float r = (float)color.red;
    float g = (float)color.green;
    float b = (float)color.blue;
    float a = (float)color.alpha;
    if constexpr (Range == pixel_range::tkuint8) {
      r *= (1.0f / 255.0f);
      g *= (1.0f / 255.0f);
      b *= (1.0f / 255.0f);
      a *= (1.0f / 255.0f);
    }
    cuda_draw_pixel(world.gpu_data(), world.width(), world.height(), world.rowbytes(), x, y, r, g, b, a);
    return;
  }
#endif
  if (x < 0 || x >= world.width() || y < 0 || y >= world.height())
    return;

  double src_a = color.alpha;
  if (src_a <= 0.0)
    return;

  visit_pixel_format<Range>(world.pixel_format(), world.is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
    auto *dst_row = reinterpret_cast<PixelT *>(
        reinterpret_cast<char *>(world.ptr()->data) + y * world.rowbytes());
    PixelT &d = dst_row[x];
    double opaque_val = (Range == pixel_range::tkfloat) ? 1.0 : 255.0;
    if (src_a >= opaque_val) {
      pixel_accessor<PixelT, IsBGRA, Range>::write(&d, color);
    } else {
      typename pixel_accessor<PixelT, IsBGRA, Range>::blender b(color);
      b.blend(&d);
    }
  });
}

/**
 * @brief Draw a line using Bresenham's line algorithm.
 *
 * @param world Destination smart_world.
 * @param x0 Start x.
 * @param y0 Start y.
 * @param x1 End x.
 * @param y1 End y.
 * @param color Line color.
 * @param thickness Line stroke width.
 */
template <pixel_range Range = pixel_range::tkfloat>
inline void line(smart_world &world, int x0, int y0, int x1, int y1,
                 const aetk::core::color<Range> &color, int thickness = 1) {
#if defined(AETK_ENABLE_CUDA)
  if (world.is_gpu()) {
    float r = (float)color.red;
    float g = (float)color.green;
    float b = (float)color.blue;
    float a = (float)color.alpha;
    if constexpr (Range == pixel_range::tkuint8) {
      r *= (1.0f / 255.0f);
      g *= (1.0f / 255.0f);
      b *= (1.0f / 255.0f);
      a *= (1.0f / 255.0f);
    }
    cuda_draw_line(world.gpu_data(), world.width(), world.height(), world.rowbytes(), x0, y0, x1, y1, r, g, b, a, thickness);
    return;
  }
#endif
  double src_a = color.alpha;
  if (src_a <= 0.0)
    return;

  float ds = get_downsample_scale(world);
  int scaled_thickness = (std::max)(1, (int)std::round(thickness * ds));

  int dx = std::abs(x1 - x0);
  int dy = std::abs(y1 - y0);
  int sx = (x0 < x1) ? 1 : -1;
  int sy = (y0 < y1) ? 1 : -1;

  int w = world.width();
  int h = world.height();
  int rowbytes = world.rowbytes();
  char *data = reinterpret_cast<char *>(world.ptr()->data);

  visit_pixel_format<Range>(world.pixel_format(), world.is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
    int err = dx - dy;
    int cx = x0;
    int cy = y0;
    int r = (scaled_thickness - 1) / 2;

    typename pixel_accessor<PixelT, IsBGRA, Range>::blender b(color);

    while (true) {
      for (int ty = -r; ty <= r; ty++) {
        int py = cy + ty;
        if (py >= 0 && py < h) {
          char *row_ptr = data + py * rowbytes;
          auto *dst_row = reinterpret_cast<PixelT *>(row_ptr);
          for (int tx = -r; tx <= r; tx++) {
            int px = cx + tx;
            if (px >= 0 && px < w) {
              PixelT &d = dst_row[px];
              double opaque_val = (Range == pixel_range::tkfloat) ? 1.0 : 255.0;
              if (src_a >= opaque_val) {
                pixel_accessor<PixelT, IsBGRA, Range>::write(&d, color);
              } else {
                b.blend(&d);
              }
            }
          }
        }
      }

      if (cx == x1 && cy == y1)
        break;
      int e2 = 2 * err;
      if (e2 > -dy) {
        err -= dy;
        cx += sx;
      }
      if (e2 < dx) {
        err += dx;
        cy += sy;
      }
    }
  });
}

/**
 * @brief Draw a quadratic Bezier curve (Optimized via Forward Differencing).
 */
template <pixel_range Range = pixel_range::tkfloat>
inline void bezier(smart_world &world, int x0, int y0, int cx, int cy, int x1,
                   int y1, const aetk::core::color<Range> &color, int thickness = 1) {
  const int steps = 24;

  const int shift = 16;
  const int scale = 1 << shift;

  int px = x0 * scale;
  int py = y0 * scale;

  float t_step = 1.0f / steps;
  float t_step2 = t_step * t_step;

  float dx_initial =
      2.0f * (cx - x0) * t_step + (x0 - 2.0f * cx + x1) * t_step2;
  float ddx = 2.0f * (x0 - 2.0f * cx + x1) * t_step2;

  float dy_initial =
      2.0f * (cy - y0) * t_step + (y0 - 2.0f * cy + y1) * t_step2;
  float ddy = 2.0f * (y0 - 2.0f * cy + y1) * t_step2;

  int fdx = (int)(dx_initial * scale);
  int fddx = (int)(ddx * scale);

  int fdy = (int)(dy_initial * scale);
  int fddy = (int)(ddy * scale);

  int prev_x = x0;
  int prev_y = y0;

  for (int i = 1; i <= steps; i++) {
    px += fdx;
    fdx += fddx;

    py += fdy;
    fdy += fddy;

    int curr_x = px >> shift;
    int curr_y = py >> shift;

    line<Range>(world, prev_x, prev_y, curr_x, curr_y, color, thickness);

    prev_x = curr_x;
    prev_y = curr_y;
  }
}

namespace detail {

struct stroke_char {
  int num_segments = 0;
  struct {
    float x0, y0, x1, y1;
  } segments[8];
};

inline const stroke_char &get_stroke_char(char c) {
  static stroke_char table[128];
  static bool initialized = false;
  if (!initialized) {
    auto add_seg = [](stroke_char &sc, float x0, float y0, float x1, float y1) {
      if (sc.num_segments < 8) {
        sc.segments[sc.num_segments] = {x0, y0, x1, y1};
        sc.num_segments++;
      }
    };

    // Populate A-Z
    add_seg(table['A'], 0, 6, 0, 2);
    add_seg(table['A'], 0, 2, 2, 0);
    add_seg(table['A'], 2, 0, 4, 2);
    add_seg(table['A'], 4, 2, 4, 6);
    add_seg(table['A'], 0, 3, 4, 3);

    add_seg(table['B'], 0, 0, 0, 6);
    add_seg(table['B'], 0, 0, 3, 0);
    add_seg(table['B'], 3, 0, 4, 1.5f);
    add_seg(table['B'], 4, 1.5f, 3, 3);
    add_seg(table['B'], 3, 3, 0, 3);
    add_seg(table['B'], 3, 3, 4, 4.5f);
    add_seg(table['B'], 4, 4.5f, 3, 6);
    add_seg(table['B'], 3, 6, 0, 6);

    add_seg(table['C'], 4, 0, 0, 0);
    add_seg(table['C'], 0, 0, 0, 6);
    add_seg(table['C'], 0, 6, 4, 6);

    add_seg(table['D'], 0, 0, 0, 6);
    add_seg(table['D'], 0, 0, 3, 0);
    add_seg(table['D'], 3, 0, 4, 3);
    add_seg(table['D'], 4, 3, 3, 6);
    add_seg(table['D'], 3, 6, 0, 6);

    add_seg(table['E'], 0, 0, 0, 6);
    add_seg(table['E'], 0, 0, 4, 0);
    add_seg(table['E'], 0, 3, 3, 3);
    add_seg(table['E'], 0, 6, 4, 6);

    add_seg(table['F'], 0, 0, 0, 6);
    add_seg(table['F'], 0, 0, 4, 0);
    add_seg(table['F'], 0, 3, 3, 3);

    add_seg(table['G'], 4, 0, 0, 0);
    add_seg(table['G'], 0, 0, 0, 6);
    add_seg(table['G'], 0, 6, 4, 6);
    add_seg(table['G'], 4, 6, 4, 3);
    add_seg(table['G'], 4, 3, 2, 3);

    add_seg(table['H'], 0, 0, 0, 6);
    add_seg(table['H'], 4, 0, 4, 6);
    add_seg(table['H'], 0, 3, 4, 3);

    add_seg(table['I'], 2, 0, 2, 6);
    add_seg(table['I'], 0, 0, 4, 0);
    add_seg(table['I'], 0, 6, 4, 6);

    add_seg(table['J'], 3, 0, 3, 5);
    add_seg(table['J'], 3, 5, 2, 6);
    add_seg(table['J'], 2, 6, 0, 6);
    add_seg(table['J'], 0, 6, 0, 4);

    add_seg(table['K'], 0, 0, 0, 6);
    add_seg(table['K'], 4, 0, 0, 3);
    add_seg(table['K'], 0, 3, 4, 6);

    add_seg(table['L'], 0, 0, 0, 6);
    add_seg(table['L'], 0, 6, 4, 6);

    add_seg(table['M'], 0, 6, 0, 0);
    add_seg(table['M'], 0, 0, 2, 3);
    add_seg(table['M'], 2, 3, 4, 0);
    add_seg(table['M'], 4, 0, 4, 6);

    add_seg(table['N'], 0, 6, 0, 0);
    add_seg(table['N'], 0, 0, 4, 6);
    add_seg(table['N'], 4, 6, 4, 0);

    add_seg(table['O'], 0, 0, 4, 0);
    add_seg(table['O'], 4, 0, 4, 6);
    add_seg(table['O'], 4, 6, 0, 6);
    add_seg(table['O'], 0, 6, 0, 0);

    add_seg(table['P'], 0, 6, 0, 0);
    add_seg(table['P'], 0, 0, 4, 0);
    add_seg(table['P'], 4, 0, 4, 3);
    add_seg(table['P'], 4, 3, 0, 3);

    add_seg(table['Q'], 0, 0, 4, 0);
    add_seg(table['Q'], 4, 0, 4, 6);
    add_seg(table['Q'], 4, 6, 0, 6);
    add_seg(table['Q'], 0, 6, 0, 0);
    add_seg(table['Q'], 2, 4, 4, 6);

    add_seg(table['R'], 0, 6, 0, 0);
    add_seg(table['R'], 0, 0, 4, 0);
    add_seg(table['R'], 4, 0, 4, 3);
    add_seg(table['R'], 4, 3, 0, 3);
    add_seg(table['R'], 2, 3, 4, 6);

    add_seg(table['S'], 4, 0, 0, 0);
    add_seg(table['S'], 0, 0, 0, 3);
    add_seg(table['S'], 0, 3, 4, 3);
    add_seg(table['S'], 4, 3, 4, 6);
    add_seg(table['S'], 4, 6, 0, 6);

    add_seg(table['T'], 0, 0, 4, 0);
    add_seg(table['T'], 2, 0, 2, 6);

    add_seg(table['U'], 0, 0, 0, 6);
    add_seg(table['U'], 0, 6, 4, 6);
    add_seg(table['U'], 4, 6, 4, 0);

    add_seg(table['V'], 0, 0, 2, 6);
    add_seg(table['V'], 2, 6, 4, 0);

    add_seg(table['W'], 0, 0, 0, 6);
    add_seg(table['W'], 0, 6, 2, 3);
    add_seg(table['W'], 2, 3, 4, 6);
    add_seg(table['W'], 4, 6, 4, 0);

    add_seg(table['X'], 0, 0, 4, 6);
    add_seg(table['X'], 4, 0, 0, 6);

    add_seg(table['Y'], 0, 0, 2, 3);
    add_seg(table['Y'], 4, 0, 2, 3);
    add_seg(table['Y'], 2, 3, 2, 6);

    add_seg(table['Z'], 0, 0, 4, 0);
    add_seg(table['Z'], 4, 0, 0, 6);
    add_seg(table['Z'], 0, 6, 4, 6);

    // Populate 0-9
    add_seg(table['0'], 0, 0, 4, 0);
    add_seg(table['0'], 4, 0, 4, 6);
    add_seg(table['0'], 4, 6, 0, 6);
    add_seg(table['0'], 0, 6, 0, 0);
    add_seg(table['0'], 4, 0, 0, 6);

    add_seg(table['1'], 1, 1, 2, 0);
    add_seg(table['1'], 2, 0, 2, 6);
    add_seg(table['1'], 0, 6, 4, 6);

    add_seg(table['2'], 0, 0, 4, 0);
    add_seg(table['2'], 4, 0, 4, 3);
    add_seg(table['2'], 4, 3, 0, 6);
    add_seg(table['2'], 0, 6, 4, 6);

    add_seg(table['3'], 0, 0, 4, 0);
    add_seg(table['3'], 4, 0, 4, 6);
    add_seg(table['3'], 0, 3, 4, 3);
    add_seg(table['3'], 0, 6, 4, 6);

    add_seg(table['4'], 0, 0, 0, 3);
    add_seg(table['4'], 0, 3, 4, 3);
    add_seg(table['4'], 4, 0, 4, 6);

    add_seg(table['5'], 4, 0, 0, 0);
    add_seg(table['5'], 0, 0, 0, 3);
    add_seg(table['5'], 0, 3, 4, 3);
    add_seg(table['5'], 4, 3, 4, 6);
    add_seg(table['5'], 4, 6, 0, 6);

    add_seg(table['6'], 4, 0, 0, 0);
    add_seg(table['6'], 0, 0, 0, 6);
    add_seg(table['6'], 0, 6, 4, 6);
    add_seg(table['6'], 4, 6, 4, 3);
    add_seg(table['6'], 4, 3, 0, 3);

    add_seg(table['7'], 0, 0, 4, 0);
    add_seg(table['7'], 4, 0, 1, 6);

    add_seg(table['8'], 0, 0, 4, 0);
    add_seg(table['8'], 0, 0, 0, 6);
    add_seg(table['8'], 4, 0, 4, 6);
    add_seg(table['8'], 0, 3, 4, 3);
    add_seg(table['8'], 0, 6, 4, 6);

    add_seg(table['9'], 0, 0, 4, 0);
    add_seg(table['9'], 0, 0, 0, 3);
    add_seg(table['9'], 4, 0, 4, 6);
    add_seg(table['9'], 0, 3, 4, 3);
    add_seg(table['9'], 4, 6, 0, 6);

    // Symbols
    add_seg(table[':'], 2, 1.5f, 2, 2.0f);
    add_seg(table[':'], 2, 4.0f, 2, 4.5f);
    add_seg(table['.'], 2, 5.5f, 2, 6.0f);
    add_seg(table['-'], 1, 3, 3, 3);
    add_seg(table['%'], 0, 6, 4, 0);
    add_seg(table['%'], 0, 1, 1, 1);
    add_seg(table['%'], 3, 5, 4, 5);
    add_seg(table['/'], 0, 6, 4, 0);

    initialized = true;
  }

  if (c >= 0 && c < 128) {
    return table[static_cast<int>(c)];
  }
  return table[' '];
}

} // namespace detail

template <pixel_range Range = pixel_range::tkfloat>
inline void stroke_character(smart_world &world, float x, float y, char c,
                             const aetk::core::color<Range> &color, float scale_factor = -1.0f,
                             float hud_text_size = 1.0f, int thickness = 1) {

  char upper_c = (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
  const detail::stroke_char &sc = detail::get_stroke_char(upper_c);

  float ds = get_downsample_scale(world);
  float active_scale = (scale_factor < 0.0f) ? ds : scale_factor;
  float par = get_pixel_aspect_ratio(world);
  float size_scale = active_scale * hud_text_size;

  float bx = x + 0.5f;
  float by = y + 0.5f;

  for (int i = 0; i < sc.num_segments; i++) {
    int x0 = (int)(bx + (sc.segments[i].x0 * size_scale) / par);
    int y0 = (int)(by + sc.segments[i].y0 * size_scale);
    int x1 = (int)(bx + (sc.segments[i].x1 * size_scale) / par);
    int y1 = (int)(by + sc.segments[i].y1 * size_scale);

    line<Range>(world, x0, y0, x1, y1, color, thickness);
  }
}

template <pixel_range Range = pixel_range::tkfloat>
inline void stroke_string(smart_world &world, float x, float y,
                          const std::string &str,
                          const aetk::core::color<Range> &color, float scale_factor = -1.0f,
                          float hud_text_size = 1.0f, int thickness = 1) {
  float ds = get_downsample_scale(world);
  float active_scale = (scale_factor < 0.0f) ? ds : scale_factor;
  float par = get_pixel_aspect_ratio(world);
  float cur_x = x;
  float step = (5.5f * active_scale * hud_text_size) / par;
  for (char c : str) {
    if (c != ' ') {
      stroke_character<Range>(world, cur_x, y, c, color, active_scale, hud_text_size,
                       thickness);
    }
    cur_x += step;
  }
}

template <pixel_range Range = pixel_range::tkfloat>
inline void stroke_string_segments(
    const smart_world& world,
    float x, float y,
    const std::string& str,
    const aetk::core::color<Range>& color,
    float scale_factor,
    float hud_text_size,
    int thickness,
    std::vector<batched_line<Range>>& out_lines
) {
    float ds = get_downsample_scale(world);
    float active_scale = (scale_factor < 0.0f) ? ds : scale_factor;
    float par = get_pixel_aspect_ratio(world);
    float cur_x = x;
    float step = (5.5f * active_scale * hud_text_size) / par;

    for (char c : str) {
        if (c != ' ') {
            char upper_c = (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
            const detail::stroke_char& sc = detail::get_stroke_char(upper_c);

            float size_scale = active_scale * hud_text_size;
            float bx = cur_x + 0.5f;
            float by = y + 0.5f;

            for (int i = 0; i < sc.num_segments; i++) {
                float x0 = bx + (sc.segments[i].x0 * size_scale) / par;
                float y0 = by + sc.segments[i].y0 * size_scale;
                float x1 = bx + (sc.segments[i].x1 * size_scale) / par;
                float y1 = by + sc.segments[i].y1 * size_scale;

                out_lines.push_back({ x0, y0, x1, y1, color, thickness });
            }
        }
        cur_x += step;
    }
}

/**
 * @brief Draw brackets on the corners of a bounding box.
 */
template <pixel_range Range = pixel_range::tkfloat>
inline void box_corners(smart_world &world, int xmin, int ymin, int xmax,
                        int ymax, const aetk::core::color<Range> &color, int len = 8,
                        int thickness = 1) {
  float par = get_pixel_aspect_ratio(world);
  float ds = get_downsample_scale(world);
  int scaled_len = (std::max)(1, (int)std::round(len * ds));
  int dlen = (int)std::round(scaled_len / par);

  line<Range>(world, xmin, ymin, xmin + dlen, ymin, color, thickness);
  line<Range>(world, xmin, ymin, xmin, ymin + scaled_len, color, thickness);
  line<Range>(world, xmax, ymin, xmax - dlen, ymin, color, thickness);
  line<Range>(world, xmax, ymin, xmax, ymin + scaled_len, color, thickness);
  line<Range>(world, xmin, ymax, xmin + dlen, ymax, color, thickness);
  line<Range>(world, xmin, ymax, xmin, ymax - scaled_len, color, thickness);
  line<Range>(world, xmax, ymax, xmax - dlen, ymax, color, thickness);
  line<Range>(world, xmax, ymax, xmax, ymax - scaled_len, color, thickness);
}

/**
 * @brief Draw a cross marker shape.
 */
template <pixel_range Range = pixel_range::tkfloat>
inline void cross_marker(smart_world &world, int cx, int cy, int size,
                         const aetk::core::color<Range> &color, int thickness = 1) {
  float par = get_pixel_aspect_ratio(world);
  float ds = get_downsample_scale(world);
  int scaled_size = (std::max)(1, (int)std::round(size * ds));
  int dx = (int)std::round(scaled_size / par);

  line<Range>(world, cx - dx, cy - scaled_size, cx + dx, cy + scaled_size, color, thickness);
  line<Range>(world, cx - dx, cy + scaled_size, cx + dx, cy - scaled_size, color, thickness);
}

/**
 * @brief Draw a plus marker shape.
 */
template <pixel_range Range = pixel_range::tkfloat>
inline void plus_marker(smart_world &world, int cx, int cy, int size,
                        const aetk::core::color<Range> &color, int thickness = 1) {
  float par = get_pixel_aspect_ratio(world);
  float ds = get_downsample_scale(world);
  int scaled_size = (std::max)(1, (int)std::round(size * ds));
  int dx = (int)std::round(scaled_size / par);

  line<Range>(world, cx - dx, cy, cx + dx, cy, color, thickness);
  line<Range>(world, cx, cy - scaled_size, cx, cy + scaled_size, color, thickness);
}

/**
 * @brief Draw a filled dot marker shape (Optimized Scanline Version).
 */
template <pixel_range Range = pixel_range::tkfloat>
inline void dot_marker(smart_world &world, int cx, int cy, int size,
                       const aetk::core::color<Range> &color) {
#if defined(AETK_ENABLE_CUDA)
  if (world.is_gpu()) {
    float r = (float)color.red;
    float g = (float)color.green;
    float b = (float)color.blue;
    float a = (float)color.alpha;
    if constexpr (Range == pixel_range::tkuint8) {
      r *= (1.0f / 255.0f);
      g *= (1.0f / 255.0f);
      b *= (1.0f / 255.0f);
      a *= (1.0f / 255.0f);
    }
    cuda_draw_dot_marker(world.gpu_data(), world.width(), world.height(), world.rowbytes(), cx, cy, size, r, g, b, a);
    return;
  }
#endif
  double src_a = color.alpha;
  if (src_a <= 0.0 || size <= 0)
    return;

  float par = get_pixel_aspect_ratio(world);
  float ds = get_downsample_scale(world);
  int scaled_size = (std::max)(1, (int)std::round(size * ds));

  int w = world.width();
  int h = world.height();
  int rowbytes = world.rowbytes();
  char *data = reinterpret_cast<char *>(world.ptr()->data);

  visit_pixel_format<Range>(world.pixel_format(), world.is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
    typename pixel_accessor<PixelT, IsBGRA, Range>::blender b(color);
    int r2 = scaled_size * scaled_size;
    for (int ty = -scaled_size; ty <= scaled_size; ++ty) {
      int py = cy + ty;
      if (py < 0 || py >= h)
        continue;

      int span_x = (int)std::round(std::sqrt(r2 - ty * ty) / par);
      int sx = (std::max)(0, cx - span_x);
      int ex = (std::min)(w - 1, cx + span_x);

      if (sx <= ex) {
        char *row_ptr = data + py * rowbytes;
        auto *dst_row = reinterpret_cast<PixelT *>(row_ptr);
        for (int x = sx; x <= ex; ++x) {
          PixelT &d = dst_row[x];
          double opaque_val = (Range == pixel_range::tkfloat) ? 1.0 : 255.0;
          if (src_a >= opaque_val) {
            pixel_accessor<PixelT, IsBGRA, Range>::write(&d, color);
          } else {
            b.blend(&d);
          }
        }
      }
    }
  });
}

/**
 * @brief Draw a filled solid triangle.
 *
 * @param dst The smart world buffer.
 * @param x1 Vertex 1 x-coordinate.
 * @param y1 Vertex 1 y-coordinate.
 * @param x2 Vertex 2 x-coordinate.
 * @param y2 Vertex 2 y-coordinate.
 * @param x3 Vertex 3 x-coordinate.
 * @param y3 Vertex 3 y-coordinate.
 * @param color The solid color to fill.
 */
template <pixel_range Range = pixel_range::tkfloat>
inline void fill_triangle(smart_world &dst, int x1, int y1, int x2, int y2,
                          int x3, int y3, const aetk::core::color<Range> &color) {
#if defined(AETK_ENABLE_CUDA)
  if (dst.is_gpu()) {
    float r = (float)color.red;
    float g = (float)color.green;
    float b = (float)color.blue;
    float a = (float)color.alpha;
    if constexpr (Range == pixel_range::tkuint8) {
      r *= (1.0f / 255.0f);
      g *= (1.0f / 255.0f);
      b *= (1.0f / 255.0f);
      a *= (1.0f / 255.0f);
    }
    cuda_draw_triangle(dst.gpu_data(), dst.width(), dst.height(), dst.rowbytes(), x1, y1, x2, y2, x3, y3, r, g, b, a);
    return;
  }
#endif
  int h = dst.height();
  int w = dst.width();
  int rowbytes = dst.rowbytes();
  char *data = reinterpret_cast<char *>(dst.ptr()->data);

  if (y1 > y2) {
    std::swap(x1, x2);
    std::swap(y1, y2);
  }
  if (y1 > y3) {
    std::swap(x1, x3);
    std::swap(y1, y3);
  }
  if (y2 > y3) {
    std::swap(x2, x3);
    std::swap(y2, y3);
  }

  if (y1 == y3)
    return;

  double src_a = color.alpha;
  if (src_a <= 0.0)
    return;

  float dx_13 = (float)(x3 - x1) / (y3 - y1);
  float dx_12 = (y2 != y1) ? (float)(x2 - x1) / (y2 - y1) : 0.0f;
  float dx_23 = (y3 != y2) ? (float)(x3 - x2) / (y3 - y2) : 0.0f;

  visit_pixel_format<Range>(dst.pixel_format(), dst.is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
    typename pixel_accessor<PixelT, IsBGRA, Range>::blender b(color);
    for (int y = y1; y <= y3; ++y) {
      if (y < 0 || y >= h)
        continue;

      float x_a = x1 + (y - y1) * dx_13;
      float x_b;
      if (y < y2) {
        x_b = x1 + (y - y1) * dx_12;
      } else {
        if (y3 == y2) {
          x_b = (float)x2;
        } else {
          x_b = x2 + (y - y2) * dx_23;
        }
      }

      int sx = (int)std::round(x_a);
      int ex = (int)std::round(x_b);
      if (sx > ex)
        std::swap(sx, ex);

      sx = std::clamp(sx, 0, w - 1);
      ex = std::clamp(ex, 0, w - 1);

      if (sx <= ex) {
        char *row_ptr = data + y * rowbytes;
        auto *dst_row = reinterpret_cast<PixelT *>(row_ptr);
        for (int x = sx; x <= ex; ++x) {
          PixelT &d = dst_row[x];
          double opaque_val = (Range == pixel_range::tkfloat) ? 1.0 : 255.0;
          if (src_a >= opaque_val) {
            pixel_accessor<PixelT, IsBGRA, Range>::write(&d, color);
          } else {
            b.blend(&d);
          }
        }
      }
    }
  });
}

namespace detail {

template <pixel_range Range>
inline double opaque_value() {
  if constexpr (Range == pixel_range::tkfloat) {
    return 1.0;
  } else {
    return 255.0;
  }
}

inline float segment_distance_sq(float x, float y, float x0, float y0, float x1,
                                 float y1) {
  float dx = x1 - x0;
  float dy = y1 - y0;
  float len_sq = dx * dx + dy * dy;
  if (len_sq < 0.0001f) {
    float px = x - x0;
    float py = y - y0;
    return px * px + py * py;
  }

  float t = ((x - x0) * dx + (y - y0) * dy) / len_sq;
  t = std::clamp(t, 0.0f, 1.0f);
  float proj_x = x0 + t * dx;
  float proj_y = y0 + t * dy;
  float px = x - proj_x;
  float py = y - proj_y;
  return px * px + py * py;
}

template <pixel_range Range>
struct cpu_line_segment {
  float x0 = 0.0f;
  float y0 = 0.0f;
  float x1 = 0.0f;
  float y1 = 0.0f;
  float radius = 0.5f;
  int y_min = 0;
  int y_max = 0;
  aetk::core::color<Range> color;
  bool opaque = false;
};

template <pixel_range Range>
struct cpu_triangle {
  float x1 = 0.0f;
  float y1 = 0.0f;
  float x2 = 0.0f;
  float y2 = 0.0f;
  float x3 = 0.0f;
  float y3 = 0.0f;
  float dx_13 = 0.0f;
  float dx_12 = 0.0f;
  float dx_23 = 0.0f;
  int y_min = 0;
  int y_max = 0;
  aetk::core::color<Range> color;
  bool opaque = false;
};

template <typename PixelT, bool IsBGRA, pixel_range Range>
inline void write_or_blend(PixelT *pixel, const aetk::core::color<Range> &color,
                           const typename pixel_accessor<PixelT, IsBGRA,
                                                          Range>::blender &blender,
                           bool opaque) {
  if (opaque) {
    pixel_accessor<PixelT, IsBGRA, Range>::write(pixel, color);
  } else {
    blender.blend(pixel);
  }
}

template <typename PixelT, bool IsBGRA, pixel_range Range>
inline void draw_lines_batched_cpu_typed(
    const context &ctx, smart_world &world,
    const std::vector<batched_line<Range>> &lines) {
  int w = world.width();
  int h = world.height();
  if (w <= 0 || h <= 0) {
    return;
  }

  float ds = get_downsample_scale(world);
  std::vector<cpu_line_segment<Range>> cpu_lines;
  cpu_lines.reserve(lines.size());
  std::vector<size_t> row_counts(static_cast<size_t>(h), 0);

  for (const auto &src_line : lines) {
    if (src_line.color.alpha <= 0.0 || src_line.thickness <= 0
        || !std::isfinite(src_line.x0) || !std::isfinite(src_line.y0)
        || !std::isfinite(src_line.x1) || !std::isfinite(src_line.y1)) {
      continue;
    }

    float scaled_thickness =
        (std::max)(1.0f, std::round(static_cast<float>(src_line.thickness) * ds));
    float radius = (std::max)(0.5f, (scaled_thickness - 1.0f) * 0.5f);
    float y_pad = radius + 1.0f;
    int y_min = (std::max)(
        0, static_cast<int>(std::floor((std::min)(src_line.y0, src_line.y1) - y_pad)));
    int y_max = (std::min)(
        h - 1,
        static_cast<int>(std::ceil((std::max)(src_line.y0, src_line.y1) + y_pad)));
    if (y_min > y_max) {
      continue;
    }

    cpu_lines.push_back({src_line.x0, src_line.y0, src_line.x1, src_line.y1, radius,
                         y_min, y_max, src_line.color,
                         src_line.color.alpha >= opaque_value<Range>()});
    for (int y = y_min; y <= y_max; ++y) {
      row_counts[static_cast<size_t>(y)]++;
    }
  }

  if (cpu_lines.empty()) {
    return;
  }

  std::vector<size_t> row_offsets(static_cast<size_t>(h) + 1, 0);
  for (int y = 0; y < h; ++y) {
    row_offsets[static_cast<size_t>(y) + 1] =
        row_offsets[static_cast<size_t>(y)] + row_counts[static_cast<size_t>(y)];
  }

  std::vector<int> row_line_indices(row_offsets.back());
  std::vector<size_t> row_cursors = row_offsets;
  for (int i = 0; i < static_cast<int>(cpu_lines.size()); ++i) {
    const auto &line = cpu_lines[static_cast<size_t>(i)];
    for (int y = line.y_min; y <= line.y_max; ++y) {
      size_t cursor = row_cursors[static_cast<size_t>(y)]++;
      row_line_indices[cursor] = i;
    }
  }

  char *data = reinterpret_cast<char *>(world.ptr()->data);
  ptrdiff_t rowbytes = static_cast<ptrdiff_t>(world.rowbytes());

  ctx.iterate_generic(h, [&](A_long /*thread_idx*/, A_long row_y, A_long /*count*/) {
    int y = static_cast<int>(row_y);
    size_t begin = row_offsets[static_cast<size_t>(y)];
    size_t end = row_offsets[static_cast<size_t>(y) + 1];
    if (begin == end) {
      return;
    }

    auto *dst_row = reinterpret_cast<PixelT *>(
        data + static_cast<ptrdiff_t>(y) * rowbytes);

    for (size_t task = begin; task < end; ++task) {
      const auto &line =
          cpu_lines[static_cast<size_t>(row_line_indices[task])];
      float dy = line.y1 - line.y0;
      float dx = line.x1 - line.x0;
      float pad = line.radius + 2.0f;
      int x_start = (std::max)(
          0, static_cast<int>(std::floor((std::min)(line.x0, line.x1) - pad)));
      int x_end = (std::min)(
          w - 1,
          static_cast<int>(std::ceil((std::max)(line.x0, line.x1) + pad)));

      if (std::fabs(dy) > 1.0f) {
        float t = (static_cast<float>(y) - line.y0) / dy;
        float x_proj = line.x0 + t * dx;
        x_start =
            (std::max)(x_start, static_cast<int>(std::floor(x_proj - pad)));
        x_end = (std::min)(x_end, static_cast<int>(std::ceil(x_proj + pad)));
      }

      if (x_start > x_end) {
        continue;
      }

      float radius_outer = line.radius + 1.0f;
      float radius_outer_sq = radius_outer * radius_outer;
      for (int x = x_start; x <= x_end; ++x) {
        float d_sq = segment_distance_sq(static_cast<float>(x), static_cast<float>(y),
                                         line.x0, line.y0, line.x1, line.y1);
        if (d_sq <= radius_outer_sq + 0.0001f) {
          float dist = std::sqrt(d_sq);
          float weight = 1.0f;
          if (dist > line.radius) {
            weight = 1.0f - (dist - line.radius);
          }
          if (weight > 0.0f) {
            aetk::core::color<Range> w_color = line.color;
            w_color.alpha *= weight;
            typename pixel_accessor<PixelT, IsBGRA, Range>::blender pixel_blender(w_color);
            bool is_opaque = (w_color.alpha >= detail::opaque_value<Range>());
            write_or_blend<PixelT, IsBGRA, Range>(&dst_row[x], w_color, pixel_blender,
                                                  is_opaque);
          }
        }
      }
    }
  });
}

template <typename PixelT, bool IsBGRA, pixel_range Range>
inline void draw_triangles_batched_cpu_typed(
    const context &ctx, smart_world &world,
    const std::vector<batched_triangle<Range>> &tris) {
  int w = world.width();
  int h = world.height();
  if (w <= 0 || h <= 0) {
    return;
  }

  std::vector<cpu_triangle<Range>> cpu_tris;
  cpu_tris.reserve(tris.size());
  std::vector<size_t> row_counts(static_cast<size_t>(h), 0);

  for (const auto &src_tri : tris) {
    if (src_tri.color.alpha <= 0.0 || !std::isfinite(src_tri.x0)
        || !std::isfinite(src_tri.y0) || !std::isfinite(src_tri.x1)
        || !std::isfinite(src_tri.y1) || !std::isfinite(src_tri.x2)
        || !std::isfinite(src_tri.y2)) {
      continue;
    }

    float x1 = src_tri.x0;
    float y1 = src_tri.y0;
    float x2 = src_tri.x1;
    float y2 = src_tri.y1;
    float x3 = src_tri.x2;
    float y3 = src_tri.y2;

    if (y1 > y2) {
      std::swap(x1, x2);
      std::swap(y1, y2);
    }
    if (y1 > y3) {
      std::swap(x1, x3);
      std::swap(y1, y3);
    }
    if (y2 > y3) {
      std::swap(x2, x3);
      std::swap(y2, y3);
    }
    if (std::fabs(y3 - y1) < 0.0001f) {
      continue;
    }

    int y_min = (std::max)(0, static_cast<int>(std::floor(y1)));
    int y_max = (std::min)(h - 1, static_cast<int>(std::ceil(y3)));
    if (y_min > y_max) {
      continue;
    }

    float dx_13 = (x3 - x1) / (y3 - y1);
    float dx_12 = (std::fabs(y2 - y1) > 0.0001f) ? (x2 - x1) / (y2 - y1)
                                                 : 0.0f;
    float dx_23 = (std::fabs(y3 - y2) > 0.0001f) ? (x3 - x2) / (y3 - y2)
                                                 : 0.0f;
    cpu_tris.push_back({x1, y1, x2, y2, x3, y3, dx_13, dx_12, dx_23, y_min,
                        y_max, src_tri.color,
                        src_tri.color.alpha >= opaque_value<Range>()});
    for (int y = y_min; y <= y_max; ++y) {
      row_counts[static_cast<size_t>(y)]++;
    }
  }

  if (cpu_tris.empty()) {
    return;
  }

  std::vector<size_t> row_offsets(static_cast<size_t>(h) + 1, 0);
  for (int y = 0; y < h; ++y) {
    row_offsets[static_cast<size_t>(y) + 1] =
        row_offsets[static_cast<size_t>(y)] + row_counts[static_cast<size_t>(y)];
  }

  std::vector<int> row_tri_indices(row_offsets.back());
  std::vector<size_t> row_cursors = row_offsets;
  for (int i = 0; i < static_cast<int>(cpu_tris.size()); ++i) {
    const auto &tri = cpu_tris[static_cast<size_t>(i)];
    for (int y = tri.y_min; y <= tri.y_max; ++y) {
      size_t cursor = row_cursors[static_cast<size_t>(y)]++;
      row_tri_indices[cursor] = i;
    }
  }

  char *data = reinterpret_cast<char *>(world.ptr()->data);
  ptrdiff_t rowbytes = static_cast<ptrdiff_t>(world.rowbytes());

  ctx.iterate_generic(h, [&](A_long /*thread_idx*/, A_long row_y, A_long /*count*/) {
    int y = static_cast<int>(row_y);
    size_t begin = row_offsets[static_cast<size_t>(y)];
    size_t end = row_offsets[static_cast<size_t>(y) + 1];
    if (begin == end) {
      return;
    }

    auto *dst_row = reinterpret_cast<PixelT *>(
        data + static_cast<ptrdiff_t>(y) * rowbytes);

    for (size_t task = begin; task < end; ++task) {
      const auto &tri = cpu_tris[static_cast<size_t>(row_tri_indices[task])];

      float x_a = tri.x1 + (static_cast<float>(y) - tri.y1) * tri.dx_13;
      float x_b;
      if (static_cast<float>(y) < tri.y2) {
        x_b = tri.x1 + (static_cast<float>(y) - tri.y1) * tri.dx_12;
      } else if (std::fabs(tri.y3 - tri.y2) < 0.0001f) {
        x_b = tri.x2;
      } else {
        x_b = tri.x2 + (static_cast<float>(y) - tri.y2) * tri.dx_23;
      }

      int sx = static_cast<int>(std::round(x_a));
      int ex = static_cast<int>(std::round(x_b));
      if (sx > ex) {
        std::swap(sx, ex);
      }
      sx = std::clamp(sx, 0, w - 1);
      ex = std::clamp(ex, 0, w - 1);
      if (sx > ex) {
        continue;
      }

      typename pixel_accessor<PixelT, IsBGRA, Range>::blender blender(tri.color);
      for (int x = sx; x <= ex; ++x) {
        write_or_blend<PixelT, IsBGRA, Range>(&dst_row[x], tri.color, blender,
                                              tri.opaque);
      }
    }
  });
}

} // namespace detail

template <pixel_range Range = pixel_range::tkfloat>
inline void draw_lines_batched(
    const context& ctx,
    smart_world& world,
    const std::vector<batched_line<Range>>& lines
) {
    if (lines.empty()) return;

#if defined(AETK_ENABLE_CUDA)
    if (world.is_gpu()) {
        int w = world.width();
        int h = world.height();

        std::vector<CudaLineSegment> cpu_lines;
        std::vector<CudaScanlineTask> cpu_tasks;
        cpu_lines.reserve(lines.size());
        cpu_tasks.reserve(lines.size() * 20); // estimate 20 scanlines per line

        for (int i = 0; i < (int)lines.size(); i++) {
            const auto& l = lines[i];
            float thickness_f = (float)l.thickness;
            float ds = get_downsample_scale(world);
            float scaled_thickness = (std::max)(1.0f, thickness_f * ds);

            int y_min = (std::max)(0, (int)std::floor((std::min)(l.y0, l.y1) - scaled_thickness));
            int y_max = (std::min)(h - 1, (int)std::ceil((std::max)(l.y0, l.y1) + scaled_thickness));

            float r = (float)l.color.red;
            float g = (float)l.color.green;
            float b = (float)l.color.blue;
            float a = (float)l.color.alpha;
            if constexpr (Range == pixel_range::tkuint8) {
                r *= (1.0f / 255.0f);
                g *= (1.0f / 255.0f);
                b *= (1.0f / 255.0f);
                a *= (1.0f / 255.0f);
            }

            CudaLineSegment cls;
            cls.x0 = l.x0;
            cls.y0 = l.y0;
            cls.x1 = l.x1;
            cls.y1 = l.y1;
            cls.r = r;
            cls.g = g;
            cls.b = b;
            cls.a = a;
            cls.thickness = scaled_thickness;

            cpu_lines.push_back(cls);

            for (int y = y_min; y <= y_max; y++) {
                cpu_tasks.push_back({ i, y });
            }
        }

        if (cpu_tasks.empty()) return;

        struct LineTensorCache {
            cuda_buffer t_lines;
            cuda_buffer t_tasks;
        };
        thread_local LineTensorCache line_cache;

        size_t lines_bytes = cpu_lines.size() * sizeof(CudaLineSegment);
        size_t tasks_bytes = cpu_tasks.size() * sizeof(CudaScanlineTask);

        if (lines_bytes > line_cache.t_lines.size()) {
            size_t new_size = (cpu_lines.size() + 255) & ~255;
            line_cache.t_lines.reserve(new_size * sizeof(CudaLineSegment));
        }
        if (tasks_bytes > line_cache.t_tasks.size()) {
            size_t new_size = (cpu_tasks.size() + 255) & ~255;
            line_cache.t_tasks.reserve(new_size * sizeof(CudaScanlineTask));
        }

        cudaError_t err_lines = cudaMemcpy(line_cache.t_lines.data(), cpu_lines.data(), lines_bytes, cudaMemcpyHostToDevice);
        cudaError_t err_tasks = cudaMemcpy(line_cache.t_tasks.data(), cpu_tasks.data(), tasks_bytes, cudaMemcpyHostToDevice);

        if (err_lines == cudaSuccess && err_tasks == cudaSuccess) {
            cuda_draw_lines_batched(
                world.gpu_data(),
                w, h,
                world.rowbytes(),
                static_cast<const CudaLineSegment*>(line_cache.t_lines.data()),
                (int)cpu_lines.size(),
                static_cast<const CudaScanlineTask*>(line_cache.t_tasks.data()),
                (int)cpu_tasks.size()
            );
        }
        return;
    }
#endif

    visit_pixel_format<Range>(world.pixel_format(), world.is_bgra(),
        [&]<typename PixelT, bool IsBGRA>() {
            detail::draw_lines_batched_cpu_typed<PixelT, IsBGRA, Range>(
                ctx, world, lines);
        });
}

template <pixel_range Range = pixel_range::tkfloat>
inline void draw_triangles_batched(
    const context& ctx,
    smart_world& world,
    const std::vector<batched_triangle<Range>>& tris
) {
    if (tris.empty()) return;

#if defined(AETK_ENABLE_CUDA)
    if (world.is_gpu()) {
        int w = world.width();
        int h = world.height();

        std::vector<CudaTriangle> cpu_tris;
        std::vector<CudaTriangleTask> cpu_tasks;
        cpu_tris.reserve(tris.size());
        cpu_tasks.reserve(tris.size() * 30); // estimate 30 scanlines per triangle

        for (int i = 0; i < (int)tris.size(); i++) {
            const auto& t = tris[i];
            int y_min = (std::max)(0, (int)std::floor((std::min)({t.y0, t.y1, t.y2})));
            int y_max = (std::min)(h - 1, (int)std::ceil((std::max)({t.y0, t.y1, t.y2})));

            float r = (float)t.color.red;
            float g = (float)t.color.green;
            float b = (float)t.color.blue;
            float a = (float)t.color.alpha;
            if constexpr (Range == pixel_range::tkuint8) {
                r *= (1.0f / 255.0f);
                g *= (1.0f / 255.0f);
                b *= (1.0f / 255.0f);
                a *= (1.0f / 255.0f);
            }

            CudaTriangle ct;
            ct.x0 = t.x0;
            ct.y0 = t.y0;
            ct.x1 = t.x1;
            ct.y1 = t.y1;
            ct.x2 = t.x2;
            ct.y2 = t.y2;
            ct.r = r;
            ct.g = g;
            ct.b = b;
            ct.a = a;

            cpu_tris.push_back(ct);

            for (int y = y_min; y <= y_max; y++) {
                cpu_tasks.push_back({ i, y });
            }
        }

        if (cpu_tasks.empty()) return;

        struct TriTensorCache {
            cuda_buffer t_tris;
            cuda_buffer t_tasks;
        };
        thread_local TriTensorCache tri_cache;

        size_t tris_bytes = cpu_tris.size() * sizeof(CudaTriangle);
        size_t tasks_bytes = cpu_tasks.size() * sizeof(CudaTriangleTask);

        if (tris_bytes > tri_cache.t_tris.size()) {
            size_t new_size = (cpu_tris.size() + 255) & ~255;
            tri_cache.t_tris.reserve(new_size * sizeof(CudaTriangle));
        }
        if (tasks_bytes > tri_cache.t_tasks.size()) {
            size_t new_size = (cpu_tasks.size() + 255) & ~255;
            tri_cache.t_tasks.reserve(new_size * sizeof(CudaTriangleTask));
        }

        cudaError_t err_tris = cudaMemcpy(tri_cache.t_tris.data(), cpu_tris.data(), tris_bytes, cudaMemcpyHostToDevice);
        cudaError_t err_tasks = cudaMemcpy(tri_cache.t_tasks.data(), cpu_tasks.data(), tasks_bytes, cudaMemcpyHostToDevice);

        if (err_tris == cudaSuccess && err_tasks == cudaSuccess) {
            cuda_draw_triangles_batched(
                world.gpu_data(),
                w, h,
                world.rowbytes(),
                static_cast<const CudaTriangle*>(tri_cache.t_tris.data()),
                (int)cpu_tris.size(),
                static_cast<const CudaTriangleTask*>(tri_cache.t_tasks.data()),
                (int)cpu_tasks.size()
            );
        }
        return;
    }
#endif

    visit_pixel_format<Range>(world.pixel_format(), world.is_bgra(),
        [&]<typename PixelT, bool IsBGRA>() {
            detail::draw_triangles_batched_cpu_typed<PixelT, IsBGRA, Range>(
                ctx, world, tris);
        });
}

} // namespace aetk::effect::draw
