#pragma once

#include <AE_Effect.h>
#include <AE_EffectCB.h>
#include <AE_EffectCBSuites.h>
#include <AE_EffectPixelFormat.h>
#include <aetk/core/error.hpp>
#include <aetk/core/log.hpp>
#include <aetk/core/suite.hpp>
#include <aetk/core/types.hpp>
#include <aetk/effect/pixel/accessor.hpp>
#include <functional>
#include <type_traits>

namespace aetk::effect {
using aetk::core::pixel_range;

namespace detail {
    // Helper to wrap the lambda for PF_Iterate8Suite2
    template <typename PixelT, typename Func>
    static PF_Err iterate_wrapper(
        void* refcon, A_long x, A_long y, PF_Pixel8* in, PF_Pixel8* out) {
        auto* func = static_cast<Func*>(refcon);
        try {
            (*func)(x, y, reinterpret_cast<PixelT*>(in), reinterpret_cast<PixelT*>(out));
            return PF_Err_NONE;
        } catch (const std::exception& e) {
            AETK_ERROR("[iterate_wrapper] Exception at x={}, y={}: {}", x, y, e.what());
            return PF_Err_OUT_OF_MEMORY;
        }
    }

    template <typename PixelT, typename Func>
    static PF_Err iterate16_wrapper(
        void* refcon, A_long x, A_long y, PF_Pixel16* in, PF_Pixel16* out) {
        auto* func = static_cast<Func*>(refcon);
        try {
            (*func)(x, y, reinterpret_cast<PixelT*>(in), reinterpret_cast<PixelT*>(out));
            return PF_Err_NONE;
        } catch (const std::exception& e) {
            AETK_ERROR("[iterate16_wrapper] Exception at x={}, y={}: {}", x, y, e.what());
            return PF_Err_OUT_OF_MEMORY;
        }
    }

    template <typename PixelT, typename Func>
    static PF_Err iterate_float_wrapper(
        void* refcon, A_long x, A_long y, PF_PixelFloat* in, PF_PixelFloat* out) {
        auto* func = static_cast<Func*>(refcon);
        try {
            (*func)(x, y, reinterpret_cast<PixelT*>(in), reinterpret_cast<PixelT*>(out));
            return PF_Err_NONE;
        } catch (const std::exception& e) {
            AETK_ERROR(
                "[iterate_float_wrapper] Exception at x={}, y={}: {}", x, y, e.what());
            return PF_Err_OUT_OF_MEMORY;
        }
    }
} // namespace detail

/**
 * @brief Standalone pixel iterator utilizing host iteration suites.
 */
template <typename PixelT, typename Func>
inline void iterate(PF_InData* in_data, PF_EffectWorld* src, PF_EffectWorld* dst, Func&& func) {
    PF_PixelFormat format = PF_PixelFormat_INVALID;

    aetk::core::suite<PF_WorldSuite2> ws(::aetk::core::context::get_basic_suite());
    if (ws->PF_GetPixelFormat(src, &format) != PF_Err_NONE) {
        format = PF_PixelFormat_INVALID;
    }

    if (format == PF_PixelFormat_INVALID) {
        aetk::core::suite<PF_PixelFormatSuite1> pfmt_suite(
            ::aetk::core::context::get_basic_suite(), kPFPixelFormatSuite,
            kPFPixelFormatSuiteVersion1);
        PrPixelFormat pr_format = PrPixelFormat_Invalid;
        if (pfmt_suite->GetPixelFormat(src, &pr_format) == PF_Err_NONE) {
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
                format = PF_PixelFormat_ARGB128;
            } else if (pr_format == PrPixelFormat_BGRA_4444_16u
                || pr_format == PrPixelFormat_ARGB_4444_16u
                || pr_format == PrPixelFormat_BGRP_4444_16u
                || pr_format == PrPixelFormat_PRGB_4444_16u
                || pr_format == PrPixelFormat_BGRX_4444_16u) {
                format = PF_PixelFormat_ARGB64;
            }
        }
    }

    if (format == PF_PixelFormat_INVALID) {
        format = PF_PixelFormat_ARGB32;
    }

