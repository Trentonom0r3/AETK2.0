#include "suites_list.h"
#include <aetk/effect.hpp>
#include <aetk/effect/ui.hpp>
#include <fstream>
#include <string>

using namespace aetk::effect;

static void run_aegp_compatibility_test(const smart_render_context &ctx) {
  static bool test_run = false;
  if (test_run)
    return;
  test_run = true;

  std::string log_filename =
      "D:\\dev\\Projects\\Repos\\AETK2.0\\aetk_aegp_prpro_test.log";
  std::ofstream outfile(log_filename);
  if (!outfile.is_open()) {
    AETK_LOG_INFO("AEGP Test: Failed to open log file.");
    return;
  }

  outfile << "AETK AEGP Premiere Pro Active Execution Test\n";
  outfile << "========================================\n\n";

  SPBasicSuite *pica = ctx.in_data_ptr()->pica_basicP;
  if (!pica) {
    outfile << "Error: SPBasicSuite is NULL!\n";
    outfile.close();
    return;
  }

  // 1. Acquire suites
  AEGP_UtilitySuite1 *util_suite = nullptr;
  AEGP_PFInterfaceSuite1 *pf_suite = nullptr;
  AEGP_EffectSuite1 *effect_suite = nullptr;
  AEGP_StreamSuite1 *stream_suite = nullptr;
  PFAppSuite4 *app_suite = nullptr;

  SPErr err = pica->AcquireSuite(
      "AEGP Utility Suite", 3,
      (const void **)&util_suite); // kAEGPUtilitySuiteVersion1 is 3
  if (err != 0 || !util_suite) {
    outfile << "Failed to acquire AEGP Utility Suite v3 (err = " << err
            << ")\n";
  }

  err = pica->AcquireSuite("AEGP PF Interface Suite", 1,
                           (const void **)&pf_suite);
  if (err != 0 || !pf_suite) {
    outfile << "Failed to acquire AEGP PF Interface Suite v1 (err = " << err
            << ")\n";
  }

  err =
      pica->AcquireSuite("AEGP Effect Suite", 1, (const void **)&effect_suite);
  if (err != 0 || !effect_suite) {
    outfile << "Failed to acquire AEGP Effect Suite v1 (err = " << err << ")\n";
  }

  err = pica->AcquireSuite(
      "AEGP Stream Suite", 4,
      (const void **)&stream_suite); // kAEGPStreamSuiteVersion1 is 4
  if (err != 0 || !stream_suite) {
    outfile << "Failed to acquire AEGP Stream Suite v4 (err = " << err << ")\n";
  }

  err =
      pica->AcquireSuite("PF AE App Suite", 6,
                         (const void **)&app_suite); // kPFAppSuiteVersion4 is 6
  if (err != 0 || !app_suite) {
    outfile << "Failed to acquire PF AE App Suite v6 (err = " << err << ")\n";
  }

  outfile << "\nBeginning active calls...\n\n";

  // Test PF App Suite (non-AEGP)
  if (app_suite) {
    outfile << "--- PF AE App Suite Tests ---\n";
    PF_Boolean is_render_engine = FALSE;
    err = app_suite->PF_IsRenderEngine(&is_render_engine);
    outfile << "PF_IsRenderEngine: err = " << err
            << ", is_render_engine = " << (is_render_engine ? "TRUE" : "FALSE")
            << "\n";

    PF_App_Color bg_color = {0, 0, 0};
    err = app_suite->PF_AppGetBgColor(&bg_color);
    outfile << "PF_AppGetBgColor: err = " << err << ", R=" << bg_color.red
            << ", G=" << bg_color.green << ", B=" << bg_color.blue << "\n";
    outfile << "-----------------------------\n\n";
  }

  // Test AEGP Utility Suite (independent functions)
  if (util_suite) {
    outfile << "--- AEGP Utility Suite Tests ---\n";
    A_short major = 0, minor = 0;
    err = util_suite->AEGP_GetDriverPluginInitFuncVersion(&major, &minor);
    outfile << "AEGP_GetDriverPluginInitFuncVersion: err = " << err
            << ", major=" << major << ", minor=" << minor << "\n";

    major = 0;
    minor = 0;
    err = util_suite->AEGP_GetDriverImplementationVersion(&major, &minor);
    outfile << "AEGP_GetDriverImplementationVersion: err = " << err
            << ", major=" << major << ", minor=" << minor << "\n";

    void *main_hwnd = nullptr;
    err = util_suite->AEGP_GetMainHWND(&main_hwnd);
    outfile << "AEGP_GetMainHWND: err = " << err
            << ", main_hwnd = " << main_hwnd << "\n";

    AEGP_PluginID aegp_id = 0;
    err = util_suite->AEGP_RegisterWithAEGP(nullptr, "AETK CrossHost Test",
                                            &aegp_id);
    outfile << "AEGP_RegisterWithAEGP: err = " << err
            << ", aegp_id = " << aegp_id << "\n";
    outfile << "--------------------------------\n\n";
  }

  // Test AEGP PF Interface Suite
  if (pf_suite) {
    outfile << "--- AEGP PF Interface Suite Tests ---\n";
    AEGP_LayerH layer_handle = nullptr;
    err = pf_suite->AEGP_GetEffectLayer(ctx.in_data_ptr()->effect_ref,
                                        &layer_handle);
    outfile << "AEGP_GetEffectLayer: err = " << err
            << ", layer_handle = " << layer_handle << "\n";

    A_Time comp_time = {0, 1};
    err = pf_suite->AEGP_ConvertEffectToCompTime(
        ctx.in_data_ptr()->effect_ref, 0, ctx.in_data_ptr()->time_scale,
        &comp_time);
    outfile << "AEGP_ConvertEffectToCompTime: err = " << err
            << ", time = " << comp_time.value << " / " << comp_time.scale
            << "\n";
    outfile << "-------------------------------------\n\n";
  }

  // Clean up suites
  if (util_suite)
    pica->ReleaseSuite("AEGP Utility Suite", 3);
  if (pf_suite)
    pica->ReleaseSuite("AEGP PF Interface Suite", 1);
  if (effect_suite)
    pica->ReleaseSuite("AEGP Effect Suite", 1);
  if (stream_suite)
    pica->ReleaseSuite("AEGP Stream Suite", 4);
  if (app_suite)
    pica->ReleaseSuite("PF AE App Suite", 6);

  outfile << "\n========================================\n";
  outfile << "Active AEGP suite execution test completed.\n";
  outfile.close();
}

