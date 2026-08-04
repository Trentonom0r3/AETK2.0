#include <aetk/effect.hpp>

using namespace aetk::effect;

class skeleton_plugin : public plugin<skeleton_plugin> {
public:
  static void on_about(const context &ctx) {
    ctx.set_dialog_response("AETK Skeleton: A blank starting point for new effects.");
  }

  static void on_global_setup(const global_setup_context &ctx) {
    ctx.set_pipl_overrides();
    ctx.enable_smart_render();
    ctx.enable_threaded_rendering();
  }

  static void on_params_setup(const params_setup_context &ctx) {
    ctx.add_topic("Settings", [&](const params_setup_context &topic) {
      topic.add_slider("Intensity", 0.0f, 100.0f, 50.0f);
    });
  }

  static void on_pre_render(const pre_render_context &ctx) {
    // Check out input layer pixels.
    ctx.checkout_layer(0, 0);
  }

  static void on_smart_render(const smart_render_context &ctx) {
    auto src = ctx.checkout_pixels(0);
    auto dst = ctx.checkout_output();
    
    // Default pass-through
    src.copy_to(dst);
  }
};

AETK_EFFECT_MAIN(skeleton_plugin)
