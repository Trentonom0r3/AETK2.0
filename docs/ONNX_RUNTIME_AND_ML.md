# ONNX Runtime (ORT) & Machine Learning in AETK 2.0

AETK 2.0 provides integration capabilities for executing machine learning models (such as YOLO, Depth Anything, or customized image models) inside After Effects plugins using the **ONNX Runtime (ORT)** C++ API.

---

## 1. Thread Isolation & Session Management

Running deep learning model inference inside After Effects requires careful session management due to concurrent rendering and host thread safety requirements:

* **Session Lifetime**: ONNX Runtime sessions (`Ort::Session`) are heavy resources. Do not recreate sessions per frame inside the render loop. Initialize and cache the session inside your plugin's lifecycle (e.g., during parameter validation or lazy-loaded on the first render call).
* **VRAM Spikes / Split Model Loading**: If your plugin uses a split pipeline (e.g., multiple sequential models), loading all model sessions into memory concurrently can spike VRAM or system RAM, causing host-side allocation crashes.
  > [!TIP]
  > **Mitigation Pattern**: Load one model stage, run inference, copy the output tensors to pinned CPU memory, unload/dispose of that session, and then load the second model session.

---

## 2. Dynamic Library Deployment

Because After Effects plugins (`.aex`) are loaded dynamically, external dependencies like `onnxruntime.dll` must be discoverable at load time.

* **Renaming and Bundling**: AETK's CMake configuration automatically renames and deploys the dependency binaries (`onnxruntime.dll -> onnxruntime_fsl_1.0.0.dll`) directly next to your plugin inside the After Effects plug-ins folder.
* **Delay-Loading Hook**: The framework utilizes a custom delay-load hook (`OrtDelayLoadHook.cpp`) to dynamically locate and load the specific renamed support DLLs from the plugin directory, preventing conflicts with other plugins using different versions of the ONNX Runtime.

---

## 3. Pixel-to-Tensor Pipeline

To feed image frames from a `smart_world` into an ONNX model, you must map the pixel channels to a model-compatible float tensor (usually planar NCHW format).

### Code Example: Planar Float Tensor Extraction

This helper method converts an interleaved ARGB/BGRA CPU world to a planar float buffer for ORT input:

```cpp
std::vector<float> extract_planar_rgb(const smart_world& world) {
    const int w = world.width();
    const int h = world.height();
    std::vector<float> planar_buffer(w * h * 3);
    
    float* r_plane = planar_buffer.data();
    float* g_plane = r_plane + (w * h);
    float* b_plane = g_plane + (w * h);

    // Dispatch format-agnostic reader
    visit_pixel_format(world.pixel_format(), world.is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
        using accessor = pixel_accessor<PixelT, IsBGRA>;
        using ChannelT = typename accessor::channel_type;
        
        auto view = world.tensor_view<ChannelT>();
        
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const PixelT* px = reinterpret_cast<const PixelT*>(&view(y, x, 0));
                
                // Read color normalized to [0.0f, 1.0f]
                core::color color = accessor::read(px);
                
                int index = y * w + x;
                r_plane[index] = static_cast<float>(color.red);
                g_plane[index] = static_cast<float>(color.green);
                b_plane[index] = static_cast<float>(color.blue);
            }
        }
    });

    return planar_buffer;
}
```

---

## 4. Running ORT Inference

Once the inputs are prepared, package them into an `Ort::Value` tensor and run the session:

```cpp
void run_model_inference(Ort::Session& session, std::vector<float>& input_data, int width, int height) {
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
    
    std::array<int64_t, 4> input_shape = { 1, 3, height, width };
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info,
        input_data.data(),
        input_data.size(),
        input_shape.data(),
        input_shape.size()
    );

    const char* input_names[] = { "images" };
    const char* output_names[] = { "output" };

    // Execute session inference
    auto output_tensors = session.Run(
        Ort::RunOptions{nullptr},
        input_names,
        &input_tensor,
        1,
        output_names,
        1
    );

    // Retrieve float output data
    float* raw_output = output_tensors.front().GetTensorMutableData<float>();
    // ... Process output values back into a smart_world ...
}
```

---

## 5. Case Study: SWARM Plugin Build & Setup

For a complete production implementation of ONNX Runtime inside an AETK effect plugin, refer to the [SWARM README](file:///d:/dev/Projects/Repos/AETK2.0/samples/Effect/SWARM/README.md). It outlines specific details on:
* How to specify ONNX Runtime search roots (`ORT_ROOT`).
* The `onnxruntime_fsl_1.0.0.dll` renaming convention for delay-loading.
* YOLO model configuration and exporting.
* Required bundle folder layout in the AE plugins directory.