    if (format == PF_PixelFormat_ARGB128) {
        aetk::core::suite<PF_IterateFloatSuite2> s(
            ::aetk::core::context::get_basic_suite());
        ::aetk::core::check_err(s->iterate(in_data, 0, src->height, src, nullptr,
            &func, detail::iterate_float_wrapper<PixelT, Func>, dst));

    } else if (format == PF_PixelFormat_ARGB64) {
        aetk::core::suite<PF_Iterate16Suite2> s(
            ::aetk::core::context::get_basic_suite());
        ::aetk::core::check_err(s->iterate(in_data, 0, src->height, src, nullptr,
            &func, detail::iterate16_wrapper<PixelT, Func>, dst));

    } else {
        aetk::core::suite<PF_Iterate8Suite2> s(
            ::aetk::core::context::get_basic_suite());
        ::aetk::core::check_err(s->iterate(in_data, 0, src->height, src, nullptr,
            &func, detail::iterate_wrapper<PixelT, Func>, dst));
    }
}

/**
 * @brief Standalone pixel iterator with origin coordinate offset.
 */
template <typename PixelT, typename Func>
inline void iterate_origin(PF_InData* in_data, PF_EffectWorld* src, PF_EffectWorld* dst,
    const aetk::core::rect* area, const PF_Point* origin, Func&& func) {
    PF_PixelFormat format = PF_PixelFormat_INVALID;

    aetk::core::suite<PF_WorldSuite2> ws(::aetk::core::context::get_basic_suite());
    if (ws->PF_GetPixelFormat(src, &format) != PF_Err_NONE) {
        format = PF_PixelFormat_INVALID;
    }

    if (format == PF_PixelFormat_INVALID) {
        aetk::core::suite<PF_PixelFormatSuite1> pfmt_suite(
            ::aetk::core::context::get_basic_suite(), kPFPixelFormatSuite,
            kPFPixelFormatSuiteVersion1);
        PrPixelFormat pr_format = PrPixelFormat_Invalid;
        if (pfmt_suite->GetPixelFormat(src, &pr_format) == PF_Err_NONE) {
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
                format = PF_PixelFormat_ARGB128;
            } else if (pr_format == PrPixelFormat_BGRA_4444_16u
                || pr_format == PrPixelFormat_ARGB_4444_16u
                || pr_format == PrPixelFormat_BGRP_4444_16u
                || pr_format == PrPixelFormat_PRGB_4444_16u
                || pr_format == PrPixelFormat_BGRX_4444_16u) {
                format = PF_PixelFormat_ARGB64;
            }
        }
    }

    if (format == PF_PixelFormat_INVALID) {
        format = PF_PixelFormat_ARGB32;
    }

    PF_Rect raw_area { };
    if (area) {
        raw_area = area->to_pf();
    }
    const PF_Rect* area_ptr = area ? &raw_area : nullptr;

    if (format == PF_PixelFormat_ARGB128) {
        aetk::core::suite<PF_IterateFloatSuite2> s(
            ::aetk::core::context::get_basic_suite());
        ::aetk::core::check_err(
            s->iterate_origin(in_data, 0, src->height, src, area_ptr, origin, &func,
                detail::iterate_float_wrapper<PixelT, Func>, dst));

    } else if (format == PF_PixelFormat_ARGB64) {
        aetk::core::suite<PF_Iterate16Suite2> s(
            ::aetk::core::context::get_basic_suite());
        ::aetk::core::check_err(s->iterate_origin(in_data, 0, src->height, src,
            area_ptr, origin, &func, detail::iterate16_wrapper<PixelT, Func>, dst));

    } else {
        aetk::core::suite<PF_Iterate8Suite2> s(
            ::aetk::core::context::get_basic_suite());
        ::aetk::core::check_err(s->iterate_origin(in_data, 0, src->height, src,
            area_ptr, origin, &func, detail::iterate_wrapper<PixelT, Func>, dst));
    }
}

/**
 * @brief Standalone pixel iterator with non-clipping origin coordinate offset.
 */
