#pragma once

#include <AE_Effect.h>
#include <AE_GeneralPlug.h>
#include <cstdint>
#include <cmath>
#include <type_traits>
#include <algorithm>

namespace aetk::core {

    /**
 * @brief Compile-time tag for routing raw PF_Pixel formats.
 * @details Prevents virtual dispatch overhead inside hot execution loops.
 */
template <typename T> 
struct pixel_tag { 
    using type = T; 
};

} // namespace aetk::core

namespace aetk::execution {

/**
 * @brief CPU execution policy for standard host-side threading (AE Suites / iterate_generic).
 */
struct cpu_policy {};

/**
 * @brief CUDA execution policy for device-side kernel launches.
 */
struct cuda_policy {
    // If <cuda_runtime.h> isn't included here to protect MSVC, use void* // and cast to cudaStream_t in the implementation.
    void* stream = nullptr; 
    
    // Optional configuration for grid/block sizing
    int block_size_x = 16;
    int block_size_y = 16;
};

// Global policy instances for clean syntax: `iterate(aetk::execution::cpu, ...)`
inline constexpr cpu_policy cpu{};
inline constexpr cuda_policy cuda{};

} // namespace aetk::execution

namespace aetk::core {

// ============================================================
//  time
// ============================================================

/**
 * @brief High-level timeline time structure.
 *
 * @details Encapsulates host scale and value fields representing time offsets in the timeline.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, time operations require manual handling of `A_Time` struct fields (value and scale), converting frames to seconds, and writing custom comparison loops. `aetk::core::time` encapsulates these fields, providing implicit casts, comparison operators, and framerate-based factory functions.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct time {
    /// Internal raw value representation.
    int32_t value{0};
    
    /// Scaling divisor value.
    uint32_t scale{1};

    /**
     * @brief Default constructor.
     *
     * @details Binds a null time structure.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard time initializer.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    time() = default;
    
    /**
     * @brief Initializer constructor.
     *
     * @details Assigns raw value and scale properties.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct field mapping.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param v Raw value index.
     * @param s Scaling divisor.
     */
    time(int32_t v, uint32_t s) : value(v), scale(s) {}

    /**
     * @brief Explicit A_Time constructor.
     *
     * @details Extracts value and scale from the raw SDK structure.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Binds raw C struct.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param t Target raw SDK A_Time structure.
     */
    explicit time(const A_Time& t) : value(t.value), scale(t.scale) {}
    
    /**
     * @brief Implicit conversion to A_Time.
     *
     * @details Creates a standard SDK time structure from this instance.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automated compatibility wrapper conversion.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    operator A_Time() const {
        return { value, scale };
    }

    /**
     * @brief Creates time from seconds.
     *
     * @details Formats scaling and values based on seconds duration.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Converts fractional seconds to fixed-point scale.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param seconds Total timeline offset in seconds.
     * @param scale Internal time resolution scale.
     * @return Formatted time object.
     */
    static time from_seconds(double seconds, uint32_t scale = 100000) {
        return {static_cast<int32_t>(seconds * scale), scale};
    }

    /**
     * @brief Creates time from frame index and fps.
     *
     * @details Converts frame indexes to seconds using relative FPS values.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simplifies standard timeline frame math.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param frame Target frame index.
     * @param fps Frame rate factor.
     * @return Formatted time object.
     */
    static time from_frames(int32_t frame, double fps) {
        uint32_t scale = 100000;
        double seconds = frame / fps;
        return from_seconds(seconds, scale);
    }

    /**
     * @brief Converts time to seconds representation.
     *
     * @details Divides value by scale.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe conversion check.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Time duration in seconds.
     */
    double as_seconds() const {
        if (scale == 0) return 0.0;
        return static_cast<double>(value) / scale;
    }

    /**
     * @brief Converts time to frame coordinates.
     *
     * @details Multiplies duration by frame rate.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Easy frame translation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param fps Target frame rate.
     * @return Dynamic frame number.
     */
    double as_frames(double fps) const {
        return as_seconds() * fps;
    }

