#pragma once

#include <aetk/core/types.hpp>
#include <aetk/aegp/dom/stream.hpp>
#include <optional>
#include <utility>

namespace aetk::aegp {

/**
 * @brief Represents a single keyframe point on a timeline property track.
 * 
 * @note <b>AE SDK Paradigm Shift:</b> Wraps keyframe timing, values, easing, and interpolations
 * in a type-safe modern C++ struct, featuring move-only semantics to prevent double-freeing
 * of evaluated stream values.
 */
struct keyframe {
    /// Time of the keyframe on the timeline.
    aetk::core::time time;
    
    /// The evaluated value of the keyframe.
    stream_value value;

    /// Easing (in/out speed and influence). Optional.
    struct ease {
        double speed = 0.0;
        double influence = 0.0;
    };
    std::optional<std::pair<ease, ease>> easing;

    /// Interpolation type (in/out). Optional.
    std::optional<std::pair<AEGP_KeyframeInterpolationType, AEGP_KeyframeInterpolationType>> interpolation;

    /// Keyframe flags (e.g. continuous, autobezier). Optional.
    std::optional<AEGP_KeyframeFlags> flags;

    // Constructors
    keyframe() = default;
    keyframe(const aetk::core::time& t, stream_value&& val) 
        : time(t), value(std::move(val)) {}

    // Move-only
    keyframe(const keyframe&) = delete;
    keyframe& operator=(const keyframe&) = delete;
    keyframe(keyframe&&) noexcept = default;
    keyframe& operator=(keyframe&&) noexcept = default;
};

} // namespace aetk::aegp
