#pragma once

#include <aetk/core/types.hpp>
#include <algorithm>
#include <type_traits>

namespace aetk::effect {
using aetk::core::pixel_range;

/**
 * @brief Compile-time pixel accessor traits for unified pixel read/write transactions.
 *
 * @tparam PixelT Raw pixel struct type (e.g. PF_Pixel8, PF_Pixel16, PF_PixelFloat).
 * @tparam IsBGRA Host byte layout option (true for Premiere Pro BGRA timeline, false for AE ARGB).
 * @tparam Range The pixel range policy (tkfloat: 0.0..1.0, tkuint8: 0.0..255.0).
 */
template <typename PixelT, bool IsBGRA, pixel_range Range = pixel_range::tkfloat>
struct pixel_accessor;

/**
 * @brief pixel_accessor specialization for 8-bit channels.
 */
template <bool IsBGRA, pixel_range Range>
struct pixel_accessor<PF_Pixel8, IsBGRA, Range> {
    using channel_type = A_u_char;
    static constexpr float max_val = 255.0f;
    static constexpr float inv_max_val = 1.0f / 255.0f;
    static constexpr bool swizzle = IsBGRA;

    /**
     * @brief Read a pixel as a color under the specified Range policy.
     */
    static inline aetk::core::color<Range> read(const PF_Pixel8* px) {
        if constexpr (Range == pixel_range::tkfloat) {
            if constexpr (swizzle) {
                return aetk::core::color<Range>{ px->blue * inv_max_val, px->green * inv_max_val, px->red * inv_max_val, px->alpha * inv_max_val };
            } else {
                return aetk::core::color<Range>{ px->alpha * inv_max_val, px->red * inv_max_val, px->green * inv_max_val, px->blue * inv_max_val };
            }
        } else {
            if constexpr (swizzle) {
                return aetk::core::color<Range>{ static_cast<double>(px->blue), static_cast<double>(px->green), static_cast<double>(px->red), static_cast<double>(px->alpha) };
            } else {
                return aetk::core::color<Range>{ static_cast<double>(px->alpha), static_cast<double>(px->red), static_cast<double>(px->green), static_cast<double>(px->blue) };
            }
        }
    }

    /**
     * @brief Write a color under the specified Range policy to a pixel.
     */
    static inline void write(PF_Pixel8* px, const aetk::core::color<Range>& c) {
        if constexpr (Range == pixel_range::tkfloat) {
            if constexpr (swizzle) {
                px->blue  = static_cast<A_u_char>(std::clamp(c.alpha * max_val, 0.0, (double)max_val));
                px->green = static_cast<A_u_char>(std::clamp(c.red * max_val, 0.0, (double)max_val));
                px->red   = static_cast<A_u_char>(std::clamp(c.green * max_val, 0.0, (double)max_val));
                px->alpha = static_cast<A_u_char>(std::clamp(c.blue * max_val, 0.0, (double)max_val));
            } else {
                px->alpha = static_cast<A_u_char>(std::clamp(c.alpha * max_val, 0.0, (double)max_val));
                px->red   = static_cast<A_u_char>(std::clamp(c.red * max_val, 0.0, (double)max_val));
                px->green = static_cast<A_u_char>(std::clamp(c.green * max_val, 0.0, (double)max_val));
                px->blue  = static_cast<A_u_char>(std::clamp(c.blue * max_val, 0.0, (double)max_val));
            }
        } else {
            if constexpr (swizzle) {
                px->blue  = static_cast<A_u_char>(std::clamp(c.alpha, 0.0, (double)max_val));
                px->green = static_cast<A_u_char>(std::clamp(c.red, 0.0, (double)max_val));
                px->red   = static_cast<A_u_char>(std::clamp(c.green, 0.0, (double)max_val));
                px->alpha = static_cast<A_u_char>(std::clamp(c.blue, 0.0, (double)max_val));
            } else {
                px->alpha = static_cast<A_u_char>(std::clamp(c.alpha, 0.0, (double)max_val));
                px->red   = static_cast<A_u_char>(std::clamp(c.red, 0.0, (double)max_val));
                px->green = static_cast<A_u_char>(std::clamp(c.green, 0.0, (double)max_val));
                px->blue  = static_cast<A_u_char>(std::clamp(c.blue, 0.0, (double)max_val));
            }
        }
    }

