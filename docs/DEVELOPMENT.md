# AETK Development Guide & Knowledge Base

This document serves as the primary developer guide and knowledge repository for working on the AETK 2.0 codebase. Adherence to these guidelines ensures architectural consistency and plugin stability.

For a step-by-step introduction to setting up your workspace and building your first plugin, please refer to the [Getting Started Guide](GETTING_STARTED.md).

For topic-specific deep dives, please refer to:
* **[Architecture Overview](ARCHITECTURE_OVERVIEW.md)**: Conceptual namespaces, entry flow, and the RAII resource ownership model.
* **[Smart Worlds & Bit Depth](SMART_WORLD_AND_BIT_DEPTH.md)**: Resource lifetimes, bit-depth ranges, channel swizzling, and Premiere Pro compatibility fallbacks.
* **[MFR & Tensors](MFR_AND_TENSORS.md)**: Thread safety, Multi-Frame Rendering (MFR) rules, and the zero-overhead multi-dimensional tensor pipeline.
* **[GPU & CUDA Rendering](GPU_AND_CUDA_RENDERING.md)**: GPU plug-in configurations, static PiPL setups, CUDA kernel dispatches, and device memory allocations.
* **[Custom UI & Widgets](CUSTOM_UI_AND_WIDGETS.md)**: Flexbox widget system, event routing, Drawbot vector graphics, and event validation rules.
* **[Backward Compatibility](BACKWARD_COMPATIBILITY.md)**: Parameter disk ID hashing, version migrations, and upgrade safety rules.

## 1. Architectural Philosophy & Coding Standards
- **RAII First**: Every After Effects resource (Handles, Pixel Checkouts, Suites) MUST be wrapped in an RAII class. Never require manual "check-in" or "unlock". Assume we own the memory unless documented by Adobe that "the host will free this."
- **MFR Safety**: The library is designed for Multi-Frame Rendering (MFR). Never use global or static variables for per-instance data. Always use `lock_sequence_data<T>(ctx)` during render commands.
- **Zero-Copy Intent**: Avoid host-side pixel copies. Direct row-pointer arithmetic (as seen in `aetk::effect::world`) is often significantly faster for per-pixel operations in high-performance filters than suite calls, provided rowbytes are respected. When drawing line segments, restrict search bounds and avoid $O(W \times H)$ bounding box checks by utilizing 1D scanline band projections to ensure interactive UI speeds.
- **Educational Documentation**: Public-facing code MUST have Doxygen comments (`/** ... */`). Comments should help developers UNDERSTAND the underlying AE SDK logic. If an SDK quirk requires a workaround, explain *why* AE behaves that way. Aim to educate.
- **Naming & Namespace**: Use `snake_case` for classes/methods; `m_member_name` for private members. All code resides in `aetk::core`, `aetk::effect`, `aetk::aegp`, etc.
- **Smart Rendering**: The `params` array is NULL during `PF_Cmd_SMART_RENDER` (causing Error 513 or crashes if accessed directly). Parameters MUST be checked out individually. Use key-based lookups (`ctx.param<T>(key)` or `ctx.float_val(key)`) to handle this implicitly.
- **Macro Inclusion Order**: Including `Param_Utils.h` requires `AE_OS_WIN` to be defined on Windows to avoid calls to the Mac-only `strlcpy` inside `PF_STRNNCPY`. Always define platform guards at the top of the translation unit.
- **The "Magic Number"**: Always use a "magic" sentinel value in global and sequence data handles. Since the AE host treats everything as `void*`, the magic number check is the final line of defense against host-side data corruption or cross-plugin handle leaks.

## 2. API Guidelines

### 2.1 The Tensor & Tensor View Pipeline (`tensor`, `tensor_view`)
To facilitate seamless integration with high-performance machine learning models, custom CUDA/DirectX/Metal kernels, and complex image operations, AETK 2.0 introduces a zero-overhead PyTorch-mirrored multidimensional tensor framework.

#### Architectural Separation:
1. **`tensor<T, Rank, Dev>` (Owning Resource Container):**
   - Manages memory allocation lifecycles.
   - For CPU pinned (`cpu_pinned`) and GPU CUDA (`cuda`) allocations, it integrates with After Effects' memory tracking API by calling `AllocateHostMemory` and `AllocateDeviceMemory` through the `PF_GPUDeviceSuite1` suite.
   - Clean move-only mechanics prevent double-free host buffer crashes.
