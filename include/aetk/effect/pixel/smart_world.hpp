#include <AE_Macros.h>
#pragma once

#include <AE_Effect.h>
#include <AE_EffectCB.h>
#include <AE_EffectPixelFormat.h>
#include <AE_EffectGPUSuites.h>
#include <aetk/aegp/dom/world.hpp>
#include <aetk/core/diagnostics.hpp>
#include <aetk/core/error.hpp>
#include <aetk/core/kernel.hpp>
#include <aetk/core/suite.hpp>
#include <aetk/core/types.hpp>
#include <aetk/effect/draw/cuda_canvas.hpp>
#include <aetk/effect/gpu/kernels/swizzle.h>
#include <aetk/effect/pixel/accessor.hpp>
#include <aetk/effect/pixel/device_kind.hpp>
#include <aetk/effect/pixel/iteration.hpp>
#include <algorithm>
#include <stdexcept>

namespace aetk::effect {
using aetk::core::pixel_range;

/** @brief Retrieve standard pixel format string name. */
inline std::string pixel_format_str(PF_PixelFormat fmt = PF_PixelFormat_INVALID) {
    switch (fmt) {
    case PF_PixelFormat_ARGB32:
        return "PF_PixelFormat_ARGB32";
    case PF_PixelFormat_ARGB64:
        return "PF_PixelFormat_ARGB64";
    case PF_PixelFormat_ARGB128:
        return "PF_PixelFormat_ARGB128";
    case PF_PixelFormat_GPU_BGRA128:
        return "PF_PixelFormat_GPU_BGRA128";
    case PF_PixelFormat_RESERVED:
        return "PF_PixelFormat_RESERVED";
    case PF_PixelFormat_BGRA32:
        return "PF_PixelFormat_BGRA32";
    case PF_PixelFormat_VUYA32:
        return "PF_PixelFormat_VUYA32";
    case PF_PixelFormat_NTSCDV25:
        return "PF_PixelFormat_NTSCDV25";
    case PF_PixelFormat_PALDV25:
        return "PF_PixelFormat_PALDV25";
    case PF_PixelFormat_INVALID:
        return "PF_PixelFormat_INVALID";
    case PF_PixelFormat_FORCE_LONG_INT:
        return "PF_PixelFormat_FORCE_LONG_INT";
    default:
        return "UNKNOWN_FORMAT";
    }
}
/**
 * @brief Unified RAII wrapper for all PF_EffectWorld types.
 *
 * @details Handles CPU worlds, GPU worlds, layer pixel checkouts, scratch
 * worlds, and output worlds in a single class. Ownership determines cleanup
 * behavior.
 *
 * Create via constructors or convenience factory methods:
 *   smart_world(w, in_data, type)     - Wrap an existing world
 *   smart_world(in_data, index)       - Classic parameter checkout
 *   smart_world(in_data, w, h, ...)   - Scratch world allocation (CPU or GPU)
 *   smart_world::zeros(...)           - Zero-initialized scratch world
 *   smart_world::empty(...)           - Uninitialized scratch world
 *
 * GPU-specific operations (gpu_data, gpu_copy, etc.) are always safe to call.
 * They return nullptr/0 when the world isn't GPU-backed.
 *
 * ### ⚠️ Crucial Data Layout & Value Scaling Rules:
 *
 * `smart_world` exposes raw pixel data in the host's native format and ranges.
 * Depending on the project's bit depth and pixel format, the channel ranges are:
 *
 * 1. **8-bit per channel (`PF_PixelFormat_ARGB32`, `PF_PixelFormat_BGRA32`)**:
 *    - **Range**: `0` to `255` (where `255` is white).
 *    - **Channel Type**: `A_u_char` (`uint8_t`).
 * 2. **16-bit per channel (`PF_PixelFormat_ARGB64`)**:
 *    - **Range**: `0` to `32768` (where `32768` is white). Value headroom exists up to
 * `65535`.
 *    - **Channel Type**: `A_u_short` (`uint16_t`).
 * 3. **32-bit float per channel (`PF_PixelFormat_ARGB128`,
 * `PF_PixelFormat_GPU_BGRA128`)**:
 *    - **Range**: `0.0f` to `1.0f` (where `1.0f` is white). Values can exceed `1.0f`
 * (HDR).
 *    - **Channel Type**: `float`.
 *
 * ### Interleaved Memory Layout:
 *
 * The raw pixel pointer (`data<T>()` or `tensor_view<T>()`) maps channels as interleaved:
 * - **After Effects Standard**: **ARGB** (channel 0=Alpha, 1=Red, 2=Green, 3=Blue).
 * - **Premiere Pro Standard**: **BGRA** (channel 0=Blue, 1=Green, 2=Red, 3=Alpha) or
 * **VUYA** (0=V, 1=U, 2=Y, 3=Alpha).
 *
 * ### High-Level vs. Raw Access:
 * - **Uniform Access**: Using `iterate_pixels(input, output, func)` automatically
 * converts and normalizes color values to the range **`[0.0, 1.0]`** float using
 * `aetk::core::color<>` references.
 * - **Raw Tensor Access**: Using `tensor_view<T>()` returns a non-owning multidimensional
 * mapping (`[H, W, 4]`) pointing directly to raw host memory in the unscaled ranges
 * above.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, managing after-effect
 * image assets (CPU and GPU scratch worlds, input/output buffers, checked-out
 * pixels) requires calling distinct APIs like `PF_NewWorld`, `PF_DisposeWorld`,
 * `checkout_layer_pixels`, `checkin_layer_pixels`, `CreateGPUWorld`, and
 * `DisposeGPUWorld`. Any unmatched call results in memory leaks, device lock
 * thrashing, or application crashes. `aetk::effect::smart_world` unifies all
 * CPU, GPU, output, and layer checked-out worlds under a single, move-only RAII
 * container. Depending on its `ownership` mode, the container automatically
 * selects the correct cleanup callback at scope exit, delivering absolute
 * thread and memory safety for classic or SmartFX plugins.
 *
 * @warning <b>Memory & Lifecycles:</b> The pixel world acts as the primary
 * holder of image assets. Standard copies are strictly disabled to prevent
 * double-free host buffer crashes. Depending on the lifecycle ownership type:
 *   - `ownership::LAYER_PIXELS`: Checked out pixels are checked back into After
 * Effects' compositing cache using `checkin_layer_pixels` upon destruction.
 *   - `ownership::SCRATCH_CPU`: CPU scratch worlds allocated using
 * `PF_NewWorld` are disposed via `PF_DisposeWorld` upon destruction.
 *   - `ownership::SCRATCH_GPU`: GPU scratch worlds are disposed via
 * `DisposeGPUWorld` upon destruction.
 */
class smart_world {
public:
    enum class ownership : std::uint8_t {
        NONE, ///< Non-owning view (output, external reference)
        LAYER_PIXELS, ///< SmartRender checkout → checkin_layer_pixels()
        LAYER_PARAM_CHECKOUT, ///< Classic parameter layer checkout → PF_CHECKIN_PARAM()
        SCRATCH_CPU, ///< new_world() → dispose_world()
        SCRATCH_GPU ///< CreateGPUWorld() → DisposeGPUWorld()
    };

    // ── Constructors, Destructors, Moves ──────────────────────────────

    ~smart_world() {
        cleanup();
    }

    smart_world() noexcept = default;

