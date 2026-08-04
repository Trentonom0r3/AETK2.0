#include "plugin_config.h"
#include "console_joystick.hpp"
#include "console_joystick_data.hpp"
#include "inference.hpp"
#include "shape_designer.hpp"
#include "yolo_class_customizer.hpp"
#include <aetk/effect.hpp>
#include <aetk/effect/draw/canvas.hpp>
#include <aetk/effect/ui.hpp>
#include <aetk/effect/ui/widgets/slider_data.hpp>
#include <aetk/ui/message.hpp>

using namespace aetk::effect;

// ══════════════════════════════════════════════════════════════════════
//  Main Plugin Implementation
// ══════════════════════════════════════════════════════════════════════

class SWARM : public plugin<SWARM> {
public:
    static inline std::unique_ptr<aetk::effect::compute_cache<std::vector<Detection>>>
        g_detections_cache;
    static inline AEGP_PluginID g_aegp_plugin_id = 0;

    static void on_about(const context& ctx) {
        ctx.set_dialog_response("SWARM v1.0.0\nCyberpunk object tracking and plexus network visualization.");
    }

    static void on_global_setup(const global_setup_context& ctx) {
        // Do not call set_pipl_overrides() so static PiPL dialog flags are preserved
        ctx.enable_smart_render();
        ctx.enable_threaded_rendering();
        ctx.enable_param_supervision();
        ctx.enable_options_button();
        ctx.enable_custom_ui();
        g_aegp_plugin_id = ctx.register_with_aegp("SWARM");

        // Local ORT dependency check
        if (aetk::core::ort_helper::test_dependencies_present(ORT_DLL_NAME)) {
            OrtEngine::Init();
        }

        ctx.enable_gpu_rendering();

        g_detections_cache
            = std::make_unique<aetk::effect::compute_cache<std::vector<Detection>>>(
                "com.aetk.swarm.detections", true, 20000);
    }

    static void on_global_setdown(const context& ctx) {
        g_detections_cache.reset();
        OrtEngine::Shutdown();
    }

    static void on_gpu_device_setup(const gpu_device_setup_context& ctx) {
        if (ctx.has_framework(PF_GPU_Framework_CUDA)) {

            ctx.enable_gpu_rendering();
            // AETK_DEBUG("[SWARM] GPU device setup: CUDA found");
        } else {
            AETK_DEBUG("[SWARM] GPU device setup: CUDA not found");
        }
    }