class cross_host_plugin : public plugin<cross_host_plugin> {
public:
  static void on_about(const context &ctx) {
    AETK_LOG_INFO("on_about called");
    ctx.set_dialog_response("AETK CrossHost: Premiere Pro & AE compat test.");
  }

  static void on_global_setup(const global_setup_context &ctx) {
    aetk::core::logger::instance().init("aetk_crosshost_debug.log");
    AETK_LOG_INFO("on_global_setup called");
    ctx.set_pipl_overrides();
    ctx.enable_threaded_rendering();
    ctx.enable_smart_render();

    SPBasicSuite *pica = ctx.in_data_ptr()->pica_basicP;
    if (!pica) {
      AETK_LOG_INFO("SPBasicSuite is null in on_global_setup!");
      return;
    }

    std::string log_filename;
    if (ctx.is_premiere()) {
      log_filename = "D:\\dev\\Projects\\Repos\\AETK2.0\\aetk_suites_prpro.log";
    } else {
      log_filename = "D:\\dev\\Projects\\Repos\\AETK2.0\\aetk_suites_ae.log";
    }

    std::ofstream outfile(log_filename);
    if (!outfile.is_open()) {
      AETK_LOG_INFO("Failed to open suites log file: " + log_filename);
      return;
    }

    outfile
        << "AETK Drawbot Suite Load Test Results (After custom UI enabled)\n";
    outfile << "Host: "
            << (ctx.is_premiere() ? "Premiere Pro" : "After Effects") << "\n";
    outfile << "========================================\n\n";

    const suite_to_test drawbot_suites[] = {
        {"DRAWBOT Draw Suite", 1, "kDRAWBOT_DrawSuite_Version1"},
        {"DRAWBOT Image Suite", 1, "kDRAWBOT_ImageSuite_Version1"},
        {"DRAWBOT Path Suite", 1, "kDRAWBOT_PathSuite_Version1"},
        {"DRAWBOT Pen Suite", 1, "kDRAWBOT_PenSuite_Version1"},
        {"DRAWBOT Supplier Suite", 1, "kDRAWBOT_SupplierSuite_Version1"},
        {"DRAWBOT Surface Suite", 1, "kDRAWBOT_SurfaceSuite_Version1"},
        {"DRAWBOT Surface Suite", 2, "kDRAWBOT_SurfaceSuite_Version2"},
    };

    int loaded_count = 0;
    int failed_count = 0;

    for (const auto &suite_info : drawbot_suites) {
      const void *suite_ptr = nullptr;
      SPErr err =
          pica->AcquireSuite(suite_info.name, suite_info.version, &suite_ptr);
      if (err == kSPNoError && suite_ptr) {
        outfile << "[SUCCESS] " << suite_info.name << " v" << suite_info.version
                << " (" << suite_info.version_const << ") loaded at "
                << suite_ptr << "\n";
        loaded_count++;
        pica->ReleaseSuite(suite_info.name, suite_info.version);
      } else {
        outfile << "[FAILED ] " << suite_info.name << " v" << suite_info.version
                << " (" << suite_info.version_const << ") - Error code: " << err
                << "\n";
        failed_count++;
      }
    }

    outfile << "\n========================================\n";
    outfile << "Total Loaded: " << loaded_count << "\n";
    outfile << "Total Failed: " << failed_count << "\n";
    outfile.close();

    AETK_LOG_INFO("Suites log written to: " + log_filename);
  }

