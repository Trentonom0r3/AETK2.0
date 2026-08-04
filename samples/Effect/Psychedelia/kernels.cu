#include "render_params.hpp"
#include <aetk/effect/gpu/kernels/swizzle.h>
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <math_constants.h>

// Matching structure to CPU side
struct gpu_history_batch {
  const void *ptrs[16];
  int pitches[16];
  int count;
};

namespace {

constexpr float kTau = 6.28318530718f;

__device__ __forceinline__ float saturatef(float value) {
  return fminf(fmaxf(value, 0.0f), 1.0f);
}

__device__ __forceinline__ float lerpf(float a, float b, float t) {
  return a + (b - a) * t;
}

__device__ __forceinline__ float4 mix_rgba(const float4 &a, const float4 &b,
                                           float t) {
  t = saturatef(t);
  return make_float4(lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t),
                     lerpf(a.w, b.w, t));
}

__device__ __forceinline__ float luminance_rgba(const float4 &rgba) {
  return rgba.x * 0.2126f + rgba.y * 0.7152f + rgba.z * 0.0722f;
}

__device__ __forceinline__ float4 clamp_rgba(const float4 &rgba) {
  return make_float4(saturatef(rgba.x), saturatef(rgba.y), saturatef(rgba.z),
                     saturatef(rgba.w));
}

__device__ __forceinline__ float4 adjust_saturation_rgba(const float4 &rgba,
                                                         float amount) {
  const float y = luminance_rgba(rgba);
  return clamp_rgba(make_float4(y + (rgba.x - y) * amount,
                                y + (rgba.y - y) * amount,
                                y + (rgba.z - y) * amount, rgba.w));
}

__device__ __forceinline__ float4 rotate_hue_rgba(const float4 &rgba,
                                                  float angle) {
  const float cosine = cosf(angle);
  const float sine = sinf(angle);
  const float shared = ((rgba.x + rgba.y + rgba.z) / 3.0f) * (1.0f - cosine);
  const float axis = 0.57735026919f;
  const float rr =
      (rgba.x * cosine) + (axis * (rgba.z - rgba.y) * sine) + shared;
  const float gg =
      (rgba.y * cosine) + (axis * (rgba.x - rgba.z) * sine) + shared;
  const float bb =
      (rgba.z * cosine) + (axis * (rgba.y - rgba.x) * sine) + shared;
  return clamp_rgba(make_float4(rr, gg, bb, rgba.w));
}

__device__ __forceinline__ unsigned int hash_u32(int x, int y) {
  unsigned int n = static_cast<unsigned int>(x) * 0x1f123bb5u;
  n ^= static_cast<unsigned int>(y) * 0x05491333u;
  n ^= n >> 15;
  n *= 0x2c1b3c6du;
  n ^= n >> 12;
  n *= 0x297a2d39u;
  n ^= n >> 15;
  return n;
}

__device__ __forceinline__ float smooth_step(float t) {
  t = saturatef(t);
  return t * t * (3.0f - 2.0f * t);
}

__device__ __forceinline__ float smooth_range(float edge0, float edge1,
                                              float value) {
  return smooth_step((value - edge0) / (edge1 - edge0 + 1.0e-5f));
}

__device__ __forceinline__ float value_hash(int x, int y) {
  return static_cast<float>(hash_u32(x, y) & 0x00ffffffu) / 16777215.0f;
}

__device__ float value_noise(float x, float y) {
  const int ix = static_cast<int>(floorf(x));
  const int iy = static_cast<int>(floorf(y));
  const float fx = x - static_cast<float>(ix);
  const float fy = y - static_cast<float>(iy);
  const float u = smooth_step(fx);
  const float v = smooth_step(fy);
  const float a = value_hash(ix, iy);
  const float b = value_hash(ix + 1, iy);
  const float c = value_hash(ix, iy + 1);
  const float d = value_hash(ix + 1, iy + 1);
  return lerpf(lerpf(a, b, u), lerpf(c, d, u), v);
}

__device__ float fbm(float x, float y, float phase, int octaves) {
  float sum = 0.0f;
  float amplitude = 0.62f;
  float frequency = 1.0f;
  for (int octave = 0; octave < octaves; ++octave) {
    sum += amplitude *
           value_noise(x * frequency + phase * (0.35f + octave * 0.09f),
                       y * frequency - phase * (0.27f + octave * 0.07f));
    frequency *= 2.15f;
    amplitude *= 0.62f;
    x += 13.7f;
    y -= 9.1f;
  }
  return sum;
}

