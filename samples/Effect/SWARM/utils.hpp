#pragma once
#include "OrtEngine.h"
#include "coco.hpp"
#include "oi7.hpp"
#include <aetk/effect.hpp>
#include <aetk/effect/gpu.hpp>
#include <aetk/effect/pixel/compute_cache.hpp>
#include <aetk/effect/ui/widgets/curve_editor.hpp>
#include <aetk/effect/ui/widgets/joystick.hpp>
#include <aetk/effect/ui/widgets/triple_joystick.hpp>
#include <algorithm>
#include <cmath>
#include <cuda_runtime.h>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

using namespace aetk::effect;

namespace aetk::effect::ui {
std::mutex g_detected_classes_mutex;
std::set<std::string> g_detected_classes;
} // namespace aetk::effect::ui
#ifdef _WIN32
#include <windows.h>
static std::wstring GetPluginDirectoryW() {
    HMODULE hMod = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
            | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&GetPluginDirectoryW, &hMod);
    if (!hMod)
        return L"";

    wchar_t path[MAX_PATH];
    GetModuleFileNameW(hMod, path, MAX_PATH);
    wchar_t* last_slash = wcsrchr(path, L'\\');
    if (last_slash)
        *last_slash = L'\0';
    return path;
}

static std::string GetPluginDirectoryA() {
    std::wstring wdir = GetPluginDirectoryW();
    int len
        = WideCharToMultiByte(CP_UTF8, 0, wdir.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string dir(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wdir.c_str(), -1, &dir[0], len, nullptr, nullptr);
    if (!dir.empty() && dir.back() == '\0')
        dir.pop_back();
    return dir;
}

static bool file_exists(const std::string& path) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), (int)path.size(), nullptr, 0);
    std::wstring wpath(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), (int)path.size(), &wpath[0], wlen);
    DWORD dwAttrib = GetFileAttributesW(wpath.c_str());
    return (
        dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}
#else
static std::string GetPluginDirectoryA() {
    return ".";
}
static bool file_exists(const std::string& path) {
    return true;
}
#endif

// Thread-safe deduplicating logging helper
void log_unique(const std::string& msg) {
    static std::unordered_set<std::string> logged_messages;
    static std::mutex log_mutex;

    std::lock_guard<std::mutex> lock(log_mutex);
    if (logged_messages.find(msg) == logged_messages.end()) {
        logged_messages.insert(msg);
        AETK_LOG_INFO(msg);
    }
}

inline std::string get_class_name(int class_id, int total_classes = 601) {
    static std::vector<std::string> parsed_names;
    static std::string last_raw_metadata;
    static std::mutex names_mutex;

    std::lock_guard<std::mutex> lock(names_mutex);

    std::string raw = OrtEngine::GetMetadata("yolo", "names");
    if (raw != last_raw_metadata) {
        last_raw_metadata = raw;
        parsed_names.clear();
        if (!raw.empty()) {
            for (int i = 0; i < 1000; i++) {
                std::string key_pattern = "\"" + std::to_string(i) + "\":";
                size_t pos = raw.find(key_pattern);
                if (pos == std::string::npos) {
                    key_pattern = " " + std::to_string(i) + ":";
                    pos = raw.find(key_pattern);
                }
                if (pos != std::string::npos) {
                    size_t val_start = raw.find('\"', pos + key_pattern.length());
                    char quote_char = '\"';
                    size_t single_start = raw.find('\'', pos + key_pattern.length());
                    if (val_start == std::string::npos
                        || (single_start != std::string::npos
                            && single_start < val_start)) {
                        val_start = single_start;
                        quote_char = '\'';
                    }
                    if (val_start != std::string::npos) {
                        size_t val_end = raw.find(quote_char, val_start + 1);
                        if (val_end != std::string::npos) {
                            std::string val
                                = raw.substr(val_start + 1, val_end - val_start - 1);
                            for (auto& c : val)
                                c = std::toupper(c);
                            if ((int)parsed_names.size() <= i) {
                                parsed_names.resize(i + 1);
                            }
                            parsed_names[i] = val;
                            continue;
                        }
                    }
                }
                if (i > 601 && parsed_names.empty())
                    break;
            }
        }
    }

    if (class_id >= 0 && class_id < (int)parsed_names.size()
        && !parsed_names[class_id].empty()) {
        return parsed_names[class_id];
    }

    if (total_classes <= 80 && class_id >= 0 && class_id < 80) {
        std::string coco_label = COCO_CLASSES[class_id];
        for (auto& c : coco_label) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return coco_label;
    }

    if (class_id >= 0 && class_id < 601) {
        return OIV7_CLASSES[class_id];
    }

    return "UNKNOWN";
}

// ══════════════════════════════════════════════════════════════════════
//  Detection Data Structures
// ══════════════════════════════════════════════════════════════════════
// ══════════════════════════════════════════════════════════════════════
//  Connected Components downscaled BFS routines
// ══════════════════════════════════════════════════════════════════════

extern "C" void launch_silhouette_downscale_cuda(const void* src_ptr, int src_pitch,
    int src_w, int src_h, int format, float* dst_gray, int dst_w, int dst_h);

extern "C" void launch_hsv_threshold_cuda(const void* src_ptr, int src_pitch, int src_w,
    int src_h, int format, unsigned char* dst_mask, int dst_w, int dst_h, float target_h,
    float target_s, float target_v, float h_tol, float s_tol, float v_tol);

struct GridCell {
    int x, y;
};

struct Detection {
    float xmin = 0.0f;
    float ymin = 0.0f;
    float xmax = 0.0f;
    float ymax = 0.0f;
    float cx = 0.0f;
    float cy = 0.0f;
    float confidence = 0.0f;
    std::string class_name;
};

struct Edge {
    int u = 0;
    int v = 0;
    float weight = 0.0f;
};

namespace aetk::effect {
template <>
struct compute_cache_traits<::Detection> {
    static size_t approx_size(const ::Detection* val) {
        if (!val)
            return 0;
        return sizeof(::Detection) + approx_size_heap_bytes(val->class_name);
    }
};
}

