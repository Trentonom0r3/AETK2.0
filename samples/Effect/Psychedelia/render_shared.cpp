#include "render_shared.hpp"
#include <aetk/effect/gpu/suite.hpp>
#include <aetk/effect/params/param.hpp>
#include <aetk/effect/plugin.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cuda_runtime.h>
#include <vector>

extern "C" void launch_psychedelia_kernel(const void* src_ptr, gpu_history_batch batch,
    void* dst_ptr, int width, int height, int src_pitch, int dst_pitch,
    psychedelia_render_params params, cudaStream_t stream);

using color = aetk::core::color<pixel_range::tkuint8>;

namespace {

constexpr float k_eps = 1.0e-5f;
constexpr float k_tau = 6.28318530718f;

struct state_profile {
    float geometry = 1.0f;
    float tracers = 1.0f;
    float chroma = 1.0f;
    float split = 1.0f;
    float halo = 1.0f;
    float pattern = 1.0f;
    float speed = 1.0f;
    float detail = 1.0f;
};

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

std::uint32_t hash_u32(int x, int y) {
    std::uint32_t n = static_cast<std::uint32_t>(x) * 0x1f123bb5u;
    n ^= static_cast<std::uint32_t>(y) * 0x05491333u;
    n ^= n >> 15;
    n *= 0x2c1b3c6du;
    n ^= n >> 12;
    n *= 0x297a2d39u;
    n ^= n >> 15;
    return n;
}

float value_hash(int x, int y) {
    return static_cast<float>(hash_u32(x, y) & 0x00ffffffu) / 16777215.0f;
}

float smooth_step(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float smooth_range(float edge0, float edge1, float value) {
    return smooth_step((value - edge0) / (edge1 - edge0 + k_eps));
}

float value_noise(float x, float y) {
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(ix);
    const float fy = y - static_cast<float>(iy);
    const float u = smooth_step(fx);
    const float v = smooth_step(fy);

    const float a = value_hash(ix, iy);
    const float b = value_hash(ix + 1, iy);
    const float c = value_hash(ix, iy + 1);
    const float d = value_hash(ix + 1, iy + 1);

    return lerp(lerp(a, b, u), lerp(c, d, u), v);
}

float quality_gain(int quality) {
    return (quality == 1) ? 0.82f : (quality == 3) ? 1.18f : 1.0f;
}

float fbm(float x, float y, float phase, int octaves) {
    float sum = 0.0f;
    float amplitude = 0.62f;
    float frequency = 1.0f;

    for (int octave = 0; octave < octaves; ++octave) {
        sum += amplitude
            * value_noise(x * frequency + phase * (0.35f + octave * 0.09f),
                y * frequency - phase * (0.27f + octave * 0.07f));
        frequency *= 2.15f;
        amplitude *= 0.62f;
        x += 13.7f;
        y -= 9.1f;
    }
    return sum;
}

float folded_fractal_field(float u, float v, float center_u, float center_v,
    float aspect, float detail_freq, float flow_a, float flow_b, float time,
    float speed, int octaves) {
    const float px = (u - center_u) * aspect;
    const float py = v - center_v;
    const float radius = std::sqrt(px * px + py * py) + k_eps;
    const float theta = std::atan2(py, px);

    const float folds = 7.0f;
    const float sector = k_tau / folds;
    const float phase = time * speed * 0.16f;

    float local_theta = std::fmod(theta + phase, sector);
    if (local_theta < 0.0f) {
        local_theta += sector;
    }
    local_theta = std::fabs(local_theta - sector * 0.5f);

    float qx = std::cos(local_theta) * radius;
    float qy = std::sin(local_theta) * radius;

    float folded_accum = 0.0f;
    float folded_weight = 0.58f;
    for (int i = 0; i < 3; ++i) {
        qx = std::fabs(qx);
        qy = std::fabs(qy);

        const float rot = phase * (0.72f + 0.13f * i) + 0.54f * i;
        const float cs = std::cos(rot);
        const float sn = std::sin(rot);
        const float rx = qx * cs - qy * sn;
        const float ry = qx * sn + qy * cs;
        qx = rx;
        qy = ry;

        const float orbit = std::sqrt(qx * qx + qy * qy);
        const float diagonal_lace
            = 1.0f - smooth_range(0.0f, 0.050f + 0.012f * i, std::fabs(qx - qy));
        const float ring_lace = 1.0f
            - smooth_range(0.0f, 0.075f + 0.016f * i,
                std::fabs(orbit - (0.10f + 0.075f * i)));
        folded_accum += (diagonal_lace * 0.56f + ring_lace * 0.44f) * folded_weight;

        const float scale = 1.56f + 0.12f * i;
        qx = qx * scale - (0.105f + 0.022f * i);
        qy = qy * scale - (0.070f + 0.018f * i);
        folded_weight *= 0.52f;
    }

    const float domain = fbm(qx * detail_freq * 1.70f + flow_a * 1.25f,
        qy * detail_freq * 1.70f - flow_b * 1.15f, time * speed * 0.21f,
        octaves);
    float ridge = 1.0f - std::fabs(domain * 2.0f - 1.0f);
    ridge *= ridge;

    const float spiral = 0.5f
        + 0.5f
            * std::sin(radius * detail_freq * 7.0f - theta * 2.0f + domain * 5.0f
                - time * speed * 0.52f);

    return smooth_range(0.18f, 0.86f,
        folded_accum * 0.48f + ridge * 0.34f + spiral * 0.18f);
}

// Optimized hue rotation using precalculated sin/cos
color apply_hue_matrix(const color& c, float cosine, float sine) {
    const float r = static_cast<float>(c.red);
    const float g = static_cast<float>(c.green);
    const float b = static_cast<float>(c.blue);

    const float shared = ((r + g + b) / 3.0f) * (1.0f - cosine);
    const float axis = 0.57735026919f;

    const float rr = (r * cosine) + (axis * (b - g) * sine) + shared;
    const float gg = (g * cosine) + (axis * (r - b) * sine) + shared;
    const float bb = (b * cosine) + (axis * (g - r) * sine) + shared;

    return color(c.alpha, rr, gg, bb).clamped();
}

color rotate_hue(const color& c, float angle) {
    return apply_hue_matrix(c, std::cos(angle), std::sin(angle));
}

float groovy_flower_field(float u, float v, float center_u, float center_v,
    float aspect, float detail, float flow_a, float flow_b, float time,
    float speed, float density, int& chosen_cx, int& chosen_cy) {
    float px = (u - center_u) * aspect;
    float py = v - center_v;

    px += (flow_a - 0.5f) * 0.05f;
    py += (flow_b - 0.5f) * 0.05f;

    float Rc = 0.15f;
    float max_layer_val = -1.0e9f;
    float final_pattern_val = 0.0f;

    if (density > 0.01f) {
        float cells_y = (std::max)(1.0f, (density / 100.0f) * 30.0f);
        float grid_size = 1.0f / cells_y;

        int cell_x = static_cast<int>(std::floor(px / grid_size + 0.5f));
        int cell_y = static_cast<int>(std::floor(py / grid_size + 0.5f));

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

                float rotation = hash_x * k_tau + time * speed * (0.2f + 0.5f * hash_y);

                float r = std::sqrt(lx * lx + ly * ly) + k_eps;
                float theta = std::atan2(ly, lx) - rotation;

                int N = 5 + static_cast<int>(hash_x * 7.0f);
                float petal_shape = std::cos(N * theta);
                float r_bound = Rc;
                if (hash_y < 0.4f) {
                    r_bound += 0.15f * (0.5f + 0.5f * petal_shape);
                } else if (hash_y < 0.7f) {
                    r_bound += 0.16f * std::sqrt((std::max)(0.0f, 0.5f + 0.5f * petal_shape));
                } else {
                    r_bound += 0.18f * std::pow((std::max)(0.0f, 0.5f + 0.5f * petal_shape), 3.0f);
                }

                float flower_dist = r - r_bound;
                float center_dist = r - Rc;

                float cell_pattern = 0.0f;
                float w_border = 0.025f;

                if (center_dist < -w_border) {
                    cell_pattern = 1.0f;
                } else if (std::abs(center_dist) <= w_border) {
                    cell_pattern = 0.8f;
                } else if (flower_dist < -w_border) {
                    cell_pattern = 0.5f;
                } else if (std::abs(flower_dist) <= w_border) {
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

        float r = std::sqrt(lx * lx + ly * ly) + k_eps;
        float theta = std::atan2(ly, lx) - rotation;

        int N = 8;
        float petal_shape = std::cos(N * theta);
        float r_bound = Rc + 0.25f * (0.5f + 0.5f * petal_shape);

        float flower_dist = r - r_bound;
        float center_dist = r - Rc;

        float cell_pattern = 0.0f;
        float w_border = 0.025f;

        if (center_dist < -w_border) {
            cell_pattern = 1.0f;
        } else if (std::abs(center_dist) <= w_border) {
            cell_pattern = 0.8f;
        } else if (flower_dist < -w_border) {
            cell_pattern = 0.5f;
        } else if (std::abs(flower_dist) <= w_border) {
            cell_pattern = 0.2f;
        }

        final_pattern_val = cell_pattern;
    }

    return final_pattern_val;
}

color get_flower_pixel_color(float pattern_val, int cx, int cy, const color& source_color, float palette_blend, float hue_shift) {
    float hash_x = value_hash(cx, cy);
    float hash_y = value_hash(cx + 37, cy + 93);

    // 5 colors in the retro palette (adding dark brown/outline)
    color palette[5] = {
        color(255, 240, 225, 185), // Cream
        color(255, 230, 170, 30),  // Yellow
        color(255, 220, 80, 20),   // Orange
        color(255, 30, 140, 130),  // Teal
        color(255, 50, 35, 30)     // Dark Brown/Outline
    };

    int petal_idx = static_cast<int>(hash_x * 4.0f);
    int center_idx = (petal_idx + 1 + static_cast<int>(hash_y * 3.0f)) % 4;

    color target_color;
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
        target_color = rotate_hue(target_color, hue_shift);
    }

    if (palette_blend < 0.999f) {
        if ((pattern_val > 0.70f && pattern_val <= 0.85f) || pattern_val <= 0.40f) {
            color dark_source = color(source_color.alpha, source_color.red * 0.3, source_color.green * 0.3, source_color.blue * 0.3);
            return color::mix(dark_source, target_color, palette_blend);
        }
        return color::mix(source_color, target_color, palette_blend);
    }
    return target_color;
}

color get_retro_palette_color(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (t < 0.25f) {
        float w = t / 0.25f;
        r = lerp(0.94f, 0.90f, w);
        g = lerp(0.88f, 0.67f, w);
        b = lerp(0.73f, 0.12f, w);
    } else if (t < 0.50f) {
        float w = (t - 0.25f) / 0.25f;
        r = lerp(0.90f, 0.86f, w);
        g = lerp(0.67f, 0.31f, w);
        b = lerp(0.12f, 0.08f, w);
    } else if (t < 0.75f) {
        float w = (t - 0.50f) / 0.25f;
        r = lerp(0.86f, 0.12f, w);
        g = lerp(0.31f, 0.55f, w);
        b = lerp(0.08f, 0.51f, w);
    } else {
        float w = (t - 0.75f) / 0.25f;
        r = lerp(0.12f, 0.94f, w);
        g = lerp(0.55f, 0.88f, w);
        b = lerp(0.51f, 0.73f, w);
    }
    return color(255, r * 255.0f, g * 255.0f, b * 255.0f);
}

state_profile profile_for_state(int state) {
    switch (state) {
    case 1:
        return { 0.58f, 0.20f, 0.28f, 0.22f, 0.22f, 0.30f, 0.85f, 0.92f };
    case 3:
        return { 1.42f, 1.08f, 1.18f, 1.34f, 0.92f, 1.22f, 1.12f, 1.24f };
    case 2:
    default:
        return { 0.96f, 0.62f, 0.74f, 0.82f, 0.58f, 0.76f, 1.0f, 1.08f };
    }
}

} // namespace

// --- Noise Grid Implementation ---
void noise_grid::build(
    int w, int h, int downsample_scale, float time, float speed, int octaves,
    float detail_freq, float center_u, float center_v, bool build_pattern, int pattern_style, float flower_density) {
    // Lower-resolution noise is good enough for the CPU path and cuts build cost a lot.
    downsample_scale = (std::max)(1, downsample_scale);
    width = (std::max)(1, w / downsample_scale);
    height = (std::max)(1, h / downsample_scale);
    has_pattern = build_pattern;

    flow_a.resize(width * height);
    flow_b.resize(width * height);
    pattern.resize(width * height);

    const float aspect = static_cast<float>(w) / static_cast<float>((std::max)(1, h));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
            float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);

