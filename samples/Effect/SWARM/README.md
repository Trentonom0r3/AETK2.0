

## 🛠️ Build Requirements & Setup

To compile and run the SWARM sample, you must have the **ONNX Runtime (ORT) C++ SDK** installed on your system.

### 1. Specifying the ORT Path
By default, the `CMakeLists.txt` does not contain hardcoded paths. You must configure CMake to locate your ORT library by either:
- Setting the environment variable `ORT_ROOT` to point to the base directory of your ONNX Runtime build/release.
- Passing the `-DORT_ROOT=<path_to_ort>` parameter during the CMake generation command.

For example:
```bash
cmake -B build -S . -DAE_SDK_ROOT="C:/dev/AE_SDK" -DORT_ROOT="C:/dev/onnxruntime-win-x64-1.16.3"
```

### 2. DLL Renaming Convention
To prevent conflicts with other third-party After Effects plugins that might load different versions of ONNX Runtime, AETK compiles SWARM to use a dedicated delay-load hook. 
- The framework expects the ONNX Runtime DLL next to the plugin to be named **`onnxruntime_fsl_1.0.0.dll`**.
- CMake's post-build script automatically copies and renames `onnxruntime.dll` from your `ORT_ROOT` to this filename during build compilation.

---

## 🧠 Supported ML Models & Export Script

SWARM supports real-time inference using the **YOLOX** model architecture.

### 1. Specific Models Used
* **YOLOX Variants**:
  - `yolox_s.onnx` (Small, optimized for interactive speed)
  - `yolox_m.onnx` (Medium, balanced speed and accuracy)
  - `yolox_l.onnx` (Large, high-accuracy detection)

### 2. Model Exporting
To export raw PyTorch models into the optimized ONNX format compatible with the SWARM engine:
* Use the provided python export utility script [`export_yolox.py`](file:///d:/dev/Projects/Repos/AETK2.0/samples/Effect/SWARM/export_yolox.py) in the plugin directory.
* Run the script within your PyTorch environment to serialize YOLOX checkpoints into compatible input-shape `.onnx` files.

---

## 📦 Deployment Directory Structure

For the SWARM plugin to load, run inference, and draw custom HUD elements properly in After Effects, its bundle files **must** reside in the same directory. 

Ensure the following folder structure inside the After Effects `Plug-ins/` directory:
```
After Effects CC 2026/Plug-ins/SWARM/
├── SWARM.aex                      # The compiled plugin binary
├── models/                         # Folder containing yolox .onnx files
│   └── yolox_s.onnx
└── framesmith_core/                # Folder containing DLLs, presets, and HUD assets
    ├── onnxruntime_fsl_1.0.0.dll       # The renamed ONNX Runtime DLL
    ├── onnxruntime_providers_shared.dll# Shared providers DLL (if using CUDA/DirectML)
    ├── onnxruntime_providers_cuda.dll  # CUDA provider DLL (for GPU acceleration)
    └── DirectML.dll                    # DirectML DLL (for GPU acceleration)
```

> [!IMPORTANT]
> The `SWARM.aex` binary, the `models/` directory, and the `framesmith_core/` resource directory **must** be placed together in the same directory location, otherwise the plugin will fail to locate its models or HUD styling assets. Note that the dynamic library dependencies (`onnxruntime_fsl_1.0.0.dll`, etc.) are placed inside `framesmith_core/`.
