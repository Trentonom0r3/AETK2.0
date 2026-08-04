#include <aetk/effect.hpp>
#include <aetk/effect/gpu.hpp>
#include <mutex>
#include <string>
#include <unordered_set>

#include <cuda_runtime.h>

// Thread-safe deduplicating logging helper
void log_unique(const std::string& msg) {
    static std::unordered_set<std::string> logged_messages;
    static std::mutex log_mutex;

    std::lock_guard<std::mutex> lock(log_mutex);
    if (logged_messages.find(msg) == logged_messages.end()) {
        logged_messages.insert(msg);
        AETK_LOG_INFO(msg);
    }
}

#include <aetk/effect/pixel/tensor_view.hpp>

extern void launch_invert_kernel_tensor_cuda(
    aetk::effect::tensor_view<float, 3, aetk::effect::device_kind::cuda> src_view,
    aetk::effect::tensor_view<float, 3, aetk::effect::device_kind::cuda> dst_view,
    float invert_ratio, cudaStream_t stream);

using namespace aetk::effect;

class AETK_GPUEffect : public plugin<AETK_GPUEffect> {
public:
    static void on_about(const context& ctx) {
        ctx.set_dialog_response(
            "AETK GPURender Test: Dual isolated CPU/GPU smart render paths.");
    }

    static void on_global_setup(const global_setup_context& ctx) {
        aetk::core::logger::instance().init("aetk_gpueffect_debug.log");
        aetk::core::logger::instance().set_level(aetk::core::log_level::debug);
        ctx.set_pipl_overrides();
        ctx.enable_smart_render();
        ctx.enable_threaded_rendering();
        ctx.enable_gpu_rendering();
    }

    static void on_gpu_device_setup(const gpu_device_setup_context& ctx) {
        auto* extra = ctx.get_gpu_extra();
        if (!extra)
            return;

        PF_GPU_Framework framework = extra->input->what_gpu;
        int32_t device_index = extra->input->device_index;

        if (framework == PF_GPU_Framework_CUDA) {
            ctx.enable_gpu_rendering();
            AETK_LOG_INFO("[AETK_GPUEffect] on_gpu_device_setup: CUDA support enabled.");
        } else if (framework == PF_GPU_Framework_DIRECTX) {
            AETK_LOG_INFO("[AETK_GPUEffect] on_gpu_device_setup: DirectX support enabled.");
        } else {
            // unknown
            AETK_LOG_INFO("[AETK_GPUEffect] on_gpu_device_setup: Unknown GPU framework: "
                + std::to_string(framework));
        }
    }

    static void on_gpu_device_setdown(const gpu_device_setdown_context& ctx) {
        AETK_LOG_INFO("Shutting down");
    }

    static void on_params_setup(const params_setup_context& ctx) {
        ctx.add_popup("Hardware Path", 3, 1, "CUDA (GPU)|DirectX (GPU)|CPU (Fallback)")
            .set_key("hardware_path");

        ctx.add_slider("Invert Ratio", 0.0f, 100.0f, 100.0f).set_key("invert_ratio");
    }

    static void on_pre_render(const pre_render_context& ctx) {
        ctx.enable_gpu_render();
        ctx.checkout_layer(0, 0);
    }

    static void on_smart_render(const smart_render_context& ctx) {
        // Isolated CPU smart rendering path
        auto src = ctx.checkout_pixels(0);
        auto dst = ctx.checkout_output();

        float ratio = static_cast<float>(ctx.float_val("invert_ratio")) / 100.0f;

        std::string format_str;
        PF_PixelFormat fmt = src.pixel_format();
        if (fmt == PF_PixelFormat_ARGB32)
            format_str = "8bpc ARGB";
        else if (fmt == PF_PixelFormat_ARGB64)
            format_str = "16bpc ARGB";
        else if (fmt == PF_PixelFormat_ARGB128)
            format_str = "32bpc float ARGB";
        else
            format_str = "Unknown Format";

        log_unique("[AETK_GPUEffect] called rendercpu: Source format=" + format_str
            + ", dimensions=" + std::to_string(src.width()) + "x"
            + std::to_string(src.height()) + ", location=CPU"
            + ", source is_gpu=" + std::string(src.is_gpu() ? "TRUE" : "FALSE")
            + ", dest is_gpu=" + std::string(dst.is_gpu() ? "TRUE" : "FALSE"));

        int32_t width = src.width();
        int32_t height = src.height();

        ctx.parallel_for(height, [&](int32_t y, int32_t thread_idx) {
            for (int32_t x = 0; x < width; ++x) {
                auto c = src.get_pixel<pixel_range::tkuint8>(x, y);
                c.red = c.red * (1.0f - ratio) + (255.0f - c.red) * ratio;
                c.green = c.green * (1.0f - ratio) + (255.0f - c.green) * ratio;
                c.blue = c.blue * (1.0f - ratio) + (255.0f - c.blue) * ratio;
                dst.set_pixel<pixel_range::tkuint8>(x, y, c);
            }
        });
    }

    static void on_smart_render_gpu(const smart_render_context& ctx) {
        // Isolated GPU smart rendering path
        auto src = ctx.checkout_pixels(0);
        auto dst = ctx.checkout_output();

        float ratio = static_cast<float>(ctx.float_val("invert_ratio")) / 100.0f;

        log_unique(
            "[AETK_GPUEffect] called rendergpu: dimensions=" + std::to_string(src.width())
            + "x" + std::to_string(src.height()) + ", location=GPU"
            + ", source is_gpu=" + std::string(src.is_gpu() ? "TRUE" : "FALSE")
            + ", dest is_gpu=" + std::string(dst.is_gpu() ? "TRUE" : "FALSE"));
        try {
            PF_GPUDeviceInfo device_info { };
            auto gpu_suite = ctx.get_suite<PF_GPUDeviceSuite1>(
                kPFGPUDeviceSuite, kPFGPUDeviceSuiteVersion1);
            if (gpu_suite->GetDeviceInfo(ctx.in_data_ptr()->effect_ref,
                    ctx.extra()->input->device_index, &device_info)
                == PF_Err_NONE) {
                cudaStream_t stream
                    = static_cast<cudaStream_t>(device_info.command_queuePV);

                // Construct CUDA tensors from smart_worlds
                tensor<float, 3, device_kind::cuda> src_tensor(std::move(src));
                tensor<float, 3, device_kind::cuda> dst_tensor(std::move(dst));

                if (src_tensor.data_ptr() && dst_tensor.data_ptr()) {
                    launch_invert_kernel_tensor_cuda(
                        src_tensor.view(), dst_tensor.view(), ratio, stream);
                    cudaStreamSynchronize(stream);
                } else {
                    log_unique("[AETK_GPUEffect] called rendergpu: Null GPU data "
                               "pointers in CUDA path.");
                }
            }
        } catch (const std::exception& e) {
            log_unique("[AETK_GPUEffect] called rendergpu: Exception in CUDA render "
                       "execution: "
                + std::string(e.what()));
        }
    }
};

AETK_EFFECT_MAIN(AETK_GPUEffect)
