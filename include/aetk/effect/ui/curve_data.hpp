#pragma once

#include <aetk/core/types.hpp>
#include <aetk/core/locale_utils.hpp>
#include <aetk/effect/params/arb_traits.hpp>
#include <aetk/effect/params/serialization.hpp>
#include <aetk/core/math.hpp>
#include <vector>
#include <algorithm>
#include <cstdio>

namespace aetk::effect::ui {

// ══════════════════════════════════════════════════════════════════════
//  Curve Point — a single control point on a curve
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief A single control point on a curve.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Standard Cartesian control point.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct curve_point {
    float x = 0.0f;
    float y = 0.0f;

    /**
     * @brief Serializes the control point coordinates.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Structured streaming.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @tparam Archive Streaming archive type.
     * @param ar Archive reference.
     */
    template <typename Archive>
    void serialize(Archive& ar) {
        ar & x & y;
    }

    bool operator==(const curve_point& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const curve_point& other) const {
        return !(*this == other);
    }
};

// ══════════════════════════════════════════════════════════════════════
//  Curve Data — serializable control point set for arbitrary data
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Serializable control point set for arbitrary data.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Custom spline data container.
 *
 * @warning <b>Memory & Lifecycles:</b> Points vector manages dynamic allocations inside AE arbitrary state memory.
 */
struct curve_data {
    std::vector<curve_point> points;
    int dragging_index = -1;

    /**
     * @brief Constructor setting default identity curve states.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe constructor initialization.
     *
     * @warning <b>Memory & Lifecycles:</b> Allocates initial default vectors.
     */
    curve_data() {
        // Identity curve: bottom-left to top-right (like AE Curves default)
        points.push_back({ 0.0f, 1.0f });  // bottom-left
        points.push_back({ 1.0f, 0.0f });  // top-right
        dragging_index = -1;
    }

    /**
     * @brief Serializes the entire control point spline.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Structured streaming.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @tparam Archive Streaming archive type.
     * @param ar Archive reference.
     */
    template <typename Archive>
    void serialize(Archive& ar) {
        ar & points & dragging_index;
    }

    bool operator==(const curve_data& other) const {
        return points == other.points && dragging_index == other.dragging_index;
    }

    bool operator!=(const curve_data& other) const {
        return !(*this == other);
    }
};

} // namespace aetk::effect::ui

// ══════════════════════════════════════════════════════════════════════
//  arb_traits specialization — handles AE lifecycle + keyframe interpolation
// ══════════════════════════════════════════════════════════════════════

namespace aetk::effect {

/**
 * @brief Arbitrary traits specialization for single curve data structures.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, storing custom spline and color mapping data requires allocating custom binary blocks inside arbitrary parameters, manually handling host lifecycle events (`dispose`, `copy`, `flatten`), and performing manual byte alignments. `aetk::effect::ui::curve_data` and `multi_curve_data` leverage `arb_traits` templates to automatically register custom structural types to the After Effects host. Furthermore, instead of simple step blending, it implements a highly advanced Catmull-Rom spline keyframe interpolator (`interpolate`) that automatically generates phantom control points when keyframe spline counts differ, delivering extremely smooth, flicker-free temporal morphs.
 *
 * @warning <b>Memory & Lifecycles:</b> Placement-new allocations and destructors must be handled correctly in all methods.
 */
template <>
struct arb_traits<ui::curve_data> {
    using T = ui::curve_data;

    /**
     * @brief Initialize arbitrary data structure.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Placement-new initialization.
     *
     * @warning <b>Memory & Lifecycles:</b> Executes placement-new into host allocations.
     *
     * @param ptr Target memory address.
     */
    static void init(T* ptr) { new (ptr) T(); }

    /**
     * @brief Dispose arbitrary data allocations.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automated destructor call.
     *
     * @warning <b>Memory & Lifecycles:</b> Invokes standard destructor to free vector allocations.
     *
     * @param ptr Target memory address.
     */
    static void dispose(T* ptr) { ptr->~T(); }