    struct blender {
        int a_val;
        int inv_a;
        int pre_r;
        int pre_g;
        int pre_b;
        int pre_a;

        inline blender(const aetk::core::color<Range>& color) {
            if constexpr (Range == pixel_range::tkfloat) {
                a_val = static_cast<int>(color.alpha * 255.0);
                inv_a = 255 - a_val;
                pre_r = static_cast<int>(color.red * 255.0) * a_val;
                pre_g = static_cast<int>(color.green * 255.0) * a_val;
                pre_b = static_cast<int>(color.blue * 255.0) * a_val;
                pre_a = 255 * a_val;
            } else {
                a_val = static_cast<int>(color.alpha);
                inv_a = 255 - a_val;
                pre_r = static_cast<int>(color.red) * a_val;
                pre_g = static_cast<int>(color.green) * a_val;
                pre_b = static_cast<int>(color.blue) * a_val;
                pre_a = 255 * a_val;
            }
        }

        inline void blend(PF_Pixel8* px) const {
            if constexpr (swizzle) {
                px->blue  = (pre_a + px->blue  * inv_a) / 255;
                px->green = (pre_r + px->green * inv_a) / 255;
                px->red   = (pre_g + px->red   * inv_a) / 255;
                px->alpha = (pre_b + px->alpha * inv_a) / 255;
            } else {
                px->alpha = (pre_a + px->alpha * inv_a) / 255;
                px->red   = (pre_r + px->red   * inv_a) / 255;
                px->green = (pre_g + px->green * inv_a) / 255;
                px->blue  = (pre_b + px->blue  * inv_a) / 255;
            }
        }
    };
};

/**
 * @brief pixel_accessor specialization for 16-bit channels.
 */
template <bool IsBGRA, pixel_range Range>
struct pixel_accessor<PF_Pixel16, IsBGRA, Range> {
    using channel_type = A_u_short;
    static constexpr float max_val = 32768.0f;
    static constexpr float inv_max_val = 1.0f / 32768.0f;
    static constexpr bool swizzle = IsBGRA;

    /**
     * @brief Read a pixel as a color under the specified Range policy.
     */
    static inline aetk::core::color<Range> read(const PF_Pixel16* px) {
        if constexpr (Range == pixel_range::tkfloat) {
            if constexpr (swizzle) {
                return aetk::core::color<Range>{ px->blue * inv_max_val, px->green * inv_max_val, px->red * inv_max_val, px->alpha * inv_max_val };
            } else {
                return aetk::core::color<Range>{ px->alpha * inv_max_val, px->red * inv_max_val, px->green * inv_max_val, px->blue * inv_max_val };
            }
        } else {
            // Range is tkuint8 (0..255).
            // Convert 0..32768 range to 0..255 using optimized bitwise operations:
            // ((x << 8) - x) >> 15
            if constexpr (swizzle) {
                double b = static_cast<double>(((px->blue  << 8) - px->blue)  >> 15);
                double g = static_cast<double>(((px->green << 8) - px->green) >> 15);
                double r = static_cast<double>(((px->red   << 8) - px->red)   >> 15);
                double a = static_cast<double>(((px->alpha << 8) - px->alpha) >> 15);
                return aetk::core::color<Range>{ b, g, r, a };
            } else {
                double a = static_cast<double>(((px->alpha << 8) - px->alpha) >> 15);
                double r = static_cast<double>(((px->red   << 8) - px->red)   >> 15);
                double g = static_cast<double>(((px->green << 8) - px->green) >> 15);
                double b = static_cast<double>(((px->blue  << 8) - px->blue)  >> 15);
                return aetk::core::color<Range>{ a, r, g, b };
            }
        }
    }

