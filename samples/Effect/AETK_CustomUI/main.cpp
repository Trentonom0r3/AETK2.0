#include <aetk/effect.hpp>
#include <aetk/effect/ui.hpp>

#include <algorithm>
#include <vector>

using namespace aetk::effect;
using namespace aetk::core;
using namespace aetk::effect::ui;

// ══════════════════════════════════════════════════════════════════════
//  Plugin Class
// ══════════════════════════════════════════════════════════════════════

class drawbot_test : public plugin<drawbot_test> {
public:
    static void on_about(const aetk::effect::context& ctx) {
        ctx.set_dialog_response("AETK Drawbot Test: Custom UI, Comp/Layer Overlay UI, "
                                "and UI framework testbed.");
    }

    static void on_global_setup(const global_setup_context& ctx) {
        aetk::core::logger::instance().init("aetk_drawbot_debug.log");
        aetk::core::logger::instance().set_level(aetk::core::log_level::trace);
        ctx.enable_param_supervision();
        ctx.enable_smart_render();
        ctx.enable_custom_ui();
        ctx.enable_threaded_rendering();
    }

    static void on_params_setup(const params_setup_context& ctx) {
        // Register for Effect Controls Window (ECW) as well as Comp and Layer viewports
        ctx.register_comp_ui(PF_CustomEFlag_COMP | PF_CustomEFlag_LAYER | PF_CustomEFlag_EFFECT);

        ui::add_widget<curve_group>(ctx, "RGB Curves",
            curve_group::options()
                .add_channel("Master",
                    curve_editor::options()
                        .set_height(150.0f)
                        .set_curve_type(curve_editor::interpolation::catmull_rom)
                        .set_grid(4, 4)
                        .set_node_color({ 1.0f, 1.0f, 1.0f, 1.0f }))
                .add_channel("Red",
                    curve_editor::options()
                        .set_height(150.0f)
                        .set_curve_type(curve_editor::interpolation::catmull_rom)
                        .set_grid(4, 4)
                        .set_node_color({ 1.0f, 1.0f, 0.2f, 0.2f }))
                .add_channel("Green",
                    curve_editor::options()
                        .set_height(150.0f)
                        .set_curve_type(curve_editor::interpolation::catmull_rom)
                        .set_grid(4, 4)
                        .set_node_color({ 1.0f, 0.2f, 1.0f, 0.2f }))
                .add_channel("Blue",
                    curve_editor::options()
                        .set_height(150.0f)
                        .set_curve_type(curve_editor::interpolation::catmull_rom)
                        .set_grid(4, 4)
                        .set_node_color({ 1.0f, 0.2f, 0.2f, 1.0f })));

        // 2. Custom Toggle Button
        ui::add_widget<button>(ctx, "Invert Output", "Invert Colors", nullptr,
            button::options().set_toggle_mode(true).set_corner_radius(4.0f));

        // 3. Custom Int Slider
        ui::add_widget<slider<int>>(ctx, "Posterize Levels", "Levels", 1, 255, 255,
            nullptr,
            slider<int>::options().set_step(1).set_display_format("%d").set_show_label(
                true));

        // 6. Standalone Keyframeable Tactile Joystick
        ui::add_widget<joystick_pad>(ctx, "XY Joystick", "XY Joystick",
            joystick_pad::options().set_resistance(0.3f).set_snap_to_center(true));
    }

