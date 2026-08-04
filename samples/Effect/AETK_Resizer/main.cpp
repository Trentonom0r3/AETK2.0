#include "plugin_config.h"
#include <aetk/effect.hpp>
#include <cmath>

using namespace aetk::effect;

// Versioning information
#define MAJOR_VERSION 2
#define MINOR_VERSION 4
#define BUG_VERSION 0
#define STAGE_VERSION PF_Stage_DEVELOP
#define BUILD_VERSION 1

// Parameter IDs
enum {
  RESIZE_INPUT = 0,
  RESIZE_AMOUNT,
  RESIZE_COLOR,
  RESIZE_DOWNSAMPLE,
  RESIZE_USE_3D,
  RESIZE_NUM_PARAMS
};

class resizer_plugin : public plugin<resizer_plugin> {
public:
  static void on_about(const context &ctx) {
    ctx.set_dialog_response("AETK Resizer: Demonstrate Output Buffer "
                            "Resizing.\rCopyright 1994-2026 Adobe Inc.");
  }

  static void on_global_setup(const global_setup_context &ctx) {
    aetk::core::logger::instance().init("aetk_resizer_debug.log");
    AETK_LOG_INFO("AETK_Resizer Global Setup");
    ctx.set_pipl_overrides();

    // Add out flags
    ctx.add_out_flags(PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_I_EXPAND_BUFFER |
                      PF_OutFlag_I_HAVE_EXTERNAL_DEPENDENCIES);

    ctx.add_out_flags2(
        PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS | PF_OutFlag2_I_USE_3D_CAMERA |
        PF_OutFlag2_I_USE_3D_LIGHTS | PF_OutFlag2_SUPPORTS_THREADED_RENDERING);
  }

  static void on_params_setup(const params_setup_context &ctx) {
    // BUG FIX: Use add_int_slider (PF_Param_SLIDER) to match the original SDK's
    // PF_ADD_SLIDER. The previous add_slider() created PF_Param_FLOAT_SLIDER,
    // whose u.fs_d.value was misread by slider_param (which reads u.sd.value),
    // causing the border to always be 0.
    ctx.add_int_slider("Border Size", 0, 100, 50).set_key(RESIZE_AMOUNT);

    ctx.add_color("Resized Area Color", 128, 255, 255).set_key(RESIZE_COLOR);

    ctx.add_checkbox("Use Downsample Factors", true,
                     "Correct at all resolutions")
        .set_key(RESIZE_DOWNSAMPLE);

    if (!ctx.is_premiere()) {
      ctx.add_checkbox("Use lights and cameras", false, "(new in 5.0!)")
          .set_key(RESIZE_USE_3D);
    }
  }

  static void on_frame_setup(const frame_setup_context &ctx) {
    double border = ctx.int_val(RESIZE_AMOUNT);
    double border_x = border;
    double border_y = border;

    if (ctx.bool_val(RESIZE_DOWNSAMPLE)) {
      border_x =
          border * (static_cast<double>(ctx.in_data_ptr()->downsample_x.num) /
                    ctx.in_data_ptr()->downsample_x.den);
      border_y =
          border * (static_cast<double>(ctx.in_data_ptr()->downsample_y.num) /
                    ctx.in_data_ptr()->downsample_y.den);
    }

    // BUG FIX: Use params[0]->u.ld dimensions (actual buffer size at current
    // downsample level), NOT in_data->width/height (which is always full-res).
    // The original SDK does: params[0]->u.ld.width / params[0]->u.ld.height.
    // Using in_data->width gave 3840x2160 at full-res while the render buffer
    // was only 1280x720, causing dimension mismatches and black output.
    int32_t src_w = ctx.params_ptr()[0]->u.ld.width;
    int32_t src_h = ctx.params_ptr()[0]->u.ld.height;
    AETK_LOG_INFO("on_frame_setup: input=" + std::to_string(src_w) + "x" +
                  std::to_string(src_h) + " border=" +
                  std::to_string(border_x) + "x" + std::to_string(border_y));

    ctx.set_width(2 * static_cast<int32_t>(border_x) + src_w);
    ctx.set_height(2 * static_cast<int32_t>(border_y) + src_h);

    ctx.set_origin_h(static_cast<int16_t>(border_x));
    ctx.set_origin_v(static_cast<int16_t>(border_y));
  }

  static void on_render(const context &ctx) {
    // BUG FIX: Do NOT use smart_render_context during classic render.
    // smart_render_context belongs to SmartFX (PF_Cmd_SMART_RENDER) and its
    // placement_offset() had a zero-origin fallback bug. Use output_origin_x/y
    // directly from in_data, exactly as the original SDK does.
    auto input = ctx.checkout_pixels(RESIZE_INPUT);
    auto output = ctx.checkout_output();

    auto color_param_val = ctx.color_val(RESIZE_COLOR);

    int32_t origin_x = static_cast<int32_t>(ctx.in_data_ptr()->output_origin_x);
    int32_t origin_y = static_cast<int32_t>(ctx.in_data_ptr()->output_origin_y);

    // 1. Fill the output world with the border color
    output.fill(color_param_val);

    // 2. Copy the input frame at the origin offset set during frame_setup
    // The origin values come from out_data->origin set in on_frame_setup,
    // which AE feeds back as in_data->output_origin_x/y during render.
    aetk::core::rect dst_rect(origin_x, origin_y, origin_x + input.width(),
                              origin_y + input.height());

    // Use HQ copy when quality is set to HI, matching the original SDK's
    // copy_hq vs copy branching.
    bool hq = (ctx.in_data_ptr()->quality == PF_Quality_HI);
    input.copy_to(output, nullptr, &dst_rect, hq);
  }

  static void on_query_dynamic_flags(const query_dynamic_flags_context &ctx) {
    if (ctx.is_premiere()) {
      return;
    }

    if (ctx.bool_val(RESIZE_USE_3D)) {
      ctx.add_out_flags2(PF_OutFlag2_I_USE_3D_LIGHTS |
                         PF_OutFlag2_I_USE_3D_CAMERA);
    } else {
      ctx.remove_out_flags2(PF_OutFlag2_I_USE_3D_LIGHTS |
                            PF_OutFlag2_I_USE_3D_CAMERA);
    }
  }

  static void on_get_dependencies(dependency_context &ctx) {
    if (ctx.check_type() == dependency_check_type::all) {
      ctx.set_report("All Dependencies requested.");
    } else if (ctx.check_type() == dependency_check_type::missing) {
      ctx.set_report("Missing Dependencies requested.");
    }
  }
};

AETK_EFFECT_MAIN(resizer_plugin)
