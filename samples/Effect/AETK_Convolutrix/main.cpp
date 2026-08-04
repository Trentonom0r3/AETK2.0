#include "plugin_config.h"
#include <aetk/effect.hpp>
#include <cmath>

using namespace aetk::effect;

/**
 * @file main.cpp
 * @brief Modernized Convolutrix sample using AETK 2.0.
 */

class convolutrix_plugin : public plugin<convolutrix_plugin> {
public:
    static void on_global_setup(const global_setup_context& ctx) {
        ctx.enable_smart_render();
        ctx.enable_threaded_rendering();
    }

    static void on_params_setup(const params_setup_context& ctx) {
        ctx.add_slider("Amount", 0, 100, 50);
        
        ctx.add_topic("Color Blend", [&](const params_setup_context& topic) {
            topic.add_slider("Blend Amount", 0, 100, 0);
            topic.add_color("Blend Color", 255, 255, 255);
        });
    }

    static void on_pre_render(const pre_render_context& ctx) {
        ctx.checkout_layer(0, 0);
    }

    static void on_smart_render(const smart_render_context& ctx) {
        auto input = ctx.checkout_pixels(0);
        auto output = ctx.checkout_output();

        float amount_val = ctx.float_val("Amount");
        
        if (amount_val > 0.0f) {
            float amount_fixed = (amount_val / 100.0f) * 65536.0f;
            float sharpen = 1.0f + std::ceil(amount_fixed) / 16.0f;
            float neighbor = (1.0f - sharpen) / 4.0f;

            aetk::core::kernel_3x3 k = {
                { 0,        neighbor, 0        },
                { neighbor, sharpen,  neighbor },
                { 0,        neighbor, 0        }
            };

            input.convolve_to(output, k, 2295.0f);
        } else {
            input.copy_to(output);
        }

        // Handle optional color blend (Independent of sharpening amount)
        float blend = ctx.float_val("Blend Amount") / 100.0f;
        if (blend > 0.0f) {
            auto blend_color = ctx.color_val("Blend Color");
            
            int32_t height = output.height();
            int32_t width = output.width();
            
            ctx.parallel_for(height, [&](int32_t y, int32_t thread_idx) {
                for (int32_t x = 0; x < width; ++x) {
                    auto c = output.get_pixel(x, y);
                    c.red = c.red * (1.0f - blend) + blend_color.red * blend;
                    c.green = c.green * (1.0f - blend) + blend_color.green * blend;
                    c.blue = c.blue * (1.0f - blend) + blend_color.blue * blend;
                    output.set_pixel(x, y, c);
                }
            });
        }
    }

    static void on_get_dependencies(dependency_context& ctx) {
        if (ctx.check_type() == dependency_check_type::all) {
            ctx.set_report("AETK 2.0 Modernized Convolutrix: All systems functional.");
        } else if (ctx.check_type() == dependency_check_type::missing) {
            // Randomly report a missing dependency 1/9th of the time (matching original sample)
            if (std::rand() % 9 == 0) {
                ctx.set_report("Missing: The spirit of the 90s SDK.");
            }
        }
    }
};

AETK_EFFECT_MAIN(convolutrix_plugin)
