
#include "render_shared.hpp"
#include <aetk/effect.hpp>

#include <aetk/effect/licensing.hpp>

#include <atomic>
#include <cuda_runtime.h>
#include <memory>

using namespace aetk::effect;

static void DisposePsychedeliaParams(void* dataP) {
    if (dataP) {
        // Cast the void pointer back to our struct type and delete it
        psychedelia_render_params* params
            = static_cast<psychedelia_render_params*>(dataP);
        delete params;
    }
}

class psychedelia_plugin : public plugin<psychedelia_plugin> {
public:
    static void on_about(const context& ctx) {
        ctx.set_dialog_response("Psychedelia 1.0.0: coherent fractal geometry, "
                                "prism trails, and real GPU acceleration.");
    }

    static void on_global_setup(const global_setup_context& ctx) {
        ctx.enable_smart_render();
        ctx.enable_threaded_rendering();
        ctx.enable_temporal_checkouts();
        ctx.enable_gpu_rendering();
        ctx.enable_custom_ui();
        ctx.enable_options_button();
    }

    static void on_gpu_device_setup(const gpu_device_setup_context& ctx) {
        if (ctx.has_framework(PF_GPU_Framework_CUDA)) {
            ctx.enable_gpu_rendering();
        }
    }

    static void on_gpu_device_setdown(const gpu_device_setdown_context& ctx) {
    }