            float f_a = fbm(u * detail_freq + 2.1f, v * detail_freq - 4.5f,
                time * speed * 0.11f, octaves);
            float f_b = fbm(u * detail_freq - 7.4f, v * detail_freq + 3.8f,
                -time * speed * 0.09f, octaves);

            int dummy_cx = 0, dummy_cy = 0;
            const float p_n = build_pattern
                ? (pattern_style == 2
                    ? groovy_flower_field(u, v, center_u, center_v, aspect,
                        (detail_freq - 3.5f) / 12.5f, f_a, f_b, time, speed, flower_density, dummy_cx, dummy_cy)
                    : folded_fractal_field(u, v, center_u, center_v, aspect,
                        detail_freq, f_a, f_b, time, speed, octaves + 1))
                : 0.0f;

            int idx = y * width + x;
            flow_a[idx] = f_a;
            flow_b[idx] = f_b;
            pattern[idx] = p_n;
        }
    }
}

void noise_grid::sample(
    float u, float v, float& out_a, float& out_b, float& out_p) const {
    if (width <= 1 || height <= 1) {
        out_a = flow_a.empty() ? 0.0f : flow_a.front();
        out_b = flow_b.empty() ? 0.0f : flow_b.front();
        out_p = pattern.empty() ? 0.0f : pattern.front();
        return;
    }

    float x = u * (width - 1);
    float y = v * (height - 1);
    int ix = std::clamp(static_cast<int>(x), 0, width - 2);
    int iy = std::clamp(static_cast<int>(y), 0, height - 2);
    float fx = x - static_cast<float>(ix);
    float fy = y - static_cast<float>(iy);

    int idx00 = iy * width + ix;
    int idx10 = idx00 + 1;
    int idx01 = idx00 + width;
    int idx11 = idx01 + 1;

    auto bilerp = [&](const std::vector<float>& buf) {
        float c0 = buf[idx00] + (buf[idx10] - buf[idx00]) * fx;
        float c1 = buf[idx01] + (buf[idx11] - buf[idx01]) * fx;
        return c0 + (c1 - c0) * fy;
    };

    out_a = bilerp(flow_a);
    out_b = bilerp(flow_b);
    out_p = has_pattern ? bilerp(pattern) : 0.0f;
}

