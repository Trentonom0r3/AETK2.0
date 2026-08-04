# Getting Started with AETK 2.0

Welcome to **AETK 2.0**! This guide will walk you through setting up your environment, creating your first After Effects plugin, processing pixels using our RAII container system, and debugging it in real time.

---

## Table of Contents
1. [Prerequisites & Environment Setup](#1-prerequisites--environment-setup)
2. [Project Directory & CMake Structure](#2-project-directory--cmake-structure)
3. [Anatomy of an AETK Plugin](#3-anatomy-of-an-aetk-plugin)
4. [Writing a Format-Agnostic Image Filter](#4-writing-a-format-agnostic-image-filter)
5. [Building & Deploying](#5-building--deploying)
6. [Debugging Your Plugin](#6-debugging-your-plugin)

---

## 1. Prerequisites & Environment Setup

Before starting, ensure you have the following installed on your machine:
*   **C++20 Compiler**: Visual Studio 2022 (Windows) or Xcode 15+ (macOS).
*   **CMake**: Version 3.20 or higher.
*   **After Effects SDK**: Adobe's official SDK (version 25.6 or higher recommended).
*   **After Effects**: Installed on the local machine for testing.

### Setting Up the environment
AETK requires knowing the path to the After Effects SDK. Define the `AE_SDK_ROOT` environment variable or specify it during CMake configuration.

**On Windows (PowerShell):**
```powershell
$env:AE_SDK_ROOT = "C:/Path/To/Adobe_After_Effects_SDK"
```

**On macOS (Terminal):**
```bash
export AE_SDK_ROOT="/Path/To/Adobe_After_Effects_SDK"
```

---

## 2. Project Directory & CMake Structure

A typical project directory layout looks like this:
```text
my_plugin/
├── CMakeLists.txt
├── main.cpp
└── my_pluginPiPL.r (Optional - AETK can generate PiPLs automatically!)
```

### Writing the `CMakeLists.txt`
AETK integrates seamlessly with CMake. Here is a simple configuration to build your plugin:

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyCustomPlugin)

# Find and configure AETK
find_package(AETK REQUIRED)

# Define your plugin target
add_aetk_plugin(MyCustomPlugin
    SOURCES 
        main.cpp
)

# AETK automatically handles:
# 1. Including After Effects SDK headers
# 2. Generating the PiPL (.r) resource file
# 3. Setting C++20 standard compile options
```

---

## 3. Anatomy of an AETK Plugin

AETK wraps the procedural, callback-heavy C entry point (`EffectMain`) in a modern C++ object-oriented wrapper. Create your plugin by inheriting from `aetk::effect::plugin<T>`.

Here is the baseline skeleton for `main.cpp`:

```cpp
#include <aetk/effect.hpp>

using namespace aetk::effect;

class my_custom_plugin : public plugin<my_custom_plugin> {
public:
    // 1. About Dialog box (triggered when users click "About" in Effect Controls)
    static void on_about(const context& ctx) {
        ctx.set_dialog_response("My Custom Plugin v1.0\nCreated using AETK 2.0.");
    }

    // 2. Global Setup (runs once when the plugin is loaded)
    static void on_global_setup(const global_setup_context& ctx) {
        ctx.set_pipl_overrides();
        ctx.enable_smart_render();
        ctx.enable_threaded_rendering(); // Enable multi-threaded execution
    }

    // 3. Registering Parameters (sliders, checkboxes, colors, etc.)
    static void on_params_setup(const params_setup_context& ctx) {
        ctx.add_slider("Brightness", 0.0f, 2.0f, 1.0f); // Label, Min, Max, Default
    }

    // 4. Pre-Render (specifies composition request bounds and bit depth)
    static void on_pre_render(const pre_render_context& ctx) {
        ctx.checkout_layer(0, 0); // Check out input layer 0
    }

    // 5. Smart Render (called when AE requests a rendered frame)
    static void on_smart_render(const smart_render_context& ctx) {
        auto src = ctx.checkout_pixels(0); // Input frame wrapper
        auto dst = ctx.checkout_output();    // Output frame wrapper
        
        // Pass-through fallback
        src.copy_to(dst);
    }
};

// Macro that hooks up the C-style Entry Point for After Effects
AETK_EFFECT_MAIN(my_custom_plugin)
```

---

## 4. Writing a Format-Agnostic Image Filter

In After Effects, your plugin must support 8-bit, 16-bit, and 32-bit float timelines. Standard SDK code requires writing three distinct loops.

AETK handles this compile-time dispatching using:
*   **`visit_pixel_format`**: Maps runtime pixel bit-depths and channel orders to compile-time template parameters.
*   **`pixel_transaction`**: An RAII scope-locked container that reads a pixel's colors into a normalized `[0.0, 1.0]` floating-point range on creation and automatically commits changes back to raw memory on destruction.

Here is a complete, thread-safe pixel inversion filter:

```cpp
#include <aetk/effect.hpp>

using namespace aetk::effect;

enum class param_keys {
    brightness = 1
};

// Generic template logic (compiled once for every format/layout)
template <typename PixelT, bool IsBGRA>
void invert_effect(const smart_world& src, smart_world& dst, float brightness) {
    using accessor = pixel_accessor<PixelT, IsBGRA>;
    using ChannelT = typename accessor::channel_type;

    // Obtain HWC views of source and destination pixel grids
    auto src_view = src.tensor_view<ChannelT>();
    auto dst_view = dst.tensor_view<ChannelT>();

    for (int y = 0; y < src.height(); ++y) {
        for (int x = 0; x < src.width(); ++x) {
            // Get raw pointer offsets
            const PixelT* src_px = reinterpret_cast<const PixelT*>(&src_view(y, x, 0));
            PixelT* dst_px = reinterpret_cast<PixelT*>(&dst_view(y, x, 0));

            // Single-line RAII transaction. Reads src_px and commits to dst_px on scope exit
            pixel_transaction<PixelT, IsBGRA> tx(src_px, dst_px);

            // Perform math on normalized double-precision floats [0.0, 1.0]
            tx.color.red   = (1.0 - tx.color.red)   * brightness;
            tx.color.green = (1.0 - tx.color.green) * brightness;
            tx.color.blue  = (1.0 - tx.color.blue)  * brightness;
            // tx.color.alpha remains unmodified
        }
    }
}

class inverter_plugin : public plugin<inverter_plugin> {
public:
    static void on_about(const context& ctx) {
        ctx.set_dialog_response("RAII Inversion Filter.");
    }

    static void on_global_setup(const global_setup_context& ctx) {
        ctx.set_pipl_overrides();
        ctx.enable_smart_render();
        ctx.enable_threaded_rendering();
    }

    static void on_params_setup(const params_setup_context& ctx) {
        ctx.add_slider("Brightness Modifier", 0.0f, 2.0f, 1.0f)
           .set_key(param_keys::brightness);
    }

    static void on_pre_render(const pre_render_context& ctx) {
        ctx.checkout_layer(0, 0);
    }

    static void on_smart_render(const smart_render_context& ctx) {
        auto src = ctx.checkout_pixels();
        auto dst = ctx.checkout_output();
        
        // Checkout parameter value safely using enum key
        float brightness = ctx.float_val(param_keys::brightness);

        // Compile-time double dispatch based on format & layout
        visit_pixel_format(src.pixel_format(), src.is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
            invert_effect<PixelT, IsBGRA>(src, dst, brightness);
        });
    }
};

AETK_EFFECT_MAIN(inverter_plugin)
```

### Detailed Code Breakdown

Let's walk through the design of this plugin to understand how AETK simplifies classic After Effects SDK complexity:

#### 1. Stable Parameter Keys (`param_keys`)
In the classic SDK, parameters are referenced by raw layout index (e.g. `params[1]`). If you add or reorder parameters, these indices shift, which requires updating all rendering lookups. 
In AETK, we define an enum:
```cpp
enum class param_keys {
    brightness = 1
};
```
We assign this key during setup (`.set_key(param_keys::brightness)`), and query it at render time (`ctx.float_val(param_keys::brightness)`). AETK resolves the current index dynamically, allowing safe parameter UI reordering without modifying the render logic.

#### 2. Format-Agnostic Processing Template (`invert_effect`)
An After Effects composition can render in 8-bit integer, 16-bit integer, or 32-bit float channels. Rather than writing three duplicate, bug-prone loop structures, AETK exposes:
* **`visit_pixel_format`**: Standard double-dispatch template that accepts runtime format and layout, and calls our template function specialized for the compile-time types `PixelT` and `IsBGRA`.
* **`pixel_accessor<PixelT, IsBGRA>`**: Provides helper types like `channel_type` (resolving to `A_u_char`, `A_u_short`, or `float`) and coordinates channel offsets transparently regardless of host layout (ARGB vs BGRA).
* **`tensor_view`**: Provides high-speed, zero-copy indexing views `view(y, x, channel)` mapping directly to row-aligned pixels.
* **`pixel_transaction<PixelT, IsBGRA>`**: An RAII wrapper that reads the pixel at construction, normalizes its color channels to the float range `[0.0, 1.0]`, and commits any changes back to target memory at destruction.

#### 3. Thread-Safe Render Context (`on_smart_render`)
Under Multi-Frame Rendering (MFR), the host renders multiple frames concurrently. Using static/global variables for state storage will crash the host.
The stack-allocated `smart_render_context` keeps all parameters and checked-out worlds local to the thread's call frame. The destructors of the `smart_world` variables `src` and `dst` automatically check themselves back into the After Effects compositing cache, eliminating the need to call manual `checkin` routines.

---

## 5. Building & Deploying

To build your plugin, configure CMake and target the build directory:

### On Windows
```bash
# 1. Configure Visual Studio 2022 solution
cmake -B build -S .

# 2. Build the Release bundle
cmake --build build --config Release
```
This generates a `MyCustomPlugin.aex` file in the build output. Copy this file into your After Effects plug-ins directory:
`C:\Program Files\Adobe\Adobe After Effects <version>\Support Files\Plug-ins\`

### On macOS
```bash
# 1. Configure Xcode project
cmake -B build -G Xcode -S .

# 2. Build the Release bundle
cmake --build build --config Release
```
Copy the resulting `.plugin` package bundle into:
`/Applications/Adobe After Effects <version>/Plug-ins/`

---

## 6. Debugging Your Plugin

Debugging custom host plug-ins requires attaching your IDE's debugger to the host process while it is running.

### Windows (Visual Studio)
1. Configure and build a **Debug** version of the plugin (`cmake --build build --config Debug`).
2. Copy the resulting `.aex` and its `.pdb` symbol files to the AE plug-ins directory.
3. Open Visual Studio.
4. Go to **Debug -> Attach to Process...** (or press `Ctrl+Alt+P`).
5. Select **AfterFX.exe** from the running processes list.
6. Set breakpoints in your source code (e.g. inside `on_smart_render`).
7. Open After Effects, apply the plugin to a layer, and hit your breakpoints!

### macOS (Xcode)
1. Build a **Debug** configuration.
2. Place the debug `.plugin` bundle inside the plug-ins directory.
3. Open Xcode.
4. Select **Debug -> Attach to Process** and choose **Adobe After Effects**.
5. Set breakpoints in Xcode.
6. Interact with the plugin inside After Effects to trigger breakpoints.

---

## 📖 Learn More
*   **[Development Guide](DEVELOPMENT.md)**: Deep dive into zero-overhead tensors, swizzle layouts, and the After Effects SDK quirks logs.
*   **[Migration Guide](MIGRATION_GUIDE.md)**: Step-by-step tutorial for upgrading old procedural codebases to AETK 2.0.
