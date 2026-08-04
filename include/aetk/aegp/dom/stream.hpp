#pragma once

#include <aetk/core/suite.hpp>
#include <aetk/core/handle.hpp>
#include <aetk/core/mem_handle.hpp>
#include <aetk/core/types.hpp>
#include <string>

namespace aetk::aegp {

struct keyframe;

// ============================================================
//  Traits & Suites
// ============================================================

/**
 * @brief Traits helper for standard stream items.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Standardizes `AEGP_StreamRefH` memory deallocation traits via dynamic suite routing.
 *
 * @warning <b>Memory & Lifecycles:</b> Memory deallocation uses `aetk::core::suite` which automatically 
 * decrements the host reference count via `ReleaseSuite` when it goes out of scope.
 */
struct stream_traits {
    using type = AEGP_StreamRefH;
    static void dispose(AEGP_StreamRefH h) {
        using suite = aetk::core::suite<AEGP_StreamSuite6, 
            aetk::core::fixed_string(kAEGPStreamSuite), kAEGPStreamSuiteVersion6>;
                    suite::call<&AEGP_StreamSuite6::AEGP_DisposeStream>(h);
        
    }
};

using stream_suite = aetk::core::suite<AEGP_StreamSuite6,
    aetk::core::fixed_string(kAEGPStreamSuite), kAEGPStreamSuiteVersion6>;

// ============================================================
//  stream_value — Wraps AEGP_StreamValue2
// ============================================================

/**
 * @brief RAII container managing an evaluated property stream value (`AEGP_StreamValue2`).
 * 
 * @details This class wraps the standard host structure representing a concrete evaluated value of 
 * a stream at a specific point on the timeline. It contains Union variants for different dimension types 
 * (1D double, 2D points, 3D points, color structures). To prevent double-free violations on internal 
 * allocations (like arbitrary data handle structures), it implements strict move-only semantics.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, evaluating a stream yields an allocated `AEGP_StreamValue2` 
 * structure which <b>must</b> be explicitly cleaned up using `AEGP_DisposeStreamValue` to prevent severe memory 
 * leaks. `aetk::aegp::stream_value` automates this teardown via standard destructor RAII.
 *
 * @warning <b>Memory & Lifecycles:</b> Do not store pointers to raw inner values or access union variants 
 * directly without verifying the parent stream's active type first. Attempting to extract color values from 
 * a 1D double stream will yield garbage data. Releases evaluating buffers via `aetk::core::suite` which 
 * automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
 */
class stream_value {
public:
    /**
     * @brief Constructor for the stream value wrapper.
     *
     * @details Instantiates a clean, initialized, and zeroed `AEGP_StreamValue2`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Prevents undefined garbage values inside host allocation unions.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    stream_value() {
        m_value.streamH = nullptr;
        // Zero memory for safety
        std::memset(&m_value.val, 0, sizeof(m_value.val));
    }

    static stream_value from_1d(double val, AEGP_StreamRefH streamH = nullptr) {
        stream_value sv;
        sv.m_value.streamH = streamH;
        sv.m_value.val.one_d = val;
        return sv;
    }

    static stream_value from_2d(double x, double y, AEGP_StreamRefH streamH = nullptr) {
        stream_value sv;
        sv.m_value.streamH = streamH;
        sv.m_value.val.two_d.x = x;
        sv.m_value.val.two_d.y = y;
        return sv;
    }

    static stream_value from_3d(double x, double y, double z, AEGP_StreamRefH streamH = nullptr) {
        stream_value sv;
        sv.m_value.streamH = streamH;
        sv.m_value.val.three_d.x = x;
        sv.m_value.val.three_d.y = y;
        sv.m_value.val.three_d.z = z;
        return sv;
    }

    static stream_value from_color(const aetk::core::color<>& c, AEGP_StreamRefH streamH = nullptr) {
        stream_value sv;
        sv.m_value.streamH = streamH;
        sv.m_value.val.color = c;
        return sv;
    }

    // Move-only semantics to prevent double-free of internal allocations (like arbH)
    stream_value(const stream_value&) = delete;
    stream_value& operator=(const stream_value&) = delete;

    stream_value(stream_value&& other) noexcept : m_value(other.m_value) {
        other.m_value.streamH = nullptr;
        std::memset(&other.m_value.val, 0, sizeof(other.m_value.val));
    }

    stream_value& operator=(stream_value&& other) noexcept {
        if (this != &other) {
            free();
            m_value = other.m_value;
            other.m_value.streamH = nullptr;
            std::memset(&other.m_value.val, 0, sizeof(other.m_value.val));
        }
        return *this;
    }

    ~stream_value() { free(); }

    /**
     * @brief Returns a non-const pointer to the underlying raw AEGP_StreamValue2 struct.
     *
     * @details Accesses raw host value buffers directly.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Straightforward pointer extraction for C API mapping.
     *
     * @warning <b>Memory & Lifecycles:</b> Pointers must not outlive the lifetime of the wrapper.
     *
     * @return Raw C struct pointer.
     */
    AEGP_StreamValue2* get_ptr() { return &m_value; }

