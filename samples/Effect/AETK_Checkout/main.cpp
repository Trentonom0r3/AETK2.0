#include "plugin_config.h"
#include <aetk/effect.hpp>

// SDK example headers
#include <DuckSuite.h>

/**
 * @file main.cpp
 * @brief Port of the SDK "Checkout" sample to AETK 2.0.
 */

using namespace aetk::effect;

enum class checkout_keys { frame_offset = 1, layer_to_checkout = 2 };

class checkout_plugin : public plugin<checkout_plugin> {
public:
    static void on_global_setup(const global_setup_context& ctx) {
        ctx.enable_temporal_checkouts();
        ctx.enable_non_param_varying();
        ctx.enable_output_extent();
        ctx.enable_options_button();
        ctx.enable_mfr();
        ctx.enable_smart_render();
        AETK_START_INFO("checkout");
        // High-level RAII wrapper for custom suites with built-in alert message
        try {
            auto duck = ctx.get_suite_with_alert<DuckSuite1>(kDuckSuite1,
                kDuckSuiteVersion1,
                "No Duck Suite! That's OK, it just means the Sweetie SDK sample wasn't loaded. \
                 Build the Sweetie SDK sample in the AEGP folder and put it in the plug-in folder.");
            duck->Quack(2);
        } catch (const std::exception&) {
            // Silently continue if suite is missing
        }
    }

    static void on_params_setup(const params_setup_context& ctx) {
        ctx.add_int_slider("Frame offset", -100, 100, 0)
            .set_key(checkout_keys::frame_offset)
            .set_key("frame_offset_str");
        ctx.add_layer("Layer to checkout", -1)
            .set_key(checkout_keys::layer_to_checkout)
            .set_key("layer_to_checkout_str");
        ctx.set_options_name("AETK Options!");
    }

    static void on_do_dialog(const context& ctx) {
        ctx.set_dialog_response(
            "Hello from AETK Checkout!\rThis is a platform-independent dialog response.");
    }

    static void on_render(const render_context& ctx) {
        auto output = ctx.output();
        // Lookup using custom string key
        int32_t offset_frames = ctx.param<slider_param>("frame_offset_str").value();

        // Checkout at offset using custom enum key
        auto checkout_layer = ctx.param_at_offset<layer_param>(
            checkout_keys::layer_to_checkout, offset_frames);
        auto input_layer = ctx.input_param(); // param[0] is always input

        auto bounds = output.bounds();
        int32_t half_h = bounds.height() / 2;

        aetk::core::rect top_rect(0, 0, bounds.width(), half_h);
        aetk::core::rect bottom_rect(0, half_h, bounds.width(), bounds.height());

        // Copy top half of offset layer to top half of output
        if (checkout_layer.world()) {
            checkout_layer.world().copy_to(output, &top_rect, &top_rect);
        } else {
            output.fill(aetk::core::color<>(1.0, 0.0, 0.0), &top_rect); // Red
        }

        // Copy bottom half of current layer to bottom half of output
        if (input_layer.world()) {
            input_layer.world().copy_to(output, &bottom_rect, &bottom_rect);
        } else {
            output.fill(aetk::core::color<>(0.0, 0.0, 1.0), &bottom_rect); // Blue
        }
    }

    static void on_pre_render(const pre_render_context& ctx) {
        // Fetch using custom enum key
        int32_t offset_frames
            = ctx.param<slider_param>(checkout_keys::frame_offset).value();

        // Checkout the input layer normally (ID 0)
        ctx.checkout_layer(0, 0);

        // Checkout the selected layer at the specified offset (ID 1) using custom string
        // key
        int32_t layer_index = ctx.m_index_lookup("layer_to_checkout_str");
        ctx.checkout_layer_at_offset(layer_index, 1, offset_frames);
    }

    static void on_smart_render(const smart_render_context& ctx) {
        // AE Requirement: Inputs MUST be checked out BEFORE the output
        try {
            auto input_layer = ctx.checkout_pixels(0);
            auto offset_layer = ctx.checkout_pixels(1);
            auto output = ctx.checkout_output();

            auto bounds = output.bounds();
            int32_t half_h = bounds.height() / 2;

            aetk::core::rect top_rect(0, 0, bounds.width(), half_h);
            aetk::core::rect bottom_rect(0, half_h, bounds.width(), bounds.height());

            // Offset layer goes to the top (Red fallback if temporal checkout fails)
            if (offset_layer) {
                offset_layer.copy_to(output, nullptr, &top_rect);
            } else {
                output.fill(aetk::core::color<>(1.0, 0.0, 0.0), &top_rect);
            }

            // Input layer goes to the bottom (Blue fallback if main checkout fails)
            if (input_layer) {
                // Scale the whole input frame into the bottom half
                input_layer.copy_to(output, nullptr, &bottom_rect);
            } else {
                output.fill(aetk::core::color<>(0.0, 0.0, 1.0), &bottom_rect);
            }
        } catch (const std::exception& e) {
            AETK_LOG_INFO(e.what());
        }
    }
};

AETK_EFFECT_MAIN(checkout_plugin)
