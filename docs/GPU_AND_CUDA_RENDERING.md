# GPU & CUDA Rendering in AETK 2.0

This guide documents the setup, execution, and safety guidelines for GPU-accelerated rendering using **CUDA** in the **AETK 2.0** framework.

---

## 1. Plugin Configuration for GPU Rendering

To enable GPU rendering in After Effects or Premiere Pro, you must configure the capabilities dynamically during setup and statically in the PiPL resource file.

### A. Dynamic Registration
During global and device setup callbacks, enable GPU rendering:
```cpp
static void on_global_setup(const global_setup_context& ctx) {
    ctx.enable_smart_render();
    ctx.enable_gpu_rendering();
}

static void on_gpu_device_setup(const gpu_device_setup_context& ctx) {
    if (ctx.has_framework(PF_GPU_Framework_CUDA)) {
        ctx.enable_gpu_rendering();
    }
}
```

### B. Static PiPL Flags
* **After Effects Quirks**: After Effects versioning requires `PF_OutFlag2_SUPPORTS_DIRECTX_RENDERING` to be enabled inside your static PiPL resource to route rendering commands to the CUDA device queue, even if you are only executing pure CUDA/OptiX kernels. AETK's build system handles this mapping in `plugin_config.h`.

---

## 2. GPU vs. CPU World Safety

> [!CAUTION]
> **GPUDeviceSuite on CPU World Verification Failure (Error 25::263)**: 
> Calling `GetGPUWorldData` or other GPU suite functions on a CPU-backed world causes a hard crash/halt by the After Effects host. 

### Best Practices for Safety
1. **Query Format first**: Always check if a world is GPU-backed before executing GPU suite calls. 
2. **Use AETK `is_gpu()` wrapper**: AETK provides a safe `is_gpu()` method on `smart_world` that queries the pixel format (checking for `PF_PixelFormat_GPU_BGRA128`) without invoking unsafe suite routines.
   ```cpp
   if (src.is_gpu() && dst.is_gpu()) {
       // Safe to execute CUDA kernels
       cudaStream_t stream = (cudaStream_t)info.command_queuePV;
       render_gpu(ctx, src, dst, stream);
   } else {
       // Fallback to CPU row-pointer processing
       render_cpu(src, dst);
   }
   ```

---

## 3. Launching CUDA Shaders

In AETK, when rendering on the GPU, you receive a command queue pointer (`command_queuePV`) from the active GPU Device Info. In CUDA setups, this corresponds directly to a `cudaStream_t`.

### Code Pattern: GPU Shading Entry Point
Inside your dynamic render dispatch:
```cpp
static void on_smart_render(const smart_render_context& ctx, bool is_gpu) {
    auto src = ctx.checkout_pixels(0);
    auto dst = ctx.checkout_output();

    if (src.is_gpu() && dst.is_gpu()) {
        auto gpusuite = gpu_device_suite(ctx.in_data_ptr());
        PF_GPUDeviceInfo info = gpusuite.device_info(ctx.extra()->input->device_index);

        if (info.device_framework == PF_GPU_Framework_CUDA) {
            try {
                // Cast command queue to CUDA stream
                cudaStream_t stream = static_cast<cudaStream_t>(info.command_queuePV);
                
                // Get raw device pointers
                const void* src_dev_ptr = src.gpu_data();
                void* dst_dev_ptr = dst.gpu_data();

                // Launch custom CUDA kernel on the host-provided stream
                launch_my_kernel(src_dev_ptr, dst_dev_ptr, src.width(), src.height(), stream);
                return;
            } catch (const std::exception& e) {
                AETK_LOG_ERR("GPU rendering failed: {}, falling back to CPU", e.what());
            }
        }
    }

    // CPU Fallback
    render_cpu(src, dst);
}
```
* **Note**: In CUDA kernels, always use signed offsets (`ptrdiff_t`) for calculating pitch/strides to support vertical coordinate flips.

> [!WARNING]
> **GPU Suite Memory Exclusive Access**:
> The SDK GPU device suites require exclusive device access to track allocations. Calling functions like `AllocateHostMemory` or `AllocateDeviceMemory` outside a dedicated GPU entry-point render callback (for instance, during CPU render fallbacks or event handlers) can return `PF_Err_OUT_OF_MEMORY` or fail.
> 
> You must wrap your allocations by calling `AcquireExclusiveDeviceAccess` before allocating and `ReleaseExclusiveDeviceAccess` immediately afterward. Be sure to release the access token even if the allocation fails, to avoid deadlocking the graphics device.