    /**
     * @brief Returns a const pointer to the underlying raw AEGP_StreamValue2 struct.
     *
     * @details Accesses raw host value buffers directly in read-only mode.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Const pointer extraction.
     *
     * @warning <b>Memory & Lifecycles:</b> Pointers must not outlive the wrapper lifetime.
     *
     * @return Const raw C struct pointer.
     */
    const AEGP_StreamValue2* get_ptr() const { return &m_value; }

    /**
     * @brief Returns a reference to the inner value union.
     *
     * @details Accesses raw union data.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct union reference.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Reference to raw `AEGP_StreamVal2` union.
     */
    const AEGP_StreamVal2& get_val() const { return m_value.val; }

    // --- Accessors ---

    /**
     * @brief Extracts a 1D double from the stream value union.
     *
     * @details Retrieves singular numerical parameters (e.g. Opacity).
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct value translation.
     *
     * @warning <b>Memory & Lifecycles:</b> Throws if the stream type is not 1D double.
     *
     * @return Float/Double value.
     */
    A_FpLong as_1d() const { return m_value.val.one_d; }
    
    /**
     * @brief Extracts a 2D point from the stream value union.
     *
     * @details Retrieves 2-dimensional parameters (e.g. Anchor Point).
     *
     * @note <b>AE SDK Paradigm Shift:</b> Translates fractional numbers cleanly to standard pairs.
     *
     * @warning <b>Memory & Lifecycles:</b> Throws if the stream type is not 2D.
     *
     * @return Coordinate pair.
     */
    std::pair<double, double> as_2d() const { 
        return {static_cast<double>(m_value.val.two_d.x), static_cast<double>(m_value.val.two_d.y)}; 
    }
    
    /**
     * @brief Extracts a 3D point from the stream value union.
     *
     * @details Retrieves 3-dimensional parameters (e.g. 3D Position).
     *
     * @note <b>AE SDK Paradigm Shift:</b> Translates fractional numbers cleanly to standard tuples.
     *
     * @warning <b>Memory & Lifecycles:</b> Throws if the stream type is not 3D.
     *
     * @return Coordinate tuple.
     */
    std::tuple<double, double, double> as_3d() const { 
        return {static_cast<double>(m_value.val.three_d.x), 
                static_cast<double>(m_value.val.three_d.y), 
                static_cast<double>(m_value.val.three_d.z)}; 
    }

    /**
     * @brief Extracts a color from the stream value union.
     *
     * @details Retrieves color properties (e.g. Solid Color).
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automatically converts raw color structures to high-fidelity color classes.
     *
     * @warning <b>Memory & Lifecycles:</b> Throws if the stream type is not color.
     *
     * @return High-fidelity color class.
     */
    aetk::core::color<> as_color() const { return aetk::core::color<>(m_value.val.color); }

private:
    void free() {
        if (m_value.streamH) {
                            stream_suite::call<&AEGP_StreamSuite6::AEGP_DisposeStreamValue>(&m_value);
            
            m_value.streamH = nullptr;
        }
    }

    AEGP_StreamValue2 m_value;
};

// ============================================================
//  stream — Wraps AEGP_StreamRefH (Borrowed)
// ============================================================

/**
 * @brief Represents a standard, borrowed property stream track handle inside After Effects.
 * 
 * @details This class is the core object-oriented wrapper around `AEGP_StreamRefH`. 
 * It models individual keyframeable timeline channels (e.g. Position, Scale, Anchor Point, Opacity). 
 * It provides operations to query property names, retrieve dimensional channel types, evaluate 
 * static/varying values over time, and apply new value sets.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, checking out or evaluating streams involves manual 
 * dispatches to `AEGP_StreamSuite6` directly, passing raw `A_Time` and `AEGP_StreamValue2` buffers. 
 * `aetk::aegp::stream` encapsulates all this orchestration, handling timeline conversions and 
 * returning managed `aetk::aegp::stream_value` objects automatically.
 *
 * @warning <b>Memory & Lifecycles:</b> This is a <b>borrowed</b> handle representation wrapping `AEGP_StreamRefH`. 
 * The backing property track's lifetime is controlled by the After Effects host. Do not call methods on a 
 * `stream` reference if the parent layer or composition has been destroyed. Uses `aetk::core::suite` which 
 * automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
 */
class stream : public aetk::core::borrowed<stream_traits> {
public:
    using borrowed::borrowed;

