#pragma once
#include <aetk/core/types.hpp>
#include <aetk/effect/context/context.hpp>
#include <aetk/effect/gpu/suite.hpp>
#include <cuda_runtime.h>
#include <optional>
#include <vector>

#include "render_params.hpp"

using namespace aetk::effect;

// Structure to pass a batch of history frames to CUDA
struct gpu_history_batch {
    const void* ptrs[16];
    int pitches[16];
    int count;
};

// Precalculated constants to avoid redundant math per-pixel
struct frame_constants {
    float base_tracer;
    float base_chroma;
    float base_split;
    float base_halo;
    float base_fractal;
    float speed;
    float detail;
    float detail_freq;
    float min_dim;
    float inv_full_width;
    float inv_full_height;
    float center_u;
    float center_v;
    float radius_x_scale_sq;
    float center_x;
    float center_y;
    float micro_scale;
    float breathing_mult;
    float morphing_amount;
    float melting_amount;
    float flowing_amount;
    float magnify;
    float sharpen_amount;
    float tracer_base_weight;
    float tracer_decay_rate;
    float color_enhance;

    // Precalculated rotations for history
    float history_cos[16];
    float history_sin[16];
};

// Pre-rendered downsampled noise grid
struct noise_grid {
    int width = 0;
    int height = 0;
    bool has_pattern = true;
    std::vector<float> flow_a;
    std::vector<float> flow_b;
    std::vector<float> pattern;

    void build(int w, int h, int downsample_scale, float time, float speed,
        int octaves, float detail_freq, float center_u = 0.5f,
        float center_v = 0.5f, bool build_pattern = true, int pattern_style = 1, float flower_density = 0.0f);
    void sample(float u, float v, float& out_a, float& out_b, float& out_p) const;
};

// Forward declarations
psychedelia_render_params read_render_params(const context& ctx);

void render_gpu(const smart_render_context& ctx, const psychedelia_render_params& params,
    smart_world& src, smart_world& dst, const std::vector<smart_world>& history,
    cudaStream_t stream = nullptr);

// CPU renderer entry point
void render_cpu_rows(const smart_render_context& ctx, const psychedelia_render_params& params,
    const smart_world& input, smart_world& output,
    const std::vector<smart_world>& history);
