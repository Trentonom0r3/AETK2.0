# 🔀 AETK_CrossHost Sample Plugin

`AETK_CrossHost` is a developer sample demonstrating cross-application hosting compatibility between Adobe After Effects and Adobe Premiere Pro.

## 🛠️ Developer Reference

### Core Concepts Demonstrated
* **Cross-Host Safety**: Detects host identity and handles API gaps dynamically (e.g., Premiere Pro's lack of SmartRender).
* **Byte Layout Swizzling**: Dynamically handles AE's standard **ARGB** vs. Premiere's standard **BGRA** interleaved pixel layouts.
* **Param Setup Fallbacks**: Configures parameters compatibly across hosts, avoiding features exclusive to After Effects.

### Key API Usages
* `ctx.is_premiere()`
* `visit_pixel_format` layout swizzle switches (`IsBGRA`)
* Parameter registration fallbacks.