// --- Constants Builder ---
frame_constants build_frame_constants(
    const psychedelia_render_params& params, int width, int height) {
    frame_constants fc;
    state_profile profile = profile_for_state(params.state);
    float quality_val = quality_gain(params.quality);

    fc.base_tracer = params.intensity * params.tracers * profile.tracers;
    fc.base_chroma = params.intensity * params.spectrum * profile.chroma;
    fc.base_split = params.intensity * params.color_separation * profile.split;
    fc.base_halo = params.intensity * params.halo * profile.halo;
    fc.base_fractal = params.intensity * params.fractal * profile.pattern * quality_val;

    fc.speed = (std::max)(0.05f, params.speed * profile.speed);
    fc.detail = (std::max)(0.35f, params.detail * profile.detail);
    fc.min_dim = static_cast<float>((std::min)(width, height));
    fc.inv_full_width = 1.0f / static_cast<float>((std::max)(1, width));
    fc.inv_full_height = 1.0f / static_cast<float>((std::max)(1, height));
    fc.center_u = params.center_x * fc.inv_full_width;
    fc.center_v = params.center_y * fc.inv_full_height;
    const float radius_x_scale
        = static_cast<float>(width) / static_cast<float>((std::max)(1, height));
    fc.radius_x_scale_sq = radius_x_scale * radius_x_scale;

    fc.center_x = params.center_x;
    fc.center_y = params.center_y;
    fc.detail_freq = 3.5f + fc.detail * 12.5f;

    fc.breathing_mult = params.breathing * params.intensity * profile.geometry;
    fc.morphing_amount = params.morphing / 100.0f * params.intensity;
    fc.melting_amount = params.melting / 100.0f * params.intensity;
    fc.flowing_amount = params.flowing / 100.0f * params.intensity;

    fc.micro_scale = fc.min_dim * 0.0075f * fc.base_fractal;

    fc.magnify = params.magnification / 100.0f * params.intensity;
    fc.sharpen_amount = (params.acuity / 100.0f) * params.intensity;
    fc.color_enhance = params.color_enhancement / 100.0f * params.intensity;

    float solidity_normalized = params.tracer_solidity / 100.0f;
    fc.tracer_base_weight = lerp(0.15f, 0.60f, solidity_normalized);
    fc.tracer_decay_rate = lerp(0.40f, 0.90f, solidity_normalized);

    float hue_shift_rate = fc.base_chroma * 0.5f;
    for (int i = 0; i < 16; ++i) {
        float angle = hue_shift_rate * static_cast<float>(i + 1);
        fc.history_cos[i] = std::cos(angle);
        fc.history_sin[i] = std::sin(angle);
    }

    return fc;
}