    static void on_gpu_device_setdown(const gpu_device_setdown_context& ctx) {
        log_unique("[SWARM] GPUDeviceSetdown: Releasing device context.");
    }
    static void on_params_setup(const params_setup_context& ctx) {
        ctx.register_custom_ui(PF_CustomEFlag_EFFECT);
        // Top-Level parameters
        aetk::effect::ui::add_widget<aetk::effect::ui::segment_selector>(ctx,
            "Tracking Mode",
            std::vector<std::string> { "Silhouette", "HSV Keyer",
                OrtEngine::IsOrtAvailable() ? "YOLOX AI (COCO)" : "YOLOX AI (Disabled)" },
            OrtEngine::IsOrtAvailable() ? 2 : 0)
            .set_key("tracking_mode");

        auto tracking_board_options
            = aetk::effect::ui::console_joystick_pad::options()
                  .set_shape(aetk::effect::ui::joystick_shape::square)
                  .set_response_exponent(1.6f)
                  .set_well_label(0, "Silhouette")
                  .set_well_label(1, "HSV Keyer")
                  .set_well_label(2,
                      OrtEngine::IsOrtAvailable() ? "YOLOX AI (COCO)" : "YOLOX AI (Disabled)")
                  .set_default_value(0, aetk::effect::ui::joystick_data { 0.0f, 0.0f })
                  .set_default_value(1, aetk::effect::ui::joystick_data { 0.0f, 0.0f })
                  .set_default_value(2,
                      aetk::effect::ui::joystick_data {
                          0.0f, 0.0f }) // Default limit to center midpoint (250)
                  .set_axis_range(0, 0, 5.0f, 250.0f)
                  .set_axis_range(0, 1, 10.0f, 1000.0f)
                  .set_axis_range(1, 0, 1.0f, 180.0f)
                  .set_axis_range(1, 1, 1.0f, 255.0f)
                  .set_axis_range(2, 0, 1.0f, 500.0f)
                  .set_axis_range(2, 1, 100.0f, 10000.0f)
                  .set_axis_formatter(0, 0,
                      [](float val) {
                          char buf[32];
                          aetk::core::c_snprintf(buf, sizeof(buf), "Th:%.0f", val);
                          return std::string(buf);
                      })
                  .set_axis_formatter(0, 1,
                      [](float val) {
                          char buf[32];
                          aetk::core::c_snprintf(buf, sizeof(buf), "Ar:%.0f", val);
                          return std::string(buf);
                      })
                  .set_axis_formatter(1, 0,
                      [](float val) {
                          char buf[32];
                          aetk::core::c_snprintf(buf, sizeof(buf), "H:%.0f", val);
                          return std::string(buf);
                      })
                  .set_axis_formatter(1, 1,
                      [](float val) {
                          char buf[32];
                          aetk::core::c_snprintf(buf, sizeof(buf), "SV:%.0f", val);
                          return std::string(buf);
                      })
                  .set_axis_formatter(2, 0,
                      [](float val) {
                          char buf[32];
                          aetk::core::c_snprintf(buf, sizeof(buf), "Lim:%.0f", val);
                          return std::string(buf);
                      })
                  .set_axis_formatter(2, 1,
                      [](float val) {
                          char buf[32];
                          aetk::core::c_snprintf(buf, sizeof(buf), "Ar:%.0f", val);
                          return std::string(buf);
                      })
                  .set_value_formatter(0,
                      [](float x, float y) {
                          char buf[64];
                          aetk::core::c_snprintf(buf, sizeof(buf), "Threshold:%.0f Area:%.0f",
                              (x + 1.0f) * 122.5f + 5.0f, (y + 1.0f) * 495.0f + 10.0f);
                          return std::string(buf);
                      })
                  .set_value_formatter(1,
                      [](float x, float y) {
                          char buf[64];
                          aetk::core::c_snprintf(buf, sizeof(buf), "Hue:%.0f Sat/Val:%.0f",
                              (x + 1.0f) * 72.5f + 35.0f, (y + 1.0f) * 12.5f + 175.0f);
                          return std::string(buf);
                      })
                  .set_value_formatter(2, [](float x, float y) {
                      char buf[64];
                      aetk::core::c_snprintf(buf, sizeof(buf), "Max Detections:%.0f Min Area:%.0f",
                          (x + 1.0f) * 249.5f + 1.0f, (y + 1.0f) * 4950.0f + 100.0f);
                      return std::string(buf);
                  });

        ctx.add_button("Pre-compute Tracking")
            .set_key("precompute_tracking")
            .on_change([](const user_changed_param_context& cb_ctx) {
                PF_InData* in_data = cb_ctx.in_data_ptr();
                if (!in_data || in_data->time_step <= 0)
                    return;

                int track_mode = 2; // Default to YOLOv8 AI
                if (auto lock = cb_ctx.arb_val<aetk::effect::ui::slider_data<int>>(
                        "tracking_mode")) {
                    track_mode = lock->value;
                }
                if (!OrtEngine::IsOrtAvailable() && track_mode == 2) {
                    track_mode = 0;
                }

                int detect_style = 2; // Default to Auto
                int spawning_mode = 0; // Default to Centroid
                float silhouette_threshold = 127.0f;
                float h_tol = 100.0f;
                float s_tol = 100.0f;
                float tracking_min_area = 100.0f;
                int max_detections = 50;

                if (auto joy = cb_ctx.arb_val<aetk::effect::ui::console_joystick_data>(
                        "triple_joystick")) {
                    detect_style = joy->detect_style;
                    spawning_mode = joy->spawning_mode;
                    silhouette_threshold = (joy->silhouette.x + 1.0f) * 122.5f + 5.0f;
                    h_tol = (joy->hsv.x + 1.0f) * 72.5f + 35.0f;
                    s_tol = (joy->hsv.y + 1.0f) * 12.5f + 175.0f;
                    max_detections = std::clamp(
                        static_cast<int>((joy->yolo.x + 1.0f) * 249.5f + 1.0f), 1, 500);
                    if (track_mode == 2) {
                        tracking_min_area = (joy->yolo.y + 1.0f) * 4950.0f + 100.0f;
                    } else {
                        tracking_min_area = (joy->silhouette.y + 1.0f) * 495.0f + 10.0f;
                    }
                }
                float spawning_spacing = cb_ctx.float_val("spawning_spacing");
                float max_bounding_area_pct = cb_ctx.float_val("max_bounding_area");
                int ai_model_size = (std::max)(0, cb_ctx.int_val("ai_model_size") - 1);

                aetk::core::color<pixel_range::tkuint8> key_color
                    = cb_ctx.color_val<pixel_range::tkuint8>("target_key_color");
                key_color.alpha = 255.0;

                smart_render_context render_ctx(cb_ctx);
                bool cuda_active = (track_mode == 2)
                    ? is_cuda_active_for_yolo(render_ctx)
                    : is_cuda_active_for_non_ai(render_ctx);

                try {
                    auto pf_interface_suite = cb_ctx.get_suite<AEGP_PFInterfaceSuite1>();
                    auto layer_suite = cb_ctx.get_suite<AEGP_LayerSuite8>(
                        kAEGPLayerSuite, kAEGPLayerSuiteVersion8);
                    auto render_options_suite
                        = cb_ctx.get_suite<AEGP_LayerRenderOptionsSuite2>();
                    auto render_suite = cb_ctx.get_suite<AEGP_RenderSuite5>();
                    auto world_suite = cb_ctx.get_suite<AEGP_WorldSuite3>();
                    auto effect_suite = cb_ctx.get_suite<AEGP_EffectSuite4>();
                    auto app_suite
                        = cb_ctx.get_suite<PFAppSuite6>(kPFAppSuite, kPFAppSuiteVersion6);

                    AEGP_LayerH layer_h = nullptr;
                    aetk::core::check_err(pf_interface_suite->AEGP_GetEffectLayer(
                        in_data->effect_ref, &layer_h));
                    if (!layer_h)
                        return;

                    AEGP_EffectRefH effect_ref_h = nullptr;
                    aetk::core::check_err(pf_interface_suite->AEGP_GetNewEffectForEffect(
                        g_aegp_plugin_id, in_data->effect_ref, &effect_ref_h));
                    if (!effect_ref_h)
                        return;

                    struct effect_guard {
                        AEGP_EffectRefH ref;
                        AEGP_EffectSuite4* suite;
                        ~effect_guard() {
                            if (ref && suite) {
                                suite->AEGP_DisposeEffect(ref);
                            }
                        }
                    } eff_guard { effect_ref_h, effect_suite.ptr() };

                    A_Time layer_ip = { };
                    A_Time layer_dur = { };
                    aetk::core::check_err(layer_suite->AEGP_GetLayerInPoint(
                        layer_h, AEGP_LTimeMode_CompTime, &layer_ip));
                    aetk::core::check_err(layer_suite->AEGP_GetLayerDuration(
                        layer_h, AEGP_LTimeMode_CompTime, &layer_dur));

                    A_long num_frames = cb_ctx.total_frames();
                    if (num_frames <= 0)
                        return;

                    PF_AppProgressDialogP prog = nullptr;
                    bool have_prog = false;
                    static const A_UTF16Char title[]
                        = { (A_UTF16Char)'T', (A_UTF16Char)'r', (A_UTF16Char)'a',
                              (A_UTF16Char)'c', (A_UTF16Char)'k', (A_UTF16Char)'i',
                              (A_UTF16Char)'n', (A_UTF16Char)'g', (A_UTF16Char)' ',
                              (A_UTF16Char)'S', (A_UTF16Char)'W', (A_UTF16Char)'A',
                              (A_UTF16Char)'R', (A_UTF16Char)'M', (A_UTF16Char)'.',
                              (A_UTF16Char)'.', (A_UTF16Char)'.', (A_UTF16Char)0 };
                    static const A_UTF16Char cancel[] = { (A_UTF16Char)'C',
                        (A_UTF16Char)'a', (A_UTF16Char)'n', (A_UTF16Char)'c',
                        (A_UTF16Char)'e', (A_UTF16Char)'l', (A_UTF16Char)0 };

                    PF_Err prog_err = app_suite->PF_CreateNewAppProgressDialog(
                        title, cancel, FALSE, &prog);
                    if (prog_err == PF_Err_NONE && prog) {
                        have_prog = true;
                    }

                    for (A_long f = 0; f < num_frames; ++f) {
                        if (have_prog && prog) {
                            if (app_suite->PF_AppProgressDialogUpdate(prog, f, num_frames)
                                == PF_Interrupt_CANCEL) {
                                break;
                            }
                        }

                        A_Time comp_time;
                        comp_time.scale = in_data->time_scale;
                        comp_time.value
                            = layer_ip.value * in_data->time_scale / layer_ip.scale
                            + f * in_data->time_step;

                        A_Time render_time = { };
                        aetk::core::check_err(layer_suite->AEGP_ConvertCompToLayerTime(
                            layer_h, &comp_time, &render_time));

                        AEGP_LayerRenderOptionsH opts_h = nullptr;
                        aetk::core::check_err(
                            render_options_suite->AEGP_NewFromUpstreamOfEffect(
                                g_aegp_plugin_id, effect_ref_h, &opts_h));

                        struct opts_guard {
                            AEGP_LayerRenderOptionsH opts;
                            AEGP_LayerRenderOptionsSuite2* suite;
                            ~opts_guard() {
                                if (opts && suite) {
                                    suite->AEGP_Dispose(opts);
                                }
                            }
                        } o_guard { opts_h, render_options_suite.ptr() };

                        aetk::core::check_err(render_options_suite->AEGP_SetWorldType(
                            opts_h, AEGP_WorldType_8));
                        aetk::core::check_err(
                            render_options_suite->AEGP_SetDownsampleFactor(opts_h, 1, 1));
                        aetk::core::check_err(
                            render_options_suite->AEGP_SetTime(opts_h, render_time));

                        AEGP_FrameReceiptH receipt_h = nullptr;
                        PF_Err render_err
                            = render_suite->AEGP_RenderAndCheckoutLayerFrame(
                                opts_h, nullptr, nullptr, &receipt_h);

                        if (render_err == PF_Err_NONE && receipt_h) {
                            struct receipt_guard {
                                AEGP_FrameReceiptH receipt;
                                AEGP_RenderSuite5* suite;
                                ~receipt_guard() {
                                    if (receipt && suite) {
                                        suite->AEGP_CheckinFrame(receipt);
                                    }
                                }
                            } r_guard { receipt_h, render_suite.ptr() };

                            AEGP_WorldH world_h = nullptr;
                            aetk::core::check_err(
                                render_suite->AEGP_GetReceiptWorld(receipt_h, &world_h));

                            if (world_h) {
                                PF_EffectWorld pf_world;
                                AEFX_CLR_STRUCT(pf_world);
                                aetk::core::check_err(
                                    world_suite->AEGP_FillOutPFEffectWorld(
                                        world_h, &pf_world));

                                auto src = smart_world(
                                    &pf_world, in_data, smart_world::ownership::NONE);

                                A_long local_frame_time = render_time.value
                                    * in_data->time_scale / render_time.scale;

                                AEGP_GUID guid = compute_detection_guid(
                                    in_data->effect_ref, local_frame_time,
                                    in_data->time_step, in_data->time_scale, track_mode,
                                    detect_style, (int)silhouette_threshold,
                                    key_color.to_float(), (int)h_tol, (int)s_tol,
                                    cuda_active, spawning_mode, spawning_spacing,
                                    tracking_min_area, max_detections,
                                    max_bounding_area_pct, ai_model_size);

                                smart_render_context render_ctx(cb_ctx);

                                g_detections_cache->checkout(guid, [&]() {
                                    auto result
                                        = std::make_unique<std::vector<Detection>>();
                                    if (track_mode == 0) { // Silhouette
                                        float threshold_val = std::clamp(
                                            silhouette_threshold, 5.0f, 250.0f);
                                        *result = run_silhouette_tracking(render_ctx, src,
                                            (int)threshold_val, detect_style, 500,
                                            spawning_mode, spawning_spacing);
                                    } else if (track_mode == 1) { // HSV Keyer
                                        *result = run_hsv_keyer_tracking(render_ctx, src,
                                            key_color, h_tol, s_tol, s_tol, 500,
                                            spawning_mode, spawning_spacing);
                                    } else if (track_mode == 2) { // YOLOv8 AI
                                        constexpr float BASELINE_CONF = 0.05f;
                                        *result = run_yolo_inference(render_ctx, src,
                                            BASELINE_CONF, 500, ai_model_size);
                                    }
                                    return result;
                                });
                            }
                        }
                    }

                    if (have_prog && prog) {
                        app_suite->PF_DisposeAppProgressDialog(prog);
                    }
                } catch (const std::exception& e) {
                    AETK_ERROR(
                        "[SWARM] Pre-compute timeline walk exception: {}", e.what());
                } catch (...) {
                    AETK_ERROR("[SWARM] Pre-compute timeline walk unknown exception.");
                }

                cb_ctx.refresh_ui();
            });

        aetk::effect::ui::add_widget<aetk::effect::ui::console_joystick_pad>(ctx,
            "Tracking Controls", "Tracking Controls Console", "tracking_mode",
            tracking_board_options)
            .set_key("triple_joystick");

        ctx.add_slider("Spawning (Grid/Contour Only)", 4.0f, 64.0f, 16.0f,
               PF_ParamFlag_SUPERVISE | PF_ParamFlag_COLLAPSE_TWIRLY)
            .set_key("spawning_spacing");

        ctx.add_slider("Connection Distance", 10.0f, 1000.0f, 450.0f)
            .set_key("plexus_max_distance");

        ctx.add_slider("Max Connections / Node", 1.0f, 32.0f, 8.0f)
            .set_key("plexus_max_connections");

        ctx.add_slider("Max Bounding Area %", 1.0f, 100.0f, 50.0f)
            .set_key("max_bounding_area");

        ctx.add_slider("Feed Darken %", 0.0f, 100.0f, 75.0f).set_key("feed_darken");

        ctx.add_color("Target Key Color", 0, 255, 0).set_key("target_key_color");

        ctx.add_popup("AI Model Size", 3, 1,
               "YOLOX Small (Fast & Balanced)|YOLOX Medium (High Accuracy)|YOLOX Large (Highest Accuracy)")
            .set_key("ai_model_size");

        aetk::effect::ui::add_widget<aetk::effect::ui::yolo_class_customizer>(
            ctx, "YOLO Class Colors")
            .on_interpolate([](aetk::effect::ui::class_color_map* dst,
                                const aetk::effect::ui::class_color_map* left,
                                const aetk::effect::ui::class_color_map* right,
                                double t) {
                aetk::effect::ui::class_color_map::interpolate_into(dst, left, right, t);
            })
            .set_key("yolo_class_colors");

        // 2. Plexus Connections topic (starts OPEN now)
        ctx.add_topic("Plexus Connections", 0, [&](const params_setup_context& topic) {
            topic.add_popup("Topology", 5, 3, "Chain|Cluster|Mesh|MST|Delaunay")
                .set_key("topology");

            topic.add_popup("Color Mode", 3, 3, "Unified|Custom|Random")
                .set_key("color_mode");

            topic.add_popup("Line Geometry", 2, 1, "Straight|Bezier")
                .set_key("line_geometry");

            topic.add_color("Line Color", 122, 234, 255).set_key("line_color");
            topic.add_color("Triangle Fill Color", 0, 204, 255).set_key("triangle_color");

            topic.add_checkbox("Draw Arrows", false).set_key("draw_arrows");
            topic.add_checkbox("Draw Triangles", true).set_key("draw_triangles");

            topic.add_slider("Plexus Line Thickness", 1.0f, 15.0f, 4.0f)
                .set_key("plexus_thickness");
            topic.add_slider("Bezier Curvature %", 0.0f, 100.0f, 40.0f)
                .set_key("curvature_pct");
            topic.add_slider("Bounding Box Thickness", 1.0f, 15.0f, 3.0f)
                .set_key("bounding_box_thickness");

            auto opt_curve = aetk::effect::ui::curve_editor::options().set_height(100.0f);
            aetk::effect::ui::add_widget<aetk::effect::ui::curve_editor>(
                topic, "Connection Falloff Spline", opt_curve)
                .on_interpolate([](aetk::effect::ui::curve_data* dst,
                                    const aetk::effect::ui::curve_data* left,
                                    const aetk::effect::ui::curve_data* right, double t) {
                    aetk::effect::arb_traits<aetk::effect::ui::curve_data>::interpolate(
                        dst, left, right, t);
                })
                .set_key("connection_spline");

            // 3. SUB TOPIC
            topic.add_topic(
                "Advanced HUD Styling", 0, [&](const params_setup_context& topic) {
                    topic.add_popup("Marker Style", 4, 3, "Dot|Plus|Cross|None")
                        .set_key("marker_style");

                    topic.add_popup("Box Style", 4, 3, "None|Hollow|Brackets|Solid")
                        .set_key("box_style");

                    topic.add_color("Marker Color", 0, 255, 102).set_key("marker_color");
                    topic.add_color("Box Color", 255, 51, 51).set_key("box_color");
                    topic.add_color("Corner Color", 255, 204, 0).set_key("corner_color");

                    topic.add_slider("Box Fill Opacity %", 0.0f, 100.0f, 35.0f)
                        .set_key("box_fill_opacity");

                    aetk::effect::ui::add_widget<aetk::effect::ui::shape_designer>(
                        topic, "HUD Shape Designer")
                        .on_interpolate(
                            [](aetk::effect::ui::hud_shape_data* dst,
                                const aetk::effect::ui::hud_shape_data* left,
                                const aetk::effect::ui::hud_shape_data* right, double t) {
                                aetk::effect::arb_traits<
                                    aetk::effect::ui::hud_shape_data>::interpolate(dst,
                                    left, right, t);
                            })
                        .set_key("hud_shape_designer");

                    topic
                        .add_popup("Label Style", 5, 2,
                            "None|Text Out|Text In|Badge Out|Badge In")
                        .set_key("label_style");

                    topic.add_checkbox("Draw Terminal Diagnostics", true)
                        .set_key("draw_diagnostics");

                    topic.add_color("HUD Text Color", 255, 255, 255)
                        .set_key("hud_text_color");

                    topic.add_slider("HUD Text Size", 0.5f, 3.0f, 1.5f)
                        .set_key("hud_text_size");
                });
        });
    }