    static void on_params_setup(const params_setup_context& ctx) {
        // ---------------------------------------------------------
        // System & Rendering Controls
        // ---------------------------------------------------------
        ctx.add_checkbox("Use GPU When Available", true)
            .set_key<param_id>(param_id::use_gpu);
        ctx.add_popup("Render Quality", 3, 2, "Preview|Balanced|Rich")
            .set_key<param_id>(param_id::render_quality);
        ctx.add_checkbox("Respect Source Alpha", true)
            .set_key<param_id>(param_id::respect_source_alpha);

        // ---------------------------------------------------------
        // Master Artistic Controls
        // ---------------------------------------------------------
        ctx.add_popup("Visual State", 3, 2, "Afterglow|Open-Eye Drift|Peak")
            .set_key<param_id>(param_id::visual_state);
        ctx.add_slider("Master Intensity", 0.0f, 1000.0f, 100.0f)
            .set_key<param_id>(param_id::intensity);
        ctx.add_slider("Reality Mix", 0.0f, 100.0f, 100.0f)
            .set_key<param_id>(param_id::effect_mix);

        // ---------------------------------------------------------
        // Localized Topics
        // ---------------------------------------------------------
        ctx.add_topic("Area of Effect", [&](const params_setup_context& topic) {
            topic.add_point2d("Center Point", 50.0f, 50.0f)
                .set_key<param_id>(param_id::center_point);
            topic.add_slider("Effect Radius", 0.0f, 5000.0f, 0.0f)
                .set_key<param_id>(param_id::radius);
            topic.add_slider("Feather", 0.0f, 1000.0f, 100.0f)
                .set_key<param_id>(param_id::feather);
        });

        ctx.add_topic("Enhancements", [&](const params_setup_context& topic) {
            topic.add_slider("Visual Acuity", 0.0f, 100.0f, 30.0f)
                .set_key<param_id>(param_id::visual_acuity);
            topic.add_slider("Color Enhancement", 0.0f, 100.0f, 40.0f)
                .set_key<param_id>(param_id::color_enhancement);
            topic.add_slider("Magnification", 0.0f, 100.0f, 0.0f)
                .set_key<param_id>(param_id::magnification);
        });

        ctx.add_topic("Motion & Distortions", [&](const params_setup_context& topic) {
            topic.add_slider("Breathing", 0.0f, 100.0f, 0.0f)
                .set_key<param_id>(param_id::breathing);
            topic
                .add_slider("Breath Speed", 0.1f, 2.5f, 0.9f) // Moved from Top-Level
                .set_key<param_id>(param_id::breath_speed);
            topic.add_slider("Melting", 0.0f, 100.0f, 0.0f)
                .set_key<param_id>(param_id::melting);
            topic.add_slider("Flowing", 0.0f, 100.0f, 0.0f)
                .set_key<param_id>(param_id::flowing);
            topic.add_slider("Morphing", 0.0f, 100.0f, 0.0f)
                .set_key<param_id>(param_id::morphing);
        });

        ctx.add_topic("Geometry & Patterns", [&](const params_setup_context& topic) {
            topic
                .add_popup(
                    "Pattern Style", 3, 1, "Psychedelic Fold|Groovy Flower|Soap Bubble")
                .set_key<param_id>(param_id::pattern_style);
            topic.add_slider("Fractal Geometry", 0.0f, 100.0f, 32.0f)
                .set_key<param_id>(param_id::fractal_pattern);
            topic.add_slider("Detail Scale", 0.1f, 4.0f, 1.0f)
                .set_key<param_id>(param_id::detail_scale);
            topic.add_slider("Flower Density", 0.0f, 100.0f, 0.0f)
                .set_key<param_id>(param_id::flower_density);
            topic
                .add_checkbox(
                    "Dynamic Flower Colors", true) // Grouped with Flower Density
                .set_key<param_id>(param_id::dynamic_flower_colors);
            topic.add_slider("Palette Blend", 0.0f, 100.0f, 100.0f)
                .set_key<param_id>(param_id::palette_blend);
        });

        ctx.add_topic("Perception (Tracers)", [&](const params_setup_context& topic) {
            // Core Mechanics
            topic.add_slider("Tracers", 0.0f, 100.0f, 48.0f)
                .set_key<param_id>(param_id::tracers);
            topic.add_slider("Tracer Solidity", 0.0f, 100.0f, 60.0f)
                .set_key<param_id>(param_id::tracer_solidity);
            topic.add_slider("Tracer Drift", 0.0f, 100.0f, 30.0f)
                .set_key<param_id>(param_id::tracer_drift);
            topic.add_slider("Drift Spread", 0.0f, 100.0f, 40.0f)
                .set_key<param_id>(param_id::drift_spread);

            // Optical Modifiers
            topic.add_slider("Color Separation", 0.0f, 100.0f, 40.0f)
                .set_key<param_id>(param_id::color_separation);
            topic.add_slider("Spectrum Drift", 0.0f, 100.0f, 0.0f)
                .set_key<param_id>(param_id::spectrum_drift);
            topic.add_slider("Edge Halo", 0.0f, 100.0f, 30.0f)
                .set_key<param_id>(param_id::edge_halo);
        });
    }

    static void on_pre_render(const pre_render_context& ctx) {
        // struct is default initialized, won't throw
        auto params = std::make_unique<psychedelia_render_params>();
        *params = read_render_params(ctx);

        const double time_step = ctx.time_step().as_seconds();
        const double current_time = ctx.current_time().as_seconds();
        const bool wants_tracers = params->tracers > 0.01f;
        bool gpu_render_enabled = false;

        if (params->allow_gpu) {
            auto gpusuite = gpu_device_suite(ctx.in_data_ptr());
            PF_GPUDeviceInfo info = gpusuite.device_info(ctx.gpu_device_index());

            if (info.device_framework == PF_GPU_Framework_CUDA) {
                ctx.enable_gpu_render();
                gpu_render_enabled = true;
            }
        }

        ctx.checkout_layer(0, 0);

        params->history_frames = 0;

        if (wants_tracers && time_step > 0) {
            // 1 to 8 distinct echoes
            int max_history
                = 1 + static_cast<int>((params->tracer_drift / 100.0f) * 3.0f);

            // STRIDE: Spacing in time controlled by drift_spread.
            int frame_stride
                = 1 + static_cast<int>((params->drift_spread / 100.0f) * 3.0f);
            if (!gpu_render_enabled) {
                max_history = (std::min)(max_history,
                    params->quality == 1       ? 1
                        : params->quality == 2 ? 2
                                               : 4);
            }

            for (int i = 1; i <= max_history; ++i) {
                int frame_offset = i * frame_stride; // Calculate the gap

                if (current_time >= (time_step * frame_offset)) {
                    // Checkout the layer at the specific offset gap
                    ctx.checkout_layer_at_offset(0, i, -frame_offset);
                    params->history_frames++;
                } else {
                    break;
                }
            }
        }

        // #TODO: Write AETK helpers for setting pre-render data + cleanup!
        ctx.set_pre_render_data(params.release(), DisposePsychedeliaParams);
    }

