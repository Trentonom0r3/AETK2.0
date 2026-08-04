# Smart Worlds & Bit Depth in AETK 2.0

This guide documents how **AETK 2.0** wraps heavy After Effects pixel resources using the `smart_world` class, and how it handles project bit depth and channel swizzling across different timelines automatically.

---

## 1. Unified Resource Model: `smart_world`

In raw After Effects development, managing image buffers is complex and error-prone because developers must call different host callbacks (`PF_NewWorld`, `PF_DisposeWorld`, `checkout_layer_pixels`, `checkin_layer_pixels`, `CreateGPUWorld`, `DisposeGPUWorld`) depending on the context.

AETK wraps all CPU, GPU, output, and layer checked-out buffers under the unified **`aetk::effect::smart_world`** class.

### A. Ownership Policies

A `smart_world` determines how it was created and selects the correct cleanup callback at scope exit via the `ownership` enum:

* **`ownership::NONE`**: Non-owning view (e.g. wrapping the default render output buffer). No cleanup occurs on destruction.
* **`ownership::LAYER_PIXELS`**: SmartRender callback pixel checkout. The destructor automatically checks the pixels back in using `checkin_layer_pixels()`.
* **`ownership::LAYER_PARAM_CHECKOUT`**: Classic render checkout. The destructor automatically calls `PF_CHECKIN_PARAM()`.
* **`ownership::SCRATCH_CPU`**: CPU scratch buffer. Automatically freed using `PF_DisposeWorld()`.
* **`ownership::SCRATCH_GPU`**: GPU CUDA device buffer. Automatically freed using `DisposeGPUWorld()`.

### B. Move-Only Semantics

Standard copy constructors and assignment operators are deleted (`= delete`). This ensures image buffers are never duplicated accidentally (which would cause double-free host crashes). If you need an independent duplicate, call:

```cpp
smart_world duplicate = original.clone();
```

> [!WARNING]
> **smart_world Move Pointer Drift**:
> Since classic parameter checkouts store the `PF_ParamDef` inside the `smart_world` container, moving a `smart_world` changes the memory address of this internal struct. AETK's custom move constructor and assignment operators prevent pointer drift by explicitly copying `m_owned_def` and re-pointing `m_world` to its own new interior address (`&m_owned_def.u.ld`). If you extend or modify `smart_world`, ensure that moved instances update their pointer references to avoid memory corruption or invalid address crashes.

### C. Device Residency Transfer

You can easily upload/download the world's storage between device and host memory using the `.to()` method:

```cpp
smart_world gpu_world = src.to(device_kind::cuda); // Upload to GPU
smart_world cpu_world = gpu_world.to(device_kind::cpu); // Download to CPU
```

---

## 2. Bit Depth & Color Normalization

After Effects and Premiere Pro use different native channel ranges and byte layouts depending on the active sequence/composition settings:

1. **8-Bit per Channel (`PF_PixelFormat_ARGB32` / `BGRA32`)**:
   * Channel type: `A_u_char` (`uint8_t`), range `0` to `255` (white point is `255`).
2. **16-Bit per Channel (`PF_PixelFormat_ARGB64`)**:
   * Channel type: `A_u_short` (`uint16_t`), range `0` to `32768` (white point is `32768`).
3. **32-Bit float per Channel (`PF_PixelFormat_ARGB128` / `GPU_BGRA128`)**:
   * Channel type: `float`, range `0.0f` to `1.0f` (supports HDR values exceeding `1.0f`).

---

## 3. Writing Format-Agnostic Code

AETK provides tools to write a single C++ pixel processing loop that runs natively across all bit depths and host layouts (such as AE's **ARGB** vs. Premiere's **BGRA**):

### A. Dynamic-to-Compile-Time Dispatch (`visit_pixel_format`)

Use `visit_pixel_format` to bridge dynamic runtime format checks to compile-time template parameters:

```cpp
visit_pixel_format(src.pixel_format(), src.is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
    // This lambda compiles separate instances for PF_Pixel8, PF_Pixel16, and PF_PixelFloat
    run_my_filter<PixelT, IsBGRA>(src, dst);
});
```

### B. Normalized Access via `pixel_accessor` & `pixel_transaction`

* **`pixel_accessor`**: Reads/writes values to the correct channels while converting them to/from normalized `[0.0, 1.0]` float ranges (`aetk::core::color<>`).
* **`pixel_transaction`**: An RAII wrapper that handles color conversion automatically. On construction, it reads and normalizes the source pixel. On destruction, it converts the color back to the target format and writes it.

#### Code Example: Generic Tint Filter

```cpp
template <typename PixelT, bool IsBGRA>
void apply_tint(const smart_world& src, smart_world& dst, float tint_r, float tint_g, float tint_b) {
    using accessor = aetk::effect::pixel_accessor<PixelT, IsBGRA>;
    using ChannelT = typename accessor::channel_type;
    
    auto src_view = src.tensor_view<ChannelT>();
    auto dst_view = dst.tensor_view<ChannelT>();
    
    for (int y = 0; y < src.height(); ++y) {
        for (int x = 0; x < src.width(); ++x) {
            const PixelT* src_px = reinterpret_cast<const PixelT*>(&src_view(y, x, 0));
            PixelT* dst_px = reinterpret_cast<PixelT*>(&dst_view(y, x, 0));
            
            // 1. Transaction constructor reads and converts src_px
            pixel_transaction<PixelT, IsBGRA> tx(src_px, dst_px);
            
            // 2. Manipulate in normalized float color space
            tx.color.red   *= tint_r;
            tx.color.green *= tint_g;
            tx.color.blue  *= tint_b;
            
            // 3. tx destructor automatically converts & commits to dst_px
        }
    }
}
```

---

## 4. Premiere Pro Compatibility & Fallbacks

Premiere Pro hosts the After Effects SDK differently than After Effects itself, introducing unique limitations:

### A. Parameter Nullness on Checkout

Under Premiere Pro, the classic render parameter pointer streams may return `NULL` or be incomplete during render queries.

* **AETK Design Solution**: Always use `ctx.checkout_pixels()` or checkouts managed by the `smart_world::from_param_checkout` RAII container. This container safely manages parameter checking and handles missing/null values gracefully.

### B. WorldTransformSuite Absence

Premiere Pro does not support `PF_WorldTransformSuite1` (which is used in After Effects for functions like `copy_to` or `convolve_to`).

* **AETK Design Solution**: AETK wraps these operations in try-catch guards. If the suite is unavailable, it automatically falls back to the host's legacy C-style callbacks `m_in_data->utils->copy` or `m_in_data->utils->convolve`.

### C. Pixel Format Fallback

On timeline YUV/float frames in Premiere Pro, `PF_WorldSuite2` might be absent. AETK implements format detection by falling back to `PF_PixelFormatSuite1` to query pixel configurations and maps layouts appropriately.