    // Basic math
    bool operator==(const time& other) const {
        return std::abs(as_seconds() - other.as_seconds()) < 1e-6;
    }
    bool operator!=(const time& other) const { return !(*this == other); }
    bool operator<(const time& other) const { return as_seconds() < other.as_seconds(); }
    bool operator>(const time& other) const { return as_seconds() > other.as_seconds(); }
    bool operator<=(const time& other) const { return !(*this > other); }
    bool operator>=(const time& other) const { return !(*this < other); }

    time operator+(const time& other) const {
        return from_seconds(as_seconds() + other.as_seconds());
    }

    time operator-(const time& other) const {
        return from_seconds(as_seconds() - other.as_seconds());
    }
};

// ============================================================
//  ratio
// ============================================================

/**
 * @brief Modern aspect ratio descriptor.
 *
 * @details Represents numerator over denominator fraction structures.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Wraps raw `A_Ratio` variables with convenient floating-point conversion helpers.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct ratio {
    /// Numerator factor.
    int32_t num{1};
    
    /// Denominator factor.
    uint32_t den{1};

    ratio() = default;
    ratio(int32_t n, uint32_t d) : num(n), den(d) {}

    // Internal conversion from/to A_Ratio
    explicit ratio(const A_Ratio& r) : num(r.num), den(r.den) {}
    operator A_Ratio() const {
        return { num, den };
    }

    /**
     * @brief Convert to float value.
     *
     * @details Resolves the numerator over the denominator.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe divide check.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Ratio decimal value.
     */
    double as_float() const {
        if (den == 0) return 0.0;
        return static_cast<double>(num) / den;
    }
};

// ============================================================
//  vec2 / vec3
// ============================================================

/**
 * @brief 2D Vector coordinate structure.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Provides structured 2D coordinate representations, adding normalized vector operations and fixed-point scale helpers.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct vec2 {
    double x{0.0};
    double y{0.0};

    vec2() = default;
    vec2(double _x, double _y) : x(_x), y(_y) {}

    /**
     * @brief Convert fixed-point coordinates.
     *
     * @details Scales down 16.16 values to fractional values.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automates standard 16.16 fixed-point scaling.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param fx Fixed X coordinate.
     * @param fy Fixed Y coordinate.
     * @return Decoded vector.
     */
    static vec2 from_fixed(int32_t fx, int32_t fy) {
        return { fx / 65536.0, fy / 65536.0 };
    }

    vec2 operator+(const vec2& o) const { return { x + o.x, y + o.y }; }
    vec2 operator-(const vec2& o) const { return { x - o.x, y - o.y }; }
    vec2 operator*(double s) const { return { x * s, y * s }; }
    vec2 operator/(double s) const { return { x / s, y / s }; }

    /**
     * @brief Squared length.
     *
     * @details Fast squared length without square root.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Performance optimized math.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Squared magnitude length.
     */
    double length_sq() const { return x * x + y * y; }
    
    /**
     * @brief Euclidean length.
     *
     * @details Returns vector magnitude.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard vector magnitude.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Magnitude length.
     */
    double length() const { return std::sqrt(length_sq()); }
    
    /**
     * @brief Normalize vector.
     *
     * @details Scales vector to unit length.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Unit vector generation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Normalized unit vector.
     */
    vec2 normalized() const {
        double l = length();
        return l > 1e-6 ? *this / l : vec2{0, 0};
    }
};