2. **`tensor_view<T, Rank, Dev>` (Non-Owning Metadata View):**
   - A lightweight view structure containing shape dimensions and signed strides (`ptrdiff_t`).
   - Strides use `ptrdiff_t` to safely support negative row strides (vertically flipped inputs common in Premiere Pro/After Effects layouts) without unsigned overflow.
   - Compatible with CUDA device kernels (`__host__ __device__`) and can be passed by value directly into compute shaders.

#### How and When to Use:
- **Use `tensor_view` when:** You want to index, manipulate, or sample channels of an existing After Effects pixel buffer (represented by `smart_world`) inside custom pixel loops or GPU kernels.
- **Use `tensor` when:** You need scratch space or an intermediate buffer (e.g. for ML model input tensors, planar image conversions, or offscreen rendering) whose lifecycle should be tied to the current scope.
- **Avoid:** Creating duplicate host-side pixel copies when direct row-pointer arithmetic on `tensor_view` is sufficient.

#### Code Examples:

##### 1. Indexing a `smart_world` via `tensor_view`
```cpp
// Acquire a non-owning CPU/GPU view from a smart_world
auto view = src.tensor_view<A_u_char>(); // Defaults to CPU view

// Index channels dynamically (e.g., y=10, x=20, channel=0 for Red/Alpha depending on layout)
A_u_char& channel_val = view(10, 20, 0);
```

##### 2. Allocate Pinned Host Tensor and Copy to CUDA Device
```cpp
std::array<size_t, 3> shape = { 1080, 1920, 4 };

// Construct a pinned host memory tensor (tracks allocation through AE host)
auto t_pinned = aetk::effect::zeros_pinned<float, 3>(shape, in_data->pica_basicP, in_data->effect_ref);

// Write to the view
auto v_pinned = t_pinned.view();
v_pinned(500, 500, 1) = 0.5f; // Modify green/red channel

// Perform high-speed DMA transfer to a GPU CUDA tensor
auto t_cuda = t_pinned.to<aetk::effect::device_kind::cuda>();
```

##### 3. Exporting Tensor Back to After Effects World
```cpp
// Export an ARGB float tensor back to a standard 8-bit After Effects world
smart_world world_dst = t_src.to_world(in_data, 8);
```

##### 4. Safety Considerations (Bounds-Safe Sub-Reference Copies)
When copying back to an After Effects output or checked-out world, never copy the raw row stride (`rowbytes`). Doing so will overwrite metadata or gutter padding in sub-referenced ROI (Region of Interest) worlds. Always use `.copy_to()` or `.to_world()`, which copies exactly `width * bytes_per_pixel` per row and advances using the signed `rowbytes` stride.

### 2.2 Format & Device Conversion (`smart_world::to`)
The `.to()` member function on `smart_world` is overloaded to handle two distinct kinds of data conversions:

1. **CPU Pixel Format Conversion (`to(PF_PixelFormat target_format) const`)**
   Converts the pixel representation of a CPU-resident world into a new `smart_world` with the requested bit-depth.
   - **Supported Target Formats**: `PF_PixelFormat_ARGB32` (8bpc), `PF_PixelFormat_ARGB64` (16bpc), or `PF_PixelFormat_ARGB128` (32bpc float).
   - **Range Normalization**: The method automatically normalizes the raw channel values between formats:
     - 8-bit range `0`...`255` is mapped to `0`...`32768` (16-bit white point) and `0.0f`...`1.0f` (32-bit float).
     - Standard 16-bit white point `32768` is mapped correctly (avoiding `65535` which would blow out whites in AE).
   - **Layout Handling**: It handles channel swizzling (ARGB vs BGRA) automatically depending on whether the source and destination are running in After Effects (ARGB) or Premiere Pro (BGRA).
   - **Example**:
     ```cpp
     // Convert a checked-out input world to a 32-bit float world for processing
     smart_world float_world = src.to(PF_PixelFormat_ARGB128);
     ```

2. **Device Residency Transfer (`to(device_kind target_device, SPBasicSuite* pica = nullptr, PF_ProgPtr effect_ref = nullptr) const`)**
   Moves the underlying image buffer between CPU memory and GPU device memory.
   - **CPU to GPU (upload)**: Call `src.to(device_kind::cuda)` to upload CPU pixels to a CUDA device buffer.
   - **GPU to CPU (download)**: Call `src.to(device_kind::cpu)` to download GPU pixels to CPU host memory.
   - **Example**:
     ```cpp
     // If pixels are on the GPU, copy them to CPU for fallback/inspection
     smart_world cpu_copy = gpu_world.to(device_kind::cpu);
     ```

