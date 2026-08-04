#pragma once

#include <aetk/core/locale_utils.hpp>
#include <aetk/effect/ui/widgets/joystick_data.hpp>

namespace aetk::effect::ui {

/**
 * @brief Data structure storing SWARM tracking joystick coordinates and consolidated UI modes.
 */
struct console_joystick_data {
    joystick_data silhouette;
    joystick_data hsv;
    joystick_data yolo;

    int output_mode = 0;   // 0: Composite, 1: Transparent, 2: Matte
    int detect_style = 2;  // 0: Dark, 1: Bright, 2: Auto
    int spawning_mode = 0; // 0: Centroid, 1: Grid Fill, 2: Contour Outline

    bool operator==(const console_joystick_data& other) const {
        return silhouette == other.silhouette && hsv == other.hsv && yolo == other.yolo
            && output_mode == other.output_mode && detect_style == other.detect_style
            && spawning_mode == other.spawning_mode;
    }

    bool operator!=(const console_joystick_data& other) const {
        return !(*this == other);
    }
};

} // namespace aetk::effect::ui

#include <aetk/effect/params/arb_traits.hpp>
#include <cstdio>
#include <cstring>

namespace aetk::effect {

template <>
struct arb_traits<ui::console_joystick_data> {
    using T = ui::console_joystick_data;

    static void init(T* ptr) { new (ptr) T(); }
    static void dispose(T* ptr) { ptr->~T(); }
    static void copy(T* dst, const T* src) { new (dst) T(*src); }
    static size_t flat_size(const T* ptr) { return sizeof(T); }

    static void flatten(const T* ptr, void* buffer, size_t size) {
        if (size >= sizeof(T)) {
            std::memcpy(buffer, ptr, sizeof(T));
        }
    }

    static void unflatten(T* ptr, const void* buffer, size_t size) {
        if (size >= sizeof(T)) {
            std::memcpy(ptr, buffer, sizeof(T));
        }
    }

    static void interpolate(T* dst, const T* left, const T* right, double t) {
        new (dst) T();
        dst->silhouette.x = left->silhouette.x + static_cast<float>(t * (right->silhouette.x - left->silhouette.x));
        dst->silhouette.y = left->silhouette.y + static_cast<float>(t * (right->silhouette.y - left->silhouette.y));
        dst->hsv.x = left->hsv.x + static_cast<float>(t * (right->hsv.x - left->hsv.x));
        dst->hsv.y = left->hsv.y + static_cast<float>(t * (right->hsv.y - left->hsv.y));
        dst->yolo.x = left->yolo.x + static_cast<float>(t * (right->yolo.x - left->yolo.x));
        dst->yolo.y = left->yolo.y + static_cast<float>(t * (right->yolo.y - left->yolo.y));
        
        // Integer mode selections use nearest-step interpolation
        dst->output_mode = (t >= 0.5) ? right->output_mode : left->output_mode;
        dst->detect_style = (t >= 0.5) ? right->detect_style : left->detect_style;
        dst->spawning_mode = (t >= 0.5) ? right->spawning_mode : left->spawning_mode;
    }

    static void print(const T* ptr, char* str, size_t max_len) {
        if (max_len > 0) {
            aetk::core::c_snprintf(str, max_len, "[Sil: (%.2f, %.2f), Hsv: (%.2f, %.2f), Yolo: (%.2f, %.2f), Out: %d, Det: %d, Spawn: %d]",
                ptr->silhouette.x, ptr->silhouette.y,
                ptr->hsv.x, ptr->hsv.y,
                ptr->yolo.x, ptr->yolo.y,
                ptr->output_mode, ptr->detect_style, ptr->spawning_mode);
        }
    }

    static size_t print_size(const T* ptr) {
        return 320;
    }

    static bool compare(const T* a, const T* b) {
        return *a == *b;
    }

    static bool scan(T* ptr, const char* str) {
        float sil_x = 0.0f, sil_y = 0.0f;
        float hsv_x = 0.0f, hsv_y = 0.0f;
        float yolo_x = 0.0f, yolo_y = 0.0f;
        int out_m = 0, det_s = 2, spawn_m = 0;
        if (aetk::core::c_sscanf(str, "[Sil: (%f, %f), Hsv: (%f, %f), Yolo: (%f, %f), Out: %d, Det: %d, Spawn: %d]",
                       &sil_x, &sil_y, &hsv_x, &hsv_y, &yolo_x, &yolo_y, &out_m, &det_s, &spawn_m) >= 6) {
            ptr->silhouette.x = sil_x;
            ptr->silhouette.y = sil_y;
            ptr->hsv.x = hsv_x;
            ptr->hsv.y = hsv_y;
            ptr->yolo.x = yolo_x;
            ptr->yolo.y = yolo_y;
            ptr->output_mode = out_m;
            ptr->detect_style = det_s;
            ptr->spawning_mode = spawn_m;
            return true;
        }
        return false;
    }
};

} // namespace aetk::effect