    // using the format "on_smart_render(const smart_render_context &ctx, bool
    // is_gpu)" tells AE to use this function for GPU and CPU Smart Render calls.
    static void on_smart_render(const smart_render_context& ctx, bool is_gpu) {
        auto* pr_data = const_cast<psychedelia_render_params*>(
            ctx.pre_render_data<psychedelia_render_params>());

        if (!pr_data) {
            AETK_LOG_INFO("Exiting Earlier, No Param data");
            return;
        }

        // 1. Check out all inputs (main input + temporal history frames) BEFORE the
        // output
        auto src = ctx.checkout_pixels(0);

        const psychedelia_render_params& effective = *pr_data;
        std::vector<smart_world> history;
        history.reserve(static_cast<size_t>(effective.history_frames));
        for (int i = 1; i <= effective.history_frames; ++i) {
            history.push_back(ctx.checkout_pixels(i));
        }

        // 2. Check out output AFTER all inputs
        auto dst = ctx.checkout_output();

        // 3. Populate dynamic properties from SmartRender checked-out buffers
        pr_data->origin_x = dst.ptr()->origin_x;
        pr_data->origin_y = dst.ptr()->origin_y;
        pr_data->src_origin_x = src.ptr()->origin_x;
        pr_data->src_origin_y = src.ptr()->origin_y;
        pr_data->full_width = ctx.in_data_ptr()->width;
        pr_data->full_height = ctx.in_data_ptr()->height;

        float par = 1.0f;
        auto& par_fraction = ctx.in_data_ptr()->pixel_aspect_ratio;
        if (par_fraction.den > 0) {
            par = static_cast<float>(par_fraction.num)
                / static_cast<float>(par_fraction.den);
        }
        pr_data->par = par;

        for (size_t i = 0; i < history.size(); ++i) {
            pr_data->history_origin_x[i] = history[i].ptr()->origin_x;
            pr_data->history_origin_y[i] = history[i].ptr()->origin_y;
        }

        // 4. GPU/CPU Dispatch with safety checks
        if (effective.allow_gpu) {
            auto gpusuite = gpu_device_suite(ctx.in_data_ptr());
            PF_GPUDeviceInfo info
                = gpusuite.device_info(ctx.extra()->input->device_index);

            if (src.is_gpu() && dst.is_gpu()) { // make sure we're safe to mess with gpu
                // Ensure all checked-out history frames are also GPU-backed
                bool all_history_gpu = true;
                for (const auto& frame : history) {
                    if (!frame.is_gpu()) {
                        all_history_gpu = false;
                        break;
                    }
                }

                if (all_history_gpu && info.device_framework == PF_GPU_Framework_CUDA) {
                    try {
                        cudaStream_t cuda_stream = (cudaStream_t)info.command_queuePV;
                        render_gpu(ctx, effective, src, dst, history, cuda_stream);
                        return;
                    } catch (const std::exception& e) {
                        AETK_LOG_ERR(
                            "GPU rendering failed: {}, falling back to CPU", e.what());
                    }
                }
            }
        }

        render_cpu_rows(ctx, effective, src, dst, history);
    }
};
AETK_EFFECT_MAIN(psychedelia_plugin)