psychedelia_render_params read_render_params(const context& ctx) {
    psychedelia_render_params params;
    params.time = static_cast<float>(ctx.current_time().as_seconds());
    params.intensity = ctx.float_val("Master Intensity") / 100.0f;
    params.effect_mix = ctx.float_val("Reality Mix") / 100.0f;
    params.breathing = ctx.float_val("Breathing") / 100.0f;
    params.tracers = ctx.float_val("Tracers") / 100.0f;
    params.spectrum = ctx.float_val("Spectrum Drift") / 100.0f;
    params.color_separation = ctx.float_val("Color Separation") / 100.0f;
    params.halo = ctx.float_val("Edge Halo") / 100.0f;
    params.fractal = ctx.float_val("Fractal Geometry") / 100.0f;
    params.speed = ctx.float_val("Breath Speed");
    params.detail = ctx.float_val("Detail Scale");
    params.tracer_drift = ctx.float_val("Tracer Drift");
    params.state = ctx.int_val("Visual State");
    params.quality = ctx.int_val("Render Quality");
    params.allow_gpu = ctx.bool_val("Use GPU When Available");
    params.respect_source_alpha = ctx.bool_val("Respect Source Alpha") ? 1 : 0;
    params.dynamic_flower_colors = ctx.bool_val("Dynamic Flower Colors") ? 1 : 0;
    params.pattern_style = ctx.int_val("Pattern Style");
    params.flower_density = ctx.float_val("Flower Density");
    params.palette_blend = ctx.float_val("Palette Blend") / 100.0f;
    params.history_frames = 0;
    params.drift_spread = ctx.float_val("Drift Spread");
    params.tracer_solidity = ctx.float_val("Tracer Solidity");
    params.acuity = ctx.float_val("Visual Acuity");
    params.color_enhancement = ctx.float_val("Color Enhancement");
    params.magnification = ctx.float_val("Magnification");
    params.melting = ctx.float_val("Melting");
    params.flowing = ctx.float_val("Flowing");
    params.morphing = ctx.float_val("Morphing");

    float ds_x = 1.0f;
    if (ctx.in_data_ptr()) {
        auto &ds = ctx.in_data_ptr()->downsample_x;
        if (ds.den > 0) {
            ds_x = static_cast<float>(ds.num) / static_cast<float>(ds.den);
        }
    }

    auto pt = ctx.point_val(param_id::center_point);
    params.center_x = static_cast<float>(pt.x);
    params.center_y = static_cast<float>(pt.y);
    params.radius = ctx.float_val(param_id::radius) * ds_x;
    params.feather = ctx.float_val(param_id::feather) * ds_x;

    return params;
}

int cpu_history_limit(int quality) {
    switch (quality) {
    case 1:
        return 1;
    case 3:
        return 4;
    case 2:
    default:
        return 2;
    }
}

template <typename PixelT, bool IsBGRA>
inline color read_pixel_fast(const PF_EffectWorld* world, int x, int y) {
    x = std::clamp(x, 0, (int)world->width - 1);
    y = std::clamp(y, 0, (int)world->height - 1);
    const char* row = reinterpret_cast<const char*>(world->data) + (y * world->rowbytes);
    const PixelT* px = reinterpret_cast<const PixelT*>(row) + x;
    return pixel_accessor<PixelT, IsBGRA, pixel_range::tkuint8>::read(px);
}

template <typename PixelT, bool IsBGRA>
inline color read_pixel_unchecked(const PF_EffectWorld* world, int x, int y) {
    const char* row = reinterpret_cast<const char*>(world->data)
        + (static_cast<ptrdiff_t>(y) * world->rowbytes);
    const PixelT* px = reinterpret_cast<const PixelT*>(row) + x;
    return pixel_accessor<PixelT, IsBGRA, pixel_range::tkuint8>::read(px);
}

template <typename PixelT, bool IsBGRA>
inline color sample_bilinear_fast(const PF_EffectWorld* world, float x, float y) {
    const int raw_x0 = static_cast<int>(std::floor(x));
    const int raw_y0 = static_cast<int>(std::floor(y));
    const float tx = x - static_cast<float>(raw_x0);
    const float ty = y - static_cast<float>(raw_y0);

    const int max_x = static_cast<int>(world->width) - 1;
    const int max_y = static_cast<int>(world->height) - 1;
    const int x0 = std::clamp(raw_x0, 0, max_x);
    const int x1 = std::clamp(raw_x0 + 1, 0, max_x);
    const int y0 = std::clamp(raw_y0, 0, max_y);
    const int y1 = std::clamp(raw_y0 + 1, 0, max_y);

    const auto c00 = read_pixel_unchecked<PixelT, IsBGRA>(world, x0, y0);
    const auto c10 = read_pixel_unchecked<PixelT, IsBGRA>(world, x1, y0);
    const auto c01 = read_pixel_unchecked<PixelT, IsBGRA>(world, x0, y1);
    const auto c11 = read_pixel_unchecked<PixelT, IsBGRA>(world, x1, y1);

    return color::mix(color::mix(c00, c10, tx),
                      color::mix(c01, c11, tx), ty);
}

