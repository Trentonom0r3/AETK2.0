# Multi-Frame Rendering (MFR) & Tensors in AETK 2.0

This guide documents the design patterns for Multi-Frame Rendering (MFR) safety and how to use AETK's PyTorch-mirrored multidimensional **Tensor** framework for high-performance image processing.

## 1. Multi-Frame Rendering (MFR) Safety

Multi-Frame Rendering (MFR) allows After Effects to render multiple frames of a composition concurrently across all available CPU cores.

### The Core Rules of MFR
* **No Global or Static State**: Because multiple render threads execute the plugin entry point concurrently, storing per-instance data (like current frames, settings, or processing states) in global or static variables will cause data races, visual corruption, or hard crashes.
* **RAII Sequence Data**: If your plugin must store persistent sequence data (such as cache keys or user choices), wrap it in an RAII struct and lock it during render commands using AETK's sequence data lock:
  ```cpp
  // Thread-safe access to instance-specific sequence data
  auto locked_data = lock_sequence_data<my_sequence_struct>(ctx);
  ```
* **Individual Parameter Checkouts**: The classic `params` array pointer is `NULL` during `PF_Cmd_SMART_RENDER` (accessing it directly causes crashes). You must check out parameters individually using stable keys:
  ```cpp
  float scale = ctx.float_val(param_id::scale);
  ```

---

## 2. The Tensor & Tensor View Framework

To support high-performance machine learning models, custom CUDA/DirectX/Metal kernels, and complex pixel operations without unnecessary copy overhead, AETK 2.0 provides a zero-overhead multidimensional tensor system.

### A. Architectural Separation
1. **`tensor<T, Rank, Device>` (Owning Resource Container)**
   * Manages the memory allocation lifecycle.
   * Tracks host allocations through the After Effects memory tracking API.
   * Integrates with After Effects' GPU suites (calling `AllocateHostMemory` and `AllocateDeviceMemory` via `PF_GPUDeviceSuite1`).
   * Clean move-only mechanics prevent double-free host buffer crashes.
2. **`tensor_view<T, Rank, Device>` (Non-Owning Metadata View)**
   * A lightweight metadata mapping structure containing shape dimensions and strides.
   * Compatible with CUDA device kernels (`__host__ __device__`) and can be passed directly into GPU shaders by value.
   * Strides are represented using signed **`ptrdiff_t`** (rather than unsigned `size_t`) to safely support negative row strides (vertically flipped inputs common in Premiere Pro/After Effects layouts) without unsigned overflow.

---

## 3. How to Use Tensors in Your Code

### A. Indexing a Checked-Out `smart_world`
You can acquire a non-owning CPU or GPU view from an existing world for pixel-level loops:
```cpp
// Defaults to CPU view
auto view = src.tensor_view<A_u_char>();

// Index channels: view(y, x, channel)
// (channel: 0=Alpha, 1=Red, 2=Green, 3=Blue in standard ARGB)
A_u_char alpha_val = view(10, 20, 0);
A_u_char red_val   = view(10, 20, 1);
```

### B. Allocation & Pinned DMA Transfer
You can allocate a pinned host-memory tensor and copy it directly to a CUDA device buffer:
```cpp
std::array<size_t, 3> shape = { 1080, 1920, 4 };

// Construct a pinned host memory tensor (tracks allocation through AE host)
auto t_pinned = aetk::effect::zeros_pinned<float, 3>(shape, ctx);

// Write to the view
auto v_pinned = t_pinned.view();
v_pinned(500, 500, 1) = 0.5f; // Modify green/red channel

// Perform high-speed DMA transfer to a GPU CUDA tensor
auto t_cuda = t_pinned.to<aetk::effect::device_kind::cuda>();
```

### C. Exporting Tensors to After Effects Worlds
Always use `.copy_to()` or `.to_world()`—these methods copy exactly `width * bytes_per_pixel` per row and advance using the signed `rowbytes` stride:
```cpp
// Export an ARGB float tensor back to a standard 8-bit After Effects world
smart_world world_dst = t_src.to_world(ctx, 8);
```

> [!CAUTION]
> **Sub-Reference ROI Corruption**:
> When writing back to a checked-out After Effects layer, do not copy the raw row bytes directly. If the layer is a Region of Interest (ROI) sub-reference, doing so will overwrite gutter padding or metadata, leading to memory corruption or adjacent frame corruption. Always rely on `.copy_to()` or `.to_world()`.

> [!WARNING]
> **Negative Strides & Pointer Overflows**:
> Premiere Pro and After Effects utilize negative `rowbytes` for vertically flipped frames. Ensure any custom pointer calculations use signed `ptrdiff_t` rather than unsigned `size_t` or `uint64_t` for stride values, as unsigned offsets will wrap around and trigger segmentation faults on 64-bit systems.