    /**
     * @brief Copy arbitrary data allocations.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automated copy allocation.
     *
     * @warning <b>Memory & Lifecycles:</b> Performs placement-new copy constructor.
     *
     * @param dst Destination address.
     * @param src Source address.
     */
    static void copy(T* dst, const T* src) { new (dst) T(*src); }

    /**
     * @brief Measures flat buffer size needed for serialization.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Streaming size calculation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ptr Spline pointer.
     * @return Bounded buffer size in bytes.
     */
    static size_t flat_size(const T* ptr) {
        serialization::size_archive ar;
        const_cast<T*>(ptr)->serialize(ar);
        return ar.size();
    }

    /**
     * @brief Flatten structures into a binary buffer.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automated stream flattening.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ptr Spline pointer.
     * @param buffer Bounded target buffer.
     * @param size Target size.
     */
    static void flatten(const T* ptr, void* buffer, size_t size) {
        serialization::binary_oarchive ar(buffer, size);
        const_cast<T*>(ptr)->serialize(ar);
    }

    /**
     * @brief Unflatten binary buffer back into struct fields.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Placement-new stream unflattening.
     *
     * @warning <b>Memory & Lifecycles:</b> Standard placement-new allocation.
     *
     * @param ptr Spline pointer.
     * @param buffer Bounded source buffer.
     * @param size Source size.
     */
    static void unflatten(T* ptr, const void* buffer, size_t size) {
        serialization::binary_iarchive ar(buffer, size);
        new (ptr) T();
        ptr->serialize(ar);
    }

    /**
     * @brief Performs Catmull-Rom keyframe interpolation between splines.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Catmull-Rom temporal keyframe splining.
     *
     * @warning <b>Memory & Lifecycles:</b> Standard placement-new allocation.
     *
     * @param dst Interpolated target.
     * @param left Source keyframe spline left.
     * @param right Source keyframe spline right.
     * @param t Temporal fraction value.
     */
    static void interpolate(T* dst, const T* left, const T* right, double t) {
        new (dst) T();
        dst->points.clear();
        dst->dragging_index = -1;
        // Per-point lerp when counts match perfectly
        if (left->points.size() == right->points.size()) {
            dst->points.resize(left->points.size());
            for (size_t i = 0; i < left->points.size(); ++i) {
                dst->points[i].x = left->points[i].x + (float)t * (right->points[i].x - left->points[i].x);
                dst->points[i].y = left->points[i].y + (float)t * (right->points[i].y - left->points[i].y);
            }
        } else {
            // Mismatched counts: use union of X coordinates to generate phantom points
            // This guarantees smooth shape interpolation regardless of how many points were added/removed
            std::vector<float> all_x;
            all_x.reserve(left->points.size() + right->points.size());
            for (const auto& p : left->points) all_x.push_back(p.x);
            for (const auto& p : right->points) all_x.push_back(p.x);

            std::sort(all_x.begin(), all_x.end());
            all_x.erase(std::unique(all_x.begin(), all_x.end(), [](float a, float b) {
                return std::abs(a - b) < 0.001f;
            }), all_x.end());

            dst->points.reserve(all_x.size());
            for (float x : all_x) {
                float y_left = core::math::evaluate_catmull_rom(left->points, x);
                float y_right = core::math::evaluate_catmull_rom(right->points, x);
                float y_dst = y_left + (float)t * (y_right - y_left);
                dst->points.push_back({x, y_dst});
            }
        }
    }

    /**
     * @brief Print debugging string to host logger.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Debugging string printing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ptr Spline pointer.
     * @param str Bounded string character target.
     * @param max_len Max string capacity.
     */
    static void print(const T* ptr, char* str, size_t max_len) {
        if (max_len > 0) {
            std::string out = "Curve:";
            for (size_t i = 0; i < ptr->points.size(); ++i) {
                char pt_buf[64];
                aetk::core::c_snprintf(pt_buf, sizeof(pt_buf), " (%.4f, %.4f)%s", 
                         ptr->points[i].x, ptr->points[i].y,
                         (i + 1 < ptr->points.size() ? "," : ""));
                out += pt_buf;
            }
            std::strncpy(str, out.c_str(), max_len);
            str[max_len - 1] = '\0';
        }
    }

