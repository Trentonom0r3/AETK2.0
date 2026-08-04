# 📐 AETK_Resizer Sample Plugin

`AETK_Resizer` is a developer sample demonstrating layer coordinate conversions and dynamic output frame buffer resizing (ROI expansion and padding) in After Effects.

## 🛠️ Developer Reference

### Core Concepts Demonstrated
* **Buffer Expansion**: Resizes the output frame bounds beyond the source layer dimensions (e.g., adding padding for blurs or borders).
* **ROI Coordinate Transforms**: Correctly scales and shifts coordinate offsets between input space and resized output space.
* **Pre-Render Setup Checks**: Defines target dimensions in `on_pre_render` to request larger buffers from the host.

### Key API Usages
* `pre_render_context::checkout_layer` dimensions override.
* `smart_world::zeros` custom dimensions allocation.
* Sub-reference Region of Interest (ROI) mapping.