/**
 * @brief 3D Vector coordinate structure.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Modern representation of 3D spatial vectors.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    vec3() = default;
    vec3(double _x, double _y, double _z) : x(_x), y(_y), z(_z) {}
};

// ============================================================
//  rect
// ============================================================

struct lrect;

/**
 * @brief Integer 2D bounds bounds structure.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Replaces raw `PF_Rect` bounds structures (and solves Windows/Mac compatibility discrepancies) with a unified, cross-platform bounding rect wrapper.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct rect {
    int32_t left{0};
    int32_t top{0};
    int32_t right{0};
    int32_t bottom{0};

    rect() = default;
    rect(int32_t l, int32_t t, int32_t r, int32_t b) 
        : left(l), top(t), right(r), bottom(b) {}

    explicit rect(const lrect& r);

    /**
     * @brief Get width.
     *
     * @details Computes horizontal bounds span.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounding width calculation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Width in integer units.
     */
    int32_t width() const { return right - left; }
    
    /**
     * @brief Get height.
     *
     * @details Computes vertical bounds span.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounding height calculation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Height in integer units.
     */
    int32_t height() const { return bottom - top; }
    
    /**
     * @brief Check if bounds are empty.
     *
     * @details Returns true if dimensions are zero or invalid.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bounds emptiness check.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return True if dimensions are empty.
     */
    bool empty() const { return left >= right || top >= bottom; }

    // Internal conversion
    explicit rect(const PF_Rect& r) : left(r.left), top(r.top), right(r.right), bottom(r.bottom) {}

    operator PF_Rect() const {
        PF_Rect r;
        r.left   = (int16_t)std::clamp<int32_t>(left, -32768, 32767);
        r.top    = (int16_t)std::clamp<int32_t>(top, -32768, 32767);
        r.right  = (int16_t)std::clamp<int32_t>(right, -32768, 32767);
        r.bottom = (int16_t)std::clamp<int32_t>(bottom, -32768, 32767);
        return r;
    }

    PF_Rect to_pf() const { return static_cast<PF_Rect>(*this); }

    /**
     * @brief Create rect from center and size.
     *
     * @details Computes bounding extents.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simplifies centered layouts.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param cx Center X coordinate.
     * @param cy Center Y coordinate.
     * @param w Target width.
     * @param h Target height.
     * @return Bounding rectangle.
     */
    static rect from_center(int32_t cx, int32_t cy, int32_t w, int32_t h) {
        return { cx - w/2, cy - h/2, cx + w/2, cy + h/2 };
    }
};

// ============================================================
//  lrect
// ============================================================

/**
 * @brief 32-bit Integer 2D bounds structure for SmartFX rendering.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Wraps native PF_LRect structures, avoiding 16-bit truncation of high-resolution layer coordinates.
 */
struct lrect {
    int32_t left{0};
    int32_t top{0};
    int32_t right{0};
    int32_t bottom{0};

    lrect() = default;
    lrect(int32_t l, int32_t t, int32_t r, int32_t b) 
        : left(l), top(t), right(r), bottom(b) {}

    explicit lrect(const PF_LRect& r) : left(r.left), top(r.top), right(r.right), bottom(r.bottom) {}
    explicit lrect(const rect& r) : left(r.left), top(r.top), right(r.right), bottom(r.bottom) {}

    operator PF_LRect() const {
        PF_LRect r;
        r.left   = left;
        r.top    = top;
        r.right  = right;
        r.bottom = bottom;
        return r;
    }

    PF_LRect to_pf() const { return static_cast<PF_LRect>(*this); }

    int32_t width() const { return right - left; }
    int32_t height() const { return bottom - top; }
    bool empty() const { return left >= right || top >= bottom; }

    rect to_rect() const {
        return rect(*this);
    }

    PF_Rect to_pf_rect() const {
        PF_Rect r;
        r.left   = static_cast<int16_t>(std::clamp<int32_t>(left, -32768, 32767));
        r.top    = static_cast<int16_t>(std::clamp<int32_t>(top, -32768, 32767));
        r.right  = static_cast<int16_t>(std::clamp<int32_t>(right, -32768, 32767));
        r.bottom = static_cast<int16_t>(std::clamp<int32_t>(bottom, -32768, 32767));
        return r;
    }

    static lrect from_center(int32_t cx, int32_t cy, int32_t w, int32_t h) {
        return { cx - w/2, cy - h/2, cx + w/2, cy + h/2 };
    }
};