__device__ float folded_fractal_field(float u, float v, float center_u,
                                      float center_v, float aspect,
                                      float detail_freq, float flow_a,
                                      float flow_b, float time, float speed,
                                      int octaves) {
  const float px = (u - center_u) * aspect;
  const float py = v - center_v;
  const float radius = sqrtf(px * px + py * py) + 1.0e-5f;
  const float theta = atan2f(py, px);

  const float folds = 7.0f;
  const float sector = kTau / folds;
  const float phase = time * speed * 0.16f;

  float local_theta = fmodf(theta + phase, sector);
  if (local_theta < 0.0f) {
    local_theta += sector;
  }
  local_theta = fabsf(local_theta - sector * 0.5f);

  float qx = cosf(local_theta) * radius;
  float qy = sinf(local_theta) * radius;

  float folded_accum = 0.0f;
  float folded_weight = 0.58f;
  for (int i = 0; i < 3; ++i) {
    qx = fabsf(qx);
    qy = fabsf(qy);

    const float fi = static_cast<float>(i);
    const float rot = phase * (0.72f + 0.13f * fi) + 0.54f * fi;
    const float cs = cosf(rot);
    const float sn = sinf(rot);
    const float rx = qx * cs - qy * sn;
    const float ry = qx * sn + qy * cs;
    qx = rx;
    qy = ry;

    const float orbit = sqrtf(qx * qx + qy * qy);
    const float diagonal_lace =
        1.0f - smooth_range(0.0f, 0.050f + 0.012f * fi, fabsf(qx - qy));
    const float ring_lace =
        1.0f - smooth_range(0.0f, 0.075f + 0.016f * fi,
                             fabsf(orbit - (0.10f + 0.075f * fi)));
    folded_accum +=
        (diagonal_lace * 0.56f + ring_lace * 0.44f) * folded_weight;

    const float scale = 1.56f + 0.12f * fi;
    qx = qx * scale - (0.105f + 0.022f * fi);
    qy = qy * scale - (0.070f + 0.018f * fi);
    folded_weight *= 0.52f;
  }

  const float domain =
      fbm(qx * detail_freq * 1.70f + flow_a * 1.25f,
          qy * detail_freq * 1.70f - flow_b * 1.15f,
          time * speed * 0.21f, octaves);
  float ridge = 1.0f - fabsf(domain * 2.0f - 1.0f);
  ridge *= ridge;

  const float spiral =
      0.5f + 0.5f * sinf(radius * detail_freq * 7.0f - theta * 2.0f +
                         domain * 5.0f - time * speed * 0.52f);

  return smooth_range(0.18f, 0.86f,
                      folded_accum * 0.48f + ridge * 0.34f +
                          spiral * 0.18f);
}

