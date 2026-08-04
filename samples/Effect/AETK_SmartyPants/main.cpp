#include <aetk/effect.hpp>
#include <algorithm>
#include <cmath>

using namespace aetk::effect;

// Versioning information
#define MAJOR_VERSION 1
#define MINOR_VERSION 4
#define BUG_VERSION 0
#define STAGE_VERSION PF_Stage_DEVELOP
#define BUILD_VERSION 1

// Parameter IDs
enum {
  SMARTY_INPUT = 0,
  SMARTY_CHANNEL,
  SMARTY_BLEND,
  SMARTY_NUM_PARAMS
};

// Sequence data enum
enum {
  Channel_RGB = 1,
  Channel_RED,
  Channel_GREEN,
  Channel_BLUE,
  PADDING1,
  Channel_HLS,
  Channel_HUE,
  Channel_LIGHTNESS,
  Channel_SATURATION,
  PADDING2,
  Channel_YIQ,
  Channel_LUMINANCE,
  Channel_IN_PHASE_CHROMINANCE,
  Channel_QUADRATURE_CHROMINANCE,
  PADDING3,
  Channel_ALPHA
};

// Sequence Data Structs
struct SmartyPantsData {
  A_long channel;
  PF_Pixel last, last_to;
  A_short blend;
  A_Boolean cache_good;
  A_char padding;
  A_long tab[3][3][256];
};

class smartypants_plugin : public plugin<smartypants_plugin> {
public:
  static void on_about(const context &ctx) {
    ctx.set_dialog_response(
        "AETK SmartyPants: Ported AE SDK SmartyPants Sample Plugin.");
  }

  static void on_global_setup(const global_setup_context &ctx) {
    ctx.set_pipl_overrides();
    ctx.enable_smart_render();
    ctx.enable_threaded_rendering();
  }

  static void on_sequence_setup(const sequence_setup_context &ctx) {
    ctx.set_sequence_data<SmartyPantsData>();
    auto seq = aetk::effect::mutable_sequence_data<SmartyPantsData>(ctx);
    if (seq) {
      seq->channel = -1L;
    }
    ctx.out_data_ptr()->flat_sdata_size = sizeof(SmartyPantsData);
  }

  static void on_sequence_resetup(const sequence_setup_context &ctx) {
    aetk::effect::dispose_sequence_data<SmartyPantsData>(ctx);
    on_sequence_setup(ctx);
  }

  static void on_sequence_setdown(const sequence_setdown_context &ctx) {
    aetk::effect::dispose_sequence_data<SmartyPantsData>(ctx);
  }

  static void on_params_setup(const params_setup_context &ctx) {
    ctx.add_popup(
           "Channel", 16, Channel_ALPHA,
           "RGB|Red|Green|Blue|(-|HLS|Hue|Lightness|Saturation|(-|YIQ|"
           "Luminance|In Phase Chrominance|Quadrature Chrominance|(-|Alpha")
         .set_key(SMARTY_CHANNEL);
    ctx.add_slider("Layer Blend Ratio", 0.0f, 100.0f, 0.0f)
         .set_key(SMARTY_BLEND);
  }

  static void on_query_dynamic_flags(const query_dynamic_flags_context &ctx) {
    if (ctx.int_val(SMARTY_CHANNEL) == Channel_ALPHA) {
      ctx.remove_out_flags2(PF_OutFlag2_DOESNT_NEED_EMPTY_PIXELS);
    }
  }

  static void on_pre_render(const pre_render_context &ctx) {
    // Retrieve BG color using the modernized AETK backend wrapper
    aetk::core::color<> bg_color = ctx.get_comp_background_color();
    ctx.mix_guid(bg_color);

    auto req = ctx.output_request();
    if (ctx.int_val(SMARTY_CHANNEL) == Channel_ALPHA) {
      req.with_channel_mask(req.channel_mask() | channel_mask::alpha);
    }
    req.with_preserve_rgb_of_zero_alpha(true);

    ctx.checkout_layer(SMARTY_INPUT, req, SMARTY_INPUT);
  }

