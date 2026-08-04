# AETK 2.0: The Modern After Effects SDK

[![Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![License](https://img.shields.io/badge/license-AGPL--3.0-blue.svg)](LICENSE)
[![Static Analysis](https://img.shields.io/badge/Static%20Analysis-Clang--Tidy-orange.svg)](.clang-tidy)

**AETK 2.0** is a premium, header-only C++20 framework designed to strip away the archaic complexity of the Adobe After Effects SDK. It provides a type-safe, RAII-first abstraction layer that lets you focus on pixels and logic, not handle management and boilerplate.

# **A NOTE FROM THE DEV**

I started this project on the AEGP side a few years back because I wanted to create python bindings for AE. I've now worked on adapting this to the effect-plugin side, and have a pretty solid framework. Not everything is tested. The project has been, and will continue to, evolve rapidly, and as such- some of the documentation may appear outdated or incorrect.

- **A Note on Samples & Source of Truth**: All sample plugins are up-to-date and should compile/function without issue. If any of the documentation seems outdated, treat the samples and headers themselves as the source of truth. In a general sense, *SWARM* will be the best overall sample to study for how to use AETK in most use cases. Using ```AETK_START_DEBUG("name");``` in ```on_global_setup``` will provide a log file in your Documents folder to track errors and logs.
- **A Personal Note**: I'd still consider myself a "noob" developer, so there will almost certainly be decisions or code choices that appear suboptimal. While I've tried to optimize where I can (particularly surrounding pixel processing and manipulation), I'm sure there's room for improvement. Please feel free to critique and suggest changes; I'm always looking to learn and improve. This applies even more so to the documentation.

---

## Key Features

### Modern RAII & Type Safety

- **Zero-Cost Resource Management**: No more manual `PF_CHECKOUT_PARAM` or `host_new_handle`. All resources are scoped and automatically clean up on destruction.
- **Elegant Suite Management**: Deduce and acquire SDK suites type-safely via `aetk::core::suite<T>`.

### High-Performance Pixel Engine

- **`smart_world`**: A pitch-aware abstraction for `PF_EffectWorld` with built-in support for ARGB8, ARGB16, and ARGB32 (Float).
- **Multi-Dimensional Tensors**: Zero-overhead PyTorch-mirrored tensor pipeline (`tensor`, `tensor_view`) with negative stride support and direct CUDA interop.
- **Zero-Copy Intent**: Direct row-pointer arithmetic on pixel buffers for high-speed CPU filters.

### Interactive UI & Custom Widgets

- **Drawbot Wrapping**: Wraps the After Effects Drawbot suites to make it **SOMEWHAT** easier to implement custom UI.
- **Curves & Splines**: Fully interactive Spline editor widget supporting Catmull-Rom and Linear interpolation modes.
- **Host-Native Styling**: Custom UI buttons, sliders, and layouts with shadow rendering, vector graphics, and smooth hover animations.

---

## Educational Sample Plugins

AETK 2.0 includes a comprehensive set of sample plugins in `samples/` demonstrating framework features.

### Modernized Adobe AE SDK Sample Ports

- **[AETK_Skeleton](samples/Effect/AETK_Skeleton)**: A SmartFX, AETK 2.0 modernized version of the original Adobe After Effects SDK Skeleton sample, serving as a blank starting template.
- **[AETK_Checkout](samples/Effect/AETK_Checkout)**: A SmartFX, AETK 2.0 modernized version of the original Adobe SDK Checkout sample.
- **[AETK_Convolutrix](samples/Effect/AETK_Convolutrix)**: A SmartFX, AETK 2.0 modernized version of the original Adobe SDK Convolutrix sample.
- **[AETK_Supervisor](samples/Effect/AETK_Supervisor)**: A SmartFX, AETK 2.0 modernized version of the original Adobe SDK Supervisor sample.
- **[AETK_SmartyPants](samples/Effect/AETK_SmartyPants)**: A SmartFX, AETK 2.0 modernized version of the original Adobe SDK SmartyPants sample.
- **[AETK_Resizer](samples/Effect/AETK_Resizer)**: A SmartFX, AETK 2.0 modernized version of the original Adobe SDK Resizer sample.
- **[AETK_Sweetie](samples/AEGP/AETK_Sweetie)**: An AETK 2.0 modernized version of the original Adobe SDK Sweetie AEGP sample.

### Native AETK 2.0 Feature Demonstrators

- **[AETK_CustomUI](samples/Effect/AETK_CustomUI)**: Demonstrates AETK's Drawbot vector graphics, custom controls, and widget layout framework.
- **[AETK_CrossHost](samples/Effect/AETK_CrossHost)**: Demonstrates cross-host execution and suite handling between After Effects and Premiere Pro.
- **[AETK_GPUEffect](samples/Effect/AETK_GPUEffect)**: Demonstrates GPU acceleration pipelines using CUDA kernels.
- **[AETK_TestSuite](samples/Effect/AETK_TestSuite)**: Comprehensive feature validation and parameter suite test harness for AETK.
- **[Psychedelia](samples/Effect/Psychedelia)**: Real-time CUDA-accelerated volumetric fractal generator filter plugin.
- **[SWARM](samples/Effect/SWARM)**: Real-time object detection filter using YOLOX via ONNX Runtime (ORT), featuring CUDA GPU acceleration, DLL delay-load isolation, and a custom interactive HUD/joystick interface.

---

## Quick Start

### Prerequisites

- **CMake 3.20+**
- **Visual Studio 2022** (Windows) / **Xcode** (macOS)
- **After Effects SDK** (25.6 or higher)
- **CUDA Toolkit** (Optional, for GPU/CUDA acceleration features)

### Configuration

AETK requires the path to the After Effects SDK to configure correctly. You can supply this by setting the `AE_SDK_ROOT` environment variable or by passing it directly to CMake:

```bash
# Method A: Pass directly to CMake (Recommended)
cmake -B build -DAE_SDK_ROOT="C:/Path/To/AfterEffectsSDK"

# Method B: Environment Variable (Windows PowerShell)
$env:AE_SDK_ROOT = "C:/Path/To/AfterEffectsSDK"
cmake -B build
```

### Build

```bash
cmake --build build --config Release
```

---

## Documentation

- **[Getting Started Guide](docs/GETTING_STARTED.md)**: Step-by-step tutorial on environment configuration, project setup, writing format-agnostic loops, and host-debugging.
- **[Development Guide](docs/DEVELOPMENT.md)**: Architectural philosophy, coding standards, tensor/view pipeline guidelines, and the SDK troubleshooting/quirks log.
- **[Migration & Modernization Guide](docs/MIGRATION_GUIDE.md)**: Step-by-step instructions for porting legacy boilerplate-heavy After Effects SDK plugins to modern AETK 2.0 paradigms.
- **[Backward Compatibility & Versioning](docs/BACKWARD_COMPATIBILITY.md)**: Best practices for parameter migration and avoiding common SDK versioning pitfalls.
- **[Smart Worlds & Bit Depth](docs/SMART_WORLD_AND_BIT_DEPTH.md)**: Deeper dive into the `smart_world` RAII wrapper, layout swizzling, and format-agnostic rendering loops.
- **[Custom UI & Widget Framework](docs/CUSTOM_UI_AND_WIDGETS.md)**: Architecture of vector-based Custom UI controls, flexbox layout engine, and event routing.
- **[MFR Safety & Tensors](docs/MFR_AND_TENSORS.md)**: Guidelines on thread safety in Multi-Frame Rendering and AETK's zero-copy Tensor system.
- **[GPU & CUDA Rendering](docs/GPU_AND_CUDA_RENDERING.md)**: Setting up GPU/CUDA filters and executing device kernels safely.
- **[ONNX Runtime & ML Inference](docs/ONNX_RUNTIME_AND_ML.md)**: Thread-safe machine learning model execution, DLL delay-loading, and pixel-to-tensor pipelines.
- **[Licensing Framework & Custom Stubs](docs/LICENSING.md)**: Licensing architecture, default execution mode, `USE_AESCRIPTS_LIC` integration, and custom licensing implementations.
- **API Reference**: API structure generated via the project Doxyfile.

---

## Architecture Overview

| Component | Description |
| :--- | :--- |
| **`include/aetk`** | Core header-only library containing the framework wrapper classes. |
| **`samples/Effect`** | Sample plugins demonstrating parameter controls, custom UI drawing, GPU rendering, and ONNX runtime integration. |
| **`samples/AEGP`** | Sample AEGP plugins for automation and hook management. |
| **`cmake/`** | Centralized build rules, target properties, and automated PiPL resource generation tools. |
| **`docs/`** | Doxygen configuration files and static analysis setup. |

---

## License

AETK 2.0 is licensed under the **GNU Affero General Public License v3 (AGPL-3.0)**.

### Commercial Use & Dual Licensing

If you are interested in using AETK 2.0 in a closed-source or commercial product and want to avoid the copyleft requirements of the AGPL-3.0, please contact me via Email: <spigonvids@gmail.com> or [Discord](https://discord.com/users/728099522542043341) (.spigon) to discuss further options. Responses are typically faster via discord.

---
*Created by and for After Effects Developers.*