__device__ float groovy_flower_field(float u, float v, float center_u,
                                     float center_v, float aspect,
                                     float detail, float flow_a,
                                     float flow_b, float time, float speed,
                                     float density, int &chosen_cx, int &chosen_cy) {
  float px = (u - center_u) * aspect;
  float py = v - center_v;

  px += (flow_a - 0.5f) * 0.05f;
  py += (flow_b - 0.5f) * 0.05f;

  float Rc = 0.15f;
  float max_layer_val = -1.0e9f;
  float final_pattern_val = 0.0f;

  if (density > 0.01f) {
    float cells_y = fmaxf(1.0f, (density / 100.0f) * 30.0f);
    float grid_size = 1.0f / cells_y;

    int cell_x = static_cast<int>(floorf(px / grid_size + 0.5f));
    int cell_y = static_cast<int>(floorf(py / grid_size + 0.5f));

    chosen_cx = cell_x;
    chosen_cy = cell_y;

    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        int cx = cell_x + dx;
        int cy = cell_y + dy;

        float lx = px - static_cast<float>(cx) * grid_size;
        float ly = py - static_cast<float>(cy) * grid_size;

        lx /= grid_size;
        ly /= grid_size;

        float hash_x = value_hash(cx, cy);
        float hash_y = value_hash(cx + 37, cy + 93);

        lx -= (hash_x - 0.5f) * 0.35f;
        ly -= (hash_y - 0.5f) * 0.35f;

        float scale_var = 0.6f + 0.6f * hash_x;
        float total_scale = detail * scale_var;
        if (total_scale < 0.01f) total_scale = 0.01f;

        lx /= total_scale;
        ly /= total_scale;

        float rotation = hash_x * kTau + time * speed * (0.2f + 0.5f * hash_y);

        float r = sqrtf(lx * lx + ly * ly) + 1.0e-5f;
        float theta = atan2f(ly, lx) - rotation;

        int N = 5 + static_cast<int>(hash_x * 7.0f);
        float petal_shape = cosf(N * theta);
        float r_bound = Rc;
        if (hash_y < 0.4f) {
          r_bound += 0.15f * (0.5f + 0.5f * petal_shape);
        } else if (hash_y < 0.7f) {
          r_bound += 0.16f * sqrtf(fmaxf(0.0f, 0.5f + 0.5f * petal_shape));
        } else {
          r_bound += 0.18f * powf(fmaxf(0.0f, 0.5f + 0.5f * petal_shape), 3.0f);
        }

        float flower_dist = r - r_bound;
        float center_dist = r - Rc;

        float cell_pattern = 0.0f;
        float w_border = 0.025f;

        if (center_dist < -w_border) {
          cell_pattern = 1.0f;
        } else if (fabsf(center_dist) <= w_border) {
          cell_pattern = 0.8f;
        } else if (flower_dist < -w_border) {
          cell_pattern = 0.5f;
        } else if (fabsf(flower_dist) <= w_border) {
          cell_pattern = 0.2f;
        }

        if (cell_pattern > 0.0f) {
          float layer_depth = hash_x + cell_pattern;
          if (layer_depth > max_layer_val) {
            max_layer_val = layer_depth;
            final_pattern_val = cell_pattern;
            chosen_cx = cx;
            chosen_cy = cy;
          }
        }
      }
    }
  } else {
    // Single center flower
    float lx = px;
    float ly = py;

    chosen_cx = 0;
    chosen_cy = 0;

    float total_scale = detail;
    if (total_scale < 0.01f) total_scale = 0.01f;

    lx /= total_scale;
    ly /= total_scale;

    float rotation = time * speed * 0.3f;

    float r = sqrtf(lx * lx + ly * ly) + 1.0e-5f;
    float theta = atan2f(ly, lx) - rotation;

    int N = 8;
    float petal_shape = cosf(N * theta);
    float r_bound = Rc + 0.25f * (0.5f + 0.5f * petal_shape);

    float flower_dist = r - r_bound;
    float center_dist = r - Rc;

    float cell_pattern = 0.0f;
    float w_border = 0.025f;

    if (center_dist < -w_border) {
      cell_pattern = 1.0f;
    } else if (fabsf(center_dist) <= w_border) {
      cell_pattern = 0.8f;
    } else if (flower_dist < -w_border) {
      cell_pattern = 0.5f;
    } else if (fabsf(flower_dist) <= w_border) {
      cell_pattern = 0.2f;
    }

    final_pattern_val = cell_pattern;
  }

  return final_pattern_val;
}

__device__ __forceinline__ float4 get_flower_pixel_color_rgba(float pattern_val, int cx, int cy, float4 source_color, float palette_blend, float hue_shift) {
  float hash_x = value_hash(cx, cy);
  float hash_y = value_hash(cx + 37, cy + 93);

  float4 palette[5] = {
    make_float4(0.941f, 0.882f, 0.725f, 1.0f), // Cream
    make_float4(0.902f, 0.667f, 0.118f, 1.0f), // Yellow
    make_float4(0.863f, 0.314f, 0.078f, 1.0f), // Orange
    make_float4(0.118f, 0.549f, 0.510f, 1.0f), // Teal
    make_float4(0.196f, 0.137f, 0.118f, 1.0f)  // Dark Brown/Outline
  };

  int petal_idx = static_cast<int>(hash_x * 4.0f);
  int center_idx = (petal_idx + 1 + static_cast<int>(hash_y * 3.0f)) % 4;

  float4 target_color;
  if (pattern_val > 0.85f) {
    target_color = palette[center_idx];
  } else if (pattern_val > 0.70f) {
    target_color = palette[4];
  } else if (pattern_val > 0.40f) {
    target_color = palette[petal_idx];
  } else {
    target_color = palette[4];
  }

  if (hue_shift != 0.0f) {
    target_color = rotate_hue_rgba(target_color, hue_shift);
  }

  if (palette_blend < 0.999f) {
    if ((pattern_val > 0.70f && pattern_val <= 0.85f) || pattern_val <= 0.40f) {
      float4 dark_source = make_float4(source_color.x * 0.3f, source_color.y * 0.3f, source_color.z * 0.3f, source_color.w);
      return mix_rgba(dark_source, target_color, palette_blend);
    }
    return mix_rgba(source_color, target_color, palette_blend);
  }
  return target_color;
}