template <typename PixelT, typename Func>
inline void iterate_origin_non_clip(PF_InData* in_data, PF_EffectWorld* src, PF_EffectWorld* dst,
    const aetk::core::rect* area, const PF_Point* origin, Func&& func) {
    PF_PixelFormat format = PF_PixelFormat_INVALID;

    aetk::core::suite<PF_WorldSuite2> ws(::aetk::core::context::get_basic_suite());
    if (ws->PF_GetPixelFormat(src, &format) != PF_Err_NONE) {
        format = PF_PixelFormat_INVALID;
    }

    if (format == PF_PixelFormat_INVALID) {
        aetk::core::suite<PF_PixelFormatSuite1> pfmt_suite(
            ::aetk::core::context::get_basic_suite(), kPFPixelFormatSuite,
            kPFPixelFormatSuiteVersion1);
        PrPixelFormat pr_format = PrPixelFormat_Invalid;
        if (pfmt_suite->GetPixelFormat(src, &pr_format) == PF_Err_NONE) {
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
                format = PF_PixelFormat_ARGB128;
            } else if (pr_format == PrPixelFormat_BGRA_4444_16u
                || pr_format == PrPixelFormat_ARGB_4444_16u
                || pr_format == PrPixelFormat_BGRP_4444_16u
                || pr_format == PrPixelFormat_PRGB_4444_16u
                || pr_format == PrPixelFormat_BGRX_4444_16u) {
                format = PF_PixelFormat_ARGB64;
            }
        }
    }

    if (format == PF_PixelFormat_INVALID) {
        format = PF_PixelFormat_ARGB32;
    }

    PF_Rect raw_area { };
    if (area) {
        raw_area = area->to_pf();
    }
    const PF_Rect* area_ptr = area ? &raw_area : nullptr;

    if (format == PF_PixelFormat_ARGB128) {
        aetk::core::suite<PF_IterateFloatSuite2> s(
            ::aetk::core::context::get_basic_suite());
        ::aetk::core::check_err(
            s->iterate_origin_non_clip_src(in_data, 0, src->height, src, area_ptr,
                origin, &func, detail::iterate_float_wrapper<PixelT, Func>, dst));

    } else if (format == PF_PixelFormat_ARGB64) {
        aetk::core::suite<PF_Iterate16Suite2> s(
            ::aetk::core::context::get_basic_suite());
        ::aetk::core::check_err(
            s->iterate_origin_non_clip_src(in_data, 0, src->height, src, area_ptr,
                origin, &func, detail::iterate16_wrapper<PixelT, Func>, dst));

    } else {
        aetk::core::suite<PF_Iterate8Suite2> s(
            ::aetk::core::context::get_basic_suite());
        ::aetk::core::check_err(
            s->iterate_origin_non_clip_src(in_data, 0, src->height, src, area_ptr,
                origin, &func, detail::iterate_wrapper<PixelT, Func>, dst));
    }
}

/**
 * @brief Color-mapped pixel iterator with origin offset.
 */
template <pixel_range Range = pixel_range::tkfloat, typename Func>
inline void iterate_pixels_origin(PF_InData* in_data, PF_EffectWorld* src, PF_EffectWorld* dst,
    const aetk::core::rect* area, const PF_Point* origin, Func&& func) {
    PF_PixelFormat format = PF_PixelFormat_INVALID;
    aetk::core::suite<PF_WorldSuite2> ws(::aetk::core::context::get_basic_suite());
    if (ws->PF_GetPixelFormat(src, &format) != PF_Err_NONE) {
        format = PF_PixelFormat_INVALID;
    }

    if (format == PF_PixelFormat_INVALID) {
        aetk::core::suite<PF_PixelFormatSuite1> pfmt_suite(
            ::aetk::core::context::get_basic_suite(), kPFPixelFormatSuite,
            kPFPixelFormatSuiteVersion1);
        PrPixelFormat pr_format = PrPixelFormat_Invalid;
        if (pfmt_suite->GetPixelFormat(src, &pr_format) == PF_Err_NONE) {
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
                format = PF_PixelFormat_ARGB128;
            } else if (pr_format == PrPixelFormat_BGRA_4444_16u
                || pr_format == PrPixelFormat_ARGB_4444_16u
                || pr_format == PrPixelFormat_BGRP_4444_16u
                || pr_format == PrPixelFormat_PRGB_4444_16u
                || pr_format == PrPixelFormat_BGRX_4444_16u) {
                format = PF_PixelFormat_ARGB64;
            }
        }
    }

    if (format == PF_PixelFormat_INVALID) {
        format = PF_PixelFormat_ARGB32;
    }

    const bool is_bgra = in_data && (in_data->appl_id == 'PrMr');

    visit_pixel_format<Range>(format, is_bgra, [&]<typename PixelT, bool IsBGRA>() {
        iterate_origin<PixelT>(in_data, src, dst, area, origin,
            [&func](A_long x, A_long y, PixelT* inP, PixelT* outP) {
                core::color<Range> c
                    = pixel_accessor<PixelT, IsBGRA, Range>::read(inP);
                func(x, y, c);
                pixel_accessor<PixelT, IsBGRA, Range>::write(outP, c);
            });
    });
}