### 2.3 Deep Copy Cloning (`smart_world::clone`)
Because After Effects pixel buffers and GPU worlds are heavy system resources, standard copy constructors and assignment operators are deleted (`= delete`) on `smart_world`. This prevents accidental, expensive copies and host-side memory leaks.

To explicitly duplicate a `smart_world` with its own independent memory lifecycle, call the `.clone()` method:
- **How it works**: It automatically allocates a fresh owning destination (`PF_EffectWorld` or GPU/Host buffer) matching the original's dimensions, format, and device residency, and performs a block pixel copy (using `PF_WorldTransformSuite1::copy` on CPU, `cudaMemcpy2D` on GPU, or `std::memcpy` on raw buffers).
- **Example**:
  ```cpp
  // Create an independent duplicate of the scratch world
  smart_world backup = original.clone();
  ```

### 2.4 Writing Generic, Format-Agnostic CPU Pixel Loops (`pixel_accessor`, `pixel_transaction`)
To avoid duplicating render logic or branching on bit-depth with `if/else` checks for every format, use the compile-time `pixel_accessor`, `pixel_transaction`, and `visit_pixel_format` double-dispatch template pattern.

- **`pixel_accessor<PixelT, IsBGRA>`**: A static utility specialized for `PF_Pixel8`, `PF_Pixel16`, and `PF_PixelFloat`.
  - **`pixel_accessor::channel_type`**: The underlying C++ channel type (`A_u_char`, `A_u_short`, `float`).
  - **`pixel_accessor::max_val`**: The format's white point (`255.0f`, `32768.0f`, `1.0f`).
  - **`pixel_accessor::read(const PixelT* px)`**: Reads a pixel and returns a normalized `aetk::core::color` (`[0.0, 1.0]` float ranges).
  - **`pixel_accessor::write(PixelT* px, const aetk::core::color& color)`**: Converts normalized floats back to the native integer/float range and writes it to the destination.
- **`pixel_transaction<PixelT, IsBGRA>`**: A scope-locked RAII container for transaction-safe pixel manipulation. On construction, it automatically reads the pixel from raw memory. On destruction, it automatically writes the color back to memory (only if `PixelT` is non-const).
- **`visit_pixel_format(format, is_bgra, lambda)`**: A helper that maps dynamic runtime format and layout enums to compile-time template parameters.

#### Code Example: Writing a generic color tint filter

```cpp
// 1. Write the generic, templated pixel processing logic using RAII transactions
template <typename PixelT, bool IsBGRA>
void apply_tint(const smart_world& src, smart_world& dst, float tint_r, float tint_g, float tint_b) {
    using accessor = aetk::effect::pixel_accessor<PixelT, IsBGRA>;
    using ChannelT = typename accessor::channel_type;
    
    // Obtain view of memory layout [H, W, 4]
    auto src_view = src.tensor_view<ChannelT>();
    auto dst_view = dst.tensor_view<ChannelT>();
    
    for (int y = 0; y < src.height(); ++y) {
        for (int x = 0; x < src.width(); ++x) {
            // Get pointer to raw pixel components
            const PixelT* src_px = reinterpret_cast<const PixelT*>(&src_view(y, x, 0));
            PixelT* dst_px = reinterpret_cast<PixelT*>(&dst_view(y, x, 0));
            
            // A single RAII transfer transaction (reads from src, commits to dst on destruction)
            aetk::effect::pixel_transaction<PixelT, IsBGRA> tx(src_px, dst_px);
            
            // Modify components uniformly in normalized double-precision color space
            tx.color.red   *= tint_r;
            tx.color.green *= tint_g;
            tx.color.blue  *= tint_b;
        } // tx goes out of scope and commits to dst_px automatically
    }
}

// 2. Dispatch from your on_render/on_smart_render entry point
void my_plugin::on_smart_render(smart_render_context& ctx) {
    auto src = ctx.checkout_input(0);
    auto dst = ctx.checkout_output();
    
    // Dynamically query layout & bit-depth and compile-time dispatch
    aetk::effect::visit_pixel_format(src.pixel_format(), src.is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
        apply_tint<PixelT, IsBGRA>(src, dst, 0.5f, 1.0f, 0.8f);
    });
}
```