    /**
     * @brief Wrapping constructor for existing worlds (checkout, output, external reference).
     */
    smart_world(PF_EffectWorld* w, PF_InData* in_data, ownership type = ownership::NONE,
        PF_SmartRenderCallbacks* cb = nullptr, A_long index = -1)
        : m_world(w)
        , m_in_data(in_data)
        , m_ownership(type)
        , m_cb(cb)
        , m_index(index) {
        if (!m_world) {
            throw aetk::core::exception(PF_Err_INVALID_CALLBACK, "World pointer is null");
        }
        init_format();
        track();
    }

    /**
     * @brief Classic parameter layer checkout constructor.
     */
    smart_world(PF_InData* in_data, A_long index) {
        if (!in_data)
            throw aetk::core::exception(PF_Err_INTERNAL_STRUCT_DAMAGED, "Null in_data");

        PF_ParamDef def { };
        PF_Err err = PF_CHECKOUT_PARAM(in_data, static_cast<PF_ParamIndex>(index),
            in_data->current_time, in_data->time_step, in_data->time_scale, &def);
        if (err != PF_Err_NONE) {
            throw aetk::core::exception(err,
                "Failed to checkout layer parameter at index " + std::to_string(index));
        }

        m_in_data = in_data;
        m_ownership = ownership::LAYER_PARAM_CHECKOUT;
        m_index = index;
        m_owned_def = def;
        m_world = &m_owned_def.u.ld;
        init_format();

        aetk::core::memory_tracker::world_created(false);
    }

    /**
     * @brief Scratch world allocation constructor (CPU or GPU).
     */
    smart_world(PF_InData* in_data, A_long width, A_long height, short bitdepth = 8,
        bool clear_pixels = true, device_kind device_loc = device_kind::cpu) {
        if (!in_data)
            throw aetk::core::exception(PF_Err_INTERNAL_STRUCT_DAMAGED, "Null in_data");

        m_in_data = in_data;
        m_ownership = (device_loc == device_kind::cuda) ? ownership::SCRATCH_GPU : ownership::SCRATCH_CPU;

        if (device_loc == device_kind::cuda) {
            PF_PixelFormat format = PF_PixelFormat_GPU_BGRA128;
            PF_EffectWorld* world_ptr = nullptr;
            aetk::core::suite<PF_GPUDeviceSuite1> s(
                ::aetk::core::context::get_basic_suite());

            A_u_long device_index = 0;
            PF_RationalScale par = in_data->pixel_aspect_ratio;
            if (par.den <= 0) {
                par = { 1, 1 };
            }
            aetk::core::check_err(
                s->CreateGPUWorld(in_data->effect_ref, device_index, width, height, par,
                    PF_Field_FRAME, format, true, &world_ptr),
                "Failed to create GPU world");

            m_world = world_ptr;
            m_cached_format = format;
        } else {
            PF_PixelFormat format = PF_PixelFormat_ARGB32;
            if (bitdepth == 16)
                format = PF_PixelFormat_ARGB64;
            else if (bitdepth == 32)
                format = PF_PixelFormat_ARGB128;

            PF_EffectWorld local { };
            local.pix_aspect_ratio = in_data->pixel_aspect_ratio;
            try {
                aetk::core::suite<PF_WorldSuite2> world_suite(
                    ::aetk::core::context::get_basic_suite());
                aetk::core::check_err(
                    world_suite->PF_NewWorld(in_data->effect_ref, width, height,
                        clear_pixels ? TRUE : FALSE, format, &local),
                    "Failed to create scratch world");
            } catch (const std::exception&) {
                if (bitdepth == 8) {
                    aetk::core::check_err(
                        in_data->utils->new_world(in_data->effect_ref, width, height,
                            clear_pixels ? PF_NewWorldFlag_CLEAR_PIXELS
                                         : PF_NewWorldFlag_NONE,
                            &local),
                        "Failed to create fallback scratch world");
                } else {
                    throw;
                }
            }

            m_world = new PF_EffectWorld(local);
            m_cached_format = format;
        }
        track();
    }

    /**
     * @brief Construct smart_world from AEGP world handle.
     */
    smart_world(const aetk::aegp::aegp_world& src, PF_InData* in_data) {
        if (!in_data) {
            throw aetk::core::exception(PF_Err_INTERNAL_STRUCT_DAMAGED, "Null in_data");
        }
        if (!src.get()) {
            throw aetk::core::exception(
                PF_Err_BAD_CALLBACK_PARAM, "Null AEGP world handle");
        }
        PF_EffectWorld src_pf_world { };
        aetk::core::suite<AEGP_WorldSuite3> suite(
            ::aetk::core::context::get_basic_suite());
        aetk::core::check_err(suite->AEGP_FillOutPFEffectWorld(src.get(), &src_pf_world),
            "AEGP_FillOutPFEffectWorld failed");

        smart_world src_view(&src_pf_world, in_data, ownership::NONE);

        short bitdepth = 8;
        PF_PixelFormat pf = src_view.pixel_format();
        if (pf == PF_PixelFormat_ARGB64) {
            bitdepth = 16;
        } else if (pf == PF_PixelFormat_ARGB128) {
            bitdepth = 32;
        }

        *this = smart_world(in_data, src.width(), src.height(), bitdepth, false);
        src_view.copy_to(*this);
    }

    smart_world(smart_world&& other) noexcept
        : m_world(other.m_world)
        , m_in_data(other.m_in_data)
        , m_ownership(other.m_ownership)
        , m_cb(other.m_cb)
        , m_index(other.m_index)
        , m_cached_format(other.m_cached_format)
        , m_owned_def(other.m_owned_def) {
        if (other.m_world == &other.m_owned_def.u.ld) {
            m_world = &m_owned_def.u.ld;
        }
        other.m_world = nullptr;
        other.m_cached_format = PF_PixelFormat_INVALID;
    }

    smart_world& operator=(smart_world&& other) noexcept {
        if (this != &other) {
            cleanup();
            m_world = other.m_world;
            m_in_data = other.m_in_data;
            m_ownership = other.m_ownership;
            m_cb = other.m_cb;
            m_index = other.m_index;
            m_cached_format = other.m_cached_format;
            m_owned_def = other.m_owned_def;
            if (other.m_world == &other.m_owned_def.u.ld) {
                m_world = &m_owned_def.u.ld;
            }
            other.m_world = nullptr;
            other.m_cached_format = PF_PixelFormat_INVALID;
        }
        return *this;
    }

    smart_world(const smart_world&) = delete;
    smart_world& operator=(const smart_world&) = delete;

    // ── Common Accessors ───────────────────────────────────────────────

    PF_EffectWorld* ptr() const { return m_world; }
    PF_EffectWorld& get() const { return *m_world; }
    PF_InData* in_data() const { return m_in_data; }
    PF_InData* in_data_ptr() const { return m_in_data; }
    PF_ProgPtr effect_ref() const { return m_in_data ? m_in_data->effect_ref : nullptr; }
    void require_in_data() const {
        if (!m_in_data) {
            throw aetk::core::exception(PF_Err_INTERNAL_STRUCT_DAMAGED, "in_data is required for this operation");
        }
    }
    ownership get_ownership() const { return m_ownership; }

    explicit operator bool() const { return m_world != nullptr; }

    A_long width() const { return m_world ? m_world->width : 0; }
    A_long height() const { return m_world ? m_world->height : 0; }
    A_long rowbytes() const { return m_world ? m_world->rowbytes : 0; }
    aetk::core::rect bounds() const { return { 0, 0, (int32_t)width(), (int32_t)height() }; }

    PF_PixelFormat pixel_format(PF_PixelFormat fallback = PF_PixelFormat_ARGB32) const {
        if (m_cached_format != PF_PixelFormat_INVALID)
            return m_cached_format;
        return fallback;
    }

