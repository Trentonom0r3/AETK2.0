# ⚡ AETK_GPUEffect Sample Plugin

`AETK_GPUEffect` is a developer sample demonstrating cross-platform GPU-accelerated rendering inside After Effects.

## 🛠️ Developer Reference

### Core Concepts Demonstrated
* **Direct3D Shader Compilation**: Compiles HLSL vertex and pixel shaders dynamically into C++ headers.
* **SmartRender GPU Pipeline**: Checks out GPU-allocated device memory blocks without host round-trip copy delays.
* **Safe Fallbacks**: Falls back to CPU rendering if direct device rendering suites are missing or fail.

### Key API Usages
* `ctx.enable_gpu_rendering()`
* `gpu_device_suite` device details query.
* HLSL compiler macro pipelines.