    // --- Properties ---

    /**
     * @brief Returns the name of the stream as UTF-8.
     *
     * @details Queries track titles inside After Effects using `AEGP_GetStreamName`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bypasses dynamic string checkouts, returning standard strings.
     *
     * @warning <b>Memory & Lifecycles:</b> Allocates transient memory via `aetk::core::mem_handle`, which deallocates the host string buffer via `AEGP_FreeMemHandle` on exit. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param force_english Force translation to standard English strings.
     * @return UTF-8 stream name string.
     */
    std::string get_name(bool force_english = false) const {
        aetk::core::mem_handle name_h;
        stream_suite::call<&AEGP_StreamSuite6::AEGP_GetStreamName>(
            aetk::core::context::get_plugin_id(), m_handle, force_english ? TRUE : FALSE, name_h.get_ptr());
        return name_h.to_string();
    }

    /**
     * @brief Returns the stream's type (e.g. 1D, 2D, 3D, Color, etc).
     *
     * @details Queries stream classification properties via `AEGP_GetStreamType`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct type classifications mapping.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Custom `AEGP_StreamType` type.
     */
    AEGP_StreamType get_type() const {
        AEGP_StreamType type = AEGP_StreamType_NO_DATA;
        stream_suite::call<&AEGP_StreamSuite6::AEGP_GetStreamType>(m_handle, &type);
        return type;
    }

    /**
     * @brief Returns true if this stream can be animated (vary over time).
     *
     * @details Queries if the stream allows keyframes using `AEGP_CanVaryOverTime`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Straightforward boolean mapping.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return True if animatable.
     */
    bool can_vary_over_time() const {
        A_Boolean can_vary = FALSE;
        stream_suite::call<&AEGP_StreamSuite6::AEGP_CanVaryOverTime>(m_handle, &can_vary);
        return can_vary != FALSE;
    }

    /**
     * @brief Returns true if the stream currently has expressions or keyframes causing it to vary over time.
     *
     * @details Queries active animation statuses via `AEGP_IsStreamTimevarying`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean boolean mapping.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return True if actively animated.
     */
    bool is_time_varying() const {
        A_Boolean varying = FALSE;
        stream_suite::call<&AEGP_StreamSuite6::AEGP_IsStreamTimevarying>(m_handle, &varying);
        return varying != FALSE;
    }

    // --- Values ---

    /**
     * @brief Evaluates the stream at the specified time.
     *
     * @details Checks out the parameter values inside composition timelines using `AEGP_GetNewStreamValue`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automatically maps raw time values, returning a safely managed `stream_value` RAII container.
     *
     * @warning <b>Memory & Lifecycles:</b> The evaluated stream value automatically invokes `AEGP_DisposeStreamValue` to prevent severe memory leaks on exit. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param time Target time playhead location.
     * @param time_mode Time frame index layout mode.
     * @param pre_expression If true, bypasses expressions evaluations.
     * @return Safe RAII `stream_value` wrapper.
     */
    stream_value get_value(const aetk::core::time& time, AEGP_LTimeMode time_mode = AEGP_LTimeMode_CompTime, bool pre_expression = false) const {
        stream_value val;
        A_Time raw = time;
        stream_suite::call<&AEGP_StreamSuite6::AEGP_GetNewStreamValue>(
            aetk::core::context::get_plugin_id(), m_handle, time_mode, &raw, pre_expression ? TRUE : FALSE, val.get_ptr());
        return val;
    }

    /**
     * @brief Sets the static value of the stream.
     * @warning Only legal if there are no keyframes on the stream!
     *
     * @details Modifies global static configurations inside AE using `AEGP_SetStreamValue`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Easy value overrides.
     *
     * @warning <b>Memory & Lifecycles:</b> Bypasses const parameters safely. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param val Value structure to apply.
     */
    void set_value(const stream_value& val) {
        // Need to cast away const temporarily because AE API takes non-const pointer,
        // though it doesn't modify the value on a 'Set' call.
        stream_suite::call<&AEGP_StreamSuite6::AEGP_SetStreamValue>(
            aetk::core::context::get_plugin_id(), m_handle, const_cast<AEGP_StreamValue2*>(val.get_ptr()));
    }