    bool is_bgra() const { return m_in_data && m_in_data->appl_id == 'PrMr'; }
    bool is_gpu() const { return pixel_format() == PF_PixelFormat_GPU_BGRA128; }

    // ── Convenience Static Factories ──────────────────────────────────

    static smart_world zeros(PF_InData* in_data, A_long width, A_long height,
        short bitdepth = 8, device_kind device_loc = device_kind::cpu) {
        return { in_data, width, height, bitdepth, true, device_loc };
    }

    static smart_world zeros_like(const smart_world& other) {
        short bitdepth = 8;
        PF_PixelFormat pf = other.pixel_format();
        if (pf == PF_PixelFormat_ARGB64)
            bitdepth = 16;
        else if (pf == PF_PixelFormat_ARGB128)
            bitdepth = 32;
        device_kind dev = other.is_gpu() ? device_kind::cuda : device_kind::cpu;
        return { other.m_in_data, other.width(), other.height(), bitdepth, true, dev };
    }

    static smart_world empty(PF_InData* in_data, A_long width, A_long height,
        short bitdepth = 8, device_kind device_loc = device_kind::cpu) {
        return { in_data, width, height, bitdepth, false, device_loc };
    }

    static smart_world empty_like(const smart_world& other) {
        short bitdepth = 8;
        PF_PixelFormat pf = other.pixel_format();
        if (pf == PF_PixelFormat_ARGB64)
            bitdepth = 16;
        else if (pf == PF_PixelFormat_ARGB128)
            bitdepth = 32;
        device_kind dev = other.is_gpu() ? device_kind::cuda : device_kind::cpu;
        return { other.m_in_data, other.width(), other.height(), bitdepth, false, dev };
    }

    static int find_device_index(PF_ProgPtr effect_ref, device_kind kind) {
        int device_index = 0;
        if (kind != device_kind::cpu && ::aetk::core::context::get_basic_suite()
            && effect_ref) {
            try {
                aetk::core::suite<PF_GPUDeviceSuite1> s(
                    ::aetk::core::context::get_basic_suite());
                A_u_long count = 0;
                if (s->GetDeviceCount(effect_ref, &count) == PF_Err_NONE && count > 0) {
                    PF_GPU_Framework target_framework = PF_GPU_Framework_NONE;
                    if (kind == device_kind::cuda) {
                        target_framework = PF_GPU_Framework_CUDA;
                    } else if (kind == device_kind::metal) {
                        target_framework = PF_GPU_Framework_METAL;
                    } else if (kind == device_kind::opencl) {
                        target_framework = PF_GPU_Framework_OPENCL;
                    } else if (kind == device_kind::d3d12) {
                        target_framework = PF_GPU_Framework_DIRECTX;
                    }

                    if (kind == device_kind::cpu_pinned) {
                        bool found = false;
                        for (A_u_long i = 0; i < count; ++i) {
                            PF_GPUDeviceInfo info { };
                            if (s->GetDeviceInfo(effect_ref, i, &info) == PF_Err_NONE
                                && info.compatibleB) {
                                device_index = static_cast<int>(i);
                                found = true;
                                break;
                            }
                        }
                        if (!found)
                            device_index = 0;
                    } else {
                        bool found = false;
                        for (A_u_long i = 0; i < count; ++i) {
                            PF_GPUDeviceInfo info { };
                            if (s->GetDeviceInfo(effect_ref, i, &info) == PF_Err_NONE) {
                                if (info.device_framework == target_framework
                                    && info.compatibleB) {
                                    device_index = static_cast<int>(i);
                                    found = true;
                                    break;
                                }
                            }
                        }
                        if (!found)
                            device_index = 0;
                    }
                }
            } catch (const std::exception&) {
                device_index = 0;
            }
        }
        return device_index;
    }

    // ── CPU & GPU Pixel Access ─────────────────────────────────────────

    template <typename T = char>
    T* data() const {
        if (!m_world)
            return nullptr;
        if (is_gpu()) {
            return reinterpret_cast<T*>(const_cast<smart_world*>(this)->gpu_data());
        }
        return reinterpret_cast<T*>(m_world->data);
    }

    template <aetk::core::pixel_range Range = aetk::core::pixel_range::tkfloat>
    aetk::core::color<Range> get_pixel(int x, int y) const {
        if (!m_world || !m_world->data)
            return { };

        x = std::clamp(x, 0, (int)width() - 1);
        y = std::clamp(y, 0, (int)height() - 1);

        const char* row
            = reinterpret_cast<const char*>(m_world->data) + (y * m_world->rowbytes);

        return visit_pixel_format<Range>(
            pixel_format(), is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
                const auto* px = reinterpret_cast<const PixelT*>(row) + x;
                return pixel_accessor<PixelT, IsBGRA, Range>::read(px);
            });
    }