// Define out-of-line constructor for rect
inline rect::rect(const lrect& r)
    : left(std::clamp<int32_t>(r.left, -32768, 32767)),
      top(std::clamp<int32_t>(r.top, -32768, 32767)),
      right(std::clamp<int32_t>(r.right, -32768, 32767)),
      bottom(std::clamp<int32_t>(r.bottom, -32768, 32767)) {}

// ============================================================
//  rect_f
// ============================================================

/**
 * @brief Floating-point 2D bounds bounding structure.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Floating-point 2D bounds representation for UI layout, containing intersects and center calculations.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct rect_f {
    float left{0.0f};
    float top{0.0f};
    float width{0.0f};
    float height{0.0f};

    rect_f() = default;
    rect_f(float l, float t, float w, float h) 
        : left(l), top(t), width(w), height(h) {}

    bool empty() const { return width <= 0.0f || height <= 0.0f; }

    /**
     * @brief Create floating rect from center.
     *
     * @details Centered coordinate solver.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Layout helper.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param cx Center X coordinate.
     * @param cy Center Y coordinate.
     * @param w Width factor.
     * @param h Height factor.
     * @return Layout rectangle.
     */
    static rect_f from_center(float cx, float cy, float w, float h) {
        return { cx - w/2.0f, cy - h/2.0f, w, h };
    }

    /**
     * @brief Point containment test.
     *
     * @details Verifies if coordinates fall within bounds.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Hit testing helper.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param px Target X coordinate.
     * @param py Target Y coordinate.
     * @return True if point is inside.
     */
    bool contains(float px, float py) const {
        return px >= left && px < left + width && py >= top && py < top + height;
    }

    bool contains(const vec2& pt) const {
        return contains((float)pt.x, (float)pt.y);
    }

    /**
     * @brief Overlap test.
     *
     * @details Checks if bounds overlap with another rect.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Intersection validation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param o Comparison rectangle.
     * @return True if overlapping.
     */
    bool intersects(const rect_f& o) const {
        return !(left + width <= o.left || o.left + o.width <= left ||
                 top + height <= o.top || o.top + o.height <= top);
    }

    /**
     * @brief Get center point.
     *
     * @details Solves the geometric center coordinate.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Center calculation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Center vector.
     */
    vec2 center() const {
        return { left + width * 0.5, top + height * 0.5 };
    }
};

// ============================================================
//  matrix2d
// ============================================================

/**
 * @brief Modern 2D transformation matrix.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Modern 2D transformation matrix for Drawbot rendering or spatial transformations, supporting custom rotation and multiplication operations.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct matrix2d {
    /// Internal matrix coefficients.
    float m[3][3] = { 
        {1.0f, 0.0f, 0.0f}, 
        {0.0f, 1.0f, 0.0f}, 
        {0.0f, 0.0f, 1.0f} 
    };

    /**
     * @brief Creates identity matrix.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Identity initializer.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Identity matrix.
     */
    static matrix2d identity() { return matrix2d{}; }

    /**
     * @brief Creates translation matrix.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Offset generator.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param tx Translation X offset.
     * @param ty Translation Y offset.
     * @return Transformed matrix.
     */
    static matrix2d translation(float tx, float ty) {
        matrix2d mat;
        mat.m[2][0] = tx;
        mat.m[2][1] = ty;
        return mat;
    }

    /**
     * @brief Creates scaling matrix.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Scale factor mapping.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param sx Horizontal scale factor.
     * @param sy Vertical scale factor.
     * @return Scaled matrix.
     */
    static matrix2d scale(float sx, float sy) {
        matrix2d mat;
        mat.m[0][0] = sx;
        mat.m[1][1] = sy;
        return mat;
    }

    /**
     * @brief Creates rotation matrix.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Angle rotation builder.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param angle_rad Angle in radians.
     * @return Rotated matrix.
     */
    static matrix2d rotation(float angle_rad) {
        matrix2d mat;
        float c = std::cos(angle_rad);
        float s = std::sin(angle_rad);
        mat.m[0][0] = c;
        mat.m[0][1] = s;
        mat.m[1][0] = -s;
        mat.m[1][1] = c;
        return mat;
    }

    matrix2d operator*(const matrix2d& o) const {
        matrix2d res;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                res.m[i][j] = m[i][0] * o.m[0][j] + 
                              m[i][1] * o.m[1][j] + 
                              m[i][2] * o.m[2][j];
            }
        }
        return res;
    }

    matrix2d& operator*=(const matrix2d& o) {
        *this = *this * o;
        return *this;
    }
};