  static void on_smart_render(const smart_render_context &ctx) {
    auto input = ctx.checkout_pixels(SMARTY_INPUT);
    auto output = ctx.checkout_output();

    double blend = ctx.float_val(SMARTY_BLEND) / 100.0;
    A_long channel = ctx.int_val(SMARTY_CHANNEL);

    if (blend >= 0.999) {
      input.copy_to(output);
      return;
    }

    // Convert input and output to float scratch worlds for processing
    smart_world float_input = input.to(PF_PixelFormat_ARGB128);
    smart_world float_output = smart_world(ctx.in_data_ptr(), output.width(), output.height(), 32);

    float_input.iterate<pixel_range::tkuint8>(float_output, [&](int32_t x, int32_t y, aetk::core::color<pixel_range::tkuint8> &c) {
      aetk::core::color<pixel_range::tkuint8> in_pixel = c;
      double h, s, l;
      double y_yiq, i_yiq, q_yiq;

      switch (channel) {
      case Channel_ALPHA:
        c.alpha = blend * in_pixel.alpha + (1.0 - blend) * (255.0 - in_pixel.alpha);
        break;
      case Channel_RGB:
        c.red = 255.0 - in_pixel.red;
        c.green = 255.0 - in_pixel.green;
        c.blue = 255.0 - in_pixel.blue;
        break;
      case Channel_RED:
        c.red = 255.0 - in_pixel.red;
        break;
      case Channel_GREEN:
        c.green = 255.0 - in_pixel.green;
        break;
      case Channel_BLUE:
        c.blue = 255.0 - in_pixel.blue;
        break;

      case Channel_HLS:
      case Channel_HUE:
      case Channel_LIGHTNESS:
      case Channel_SATURATION:
        in_pixel.to_hsl(h, s, l);
        switch (channel) {
        case Channel_HLS:
          h = 0.5 - h;
          if (h < 0.0) h += 1.0;
          s = 1.0 - s;
          l = 1.0 - l;
          break;
        case Channel_HUE:
          h = 0.5 - h;
          if (h < 0.0) h += 1.0;
          break;
        case Channel_LIGHTNESS:
          l = 1.0 - l;
          break;
        case Channel_SATURATION:
          s = 1.0 - s;
          break;
        }
        c = aetk::core::color<pixel_range::tkuint8>::from_hsl(h, s, l, in_pixel.alpha);
        break;

      case Channel_YIQ:
      case Channel_LUMINANCE:
      case Channel_IN_PHASE_CHROMINANCE:
      case Channel_QUADRATURE_CHROMINANCE:
        in_pixel.to_yiq(y_yiq, i_yiq, q_yiq);
        switch (channel) {
        case Channel_YIQ:
          y_yiq = 1.0 - y_yiq;
          i_yiq = -i_yiq;
          q_yiq = -q_yiq;
          break;
        case Channel_LUMINANCE:
          y_yiq = 1.0 - y_yiq;
          break;
        case Channel_IN_PHASE_CHROMINANCE:
          i_yiq = -i_yiq;
          break;
        case Channel_QUADRATURE_CHROMINANCE:
          q_yiq = -q_yiq;
          break;
        }
        c = aetk::core::color<pixel_range::tkuint8>::from_yiq(y_yiq, i_yiq, q_yiq, in_pixel.alpha);
        break;

      default:
        break;
      }

      if (blend > 0.0 && channel != Channel_ALPHA) {
        c.red = c.red + (in_pixel.red - c.red) * blend;
        c.green = c.green + (in_pixel.green - c.green) * blend;
        c.blue = c.blue + (in_pixel.blue - c.blue) * blend;
      }
    });

    // Convert processed float world back to output's native format and copy
    smart_world converted_output = float_output.to(output.pixel_format());
    converted_output.copy_to(output);
  }
};

AETK_EFFECT_MAIN(smartypants_plugin)