    template <aetk::core::pixel_range Range = aetk::core::pixel_range::tkfloat>
    void set_pixel(int x, int y, const aetk::core::color<Range>& c) {
        if (!m_world || !m_world->data || x < 0 || y < 0 || x >= width() || y >= height())
            return;

        char* row = reinterpret_cast<char*>(m_world->data) + (y * m_world->rowbytes);

        visit_pixel_format<Range>(
            pixel_format(), is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
                auto* px = reinterpret_cast<PixelT*>(row) + x;
                pixel_accessor<PixelT, IsBGRA, Range>::write(px, c);
            });
    }

    // ── GPU Platform accessors ─────────────────────────────────────────

    void* gpu_data() const {
        if (!m_world || !::aetk::core::context::get_basic_suite() || !is_gpu())
            return nullptr;
        void* ptr = nullptr;
        try {
            aetk::core::suite<PF_GPUDeviceSuite1> s(
                ::aetk::core::context::get_basic_suite());
            aetk::core::check_err(s->GetGPUWorldData(m_in_data->effect_ref, m_world, &ptr));
        } catch (const std::exception&) {
            return nullptr;
        }
        return ptr;
    }

    std::size_t gpu_size() const {
        if (!m_world || !::aetk::core::context::get_basic_suite() || !is_gpu())
            return 0;
        std::size_t size = 0;
        try {
            aetk::core::suite<PF_GPUDeviceSuite1> s(
                ::aetk::core::context::get_basic_suite());
            aetk::core::check_err(s->GetGPUWorldSize(m_in_data->effect_ref, m_world, &size));
        } catch (const std::exception&) {
            return 0;
        }
        return size;
    }

    A_u_long gpu_device_index() const {
        if (!m_world || !::aetk::core::context::get_basic_suite() || !is_gpu())
            return 0;
        A_u_long idx = 0;

        aetk::core::suite<PF_GPUDeviceSuite1> s(::aetk::core::context::get_basic_suite());
        aetk::core::check_err(s->GetGPUWorldDeviceIndex(m_in_data->effect_ref, m_world, &idx));

        return idx;
    }

    // ── Pixel Iterators ────────────────────────────────────────────────

    template <pixel_range Range = pixel_range::tkfloat, typename Func>
    void iterate(smart_world& dst, Func&& func) const {
        aetk::effect::iterate_pixels<Range>(m_in_data, m_world, dst.ptr(), std::forward<Func>(func));
    }

    template <pixel_range Range = pixel_range::tkfloat, typename Func>
    void iterate_origin(smart_world& dst, const aetk::core::rect* area, const PF_Point* origin, Func&& func) const {
        aetk::effect::iterate_pixels_origin<Range>(m_in_data, m_world, dst.ptr(), area, origin, std::forward<Func>(func));
    }

    // ── High Level utility members ──

    template <aetk::core::pixel_range Range = aetk::core::pixel_range::tkfloat>
    aetk::core::color<Range> sample_bilinear(float x, float y) const;

    void copy_to(smart_world& dest, const aetk::core::rect* src_rect = nullptr,
        const aetk::core::rect* dst_rect = nullptr, bool hq = true);

    void copy_to_centered(smart_world& dest, bool hq = true);

    smart_world clone() const;

    template <aetk::core::pixel_range Range = aetk::core::pixel_range::tkfloat>
    void fill(const aetk::core::color<Range>& c, const aetk::core::rect* r = nullptr);

    void convolve_to(smart_world& dest, const aetk::core::kernel_3x3& k,
        float unity_scale = 65536.0f, const aetk::core::rect* area = nullptr);

    void blend_with(
        const smart_world& other, float amount, const aetk::core::rect* area = nullptr);

    // ── Format & Device Conversion ─────────────────────────────────────

    enum class color_format : std::uint8_t { ARGB, RGBA, BGRA, RGB, BGR };

    struct convert_options {
        bool normalize; ///< Scale 8-bit [0,255] or 16-bit [0,32768] → [0,1]
        bool planar; ///< If true, output as NCHW (planar). Otherwise NHWC
        convert_options()
            : normalize(false)
            , planar(false) {
        }
    };

    template <typename T, device_kind Dev = device_kind::cpu> auto tensor_view() const;

    template <aetk::core::pixel_range Range = aetk::core::pixel_range::tkfloat>
    smart_world to(PF_PixelFormat target_format) const {
        if (!m_world)
            return { };

        if (target_format == pixel_format()) {
            if (m_ownership == ownership::NONE) {
                return { m_world, m_in_data, ownership::NONE };
            }
        }

        short target_bitdepth = 8;
        if (target_format == PF_PixelFormat_ARGB64) {
            target_bitdepth = 16;
        } else if (target_format == PF_PixelFormat_ARGB128) {
            target_bitdepth = 32;
        }

        smart_world dst(m_in_data, width(), height(), target_bitdepth, false);

        visit_pixel_format<Range>(
            pixel_format(), is_bgra(), [&]<typename SrcPixelT, bool SrcBGRA>() {
                visit_pixel_format<Range>(target_format, dst.is_bgra(),
                    [&]<typename DstPixelT, bool DstBGRA>() {
                        for (A_long y = 0; y < height(); ++y) {
                            const char* src_row
                                = reinterpret_cast<const char*>(m_world->data)
                                + (y * m_world->rowbytes);
                            const SrcPixelT* src_pxs
                                = reinterpret_cast<const SrcPixelT*>(src_row);

                            char* dst_row = reinterpret_cast<char*>(dst.ptr()->data)
                                + (y * dst.rowbytes());
                            DstPixelT* dst_pxs = reinterpret_cast<DstPixelT*>(dst_row);

                            for (A_long x = 0; x < width(); ++x) {
                                aetk::core::color<Range> c
                                    = pixel_accessor<SrcPixelT, SrcBGRA, Range>::read(
                                        &src_pxs[x]);
                                pixel_accessor<DstPixelT, DstBGRA, Range>::write(
                                    &dst_pxs[x], c);
                            }
                        }
                    });
            });

        return dst;
    }

    smart_world to(device_kind target_device) const {
        if (!m_world)
            return { };

        if (target_device == device_kind::cpu) {
            if (!is_gpu()) {
                auto dst = smart_world::empty_like(*this);
                const_cast<smart_world*>(this)->copy_to(dst);
                return dst;
            } else {
#if defined(AETK_ENABLE_CUDA) || defined(AETK_CUDA_SUPPORT) || defined(__CUDACC__)
                size_t w = width();
                size_t h = height();

                auto dst = smart_world::empty(m_in_data, static_cast<A_long>(w),
                    static_cast<A_long>(h), 32, device_kind::cpu);

                void* cpu_data = dst.ptr()->data;
                int cpu_pitch = dst.rowbytes();
                void* gpu_ptr = const_cast<smart_world*>(this)->gpu_data();
                int gpu_pitch = rowbytes();

                if (cudaMemcpy2D(cpu_data, cpu_pitch, gpu_ptr, gpu_pitch,
                        w * 4 * sizeof(float), h, cudaMemcpyDeviceToHost)
                    != cudaSuccess) {
                    throw std::runtime_error(
                        "cudaMemcpy2D (DeviceToHost) failed in smart_world::to");
                }

                cudaDeviceSynchronize();

                float* p = static_cast<float*>(cpu_data);
                int pitch_floats = cpu_pitch / sizeof(float);
                for (size_t y = 0; y < h; ++y) {
                    float* row = p + y * pitch_floats;
                    for (size_t x = 0; x < w; ++x) {
                        std::swap(row[x * 4], row[x * 4 + 2]);
                    }
                }
                return dst;
#else
                throw std::runtime_error("GPU support is not enabled in this build");
#endif
            }
        } else if (target_device == device_kind::cuda) {
            if (is_gpu()) {
                auto dst = smart_world::empty_like(*this);
#if defined(AETK_ENABLE_CUDA) || defined(AETK_CUDA_SUPPORT) || defined(__CUDACC__)
                void* src_ptr = const_cast<smart_world*>(this)->gpu_data();
                void* dst_ptr = dst.gpu_data();
                if (cudaMemcpy2D(dst_ptr, dst.rowbytes(), src_ptr, rowbytes(),
                        width() * 4 * sizeof(float), height(), cudaMemcpyDeviceToDevice)
                    != cudaSuccess) {
                    throw std::runtime_error(
                        "cudaMemcpy2D (DeviceToDevice) failed in smart_world::to");
                }
                return dst;
#else
                throw std::runtime_error("GPU support is not enabled in this build");
#endif
            } else {
#if defined(AETK_ENABLE_CUDA) || defined(AETK_CUDA_SUPPORT) || defined(__CUDACC__)
                size_t w = width();
                size_t h = height();

                smart_world cpu_float_world;
                if (pixel_format() != PF_PixelFormat_ARGB128) {
                    cpu_float_world = this->to(PF_PixelFormat_ARGB128);
                } else {
                    cpu_float_world = { const_cast<PF_EffectWorld*>(ptr()), m_in_data, ownership::NONE };
                }

                auto dst = smart_world::empty(m_in_data, static_cast<A_long>(w),
                    static_cast<A_long>(h), 32, device_kind::cuda);

                void* src_cpu = cpu_float_world.ptr()->data;
                int src_pitch = cpu_float_world.rowbytes();
                float* dst_bgra = static_cast<float*>(dst.gpu_data());
                int dst_bgra_pitch = dst.rowbytes();

                if (cudaMemcpy2D(dst_bgra, dst_bgra_pitch, src_cpu, src_pitch,
                        w * 4 * sizeof(float), h, cudaMemcpyHostToDevice)
                    != cudaSuccess) {
                    throw std::runtime_error(
                        "cudaMemcpy2D (HostToDevice) failed in smart_world::to");
                }

                cudaStream_t stream = nullptr;
                try {
                    aetk::core::suite<PF_GPUDeviceSuite1> s(
                        ::aetk::core::context::get_basic_suite());
                    PF_GPUDeviceInfo info { };
                    if (s->GetDeviceInfo(m_in_data->effect_ref, static_cast<int>(dst.gpu_device_index()), &info)
                        == PF_Err_NONE) {
                        if (info.device_framework == PF_GPU_Framework_CUDA) {
                            stream = static_cast<cudaStream_t>(info.command_queuePV);
                        }
                    }
                } catch (const std::exception&) { }

                aetk::effect::cuda_swizzle_inplace(dst_bgra, dst_bgra_pitch,
                    static_cast<int>(w), static_cast<int>(h), stream);

                cudaDeviceSynchronize();
                return dst;
#else
                throw std::runtime_error("GPU support is not enabled in this build");
#endif
            }
        }
        return { };
    }

    void to(color_format format, float* dst, convert_options opts = { }) const {
        if (!m_world || !dst)
            return;

        const int w = width();
        const int h = height();
        const int channels
            = (format == color_format::RGB || format == color_format::BGR) ? 3 : 4;

        const size_t plane_size = static_cast<size_t>(w) * h;

        for (A_long y = 0; y < h; ++y) {
            for (A_long x = 0; x < w; ++x) {
                aetk::core::color<> c = get_pixel(x, y);

                float r = c.red;
                float g = c.green;
                float b = c.blue;
                float a = c.alpha;

                if (!opts.normalize) {
                    PF_PixelFormat pf = pixel_format();
                    float max_val = 255.0f;
                    if (pf == PF_PixelFormat_ARGB64)
                        max_val = 32768.0f;
                    else if (pf == PF_PixelFormat_ARGB128)
                        max_val = 1.0f;

                    r *= max_val;
                    g *= max_val;
                    b *= max_val;
                    a *= max_val;
                }

                float out[4] = { 0 };
                switch (format) {
                case color_format::ARGB:
                    out[0] = a;
                    out[1] = r;
                    out[2] = g;
                    out[3] = b;
                    break;
                case color_format::RGBA:
                    out[0] = r;
                    out[1] = g;
                    out[2] = b;
                    out[3] = a;
                    break;
                case color_format::BGRA:
                    out[0] = b;
                    out[1] = g;
                    out[2] = r;
                    out[3] = a;
                    break;
                case color_format::RGB:
                    out[0] = r;
                    out[1] = g;
                    out[2] = b;
                    break;
                case color_format::BGR:
                    out[0] = b;
                    out[1] = g;
                    out[2] = r;
                    break;
                }

                if (opts.planar) {
                    for (int c_idx = 0; c_idx < channels; ++c_idx) {
                        dst[c_idx * plane_size + y * w + x] = out[c_idx];
                    }
                } else {
                    float* p = dst + (y * w + x) * channels;
                    for (int c_idx = 0; c_idx < channels; ++c_idx) {
                        p[c_idx] = out[c_idx];
                    }
                }
            }
        }
    }

    void from(color_format format, const float* src, convert_options opts = { }) {
        if (!m_world || !src)
            return;

        const int w = width();
        const int h = height();
        const int channels
            = (format == color_format::RGB || format == color_format::BGR) ? 3 : 4;

        visit_pixel_format(
            pixel_format(), is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
                const size_t plane_size = static_cast<size_t>(w) * h;

                for (A_long y = 0; y < h; ++y) {
                    auto* row_start = reinterpret_cast<char*>(m_world->data)
                        + (y * m_world->rowbytes);
                    auto* px_row = reinterpret_cast<PixelT*>(row_start);
                    for (A_long x = 0; x < w; ++x) {
                        float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

                        if (opts.planar) {
                            size_t idx = static_cast<size_t>(y) * w + x;
                            switch (format) {
                            case color_format::ARGB:
                                a = src[idx];
                                r = src[idx + plane_size];
                                g = src[idx + plane_size * 2];
                                b = src[idx + plane_size * 3];
                                break;
                            case color_format::RGBA:
                                r = src[idx];
                                g = src[idx + plane_size];
                                b = src[idx + plane_size * 2];
                                a = src[idx + plane_size * 3];
                                break;
                            case color_format::BGRA:
                                b = src[idx];
                                g = src[idx + plane_size];
                                r = src[idx + plane_size * 2];
                                a = src[idx + plane_size * 3];
                                break;
                            case color_format::RGB:
                                r = src[idx];
                                g = src[idx + plane_size];
                                b = src[idx + plane_size * 2];
                                break;
                            case color_format::BGR:
                                b = src[idx];
                                g = src[idx + plane_size];
                                r = src[idx + plane_size * 2];
                                break;
                            }
                        } else {
                            const size_t stride = (channels == 3) ? 3 : 4;
                            size_t idx = (static_cast<size_t>(y) * w + x) * stride;
                            switch (format) {
                            case color_format::ARGB:
                                a = src[idx];
                                r = src[idx + 1];
                                g = src[idx + 2];
                                b = src[idx + 3];
                                break;
                            case color_format::RGBA:
                                r = src[idx];
                                g = src[idx + 1];
                                b = src[idx + 2];
                                a = src[idx + 3];
                                break;
                            case color_format::BGRA:
                                b = src[idx];
                                g = src[idx + 1];
                                r = src[idx + 2];
                                a = src[idx + 3];
                                break;
                            case color_format::RGB:
                                r = src[idx];
                                g = src[idx + 1];
                                b = src[idx + 2];
                                break;
                            case color_format::BGR:
                                b = src[idx];
                                g = src[idx + 1];
                                r = src[idx + 2];
                                break;
                            }
                        }

                        if (!opts.normalize) {
                            r *= pixel_accessor<PixelT, IsBGRA>::inv_max_val;
                            g *= pixel_accessor<PixelT, IsBGRA>::inv_max_val;
                            b *= pixel_accessor<PixelT, IsBGRA>::inv_max_val;
                            a *= pixel_accessor<PixelT, IsBGRA>::inv_max_val;
                        }

                        aetk::core::color<> c(a, r, g, b);
                        pixel_accessor<PixelT, IsBGRA>::write(&px_row[x], c);
                    }
                }
            });
    }