    static void on_ui_update(ui_update_context& ctx) {

        int track_mode = 2; // Default to YOLO
        if (auto lock
            = ctx.arb_val<aetk::effect::ui::slider_data<int>>("tracking_mode")) {
            if (!OrtEngine::IsOrtAvailable() && lock->value == 2) {
                lock->value = 0; // Fallback/revert to Silhouette
                auto p = ctx.param<
                    aetk::effect::arbitrary_param<aetk::effect::ui::slider_data<int>>>(
                    "tracking_mode");
                p.commit();
            }
            track_mode = lock->value;
        }

        // Check out all mode-dependent parameter wrappers using type-safe context
        // param by key
        auto spawning_spacing
            = ctx.param<aetk::effect::float_slider_param>("spawning_spacing");
        auto key_color = ctx.param<aetk::effect::color_param>("target_key_color");
        auto yolo_colors
            = ctx.param<aetk::effect::arbitrary_param<aetk::effect::ui::class_color_map>>(
                "yolo_class_colors");

        // Hide or show using unified host-aware API directly on parameters
        if (track_mode == 2) {
            spawning_spacing.hide();
        } else {
            spawning_spacing.show();
        }

        if (track_mode != 1)
            key_color.hide();
        else
            key_color.show();
        auto ai_model_size = ctx.param<aetk::effect::popup_param>("ai_model_size");
        if (track_mode != 2) {
            yolo_colors.hide();
            ai_model_size.hide();
        } else {
            yolo_colors.show();
            ai_model_size.show();
        }

        // Commit changes back to the host UI
        spawning_spacing.commit();
        key_color.commit();
        yolo_colors.commit();
        ai_model_size.commit();

        int topology = 2; // Default to Mesh
        if (auto lock = ctx.arb_val<aetk::effect::ui::slider_data<int>>("topology")) {
            topology = lock->value;
        }

        auto max_dist
            = ctx.param<aetk::effect::float_slider_param>("plexus_max_distance");
        if (topology != 2 && topology != 3 && topology != 4)
            max_dist.hide();
        else
            max_dist.show();
        max_dist.commit();

        auto max_conn
            = ctx.param<aetk::effect::float_slider_param>("plexus_max_connections");
        if (topology != 2)
            max_conn.hide();
        else
            max_conn.show();
        max_conn.commit();

        // Set host refresh flag to trigger immediate redraw in Premiere Pro ECW
        ctx.force_rerender();
        ctx.set_refresh_ui();
    }

    struct SwarmRenderData {
        int track_mode = 2;
        int detect_style = 1;
        bool cuda_active = false;
        float silhouette_threshold = 127.0f;
        float tracking_min_area = 100.0f;
        int max_detections = 50;
        float h_tol = 100.0f;
        float s_tol = 100.0f;
        float max_bounding_area_pct = 100.0f;
        aetk::core::color<pixel_range::tkuint8> key_color { 255, 0, 0, 255 };
        float target_h = 0.0f, target_s = 0.0f, target_v = 0.0f;
        int topology = 2;
        int line_geometry = 0;
        float plexus_max_distance = 120.0f;
        float plexus_max_connections = 8.0f;
        float curvature_pct = 0.0f;
        float plexus_thickness = 2.0f;
        aetk::effect::ui::curve_data connection_spline;
        aetk::core::color<pixel_range::tkuint8> line_color { 255, 0, 220, 255 };
        bool draw_triangles = false;
        aetk::core::color<pixel_range::tkuint8> triangle_fill_color { 255, 255, 255,
            255 };
        int marker_style = 2;
        aetk::core::color<pixel_range::tkuint8> marker_color { 255, 255, 255, 255 };
        int box_style = 2;
        aetk::core::color<pixel_range::tkuint8> box_color { 255, 255, 255, 255 };
        aetk::core::color<pixel_range::tkuint8> corner_color { 255, 255, 255, 255 };
        float box_fill_opacity = 0.0f;
        int label_style = 1;
        aetk::core::color<pixel_range::tkuint8> text_color { 255, 255, 255, 255 };
        bool draw_arrows = false;
        bool draw_diagnostics = false;
        float bounding_box_thickness = 2.0f;
        int color_mode = 0;
        float hud_text_size = 12.0f;
        std::vector<aetk::core::vec2> shape_points;
        int output_mode = 0;
        float feed_darken = 0.0f;
        int spawning_mode = 0;
        float spawning_spacing = 50.0f;
        int ai_model_size = 0;
    };

    static SwarmRenderData extract_render_data(const context& ctx) {
        SwarmRenderData d;
        d.ai_model_size = (std::max)(0, ctx.int_val("ai_model_size") - 1);
        if (auto lock
            = ctx.arb_val<aetk::effect::ui::slider_data<int>>("tracking_mode")) {
            d.track_mode = lock->value;
        }
        if (!OrtEngine::IsOrtAvailable() && d.track_mode == 2) {
            d.track_mode = 0;
        }

        if (auto joy
            = ctx.arb_val<aetk::effect::ui::console_joystick_data>("triple_joystick")) {
            d.silhouette_threshold = (joy->silhouette.x + 1.0f) * 122.5f + 5.0f;
            d.tracking_min_area = (joy->silhouette.y + 1.0f) * 495.0f + 10.0f;
            d.h_tol = (joy->hsv.x + 1.0f) * 72.5f + 35.0f;
            d.s_tol = (joy->hsv.y + 1.0f) * 12.5f + 175.0f;
            d.max_detections = std::clamp(
                static_cast<int>((joy->yolo.x + 1.0f) * 249.5f + 1.0f), 1, 500);
            if (d.track_mode == 2) {
                d.tracking_min_area = (joy->yolo.y + 1.0f) * 4950.0f + 100.0f;
            }

            d.output_mode = joy->output_mode;
            d.detect_style = joy->detect_style;
            d.spawning_mode = joy->spawning_mode;
        }

        d.max_bounding_area_pct = ctx.float_val("max_bounding_area");
        d.key_color = ctx.color_val<pixel_range::tkuint8>("target_key_color");
        d.key_color.alpha = 255.0;
        d.key_color.to_hsv(d.target_h, d.target_s, d.target_v);

        d.topology = (std::max)(0, ctx.int_val("topology") - 1);
        d.line_geometry = (std::max)(0, ctx.int_val("line_geometry") - 1);

        d.plexus_max_distance = ctx.float_val("plexus_max_distance");
        d.plexus_max_connections = ctx.float_val("plexus_max_connections");
        d.curvature_pct = ctx.float_val("curvature_pct");
        d.plexus_thickness = ctx.float_val("plexus_thickness");

        if (auto lock = ctx.arb_val<aetk::effect::ui::curve_data>("connection_spline")) {
            d.connection_spline = *lock;
        }

        auto convert_color = [](const aetk::core::color<pixel_range::tkuint8>& c) {
            return aetk::core::color<pixel_range::tkuint8> { 255.0, c.red, c.green,
                c.blue };
        };

        d.line_color = convert_color(ctx.color_val<pixel_range::tkuint8>("line_color"));
        d.draw_triangles = ctx.bool_val("draw_triangles");
        d.triangle_fill_color
            = convert_color(ctx.color_val<pixel_range::tkuint8>("triangle_color"));

        d.marker_style = (std::max)(0, ctx.int_val("marker_style") - 1);
        d.marker_color
            = convert_color(ctx.color_val<pixel_range::tkuint8>("marker_color"));

        d.box_style = (std::max)(0, ctx.int_val("box_style") - 1);
        d.box_color = convert_color(ctx.color_val<pixel_range::tkuint8>("box_color"));
        d.corner_color
            = convert_color(ctx.color_val<pixel_range::tkuint8>("corner_color"));
        d.box_fill_opacity = ctx.float_val("box_fill_opacity");

        d.label_style = (std::max)(0, ctx.int_val("label_style") - 1);
        d.text_color
            = convert_color(ctx.color_val<pixel_range::tkuint8>("hud_text_color"));

        d.draw_arrows = ctx.bool_val("draw_arrows");
        d.draw_diagnostics = ctx.bool_val("draw_diagnostics");
        d.bounding_box_thickness = ctx.float_val("bounding_box_thickness");

        d.color_mode = (std::max)(0, ctx.int_val("color_mode") - 1);
        d.hud_text_size = ctx.float_val("hud_text_size");

        if (auto shape_lock
            = ctx.arb_val<aetk::effect::ui::hud_shape_data>("hud_shape_designer")) {
            if (!shape_lock->points.empty()) {
                d.shape_points.reserve(shape_lock->points.size());
                for (const auto& p : shape_lock->points) {
                    d.shape_points.push_back({ p.x, p.y });
                }
            }
        }
        if (d.shape_points.empty()) {
            d.shape_points.push_back({ -0.7f, -0.7f });
            d.shape_points.push_back({ 0.7f, -0.7f });
            d.shape_points.push_back({ 0.7f, 0.7f });
            d.shape_points.push_back({ -0.7f, 0.7f });
        }

        d.feed_darken = ctx.float_val("feed_darken");
        d.spawning_spacing = ctx.float_val("spawning_spacing");

        if (d.output_mode == 2) {
            d.line_color = { 255.0, 255.0, 255.0, 255.0 };
            d.triangle_fill_color = { 255.0, 255.0, 255.0, 255.0 };
            d.marker_color = { 255.0, 255.0, 255.0, 255.0 };
            d.box_color = { 255.0, 255.0, 255.0, 255.0 };
            d.corner_color = { 255.0, 255.0, 255.0, 255.0 };
            d.text_color = { 255.0, 255.0, 255.0, 255.0 };
        }

        return d;
    }