// ============================================================
//  color
// ============================================================

/**
 * @brief Floating-point color representation.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, colors are represented either as raw 8-bit integers or `AEGP_ColorVal` structs, making interpolation or format conversion hard. `aetk::core::color<>` offers floating-point color bounds, automatic clamp methods, and luminance/saturate filters.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
enum class pixel_range {
    tkfloat,
    tkuint8
};

template <pixel_range Range = pixel_range::tkfloat>
struct color {
    /// Alpha opacity factor (0.0 to 1.0 or 0.0 to 255.0).
    double alpha{Range == pixel_range::tkfloat ? 1.0 : 255.0};
    
    /// Red intensity level (0.0 to 1.0 or 0.0 to 255.0).
    double red{0.0};
    
    /// Green intensity level (0.0 to 1.0 or 0.0 to 255.0).
    double green{0.0};
    
    /// Blue intensity level (0.0 to 1.0 or 0.0 to 255.0).
    double blue{0.0};

    color() = default;
    
    color(double a, double r, double g, double b) : alpha(a), red(r), green(g), blue(b) {}
    color(double r, double g, double b) : alpha(Range == pixel_range::tkfloat ? 1.0 : 255.0), red(r), green(g), blue(b) {}

    color(const color&) = default;
    color& operator=(const color&) = default;
    color(color&&) noexcept = default;
    color& operator=(color&&) noexcept = default;

    template <pixel_range OtherRange, typename = std::enable_if_t<OtherRange != Range>>
    explicit color(const color<OtherRange>& other) {
        if constexpr (Range == pixel_range::tkfloat && OtherRange == pixel_range::tkuint8) {
            alpha = other.alpha * (1.0 / 255.0);
            red = other.red * (1.0 / 255.0);
            green = other.green * (1.0 / 255.0);
            blue = other.blue * (1.0 / 255.0);
        } else {
            alpha = other.alpha * 255.0;
            red = other.red * 255.0;
            green = other.green * 255.0;
            blue = other.blue * 255.0;
        }
    }

    color<pixel_range::tkfloat> to_float() const {
        if constexpr (Range == pixel_range::tkfloat) return *this;
        else return color<pixel_range::tkfloat>(*this);
    }

    color<pixel_range::tkuint8> to_uint8() const {
        if constexpr (Range == pixel_range::tkuint8) return *this;
        else return color<pixel_range::tkuint8>(*this);
    }

    static color from_int(int a, int r, int g, int b) {
        if constexpr (Range == pixel_range::tkfloat) {
            return {a / 255.0, r / 255.0, g / 255.0, b / 255.0};
        } else {
            return {static_cast<double>(a), static_cast<double>(r), static_cast<double>(g), static_cast<double>(b)};
        }
    }
    
    static color from_int(int r, int g, int b) {
        if constexpr (Range == pixel_range::tkfloat) {
            return {1.0, r / 255.0, g / 255.0, b / 255.0};
        } else {
            return {255.0, static_cast<double>(r), static_cast<double>(g), static_cast<double>(b)};
        }
    }

    void to_hsv(float& h, float& s, float& v) const {
        float r = (float)red;
        float g = (float)green;
        float b = (float)blue;
        if constexpr (Range == pixel_range::tkuint8) {
            r *= 1.0f / 255.0f;
            g *= 1.0f / 255.0f;
            b *= 1.0f / 255.0f;
        }
        float min_val = (std::min)({r, g, b});
        float max_val = (std::max)({r, g, b});
        float delta = max_val - min_val;

        v = max_val * 255.0f;
        s = (max_val > 0.0f) ? (delta / max_val * 255.0f) : 0.0f;

        if (delta > 0.0f) {
            if (max_val == r) {
                h = (g - b) / delta;
            } else if (max_val == g) {
                h = 2.0f + (b - r) / delta;
            } else {
                h = 4.0f + (r - g) / delta;
            }
            h *= 60.0f;
            if (h < 0.0f) h += 360.0f;
            h /= 2.0f; // H range [0, 180]
        } else {
            h = 0.0f;
        }
    }

    static color from_hsv(float h, float s, float v, float a = -1.0f) {
        float h_degrees = h * 2.0f; // Convert back to [0, 360]
        float s_norm = s / 255.0f;
        float v_norm = v / 255.0f;

        float c = v_norm * s_norm;
        float x = c * (1.0f - std::abs(std::fmod(h_degrees / 60.0f, 2.0f) - 1.0f));
        float m = v_norm - c;

        float r = 0, g = 0, b = 0;
        if (h_degrees >= 0 && h_degrees < 60) { r = c; g = x; b = 0; }
        else if (h_degrees >= 60 && h_degrees < 120) { r = x; g = c; b = 0; }
        else if (h_degrees >= 120 && h_degrees < 180) { r = 0; g = c; b = x; }
        else if (h_degrees >= 180 && h_degrees < 240) { r = 0; g = x; b = c; }
        else if (h_degrees >= 240 && h_degrees < 300) { r = x; g = 0; b = c; }
        else { r = c; g = 0; b = x; }

        double final_a = (a < 0.0f) ? ((Range == pixel_range::tkfloat) ? 1.0 : 255.0) : a;
        if constexpr (Range == pixel_range::tkuint8) {
            return { final_a, (r + m) * 255.0, (g + m) * 255.0, (b + m) * 255.0 };
        } else {
            return { final_a, (double)(r + m), (double)(g + m), (double)(b + m) };
        }
    }

    void to_hsl(double &h, double &s, double &l) const {
        double r = red;
        double g = green;
        double b = blue;
        if constexpr (Range == pixel_range::tkuint8) {
            r *= 1.0 / 255.0;
            g *= 1.0 / 255.0;
            b *= 1.0 / 255.0;
        }
        r = std::clamp(r, 0.0, 1.0);
        g = std::clamp(g, 0.0, 1.0);
        b = std::clamp(b, 0.0, 1.0);
        double max_val = (std::max)({r, g, b});
        double min_val = (std::min)({r, g, b});
        double delta = max_val - min_val;

        l = (max_val + min_val) * 0.5;

        if (delta < 1e-6) {
            h = 0.0;
            s = 0.0;
        } else {
            s = (l < 0.5) ? (delta / (max_val + min_val)) : (delta / (2.0 - max_val - min_val));

            if (max_val == r) {
                h = (g - b) / delta + (g < b ? 6.0 : 0.0);
            } else if (max_val == g) {
                h = (b - r) / delta + 2.0;
            } else {
                h = (r - g) / delta + 4.0;
            }
            h /= 6.0;
        }
    }

    static color from_hsl(double h, double s, double l, double a = -1.0) {
        h = std::fmod(h, 1.0);
        if (h < 0.0) h += 1.0;
        s = std::clamp(s, 0.0, 1.0);
        l = std::clamp(l, 0.0, 1.0);

        double r = l, g = l, b = l;
        if (s > 1e-6) {
            double q = (l < 0.5) ? (l * (1.0 + s)) : (l + s - l * s);
            double p = 2.0 * l - q;

            auto hue2rgb = [](double p_val, double q_val, double t) {
                if (t < 0.0) t += 1.0;
                if (t > 1.0) t -= 1.0;
                if (t < 1.0 / 6.0) return p_val + (q_val - p_val) * 6.0 * t;
                if (t < 1.0 / 2.0) return q_val;
                if (t < 2.0 / 3.0) return p_val + (q_val - p_val) * (2.0 / 3.0 - t) * 6.0;
                return p_val;
            };

            r = hue2rgb(p, q, h + 1.0 / 3.0);
            g = hue2rgb(p, q, h);
            b = hue2rgb(p, q, h - 1.0 / 3.0);
        }
        double final_a = (a < 0.0) ? ((Range == pixel_range::tkfloat) ? 1.0 : 255.0) : a;
        if constexpr (Range == pixel_range::tkuint8) {
            return color(final_a, r * 255.0, g * 255.0, b * 255.0).clamped();
        } else {
            return color(final_a, r, g, b).clamped();
        }
    }

    void to_yiq(double &y, double &i, double &q) const {
        double r = red;
        double g = green;
        double b = blue;
        if constexpr (Range == pixel_range::tkuint8) {
            r *= 1.0 / 255.0;
            g *= 1.0 / 255.0;
            b *= 1.0 / 255.0;
        }
        y = r * 0.2989 + g * 0.5866 + b * 0.1144;
        i = r * 0.5959 - g * 0.2741 - b * 0.3218;
        q = r * 0.2113 - g * 0.5227 + b * 0.3113;
    }

    static color from_yiq(double y, double i, double q, double a = -1.0) {
        double r = y + i * 0.9562 + q * 0.6210;
        double g = y - i * 0.2717 - q * 0.6485;
        double b = y - i * 1.1053 + q * 1.7020;
        double final_a = (a < 0.0) ? ((Range == pixel_range::tkfloat) ? 1.0 : 255.0) : a;
        if constexpr (Range == pixel_range::tkuint8) {
            return color(final_a, r * 255.0, g * 255.0, b * 255.0).clamped();
        } else {
            return color(final_a, r, g, b).clamped();
        }
    }

    explicit color(const AEGP_ColorVal& c) {
        if constexpr (Range == pixel_range::tkfloat) {
            alpha = c.alphaF;
            red = c.redF;
            green = c.greenF;
            blue = c.blueF;
        } else {
            alpha = c.alphaF * 255.0;
            red = c.redF * 255.0;
            green = c.greenF * 255.0;
            blue = c.blueF * 255.0;
        }
    }

    operator AEGP_ColorVal() const {
        AEGP_ColorVal c;
        if constexpr (Range == pixel_range::tkfloat) {
            c.alphaF = alpha;
            c.redF = red;
            c.greenF = green;
            c.blueF = blue;
        } else {
            c.alphaF = alpha * (1.0 / 255.0);
            c.redF = red * (1.0 / 255.0);
            c.greenF = green * (1.0 / 255.0);
            c.blueF = blue * (1.0 / 255.0);
        }
        return c;
    }

    static color mix(const color& a, const color& b, double t) {
        t = std::clamp(t, 0.0, 1.0);
        return {
            a.alpha + (b.alpha - a.alpha) * t,
            a.red + (b.red - a.red) * t,
            a.green + (b.green - a.green) * t,
            a.blue + (b.blue - a.blue) * t
        };
    }

    double luminance() const {
        return (red * 0.2126) + (green * 0.7152) + (blue * 0.0722);
    }

    color saturate(double amount) const {
        double y = luminance();
        return {
            alpha,
            y + (red - y) * amount,
            y + (green - y) * amount,
            y + (blue - y) * amount
        };
    }

    color clamped() const {
        double max_v = (Range == pixel_range::tkfloat) ? 1.0 : 255.0;
        return {
            std::clamp(alpha, 0.0, max_v),
            std::clamp(red, 0.0, max_v),
            std::clamp(green, 0.0, max_v),
            std::clamp(blue, 0.0, max_v)
        };
    }
};

} // namespace aetk::core