private:
    void track() {
        if (m_ownership != ownership::NONE) {
            aetk::core::memory_tracker::world_created(
                m_ownership == ownership::SCRATCH_GPU);
        }
    }

    void init_format() {
        if (!m_world) {
            m_cached_format = PF_PixelFormat_ARGB32;
            return;
        }

        if (m_world->data == nullptr) {
            m_cached_format = PF_PixelFormat_GPU_BGRA128;
            return;
        }

        if (!::aetk::core::context::get_basic_suite()) {
            m_cached_format = PF_PixelFormat_ARGB32;
            return;
        }
        aetk::core::suite<PF_WorldSuite2> world_suite(
            ::aetk::core::context::get_basic_suite());
        PF_PixelFormat format = PF_PixelFormat_INVALID;
        if (world_suite->PF_GetPixelFormat(m_world, &format) == PF_Err_NONE) {
            m_cached_format = format;
            return;
        }

        aetk::core::suite<PF_PixelFormatSuite1> pfmt_suite(
            ::aetk::core::context::get_basic_suite(), kPFPixelFormatSuite,
            kPFPixelFormatSuiteVersion1);
        PrPixelFormat pr_format = PrPixelFormat_Invalid;
        if (pfmt_suite->GetPixelFormat(m_world, &pr_format) == PF_Err_NONE) {
            if (pr_format == PrPixelFormat_BGRA_4444_32f
                || pr_format == PrPixelFormat_ARGB_4444_32f
                || pr_format == PrPixelFormat_BGRP_4444_32f
                || pr_format == PrPixelFormat_PRGB_4444_32f
                || pr_format == PrPixelFormat_BGRX_4444_32f
                || pr_format == PrPixelFormat_BGRA_4444_32f_Linear
                || pr_format == PrPixelFormat_BGRP_4444_32f_Linear
                || pr_format == PrPixelFormat_BGRX_4444_32f_Linear
                || pr_format == PrPixelFormat_ARGB_4444_32f_Linear
                || pr_format == PrPixelFormat_PRGB_4444_32f_Linear
                || pr_format == PrPixelFormat_XRGB_4444_32f_Linear) {
                m_cached_format = PF_PixelFormat_ARGB128;
            } else if (pr_format == PrPixelFormat_BGRA_4444_16u
                || pr_format == PrPixelFormat_ARGB_4444_16u
                || pr_format == PrPixelFormat_BGRP_4444_16u
                || pr_format == PrPixelFormat_PRGB_4444_16u
                || pr_format == PrPixelFormat_BGRX_4444_16u) {
                m_cached_format = PF_PixelFormat_ARGB64;
            } else {
                m_cached_format = PF_PixelFormat_ARGB32;
            }
            return;
        }

        m_cached_format = PF_PixelFormat_ARGB32;
    }

    void cleanup() {
        if (!m_world)
            return;

        bool was_owned = (m_ownership != ownership::NONE);
        bool was_gpu = (m_ownership == ownership::SCRATCH_GPU);

        switch (m_ownership) {
        case ownership::LAYER_PIXELS:
            if (m_cb && m_index >= 0 && m_in_data) {
                PF_Err err = m_cb->checkin_layer_pixels(m_in_data->effect_ref, m_index);
                if (err != PF_Err_NONE) {
                    AETK_LOG_ERR(
                        "checkin_layer_pixels failed with code: " + std::to_string(err));
                }
            }
            break;

        case ownership::LAYER_PARAM_CHECKOUT:
            if (m_in_data) {
                PF_CHECKIN_PARAM(m_in_data, &m_owned_def);
            }
            break;

        case ownership::SCRATCH_CPU:
            if (m_in_data && m_world) {
                aetk::core::suite<PF_WorldSuite2> world_suite(
                    ::aetk::core::context::get_basic_suite());
                world_suite->PF_DisposeWorld(m_in_data->effect_ref, m_world);

                delete m_world;
            }
            break;

        case ownership::SCRATCH_GPU:
            if (::aetk::core::context::get_basic_suite() && m_world && m_in_data) {
                aetk::core::suite<PF_GPUDeviceSuite1> s(
                    ::aetk::core::context::get_basic_suite());
                s->DisposeGPUWorld(m_in_data->effect_ref, m_world);
            }
            break;

        case ownership::NONE:
        default:
            break;
        }
        m_world = nullptr;

        if (was_owned) {
            aetk::core::memory_tracker::world_destroyed(was_gpu);
        }
    }

    PF_EffectWorld* m_world = nullptr;
    PF_InData* m_in_data = nullptr; // Full context (preferred)
    ownership m_ownership = ownership::NONE;
    PF_SmartRenderCallbacks* m_cb = nullptr; // For LAYER_PIXELS cleanup
    A_long m_index = -1; // Checkout index
    PF_ParamDef m_owned_def { }; // Saved checkout param definition for classic mode
    mutable PF_PixelFormat m_cached_format = PF_PixelFormat_INVALID;
};

