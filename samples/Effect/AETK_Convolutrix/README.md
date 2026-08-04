# 🎛️ AETK_Convolutrix Sample Plugin

`AETK_Convolutrix` is a developer sample demonstrating custom kernel convolutions and neighborhood pixel sampling filters using AETK 2.0.

## 🛠️ Developer Reference

### Core Concepts Demonstrated
* **Pixel Neighborhood Sampling**: Performs matrix-based kernel filtering (blurs, sharpening, edge detection).
* **Pitch and Strides Access**: Directly accesses raw rowbytes and pointer arithmetic offsets safely.
* **Format Parity**: Compiles loops running over 8bpc, 16bpc, and 32bpc float sequences.

### Key API Usages
* `visit_pixel_format(format, is_bgra, lambda)`
* `pixel_accessor<PixelT, IsBGRA>::read`
* Struct-based kernel weights registration.
