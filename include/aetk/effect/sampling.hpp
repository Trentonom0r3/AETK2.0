#pragma once

#include <AE_Effect.h>
#include <AE_EffectCB.h>
#include <AE_EffectCBSuites.h>
#include <aetk/core/error.hpp>
#include <aetk/core/suite.hpp>
#include <aetk/core/types.hpp>
#include <aetk/effect/pixel/accessor.hpp>
#include <cstring>
#include <type_traits>


namespace aetk::effect {

// Helper to convert aetk::core::color<> back to a raw pixel structure
template <typename PixelT, bool IsBGRA>
inline PixelT color_to_pixel(const aetk::core::color<>& c) {
    PixelT px { };
    pixel_accessor<PixelT, IsBGRA>::write(&px, c);
    return px;
}

/**
 * @brief RAII session for optimized host subpixel/area sampling.
 *
 * Automatically manages begin/end sampling lifecycles and provides fallbacks
 * for environments without sampling suites (e.g., Premiere Pro).
 */
class sampling_session {
public:
    /**
     * @brief Begins a sampling session on the specified source world.
     */
    sampling_session(PF_InData* in_data, PF_EffectWorld* src,
        PF_Quality quality = PF_Quality_HI,
        PF_SampleEdgeBehav edge_behavior = PF_SampleEdgeBehav_ZERO)
        : m_in_data(in_data)
        , m_quality(quality)
        , m_src(src) {
        std::memset(&m_pb, 0, sizeof(m_pb));
        m_pb.src = src;
        m_pb.samp_behave = edge_behavior;
        m_active = false;

        // Check if we are running in Premiere Pro
        bool is_premiere = (in_data->appl_id == 'PrMr');
        if (!is_premiere && in_data->utils && in_data->utils->begin_sampling) {
            PF_Err err = (*m_in_data->utils->begin_sampling)(
                m_in_data->effect_ref, m_quality, PF_MF_Alpha_STRAIGHT, &m_pb);
            if (err == PF_Err_NONE) {
                m_active = true;
            }
        }
    }

    /**
     * @brief Destructor that ends the sampling session if it was successfully started.
     */
    ~sampling_session() {
        if (m_active) {
            (*m_in_data->utils->end_sampling)(
                m_in_data->effect_ref, m_quality, PF_MF_Alpha_STRAIGHT, &m_pb);
        }
    }

    // Move-only container semantics
    sampling_session(const sampling_session&) = delete;
    sampling_session& operator=(const sampling_session&) = delete;

    sampling_session(sampling_session&& other) noexcept
        : m_in_data(other.m_in_data)
        , m_quality(other.m_quality)
        , m_src(other.m_src)
        , m_pb(other.m_pb)
        , m_active(other.m_active) {
        other.m_active = false;
    }

    sampling_session& operator=(sampling_session&& other) noexcept {
        if (this != &other) {
            if (m_active) {
                (*m_in_data->utils->end_sampling)(
                    m_in_data->effect_ref, m_quality, PF_MF_Alpha_STRAIGHT, &m_pb);
            }
            m_in_data = other.m_in_data;
            m_quality = other.m_quality;
            m_src = other.m_src;
            m_pb = other.m_pb;
            m_active = other.m_active;
            other.m_active = false;
        }
        return *this;
    }

    /**
     * @brief Samples a subpixel location (bilinear in high quality, nearest-neighbor in
     * low quality).
     */
    template <typename PixelT> PixelT sample(double x, double y) {
        if (m_active) {
            PixelT dst_px { };
            PF_Fixed fx = static_cast<PF_Fixed>(x * 65536.0);
            PF_Fixed fy = static_cast<PF_Fixed>(y * 65536.0);

            if constexpr (std::is_same_v<PixelT, PF_PixelFloat>) {
                aetk::core::suite<PF_SamplingFloatSuite1> s(
                    ::aetk::core::context::get_basic_suite());
                aetk::core::check_err(s->subpixel_sample_float(
                    m_in_data->effect_ref, fx, fy, &m_pb, &dst_px));
            } else if constexpr (std::is_same_v<PixelT, PF_Pixel16>) {
                aetk::core::check_err((*m_in_data->utils->subpixel_sample16)(
                    m_in_data->effect_ref, fx, fy, &m_pb, &dst_px));
            } else {
                aetk::core::check_err((*m_in_data->utils->subpixel_sample)(
                    m_in_data->effect_ref, fx, fy, &m_pb, &dst_px));
            }
            return dst_px;
        }

        // Manual fallback (for Premiere Pro or if host calls failed)
        bool is_bgra = (m_in_data->appl_id == 'PrMr');
        if (is_bgra) {
            return color_to_pixel<PixelT, true>(sample_bilinear_fallback(x, y));
        } else {
            return color_to_pixel<PixelT, false>(sample_bilinear_fallback(x, y));
        }
    }