  static void on_params_setup(const params_setup_context &ctx) {
    AETK_LOG_INFO("on_params_setup called");
    ctx.register_custom_ui(PF_CustomEFlag_EFFECT);
    ctx.add_topic("Settings", [&](const params_setup_context &topic) {
      topic.add_slider("Invert Amount", 0.0f, 100.0f, 100.0f);
    });
  }

  static void on_pre_render(const pre_render_context &ctx) {
    AETK_LOG_INFO("on_pre_render called");
    ctx.checkout_layer(0, 0);
  }

  static void on_smart_render(const smart_render_context &ctx) {
    if (ctx.extra()) {
      AETK_LOG_INFO("on_smart_render called: SmartFX mode");
    } else {
      AETK_LOG_INFO("on_smart_render called: Classic mode fallback");
    }

    // Run active AEGP compatibility tests
    run_aegp_compatibility_test(ctx);

    auto src = ctx.checkout_pixels(0);
    auto dst = ctx.checkout_output();

    float amount =
        ctx.param<float_slider_param>("Invert Amount").value() / 100.0f;
    AETK_LOG_INFO("Invert Amount: {}");

    float red_mult = 1.0f;
    float green_mult = 1.0f;
    float blue_mult = 1.0f;

    src.iterate<pixel_range::tkuint8>(
        dst,
        [amount, red_mult, green_mult, blue_mult](int32_t x, int32_t y,
                                                  aetk::core::color<pixel_range::tkuint8> &c) {
          c.red = std::lerp(c.red, 255.0f - c.red, amount) * red_mult;
          c.green = std::lerp(c.green, 255.0f - c.green, amount) * green_mult;
          c.blue = std::lerp(c.blue, 255.0f - c.blue, amount) * blue_mult;
          c.red = std::clamp(c.red, 0.0, 255.0);
          c.green = std::clamp(c.green, 0.0, 255.0);
          c.blue = std::clamp(c.blue, 0.0, 255.0);
        });
  }
};

AETK_EFFECT_MAIN(cross_host_plugin)
