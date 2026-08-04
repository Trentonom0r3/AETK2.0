#pragma once

#include <cmath>
#include <algorithm>

namespace aetk::effect {

template <aetk::core::pixel_range Range>
inline aetk::core::color<Range> smart_world::sample_bilinear(float x, float y) const {
    int x0 = (int)std::floor(x);
    int y0 = (int)std::floor(y);
    float tx = x - x0;
    float ty = y - y0;

    auto c00 = get_pixel<Range>(x0, y0);
    auto c10 = get_pixel<Range>(x0 + 1, y0);
    auto c01 = get_pixel<Range>(x0, y0 + 1);
    auto c11 = get_pixel<Range>(x0 + 1, y0 + 1);

    return aetk::core::color<Range>::mix(aetk::core::color<Range>::mix(c00, c10, tx),
        aetk::core::color<Range>::mix(c01, c11, tx), ty);
}

inline void smart_world::copy_to(smart_world& dest, const aetk::core::rect* src_rect,
    const aetk::core::rect* dst_rect, bool hq) {
    if (!m_world || !dest.m_world)
        return;

#if defined(AETK_ENABLE_CUDA) || defined(AETK_CUDA_SUPPORT) || defined(__CUDACC__)
    if (is_gpu() && dest.is_gpu()) {
        A_long start_y = src_rect ? src_rect->top : 0;
        A_long start_x = src_rect ? src_rect->left : 0;
        A_long dest_start_y = dst_rect ? dst_rect->top : 0;
        A_long dest_start_x = dst_rect ? dst_rect->left : 0;
        A_long width_to_copy
            = src_rect ? (src_rect->right - src_rect->left) : width();
        A_long height_to_copy
            = src_rect ? (src_rect->bottom - src_rect->top) : height();

        char* s_ptr = static_cast<char*>(gpu_data()) + start_y * rowbytes()
            + start_x * 4 * sizeof(float);
        char* d_ptr = static_cast<char*>(dest.gpu_data())
            + dest_start_y * dest.rowbytes() + dest_start_x * 4 * sizeof(float);

        if (cudaMemcpy2D(d_ptr, dest.rowbytes(), s_ptr, rowbytes(),
                width_to_copy * 4 * sizeof(float), height_to_copy,
                cudaMemcpyDeviceToDevice)
            != cudaSuccess) {
            throw std::runtime_error(
                "cudaMemcpy2D (DeviceToDevice) failed in smart_world::copy_to");
        }
        return;
    }

    if (is_gpu() && !dest.is_gpu()) {
        auto cpu_temp = this->to(device_kind::cpu);
        cpu_temp.copy_to(dest, src_rect, dst_rect, hq);
        return;
    }

    if (!is_gpu() && dest.is_gpu()) {
        auto gpu_temp = this->to(device_kind::cuda);
        gpu_temp.copy_to(dest, src_rect, dst_rect, hq);
        return;
    }
#endif

    // Tier 3: Mismatch conversion / format matching
    if (pixel_format() != dest.pixel_format()) {
        auto converted_src = this->to(dest.pixel_format());
        converted_src.copy_to(dest, src_rect, dst_rect, hq);
        return;
    }

    PF_Rect src_pf, dst_pf;
    PF_Rect *src_ptr = nullptr, *dst_ptr = nullptr;

    if (src_rect) {
        src_pf = src_rect->to_pf();
        src_ptr = &src_pf;
    }
    if (dst_rect) {
        dst_pf = dst_rect->to_pf();
        dst_ptr = &dst_pf;
    }

    // Tier 1: Modern Suite (PF_WorldTransformSuite1)
    try {
        aetk::core::suite<PF_WorldTransformSuite1> s(
            ::aetk::core::context::get_basic_suite());
        if (hq) {
            aetk::core::check_err(
                s->copy_hq(effect_ref(), m_world, dest.ptr(), src_ptr, dst_ptr),
                "WorldTransformSuite copy_hq failed");
        } else {
            aetk::core::check_err(
                s->copy(effect_ref(), m_world, dest.ptr(), src_ptr, dst_ptr),
                "WorldTransformSuite copy failed");
        }
        return;
    } catch (const std::exception&) {
        // Fallback to Tier 2
    }

    // Tier 2: Legacy host callbacks (Premiere Pro / Legacy AE)
    if (m_in_data && m_in_data->utils && m_in_data->utils->copy) {
        PF_Err err = m_in_data->utils->copy(
            effect_ref(), m_world, dest.ptr(), src_ptr, dst_ptr);
        if (err == PF_Err_NONE) {
            return;
        }
    }

    // Tier 3 fallback: Manual pixel iteration
    A_long start_y = src_rect ? src_rect->top : 0;
    A_long end_y = src_rect ? src_rect->bottom : height();
    A_long start_x = src_rect ? src_rect->left : 0;
    A_long end_x = src_rect ? src_rect->right : width();

    A_long dest_start_y = dst_rect ? dst_rect->top : 0;
    A_long dest_start_x = dst_rect ? dst_rect->left : 0;

    A_long width_to_copy = end_x - start_x;
    A_long height_to_copy = end_y - start_y;

    for (A_long y = 0; y < height_to_copy; ++y) {
        for (A_long x = 0; x < width_to_copy; ++x) {
            dest.set_pixel(dest_start_x + x, dest_start_y + y,
                get_pixel(start_x + x, start_y + y));
        }
    }
}

inline void smart_world::copy_to_centered(smart_world& dest, bool hq) {
    if (!m_world || !dest.m_world)
        return;
    A_long offset_x = (dest.width() - width()) / 2;
    A_long offset_y = (dest.height() - height()) / 2;
    A_long actual_x = (m_in_data && m_in_data->output_origin_x != 0)
        ? m_in_data->output_origin_x
        : offset_x;
    A_long actual_y = (m_in_data && m_in_data->output_origin_y != 0)
        ? m_in_data->output_origin_y
        : offset_y;

    aetk::core::rect dst_r(
        actual_x, actual_y, actual_x + width(), actual_y + height());
    copy_to(dest, nullptr, &dst_r, hq);
}

template <aetk::core::pixel_range Range>
inline void smart_world::fill(const aetk::core::color<Range>& c, const aetk::core::rect* r) {
    if (!m_world)
        return;

    aetk::core::color<aetk::core::pixel_range::tkfloat> c_float(c);

    if (is_gpu()) {
#if defined(AETK_ENABLE_CUDA)
        int r_left = 0, r_top = 0, r_right = width(), r_bottom = height();
        if (r) {
            r_left = r->left;
            r_top = r->top;
            r_right = r->right;
            r_bottom = r->bottom;
        }
        aetk::effect::draw::cuda_fill_pixels(gpu_data(), width(), height(),
            rowbytes(), (float)c_float.red, (float)c_float.green, (float)c_float.blue,
            (float)c_float.alpha, r_left, r_top, r_right, r_bottom);
        return;
#else
        throw std::runtime_error(
            "GPU fill is not supported in this build (CUDA disabled)");
#endif
    }

    PF_Rect rect_pf;
    PF_Rect* rect_ptr = nullptr;
    if (r) {
        rect_pf = r->to_pf();
        rect_ptr = &rect_pf;
    }

    PF_PixelFormat pf = pixel_format();

    // Tier 1: Modern Suite (PF_FillMatteSuite2)
    try {
        aetk::core::suite<PF_FillMatteSuite2> s(
            ::aetk::core::context::get_basic_suite());
        if (pf == PF_PixelFormat_ARGB128) {
            PF_PixelFloat color_f = { (float)c_float.alpha, (float)c_float.red,
                (float)c_float.green, (float)c_float.blue };
            aetk::core::check_err(
                s->fill_float(effect_ref(), &color_f, rect_ptr, m_world),
                "FillMatteSuite fill_float failed");
            return;
        } else if (pf == PF_PixelFormat_ARGB64) {
            PF_Pixel16 color_16;
            color_16.alpha
                = (A_u_short)std::clamp(c_float.alpha * 32768.0, 0.0, 32768.0);
            color_16.red = (A_u_short)std::clamp(c_float.red * 32768.0, 0.0, 32768.0);
            color_16.green
                = (A_u_short)std::clamp(c_float.green * 32768.0, 0.0, 32768.0);
            color_16.blue
                = (A_u_short)std::clamp(c_float.blue * 32768.0, 0.0, 32768.0);
            aetk::core::check_err(
                s->fill16(effect_ref(), &color_16, rect_ptr, m_world),
                "FillMatteSuite fill16 failed");
            return;
        } else {
            PF_Pixel8 color_8;
            color_8.alpha = (A_u_char)std::clamp(c_float.alpha * 255.0, 0.0, 255.0);
            color_8.red = (A_u_char)std::clamp(c_float.red * 255.0, 0.0, 255.0);
            color_8.green = (A_u_char)std::clamp(c_float.green * 255.0, 0.0, 255.0);
            color_8.blue = (A_u_char)std::clamp(c_float.blue * 255.0, 0.0, 255.0);
            aetk::core::check_err(s->fill(effect_ref(), &color_8, rect_ptr, m_world),
                "FillMatteSuite fill failed");
            return;
        }
    } catch (const std::exception&) {
        // Fallback to Tier 2
    }

    // Tier 2: Legacy host callbacks
    if (m_in_data && m_in_data->utils) {
        if (pf == PF_PixelFormat_ARGB64 && m_in_data->utils->fill16) {
            PF_Pixel16 color_16;
            color_16.alpha
                = (A_u_short)std::clamp(c_float.alpha * 32768.0, 0.0, 32768.0);
            color_16.red = (A_u_short)std::clamp(c_float.red * 32768.0, 0.0, 32768.0);
            color_16.green
                = (A_u_short)std::clamp(c_float.green * 32768.0, 0.0, 32768.0);
            color_16.blue
                = (A_u_short)std::clamp(c_float.blue * 32768.0, 0.0, 32768.0);
            PF_Err err = m_in_data->utils->fill16(
                effect_ref(), &color_16, rect_ptr, m_world);
            if (err == PF_Err_NONE)
                return;
        } else if (pf != PF_PixelFormat_ARGB128 && m_in_data->utils->fill) {
            PF_Pixel8 color_8;
            color_8.alpha = (A_u_char)std::clamp(c_float.alpha * 255.0, 0.0, 255.0);
            color_8.red = (A_u_char)std::clamp(c_float.red * 255.0, 0.0, 255.0);
            color_8.green = (A_u_char)std::clamp(c_float.green * 255.0, 0.0, 255.0);
            color_8.blue = (A_u_char)std::clamp(c_float.blue * 255.0, 0.0, 255.0);
            PF_Err err
                = m_in_data->utils->fill(effect_ref(), &color_8, rect_ptr, m_world);
            if (err == PF_Err_NONE)
                return;
        }
    }

    // Tier 3: Manual pixel iteration fallback
    A_long start_y = r ? r->top : 0;
    A_long end_y = r ? r->bottom : height();
    A_long start_x = r ? r->left : 0;
    A_long end_x = r ? r->right : width();

    visit_pixel_format<Range>(
        pixel_format(), is_bgra(), [&]<typename PixelT, bool IsBGRA>() {
            for (A_long y = start_y; y < end_y; ++y) {
                auto* row = reinterpret_cast<char*>(m_world->data)
                    + (y * m_world->rowbytes);
                auto* px_row = reinterpret_cast<PixelT*>(row);
                for (A_long x = start_x; x < end_x; ++x) {
                    pixel_accessor<PixelT, IsBGRA, Range>::write(&px_row[x], c);
                }
            }
        });
}

inline void smart_world::convolve_to(smart_world& dest, const aetk::core::kernel_3x3& k,
    float unity_scale, const aetk::core::rect* area) {
    if (!m_world || !dest.ptr())
        return;
    require_in_data();

    PF_Rect area_pf;
    PF_Rect* area_ptr = nullptr;
    if (area) {
        area_pf = area->to_pf();
        area_ptr = &area_pf;
    }

    A_long ae_kernel[9];
    k.to_ae_fixed(ae_kernel, unity_scale);

    aetk::core::suite<PF_WorldTransformSuite1> s(
        ::aetk::core::context::get_basic_suite());
    // Apply same kernel to all channels (A, R, G, B)
    aetk::core::check_err(s->convolve(effect_ref(), m_world, area_ptr,
                              PF_KernelFlag_2D | PF_KernelFlag_CLAMP, 3, ae_kernel,
                              ae_kernel, ae_kernel, ae_kernel, dest.ptr()),
        "WorldTransformSuite convolve failed");
}

inline void smart_world::blend_with(
    const smart_world& other, float amount, const aetk::core::rect* area) {
    if (!m_world || !other.ptr())
        return;
    require_in_data();

    PF_Rect area_pf;
    PF_Rect* area_ptr = nullptr;
    if (area) {
        area_pf = area->to_pf();
        area_ptr = &area_pf;
    }

    aetk::core::check_err(m_in_data->utils->blend(effect_ref(), m_world, other.ptr(),
                              FLOAT2FIX(amount), m_world),
        "Legacy blend failed");
}

inline smart_world smart_world::clone() const {
    if (!m_world)
        return { };

    short bitdepth = 8;
    PF_PixelFormat pf = pixel_format();
    if (pf == PF_PixelFormat_ARGB64)
        bitdepth = 16;
    else if (pf == PF_PixelFormat_ARGB128)
        bitdepth = 32;

    device_kind dev = is_gpu() ? device_kind::cuda : device_kind::cpu;
    smart_world dst(m_in_data, width(), height(), bitdepth, false, dev);

    // Copy pixels
    const_cast<smart_world*>(this)->copy_to(dst);

    return dst;
}

} // namespace aetk::effect