    /**
     * @brief Samples an area (area-averaged in high quality, nearest-neighbor in low
     * quality).
     */
    template <typename PixelT>
    PixelT sample_area(double x, double y, double x_radius, double y_radius) {
        if (m_active) {
            PixelT dst_px { };
            PF_Fixed fx = static_cast<PF_Fixed>(x * 65536.0);
            PF_Fixed fy = static_cast<PF_Fixed>(y * 65536.0);

            // Update radii in parameter block
            m_pb.x_radius = static_cast<PF_Fixed>(x_radius * 65536.0);
            m_pb.y_radius = static_cast<PF_Fixed>(y_radius * 65536.0);
            m_pb.area
                = static_cast<PF_Fixed>((x_radius * 2.0) * (y_radius * 2.0) * 65536.0);

            if constexpr (std::is_same_v<PixelT, PF_PixelFloat>) {
                aetk::core::suite<PF_SamplingFloatSuite1> s(
                    ::aetk::core::context::get_basic_suite());
                aetk::core::check_err(
                    s->area_sample_float(m_in_data->effect_ref, fx, fy, &m_pb, &dst_px));
            } else if constexpr (std::is_same_v<PixelT, PF_Pixel16>) {
                aetk::core::check_err((*m_in_data->utils->area_sample16)(
                    m_in_data->effect_ref, fx, fy, &m_pb, &dst_px));
            } else {
                aetk::core::check_err((*m_in_data->utils->area_sample)(
                    m_in_data->effect_ref, fx, fy, &m_pb, &dst_px));
            }
            return dst_px;
        }

        // Manual fallback
        bool is_bgra = (m_in_data->appl_id == 'PrMr');
        if (is_bgra) {
            return color_to_pixel<PixelT, true>(sample_bilinear_fallback(x, y));
        } else {
            return color_to_pixel<PixelT, false>(sample_bilinear_fallback(x, y));
        }
    }

private:
    aetk::core::color<> sample_bilinear_fallback(double x, double y) const {
        if (!m_src || !m_src->data)
            return { };

        int x0 = (int)std::floor(x);
        int y0 = (int)std::floor(y);
        float tx = (float)(x - x0);
        float ty = (float)(y - y0);

        auto c00 = get_pixel_fallback(x0, y0);
        auto c10 = get_pixel_fallback(x0 + 1, y0);
        auto c01 = get_pixel_fallback(x0, y0 + 1);
        auto c11 = get_pixel_fallback(x0 + 1, y0 + 1);

        return aetk::core::color<>::mix(aetk::core::color<>::mix(c00, c10, tx),
            aetk::core::color<>::mix(c01, c11, tx), ty);
    }

    aetk::core::color<> get_pixel_fallback(int x, int y) const {
        if (!m_src || !m_src->data)
            return { };

        x = std::clamp(x, 0, (int)m_src->width - 1);
        y = std::clamp(y, 0, (int)m_src->height - 1);

        const char* row
            = reinterpret_cast<const char*>(m_src->data) + (y * m_src->rowbytes);

        // Attempt format detection
        PF_PixelFormat pf = PF_PixelFormat_INVALID;
        aetk::core::suite<PF_WorldSuite2> world_suite(
            ::aetk::core::context::get_basic_suite());
        if (world_suite->PF_GetPixelFormat(m_src, &pf) != PF_Err_NONE) {
            pf = PF_PixelFormat_INVALID;
        }

        if (pf == PF_PixelFormat_INVALID) {
            aetk::core::suite<PF_PixelFormatSuite1> pfmt_suite(
                ::aetk::core::context::get_basic_suite(), kPFPixelFormatSuite,
                kPFPixelFormatSuiteVersion1);
            PrPixelFormat pr_format = PrPixelFormat_Invalid;
            if (pfmt_suite->GetPixelFormat(m_src, &pr_format) == PF_Err_NONE) {
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
                    pf = PF_PixelFormat_ARGB128;
                } else if (pr_format == PrPixelFormat_BGRA_4444_16u
                    || pr_format == PrPixelFormat_ARGB_4444_16u
                    || pr_format == PrPixelFormat_BGRP_4444_16u
                    || pr_format == PrPixelFormat_PRGB_4444_16u
                    || pr_format == PrPixelFormat_BGRX_4444_16u) {
                    pf = PF_PixelFormat_ARGB64;
                }
            }
        }

        if (pf == PF_PixelFormat_INVALID) {
            pf = PF_PixelFormat_ARGB32;
        }

        bool is_bgra = (m_in_data->appl_id == 'PrMr');

        return visit_pixel_format(pf, is_bgra, [&]<typename PixelT, bool IsBGRA>() {
            const auto* px = reinterpret_cast<const PixelT*>(row) + x;
            return pixel_accessor<PixelT, IsBGRA>::read(px);
        });
    }

    PF_InData* m_in_data;
    PF_Quality m_quality;
    PF_EffectWorld* m_src;
    PF_SampPB m_pb;
    bool m_active { false };
};

} // namespace aetk::effect