template <typename PixelT, bool IsBGRA>
color shade_pixel_fast(const smart_world& input, const std::vector<smart_world>& history,
    int x, int y, const psychedelia_render_params& params,
    const frame_constants& fc, const noise_grid& noise, int history_limit) {
    const auto* input_world = input.ptr();

    const float abs_x = static_cast<float>(x + params.origin_x);
    const float abs_y = static_cast<float>(y + params.origin_y);

    const float u = (abs_x + 0.5f) * fc.inv_full_width;
    const float v = (abs_y + 0.5f) * fc.inv_full_height;

    const float du = u - fc.center_u;
    const float dv = v - fc.center_v;
    const float field_radius
        = std::sqrt((du * du * fc.radius_x_scale_sq) + (dv * dv));

    // Compute distance from center in pixels with PAR compensation
    const float dx_c = abs_x - params.center_x;
    const float dy_c = abs_y - params.center_y;
    const float dx_c_scaled = dx_c * params.par;
    const float dist2 = dx_c_scaled * dx_c_scaled + dy_c * dy_c;
    const float base_src_x = abs_x - params.src_origin_x;
    const float base_src_y = abs_y - params.src_origin_y;

    float cos_t = 1.0f;
    if (params.radius > 0.01f) {
        float dist = std::sqrt(dist2);
        float r_norm = (std::min)(1.0f, dist / params.radius);
        cos_t = std::sqrt((std::max)(0.0f, 1.0f - (r_norm * r_norm) / 1.77f));
    }

    float weight = 1.0f;
    if (params.radius > 0.01f) {
        const float r_inner = (std::max)(0.0f, params.radius - params.feather);
        const float r_outer = params.radius;
        const float r_inner2 = r_inner * r_inner;
        const float r_outer2 = r_outer * r_outer;
        if (dist2 >= r_outer2) {
            weight = 0.0f;
        } else if (dist2 > r_inner2) {
            const float dist = std::sqrt(dist2);
            const float t = (dist - r_inner) / (r_outer - r_inner + 1e-5f);
            weight = 1.0f - (t * t * (3.0f - 2.0f * t));
        }
    }

    if (params.radius > 0.01f && weight <= 0.0f) {
        return read_pixel_fast<PixelT, IsBGRA>(input_world, static_cast<int>(base_src_x), static_cast<int>(base_src_y));
    }

    // 1. Lightning fast noise fetch
    float flow_a, flow_b, pattern_noise;
    noise.sample(u, v, flow_a, flow_b, pattern_noise);
    const bool use_fractal = fc.base_fractal > 0.001f;

    float dx = (flow_a - 0.5f) * fc.min_dim * 0.025f * fc.morphing_amount;
    float dy = (flow_b - 0.5f) * fc.min_dim * 0.025f * fc.morphing_amount;

    if (fc.breathing_mult > 0.0001f) {
        const float wave_x
            = std::sin(u * fc.detail_freq - params.time * fc.speed * 1.5f + flow_a * 3.5f)
            * fc.breathing_mult;
        const float wave_y
            = std::sin(v * fc.detail_freq + params.time * fc.speed * 1.3f + flow_b * 3.5f)
            * fc.breathing_mult;
        dx += wave_x * fc.min_dim * 0.012f;
        dy += wave_y * fc.min_dim * 0.012f;
    }

    dy += fc.melting_amount * fc.min_dim * 0.035f * (flow_a + 0.1f);
    dx += fc.flowing_amount * fc.min_dim * 0.02f * flow_a;
    dy += fc.flowing_amount * fc.min_dim * 0.02f * flow_b;

    if (fc.micro_scale > 0.001f) {
        const float fractal_phase = (params.pattern_style >= 2 ? 0.0f : pattern_noise) * k_tau + field_radius * 5.5f
            - params.time * fc.speed * 0.35f;
        dx += fc.micro_scale
            * (std::cos(fractal_phase) * 0.70f + (flow_a - 0.5f) * 0.55f);
        dy += fc.micro_scale
            * (std::sin(fractal_phase) * 0.70f + (flow_b - 0.5f) * 0.55f);
    }

    dx *= weight;
    dy *= weight;

    float sx = abs_x + dx;
    float sy = abs_y + dy;

    if (params.pattern_style == 3 && params.radius > 0.01f) {
        const float dist = std::sqrt(dist2);
        if (dist < params.radius) {
            float r_norm = dist / params.radius;
            float zoom = 1.0f + 0.15f * params.detail * std::sqrt((std::max)(0.0f, 1.0f - r_norm * r_norm));
            sx = params.center_x + (abs_x - params.center_x) / zoom + dx;
            sy = params.center_y + (abs_y - params.center_y) / zoom + dy;
        }
    }

    if (fc.magnify > 0.01f) {
        if (params.radius > 0.01f) {
            const float dist = std::sqrt(dist2);
            if (dist < params.radius) {
                float p = dist / params.radius;
                float min_zoom = 1.0f / (1.0f + fc.magnify * 0.65f);
                float falloff = (1.0f - p * p);
                float zoom = 1.0f - (1.0f - min_zoom) * falloff * falloff;
                sx = params.center_x + (sx - params.center_x) * zoom;
                sy = params.center_y + (sy - params.center_y) * zoom;
            }
        } else {
            // Fallback to standard zoom if radius is 0
            const float zoom = 1.0f / (1.0f + fc.magnify * 0.35f);
            sx = fc.center_x + (sx - fc.center_x) * zoom;
            sy = fc.center_y + (sy - fc.center_y) * zoom;
        }
    }

    const float src_x = sx - params.src_origin_x;
    const float src_y = sy - params.src_origin_y;
    const color center = sample_bilinear_fast<PixelT, IsBGRA>(input_world, src_x, src_y);
    color sharp_center = center;
    float lum_left = 0, lum_right = 0, lum_up = 0, lum_down = 0;

    // 4. Visual Acuity Enhancement (Early out to save 4 fetches)
    const bool preview_quality = params.quality == 1;
    if (!preview_quality
        && (fc.sharpen_amount > 0.01f || fc.base_halo > 0.01f || fc.base_split > 0.01f
            || fc.base_fractal > 0.01f)) {
        const color left = sample_bilinear_fast<PixelT, IsBGRA>(input_world, src_x - 1.5f, src_y);
        const color right = sample_bilinear_fast<PixelT, IsBGRA>(input_world, src_x + 1.5f, src_y);
        const color up = sample_bilinear_fast<PixelT, IsBGRA>(input_world, src_x, src_y - 1.5f);
        const color down = sample_bilinear_fast<PixelT, IsBGRA>(input_world, src_x, src_y + 1.5f);

        lum_left = (float)left.luminance();
        lum_right = (float)right.luminance();
        lum_up = (float)up.luminance();
        lum_down = (float)down.luminance();

        if (fc.sharpen_amount > 0.01f) {
            sharp_center.red = center.red
                + (center.red - (left.red + right.red + up.red + down.red) * 0.25f)
                    * fc.sharpen_amount;
            sharp_center.green = center.green
                + (center.green
                      - (left.green + right.green + up.green + down.green) * 0.25f)
                    * fc.sharpen_amount;
            sharp_center.blue = center.blue
                + (center.blue - (left.blue + right.blue + up.blue + down.blue) * 0.25f)
                    * fc.sharpen_amount;
            sharp_center = sharp_center.clamped();
        }
    }

    const float lum_center = (float)sharp_center.luminance();
    const float edge = std::clamp(
        (std::fabs(lum_right - lum_left) + std::fabs(lum_down - lum_up)) * (0.88f / 255.0f), 0.0f,
        1.0f);
    const float midtone = 1.0f - std::fabs((lum_center * (2.0f / 255.0f)) - 1.0f);

    const float peripheral = std::clamp((field_radius - 0.06f) / 0.62f, 0.0f, 1.0f);

    float pattern_field = 0.5f;
    float pattern_mask = 0.0f;
    int cx = 0, cy = 0;
    if (params.pattern_style == 3 || use_fractal) {
        if (params.pattern_style == 2) {
            float aspect = static_cast<float>(params.full_width) / static_cast<float>((std::max)(1, params.full_height));
            pattern_field = groovy_flower_field(u, v, fc.center_u, fc.center_v, aspect, params.detail, flow_a, flow_b, params.time, fc.speed, params.flower_density, cx, cy);
            pattern_mask = std::clamp(params.intensity * params.fractal, 0.0f, 1.0f) * (pattern_field > 0.1f ? 1.0f : 0.0f);
        } else if (params.pattern_style == 3) {
            pattern_field = (0.2f + 2.5f * params.detail) * cos_t +
                            1.8f * (flow_a * 0.6f + flow_b * 0.4f) * (params.fractal / 100.0f * params.intensity) +
                            params.time * fc.speed * 0.15f;
            float fresnel = 0.05f + 0.95f * std::pow(1.0f - cos_t, 3.0f);
            pattern_mask = std::clamp(params.intensity * (params.fractal / 100.0f + 0.2f), 0.0f, 1.0f) * weight * fresnel;
        } else {
            pattern_field = std::clamp(pattern_noise * 0.72f
                    + (0.5f
                          + std::sin(field_radius * 18.0f * fc.detail + flow_a * 2.4f
                                - params.time * fc.speed * 0.45f)
                              * 0.5f)
                        * 0.28f,
                0.0f, 1.0f);
            pattern_mask = std::clamp(fc.base_fractal
                             * (0.06f + edge * 0.62f + peripheral * 0.18f + midtone * 0.20f),
                          0.0f, 1.0f)
                * smooth_range(0.28f, 0.82f, pattern_field);
        }
    }

    // --- Multi-Frame Accumulation (Distinct Motion Tracers) ---
    color traced = sharp_center;

    if (!history.empty() && history_limit > 0) {
        float current_weight
            = fc.tracer_base_weight * std::clamp(fc.base_tracer, 0.0f, 1.0f);
        color accum = sharp_center;
        const int limit = (std::min)(history_limit, static_cast<int>(history.size()));

        for (int i = 0; i < limit && current_weight > 0.006f; ++i) {
            const auto& past_frame = history[i];
            const int step = i + 1;
            float sample_x
                = sx + dx * (params.tracer_drift / 100.0f) * static_cast<float>(step);
            float sample_y
                = sy + dy * (params.tracer_drift / 100.0f) * static_cast<float>(step);
            const float sample_src_x = sample_x - params.src_origin_x;
            const float sample_src_y = sample_y - params.src_origin_y;

            const auto* past_world = past_frame.ptr();
            color sample = sample_bilinear_fast<PixelT, IsBGRA>(past_world,
                sample_src_x - params.history_origin_x[step - 1],
                sample_src_y - params.history_origin_y[step - 1]);
            // Fast hue rotation using precalculated sin/cos
            sample = apply_hue_matrix(
                sample, fc.history_cos[step - 1], fc.history_sin[step - 1]);
            accum = color::mix(accum, sample, current_weight);

            current_weight *= fc.tracer_decay_rate;
        }
        traced = accum;
    }

    float len = std::sqrt(dx * dx + dy * dy);
    float split_dir_x = 1.0f, split_dir_y = 0.0f;
    if (len > 0.01f) {
        split_dir_x = dx / len;
        split_dir_y = dy / len;
    }

    const float separation_px = 0.9f
        + (fc.min_dim * 0.0036f * fc.base_split
            * (0.25f + edge * 1.02f + pattern_mask * 0.72f + peripheral * 0.24f));

    color chroma = traced;
    if (fc.base_split > 0.001f) {
        chroma.red = color::mix(traced,
            sample_bilinear_fast<PixelT, IsBGRA>(input_world,
                src_x + split_dir_x * separation_px,
                src_y + split_dir_y * separation_px),
            std::clamp(fc.base_split * 0.92f, 0.0f, 1.0f))
                         .red;
        chroma.blue = color::mix(traced,
            sample_bilinear_fast<PixelT, IsBGRA>(input_world,
                src_x - split_dir_x * separation_px,
                src_y - split_dir_y * separation_px),
            std::clamp(fc.base_split * 0.96f, 0.0f, 1.0f))
                           .blue;
    }

    const float chroma_mix
        = std::clamp(fc.base_chroma * 0.30f + fc.base_split * 0.52f + pattern_mask * 0.16f,
            0.0f, 1.0f);
    color final = chroma_mix > 0.001f ? color::mix(traced, chroma, chroma_mix) : traced;

    if (fc.base_halo > 0.001f) {
        const float glow_px = 1.0f
            + (fc.min_dim * 0.0020f * fc.base_halo
                * (0.40f + edge * 1.22f + pattern_mask * 0.45f));
        const color glow
            = sample_bilinear_fast<PixelT, IsBGRA>(input_world,
                src_x + split_dir_x * glow_px, src_y + split_dir_y * glow_px);

        final = color::mix(final, glow,
            std::clamp(
                fc.base_halo * (edge * 0.68f + lum_center * (0.10f / 255.0f) + pattern_mask * 0.24f),
                0.0f, 1.0f)
                * 0.34f);
    }

    // 5. Vibrant Color Shifting (Original smooth fluid color)
    const float hue_angle = ((flow_a + flow_b) * 3.14159f * fc.base_chroma)
        + (params.time * fc.base_chroma * 0.5f)
        + ((pattern_field - 0.5f) * fc.base_fractal * 1.35f);

    final = rotate_hue(final, hue_angle)
                .saturate(1.0f + fc.color_enhance * 1.5f + pattern_mask * 0.42f);

    if (params.pattern_style == 2) {
        if (pattern_mask > 0.001f) {
            float rate = 1.0f + fc.base_chroma;
            float flower_hue_shift = params.dynamic_flower_colors ? (((flow_a + flow_b) * 3.14159f * rate) + (params.time * fc.speed * rate * 0.3f)) : 0.0f;
            color retro_color = get_flower_pixel_color(pattern_field, cx, cy, final, params.palette_blend, flower_hue_shift);
            final = color::mix(final, retro_color, pattern_mask);
        }
    } else if (params.pattern_style == 3) {
        if (pattern_mask > 0.001f) {
            float T = pattern_field;
            float r = 0.5f + 0.5f * std::cos(6.28318f * (T * 1.0f + 0.0f));
            float g = 0.5f + 0.5f * std::cos(6.28318f * (T * 1.25f + 0.15f));
            float b = 0.5f + 0.5f * std::cos(6.28318f * (T * 1.55f + 0.35f));
            color irid_color(1.0f, r, g, b);
            
            // Calculate fresnel factor based on view angle (cos_t)
            float fresnel_factor = 0.1f + 0.9f * std::pow(1.0f - cos_t, 3.0f);
            
            // Transmission: absorb complementary wavelengths (tint transmission colors)
            color transmission_color(
                1.0f,
                1.0f - irid_color.red * 0.45f * params.palette_blend,
                1.0f - irid_color.green * 0.45f * params.palette_blend,
                1.0f - irid_color.blue * 0.45f * params.palette_blend
            );
            
            color transmitted(
                final.alpha,
                final.red * transmission_color.red,
                final.green * transmission_color.green,
                final.blue * transmission_color.blue
            );
            
            // Reflection: add colored reflections scaled by fresnel
            color reflected(
                0.0f,
                irid_color.red * fresnel_factor * (0.5f + 0.5f * params.palette_blend),
                irid_color.green * fresnel_factor * (0.5f + 0.5f * params.palette_blend),
                irid_color.blue * fresnel_factor * (0.5f + 0.5f * params.palette_blend)
            );
            
            color target_color(
                transmitted.alpha,
                std::clamp(transmitted.red + reflected.red, 0.0, 1.0),
                std::clamp(transmitted.green + reflected.green, 0.0, 1.0),
                std::clamp(transmitted.blue + reflected.blue, 0.0, 1.0)
            );
            
            // Volumetric vignette: slightly darker in the center for a 3D spherical look
            if (params.radius > 0.01f) {
                float dist = std::sqrt(dist2);
                float r_norm = dist / params.radius;
                float vol_factor = 0.88f + 0.12f * r_norm * r_norm;
                target_color.red *= vol_factor;
                target_color.green *= vol_factor;
                target_color.blue *= vol_factor;
            }
            
            final = color::mix(final, target_color, pattern_mask);
        }
        
        if (params.radius > 0.01f) {
            float dist = std::sqrt(dist2);
            float nx = dx_c_scaled / params.radius;
            float ny = dy_c / params.radius;
            float nz = std::sqrt((std::max)(0.0f, 1.0f - nx * nx - ny * ny));
            
            float dot_NL1 = -0.3f * nx - 0.3f * ny + 0.916f * nz;
            float spec = std::pow((std::max)(0.0f, dot_NL1), 25.0f) * 0.70f;
            
            float dot_NL2 = 0.3f * nx + 0.3f * ny + 0.916f * nz;
            spec += std::pow((std::max)(0.0f, dot_NL2), 40.0f) * 0.25f;
            
            spec *= weight;
            
            final.red = std::clamp(final.red + static_cast<double>(spec), 0.0, 1.0);
            final.green = std::clamp(final.green + static_cast<double>(spec), 0.0, 1.0);
            final.blue = std::clamp(final.blue + static_cast<double>(spec), 0.0, 1.0);
        }
    } else {
        if (pattern_mask > 0.001f) {
            const float fractal_tint_angle
                = (pattern_field - 0.5f) * 3.2f + flow_b * fc.base_fractal;
            final = color::mix(final, rotate_hue(final, fractal_tint_angle),
                std::clamp(pattern_mask * 0.35f, 0.0f, 1.0f));
        }
    }

    const float edge_lift
        = (fc.base_halo * edge * (0.06f + lum_center * (0.05f / 255.0f))) + (pattern_mask * 0.07f);
    final.red = std::clamp(
        (float) final.red + edge_lift * (0.22f + pattern_field * 0.24f) * 255.0f, 0.0f, 255.0f);
    final.green = std::clamp((float) final.green + edge_lift * 0.18f * 255.0f, 0.0f, 255.0f);
    final.blue = std::clamp(
        (float) final.blue + edge_lift * (0.28f + (1.0f - pattern_field) * 0.18f) * 255.0f, 0.0f,
        255.0f);

    color result = final.clamped();
    const float final_mix = std::clamp(params.effect_mix * weight, 0.0f, 1.0f);
    if (final_mix < 0.999f) {
        result = color::mix(read_pixel_fast<PixelT, IsBGRA>(input_world,
                static_cast<int>(base_src_x), static_cast<int>(base_src_y)),
            result, final_mix);
    }

    if (params.respect_source_alpha) {
        color transparent_px(0.0, 0.0, 0.0, 0.0);
        result = color::mix(transparent_px, result, center.alpha / 255.0);
    }

    return result;
}