    /**
     * @brief Handler for Composition (PF_Window_COMP) and Layer (PF_Window_LAYER) Custom UI overlays.
     */
    static void on_comp_ui(const comp_ui::context& ctx) {
        // Persistent selection region bounds in layer coordinates
        static vec2 selection_center{ 200.0f, 200.0f };
        static float selection_half_size = 80.0f;
        static bool is_region_selected = false;

        switch (ctx.type()) {
        case comp_ui::context::event_type::draw: {
            auto cvs = ctx.canvas();
            if (!cvs.valid()) break;

            comp_ui::theme th(ctx);

            // Define selection box in layer space
            vec2 l_corners[4] = {
                { selection_center.x - selection_half_size, selection_center.y - selection_half_size },
                { selection_center.x + selection_half_size, selection_center.y - selection_half_size },
                { selection_center.x + selection_half_size, selection_center.y + selection_half_size },
                { selection_center.x - selection_half_size, selection_center.y + selection_half_size }
            };

            // Transform layer coordinates to viewport frame screen space
            vec2 f_corners[4];
            for (int i = 0; i < 4; ++i) {
                f_corners[i] = ctx.layer_to_frame(l_corners[i]);
            }

            // Build bounding box path using Drawbot path_builder
            aetk::core::suite<DRAWBOT_PathSuite1> path_suite(ctx.in_data_ptr()->pica_basicP);
            if (path_suite.get()) {
                aetk::ui::drawbot::path_builder builder(cvs.supplier_suite(), path_suite.get(), cvs.raw_supplier_ref());
                builder.move_to(f_corners[0].x, f_corners[0].y)
                       .line_to(f_corners[1].x, f_corners[1].y)
                       .line_to(f_corners[2].x, f_corners[2].y)
                       .line_to(f_corners[3].x, f_corners[3].y)
                       .close();
                aetk::ui::drawbot::path p = builder.build();

                // If selected, draw translucent highlight fill
                if (is_region_selected) {
                    aetk::core::color<aetk::core::pixel_range::tkfloat> fill_col(0.2f, 0.6f, 1.0f, 0.25f);
                    auto sup = cvs.get_supplier();
                    auto b = sup.create_brush(fill_col);
                    cvs.fill_path(p, b);
                }

                // Stroke outline path with host theme suite
                th.stroke_path(cvs.raw_draw_ref(), p.get(), true);
            }

            // Draw vertex handles at corner points
            for (int i = 0; i < 4; ++i) {
                th.fill_vertex(cvs.raw_draw_ref(), f_corners[i], true);
            }

            // Draw center interactive handle
            vec2 f_center = ctx.layer_to_frame(selection_center);
            th.fill_vertex(cvs.raw_draw_ref(), f_center, true);

            ctx.set_handled();
            break;
        }

        case comp_ui::context::event_type::click: {
            vec2 mouse_layer = ctx.layer_mouse_point();

            // Hit test against selection center handle
            vec2 diff = mouse_layer - selection_center;
            float dist = std::abs(diff.x) + std::abs(diff.y);

            if (dist <= selection_half_size) {
                // If Shift key is held down, toggle selection highlight
                if (ctx.shift_down()) {
                    is_region_selected = !is_region_selected;
                } else {
                    is_region_selected = true;
                }

                // Display selection info in AE's Info Panel
                char text1[64], text2[64];
                std::snprintf(text1, sizeof(text1), "Selection: [%.1f, %.1f]", selection_center.x, selection_center.y);
                std::snprintf(text2, sizeof(text2), "Mode: %s (Shift+Click to Toggle)", is_region_selected ? "Active" : "Inactive");
                ctx.info_draw_text(text1, text2);

                ctx.request_drag(true, 1);
                ctx.set_handled();
                ctx.request_update();
            }
            break;
        }

        case comp_ui::context::event_type::drag: {
            if (ctx.drag_refcon(0) == 1) {
                // Move selection region center with mouse
                selection_center = ctx.layer_mouse_point();

                char text1[64], text2[64];
                std::snprintf(text1, sizeof(text1), "Dragging Region: [%.1f, %.1f]", selection_center.x, selection_center.y);
                std::snprintf(text2, sizeof(text2), "Status: %s", ctx.is_last_drag_frame() ? "Released" : "Dragging...");
                ctx.info_draw_text(text1, text2);

                ctx.set_handled();
                ctx.request_update();
            }
            break;
        }

        case comp_ui::context::event_type::adjust_cursor: {
            // Set finger pointer when hovering inside selection region
            vec2 mouse_layer = ctx.layer_mouse_point();
            vec2 diff = mouse_layer - selection_center;
            if (std::abs(diff.x) <= selection_half_size && std::abs(diff.y) <= selection_half_size) {
                if (ctx.extra_ptr()) {
                    auto extra = static_cast<PF_EventExtra*>(ctx.extra_ptr());
                    extra->u.adjust_cursor.set_cursor = PF_Cursor_FINGER_POINTER;
                    ctx.set_handled();
                }
            }
            break;
        }

        default:
            break;
        }
    }

    static void on_pre_render(const pre_render_context& ctx) {
        ctx.checkout_layer(0);
    }

    static void on_smart_render(const smart_render_context& ctx) {
        auto src = ctx.checkout_pixels(0);
        auto dst = ctx.checkout_output();

        // 1. Read Multi-Curve Data
        auto curve_param = ctx.param<arbitrary_param<ui::multi_curve_data>>("RGB Curves");
        const auto& m_pts = curve_param.value()->channels[0].points;
        const auto& r_pts = curve_param.value()->channels[1].points;
        const auto& g_pts = curve_param.value()->channels[2].points;
        const auto& b_pts = curve_param.value()->channels[3].points;

        // 2. Read Toggle Button
        auto invert_param = ctx.param<arbitrary_param<ui::button_data>>("Invert Output");
        bool invert = invert_param.value()->active;

        // 3. Read Slider
        auto levels_param
            = ctx.param<arbitrary_param<ui::slider_data<int>>>("Posterize Levels");
        int levels = levels_param.value()->value;

        src.iterate<pixel_range::tkuint8>(dst, [&](int x, int y, color<pixel_range::tkuint8>& c) {
            // Convert to 0..1 range for curve math
            float r_val = (float)c.red * (1.0f / 255.0f);
            float g_val = (float)c.green * (1.0f / 255.0f);
            float b_val = (float)c.blue * (1.0f / 255.0f);

            // Map luminance through the Master curve
            float lum = 0.299f * r_val + 0.587f * g_val + 0.114f * b_val;
            float mapped_lum = math::evaluate_catmull_rom(m_pts, lum);

            // Preserve color by scaling with the new luminance
            if (lum > 0.001f) {
                r_val = (r_val / lum) * mapped_lum;
                g_val = (g_val / lum) * mapped_lum;
                b_val = (b_val / lum) * mapped_lum;
            }

            // Map Red through Catmull-Rom curve
            r_val = math::evaluate_catmull_rom(r_pts, r_val);

            // Map Green through Catmull-Rom curve
            g_val = math::evaluate_catmull_rom(g_pts, g_val);

            // Map Blue through Catmull-Rom curve
            b_val = math::evaluate_catmull_rom(b_pts, b_val);

            // Apply posterize
            if (levels < 255) {
                float factor = (float)levels;
                r_val = std::round(r_val * factor) / factor;
                g_val = std::round(g_val * factor) / factor;
                b_val = std::round(b_val * factor) / factor;
            }

            // Apply invert
            if (invert) {
                r_val = 1.0f - r_val;
                g_val = 1.0f - g_val;
                b_val = 1.0f - b_val;
            }

            // Clamp and write back to 0..255 color
            c.red   = std::clamp<double>(r_val * 255.0, 0.0, 255.0);
            c.green = std::clamp<double>(g_val * 255.0, 0.0, 255.0);
            c.blue  = std::clamp<double>(b_val * 255.0, 0.0, 255.0);
        });
    }
};

AETK_EFFECT_MAIN(drawbot_test)