    /**
     * @brief Write a color under the specified Range policy to a pixel.
     */
    static inline void write(PF_Pixel16* px, const aetk::core::color<Range>& c) {
        if constexpr (Range == pixel_range::tkfloat) {
            if constexpr (swizzle) {
                px->blue  = static_cast<A_u_short>(std::clamp(c.alpha * max_val, 0.0, (double)max_val));
                px->green = static_cast<A_u_short>(std::clamp(c.red * max_val, 0.0, (double)max_val));
                px->red   = static_cast<A_u_short>(std::clamp(c.green * max_val, 0.0, (double)max_val));
                px->alpha = static_cast<A_u_short>(std::clamp(c.blue * max_val, 0.0, (double)max_val));
            } else {
                px->alpha = static_cast<A_u_short>(std::clamp(c.alpha * max_val, 0.0, (double)max_val));
                px->red   = static_cast<A_u_short>(std::clamp(c.red * max_val, 0.0, (double)max_val));
                px->green = static_cast<A_u_short>(std::clamp(c.green * max_val, 0.0, (double)max_val));
                px->blue  = static_cast<A_u_short>(std::clamp(c.blue * max_val, 0.0, (double)max_val));
            }
        } else {
            // Range is tkuint8 (0..255).
            // Convert 0..255 range to 0..32768 using optimized bitwise operations:
            // (x << 7) + (x >> 1) + (x >> 7)
            auto clamp_and_convert = [](double val) -> A_u_short {
                int x = static_cast<int>(std::clamp(val, 0.0, 255.0));
                return static_cast<A_u_short>((x << 7) + (x >> 1) + (x >> 7));
            };

            if constexpr (swizzle) {
                px->blue  = clamp_and_convert(c.alpha);
                px->green = clamp_and_convert(c.red);
                px->red   = clamp_and_convert(c.green);
                px->alpha = clamp_and_convert(c.blue);
            } else {
                px->alpha = clamp_and_convert(c.alpha);
                px->red   = clamp_and_convert(c.red);
                px->green = clamp_and_convert(c.green);
                px->blue  = clamp_and_convert(c.blue);
            }
        }
    }

    struct blender {
        int a_val;
        int inv_a;
        int pre_r;
        int pre_g;
        int pre_b;
        int pre_a;

        inline blender(const aetk::core::color<Range>& color) {
            if constexpr (Range == pixel_range::tkfloat) {
                a_val = static_cast<int>(color.alpha * 32768.0);
                inv_a = 32768 - a_val;
                pre_r = static_cast<int>(color.red * 32768.0) * a_val;
                pre_g = static_cast<int>(color.green * 32768.0) * a_val;
                pre_b = static_cast<int>(color.blue * 32768.0) * a_val;
                pre_a = 32768 * a_val;
            } else {
                int a_8 = static_cast<int>(std::clamp(color.alpha, 0.0, 255.0));
                int r_8 = static_cast<int>(std::clamp(color.red, 0.0, 255.0));
                int g_8 = static_cast<int>(std::clamp(color.green, 0.0, 255.0));
                int b_8 = static_cast<int>(std::clamp(color.blue, 0.0, 255.0));

                int a_16 = (a_8 << 7) + (a_8 >> 1) + (a_8 >> 7);
                int r_16 = (r_8 << 7) + (r_8 >> 1) + (r_8 >> 7);
                int g_16 = (g_8 << 7) + (g_8 >> 1) + (g_8 >> 7);
                int b_16 = (b_8 << 7) + (b_8 >> 1) + (b_8 >> 7);

                a_val = a_16;
                inv_a = 32768 - a_val;
                pre_r = r_16 * a_val;
                pre_g = g_16 * a_val;
                pre_b = b_16 * a_val;
                pre_a = 32768 * a_val;
            }
        }