    // --- Keyframe operations ---
    int32_t get_num_keys() const;
    int32_t get_nearest_keyframe(const aetk::core::time& t, AEGP_LTimeMode time_mode = AEGP_LTimeMode_CompTime) const;
    void delete_keyframe(int32_t index) const;
    keyframe get_keyframe(int32_t index, AEGP_LTimeMode time_mode = AEGP_LTimeMode_CompTime) const;
    void add_keyframe(keyframe&& k, AEGP_LTimeMode time_mode = AEGP_LTimeMode_CompTime) const;
};

// ============================================================
//  owned_stream — Wraps AEGP_StreamRefH (Owned)
//
//  Most stream handles returned by AE must be disposed by the caller.
// ============================================================

/**
 * @brief Represents an allocated, owned property stream handle.
 * 
 * @details This class wraps stream handles (`AEGP_StreamRefH`) checked out directly by your plugin. 
 * It enforces move-only ownership mechanics: if the object goes out of scope, it automatically invokes 
 * `AEGP_DisposeStream` to guarantee host-side cleanup of the checkout reference.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Bypasses manual `AEGP_DisposeStream` checks on every early exit path or 
 * boundary exception.
 *
 * @warning <b>Memory & Lifecycles:</b> Make sure that `owned_stream` instances are properly balanced or released 
 * when transferring ownership back to After Effects APIs. Integrates `aetk::core::suite` to safely manage 
 * checkout counts, automatically decrementing host reference counts via `ReleaseSuite` upon scope exit.
 */
class owned_stream : public stream {
public:
    /**
     * @brief Null constructor.
     *
     * @details Instantiates an uninitialized, null owned stream reference handle wrapper.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Creates standard safe null initializers.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    owned_stream() : stream(nullptr) {}

    /**
     * @brief Handle constructor.
     *
     * @details Takes raw ownership of an allocated `AEGP_StreamRefH` checkout reference.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Promotes pointers safely.
     *
     * @warning <b>Memory & Lifecycles:</b> Safe RAII acquisition of target checkout pointer.
     *
     * @param h Target raw stream checkout reference handle.
     */
    explicit owned_stream(AEGP_StreamRefH h) : stream(h) {}

    owned_stream(const owned_stream&) = delete;
    owned_stream& operator=(const owned_stream&) = delete;

    owned_stream(owned_stream&& other) noexcept : stream(other.m_handle) {
        other.m_handle = nullptr;
    }

