# AETK 2.0 Migration & Modernization Guide

This guide describes how to migrate classic, boilerplate-heavy Adobe After Effects SDK plugins to **AETK 2.0**. It captures design patterns, modern C++ type translations, and best practices derived from modernizing AETK's sample effects.

## Quick Reference Checklist

| Task / Operation | Classic AE SDK Approach | Modern AETK 2.0 Way |
| :--- | :--- | :--- |
| **Custom UI Setup** | Intercepting events in `PF_Cmd_EVENT` | Layout widgets autodetected via out-flags & canvas |
| **Pre-Render Data** | Setting `pre_render_data` pointers on `PF_PreRenderExtra` | `ctx.set_pre_render_data(data, deleter)` |
| **Pixel Iteration** | `in_data->utils->iterate` with a static callback | `ctx.iterate_pixels(input, output, lambda)` with standard coords |
| **Color Picker Dialog** | Querying `PF_AppSuite` color picker | `ctx.show_color_picker("Title", initial_color)` |
| **Comp Background** | Multi-suite AEGP hierarchy lookups | `ctx.get_comp_background_color()` |
| **Bit-Depth Formats** | Nested code branches per 8/16/32bpc formats | `.to(PF_PixelFormat_ARGB128)` to unify on float processing |

## 1. Custom UI Registration & Layouts

### Classic AE SDK Boilerplate

In classic plugins, custom UI and parameter drawing required intercepting events like `PF_Event_DRAW` inside `PF_Cmd_EVENT` and writing custom mouse interaction tracking loops:

```cpp
// CLASSIC AE SDK
static PF_Err HandleEvent(PF_InData *in_data, PF_OutData *out_data, PF_Cmd cmd, PF_EventExtra *extra) {
    if (cmd == PF_Cmd_EVENT) {
        if (extra->evt_code == PF_Event_DRAW) {
            // Manually draw UI elements with Drawbot or GDI...
        }
    }
    return PF_Err_NONE;
}
```

### Modern AETK Way

AETK handles this behind the scenes. Enable custom UI during setup, register it for viewports, and add widgets directly in `on_params_setup`:

```cpp
// MODERN AETK
static void on_global_setup(const global_setup_context &ctx) {
    ctx.enable_custom_ui();
}
static void on_params_setup(const params_setup_context& ctx) {
    // Register for Effect Controls Window (ECW) as well as Comp and Layer viewports
    ctx.register_comp_ui(PF_CustomEFlag_COMP | PF_CustomEFlag_LAYER | PF_CustomEFlag_EFFECT);

    // 1. Add interactive RGB Curves Group
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
    ui::add_widget<slider<int>>(ctx, "Posterize Levels", "Levels", 1, 255, 255, nullptr,
        slider<int>::options().set_step(1).set_display_format("%d").set_show_label(true));

    // 4. Standalone Keyframeable Tactile Joystick
    ui::add_widget<joystick_pad>(ctx, "XY Joystick", "XY Joystick",
        joystick_pad::options().set_resistance(0.3f).set_snap_to_center(true));
}
```

---

## 2. Pre-Render Data Management & Output Resizing (SmartFX)

### Classic AE SDK Boilerplate

In classic pre-render, expanding output bounds and passing pre-render structures to rendering threads requires manual pointer manipulation:

```cpp
// CLASSIC AE SDK
static PF_Err PreRender(PF_InData *in_data, PF_OutData *out_data, PF_PreRenderExtra *extra) {
    // 1. Pre-render data allocation
    MyRenderParams *params = new MyRenderParams();
    extra->output->pre_render_data = params;
    extra->output->delete_pre_render_data_func = FreeMyParams;

    // 2. Expanding output bounding box / requesting extra pixels
    extra->output->result_rect.left -= border;
    extra->output->result_rect.right += border;
    extra->output->result_rect.top -= border;
    extra->output->result_rect.bottom += border;
    extra->output->max_result_rect = extra->output->result_rect;
    extra->output->flags |= PF_RenderOutputFlag_RETURNS_EXTRA_PIXELS;

    return PF_Err_NONE;
}
```

### Modern AETK Way

Use the pre-render context methods to attach parameter structures cleanly and resize output bounds without direct struct manipulation:

```cpp
// MODERN AETK
static void on_pre_render(const pre_render_context &ctx) {
    ctx.checkout_layer(0, 0); // Check out input layer 0

    // 1. Safe pre-render data tracking
    MyRenderParams *params = new MyRenderParams();
    ctx.set_pre_render_data(params, FreeMyParams);

    // 2. Clear output bounds expansion
    ctx.expand_output_bounds(border, border, border, border); // left, top, right, bottom
    ctx.set_returns_extra_pixels(true);
}
```

---

## 2b. Output Sizing & Offsets for Classic Plugins (`PF_Cmd_FRAME_SETUP`)

> [!NOTE]
> `on_frame_setup` is only utilized for classic (non-SmartFX) plugins. For SmartFX plugins, size adjustments must be performed inside `on_pre_render` as shown in Section 2.

### Classic AE SDK Boilerplate

Resizing the output buffer and adjusting the coordinate origin had to be calculated on `out_data` manually:

```cpp
// CLASSIC AE SDK
static PF_Err FrameSetup(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_LayerDef *output) {
    out_data->width = 2 * border + params[0]->u.ld.width;
    out_data->height = 2 * border + params[0]->u.ld.height;
    out_data->origin.h = border;
    out_data->origin.v = border;
}
```

### Modern AETK Way

Use the `frame_setup_context` to adjust dimensions:

```cpp
// MODERN AETK
static void on_frame_setup(const frame_setup_context &ctx) {
    int32_t src_w = ctx.input_param().world().width();
    int32_t src_h = ctx.input_param().world().height();

    ctx.set_width(2 * border + src_w);
    ctx.set_height(2 * border + src_h);
    ctx.set_origin_h(border);
    ctx.set_origin_v(border);
}
```

---

## 3. High-Level Pixel Iteration

### Classic AE SDK Boilerplate

In classic code, iterating pixels required defining a static callback function, passing it to `in_data->utils->iterate`, and passing a state struct (`refcon` pointer) to share variables across pixels:

```cpp
// CLASSIC AE SDK
static PF_Err InvertPixel(void *refcon, A_long x, A_long y, PF_Pixel8 *in, PF_Pixel8 *out) {
    out->red   = 255 - in->red;
    out->green = 255 - in->green;
    out->blue  = 255 - in->blue;
    out->alpha = in->alpha;
    return PF_Err_NONE;
}

// Inside Render:
in_data->utils->iterate(
    in_data,
    0,
    output->height,
    &input,
    nullptr,
    nullptr, // refcon
    InvertPixel,
    output
);
```

### Modern AETK Way

Pass the `smart_world` references directly. Iteration is invoked as a member function `.iterate()` on the source `smart_world`. It automatically maps pixels to normalized floating-point references:

```cpp
// MODERN AETK
input.iterate(output, 
    [](int32_t x, int32_t y, aetk::core::color<>& c) {
        c.red = 1.0f - c.red;
        c.green = 1.0f - c.green;
        c.blue = 1.0f - c.blue;
    });
```

---

## 4. Dialogs & Composition Queries

### Classic AE SDK Boilerplate

Summoning the color picker dialog requires acquiring the host application suite, displaying the dialog with raw struct pointers, and releasing the suite:

```cpp
// CLASSIC AE SDK
PF_AppSuite4 *app_suite = nullptr;
AEFX_SuiteScoper<PF_AppSuite4> app_scoper(
    in_data,
    kPFAppSuite,
    kPFAppSuiteVersion4,
    out_data
);
app_scoper->PF_AppColorPickerDialog("Pick a Color", &initial, TRUE, &result);
// ... plus querying nested Comp/Layer/Interface AEGP suites to get background color
```

### Modern AETK Way

Use the built-in, type-safe context helpers directly on `context`:

```cpp
// MODERN AETK
aetk::core::color<> initial_color(1.0, 0.5, 0.2, 0.8);

// 1. Color Picker Dialog
if (auto color = ctx.show_color_picker("Customize Color", initial_color)) {
    // color is std::optional<aetk::core::color<>>
}

// 2. Get Composition Background Color
aetk::core::color<> bg = ctx.get_comp_background_color();
```

---

## 5. Unifying Rendering Pipeline (Bit-Depth Conversions)

Duplicate code branches for 8bpc, 16bpc, and 32bpc float pipelines lead to code bloat and maintenance errors.

### Before (Format-Specific Duplicate Loops)

```cpp
if (format == PF_PixelFormat_ARGB128) {
    // Loop 1 (Float math)
} else if (format == PF_PixelFormat_ARGB64) {
    // Loop 2 (16-bit integer scaling math)
} else {
    // Loop 3 (8-bit integer scaling math)
}
```

### After (Single Unified Float Pipeline)

Convert the input to a float scratch world, run a single clean float-based calculation loop, and convert back to the native output format:

```cpp
static void on_smart_render(const smart_render_context &ctx) {
    auto input = ctx.checkout_pixels(INPUT_PARAM);
    auto output = ctx.checkout_output();

    // 1. Convert source to 32bpc float and allocate 32-bit scratch output
    smart_world float_input = input.to(PF_PixelFormat_ARGB128);
    smart_world float_output(ctx.in_data_ptr(), output.width(), output.height(), 32);

    // 2. Process in float space using high-level color space conversions
    float_input.iterate(float_output, [&](int32_t x, int32_t y, aetk::core::color<>& c) {
        double h, s, l;
        c.to_hsl(h, s, l);
        
        // Example: Shift Hue
        h = std::fmod(h + 0.5, 1.0);
        
        c = aetk::core::color<>::from_hsl(h, s, l, c.alpha);
    });

    // 3. Convert processed scratch back to output's native format and copy
    smart_world native_converted = float_output.to(output.pixel_format());
    native_converted.copy_to(output);
}
```

---

## 6. Type Translations & Cleanup

When porting raw SDK code:

* Replace **`PF_Boolean`** with C++ standard **`bool`**.
* Replace **`PF_FpLong`** or **`PF_FpShort`** with standard **`double`** and **`float`**.
* **Remove redundant lines** setting `num_params` at param setup (e.g. `ctx.out_data_ptr()->num_params = NUM_PARAMS;`). AETK computes this count automatically based on the parameters added.