void render_cpu_rows(const smart_render_context& ctx, const psychedelia_render_params& params,
    const smart_world& input, smart_world& output,
    const std::vector<smart_world>& history) {
    const int width = static_cast<int>(output.width());
    const int height = static_cast<int>(output.height());
    if (width <= 0 || height <= 0 || input.width() <= 0 || input.height() <= 0) {
        return;
    }

    // --- Precompute Frame Constants ---
    extern frame_constants build_frame_constants(
        const psychedelia_render_params& params, int width, int height);
    frame_constants fc
        = build_frame_constants(params, params.full_width, params.full_height);

    // --- Precompute Noise Grid (1/4 Resolution) ---
    noise_grid noise;
    int octaves = (params.quality == 1) ? 2 : (params.quality == 3) ? 4 : 3;
    const int history_limit = cpu_history_limit(params.quality);
    const bool build_pattern = (params.pattern_style == 3) || (fc.base_fractal > 0.001f);
    noise.build(params.full_width, params.full_height, params.quality == 1 ? 8 : 4,
        params.time, fc.speed, octaves, fc.detail_freq, fc.center_u, fc.center_v,
        build_pattern, params.pattern_style, params.flower_density);

    // Hoist dynamic format dispatch to top-level once per frame
    visit_pixel_format<pixel_range::tkuint8>(output.pixel_format(), output.is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
        ctx.iterate_generic(height, [&](A_long, A_long row, A_long) {
            const int y = static_cast<int>(row);
            char* out_row = reinterpret_cast<char*>(output.ptr()->data) + (y * output.ptr()->rowbytes);
            PixelT* out_row_px = reinterpret_cast<PixelT*>(out_row);

            for (int x = 0; x < width; ++x) {
                color final_color = shade_pixel_fast<PixelT, IsBGRA>(
                    input, history, x, y, params, fc, noise,
                    history_limit);
                pixel_accessor<PixelT, IsBGRA, pixel_range::tkuint8>::write(out_row_px + x, final_color);
            }
        });
    });
}

void render_gpu(const smart_render_context& ctx, const psychedelia_render_params& params,
    smart_world& src, smart_world& dst, const std::vector<smart_world>& history,
    cudaStream_t stream) {
    gpu_history_batch batch;
    batch.count = (std::min)(static_cast<int>(history.size()), 16);
    for (int i = 0; i < batch.count; ++i) {
        batch.ptrs[i] = history[i].gpu_data();
        batch.pitches[i] = static_cast<int>(history[i].rowbytes());
    }
    bool local_stream = false;
    if (!stream) {
        // get default cuda stream
        cudaStreamCreate(&stream);
        local_stream = true;
        AETK_LOG_INFO("Created default cuda stream");
    }

    launch_psychedelia_kernel(src.gpu_data(), batch, dst.gpu_data(),
        static_cast<int>(dst.width()), static_cast<int>(dst.height()),
        static_cast<int>(src.rowbytes()), static_cast<int>(dst.rowbytes()), params,
        stream);

    cudaStreamSynchronize(stream);

    if (local_stream) {
        cudaStreamDestroy(stream);
    }
}