## 3. Trouble-Shooting Log & Quirks
| Issue | Symptom | Nonsense Solution |
| :--- | :--- | :--- |
| **Handle Lock Failure** | Compilation error: `in_data` not found | `PF_LOCK_HANDLE` is a macro that literally uses the characters `in_data`. Fix: Access `(*(in_data)->utils->host_lock_handle)(h)` directly. |
| **SmartFX Rect Mismatch** | Output is cropped or shifted | `PF_LRect` (SmartFX) and `PF_Rect` (Classic) are NOT binary compatible. Use a converter utility when piping `checkout_result` into `output_request`. |
| **Logger Crash** | Garbage pointer crash in `m_pica` | `PF_InData` pointers are often stack-allocated by AE and become stale after `EffectMain` returns. Fix: Use a global/static context refreshed on every call instead of storing the pointer. |
| **Compute Cache Reprocess** | Cache does not evict explicitly | The generic compute cache struct does not provide an in-place overwrite API for the same key. Fix: include a hidden process nonce/version in the cache GUID and bump it whenever the user explicitly re-runs processing. |
| **Compute Cache Size Underreporting** | Compute cache entries never evict, or memory usage grows far beyond expectations for large cached structs | The generic `get_approx_size()` helper only sees inline struct size unless you account for nested heap members explicitly. Fix: specialize `compute_cache_traits<T>::approx_size(...)` and use `approx_size_heap_bytes(...)` / `approx_size_sum(...)` for vectors, strings, and other owned buffers. |
| **Split ONNX VRAM Spike** | A supposedly split inference path still peaks as though both models are resident | Splitting the `.onnx` file is not enough if helper code preloads both ORT sessions. Fix: load one model stage, run it, copy/keep only the CPU outputs needed for the next stage, unload that session, then load the next stage. |
| **Manual Cache GUID Dimension Drift** | Manual processing completes, but Smart Render still shows the "press process" overlay | The button path can render an upstream source world whose dimensions differ from the effect output/world dimensions used during Smart Render. Fix: compute the cache GUID from the same output-space dimensions in both button and render paths; store source dimensions only as payload metadata. |
| **InvalidateRect Event Validation** | After Effects error: internal verification failure. `PF_InvalidateRect can only be called during valid events.` | `PF_InvalidateRect` is prohibited during `PF_Event_DRAW`, `PF_Event_ADJUST_CURSOR` (hover moves), `PF_Event_MOUSE_EXITED` (hover exits), `PF_Event_KEYDOWN`, and teardowns. Fix: Whitelist `PF_InvalidateRect` calls strictly to `DO_CLICK`, `DRAG` (except last_time), and `IDLE` events; during other events, bypass the suite call and set `PF_EO_UPDATE_NOW` in `evt_out_flags` to safely request redraws. |
| **Premiere Pro Pixel Format Fallback** | Colorspace/depth glitches (garbage black-and-white layouts) on YUV or float timelines in Premiere Pro when `PF_WorldSuite2` is absent. | Fallback to querying pixel formats via `PF_PixelFormatSuite1` in `init_format()` and `iterate`/`iterate_pixels` when `PF_WorldSuite2` is unavailable. |
| **Premiere Pro Params Nullness** | `ctx.checkout_pixels()` crashes or fails under Premiere Pro due to null or incomplete parameter streams at render time. | In classic render fallback paths where `m_sr_extra` is null, perform dynamic parameter checkouts via `PF_CHECKOUT_PARAM` / `PF_CHECKIN_PARAM` automatically using the `smart_world::from_param_checkout` RAII container wrapper. |
| **`smart_world` Move Pointer Drift** | Accessing pixel data of moved `smart_world` instances causes memory corruption or invalid address errors. | Since classic parameter checkout binds the `PF_ParamDef` inside the `smart_world` container, ensure move constructor and assignment operators copy `m_owned_def` and re-point `m_world` to its own new interior address (`&m_owned_def.u.ld`). |
| **Classic Render CRTP Dispatch Miss** | `on_render` base fallback executes empty parent implementation instead of user overridden `on_smart_render` | Change `on_smart_render(smart_render_context(ctx))` to `T::on_smart_render(smart_render_context(ctx))` inside the CRTP `plugin<T>::on_render` base helper so compilation routes the virtual fallback method dispatch statically to the correct subclass. |
| **PiPL SmartFX & CUDA Setup** | Premiere Pro or After Effects fails to load the plugin, or GPU/CUDA render commands are not received. | SmartFX flags (e.g., `SUPPORTS_SMART_RENDER`) must be set BOTH dynamically in `on_global_setup` (via `ctx.enable_smart_render()`) and statically in the PiPL resource definition. Furthermore, `PF_OutFlag2_SUPPORTS_DIRECTX_RENDERING` is required for CUDA rendering in some versions of After Effects, so it must be kept in the PiPL/out flags (weird quirk). |
| **Premiere Windows Color Swizzle Mismatch** | Colors are swapped/inverted in Premiere Pro on Windows for 8-bit/16-bit depths despite BGRA swizzling being enabled. | This was a transient troubleshooting artifact. `PF_Pixel` is always ARGB in the SDK. Premiere Pro provides frames in BGRA format in memory on both Windows and macOS, so we must always swizzle to ARGB for proper integration with AETK. |
| **Obsolete PF_Field & PF_ChannelMask Constants** | Compilation error: identifier undeclared | Old or unofficial documentation mentions identifiers like `PF_Field_DISPLAY_PHASE_0` or `PF_ChannelMask_RGB`. Fix: Use standard `PF_Field_FRAME`/`UPPER`/`LOWER` constants and synthesize `RGB` via `RED | GREEN | BLUE` flags. |
| **Smart World Copying Bit-depth Constraints** | World-copying fails or results in black/corrupted frames | AE's native `WorldTransformSuite::copy` does not support copying between different bit depths. Fix: Use `smart_world::to(PF_PixelFormat)` to perform pixel-wise format conversions before copying. |
| **Buffer Expansion Invisible at 100% Scale** | Resized output border or shapes are not visible, appearing to have no effect at all. | If the layer is composition-sized and placed at center (100% scale), any expanded frame buffer pixels will extend beyond composition boundaries and be cropped by the viewer. Fix/Verification: Scale down the layer (e.g. to 50%) or translate it in the composition to reveal the expanded border. |
| **Premiere Pro WorldTransformSuite Absence** | Crash or exception in Premiere Pro when calling `copy_to` or `convolve_to` on `smart_world` | Premiere Pro does not support `PF_WorldTransformSuite1`. Fix: Wrap the suite checkout/calls in try-catch blocks and fall back to the host's legacy callbacks `m_in_data->utils->copy` or `m_in_data->utils->convolve`. |
| **GPUDeviceSuite on CPU World Verification Failure** | After Effects error (25::263): internal verification failure. `only gpu effect world can be operated with this suite.` | Calling `GetGPUWorldData` or other GPU suite functions on a CPU-backed world causes a hard crash/halt. Fix: Implement `is_gpu()` by querying the format via `PF_GetPixelFormat` (checking for `PF_PixelFormat_GPU_BGRA128`) and guard all GPUDeviceSuite calls with it. |
| **After Effects Disabling Plugin Instance** | Plugin stops rendering, does not receive events, or appears disabled after an exception. | Returning error codes like `PF_Err_INTERNAL_STRUCT_DAMAGED` or `A_Err_GENERIC` from entry point tells After Effects the instance is corrupted. Fix: Return `PF_Err_OUT_OF_MEMORY` instead; After Effects will discard only the current frame render and keep the plugin active. |
| **PWSH.exe Build Warning** | Build log shows: `'pwsh.exe' is not recognized as an internal or external command...` | This error is completely harmless and originates from shell-specific post-build events. It does not affect compilation, linking, or asset copying, and must be ignored. Do not waste time investigating it. |
| **Comp Camera 3D Flattening on Release** | Volumetric 3D render looks correct when dragging/rotating the 3D camera but turns completely flat when mouse is released. | Mouse release triggers a high-quality render cache miss. Using the rotated camera matrix as the unprojection reference makes the relative extrinsics $E = M_{curr} \cdot M_{ref}^{-1}$ Identity. Fix: set the reference matrix to the default unrotated camera matrix on cache misses to keep the coordinate frame stable. |
| **Comp Camera Matrix Lookup Failure** | Camera matrix is not queried, `cam.is_valid()` returns false, and camera moves do not update the renderer. | After Effects requires `PF_OutFlag2_I_USE_3D_CAMERA` to be set statically in the PiPL resource. Dynamic addition via `ctx.add_out_flags2` is ignored unless `PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS` is also set statically. Fix: update CMake PiPL `OUT_FLAGS2` to `0x2A801C0A` (adding `0x2`). |
| **Washed-out / Dark Splat Colors** | Splat rendering output looks lower quality, dark, and washed out compared to the Python interactive viewer. | The ML model predicts colors in linear RGB space, but 8/16bpc compositions operate in gamma-corrected sRGB space. Fix: convert final accumulated pixel colors from linear RGB to sRGB using standard IEC sRGB conversion at the end of the rasterizer. |
| **Negative Rowbytes Strides** | Pointer offsets wrap around or crash in 64-bit systems | Premiere Pro and AE sometimes use negative `rowbytes` for vertically flipped frames. Fix: Store strides as signed `ptrdiff_t` in `tensor_view` and use signed pointer arithmetic to avoid unsigned integer overflow. |
| **Sub-Reference ROI Corruption** | Overwritten metadata or adjacent timeline frames | Writing to a sub-referenced ROI world using full `rowbytes` copies overwrite gutter padding/metadata. Fix: Copy exactly `width * bytes_per_pixel` per row and step by signed `rowbytes` stride. |
| **GPU Suite Memory Without Exclusive Access** | `AllocateHostMemory` or `AllocateDeviceMemory` returns `PF_Err_OUT_OF_MEMORY` even when a valid `PF_ProgPtr` and `SPBasicSuite` are supplied. | The SDK docs state: *"All calls below [AcquireExclusiveDeviceAccess] generally require access be held."* Full GPU-entry-point plugins hold this implicitly; all other contexts (e.g. SmartFX CPU render path, idle events) must call `AcquireExclusiveDeviceAccess` before and `ReleaseExclusiveDeviceAccess` after every `Allocate*/Free*` call. Always release even if the allocation fails to avoid deadlocking the device. |
| **`tensor` `copy_to` ARGB Channel Order** | Exported `smart_world` colors are shifted by one channel (e.g. Alpha appears as Red, Red appears as Green). | AE's interleaved pixel layout is ARGB: channel index 0 = Alpha, 1 = Red, 2 = Green, 3 = Blue. The `tensor::copy_to` loop must map `view(y,x,0)` → `c.alpha`, not `c.red`. This matches the stride ordering produced by `smart_world::tensor_view()`. |
| **Pixel Range Setting (tkfloat vs tkuint8)** | Color, drawing, and accessor mismatch errors | Templated `color<Range>` and `pixel_accessor<PixelT, IsBGRA, Range>` to support both float (`0..1`) and uint8 (`0..255`) ranges. UI and canvas systems operate on `tkfloat`. |
| **No-Op Format Conversion Copy Bypass** | Extra heap allocation and pixel copy overhead when converting a world to its own format | Added `clone_view()` method to construct a non-owning metadata reference (`ownership::NONE`), allowing `smart_world::to` to bypass allocations and copies when the target format matches the source. |
| **Bitwise Scaling Conversions** | Slow float division/multiplication when converting pixel depths | Optimized CPU-path pixel depth scaling using integer bitwise shifts and arithmetic: 8 -> 16 via `(x << 7) + (x >> 1) + (x >> 7)` and 16 -> 8 via `((x << 8) - x) >> 15`. |
| **Accidental World Copies / Destructor Crashes** | Memory corruption, double-free crashes, or compilation errors when trying to copy a `smart_world`. | Standard copy constructor and assignment operator are deleted (`= delete`). Explicit deep copying is supported via the `.clone()` method, which allocates a new independent owning buffer and copies the data. |
| **No-Op Copy Bypass Memory Leaks / Crashes** | Memory leaks or double-free crashes when returning no-op converted worlds. | Restrict format copy-bypass in `to(PF_PixelFormat)` strictly to non-owning views (`ownership::NONE`). Returning non-owning views of owning scratch worlds leads to lifetime dangling bugs or double-frees upon destruction. |
| **Restricted Parameter Index Lookups** | Passing raw integers (index) to `param` or `_val` helpers causes compilation failure. | The public API now strictly enforces key-based lookups (enums or strings) via the `is_param_key` trait to avoid parameter index drifts. Use `ctx.input_param()` for the default input layer (index 0), and define scoped/anonymous enums or string keys for custom parameters. |
