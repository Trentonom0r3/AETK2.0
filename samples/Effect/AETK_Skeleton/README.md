# 💀 AETK_Skeleton Starter Template

`AETK_Skeleton` is a minimal, clean starter template for bootstrapping new After Effects effect plugins using AETK 2.0.

## 🛠️ Developer Reference

### How to Use this Template
1. **Copy folder**: Duplicate the `AETK_Skeleton` directory to your new plugin folder.
2. **Rename project**: Update `project(YourPluginName CXX)` and `aetk_add_effect(YourPluginName ...)` inside `CMakeLists.txt`.
3. **Register subdirectory**: Include `add_subdirectory(samples/Effect/YourPluginName)` in `cmake/AETK_Main.cmake`.
4. **Implement logic**: Update `on_params_setup` and `on_smart_render` in `main.cpp`.

### Default Configuration
* Threaded multi-frame rendering (MFR) enabled.
* SmartRender enabled (GPU and CPU fallback loops ready).
* Single-dependency header imports.