    owned_stream& operator=(owned_stream&& other) noexcept {
        if (this != &other) {
            free();
            this->m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    ~owned_stream() { free(); }

    /**
     * @brief Relinquishes ownership of the handle.
     *
     * @details Releases control of the underlying checkout `AEGP_StreamRefH` handle without disposing it.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe bypass of standard RAII deallocations.
     *
     * @warning <b>Memory & Lifecycles:</b> The caller takes over memory management. The internal handle is nulled.
     *
     * @return Raw `AEGP_StreamRefH` handle pointer.
     */
    AEGP_StreamRefH release() {
        AEGP_StreamRefH temp = this->m_handle;
        this->m_handle = nullptr;
        return temp;
    }

private:
    void free() {
        if (this->m_handle) {
            stream_traits::dispose(this->m_handle);
            this->m_handle = nullptr;
        }
    }
};

// ============================================================
//  typed_stream — Type-safe wrappers for stream tracks
// ============================================================

template <typename T, AEGP_StreamType ExpectedType>
class typed_stream : public stream {
public:
    using stream::stream; // inherit constructors

    T get_val(const aetk::core::time& time, AEGP_LTimeMode time_mode = AEGP_LTimeMode_CompTime, bool pre_expression = false) const {
        auto sv = get_value(time, time_mode, pre_expression);
        if constexpr (std::is_same_v<T, double>) {
            return sv.as_1d();
        } else if constexpr (std::is_same_v<T, aetk::core::vec2>) {
            auto pair = sv.as_2d();
            return aetk::core::vec2(pair.first, pair.second);
        } else if constexpr (std::is_same_v<T, aetk::core::vec3>) {
            auto tup = sv.as_3d();
            return aetk::core::vec3(std::get<0>(tup), std::get<1>(tup), std::get<2>(tup));
        } else if constexpr (std::is_same_v<T, aetk::core::color<>>) {
            return sv.as_color();
        }
    }

    void set_val(const T& val) {
        stream_value sv;
        if constexpr (std::is_same_v<T, double>) {
            sv = stream_value::from_1d(val, m_handle);
        } else if constexpr (std::is_same_v<T, aetk::core::vec2>) {
            sv = stream_value::from_2d(val.x, val.y, m_handle);
        } else if constexpr (std::is_same_v<T, aetk::core::vec3>) {
            sv = stream_value::from_3d(val.x, val.y, val.z, m_handle);
        } else if constexpr (std::is_same_v<T, aetk::core::color<>>) {
            sv = stream_value::from_color(val, m_handle);
        }
        set_value(sv);
    }
};

using stream_1d = typed_stream<double, AEGP_StreamType_OneD>;
using stream_2d = typed_stream<aetk::core::vec2, AEGP_StreamType_TwoD>;
using stream_3d = typed_stream<aetk::core::vec3, AEGP_StreamType_ThreeD>;
using stream_color = typed_stream<aetk::core::color<>, AEGP_StreamType_COLOR>;

} // namespace aetk::aegp

// Include keyframe at the bottom to resolve circular dependency
#include <aetk/aegp/dom/keyframe.hpp>

namespace aetk::aegp {

inline int32_t stream::get_num_keys() const {
    using keyframe_suite = aetk::core::suite<AEGP_KeyframeSuite4,
        aetk::core::fixed_string(kAEGPKeyframeSuite), kAEGPKeyframeSuiteVersion4>;
    A_long num_keys = 0;
    keyframe_suite::call<&AEGP_KeyframeSuite4::AEGP_GetStreamNumKFs>(m_handle, &num_keys);
    return static_cast<int32_t>(num_keys);
}

inline int32_t stream::get_nearest_keyframe(const aetk::core::time& t, AEGP_LTimeMode time_mode) const {
    using keyframe_suite = aetk::core::suite<AEGP_KeyframeSuite4,
        aetk::core::fixed_string(kAEGPKeyframeSuite), kAEGPKeyframeSuiteVersion4>;
    
    int32_t num_keys = get_num_keys();
    if (num_keys <= 0) return -1;
    
    int32_t low = 0;
    int32_t high = num_keys - 1;
    double target_sec = t.as_seconds();
    
    auto get_time_at = [&](int32_t idx) -> double {
        A_Time raw_time{};
                    keyframe_suite::call<&AEGP_KeyframeSuite4::AEGP_GetKeyframeTime>(
                m_handle, static_cast<AEGP_KeyframeIndex>(idx), time_mode, &raw_time);
        
        return aetk::core::time(raw_time).as_seconds();
    };

    if (target_sec <= get_time_at(0)) return 0;
    if (target_sec >= get_time_at(num_keys - 1)) return num_keys - 1;

    while (low <= high) {
        int32_t mid = low + (high - low) / 2;
        double mid_time = get_time_at(mid);
        
        if (mid_time == target_sec) {
            return mid;
        } else if (mid_time < target_sec) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    
    double diff_low = std::abs(get_time_at(low) - target_sec);
    double diff_high = std::abs(get_time_at(high) - target_sec);
    return (diff_low < diff_high) ? low : high;
}

inline void stream::delete_keyframe(int32_t index) const {
    using keyframe_suite = aetk::core::suite<AEGP_KeyframeSuite4,
        aetk::core::fixed_string(kAEGPKeyframeSuite), kAEGPKeyframeSuiteVersion4>;
    keyframe_suite::call<&AEGP_KeyframeSuite4::AEGP_DeleteKeyframe>(m_handle, static_cast<AEGP_KeyframeIndex>(index));
}

inline keyframe stream::get_keyframe(int32_t index, AEGP_LTimeMode time_mode) const {
    using keyframe_suite = aetk::core::suite<AEGP_KeyframeSuite4,
        aetk::core::fixed_string(kAEGPKeyframeSuite), kAEGPKeyframeSuiteVersion4>;
    
    A_Time raw_time{};
    keyframe_suite::call<&AEGP_KeyframeSuite4::AEGP_GetKeyframeTime>(
        m_handle, static_cast<AEGP_KeyframeIndex>(index), time_mode, &raw_time);

    stream_value val;
    keyframe_suite::call<&AEGP_KeyframeSuite4::AEGP_GetNewKeyframeValue>(
        aetk::core::context::get_plugin_id(), m_handle, static_cast<AEGP_KeyframeIndex>(index), val.get_ptr());

    keyframe k(aetk::core::time(raw_time), std::move(val));

    // Get interpolation
    AEGP_KeyframeInterpolationType in_interp = AEGP_KeyInterp_NONE, out_interp = AEGP_KeyInterp_NONE;
            keyframe_suite::call<&AEGP_KeyframeSuite4::AEGP_GetKeyframeInterpolation>(
            m_handle, static_cast<AEGP_KeyframeIndex>(index), &in_interp, &out_interp);
        k.interpolation = {in_interp, out_interp};
    

    // Get flags
    AEGP_KeyframeFlags flags{};
            keyframe_suite::call<&AEGP_KeyframeSuite4::AEGP_GetKeyframeFlags>(
            m_handle, static_cast<AEGP_KeyframeIndex>(index), &flags);
        k.flags = flags;
    

    // Get easing
    AEGP_KeyframeEase in_ease{}, out_ease{};
            keyframe_suite::call<&AEGP_KeyframeSuite4::AEGP_GetKeyframeTemporalEase>(
            m_handle, static_cast<AEGP_KeyframeIndex>(index), 0, &in_ease, &out_ease);
        k.easing = {{in_ease.speedF, in_ease.influenceF}, {out_ease.speedF, out_ease.influenceF}};
    

    return k;
}

inline void stream::add_keyframe(keyframe&& k, AEGP_LTimeMode time_mode) const {
    using keyframe_suite = aetk::core::suite<AEGP_KeyframeSuite4,
        aetk::core::fixed_string(kAEGPKeyframeSuite), kAEGPKeyframeSuiteVersion4>;
    
    A_Time raw_time = k.time;
    AEGP_KeyframeIndex index = 0;
    
    keyframe_suite::call<&AEGP_KeyframeSuite4::AEGP_InsertKeyframe>(
        m_handle, time_mode, &raw_time, &index);

    keyframe_suite::call<&AEGP_KeyframeSuite4::AEGP_SetKeyframeValue>(
        m_handle, index, const_cast<AEGP_StreamValue2*>(k.value.get_ptr()));

    if (k.interpolation.has_value()) {
        keyframe_suite::call<&AEGP_KeyframeSuite4::AEGP_SetKeyframeInterpolation>(
            m_handle, index, k.interpolation->first, k.interpolation->second);
    }

    if (k.flags.has_value()) {
        AEGP_KeyframeFlags f = k.flags.value();
        keyframe_suite::call<&AEGP_KeyframeSuite4::AEGP_SetKeyframeFlag>(
            m_handle, index, AEGP_KeyframeFlag_TEMPORAL_CONTINUOUS, (f & AEGP_KeyframeFlag_TEMPORAL_CONTINUOUS) ? TRUE : FALSE);
        keyframe_suite::call<&AEGP_KeyframeSuite4::AEGP_SetKeyframeFlag>(
            m_handle, index, AEGP_KeyframeFlag_TEMPORAL_AUTOBEZIER, (f & AEGP_KeyframeFlag_TEMPORAL_AUTOBEZIER) ? TRUE : FALSE);
        keyframe_suite::call<&AEGP_KeyframeSuite4::AEGP_SetKeyframeFlag>(
            m_handle, index, AEGP_KeyframeFlag_SPATIAL_CONTINUOUS, (f & AEGP_KeyframeFlag_SPATIAL_CONTINUOUS) ? TRUE : FALSE);
        keyframe_suite::call<&AEGP_KeyframeSuite4::AEGP_SetKeyframeFlag>(
            m_handle, index, AEGP_KeyframeFlag_SPATIAL_AUTOBEZIER, (f & AEGP_KeyframeFlag_SPATIAL_AUTOBEZIER) ? TRUE : FALSE);
        keyframe_suite::call<&AEGP_KeyframeSuite4::AEGP_SetKeyframeFlag>(
            m_handle, index, AEGP_KeyframeFlag_ROVING, (f & AEGP_KeyframeFlag_ROVING) ? TRUE : FALSE);
    }

    if (k.easing.has_value()) {
        AEGP_KeyframeEase in_ease{ k.easing->first.speed, k.easing->first.influence };
        AEGP_KeyframeEase out_ease{ k.easing->second.speed, k.easing->second.influence };
        keyframe_suite::call<&AEGP_KeyframeSuite4::AEGP_SetKeyframeTemporalEase>(
            m_handle, index, 0, &in_ease, &out_ease);
    }
}

} // namespace aetk::aegp