template <typename PixelT, typename Func>
inline void iterate(const smart_world& src, smart_world& dst, Func&& func) {
    iterate<PixelT>(src.in_data(), src.ptr(), dst.ptr(), std::forward<Func>(func));
}

template <typename PixelT, typename Func>
inline void iterate_origin(const smart_world& src, smart_world& dst,
    const aetk::core::rect* area, const PF_Point* origin, Func&& func) {
    iterate_origin<PixelT>(src.in_data(), src.ptr(), dst.ptr(), area, origin, std::forward<Func>(func));
}

template <typename PixelT, typename Func>
inline void iterate_origin_non_clip(const smart_world& src, smart_world& dst,
    const aetk::core::rect* area, const PF_Point* origin, Func&& func) {
    iterate_origin_non_clip<PixelT>(src.in_data(), src.ptr(), dst.ptr(), area, origin, std::forward<Func>(func));
}

template <pixel_range Range = pixel_range::tkfloat, typename Func>
inline void iterate_pixels_origin(const smart_world& src, smart_world& dst,
    const aetk::core::rect* area, const PF_Point* origin, Func&& func) {
    iterate_pixels_origin<Range>(src.in_data(), src.ptr(), dst.ptr(), area, origin, std::forward<Func>(func));
}

template <pixel_range Range = pixel_range::tkfloat, typename Func>
inline void iterate_pixels(const smart_world& src, smart_world& dst, Func&& func) {
    iterate_pixels<Range>(src.in_data(), src.ptr(), dst.ptr(), std::forward<Func>(func));
}

} // namespace aetk::effect

#include <aetk/effect/pixel/tensor_view.hpp>

