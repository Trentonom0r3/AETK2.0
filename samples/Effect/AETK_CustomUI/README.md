# 🎨 AETK_CustomUI Sample Plugin

`AETK_CustomUI` is a premium sample demonstrating custom user interfaces, vector drawing overlays, and complex widgets (curves, buttons, sliders, and joysticks) using AETK's Drawbot wrappers.

## 🛠️ Developer Reference

### Core Concepts Demonstrated
* **Interactive Overlays**: Draws custom vector items directly onto the After Effects composition view window.
* **Complex Custom Controls**:
  * `curve_group`: Multi-channel RGB spline curve editor with Catmull-Rom and Linear interpolation.
  * `joystick_pad`: Interactive 2D coordinates joystick controller.
  * `button`/`slider`: Custom vector buttons and sliders.
* **Persistent Widget State**: Binds parameters as arbitrary-data structs that serialize automatically inside saved projects.

### Key API Usages
* `ctx.register_custom_ui(PF_CustomEFlag_EFFECT)`
* `ui::add_widget<T>`
* Spline curve math and vector drawing calls.