        inline void blend(PF_Pixel16* px) const {
            if constexpr (swizzle) {
                px->blue  = (pre_a + px->blue  * inv_a) >> 15;
                px->green = (pre_r + px->green * inv_a) >> 15;
                px->red   = (pre_g + px->red   * inv_a) >> 15;
                px->alpha = (pre_b + px->alpha * inv_a) >> 15;
            } else {
                px->alpha = (pre_a + px->alpha * inv_a) >> 15;
                px->red   = (pre_r + px->red   * inv_a) >> 15;
                px->green = (pre_g + px->green * inv_a) >> 15;
                px->blue  = (pre_b + px->blue  * inv_a) >> 15;
            }
        }
    };
};

/**
 * @brief pixel_accessor specialization for 32-bit float channels.
 */
template <bool IsBGRA, pixel_range Range>
struct pixel_accessor<PF_PixelFloat, IsBGRA, Range> {
    using channel_type = float;
    static constexpr float max_val = 1.0f;
    static constexpr float inv_max_val = 1.0f;
    static constexpr bool swizzle = IsBGRA;

    /**
     * @brief Read a pixel as a color under the specified Range policy.
     */
    static inline aetk::core::color<Range> read(const PF_PixelFloat* px) {
        if constexpr (Range == pixel_range::tkfloat) {
            if constexpr (swizzle) {
                return aetk::core::color<Range>{ px->blue, px->green, px->red, px->alpha };
            } else {
                return aetk::core::color<Range>{ px->alpha, px->red, px->green, px->blue };
            }
        } else {
            // Range is tkuint8 (0..255). Convert 0.0..1.0 floats to 0.0..255.0 color range.
            if constexpr (swizzle) {
                return aetk::core::color<Range>{ px->blue * 255.0, px->green * 255.0, px->red * 255.0, px->alpha * 255.0 };
            } else {
                return aetk::core::color<Range>{ px->alpha * 255.0, px->red * 255.0, px->green * 255.0, px->blue * 255.0 };
            }
        }
    }

    /**
     * @brief Write a color under the specified Range policy to a pixel.
     */
    static inline void write(PF_PixelFloat* px, const aetk::core::color<Range>& c) {
        if constexpr (Range == pixel_range::tkfloat) {
            if constexpr (swizzle) {
                px->blue  = static_cast<float>(c.alpha);
                px->green = static_cast<float>(c.red);
                px->red   = static_cast<float>(c.green);
                px->alpha = static_cast<float>(c.blue);
            } else {
                px->alpha = static_cast<float>(c.alpha);
                px->red   = static_cast<float>(c.red);
                px->green = static_cast<float>(c.green);
                px->blue  = static_cast<float>(c.blue);
            }
        } else {
            // Range is tkuint8 (0..255). Convert 0.0..255.0 color range back to 0.0..1.0 floats.
            static constexpr float scale = 1.0f / 255.0f;
            if constexpr (swizzle) {
                px->blue  = static_cast<float>(c.alpha * scale);
                px->green = static_cast<float>(c.red * scale);
                px->red   = static_cast<float>(c.green * scale);
                px->alpha = static_cast<float>(c.blue * scale);
            } else {
                px->alpha = static_cast<float>(c.alpha * scale);
                px->red   = static_cast<float>(c.red * scale);
                px->green = static_cast<float>(c.green * scale);
                px->blue  = static_cast<float>(c.blue * scale);
            }
        }
    }

    struct blender {
        float inv_a;
        float pre_r;
        float pre_g;
        float pre_b;
        float pre_a;