    static void on_pre_render(const pre_render_context& ctx) {
        SwarmRenderData* data = new SwarmRenderData(extract_render_data(ctx));
        ctx.set_pre_render_data(data);

        int track_mode = data->track_mode;

        if (track_mode == 2) {
            // YOLOv8 AI Mode: Always use CPU worlds for maximum DirectML pipeline
            // throughput
            ctx.set_what_gpu(PF_GPU_Framework_NONE);
            AETK_DEBUG(
                "[SWARM] pre render: YOLOv8 AI mode using DirectML, setting CPU worlds");
        } else {
            // Silhouette / HSV Keyer Modes: Enable GPU rendering if CUDA framework is
            // available
            if (ctx.has_framework(PF_GPU_Framework_CUDA)) {
                ctx.enable_gpu_render();
            }
        }
        ctx.checkout_layer(0, 0);
    }

    static void on_smart_render(const smart_render_context& ctx, bool is_gpu) {
        auto src = ctx.checkout_pixels(0);
        auto dst = ctx.checkout_output();
        AETK_DEBUG("Running on GPU? {}", is_gpu);
        int width = src.width();
        int height = src.height();
        std::vector<uint32_t> text_chars;

        std::vector<aetk::effect::draw::batched_line<pixel_range::tkuint8>> batched_lines;
        std::vector<aetk::effect::draw::batched_triangle<pixel_range::tkuint8>>
            batched_tris;

        auto draw_line
            = [&](float x0, float y0, float x1, float y1,
                  const aetk::core::color<pixel_range::tkuint8>& col, int thickness) {
                  batched_lines.push_back({ x0, y0, x1, y1, col, thickness });
              };

        auto draw_bezier
            = [&](int x0, int y0, int cp_x, int cp_y, int x1, int y1,
                  const aetk::core::color<pixel_range::tkuint8>& col, int thickness) {
                  const int steps = 24;
                  const int shift = 16;
                  const int scale = 1 << shift;

                  int px = x0 * scale;
                  int py = y0 * scale;

                  float t_step = 1.0f / steps;
                  float t_step2 = t_step * t_step;

                  float dx_initial
                      = 2.0f * (cp_x - x0) * t_step + (x0 - 2.0f * cp_x + x1) * t_step2;
                  float ddx = 2.0f * (x0 - 2.0f * cp_x + x1) * t_step2;

                  float dy_initial
                      = 2.0f * (cp_y - y0) * t_step + (y0 - 2.0f * cp_y + y1) * t_step2;
                  float ddy = 2.0f * (y0 - 2.0f * cp_y + y1) * t_step2;

                  int fdx = (int)(dx_initial * scale);
                  int fddx = (int)(ddx * scale);

                  int fdy = (int)(dy_initial * scale);
                  int fddy = (int)(ddy * scale);

                  int prev_x = x0;
                  int prev_y = y0;

                  for (int i = 1; i <= steps; i++) {
                      px += fdx;
                      fdx += fddx;

                      py += fdy;
                      fdy += fddy;

                      int curr_x = px >> shift;
                      int curr_y = py >> shift;

                      draw_line((float)prev_x, (float)prev_y, (float)curr_x,
                          (float)curr_y, col, thickness);

                      prev_x = curr_x;
                      prev_y = curr_y;
                  }
              };

        auto draw_triangle
            = [&](float x0, float y0, float x1, float y1, float x2, float y2,
                  const aetk::core::color<pixel_range::tkuint8>& col) {
                  batched_tris.push_back({ x0, y0, x1, y1, x2, y2, col });
              };

        auto draw_marker = [&](int cx, int cy, int size,
                               const aetk::core::color<pixel_range::tkuint8>& col,
                               int thickness, int style) {
            if (style == 0) {
                aetk::effect::draw::dot_marker<pixel_range::tkuint8>(
                    dst, cx, cy, size, col);
            } else {
                float par = 1.0f;
                if (dst.in_data_ptr()) {
                    auto& ar = dst.in_data_ptr()->pixel_aspect_ratio;
                    if (ar.den > 0)
                        par = (float)ar.num / ar.den;
                }
                float ds = 1.0f;
                if (dst.in_data_ptr()) {
                    auto& d = dst.in_data_ptr()->downsample_x;
                    if (d.den > 0)
                        ds = (float)d.num / d.den;
                }
                int scaled_size = (std::max)(1, (int)std::round(size * ds));
                int dx = (int)std::round(scaled_size / par);

                if (style == 1) { // Plus
                    draw_line((float)(cx - dx), (float)cy, (float)(cx + dx), (float)cy,
                        col, thickness);
                    draw_line((float)cx, (float)(cy - scaled_size), (float)cx,
                        (float)(cy + scaled_size), col, thickness);
                } else if (style == 2) { // Cross
                    draw_line((float)(cx - dx), (float)(cy - scaled_size),
                        (float)(cx + dx), (float)(cy + scaled_size), col, thickness);
                    draw_line((float)(cx - dx), (float)(cy + scaled_size),
                        (float)(cx + dx), (float)(cy - scaled_size), col, thickness);
                }
            }
        };

        auto draw_stroke_string = [&](float x, float y, const std::string& text,
                                      const aetk::core::color<pixel_range::tkuint8>& col,
                                      float scale_f, float text_size, int thickness) {
            aetk::effect::draw::stroke_string_segments<pixel_range::tkuint8>(
                dst, x, y, text, col, scale_f, text_size, thickness, batched_lines);
        };

        // Calculate downsample scale factor for scale-invariant visual sizes
        float scale_factor = ctx.scale_factor_x();

        float par = 1.0f;
        if (ctx.in_data_ptr()) {
            auto& ar = ctx.in_data_ptr()->pixel_aspect_ratio;
            if (ar.den > 0) {
                par = static_cast<float>(ar.num) / static_cast<float>(ar.den);
            }
        }

        // 1. Fetch pre-extracted render parameters from PreRender (zero host suite
        // checkout overhead!)
        SwarmRenderData fallback_data;
        const SwarmRenderData* pr_data = ctx.pre_render_data<SwarmRenderData>();
        if (!pr_data) {
            fallback_data = extract_render_data(ctx);
            pr_data = &fallback_data;
        }

        int track_mode = pr_data->track_mode;
        int detect_style = pr_data->detect_style;
        int ai_model_size = pr_data->ai_model_size;
        bool cuda_active = pr_data->cuda_active;

        float silhouette_threshold = pr_data->silhouette_threshold;
        float tracking_min_area = pr_data->tracking_min_area;
        int max_detections = pr_data->max_detections;
        float h_tol = pr_data->h_tol;
        float s_tol = pr_data->s_tol;

        float max_bounding_area_pct = pr_data->max_bounding_area_pct;
        aetk::core::color<pixel_range::tkuint8> key_color = pr_data->key_color;
        float target_h = pr_data->target_h;
        float target_s = pr_data->target_s;
        float target_v = pr_data->target_v;

        int topology = pr_data->topology;
        int line_geometry = pr_data->line_geometry;

        float plexus_max_distance = pr_data->plexus_max_distance;
        float plexus_max_connections = pr_data->plexus_max_connections;
        float curvature_pct = pr_data->curvature_pct;
        float plexus_thickness = pr_data->plexus_thickness;

        aetk::core::color<pixel_range::tkuint8> line_color = pr_data->line_color;
        bool draw_triangles = pr_data->draw_triangles;
        aetk::core::color<pixel_range::tkuint8> triangle_fill_color
            = pr_data->triangle_fill_color;

        int marker_style = pr_data->marker_style;
        aetk::core::color<pixel_range::tkuint8> marker_color = pr_data->marker_color;

        int box_style = pr_data->box_style;
        aetk::core::color<pixel_range::tkuint8> box_color = pr_data->box_color;
        aetk::core::color<pixel_range::tkuint8> corner_color = pr_data->corner_color;
        float box_fill_opacity = pr_data->box_fill_opacity;

        int label_style = pr_data->label_style;
        aetk::core::color<pixel_range::tkuint8> text_color = pr_data->text_color;

        bool draw_arrows = pr_data->draw_arrows;
        bool draw_diagnostics = pr_data->draw_diagnostics;
        float bounding_box_thickness = pr_data->bounding_box_thickness;

        int color_mode = pr_data->color_mode;
        float hud_text_size = pr_data->hud_text_size;

        const auto& shape_points = pr_data->shape_points;
        int output_mode = pr_data->output_mode;

        float darken_pct = pr_data->feed_darken;

        try {
            if (output_mode == 0) { // Composite on Source
                src.copy_to(dst);
                if (darken_pct > 0.001f) {
                    float factor = std::clamp(1.0f - (darken_pct / 100.0f), 0.0f, 1.0f);
                    if (dst.is_gpu()) {
#ifdef AETK_ENABLE_CUDA
                        aetk::effect::draw::cuda_multiply_pixels(dst.gpu_data(),
                            dst.width(), dst.height(), dst.rowbytes(), factor);
#else
                        throw std::runtime_error(
                            "GPU rendering is not supported in this build");
#endif
                    } else {
                        dst.iterate<pixel_range::tkuint8>(dst,
                            [&](A_long /*x*/, A_long /*y*/,
                                aetk::core::color<pixel_range::tkuint8>& c) {
                                c.red *= factor;
                                c.green *= factor;
                                c.blue *= factor;
                            });
                    }
                }

            } else if (output_mode == 1) { // HUD on Transparent
                dst.fill(aetk::core::color<pixel_range::tkuint8> { 0.0, 0.0, 0.0, 0.0 });
            } else if (output_mode == 2) { // Luma Matte
                dst.fill(
                    aetk::core::color<pixel_range::tkuint8> { 255.0, 0.0, 0.0, 0.0 });
            }
        } catch (const aetk::core::exception& e) {
            AETK_DEBUG("SWARM (GPU) Error: {} ", e.what());
            src.copy_to(dst);
        }

        int spawning_mode = pr_data->spawning_mode;
        float spawning_spacing = ctx.float_val("spawning_spacing");

        constexpr bool enable_cache = true;

        using ReceiptLock = aetk::effect::receipt_lock<std::vector<Detection>>;

        std::unique_ptr<std::vector<Detection>> uncached_result;
        const std::vector<Detection>* cached_detections = nullptr;
        std::optional<ReceiptLock> detections_lock;

        auto t_det_0 = std::chrono::high_resolution_clock::now();

        if (enable_cache) {
            AEGP_GUID guid = compute_detection_guid(ctx.in_data_ptr()->effect_ref,
                ctx.in_data_ptr()->current_time, ctx.in_data_ptr()->time_step,
                ctx.in_data_ptr()->time_scale, track_mode, detect_style,
                (int)silhouette_threshold, key_color.to_float(), (int)h_tol, (int)s_tol,
                cuda_active, spawning_mode, spawning_spacing, tracking_min_area,
                max_detections, max_bounding_area_pct, ai_model_size);

            detections_lock.emplace(g_detections_cache->checkout(guid, [&]() {
                auto result = std::make_unique<std::vector<Detection>>();

                if (track_mode == 0) { // Contour Silhouette
                    float threshold_val = std::clamp(silhouette_threshold, 5.0f, 250.0f);
                    *result = run_silhouette_tracking(ctx, src, (int)threshold_val,
                        detect_style, 500, spawning_mode, spawning_spacing);
                } else if (track_mode == 1) { // HSV Keyer
                    *result = run_hsv_keyer_tracking(ctx, src, key_color, (float)h_tol,
                        (float)s_tol, (float)s_tol, 500, spawning_mode, spawning_spacing);
                } else if (track_mode == 2) { // YOLOv8 AI
                    constexpr float BASELINE_CONF = 0.05f;
                    *result
                        = run_yolo_inference(ctx, src, BASELINE_CONF, 500, ai_model_size);
                }

                return result;
            }));

            if (*detections_lock) {
                cached_detections = detections_lock->get();
            }
        } else {
            uncached_result = std::make_unique<std::vector<Detection>>();

            if (track_mode == 0) { // Contour Silhouette
                float threshold_val = std::clamp(silhouette_threshold, 5.0f, 250.0f);
                *uncached_result = run_silhouette_tracking(ctx, src, (int)threshold_val,
                    detect_style, 500, spawning_mode, spawning_spacing);
            } else if (track_mode == 1) { // HSV Keyer
                *uncached_result
                    = run_hsv_keyer_tracking(ctx, src, key_color, (float)h_tol,
                        (float)s_tol, (float)s_tol, 500, spawning_mode, spawning_spacing);
            } else if (track_mode == 2) { // YOLOv8 AI
                constexpr float BASELINE_CONF = 0.05f;
                *uncached_result
                    = run_yolo_inference(ctx, src, BASELINE_CONF, 500, ai_model_size);
            }

            cached_detections = uncached_result.get();
        }

        if (!cached_detections) {
            AETK_WARN("[SWARM] Detections checkout failed or returned null.");
            return;
        }

        auto t_spawn_0 = std::chrono::high_resolution_clock::now();

        // Filter detections safely into local vector
        float frame_area = (float)(width * height);
        float max_area_threshold = (max_bounding_area_pct / 100.0f) * frame_area;
        float scaled_min_area = tracking_min_area * scale_factor * scale_factor;

        std::vector<Detection> active_detections;
        active_detections.reserve(cached_detections->size());

        for (const auto& cached_det : *cached_detections) {
            Detection det = cached_det;
            // Scale from normalized space to local pixel space
            det.xmin *= width;
            det.xmax *= width;
            det.ymin *= height;
            det.ymax *= height;
            det.cx *= width;
            det.cy *= height;

            float area = (det.xmax - det.xmin) * (det.ymax - det.ymin);

            // Universal area checks
            if (area > max_area_threshold)
                continue;
            if (area < scaled_min_area)
                continue;

            active_detections.push_back(det);
        }

        // Sort and limit detections in YOLO mode to reduce clutter
        if (track_mode == 2) {
            std::sort(active_detections.begin(), active_detections.end(),
                [](const Detection& a, const Detection& b) {
                    return a.confidence > b.confidence;
                });
            if (active_detections.size() > static_cast<size_t>(max_detections)) {
                active_detections.resize(max_detections);
            }

            // Apply Spawning Mode (Grid Fill or Perimeter Outline) to YOLO object
            // bounding boxes
            if (spawning_mode == 1 || spawning_mode == 2) {
                std::vector<Detection> spawned_detections;
                spawned_detections.reserve(
                    std::min((size_t)max_detections, active_detections.size() * 16));
                int max_per_object = std::max(4,
                    (int)max_detections
                        / (int)std::max((size_t)1, active_detections.size()));

                for (const auto& base_det : active_detections) {
                    float w_box = base_det.xmax - base_det.xmin;
                    float h_box = base_det.ymax - base_det.ymin;
                    if (w_box < 8.0f || h_box < 8.0f) {
                        spawned_detections.push_back(base_det);
                        continue;
                    }

                    int steps_x = std::clamp(
                        (int)(w_box / (spawning_spacing * scale_factor)), 2, 16);
                    int steps_y = std::clamp(
                        (int)(h_box / (spawning_spacing * scale_factor)), 2, 16);

                    float step_x = w_box / (float)steps_x;
                    float step_y = h_box / (float)steps_y;

                    int obj_count = 0;
                    if (spawning_mode == 1) { // Grid Fill inside YOLO Bounding Box
                        for (int iy = 0; iy <= steps_y && obj_count < max_per_object;
                            ++iy) {
                            for (int ix = 0; ix <= steps_x && obj_count < max_per_object;
                                ++ix) {
                                Detection det = base_det;
                                det.cx = base_det.xmin + ix * step_x;
                                det.cy = base_det.ymin + iy * step_y;
                                spawned_detections.push_back(det);
                                obj_count++;
                            }
                        }
                    } else if (spawning_mode
                        == 2) { // Perimeter Outline of YOLO Bounding Box
                        for (int ix = 0; ix <= steps_x && obj_count < max_per_object;
                            ++ix) {
                            Detection det1 = base_det;
                            det1.cx = base_det.xmin + ix * step_x;
                            det1.cy = base_det.ymin;
                            spawned_detections.push_back(det1);
                            obj_count++;
                            if (obj_count < max_per_object) {
                                Detection det2 = base_det;
                                det2.cx = base_det.xmin + ix * step_x;
                                det2.cy = base_det.ymax;
                                spawned_detections.push_back(det2);
                                obj_count++;
                            }
                        }
                        for (int iy = 1; iy < steps_y && obj_count < max_per_object;
                            ++iy) {
                            Detection det1 = base_det;
                            det1.cx = base_det.xmin;
                            det1.cy = base_det.ymin + iy * step_y;
                            spawned_detections.push_back(det1);
                            obj_count++;
                            if (obj_count < max_per_object) {
                                Detection det2 = base_det;
                                det2.cx = base_det.xmax;
                                det2.cy = base_det.ymin + iy * step_y;
                                spawned_detections.push_back(det2);
                                obj_count++;
                            }
                        }
                    }
                    if (spawned_detections.size() >= static_cast<size_t>(max_detections))
                        break;
                }

                if (!spawned_detections.empty()) {
                    active_detections = std::move(spawned_detections);
                }
            }
        }

        // Populate global set of detected classes for the UI class customizer
        // when in YOLO mode
        if (track_mode == 2) {
            thread_local std::unordered_set<std::string> tls_registered_classes;
            for (const auto& det : active_detections) {
                if (tls_registered_classes.find(det.class_name)
                    == tls_registered_classes.end()) {
                    std::lock_guard<std::mutex> lock(
                        aetk::effect::ui::g_detected_classes_mutex);
                    aetk::effect::ui::g_detected_classes.insert(det.class_name);
                    tls_registered_classes.insert(det.class_name);
                }
            }
        }

        // 4. Graph edge generation based on Plexus reach threshold and selected
        // topology
        float max_dist = plexus_max_distance * scale_factor;
        max_dist = std::max(1.0f, max_dist);
        float max_dist_sq = max_dist * max_dist;

        std::vector<Edge> edges;
        int n = (int)active_detections.size();

        if (topology == 0) { // Sequential Chain
            for (int i = 0; i < n - 1; ++i) {
                float dx = (active_detections[i].cx - active_detections[i + 1].cx) * par;
                float dy = active_detections[i].cy - active_detections[i + 1].cy;
                float dist_sq = dx * dx + dy * dy;
                if (dist_sq <= max_dist_sq) {
                    float dist = std::sqrt(dist_sq);
                    edges.push_back({ i, i + 1, dist });
                }
            }
        } else if (topology == 1) { // Star Cluster
            for (int i = 1; i < n; ++i) {
                float dx = (active_detections[0].cx - active_detections[i].cx) * par;
                float dy = active_detections[0].cy - active_detections[i].cy;
                float dist_sq = dx * dx + dy * dy;
                if (dist_sq <= max_dist_sq) {
                    float dist = std::sqrt(dist_sq);
                    edges.push_back({ 0, i, dist });
                }
            }
        } else if (topology == 2) { // Full Plexus Mesh
            std::vector<bool> connected(n * n, false);
            int max_conn_per_node = static_cast<int>(plexus_max_connections);
            if (max_conn_per_node < 1)
                max_conn_per_node = 8;

            for (int i = 0; i < n; ++i) {
                std::vector<std::pair<int, float>> neighbors;
                for (int j = 0; j < n; ++j) {
                    if (i == j)
                        continue;
                    float dx = (active_detections[i].cx - active_detections[j].cx) * par;
                    float dy = active_detections[i].cy - active_detections[j].cy;
                    float dist_sq = dx * dx + dy * dy;
                    if (dist_sq <= max_dist_sq) {
                        float dist = std::sqrt(dist_sq);
                        neighbors.push_back({ j, dist });
                    }
                }

                std::sort(neighbors.begin(), neighbors.end(),
                    [](const auto& a, const auto& b) { return a.second < b.second; });

                int limit = (std::min)((int)neighbors.size(), max_conn_per_node);
                for (int k = 0; k < limit; ++k) {
                    int j = neighbors[k].first;
                    connected[i * n + j] = true;
                    connected[j * n + i] = true;
                }
            }

            for (int i = 0; i < n; ++i) {
                for (int j = i + 1; j < n; ++j) {
                    if (connected[i * n + j]) {
                        float dx
                            = (active_detections[i].cx - active_detections[j].cx) * par;
                        float dy = active_detections[i].cy - active_detections[j].cy;
                        float dist = std::sqrt(dx * dx + dy * dy);
                        edges.push_back({ i, j, dist });
                    }
                }
            }
        } else if (topology == 3) { // Minimum Spanning Tree (MST)
            auto mst_edges = compute_mst(active_detections, par);
            for (const auto& e : mst_edges) {
                if (e.weight <= max_dist) {
                    edges.push_back(e);
                }
            }
        } else if (topology == 4) { // Delaunay Triangulation
            auto delaunay_edges = compute_delaunay(active_detections, par);
            for (const auto& e : delaunay_edges) {
                if (e.weight <= max_dist) {
                    edges.push_back(e);
                }
            }
        }

        // Pre-reserve drawing vector capacity to avoid dynamic reallocations during
        // drawing loops
        size_t estimated_lines = edges.size() + active_detections.size() * 12;
        batched_lines.reserve(estimated_lines);
        if (draw_triangles) {
            size_t estimated_tris = edges.size() * 2;
            batched_tris.reserve(estimated_tris);
        }

        auto yolo_colors_param
            = ctx.param<aetk::effect::arbitrary_param<aetk::effect::ui::class_color_map>>(
                "yolo_class_colors");
        aetk::effect::locked_arbitrary<aetk::effect::ui::class_color_map> yolo_colors(
            yolo_colors_param);

        auto is_class_disabled = [&](const std::string& name) {
            if (track_mode == 2 && yolo_colors) {
                return yolo_colors->get_disabled(name);
            }
            return false;
        };

        auto get_class_color
            = [&](const std::string& name) -> aetk::core::color<pixel_range::tkuint8> {
            if (yolo_colors) {
                float r = 1.0f, g = 1.0f, b = 1.0f;
                if (yolo_colors->get_color(name, r, g, b)) {
                    aetk::core::color<pixel_range::tkuint8> c;
                    c.red = r * 255.0f;
                    c.green = g * 255.0f;
                    c.blue = b * 255.0f;
                    c.alpha = 255.0f;
                    return c;
                }
            }
            float h = std::fmod(
                (int)(std::hash<std::string> { }(name) & 0x7FFFFFFF) * 137.5f, 360.0f);
            constexpr float palette_s = 0.85f * 255.0f;
            constexpr float palette_v = 1.0f * 255.0f;
            aetk::core::color<pixel_range::tkuint8> c
                = aetk::core::color<pixel_range::tkuint8>::from_hsv(
                    h, palette_s, palette_v);
            return c;
        };

        auto get_random_instance_color
            = [](int instance_id) -> aetk::core::color<pixel_range::tkuint8> {
            uint32_t seed = static_cast<uint32_t>(instance_id + 1);
            seed ^= seed >> 16;
            seed *= 0x7feb352dU;
            seed ^= seed >> 15;
            seed *= 0x846ca68bU;
            seed ^= seed >> 16;

            float h = static_cast<float>(seed % 360u);
            float saturation
                = 0.72f + static_cast<float>((seed >> 8) & 0xFFu) / 255.0f * 0.23f;
            float value
                = 0.84f + static_cast<float>((seed >> 16) & 0xFFu) / 255.0f * 0.16f;
            aetk::core::color<pixel_range::tkuint8> c
                = aetk::core::color<pixel_range::tkuint8>::from_hsv(
                    h, saturation * 255.0f, value * 255.0f);
            return c;
        };

        const bool use_class_spectrum = (output_mode != 2 && color_mode == 1);
        const bool use_random_instance_colors = (output_mode != 2 && color_mode == 2);

        // Construct connectivity and opacity matrices for Clique-3 triangle
        // rendering (using flat 1D contiguous vectors to avoid dynamic allocation
        // overhead)
        std::vector<uint8_t> connected(n * n, 0);
        std::vector<float> edge_opacities(n * n, 0.0f);

        // 5. Draw Plexus connections
        for (const auto& edge : edges) {
            const auto& n1 = active_detections[edge.u];
            const auto& n2 = active_detections[edge.v];

            if (is_class_disabled(n1.class_name) || is_class_disabled(n2.class_name)) {
                continue;
            }

            // Safe coordinates check
            if (!std::isfinite(n1.cx) || !std::isfinite(n1.cy) || !std::isfinite(n2.cx)
                || !std::isfinite(n2.cy)) {
                continue;
            }

            float x = edge.weight / max_dist;
            x = std::clamp(x, 0.0f, 1.0f);

            float opacity_multiplier = 1.0f;
            if (!pr_data->connection_spline.points.empty()) {
                float val_y = aetk::effect::ui::curve_editor::evaluate(
                    pr_data->connection_spline.points, x,
                    aetk::effect::ui::curve_editor::interpolation::catmull_rom);
                opacity_multiplier = std::clamp(val_y, 0.0f, 1.0f);
            }

            if (opacity_multiplier <= 0.001f)
                continue;

            // Register valid edge in connectivity matrix for triangles
            connected[edge.u * n + edge.v] = 1;
            connected[edge.v * n + edge.u] = 1;
            edge_opacities[edge.u * n + edge.v] = opacity_multiplier;
            edge_opacities[edge.v * n + edge.u] = opacity_multiplier;

            aetk::core::color<pixel_range::tkuint8> line_col = line_color;
            if (use_class_spectrum) {
                aetk::core::color<pixel_range::tkuint8> c_u
                    = get_class_color(active_detections[edge.u].class_name);
                aetk::core::color<pixel_range::tkuint8> c_v
                    = get_class_color(active_detections[edge.v].class_name);
                line_col = aetk::core::color<pixel_range::tkuint8> { 255.0f,
                    (c_u.red + c_v.red) * 0.5f, (c_u.green + c_v.green) * 0.5f,
                    (c_u.blue + c_v.blue) * 0.5f };
            } else if (use_random_instance_colors) {
                aetk::core::color<pixel_range::tkuint8> c_u
                    = get_random_instance_color(edge.u);
                aetk::core::color<pixel_range::tkuint8> c_v
                    = get_random_instance_color(edge.v);
                line_col = aetk::core::color<pixel_range::tkuint8> { 255.0f,
                    (c_u.red + c_v.red) * 0.5f, (c_u.green + c_v.green) * 0.5f,
                    (c_u.blue + c_v.blue) * 0.5f };
            }

            line_col.alpha = opacity_multiplier * 255.0f;
            int line_w = std::max(1, (int)(plexus_thickness * scale_factor));

            if (line_geometry == 0) { // Straight Vectors
                draw_line(
                    (int)n1.cx, (int)n1.cy, (int)n2.cx, (int)n2.cy, line_col, line_w);
            } else { // Quadratic Bezier Arcs
                float mid_x = (n1.cx + n2.cx) * 0.5f;
                float mid_y = (n1.cy + n2.cy) * 0.5f;
                float dx = n2.cx - n1.cx;
                float dy = n2.cy - n1.cy;

                float perp_x = -dy;
                float perp_y = dx;
                float perp_len = std::sqrt(perp_x * perp_x + perp_y * perp_y);
                if (perp_len > 0.0001f) {
                    perp_x /= perp_len;
                    perp_y /= perp_len;
                }

                float curvature_offset = (curvature_pct / 100.0f) * edge.weight * 0.25f;
                if ((edge.u + edge.v) % 2 == 0) {
                    curvature_offset = -curvature_offset;
                }

                int cx_val = (int)(mid_x + perp_x * curvature_offset);
                int cy_val = (int)(mid_y + perp_y * curvature_offset);

                draw_bezier((int)n1.cx, (int)n1.cy, cx_val, cy_val, (int)n2.cx,
                    (int)n2.cy, line_col, line_w);
            }

            if (draw_arrows) {
                float dx = n2.cx - n1.cx;
                float dy = n2.cy - n1.cy;
                float len = std::sqrt(dx * dx + dy * dy);
                if (len > 15.0f * scale_factor) {
                    dx /= len;
                    dy /= len;

                    // Determine midpoint of the line or bezier curve
                    float mx = (n1.cx + n2.cx) * 0.5f;
                    float my = (n1.cy + n2.cy) * 0.5f;
                    if (line_geometry != 0) { // Bezier midpoint
                        float mid_x = (n1.cx + n2.cx) * 0.5f;
                        float mid_y = (n1.cy + n2.cy) * 0.5f;
                        float perp_x = -dy;
                        float perp_y = dx;
                        float curvature_offset
                            = (curvature_pct / 100.0f) * edge.weight * 0.25f;
                        if ((edge.u + edge.v) % 2 == 0) {
                            curvature_offset = -curvature_offset;
                        }
                        mx = mid_x + perp_x * curvature_offset * 0.5f;
                        my = mid_y + perp_y * curvature_offset * 0.5f;
                    }

                    // Scale arrow size
                    float arrow_len_val = 16.0f * scale_factor;
                    float arrow_width_val = 8.0f * scale_factor;

                    // Tip of the arrow (centered at midpoint, pointing from n1 to n2)
                    float tx = mx + dx * arrow_len_val * 0.5f;
                    float ty = my + dy * arrow_len_val * 0.5f;

                    // Base of the arrow
                    float bx = mx - dx * arrow_len_val * 0.5f;
                    float by = my - dy * arrow_len_val * 0.5f;

                    // Wings extending from the base perpendicular to direction
                    float wx1 = bx - dy * arrow_width_val;
                    float wy1 = by + dx * arrow_width_val;
                    float wx2 = bx + dy * arrow_width_val;
                    float wy2 = by - dx * arrow_width_val;

                    aetk::core::color<pixel_range::tkuint8> arrow_col = line_col;
                    arrow_col.alpha = opacity_multiplier * 255.0f;
                    draw_triangle((int)tx, (int)ty, (int)wx1, (int)wy1, (int)wx2,
                        (int)wy2, arrow_col);
                }
            }
        }

        // 5b. Render Plexus Triangles (Mutually connected triplets / Clique size 3)
        if (draw_triangles && n >= 3 && n <= 300) {
            std::vector<int> active_indices;
            active_indices.reserve(n);
            for (int i = 0; i < n; ++i) {
                if (!is_class_disabled(active_detections[i].class_name)) {
                    active_indices.push_back(i);
                }
            }
            int num_active = (int)active_indices.size();

            // Build adjacency list for active nodes
            std::vector<std::vector<int>> adj(num_active);
            for (int i = 0; i < num_active; ++i) {
                int u = active_indices[i];
                for (int j = i + 1; j < num_active; ++j) {
                    int v = active_indices[j];
                    if (connected[u * n + v]) {
                        adj[i].push_back(j);
                    }
                }
            }

            int tri_count = 0;
            for (int i = 0; i < num_active && tri_count < 1000; ++i) {
                const auto& neighbors = adj[i];
                int num_neighbors = (int)neighbors.size();
                int u_global = active_indices[i];

                for (int nj = 0; nj < num_neighbors && tri_count < 1000; ++nj) {
                    int j_local = neighbors[nj];
                    int j_global = active_indices[j_local];

                    for (int nk = nj + 1; nk < num_neighbors && tri_count < 1000; ++nk) {
                        int k_local = neighbors[nk];
                        int k_global = active_indices[k_local];

                        if (connected[j_global * n + k_global]) {
                            const auto& p1 = active_detections[u_global];
                            const auto& p2 = active_detections[j_global];
                            const auto& p3 = active_detections[k_global];

                            float op_ij = edge_opacities[u_global * n + j_global];
                            float op_jk = edge_opacities[j_global * n + k_global];
                            float op_ki = edge_opacities[k_global * n + u_global];
                            float avg_opacity = (op_ij + op_jk + op_ki) / 3.0f;

                            if (avg_opacity > 0.001f) {
                                aetk::core::color<pixel_range::tkuint8> tri_col
                                    = triangle_fill_color;
                                if (use_class_spectrum) {
                                    aetk::core::color<pixel_range::tkuint8> c_i
                                        = get_class_color(p1.class_name);
                                    aetk::core::color<pixel_range::tkuint8> c_j
                                        = get_class_color(p2.class_name);
                                    aetk::core::color<pixel_range::tkuint8> c_k
                                        = get_class_color(p3.class_name);
                                    tri_col = aetk::core::color<pixel_range::tkuint8> {
                                        255.0f, (c_i.red + c_j.red + c_k.red) / 3.0f,
                                        (c_i.green + c_j.green + c_k.green) / 3.0f,
                                        (c_i.blue + c_j.blue + c_k.blue) / 3.0f
                                    };
                                } else if (use_random_instance_colors) {
                                    aetk::core::color<pixel_range::tkuint8> c_i
                                        = get_random_instance_color(u_global);
                                    aetk::core::color<pixel_range::tkuint8> c_j
                                        = get_random_instance_color(j_global);
                                    aetk::core::color<pixel_range::tkuint8> c_k
                                        = get_random_instance_color(k_global);
                                    tri_col = aetk::core::color<pixel_range::tkuint8> {
                                        255.0f, (c_i.red + c_j.red + c_k.red) / 3.0f,
                                        (c_i.green + c_j.green + c_k.green) / 3.0f,
                                        (c_i.blue + c_j.blue + c_k.blue) / 3.0f
                                    };
                                }
                                tri_col.alpha = avg_opacity * 0.25f
                                    * 255.0f; // subtle cyberpunk tint
                                draw_triangle((int)p1.cx, (int)p1.cy, (int)p2.cx,
                                    (int)p2.cy, (int)p3.cx, (int)p3.cy, tri_col);
                                tri_count++;
                            }
                        }
                    }
                }
            }
        }

        // 6. Draw Markers, Bounding Boxes, Corner brackets, and Alphanumeric
        // Labels
        int marker_thickness = (std::max)(1, (int)(2.0f * scale_factor));
        int marker_size = (std::max)(2, (int)(6.0f * scale_factor));
        int dot_radius = (std::max)(2, (int)(4.0f * scale_factor));
        int line_w = (std::max)(1, (int)(bounding_box_thickness * scale_factor));
        int corner_len = (std::max)(3, (int)(8.0f * scale_factor));

        struct BoxBounds {
            float xmin, ymin, xmax, ymax;
            bool operator<(const BoxBounds& o) const {
                if (xmin != o.xmin)
                    return xmin < o.xmin;
                if (ymin != o.ymin)
                    return ymin < o.ymin;
                if (xmax != o.xmax)
                    return xmax < o.xmax;
                return ymax < o.ymax;
            }
        };
        std::set<BoxBounds> drawn_boxes;

        for (int i = 0; i < n; ++i) {
            const auto& det = active_detections[i];

            if (is_class_disabled(det.class_name)) {
                continue;
            }

            // Safe coordinates check
            if (!std::isfinite(det.cx) || !std::isfinite(det.cy)
                || !std::isfinite(det.xmin) || !std::isfinite(det.ymin)
                || !std::isfinite(det.xmax) || !std::isfinite(det.ymax)) {
                continue;
            }

            aetk::core::color<pixel_range::tkuint8> c_marker = marker_color;
            aetk::core::color<pixel_range::tkuint8> c_box = box_color;
            aetk::core::color<pixel_range::tkuint8> c_corner = corner_color;
            aetk::core::color<pixel_range::tkuint8> c_text = text_color;

            // Apply custom coloring spectrums if not in Luma Matte output mode
            if (use_class_spectrum) {
                aetk::core::color<pixel_range::tkuint8> clr
                    = get_class_color(det.class_name);
                c_marker = clr;
                c_box = clr;
                c_corner = clr;
            } else if (use_random_instance_colors) {
                aetk::core::color<pixel_range::tkuint8> clr
                    = get_random_instance_color(i);
                c_marker = clr;
                c_box = clr;
                c_corner = clr;
            }

            if (marker_style == 0) {
                draw_marker(
                    (int)det.cx, (int)det.cy, dot_radius, c_marker, marker_thickness, 0);
            } else if (marker_style == 1) {
                draw_marker(
                    (int)det.cx, (int)det.cy, marker_size, c_marker, marker_thickness, 1);
            } else if (marker_style == 2) {
                draw_marker(
                    (int)det.cx, (int)det.cy, marker_size, c_marker, marker_thickness, 2);
            }

            float box_cx = (det.xmin + det.xmax) * 0.5f;
            float box_cy = (det.ymin + det.ymax) * 0.5f;
            float rx = (det.xmax - det.xmin) * 0.5f;
            float ry = (det.ymax - det.ymin) * 0.5f;
            size_t num_shape_pts = std::min(shape_points.size(), (size_t)32);
            aetk::core::vec2 proj_pts[32];
            for (size_t pt_idx = 0; pt_idx < num_shape_pts; ++pt_idx) {
                proj_pts[pt_idx].x = box_cx + shape_points[pt_idx].x * rx;
                proj_pts[pt_idx].y = box_cy + shape_points[pt_idx].y * ry;
            }

            BoxBounds bounds { det.xmin, det.ymin, det.xmax, det.ymax };
            bool already_drawn = (drawn_boxes.find(bounds) != drawn_boxes.end());

            // Box Style: None (0), Hollow Box (1), Corner Brackets (2), Solid Filled (3)
            if (box_style > 0 && !already_drawn) {

                if (box_style == 1) { // Hollow Box
                    for (size_t pt_idx = 0; pt_idx < num_shape_pts; ++pt_idx) {
                        size_t next_idx = (pt_idx + 1) % num_shape_pts;
                        draw_line((int)proj_pts[pt_idx].x, (int)proj_pts[pt_idx].y,
                            (int)proj_pts[next_idx].x, (int)proj_pts[next_idx].y, c_box,
                            line_w);
                    }
                } else if (box_style == 2) { // Corner Brackets
                    float bracket_len_val = (float)corner_len;
                    for (size_t pt_idx = 0; pt_idx < num_shape_pts; ++pt_idx) {
                        size_t prev_idx = (pt_idx + num_shape_pts - 1) % num_shape_pts;
                        size_t next_idx = (pt_idx + 1) % num_shape_pts;

                        float px = proj_pts[pt_idx].x;
                        float py = proj_pts[pt_idx].y;

                        float prev_x = proj_pts[prev_idx].x;
                        float prev_y = proj_pts[prev_idx].y;

                        float next_x = proj_pts[next_idx].x;
                        float next_y = proj_pts[next_idx].y;

                        float dx_in = prev_x - px;
                        float dy_in = prev_y - py;
                        float len_in = std::sqrt(dx_in * dx_in + dy_in * dy_in);
                        if (len_in > 0.0001f) {
                            float limit_len = std::min(bracket_len_val, len_in * 0.5f);
                            float bx = px + (dx_in / len_in) * limit_len;
                            float by = py + (dy_in / len_in) * limit_len;
                            draw_line(
                                (int)px, (int)py, (int)bx, (int)by, c_corner, line_w);
                        }

                        float dx_out = next_x - px;
                        float dy_out = next_y - py;
                        float len_out = std::sqrt(dx_out * dx_out + dy_out * dy_out);
                        if (len_out > 0.0001f) {
                            float limit_len = std::min(bracket_len_val, len_out * 0.5f);
                            float bx = px + (dx_out / len_out) * limit_len;
                            float by = py + (dy_out / len_out) * limit_len;
                            draw_line(
                                (int)px, (int)py, (int)bx, (int)by, c_corner, line_w);
                        }
                    }
                } else if (box_style == 3) { // Solid Filled
                    aetk::core::color<pixel_range::tkuint8> fill_color = c_box;
                    fill_color.alpha
                        = std::clamp(box_fill_opacity / 100.0f, 0.0f, 1.0f) * 255.0f;
                    for (size_t pt_idx = 0; pt_idx < num_shape_pts; ++pt_idx) {
                        size_t next_idx = (pt_idx + 1) % num_shape_pts;
                        draw_triangle((int)det.cx, (int)det.cy, (int)proj_pts[pt_idx].x,
                            (int)proj_pts[pt_idx].y, (int)proj_pts[next_idx].x,
                            (int)proj_pts[next_idx].y, fill_color);
                    }
                    for (size_t pt_idx = 0; pt_idx < num_shape_pts; ++pt_idx) {
                        size_t next_idx = (pt_idx + 1) % num_shape_pts;
                        draw_line((int)proj_pts[pt_idx].x, (int)proj_pts[pt_idx].y,
                            (int)proj_pts[next_idx].x, (int)proj_pts[next_idx].y, c_box,
                            line_w);
                    }
                }
            }

            // Label Style: None (0), Text Out (1), Text In (2), Badge Out (3),
            // Badge In (4)
            if (label_style > 0 && !already_drawn) {
                std::string label_text;
                if (track_mode == 2) {
                    label_text = yolo_colors
                        ? yolo_colors->get_display_name(det.class_name)
                        : det.class_name;
                    char conf_str[16];
                    aetk::core::c_snprintf(conf_str, sizeof(conf_str), " %d%%",
                        (int)(det.confidence * 100.0f));
                    label_text += conf_str;
                } else {
                    char pos_str[32];
                    aetk::core::c_snprintf(pos_str, sizeof(pos_str), "X:%.0f Y:%.0f", det.cx, det.cy);
                    label_text = pos_str;
                }

                float text_w
                    = (label_text.length() * 5.5f * scale_factor * hud_text_size) / par;
                float text_h = 6.0f * scale_factor * hud_text_size;

                float shape_xmin = det.xmin;
                float shape_ymin = det.ymin;
                if (num_shape_pts > 0) {
                    shape_xmin = proj_pts[0].x;
                    shape_ymin = proj_pts[0].y;
                    for (size_t pt_idx = 1; pt_idx < num_shape_pts; ++pt_idx) {
                        if (proj_pts[pt_idx].x < shape_xmin)
                            shape_xmin = proj_pts[pt_idx].x;
                        if (proj_pts[pt_idx].y < shape_ymin)
                            shape_ymin = proj_pts[pt_idx].y;
                    }
                }

                int lx = (int)shape_xmin;
                int ly = (int)shape_ymin;

                aetk::core::color<pixel_range::tkuint8> label_text_color = c_text;

                bool is_badge = (label_style == 3 || label_style == 4);
                int label_pad = (std::max)(1, (int)(2.0f * scale_factor));
                int label_gap = (std::max)(2, (int)(4.0f * scale_factor));

                if (is_badge) {
                    int pad_x = (int)((3.0f * scale_factor * hud_text_size) / par);
                    int pad_y = (int)(2.0f * scale_factor * hud_text_size);

                    int bx0 = (int)shape_xmin;
                    int bx1 = (int)shape_xmin + (int)text_w + 2 * pad_x;
                    int by0 = 0;
                    int by1 = 0;

                    if (label_style == 3) { // Badge Out
                        by0 = (int)shape_ymin - (int)text_h - 2 * pad_y;
                        by1 = (int)shape_ymin;
                        if (by0 < 0) { // Clamp/flip to inside
                            by0 = (int)shape_ymin;
                            by1 = (int)shape_ymin + (int)text_h + 2 * pad_y;
                        }
                    } else { // Badge In
                        by0 = (int)shape_ymin;
                        by1 = (int)shape_ymin + (int)text_h + 2 * pad_y;
                        if (by1 >= dst.height()) { // Clamp/flip to outside
                            by0 = (int)shape_ymin - (int)text_h - 2 * pad_y;
                            by1 = (int)shape_ymin;
                        }
                    }

                    // Shift horizontally if clipping off right side
                    if (bx1 >= dst.width()) {
                        int shift = bx1 - (dst.width() - 1);
                        bx0 -= shift;
                        bx1 -= shift;
                        if (bx0 < 0)
                            bx0 = 0;
                    }

                    bx0 = (std::max)(0, bx0);
                    by0 = (std::max)(0, by0);
                    bx1 = (std::min)(dst.width() - 1, bx1);
                    by1 = (std::min)(dst.height() - 1, by1);

                    lx = bx0 + pad_x;
                    ly = by0 + pad_y;

                    // Classic solid badge colored by c_box (bounding box color)
                    aetk::core::color<pixel_range::tkuint8> bg_col = c_box;
                    bg_col.alpha = 255.0f; // Solid colored background

                    draw_triangle((float)bx0, (float)by0, (float)bx1, (float)by0,
                        (float)bx0, (float)by1, bg_col);
                    draw_triangle((float)bx1, (float)by0, (float)bx1, (float)by1,
                        (float)bx0, (float)by1, bg_col);

                    // Keep label text color as c_text (customizable HUD Text Color)
                    label_text_color = c_text;
                } else {
                    // Regular text (no badge background)
                    if (label_style == 2) { // Text In
                        lx = (int)shape_xmin + label_pad;
                        ly = (int)shape_ymin + label_pad;
                    } else { // Text Out
                        lx = (int)shape_xmin;
                        ly = (int)shape_ymin - (int)text_h - label_gap;
                        if (ly < 0) { // Clamp to inside
                            ly = (int)shape_ymin + label_pad;
                        }
                    }
                    label_text_color = c_text;
                }

                int text_thickness
                    = (std::max)(1, (int)(1.5f * scale_factor * hud_text_size));
                draw_stroke_string((float)lx, (float)ly, label_text, label_text_color,
                    scale_factor, hud_text_size, text_thickness);
            }
            if (!already_drawn) {
                drawn_boxes.insert(bounds);
            }
        }

        // 7. Draw sci-fi diagnostic terminal readout in top-left
        if (draw_diagnostics) {
            std::vector<std::string> diag_lines;
            diag_lines.push_back("SYS STATUS: ACTIVE");
            diag_lines.push_back("TRACK MODE: "
                + std::string(track_mode == 0
                        ? "SILHOUETTE"
                        : (track_mode == 1 ? "HSV KEYER" : "YOLOv8 AI")));
            diag_lines.push_back("NODES CNT : " + std::to_string(n));
            diag_lines.push_back("EDGES CNT : " + std::to_string(edges.size()));
            diag_lines.push_back("MAX DIST  : " + std::to_string((int)max_dist));
            diag_lines.push_back("CURVE OK  : "
                + std::string(!pr_data->connection_spline.points.empty() ? "YES" : "NO"));

            if (track_mode == 0 || track_mode == 1) {
                diag_lines.push_back("HSV TARGET: H:" + std::to_string((int)target_h)
                    + " S:" + std::to_string((int)target_s)
                    + " V:" + std::to_string((int)target_v));
                diag_lines.push_back("HSV TOLS  : H:" + std::to_string((int)h_tol)
                    + " S/V:" + std::to_string((int)s_tol));
            } else if (track_mode == 2) {
                int total_raw = cached_detections ? (int)cached_detections->size() : 0;
                float max_conf = 0.0f;
                std::string top_cls = "NONE";
                int rej_area = 0;
                if (cached_detections) {
                    for (const auto& det : *cached_detections) {
                        if (det.confidence > max_conf) {
                            max_conf = det.confidence;
                            top_cls = yolo_colors
                                ? yolo_colors->get_display_name(det.class_name)
                                : det.class_name;
                        }
                        float area = (det.xmax - det.xmin) * (det.ymax - det.ymin);
                        if (area < scaled_min_area || area > max_area_threshold) {
                            rej_area++;
                        }
                    }
                }
                diag_lines.push_back("YOLO MODEL: "
                    + std::string(
                        OrtEngine::IsModelLoaded("yolo", "") ? "LOADED" : "NOT LOADED"));
                diag_lines.push_back("RAW CAND  : " + std::to_string(total_raw));
                diag_lines.push_back("TOP CONF  : "
                    + std::to_string((int)(max_conf * 100)) + "% (" + top_cls + ")");
                diag_lines.push_back("REJ AREA  : " + std::to_string(rej_area)
                    + " (MIN:" + std::to_string((int)scaled_min_area) + ")");
            }

            float diag_margin = 15.0f * scale_factor * hud_text_size;
            float diag_step = 10.0f * scale_factor * hud_text_size;
            int text_thickness
                = (std::max)(1, (int)(1.5f * scale_factor * hud_text_size));

            for (size_t i = 0; i < diag_lines.size(); ++i) {
                draw_stroke_string(diag_margin, diag_margin + diag_step * i,
                    diag_lines[i], text_color, scale_factor, hud_text_size,
                    text_thickness);
            }
        }


        aetk::effect::draw::draw_triangles_batched(ctx, dst, batched_tris);
        aetk::effect::draw::draw_lines_batched(ctx, dst, batched_lines);
    }
};

AETK_EFFECT_MAIN(SWARM)