    static size_t print_size(const T* ptr) {
        return 32 + ptr->points.size() * 32;
    }

    static bool compare(const T* a, const T* b) {
        return *a == *b;
    }

    static bool scan(T* ptr, const char* str) {
        if (std::strncmp(str, "Curve:", 6) != 0) return false;
        
        std::vector<ui::curve_point> pts;
        const char* p = str + 6;
        float x, y;
        while (true) {
            p = std::strchr(p, '(');
            if (!p) break;
            if (aetk::core::c_sscanf(p, "(%f, %f)", &x, &y) == 2) {
                pts.push_back({x, y});
            }
            p++;
        }
        if (!pts.empty()) {
            ptr->points = std::move(pts);
            ptr->dragging_index = -1;
            return true;
        }
        return false;
    }
};

} // namespace aetk::effect

namespace aetk::effect::ui {

// ══════════════════════════════════════════════════════════════════════
//  Multi Curve Data — manages multiple channels
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Manages multiple curve channels.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Multi-channel spline editor mapping.
 *
 * @warning <b>Memory & Lifecycles:</b> Channels vector manages dynamic allocations inside AE arbitrary state memory.
 */
struct multi_curve_data {
    std::vector<curve_data> channels;

    /**
     * @brief Serializes all spline channels.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Structured streaming.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @tparam Archive Streaming archive type.
     * @param ar Archive reference.
     */
    template <typename Archive>
    void serialize(Archive& ar) {
        ar & channels;
    }

    bool operator==(const multi_curve_data& other) const {
        return channels == other.channels;
    }

    bool operator!=(const multi_curve_data& other) const {
        return !(*this == other);
    }
};

} // namespace aetk::effect::ui

namespace aetk::effect {

/**
 * @brief Arbitrary traits specialization for multi-curve data structures.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, storing custom spline and color mapping data requires allocating custom binary blocks inside arbitrary parameters, manually handling host lifecycle events (`dispose`, `copy`, `flatten`), and performing manual byte alignments. `aetk::effect::ui::curve_data` and `multi_curve_data` leverage `arb_traits` templates to automatically register custom structural types to the After Effects host. Furthermore, instead of simple step blending, it implements a highly advanced Catmull-Rom spline keyframe interpolator (`interpolate`) that automatically generates phantom control points when keyframe spline counts differ, delivering extremely smooth, flicker-free temporal morphs.
 *
 * @warning <b>Memory & Lifecycles:</b> Placement-new allocations and destructors must be handled correctly in all methods.
 */
template <>
struct arb_traits<ui::multi_curve_data> {
    using T = ui::multi_curve_data;

    /**
     * @brief Initialize arbitrary data structure.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Placement-new initialization.
     *
     * @warning <b>Memory & Lifecycles:</b> Executes placement-new into host allocations.
     *
     * @param ptr Target memory address.
     */
    static void init(T* ptr) { new (ptr) T(); }

    /**
     * @brief Dispose arbitrary data allocations.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automated destructor call.
     *
     * @warning <b>Memory & Lifecycles:</b> Invokes standard destructor to free vector allocations.
     *
     * @param ptr Target memory address.
     */
    static void dispose(T* ptr) { ptr->~T(); }

    /**
     * @brief Copy arbitrary data allocations.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automated copy allocation.
     *
     * @warning <b>Memory & Lifecycles:</b> Performs placement-new copy constructor.
     *
     * @param dst Destination address.
     * @param src Source address.
     */
    static void copy(T* dst, const T* src) { new (dst) T(*src); }

    /**
     * @brief Measures flat buffer size needed for serialization.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Streaming size calculation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ptr Spline pointer.
     * @return Bounded buffer size in bytes.
     */
    static size_t flat_size(const T* ptr) {
        serialization::size_archive ar;
        const_cast<T*>(ptr)->serialize(ar);
        return ar.size();
    }