namespace aetk::effect {

template <typename T, device_kind Dev> auto smart_world::tensor_view() const {
    size_t h = height();
    size_t w = width();
    ptrdiff_t rowbytes = this->rowbytes();

    ptrdiff_t y_stride = rowbytes / sizeof(T);
    ptrdiff_t x_stride = 4; // Interleaved ARGB/BGRA
    ptrdiff_t c_stride = 1;

    size_t shape[3] = { h, w, 4 };
    ptrdiff_t strides[3] = { y_stride, x_stride, c_stride };

    if constexpr (Dev == device_kind::cuda) {
        return aetk::effect::tensor_view<T, 3, device_kind::cuda>(
            reinterpret_cast<T*>(gpu_data()), shape, strides);
    } else {
        return aetk::effect::tensor_view<T, 3, device_kind::cpu>(
            const_cast<T*>(data<T>()), shape, strides);
    }
}

// --------------------------------------------------------------------
// Out-of-line implementations of tensor/tensor_view to/from smart_world
// --------------------------------------------------------------------

template <typename T, size_t Rank, device_kind Dev>
void tensor<T, Rank, Dev>::allocate(PF_ProgPtr effect_ref, size_t bytes, int device_index, bool avoid_lock) {
    m_effect_ref = effect_ref;
    m_raw_size = bytes;
    m_avoid_lock = avoid_lock;

    if (device_index == -1) {
        device_index = smart_world::find_device_index(effect_ref, Dev);
    }
    m_device_index = device_index;

    if constexpr (Dev == device_kind::cpu) {
        m_data = reinterpret_cast<T*>(new char[bytes]());
        m_owned = true;
    } else if constexpr (Dev == device_kind::cpu_pinned) {
        if (!::aetk::core::context::get_basic_suite() || !effect_ref) {
            m_data = reinterpret_cast<T*>(new char[bytes]());
            m_owned = true;
        } else {
            aetk::core::suite<PF_GPUDeviceSuite1> s(
                ::aetk::core::context::get_basic_suite());
            if (!avoid_lock) {
                ::aetk::core::check_err(
                    s->AcquireExclusiveDeviceAccess(effect_ref, device_index));
            }
            void* raw_ptr = nullptr;
            A_Err alloc_err
                = s->AllocateHostMemory(effect_ref, device_index, bytes, &raw_ptr);
            if (!avoid_lock) {
                s->ReleaseExclusiveDeviceAccess(effect_ref, device_index);
            }
            ::aetk::core::check_err(alloc_err);
            m_data = reinterpret_cast<T*>(raw_ptr);
            std::memset(m_data, 0, bytes);
            m_owned = true;
        }
    } else if constexpr (Dev == device_kind::cuda) {
        if (!::aetk::core::context::get_basic_suite() || !effect_ref) {
#if defined(AETK_ENABLE_CUDA) || defined(AETK_CUDA_SUPPORT) || defined(__CUDACC__)
            void* raw_ptr = nullptr;
            cudaError_t err = cudaMalloc(&raw_ptr, bytes);
            if (err != cudaSuccess) {
                throw std::runtime_error(std::string("cudaMalloc fallback failed: ") + cudaGetErrorString(err));
            }
            m_data = reinterpret_cast<T*>(raw_ptr);
            m_owned = true;
#else
            throw std::runtime_error("CUDA is not enabled in this build");
#endif
        } else {
            aetk::core::suite<PF_GPUDeviceSuite1> s(
                ::aetk::core::context::get_basic_suite());
            if (!avoid_lock) {
                ::aetk::core::check_err(
                    s->AcquireExclusiveDeviceAccess(effect_ref, device_index));
            }
            void* raw_ptr = nullptr;
            A_Err alloc_err
                = s->AllocateDeviceMemory(effect_ref, device_index, bytes, &raw_ptr);
            if (!avoid_lock) {
                s->ReleaseExclusiveDeviceAccess(effect_ref, device_index);
            }
            ::aetk::core::check_err(alloc_err);
            m_data = reinterpret_cast<T*>(raw_ptr);
            m_owned = true;
        }
    } else {
        throw std::runtime_error("Unsupported device kind for raw allocation");
    }
    aetk::core::memory_tracker::world_created(Dev == device_kind::cuda);
}

template <typename T, size_t Rank, device_kind Dev>
void tensor<T, Rank, Dev>::cleanup() {
    if (!m_owned || !m_data)
        return;

    bool was_gpu = (Dev == device_kind::cuda);

    if constexpr (Dev == device_kind::cpu) {
        delete[] reinterpret_cast<char*>(m_data);
    } else if constexpr (Dev == device_kind::cpu_pinned) {
        if (::aetk::core::context::get_basic_suite() && m_effect_ref && m_data) {
            aetk::core::suite<PF_GPUDeviceSuite1> s(
                ::aetk::core::context::get_basic_suite());
            if (!m_avoid_lock) {
                s->AcquireExclusiveDeviceAccess(m_effect_ref, m_device_index);
            }
            s->FreeHostMemory(m_effect_ref, m_device_index, m_data);
            if (!m_avoid_lock) {
                s->ReleaseExclusiveDeviceAccess(m_effect_ref, m_device_index);
            }
        } else if (m_data) {
            delete[] reinterpret_cast<char*>(m_data);
        }
    } else if constexpr (Dev == device_kind::cuda) {
        if (::aetk::core::context::get_basic_suite() && m_effect_ref && m_data) {
            aetk::core::suite<PF_GPUDeviceSuite1> s(
                ::aetk::core::context::get_basic_suite());
            if (!m_avoid_lock) {
                s->AcquireExclusiveDeviceAccess(m_effect_ref, m_device_index);
            }
            s->FreeDeviceMemory(m_effect_ref, m_device_index, m_data);
            if (!m_avoid_lock) {
                s->ReleaseExclusiveDeviceAccess(m_effect_ref, m_device_index);
            }
        } else if (m_data) {
#if defined(AETK_ENABLE_CUDA) || defined(AETK_CUDA_SUPPORT) || defined(__CUDACC__)
            cudaFree(m_data);
#endif
        }
    }
    m_data = nullptr;
    m_owned = false;
    m_raw_size = 0;
    m_effect_ref = nullptr;

    aetk::core::memory_tracker::world_destroyed(was_gpu);
}

template <typename T, size_t Rank, device_kind Dev>
tensor<T, Rank, Dev>::~tensor() {
    cleanup();
}

template <typename T, size_t Rank, device_kind Dev>
void tensor<T, Rank, Dev>::copy_to(smart_world& dest) const {
    static_assert(Rank == 3, "copy_to requires Rank-3 tensor [height, width, channels]");
    size_t h = m_shape[0];
    size_t w = m_shape[1];
    size_t channels = m_shape[2];

    if (h != dest.height() || w != dest.width()) {
        throw std::runtime_error("Dimension mismatch when copying tensor to smart_world");
    }

    if (dest.is_gpu()) {
#if defined(AETK_ENABLE_CUDA) || defined(AETK_CUDA_SUPPORT) || defined(__CUDACC__)
        if constexpr (Dev == device_kind::cuda) {
            size_t src_pitch = m_strides[0] * sizeof(T);
            size_t dst_pitch = dest.rowbytes();
            void* dst_ptr = dest.gpu_data();

            if (cudaMemcpy2D(dst_ptr, dst_pitch, m_data, src_pitch,
                    w * channels * sizeof(T), h, cudaMemcpyDeviceToDevice)
                != cudaSuccess) {
                throw std::runtime_error("cudaMemcpy2D (DeviceToDevice) failed");
            }
        } else {
            auto gpu_temp = const_cast<tensor<T, Rank, Dev>*>(this)
                                ->template to<device_kind::cuda>();
            gpu_temp.copy_to(dest);
        }
#else
        throw std::runtime_error("GPU/CUDA copies are not enabled in this build");
#endif
    } else {
        if constexpr (Dev == device_kind::cuda) {
#if defined(AETK_ENABLE_CUDA) || defined(AETK_CUDA_SUPPORT) || defined(__CUDACC__)
            auto cpu_temp = const_cast<tensor<T, Rank, Dev>*>(this)
                                ->template to<device_kind::cpu_pinned>();
            cpu_temp.copy_to(dest);
#else
            throw std::runtime_error("CUDA is not enabled");
#endif
        } else {
            auto view = this->view();
            visit_pixel_format(
                dest.pixel_format(), dest.is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
                    char* data = reinterpret_cast<char*>(dest.ptr()->data);
                    ptrdiff_t rowbytes = dest.rowbytes();

                    for (size_t y = 0; y < h; ++y) {
                        PixelT* row_ptr = reinterpret_cast<PixelT*>(data + y * rowbytes);
                        for (size_t x = 0; x < w; ++x) {
                            aetk::core::color<> c;
                            // AE's interleaved layout is ARGB: index 0=Alpha, 1=Red,
                            // 2=Green, 3=Blue. This matches the stride ordering produced
                            // by smart_world::tensor_view().
                            if constexpr (std::is_floating_point_v<T>) {
                                c.alpha = (channels > 0) ? view(y, x, 0) : 1.0;
                                c.red = (channels > 1) ? view(y, x, 1) : 0.0;
                                c.green = (channels > 2) ? view(y, x, 2) : 0.0;
                                c.blue = (channels > 3) ? view(y, x, 3) : 0.0;
                            } else {
                                float max_v = 255.0f;
                                if constexpr (sizeof(T) == 2)
                                    max_v = 32768.0f;
                                c.alpha = (channels > 0) ? (view(y, x, 0) / max_v) : 1.0;
                                c.red = (channels > 1) ? (view(y, x, 1) / max_v) : 0.0;
                                c.green = (channels > 2) ? (view(y, x, 2) / max_v) : 0.0;
                                c.blue = (channels > 3) ? (view(y, x, 3) / max_v) : 0.0;
                            }
                            pixel_accessor<PixelT, IsBGRA>::write(row_ptr + x, c);
                        }
                    }
                });
        }
    }
}

template <typename T, size_t Rank, device_kind Dev>
tensor<T, Rank, Dev>::tensor(std::array<size_t, Rank> shape, smart_world&& storage)
    : m_shape(shape)
    , m_storage(std::move(storage)) {
    // Compute contiguous strides (C-order)
    ptrdiff_t stride = 1;
    for (int i = static_cast<int>(Rank) - 1; i >= 0; --i) {
        m_strides[i] = stride;
        stride *= m_shape[i];
    }
    if (m_storage.is_gpu()) {
        m_data = reinterpret_cast<T*>(m_storage.gpu_data());
    } else {
        m_data = const_cast<T*>(m_storage.data<T>());
    }
}