        inline blender(const aetk::core::color<Range>& color) {
            if constexpr (Range == pixel_range::tkfloat) {
                float src_a = static_cast<float>(color.alpha);
                inv_a = 1.0f - src_a;
                pre_r = static_cast<float>(color.red) * src_a;
                pre_g = static_cast<float>(color.green) * src_a;
                pre_b = static_cast<float>(color.blue) * src_a;
                pre_a = src_a;
            } else {
                float src_a = static_cast<float>(color.alpha * (1.0f / 255.0f));
                inv_a = 1.0f - src_a;
                pre_r = static_cast<float>(color.red * (1.0f / 255.0f)) * src_a;
                pre_g = static_cast<float>(color.green * (1.0f / 255.0f)) * src_a;
                pre_b = static_cast<float>(color.blue * (1.0f / 255.0f)) * src_a;
                pre_a = src_a;
            }
        }

        inline void blend(PF_PixelFloat* px) const {
            if constexpr (swizzle) {
                px->blue  = pre_a + inv_a * px->blue;
                px->green = pre_r + inv_a * px->green;
                px->red   = pre_g + inv_a * px->red;
                px->alpha = pre_b + inv_a * px->alpha;
            } else {
                px->alpha = pre_a + inv_a * px->alpha;
                px->red   = pre_r + inv_a * px->red;
                px->green = pre_g + inv_a * px->green;
                px->blue  = pre_b + inv_a * px->blue;
            }
        }
    };
};

/**
 * @brief RAII transaction wrapper for single-pixel or dual-pixel read-modify-write sequences.
 *
 * @tparam PixelT Raw pixel struct type.
 * @tparam IsBGRA Host byte layout option (true for Premiere Pro BGRA timeline, false for AE ARGB).
 * @tparam Range The pixel range policy (tkfloat: 0.0..1.0, tkuint8: 0.0..255.0).
 */
template <typename PixelT, bool IsBGRA, pixel_range Range = pixel_range::tkfloat>
struct pixel_transaction {
    using accessor = pixel_accessor<typename std::remove_const<PixelT>::type, IsBGRA, Range>;
    
    PixelT* m_px = nullptr;
    const PixelT* m_src_px = nullptr;
    PixelT* m_dst_px = nullptr;
    aetk::core::color<Range> color;

    // Single-pixel transaction (in-place)
    inline pixel_transaction(PixelT* px) 
        : m_px(px), color(accessor::read(px)) {}

    // Dual-pixel transfer transaction (read from src, write to dst)
    inline pixel_transaction(const PixelT* src_px, PixelT* dst_px) 
        : m_src_px(src_px), m_dst_px(dst_px), color(accessor::read(src_px)) {}
    
    inline ~pixel_transaction() {
        if (m_dst_px) {
            accessor::write(m_dst_px, color);
        } else if constexpr (!std::is_const<PixelT>::value) {
            if (m_px) {
                accessor::write(m_px, color);
            }
        }
    }

    pixel_transaction(const pixel_transaction&) = delete;
    pixel_transaction& operator=(const pixel_transaction&) = delete;
    pixel_transaction(pixel_transaction&&) = delete;
    pixel_transaction& operator=(pixel_transaction&&) = delete;
};

/**
 * @brief Double-dispatch visitor wrapper. Maps runtime format and layout enums to compile-time template parameters.
 */
template <pixel_range Range = pixel_range::tkfloat, typename Visitor>
inline auto visit_pixel_format(PF_PixelFormat format, bool is_bgra, Visitor&& visitor) {
    if (format == PF_PixelFormat_ARGB128) {
        if (is_bgra) return visitor.template operator()<PF_PixelFloat, true>();
        else         return visitor.template operator()<PF_PixelFloat, false>();
    } else if (format == PF_PixelFormat_ARGB64) {
        if (is_bgra) return visitor.template operator()<PF_Pixel16, true>();
        else         return visitor.template operator()<PF_Pixel16, false>();
    } else {
        if (is_bgra) return visitor.template operator()<PF_Pixel8, true>();
        else         return visitor.template operator()<PF_Pixel8, false>();
    }
}

} // namespace aetk::effect