    /**
     * @brief Flatten structures into a binary buffer.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automated stream flattening.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ptr Spline pointer.
     * @param buffer Bounded target buffer.
     * @param size Target size.
     */
    static void flatten(const T* ptr, void* buffer, size_t size) {
        serialization::binary_oarchive ar(buffer, size);
        const_cast<T*>(ptr)->serialize(ar);
    }

    /**
     * @brief Unflatten binary buffer back into struct fields.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Placement-new stream unflattening.
     *
     * @warning <b>Memory & Lifecycles:</b> Standard placement-new allocation.
     *
     * @param ptr Spline pointer.
     * @param buffer Bounded source buffer.
     * @param size Source size.
     */
    static void unflatten(T* ptr, const void* buffer, size_t size) {
        serialization::binary_iarchive ar(buffer, size);
        new (ptr) T();
        ptr->serialize(ar);
    }

    /**
     * @brief Performs Catmull-Rom keyframe interpolation between multi-channel splines.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Catmull-Rom temporal keyframe splining.
     *
     * @warning <b>Memory & Lifecycles:</b> Standard placement-new allocation.
     *
     * @param dst Interpolated target.
     * @param left Source keyframe spline left.
     * @param right Source keyframe spline right.
     * @param t Temporal fraction value.
     */
    static void interpolate(T* dst, const T* left, const T* right, double t) {
        new (dst) T();
        dst->channels.clear();
        
        size_t count = (std::max)(left->channels.size(), right->channels.size());
        dst->channels.resize(count);
        
        for (size_t i = 0; i < count; ++i) {
            const auto* l = i < left->channels.size() ? &left->channels[i] : nullptr;
            const auto* r = i < right->channels.size() ? &right->channels[i] : nullptr;
            
            if (l && r) {
                alignas(ui::curve_data) char buf[sizeof(ui::curve_data)];
                auto* temp = reinterpret_cast<ui::curve_data*>(buf);
                arb_traits<ui::curve_data>::interpolate(temp, l, r, t);
                dst->channels[i] = std::move(*temp);
                temp->~curve_data();
            } else if (l) {
                dst->channels[i] = *l;
            } else if (r) {
                dst->channels[i] = *r;
            }
        }
    }

    /**
     * @brief Print debugging string to host logger.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Debugging string printing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ptr Spline pointer.
     * @param str Bounded string character target.
     * @param max_len Max string capacity.
     */
    static void print(const T* ptr, char* str, size_t max_len) {
        if (max_len > 0) {
            std::string out = "MultiCurve:";
            for (size_t i = 0; i < ptr->channels.size(); ++i) {
                char sub_buf[1024];
                arb_traits<ui::curve_data>::print(&ptr->channels[i], sub_buf, sizeof(sub_buf));
                out += " [";
                out += sub_buf;
                out += "]";
            }
            std::strncpy(str, out.c_str(), max_len);
            str[max_len - 1] = '\0';
        }
    }

    static size_t print_size(const T* ptr) {
        size_t total = 32;
        for (const auto& ch : ptr->channels) {
            total += arb_traits<ui::curve_data>::print_size(&ch) + 8;
        }
        return total;
    }

    static bool compare(const T* a, const T* b) {
        return *a == *b;
    }

    static bool scan(T* ptr, const char* str) {
        if (std::strncmp(str, "MultiCurve:", 11) != 0) return false;
        
        std::vector<ui::curve_data> chans;
        const char* p = str + 11;
        while (true) {
            p = std::strchr(p, '[');
            if (!p) break;
            p++;
            const char* end = std::strchr(p, ']');
            if (!end) break;
            std::string sub_str(p, end - p);
            ui::curve_data cd;
            if (arb_traits<ui::curve_data>::scan(&cd, sub_str.c_str())) {
                chans.push_back(std::move(cd));
            }
            p = end + 1;
        }
        if (!chans.empty()) {
            ptr->channels = std::move(chans);
            return true;
        }
        return false;
    }
};

} // namespace aetk::effect