template <typename T, size_t Rank, device_kind Dev>
tensor<T, Rank, Dev>::tensor(std::array<size_t, Rank> shape)
    : m_shape(shape) {
    static_assert(Dev == device_kind::cpu,
        "No-context constructor is only available for CPU tensors");
    size_t total_elements = 1;
    for (size_t s : m_shape)
        total_elements *= s;
    size_t bytes = total_elements * sizeof(T);

    // Compute contiguous strides (C-order)
    ptrdiff_t stride = 1;
    for (int i = static_cast<int>(Rank) - 1; i >= 0; --i) {
        m_strides[i] = stride;
        stride *= m_shape[i];
    }

    allocate(nullptr, bytes, 0, false);
}

template <typename T, size_t Rank, device_kind Dev>
tensor<T, Rank, Dev>::tensor(std::array<size_t, Rank> shape, PF_ProgPtr effect_ref, device_kind kind, bool avoid_lock)
    : m_shape(shape) {
    if (kind != Dev) {
        throw std::runtime_error("Constructor kind parameter must match Dev template parameter");
    }
    size_t total_elements = 1;
    for (size_t s : m_shape)
        total_elements *= s;
    size_t bytes = total_elements * sizeof(T);

    // Compute contiguous strides (C-order)
    ptrdiff_t stride = 1;
    for (int i = static_cast<int>(Rank) - 1; i >= 0; --i) {
        m_strides[i] = stride;
        stride *= m_shape[i];
    }

    allocate(effect_ref, bytes, -1, avoid_lock);
}

template <typename T, size_t Rank, device_kind Dev>
tensor<T, Rank, Dev>::tensor(smart_world&& storage)
    : m_storage(std::move(storage)) {
    size_t h = m_storage.height();
    size_t w = m_storage.width();
    size_t c = 4; // AE worlds are interleaved ARGB / BGRA (4 channels)
    ptrdiff_t rowbytes = m_storage.rowbytes();

    if constexpr (Rank == 1) {
        m_shape = { h * w * c };
        m_strides = { 1 };
    } else if constexpr (Rank == 2) {
        m_shape = { h, w * c };
        m_strides = { static_cast<ptrdiff_t>(rowbytes / sizeof(T)), 1 };
    } else if constexpr (Rank == 3) {
        m_shape = { h, w, c };
        m_strides = { static_cast<ptrdiff_t>(rowbytes / sizeof(T)), 4, 1 };
    } else {
        static_assert(Rank >= 1 && Rank <= 3, "Rank must be 1, 2, or 3");
    }

    if (m_storage.is_gpu()) {
        m_data = reinterpret_cast<T*>(m_storage.gpu_data());
    } else {
        m_data = const_cast<T*>(m_storage.data<T>());
    }
}

template <typename T, size_t Rank, device_kind Dev>
tensor<T, Rank, Dev>::tensor(
    std::array<size_t, Rank> shape, void* data, size_t bytes, bool own)
    : m_shape(shape) {
    m_data = static_cast<T*>(data);
    m_raw_size = bytes;
    m_owned = own;
    // Compute contiguous strides (C-order)
    ptrdiff_t stride = 1;
    for (int i = static_cast<int>(Rank) - 1; i >= 0; --i) {
        m_strides[i] = stride;
        stride *= m_shape[i];
    }
}

template <typename T, size_t Rank, device_kind Dev>
tensor<T, Rank, Dev>::tensor(tensor&& other) noexcept
    : m_data(other.m_data)
    , m_shape(other.m_shape)
    , m_strides(other.m_strides)
    , m_storage(std::move(other.m_storage))
    , m_effect_ref(other.m_effect_ref)
    , m_raw_size(other.m_raw_size)
    , m_device_index(other.m_device_index)
    , m_avoid_lock(other.m_avoid_lock)
    , m_owned(other.m_owned) {
    other.m_data = nullptr;
    other.m_owned = false;
    other.m_raw_size = 0;
    other.m_effect_ref = nullptr;
}

template <typename T, size_t Rank, device_kind Dev>
tensor<T, Rank, Dev>& tensor<T, Rank, Dev>::operator=(tensor&& other) noexcept {
    if (this != &other) {
        cleanup();
        m_data = other.m_data;
        m_shape = other.m_shape;
        m_strides = other.m_strides;
        m_storage = std::move(other.m_storage);
        m_effect_ref = other.m_effect_ref;
        m_raw_size = other.m_raw_size;
        m_device_index = other.m_device_index;
        m_avoid_lock = other.m_avoid_lock;
        m_owned = other.m_owned;

        other.m_data = nullptr;
        other.m_owned = false;
        other.m_raw_size = 0;
        other.m_effect_ref = nullptr;
    }
    return *this;
}

template <typename T, size_t Rank, device_kind Dev>
template <device_kind TargetDev>
tensor<T, Rank, TargetDev> tensor<T, Rank, Dev>::to(device_kind kind) const {
    auto effect_ref = m_effect_ref;
    if (!effect_ref) {
        throw std::runtime_error(
            "to() requires valid captured PF_ProgPtr. Or pass context.");
    }

    size_t total_elements = 1;
    for (size_t s : m_shape)
        total_elements *= s;
    size_t bytes = total_elements * sizeof(T);

    tensor<T, Rank, TargetDev> dst(m_shape, effect_ref, TargetDev, m_avoid_lock);

    if constexpr (Dev == device_kind::cuda
        && (TargetDev == device_kind::cpu || TargetDev == device_kind::cpu_pinned)) {
#if defined(AETK_ENABLE_CUDA) || defined(AETK_CUDA_SUPPORT) || defined(__CUDACC__)
        size_t height = m_shape[0];
        size_t width = m_shape[1];
        size_t channels = (Rank == 3) ? m_shape[2] : 1;
        ptrdiff_t src_pitch = m_strides[0] * sizeof(T);
        ptrdiff_t dst_pitch = bytes / height;

        if (cudaMemcpy2D(dst.data_ptr(), dst_pitch, m_data, src_pitch,
                width * channels * sizeof(T), height, cudaMemcpyDeviceToHost)
            != cudaSuccess) {
            throw std::runtime_error(
                "cudaMemcpy2D failed during device-to-host transfer");
        }
#else
        throw std::runtime_error("CUDA support is not enabled in this build");
#endif
    } else if constexpr ((Dev == device_kind::cpu || Dev == device_kind::cpu_pinned)
        && TargetDev == device_kind::cuda) {
#if defined(AETK_ENABLE_CUDA) || defined(AETK_CUDA_SUPPORT) || defined(__CUDACC__)
        size_t height = m_shape[0];
        size_t width = m_shape[1];
        size_t channels = (Rank == 3) ? m_shape[2] : 1;
        ptrdiff_t src_pitch = m_strides[0] * sizeof(T);
        ptrdiff_t dst_pitch = bytes / height;

        if (cudaMemcpy2D(dst.data_ptr(), dst_pitch, m_data, src_pitch,
                width * channels * sizeof(T), height, cudaMemcpyHostToDevice)
            != cudaSuccess) {
            throw std::runtime_error(
                "cudaMemcpy2D failed during host-to-device transfer");
        }
#else
        throw std::runtime_error("CUDA support is not enabled in this build");
#endif
    } else {
        std::memcpy(dst.data_ptr(), m_data, bytes);
    }

    return dst;
}

} // namespace aetk::effect

#include <aetk/effect/pixel/smart_world_utils.hpp>