/**
 * @brief Absolute color-mapped pixel iterator.
 */
template <pixel_range Range = pixel_range::tkfloat, typename Func>
inline void iterate_pixels(PF_InData* in_data, PF_EffectWorld* src, PF_EffectWorld* dst, Func&& func) {
    PF_PixelFormat format = PF_PixelFormat_INVALID;
    aetk::core::suite<PF_WorldSuite2> ws(::aetk::core::context::get_basic_suite());
    if (ws->PF_GetPixelFormat(src, &format) != PF_Err_NONE) {
        format = PF_PixelFormat_INVALID;
    }

    if (format == PF_PixelFormat_INVALID) {
        aetk::core::suite<PF_PixelFormatSuite1> pfmt_suite(
            ::aetk::core::context::get_basic_suite(), kPFPixelFormatSuite,
            kPFPixelFormatSuiteVersion1);
        PrPixelFormat pr_format = PrPixelFormat_Invalid;
        if (pfmt_suite->GetPixelFormat(src, &pr_format) == PF_Err_NONE) {
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
                format = PF_PixelFormat_ARGB128;
            } else if (pr_format == PrPixelFormat_BGRA_4444_16u
                || pr_format == PrPixelFormat_ARGB_4444_16u
                || pr_format == PrPixelFormat_BGRP_4444_16u
                || pr_format == PrPixelFormat_PRGB_4444_16u
                || pr_format == PrPixelFormat_BGRX_4444_16u) {
                format = PF_PixelFormat_ARGB64;
            }
        }
    }

    if (format == PF_PixelFormat_INVALID) {
        format = PF_PixelFormat_ARGB32;
    }

    const bool is_bgra = in_data && (in_data->appl_id == 'PrMr');

    visit_pixel_format<Range>(format, is_bgra, [&]<typename PixelT, bool IsBGRA>() {
        iterate<PixelT>(in_data, src, dst, [&func](A_long x, A_long y, PixelT* inP, PixelT* outP) {
            core::color<Range> c
                = pixel_accessor<PixelT, IsBGRA, Range>::read(inP);
            func(x, y, c);
            pixel_accessor<PixelT, IsBGRA, Range>::write(outP, c);
        });
    });
}

/**
 * @brief Raw parallel execution thread worker.
 */
template <typename Func>
inline void iterate_generic(PF_InData* in_data, A_long iterations, Func&& func) {
    aetk::core::suite<PF_Iterate8Suite2> s(::aetk::core::context::get_basic_suite());

    struct wrapper_data {
        Func func;
    } data { std::forward<Func>(func) };

    auto wrapper = [](void* refcon, A_long thread_idx, A_long i, A_long count) -> PF_Err {
        auto* d = static_cast<wrapper_data*>(refcon);
        try {
            d->func(thread_idx, i, count);
            return PF_Err_NONE;
        } catch (const std::exception& e) {
            AETK_ERROR("[iterate_generic] Exception in worker thread {}: {}",
                thread_idx, e.what());
            return PF_Err_OUT_OF_MEMORY;
        }
    };

    ::aetk::core::check_err(s->iterate_generic(iterations, &data, wrapper));
}

/**
 * @brief Parallel loop executor utilizing the host's native iterate suite.
 */
template <typename Func>
inline void parallel_for(PF_InData* in_data, int32_t iterations, Func&& func) {
    struct host_data {
        typename std::decay<Func>::type func;
    } data { std::forward<Func>(func) };

    auto callback = [](void* refcon, A_long thread_idx, A_long iter_idx,
                        A_long total_iters) -> PF_Err {
        auto* d = static_cast<host_data*>(refcon);
        try {
            d->func(static_cast<int32_t>(iter_idx), static_cast<int32_t>(thread_idx));
            return PF_Err_NONE;
        } catch (const std::exception& e) {
            AETK_ERROR("[parallel_for] Exception in worker thread {} at index {}: {}",
                thread_idx, iter_idx, e.what());
            return PF_Err_OUT_OF_MEMORY;
        }
    };

    aetk::core::suite<PF_Iterate8Suite2> s(::aetk::core::context::get_basic_suite());
    ::aetk::core::check_err(s->iterate_generic(iterations, &data, callback));
}

} // namespace aetk::effect