struct device_state_profile {
  float geometry;
  float tracers;
  float chroma;
  float split;
  float halo;
  float pattern;
  float speed;
  float detail;
};

__device__ __forceinline__ device_state_profile profile_for_state(int state) {
  switch (state) {
  case 1:
    return {0.58f, 0.20f, 0.28f, 0.22f, 0.22f, 0.30f, 0.85f, 0.92f};
  case 3:
    return {1.42f, 1.08f, 1.18f, 1.34f, 0.92f, 1.22f, 1.12f, 1.24f};
  case 2:
  default:
    return {0.96f, 0.62f, 0.74f, 0.82f, 0.58f, 0.76f, 1.0f, 1.08f};
  }
}

__device__ float4 read_rgba(const unsigned char *src, int pitch, int width,
                            int height, int x, int y) {
  x = max(0, min(width - 1, x));
  y = max(0, min(height - 1, y));
  const float4 *row = reinterpret_cast<const float4 *>(src + y * pitch);
  return aetk_bgra_to_rgba(row[x]);
}

__device__ float4 sample_rgba(const unsigned char *src, int pitch, int width,
                              int height, float x, float y) {
  const int x0 = static_cast<int>(floorf(x));
  const int y0 = static_cast<int>(floorf(y));
  const float tx = x - static_cast<float>(x0);
  const float ty = y - static_cast<float>(y0);
  const float4 c00 = read_rgba(src, pitch, width, height, x0, y0);
  const float4 c10 = read_rgba(src, pitch, width, height, x0 + 1, y0);
  const float4 c01 = read_rgba(src, pitch, width, height, x0, y0 + 1);
  const float4 c11 = read_rgba(src, pitch, width, height, x0 + 1, y0 + 1);
  return mix_rgba(mix_rgba(c00, c10, tx), mix_rgba(c01, c11, tx), ty);
}

