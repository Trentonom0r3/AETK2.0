#pragma once

#include <AE_Effect.h>
#include <AE_EffectCB.h>
#include <AE_EffectCBSuites.h>
#include <aetk/core/error.hpp>
#include <aetk/core/suite.hpp>
#include <aetk/core/types.hpp>
#include <algorithm>
#include <type_traits>


namespace aetk::effect::colorspaces {

struct hls_double {
    double h { 0.0 }; // [0.0, 360.0]
    double l { 0.0 }; // [0.0, 1.0]
    double s { 0.0 }; // [0.0, 1.0]
};

struct yiq_double {
    double y { 0.0 }; // [0.0, 1.0]
    double i { 0.0 }; // [-0.5957, 0.5957]
    double q { 0.0 }; // [-0.5226, 0.5226]
};

// Convert a raw pixel to aetk::core::color<>
template <typename PixelT> inline aetk::core::color<> pixel_to_color(const PixelT* px) {
    if constexpr (std::is_same_v<PixelT, PF_PixelFloat>) {
        return aetk::core::color<>(px->alpha, px->red, px->green, px->blue);
    } else if constexpr (std::is_same_v<PixelT, PF_Pixel16>) {
        constexpr double inv_32768 = 1.0 / 32768.0;
        return aetk::core::color<>(px->alpha * inv_32768, px->red * inv_32768,
            px->green * inv_32768, px->blue * inv_32768);
    } else {
        constexpr double inv_255 = 1.0 / 255.0;
        return aetk::core::color<>(
            px->alpha * inv_255, px->red * inv_255, px->green * inv_255, px->blue * inv_255);
    }
}

// Convert aetk::core::color<> to raw pixel
template <typename PixelT> inline PixelT color_to_pixel(const aetk::core::color<>& c) {
    PixelT px { };
    if constexpr (std::is_same_v<PixelT, PF_PixelFloat>) {
        px.alpha = (float)c.alpha;
        px.red = (float)c.red;
        px.green = (float)c.green;
        px.blue = (float)c.blue;
    } else if constexpr (std::is_same_v<PixelT, PF_Pixel16>) {
        px.alpha = (A_u_short)std::clamp(c.alpha * 32768.0, 0.0, 32768.0);
        px.red = (A_u_short)std::clamp(c.red * 32768.0, 0.0, 32768.0);
        px.green = (A_u_short)std::clamp(c.green * 32768.0, 0.0, 32768.0);
        px.blue = (A_u_short)std::clamp(c.blue * 32768.0, 0.0, 32768.0);
    } else {
        px.alpha = (A_u_char)std::clamp(c.alpha * 255.0, 0.0, 255.0);
        px.red = (A_u_char)std::clamp(c.red * 255.0, 0.0, 255.0);
        px.green = (A_u_char)std::clamp(c.green * 255.0, 0.0, 255.0);
        px.blue = (A_u_char)std::clamp(c.blue * 255.0, 0.0, 255.0);
    }
    return px;
}

/**
 * @brief Converts RGB to HLS space, adhering to AE's bit depth differences.
 *
 * Normalizes the result to:
 * - h: [0.0, 360.0]
 * - l: [0.0, 1.0]
 * - s: [0.0, 1.0]
 */
template <typename PixelT>
inline PF_Err rgb_to_hls(PF_InData* in_data, const PixelT* rgb, hls_double& hls) {
    if (in_data->appl_id == 'PrMr') {
        // Under Premiere Pro, color suites are missing. Use C++ math fallback.
        aetk::core::color<> col = pixel_to_color(rgb);
        double h_norm = 0.0;
        col.to_hsl(h_norm, hls.s, hls.l);
        hls.h = h_norm * 360.0;
        return PF_Err_NONE;
    }

    PF_HLS_Pixel raw_hls { };
    PF_Err err = PF_Err_NONE;

    constexpr double inv_65536 = 1.0 / 65536.0;
    if constexpr (std::is_same_v<PixelT, PF_PixelFloat>) {
        aetk::core::suite<PF_ColorCallbacksFloatSuite1> s(in_data->pica_basicP);
        err = s->RGBtoHLS(in_data->effect_ref, const_cast<PF_PixelFloat*>(rgb), raw_hls);
        if (err == PF_Err_NONE) {
            hls.h = raw_hls[0] * inv_65536;
            hls.l = raw_hls[1] * inv_65536;
            hls.s = raw_hls[2] * inv_65536;
        }
    } else if constexpr (std::is_same_v<PixelT, PF_Pixel16>) {
        aetk::core::suite<PF_ColorCallbacks16Suite1> s(in_data->pica_basicP);
        err = s->RGBtoHLS(in_data->effect_ref, const_cast<PF_Pixel16*>(rgb), raw_hls);
        if (err == PF_Err_NONE) {
            constexpr double scale_h = inv_65536 * (360.0 / 255.0);
            constexpr double scale_l_s = inv_65536 / 32768.0;
            hls.h = raw_hls[0] * scale_h;
            hls.l = raw_hls[1] * scale_l_s;
            hls.s = raw_hls[2] * scale_l_s;
        }
    } else {
        // 8-bit fallback
        err = (*in_data->utils->colorCB.RGBtoHLS)(
            in_data->effect_ref, const_cast<PF_Pixel*>(rgb), raw_hls);
        if (err == PF_Err_NONE) {
            constexpr double scale_h = inv_65536 * (360.0 / 255.0);
            constexpr double scale_l_s = inv_65536 / 255.0;
            hls.h = raw_hls[0] * scale_h;
            hls.l = raw_hls[1] * scale_l_s;
            hls.s = raw_hls[2] * scale_l_s;
        }
    }

    return err;
}

/**
 * @brief Converts HLS space to RGB, adhering to AE's bit depth differences.
 *
 * Input HLS should be normalized to:
 * - h: [0.0, 360.0]
 * - l: [0.0, 1.0]
 * - s: [0.0, 1.0]
 */
template <typename PixelT>
inline PF_Err hls_to_rgb(PF_InData* in_data, const hls_double& hls, PixelT* rgb) {
    if (in_data->appl_id == 'PrMr') {
        // Premiere Pro fallback
        double h_norm = hls.h / 360.0;
        aetk::core::color<> col = aetk::core::color<>::from_hsl(h_norm, hls.s, hls.l, 1.0);
        *rgb = color_to_pixel<PixelT>(col);
        return PF_Err_NONE;
    }

    PF_HLS_Pixel raw_hls { };
    PF_Err err = PF_Err_NONE;

    if constexpr (std::is_same_v<PixelT, PF_PixelFloat>) {
        raw_hls[0] = static_cast<PF_Fixed>(hls.h * 65536.0);
        raw_hls[1] = static_cast<PF_Fixed>(hls.l * 65536.0);
        raw_hls[2] = static_cast<PF_Fixed>(hls.s * 65536.0);

        aetk::core::suite<PF_ColorCallbacksFloatSuite1> s(in_data->pica_basicP);
        err = s->HLStoRGB(in_data->effect_ref, raw_hls, rgb);

    } else if constexpr (std::is_same_v<PixelT, PF_Pixel16>) {
        raw_hls[0] = static_cast<PF_Fixed>((hls.h * (255.0 / 360.0)) * 65536.0);
        raw_hls[1] = static_cast<PF_Fixed>((hls.l * 32768.0) * 65536.0);
        raw_hls[2] = static_cast<PF_Fixed>((hls.s * 32768.0) * 65536.0);

        aetk::core::suite<PF_ColorCallbacks16Suite1> s(in_data->pica_basicP);
        err = s->HLStoRGB(in_data->effect_ref, raw_hls, rgb);

    } else {
        // 8-bit
        raw_hls[0] = static_cast<PF_Fixed>((hls.h * (255.0 / 360.0)) * 65536.0);
        raw_hls[1] = static_cast<PF_Fixed>((hls.l * 255.0) * 65536.0);
        raw_hls[2] = static_cast<PF_Fixed>((hls.s * 255.0) * 65536.0);

        err = (*in_data->utils->colorCB.HLStoRGB)(in_data->effect_ref, raw_hls, rgb);
    }

    if (err != PF_Err_NONE) {
        // CPU Fallback on error
        double h_norm = hls.h / 360.0;
        aetk::core::color<> col = aetk::core::color<>::from_hsl(h_norm, hls.s, hls.l, 1.0);
        *rgb = color_to_pixel<PixelT>(col);
        err = PF_Err_NONE;
    }

    return err;
}

/**
 * @brief Converts RGB to YIQ space.
 */
template <typename PixelT>
inline PF_Err rgb_to_yiq(PF_InData* in_data, const PixelT* rgb, yiq_double& yiq) {
    if (in_data->appl_id == 'PrMr') {
        aetk::core::color<> col = pixel_to_color(rgb);
        col.to_yiq(yiq.y, yiq.i, yiq.q);
        return PF_Err_NONE;
    }

    PF_YIQ_Pixel raw_yiq { };
    PF_Err err = PF_Err_NONE;

    constexpr double inv_65536 = 1.0 / 65536.0;
    if constexpr (std::is_same_v<PixelT, PF_PixelFloat>) {
        aetk::core::suite<PF_ColorCallbacksFloatSuite1> s(in_data->pica_basicP);
        err = s->RGBtoYIQ(in_data->effect_ref, const_cast<PF_PixelFloat*>(rgb), raw_yiq);
        if (err == PF_Err_NONE) {
            yiq.y = raw_yiq[0] * inv_65536;
            yiq.i = raw_yiq[1] * inv_65536;
            yiq.q = raw_yiq[2] * inv_65536;
        }
    } else if constexpr (std::is_same_v<PixelT, PF_Pixel16>) {
        aetk::core::suite<PF_ColorCallbacks16Suite1> s(in_data->pica_basicP);
        err = s->RGBtoYIQ(in_data->effect_ref, const_cast<PF_Pixel16*>(rgb), raw_yiq);
        if (err == PF_Err_NONE) {
            yiq.y = raw_yiq[0] * inv_65536;
            yiq.i = raw_yiq[1] * inv_65536;
            yiq.q = raw_yiq[2] * inv_65536;
        }
    } else {
        err = (*in_data->utils->colorCB.RGBtoYIQ)(
            in_data->effect_ref, const_cast<PF_Pixel*>(rgb), raw_yiq);
        if (err == PF_Err_NONE) {
            yiq.y = raw_yiq[0] * inv_65536;
            yiq.i = raw_yiq[1] * inv_65536;
            yiq.q = raw_yiq[2] * inv_65536;
        }
    }

    return err;
}

/**
 * @brief Converts YIQ space to RGB.
 */
template <typename PixelT>
inline PF_Err yiq_to_rgb(PF_InData* in_data, const yiq_double& yiq, PixelT* rgb) {
    if (in_data->appl_id == 'PrMr') {
        aetk::core::color<> col = aetk::core::color<>::from_yiq(yiq.y, yiq.i, yiq.q, 1.0);
        *rgb = color_to_pixel<PixelT>(col);
        return PF_Err_NONE;
    }

    PF_YIQ_Pixel raw_yiq { };
    PF_Err err = PF_Err_NONE;

    raw_yiq[0] = static_cast<PF_Fixed>(yiq.y * 65536.0);
    raw_yiq[1] = static_cast<PF_Fixed>(yiq.i * 65536.0);
    raw_yiq[2] = static_cast<PF_Fixed>(yiq.q * 65536.0);

    if constexpr (std::is_same_v<PixelT, PF_PixelFloat>) {
        aetk::core::suite<PF_ColorCallbacksFloatSuite1> s(in_data->pica_basicP);
        err = s->YIQtoRGB(in_data->effect_ref, raw_yiq, rgb);

    } else if constexpr (std::is_same_v<PixelT, PF_Pixel16>) {
        aetk::core::suite<PF_ColorCallbacks16Suite1> s(in_data->pica_basicP);
        err = s->YIQtoRGB(in_data->effect_ref, raw_yiq, rgb);

    } else {
        err = (*in_data->utils->colorCB.YIQtoRGB)(in_data->effect_ref, raw_yiq, rgb);
    }

    if (err != PF_Err_NONE) {
        aetk::core::color<> col = aetk::core::color<>::from_yiq(yiq.y, yiq.i, yiq.q, 1.0);
        *rgb = color_to_pixel<PixelT>(col);
        err = PF_Err_NONE;
    }

    return err;
}

/**
 * @brief Retrieves the normalized luminance [0.0, 1.0] of a pixel.
 */
template <typename PixelT>
inline double get_luminance(PF_InData* in_data, const PixelT* rgb) {
    if (in_data->appl_id == 'PrMr') {
        // Pure math luminance fallback for Premiere
        aetk::core::color<> col = pixel_to_color(rgb);
        return col.red * 0.2989 + col.green * 0.5866 + col.blue * 0.1144;
    }

    try {
        if constexpr (std::is_same_v<PixelT, PF_PixelFloat>) {
            float lum = 0.0f;
            aetk::core::suite<PF_ColorCallbacksFloatSuite1> s(in_data->pica_basicP);
            aetk::core::check_err(
                s->Luminance(in_data->effect_ref, const_cast<PF_PixelFloat*>(rgb), &lum));
            return (double)lum;
        } else if constexpr (std::is_same_v<PixelT, PF_Pixel16>) {
            A_long lum100 = 0;
            aetk::core::suite<PF_ColorCallbacks16Suite1> s(in_data->pica_basicP);
            aetk::core::check_err(
                s->Luminance(in_data->effect_ref, const_cast<PF_Pixel16*>(rgb), &lum100));
            return lum100 * 0.01;
        } else {
            A_long lum100 = 0;
            aetk::core::check_err((*in_data->utils->colorCB.Luminance)(
                in_data->effect_ref, const_cast<PF_Pixel*>(rgb), &lum100));
            return lum100 * 0.01;
        }
    } catch (const std::exception&) {
        aetk::core::color<> col = pixel_to_color(rgb);
        return col.red * 0.2989 + col.green * 0.5866 + col.blue * 0.1144;
    }
}

} // namespace aetk::effect::colorspaces