__global__ void psychedelia_kernel(const unsigned char *src,
                                   gpu_history_batch batch, unsigned char *dst,
                                   int width, int height, int src_pitch,
                                   int dst_pitch,
                                   psychedelia_render_params params) {
  const int x = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
  const int y = static_cast<int>(blockIdx.y * blockDim.y + threadIdx.y);
  if (x >= width || y >= height)
    return;

  const device_state_profile profile = profile_for_state(params.state);
  const int octaves = (params.quality == 1) ? 2 : (params.quality == 3) ? 4 : 3;
  const float quality = (params.quality == 1)   ? 0.82f
                        : (params.quality == 3) ? 1.18f
                                                : 1.0f;

  const float base_tracer = params.intensity * params.tracers * profile.tracers;
  const float base_chroma = params.intensity * params.spectrum * profile.chroma;
  const float base_split =
      params.intensity * params.color_separation * profile.split;
  const float base_halo = params.intensity * params.halo * profile.halo;
  const float base_fractal =
      params.intensity * params.fractal * profile.pattern * quality;

  const float speed = fmaxf(0.05f, params.speed * profile.speed);
  const float detail = fmaxf(0.35f, params.detail * profile.detail);
  const float min_dim = (float)min(params.full_width, params.full_height);

  const float abs_x = (float)(x + params.origin_x);
  const float abs_y = (float)(y + params.origin_y);

  const float u = (abs_x + 0.5f) / params.full_width;
  const float v = (abs_y + 0.5f) / params.full_height;

  const float center_u = params.center_x / params.full_width;
  const float center_v = params.center_y / params.full_height;
  const float field_radius =
      sqrtf(((u - center_u) * (u - center_u) *
             (params.full_width / min_dim) * (params.full_width / min_dim)) +
            ((v - center_v) * (v - center_v)));

  // Compute distance from center in pixels with PAR compensation
  const float dx_c = abs_x - params.center_x;
  const float dy_c = abs_y - params.center_y;
  const float dx_c_scaled = dx_c * params.par;
  const float dist = sqrtf(dx_c_scaled * dx_c_scaled + dy_c * dy_c);
  float cos_t = 1.0f;
  if (params.radius > 0.01f) {
    float r_norm = fminf(1.0f, dist / params.radius);
    cos_t = sqrtf(fmaxf(0.0f, 1.0f - (r_norm * r_norm) / 1.77f));
  }
  float weight = 1.0f;
  if (params.radius > 0.01f) {
    const float r_inner = fmaxf(0.0f, params.radius - params.feather);
    const float r_outer = params.radius;
    if (dist <= r_inner) {
      weight = 1.0f;
    } else if (dist >= r_outer) {
      weight = 0.0f;
    } else {
      const float t = (dist - r_inner) / (r_outer - r_inner + 1e-5f);
      weight = 1.0f - (t * t * (3.0f - 2.0f * t));
    }
  }

  // If outside outer radius, output the clean input pixel directly
  if (params.radius > 0.01f && weight <= 0.0f) {
    const float4 clean_px = sample_rgba(src, src_pitch, width, height, abs_x - params.src_origin_x, abs_y - params.src_origin_y);
    reinterpret_cast<float4 *>(dst + y * dst_pitch)[x] = aetk_rgba_to_bgra(clamp_rgba(clean_px));
    return;
  }

  const float detail_freq = 3.5f + detail * 12.5f;
  const float f_a = fbm(u * detail_freq + 2.1f, v * detail_freq - 4.5f,
                        params.time * speed * 0.11f, octaves);
  const float f_b = fbm(u * detail_freq - 7.4f, v * detail_freq + 3.8f,
                        -params.time * speed * 0.09f, octaves);

  const float aspect =
      static_cast<float>(params.full_width) /
      static_cast<float>(max(1, params.full_height));
  float pattern_noise = 0.0f;
  int cx = 0, cy = 0;
  if (params.pattern_style == 2) {
    pattern_noise = groovy_flower_field(u, v, center_u, center_v, aspect,
                                        detail, f_a, f_b, params.time, speed, params.flower_density, cx, cy);
  } else if (params.pattern_style == 3) {
    pattern_noise = (0.2f + 2.5f * detail) * cos_t +
                    1.8f * (f_a * 0.6f + f_b * 0.4f) * (params.fractal / 100.0f * params.intensity) +
                    params.time * speed * 0.15f;
  } else {
    pattern_noise = folded_fractal_field(u, v, center_u, center_v, aspect,
                                         detail_freq, f_a, f_b, params.time, speed,
                                         octaves + 1);
  }

  // Organic 2D Visual Drifting (Breathing, Melting, Flowing, Morphing)
  const float breathing_mult =
      params.breathing * params.intensity * profile.geometry;
  const float wave_x =
      sinf(u * detail_freq - params.time * speed * 1.5f + f_a * 3.5f) *
      breathing_mult;
  const float wave_y =
      sinf(v * detail_freq + params.time * speed * 1.3f + f_b * 3.5f) *
      breathing_mult;

  const float morphing_amount = params.morphing / 100.0f * params.intensity;
  const float melting_amount = params.melting / 100.0f * params.intensity;
  const float flowing_amount = params.flowing / 100.0f * params.intensity;

  float dx = (f_a - 0.5f) * min_dim * 0.025f * morphing_amount;
  float dy = (f_b - 0.5f) * min_dim * 0.025f * morphing_amount;

  dx += wave_x * min_dim * 0.012f;
  dy += wave_y * min_dim * 0.012f;
  dy += melting_amount * min_dim * 0.035f * (f_a + 0.1f);
  dx += flowing_amount * min_dim * 0.02f * f_a;
  dy += flowing_amount * min_dim * 0.02f * f_b;

  const float fractal_phase =
      (params.pattern_style >= 2 ? 0.0f : pattern_noise) * kTau + field_radius * 5.5f - params.time * speed * 0.35f;
  const float micro_scale = min_dim * 0.0075f * base_fractal;
  dx += micro_scale * (cosf(fractal_phase) * 0.70f + (f_a - 0.5f) * 0.55f);
  dy += micro_scale * (sinf(fractal_phase) * 0.70f + (f_b - 0.5f) * 0.55f);

  dx *= weight;
  dy *= weight;

  float sx = abs_x + dx;
  float sy = abs_y + dy;

  if (params.pattern_style == 3 && params.radius > 0.01f) {
    if (dist < params.radius) {
      float r_norm = dist / params.radius;
      float zoom = 1.0f + 0.15f * detail * sqrtf(fmaxf(0.0f, 1.0f - r_norm * r_norm));
      sx = params.center_x + (abs_x - params.center_x) / zoom + dx;
      sy = params.center_y + (abs_y - params.center_y) / zoom + dy;
    }
  }

  // Magnification (Macropsia Center Zoom)
  const float magnify = params.magnification / 100.0f * params.intensity;
  const float center_x = params.center_x;
  const float center_y = params.center_y;
  if (magnify > 0.01f) {
    if (params.radius > 0.01f) {
      if (dist < params.radius) {
        float p = dist / params.radius;
        float min_zoom = 1.0f / (1.0f + magnify * 0.65f);
        float falloff = (1.0f - p * p);
        float zoom = 1.0f - (1.0f - min_zoom) * falloff * falloff;
        sx = center_x + (sx - center_x) * zoom;
        sy = center_y + (sy - center_y) * zoom;
      }
    } else {
      // Fallback to standard zoom if radius is 0
      const float zoom = 1.0f / (1.0f + magnify * 0.35f);
      sx = center_x + (sx - center_x) * zoom;
      sy = center_y + (sy - center_y) * zoom;
    }
  }

  const float4 center = sample_rgba(src, src_pitch, width, height, sx - params.src_origin_x, sy - params.src_origin_y);
  const float4 left = sample_rgba(src, src_pitch, width, height, sx - params.src_origin_x - 1.5f, sy - params.src_origin_y);
  const float4 right =
      sample_rgba(src, src_pitch, width, height, sx - params.src_origin_x + 1.5f, sy - params.src_origin_y);
  const float4 up = sample_rgba(src, src_pitch, width, height, sx - params.src_origin_x, sy - params.src_origin_y - 1.5f);
  const float4 down = sample_rgba(src, src_pitch, width, height, sx - params.src_origin_x, sy - params.src_origin_y + 1.5f);

  const float sharpen_amount = (params.acuity / 100.0f) * params.intensity;
  float4 sharp_center = center;
  if (sharpen_amount > 0.01f) {
    sharp_center.x =
        center.x + (center.x - (left.x + right.x + up.x + down.x) * 0.25f) *
                       sharpen_amount;
    sharp_center.y =
        center.y + (center.y - (left.y + right.y + up.y + down.y) * 0.25f) *
                       sharpen_amount;
    sharp_center.z =
        center.z + (center.z - (left.z + right.z + up.z + down.z) * 0.25f) *
                       sharpen_amount;
    sharp_center = clamp_rgba(sharp_center);
  }

  const float lum_center = luminance_rgba(sharp_center);
  const float edge =
      saturatef((fabsf(luminance_rgba(right) - luminance_rgba(left)) +
                 fabsf(luminance_rgba(down) - luminance_rgba(up))) *
                0.88f);

  const float midtone = 1.0f - fabsf((lum_center * 2.0f) - 1.0f);

  const float peripheral = saturatef((field_radius - 0.06f) / 0.62f);

  float pattern_field = 0.5f;
  float pattern_mask = 0.0f;
  if (params.pattern_style == 3 || base_fractal > 0.001f) {
    if (params.pattern_style == 2) {
      pattern_field = pattern_noise;
      pattern_mask = saturatef(params.intensity * params.fractal) * (pattern_field > 0.1f ? 1.0f : 0.0f);
    } else if (params.pattern_style == 3) {
      pattern_field = pattern_noise;
      float fresnel = 0.05f + 0.95f * powf(1.0f - cos_t, 3.0f);
      pattern_mask = saturatef(params.intensity * (params.fractal / 100.0f + 0.2f)) * weight * fresnel;
    } else {
      pattern_field =
          saturatef(pattern_noise * 0.72f +
                    (0.5f + sinf(field_radius * 18.0f * detail + f_a * 2.4f -
                                  params.time * speed * 0.45f) *
                                0.5f) *
                        0.28f);
      pattern_mask =
          saturatef(base_fractal *
                    (0.06f + edge * 0.62f + peripheral * 0.18f + midtone * 0.20f)) *
          smooth_range(0.28f, 0.82f, pattern_field);
    }
  }

  // --- Multi-Frame GPU Accumulation (Distinct Motion Tracers) ---
  float4 traced = sharp_center;
  const float hue_shift_rate = base_chroma * 0.5f;

  if (batch.count > 0) {
    float solidity_normalized = params.tracer_solidity / 100.0f;
    float base_weight = lerpf(0.15f, 0.60f, solidity_normalized);
    float current_weight = base_weight * saturatef(base_tracer);
    float decay_rate = lerpf(0.40f, 0.90f, solidity_normalized);

    float4 accum = sharp_center;

    for (int i = 0; i < batch.count; ++i) {
      float step = (float)(i + 1);
      float sample_x = sx + dx * (params.tracer_drift / 100.0f) * step;
      float sample_y = sy + dy * (params.tracer_drift / 100.0f) * step;
      const unsigned char *frame_ptr =
          static_cast<const unsigned char *>(batch.ptrs[i]);
      float4 sample = sample_rgba(frame_ptr, batch.pitches[i], width, height,
                                  sample_x - params.history_origin_x[i], sample_y - params.history_origin_y[i]);
      sample = rotate_hue_rgba(sample, hue_shift_rate * step);
      accum = mix_rgba(accum, sample, current_weight);
      current_weight *= decay_rate;
    }

    traced = accum;
  }

  float len = sqrtf(dx * dx + dy * dy);
  float split_dir_x = 1.0f;
  float split_dir_y = 0.0f;
  if (len > 0.01f) {
    split_dir_x = dx / len;
    split_dir_y = dy / len;
  }

  const float sep =
      0.9f +
      (min_dim * 0.0036f * base_split *
       (0.25f + edge * 1.02f + pattern_mask * 0.72f + peripheral * 0.24f));

  float4 chroma = traced;
  chroma.x = lerpf(traced.x,
                   sample_rgba(src, src_pitch, width, height,
                               sx - params.src_origin_x + split_dir_x * sep, sy - params.src_origin_y + split_dir_y * sep)
                       .x,
                   saturatef(base_split * 0.92f));
  chroma.z = lerpf(traced.z,
                   sample_rgba(src, src_pitch, width, height,
                               sx - params.src_origin_x - split_dir_x * sep, sy - params.src_origin_y - split_dir_y * sep)
                       .z,
                   saturatef(base_split * 0.96f));

  float4 final = mix_rgba(traced, chroma,
                          saturatef(base_chroma * 0.30f + base_split * 0.52f +
                                    pattern_mask * 0.16f));

  const float g_px = 1.0f + (min_dim * 0.0020f * base_halo *
                             (0.40f + edge * 1.22f + pattern_mask * 0.45f));
  final =
      mix_rgba(final,
               sample_rgba(src, src_pitch, width, height,
                           sx - params.src_origin_x + split_dir_x * g_px, sy - params.src_origin_y + split_dir_y * g_px),
               saturatef(base_halo * (edge * 0.68f + lum_center * 0.10f +
                                      pattern_mask * 0.24f)) *
                   0.34f);

  // 5. Vibrant Color Shifting (Original smooth fluid color)
  const float color_enhance =
      params.color_enhancement / 100.0f * params.intensity;

  const float hue_angle = ((f_a + f_b) * 3.14159f * base_chroma) +
                          (params.time * base_chroma * 0.5f) +
                          ((pattern_field - 0.5f) * base_fractal * 1.35f);

  final = rotate_hue_rgba(final, hue_angle);
  final = adjust_saturation_rgba(final, 1.0f + color_enhance * 1.5f +
                                             pattern_mask * 0.42f);

  if (params.pattern_style == 2) {
    if (pattern_mask > 0.001f) {
      float rate = 1.0f + base_chroma;
      float flower_hue_shift = params.dynamic_flower_colors ? (((f_a + f_b) * 3.14159f * rate) + (params.time * speed * rate * 0.3f)) : 0.0f;
      float4 retro_color = get_flower_pixel_color_rgba(pattern_field, cx, cy, final, params.palette_blend, flower_hue_shift);
      final = mix_rgba(final, retro_color, pattern_mask);
    }
  } else if (params.pattern_style == 3) {
    if (pattern_mask > 0.001f) {
      float T = pattern_field;
      float r = 0.5f + 0.5f * cosf(6.28318f * (T * 1.0f + 0.0f));
      float g = 0.5f + 0.5f * cosf(6.28318f * (T * 1.25f + 0.15f));
      float b = 0.5f + 0.5f * cosf(6.28318f * (T * 1.55f + 0.35f));
      float4 irid_color = make_float4(r, g, b, 1.0f);
      
      // Calculate fresnel factor based on view angle (cos_t)
      float fresnel_factor = 0.1f + 0.9f * powf(1.0f - cos_t, 3.0f);
      
      // Transmission: absorb complementary wavelengths (tint transmission colors)
      float4 transmission_color = make_float4(
        1.0f - irid_color.x * 0.45f * params.palette_blend,
        1.0f - irid_color.y * 0.45f * params.palette_blend,
        1.0f - irid_color.z * 0.45f * params.palette_blend,
        1.0f
      );
      
      float4 transmitted = make_float4(
        final.x * transmission_color.x,
        final.y * transmission_color.y,
        final.z * transmission_color.z,
        final.w
      );
      
      // Reflection: add colored reflections scaled by fresnel
      float4 reflected = make_float4(
        irid_color.x * fresnel_factor * (0.5f + 0.5f * params.palette_blend),
        irid_color.y * fresnel_factor * (0.5f + 0.5f * params.palette_blend),
        irid_color.z * fresnel_factor * (0.5f + 0.5f * params.palette_blend),
        0.0f
      );
      
      float4 target_color = clamp_rgba(make_float4(
        transmitted.x + reflected.x,
        transmitted.y + reflected.y,
        transmitted.z + reflected.z,
        transmitted.w
      ));
      
      // Volumetric vignette: slightly darker in the center for a 3D spherical look
      if (params.radius > 0.01f) {
        float r_norm = dist / params.radius;
        float vol_factor = 0.88f + 0.12f * r_norm * r_norm;
        target_color.x *= vol_factor;
        target_color.y *= vol_factor;
        target_color.z *= vol_factor;
      }
      
      final = mix_rgba(final, target_color, pattern_mask);
    }
  }

  if (params.pattern_style == 3 && params.radius > 0.01f) {
    float nx = dx_c_scaled / params.radius;
    float ny = dy_c / params.radius;
    float nz = sqrtf(fmaxf(0.0f, 1.0f - nx * nx - ny * ny));
    
    float dot_NL1 = -0.3f * nx - 0.3f * ny + 0.916f * nz;
    float spec = powf(fmaxf(0.0f, dot_NL1), 25.0f) * 0.70f;
    
    float dot_NL2 = 0.3f * nx + 0.3f * ny + 0.916f * nz;
    spec += powf(fmaxf(0.0f, dot_NL2), 40.0f) * 0.25f;
    
    spec *= weight;
    
    final.x = saturatef(final.x + spec);
    final.y = saturatef(final.y + spec);
    final.z = saturatef(final.z + spec);
  } else {
    if (pattern_mask > 0.001f) {
      const float tint_angle = (pattern_field - 0.5f) * 3.2f + f_b * base_fractal;
      final =
          mix_rgba(final, rotate_hue_rgba(final, tint_angle),
                   saturatef(pattern_mask * 0.35f));
    }
  }

  const float lift = (base_halo * edge * (0.06f + lum_center * 0.05f)) +
                     (pattern_mask * 0.07f);
  final.x = saturatef(final.x + lift * (0.22f + pattern_field * 0.24f));
  final.y = saturatef(final.y + lift * 0.18f);
  final.z =
      saturatef(final.z + lift * (0.28f + (1.0f - pattern_field) * 0.18f));

  const float final_mix = saturatef(params.effect_mix * weight);
  if (final_mix < 0.999f) {
    const float4 clean_px = sample_rgba(src, src_pitch, width, height, abs_x - params.src_origin_x, abs_y - params.src_origin_y);
    final = mix_rgba(clean_px, final, final_mix);
  }

  if (params.respect_source_alpha) {
    float4 transparent_px = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
    final = mix_rgba(transparent_px, final, center.w);
  }

  reinterpret_cast<float4 *>(dst + y * dst_pitch)[x] =
      aetk_rgba_to_bgra(clamp_rgba(final));
}

} // namespace

extern "C" void launch_psychedelia_kernel(const void *src_ptr,
                                          gpu_history_batch batch,
                                          void *dst_ptr, int width, int height,
                                          int src_pitch, int dst_pitch,
                                          psychedelia_render_params params,
                                          cudaStream_t stream) {
  dim3 block(16, 16);
  dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);
  psychedelia_kernel<<<grid, block, 0, stream>>>(
      static_cast<const unsigned char *>(src_ptr), batch,
      static_cast<unsigned char *>(dst_ptr), width, height, src_pitch,
      dst_pitch, params);
}

#include <aetk/effect/gpu/kernels/swizzle.cu>
