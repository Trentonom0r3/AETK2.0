#pragma once

#include <aetk/core/locale_utils.hpp>
#include <aetk/effect/context/context.hpp>
#include <aetk/effect/params/arb_traits.hpp>
#include <aetk/effect/ui/widget.hpp>
#include <aetk/ui/theme.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace aetk::effect::ui {

// Global thread-safe set populated by render thread
extern std::mutex g_detected_classes_mutex;
extern std::set<std::string> g_detected_classes;

struct class_color_entry {
    char name[32] = { 0 };
    char alias[32] = { 0 };
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    bool disabled = false;
    char _padding[3] = { 0, 0, 0 }; // Forces the compiler to zero the garbage bytes

    bool operator==(const class_color_entry& other) const {
        return std::strcmp(name, other.name) == 0 && std::strcmp(alias, other.alias) == 0
            && r == other.r && g == other.g && b == other.b && disabled == other.disabled;
    }
};

struct class_color_map {
    static constexpr int MAX_ENTRIES = 80;
    int num_entries = 0;
    class_color_entry entries[MAX_ENTRIES];

    static std::string normalize_class_name(const std::string& class_name) {
        std::string name_upper = class_name;
        for (auto& c : name_upper) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return name_upper;
    }

    static bool is_effectively_black(float r, float g, float b) {
        return (std::max)({ r, g, b }) <= 0.02f;
    }

    static void make_visible_default_color(
        const std::string& class_name, float& out_r, float& out_g, float& out_b) {
        std::string name_upper = normalize_class_name(class_name);

        const uint32_t seed
            = static_cast<uint32_t>(std::hash<std::string> { }(name_upper));
        const float hue = static_cast<float>(seed % 360u);
        constexpr float palette_s = 0.85f * 255.0f;
        constexpr float palette_v = 1.0f * 255.0f;
        auto temp_col = core::color<>::from_hsv(hue, palette_s, palette_v);

        out_r = temp_col.red;
        out_g = temp_col.green;
        out_b = temp_col.blue;
    }

    static void sanitize_visible_color(
        const std::string& class_name, float& r, float& g, float& b) {
        if (is_effectively_black(r, g, b)) {
            make_visible_default_color(class_name, r, g, b);
        }
    }

    int find_entry_index(const std::string& class_name) const {
        const std::string name_upper = normalize_class_name(class_name);
        for (int i = 0; i < num_entries; ++i) {
            if (name_upper == entries[i].name) {
                return i;
            }
        }
        return -1;
    }

    bool operator==(const class_color_map& other) const {
        if (num_entries != other.num_entries)
            return false;
        for (int i = 0; i < num_entries; ++i) {
            if (!(entries[i] == other.entries[i]))
                return false;
        }
        return true;
    }

    bool operator!=(const class_color_map& other) const {
        return !(*this == other);
    }

    bool get_color(
        const std::string& class_name, float& out_r, float& out_g, float& out_b) const {
        const int idx = find_entry_index(class_name);
        if (idx >= 0) {
            out_r = entries[idx].r;
            out_g = entries[idx].g;
            out_b = entries[idx].b;
            sanitize_visible_color(entries[idx].name, out_r, out_g, out_b);
            return true;
        }
        return false;
    }

    void set_color(const std::string& class_name, float r, float g, float b) {
        const std::string name_upper = normalize_class_name(class_name);
        sanitize_visible_color(name_upper, r, g, b);
        const int idx = find_entry_index(name_upper);
        if (idx >= 0) {
            entries[idx].r = r;
            entries[idx].g = g;
            entries[idx].b = b;
            return;
        }
        if (num_entries < MAX_ENTRIES) {
            std::strncpy(entries[num_entries].name, name_upper.c_str(), 31);
            entries[num_entries].name[31] = '\0';
            entries[num_entries].r = r;
            entries[num_entries].g = g;
            entries[num_entries].b = b;
            entries[num_entries].disabled = false;
            num_entries++;
        }
    }

    bool get_disabled(const std::string& class_name) const {
        const int idx = find_entry_index(class_name);
        if (idx >= 0) {
            return entries[idx].disabled;
        }
        return false;
    }

    std::string get_display_name(const std::string& class_name) const {
        const int idx = find_entry_index(class_name);
        if (idx >= 0 && entries[idx].alias[0] != '\0') {
            return std::string(entries[idx].alias);
        }
        return class_name;
    }

    std::string get_alias(const std::string& class_name) const {
        const int idx = find_entry_index(class_name);
        if (idx >= 0 && entries[idx].alias[0] != '\0') {
            return std::string(entries[idx].alias);
        }
        return "";
    }

    void set_alias(const std::string& class_name, const std::string& alias_text) {
        const std::string name_upper = normalize_class_name(class_name);
        int idx = find_entry_index(name_upper);
        if (idx < 0) {
            float r = 1.0f, g = 1.0f, b = 1.0f;
            make_visible_default_color(name_upper, r, g, b);
            set_color(name_upper, r, g, b);
            idx = find_entry_index(name_upper);
        }
        if (idx >= 0) {
            std::memset(entries[idx].alias, 0, sizeof(entries[idx].alias));
            std::string alias_upper = normalize_class_name(alias_text);
            PF_STRNNCPY(entries[idx].alias, alias_upper.c_str(), sizeof(entries[idx].alias));
        }
    }

    void set_disabled(const std::string& class_name, bool disabled) {
        const std::string name_upper = normalize_class_name(class_name);
        const int idx = find_entry_index(name_upper);
        if (idx >= 0) {
            entries[idx].disabled = disabled;
            return;
        }
        if (num_entries < MAX_ENTRIES) {
            std::strncpy(entries[num_entries].name, name_upper.c_str(), 31);
            entries[num_entries].name[31] = '\0';
            make_visible_default_color(name_upper, entries[num_entries].r,
                entries[num_entries].g, entries[num_entries].b);
            entries[num_entries].disabled = disabled;
            num_entries++;
        }
    }

    static void interpolate_into(class_color_map* dst, const class_color_map* left,
        const class_color_map* right, double t) {
        new (dst) class_color_map();
        const double t_clamped = std::clamp(t, 0.0, 1.0);

        auto append_or_blend = [&](const char* raw_name) {
            if (!raw_name || !raw_name[0] || dst->num_entries >= MAX_ENTRIES) {
                return;
            }

            const std::string name = normalize_class_name(raw_name);
            if (dst->find_entry_index(name) >= 0) {
                return; // Already processed
            }

            // 1. Always guarantee a left and right color to blend across
            float left_r, left_g, left_b;
            if (!left->get_color(name, left_r, left_g, left_b)) {
                make_visible_default_color(name, left_r, left_g, left_b);
            }

            float right_r, right_g, right_b;
            if (!right->get_color(name, right_r, right_g, right_b)) {
                make_visible_default_color(name, right_r, right_g, right_b);
            }

            // 2. Unconditionally perform the smooth math lerp
            float out_r = left_r + static_cast<float>(t_clamped * (right_r - left_r));
            float out_g = left_g + static_cast<float>(t_clamped * (right_g - left_g));
            float out_b = left_b + static_cast<float>(t_clamped * (right_b - left_b));

            dst->set_color(name, out_r, out_g, out_b);

            // 3. Hard swap the disabled boolean & alias halfway through the transition
            bool disabled = (t_clamped < 0.5) ? left->get_disabled(name)
                                              : right->get_disabled(name);
            dst->set_disabled(name, disabled);

            std::string alias = (t_clamped < 0.5) ? left->get_alias(name)
                                                  : right->get_alias(name);
            if (!alias.empty()) {
                dst->set_alias(name, alias);
            }
        };

        for (int i = 0; i < left->num_entries; ++i) {
            append_or_blend(left->entries[i].name);
        }
        for (int i = 0; i < right->num_entries; ++i) {
            append_or_blend(right->entries[i].name);
        }
    }
};

#ifdef _WIN32
#include <windows.h>
#include <optional>

inline std::optional<std::string> prompt_class_alias_dialog(
    const std::string& raw_name, const std::string& current_alias) {
    struct InputData {
        std::wstring raw_name_w;
        std::wstring current_alias_w;
        std::string result;
        bool confirmed = false;
        HWND hwnd_edit = NULL;
    } data;

    data.raw_name_w = std::wstring(raw_name.begin(), raw_name.end());
    data.current_alias_w = std::wstring(current_alias.begin(), current_alias.end());

    static bool registered = false;
    static const wchar_t* CLS_NAME = L"AETK_RenameClassPromptWnd";

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
        InputData* pData = (InputData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
            pData = (InputData*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pData);

            std::wstring prompt_str = L"Enter custom display label for '" + pData->raw_name_w + L"':";
            CreateWindowExW(0, L"STATIC", prompt_str.c_str(), WS_CHILD | WS_VISIBLE,
                16, 16, 260, 20, hwnd, NULL, NULL, NULL);

            pData->hwnd_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pData->current_alias_w.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                16, 40, 260, 24, hwnd, (HMENU)101, NULL, NULL);

            CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                110, 75, 75, 26, hwnd, (HMENU)IDOK, NULL, NULL);

            CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE,
                195, 75, 75, 26, hwnd, (HMENU)IDCANCEL, NULL, NULL);

            SendMessageW(pData->hwnd_edit, EM_SETSEL, 0, -1);
            SetFocus(pData->hwnd_edit);
            return 0;
        }
        case WM_COMMAND: {
            if (LOWORD(wp) == IDOK && pData) {
                wchar_t buf[256] = { 0 };
                GetWindowTextW(pData->hwnd_edit, buf, 256);
                int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, NULL, 0, NULL, NULL);
                if (len > 0) {
                    pData->result.resize(len - 1);
                    WideCharToMultiByte(CP_UTF8, 0, buf, -1, &pData->result[0], len, NULL, NULL);
                }
                pData->confirmed = true;
                DestroyWindow(hwnd);
                return 0;
            } else if (LOWORD(wp) == IDCANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    };

    if (!registered) {
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = CLS_NAME;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassW(&wc);
        registered = true;
    }

    HWND hwnd_parent = GetActiveWindow();
    if (!hwnd_parent) hwnd_parent = GetForegroundWindow();

    int win_w = 300, win_h = 150;
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int pos_x = (screen_w - win_w) / 2;
    int pos_y = (screen_h - win_h) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
        CLS_NAME,
        L"Rename Class Display Alias",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        pos_x, pos_y, win_w, win_h,
        hwnd_parent, NULL, GetModuleHandle(NULL), &data);

    if (hwnd) {
        EnableWindow(hwnd_parent, FALSE);
        MSG msg;
        while (GetMessageW(&msg, NULL, 0, 0)) {
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
                SendMessageW(hwnd, WM_COMMAND, IDOK, 0);
                continue;
            }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
                SendMessageW(hwnd, WM_COMMAND, IDCANCEL, 0);
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        EnableWindow(hwnd_parent, TRUE);
        SetForegroundWindow(hwnd_parent);
    }

    if (data.confirmed) {
        return data.result;
    }
#endif
    return std::nullopt;
}

// Rounded rectangle path builder utility
inline aetk::ui::drawbot::path create_rounded_rect_path(
    drawbot::supplier& supplier, float x, float y, float w, float h, float r) {
    auto builder = supplier.create_path();
    builder.move_to(x + r, y);
    builder.line_to(x + w - r, y);
    builder.bezier_to(core::vec2 { x + w - r / 2, y }, core::vec2 { x + w, y + r / 2 },
        core::vec2 { x + w, y + r });
    builder.line_to(x + w, y + h - r);
    builder.bezier_to(core::vec2 { x + w, y + h - r / 2 },
        core::vec2 { x + w - r / 2, y + h }, core::vec2 { x + w - r, y + h });
    builder.line_to(x + r, y + h);
    builder.bezier_to(core::vec2 { x + r / 2, y + h }, core::vec2 { x, y + h - r / 2 },
        core::vec2 { x, y + h - r });
    builder.line_to(x, y + r);
    builder.bezier_to(core::vec2 { x, y + r / 2 }, core::vec2 { x + r / 2, y },
        core::vec2 { x + r, y });
    builder.close();
    return builder.build();
}

inline aetk::ui::drawbot::path create_circle_path(
    drawbot::supplier& supplier, float cx, float cy, float r) {
    auto builder = supplier.create_path();

    // Magic constant for a perfect circle via 4 cubic Beziers
    float kappa = 0.552284749831f;
    float offset = r * kappa;

    builder.move_to(cx, cy - r);
    builder.bezier_to(core::vec2 { cx + offset, cy - r },
        core::vec2 { cx + r, cy - offset }, core::vec2 { cx + r, cy });
    builder.bezier_to(core::vec2 { cx + r, cy + offset },
        core::vec2 { cx + offset, cy + r }, core::vec2 { cx, cy + r });
    builder.bezier_to(core::vec2 { cx - offset, cy + r },
        core::vec2 { cx - r, cy + offset }, core::vec2 { cx - r, cy });
    builder.bezier_to(core::vec2 { cx - r, cy - offset },
        core::vec2 { cx - offset, cy - r }, core::vec2 { cx, cy - r });

    builder.close(); // Ensure the shape is sealed for filling
    return builder.build();
}

// Vector Eye Path outlines
inline aetk::ui::drawbot::path create_eye_path(
    drawbot::supplier& supplier, float cx, float cy) {
    auto builder = supplier.create_path();
    builder.move_to(cx - 6.0f, cy);
    builder.bezier_to(core::vec2 { cx - 3.0f, cy - 4.0f },
        core::vec2 { cx + 3.0f, cy - 4.0f }, core::vec2 { cx + 6.0f, cy });
    builder.bezier_to(core::vec2 { cx + 3.0f, cy + 4.0f },
        core::vec2 { cx - 3.0f, cy + 4.0f }, core::vec2 { cx - 6.0f, cy });
    builder.close();
    return builder.build();
}

inline aetk::ui::drawbot::path create_pupil_path(
    drawbot::supplier& supplier, float cx, float cy) {
    auto builder = supplier.create_path();
    builder.add_arc(core::vec2 { cx, cy }, 1.8f, 0.0f, 2.0f * 3.14159f);
    return builder.build();
}

class yolo_class_customizer : public widget {
public:
    using data_type = class_color_map;

    static data_type get_default_data() {
        data_type d;
        d.set_color("PERSON", 0.0f, 1.0f, 0.4f);
        d.set_color("VEHICLE", 1.0f, 0.2f, 0.2f);
        d.set_color("ANIMAL", 1.0f, 0.8f, 0.0f);
        d.set_color("DEVICE", 0.0f, 0.8f, 1.0f);
        return d;
    }

    int32_t m_param_index = 0;
    class_color_map m_map;

    std::vector<std::string> m_sorted_classes;
    std::string m_color_pick_class;
    std::string m_rename_pick_class;
    bool m_needs_color_pick = false;
    bool m_needs_rename = false;
    bool m_needs_commit = false;

    // Horizontal Scrolling states
    float m_scroll_x = 0.0f;
    bool m_is_dragging_scrollbar = false;
    bool m_is_dragging_list = false;
    float m_drag_start_x = 0.0f;
    float m_drag_start_scroll_x = 0.0f;

    // Layout constants
    // Updated Layout constants
    const float CARD_W = 50.0f; // Shrunk from 64
    const float CARD_H = 60.0f; // Shrunk from 74
    const float ORB_RADIUS = 9.0f; // Scaled down to match

    enum class hover_type { none, card_bg, color_orb, edit_icon, scrollbar_thumb, scrollbar_track };
    hover_type m_hover_type = hover_type::none;
    int m_hovered_index = -1;

    yolo_class_customizer(int32_t param_idx)
        : m_param_index(param_idx) {
        layout.min_width = 300.0f;
    }

    void update_sorted_classes() {
        {
            std::lock_guard<std::mutex> lock(g_detected_classes_mutex);
            m_sorted_classes.clear();
            for (const auto& cls : g_detected_classes) {
                m_sorted_classes.push_back(cls);
            }
        }

        // Only show default placeholder classes if no real classes have been detected yet
        if (m_sorted_classes.empty()) {
            m_sorted_classes = { "ANIMAL", "DEVICE", "PERSON", "VEHICLE" };
        }

        std::sort(m_sorted_classes.begin(), m_sorted_classes.end());

        for (const auto& cls : m_sorted_classes) {
            float r, g, b;
            if (!m_map.get_color(cls, r, g, b)) {
                float h = std::fmod(std::hash<std::string> { }(cls) * 137.5f, 360.0f);
                constexpr float palette_s = 0.85f * 255.0f;
                constexpr float palette_v = 1.0f * 255.0f;
                auto temp_col = core::color<>::from_hsv(h, palette_s, palette_v);
                m_map.set_color(cls, temp_col.red, temp_col.green, temp_col.blue);
            }
        }
    }

    virtual core::vec2 measure_impl(float avail_w, float /*avail_h*/) override {
        update_sorted_classes();

        // Shrink the hardcoded height from 130.0f down to 104.0f
        return { (std::max)(avail_w, 300.0f), 84.0f };
    }

    virtual void paint_impl(
        drawbot::canvas& canvas, drawbot::supplier& supplier, const theme& t) override {
        update_sorted_classes();

        canvas.fill_rect(bounds.x, bounds.y, bounds.w, bounds.h, t.bg);

        auto font_hdr = supplier.create_font(10.0f);
        auto brush_hdr = supplier.create_brush(t.text_dim);

        // 2. Maximize visible width
        float total_w = m_sorted_classes.size() * (CARD_W + 6.0f);
        float visible_w = bounds.w; // Changed from bounds.w - 16.0f
        float max_scroll = (std::max)(0.0f, total_w - visible_w);
        bool has_scrollbar = max_scroll > 0.0f;
        // 3. Cards flush left
        float start_x = bounds.x; // Removed + 8.0f
        float cards_y = bounds.y + 4.0f;
        auto font_title = supplier.create_font(9.0f);

        for (int i = 0; i < (int)m_sorted_classes.size(); ++i) {
            float card_x = start_x + i * (CARD_W + 6.0f) - m_scroll_x;

            // Cull off-screen cards
            if (card_x + CARD_W < bounds.x || card_x > bounds.x + bounds.w) {
                continue;
            }

            std::string name = m_sorted_classes[i];
            float r = 0.8f, g = 0.8f, b = 0.8f;
            m_map.get_color(name, r, g, b);
            bool disabled = m_map.get_disabled(name);
            core::color<> class_color = { 1.0f, r, g, b };

            bool card_hovered
                = (m_hovered_index == i && m_hover_type == hover_type::card_bg);
            bool orb_hovered
                = (m_hovered_index == i && m_hover_type == hover_type::color_orb);

            // --- COLOR UPDATE: True Dark Adobe Gray ---
            // --- COLOR UPDATE: 29,29,29 Base & Class Color Highlight ---

            // 1. The Base Background
            // 29 / 255 = ~0.1137f. Alpha is 1.0f (fully opaque).
            core::color<> card_bg_color = disabled
                ? t.bg // Disabled cards still sink into the deeper panel background
                : core::color<> { 1.0f, 29.0f / 255.0f, 29.0f / 255.0f, 29.0f / 255.0f };

            // 2. The Border & Highlight
            core::color<> card_border_color = disabled
                ? core::color<> { 1.0f, 0.15f, 0.15f, 0.15f }
                : core::color<> { 1.0f, 0.12f, 0.12f, 0.12f };

            if (card_hovered && !disabled) {
                card_border_color = class_color;
                // Tint the 29,29,29 background with 15% of the class color for a subtle
                // glow
                card_bg_color = core::color<> { 1.0f, (29.0f / 255.0f) + (r * 0.15f),
                    (29.0f / 255.0f) + (g * 0.15f), (29.0f / 255.0f) + (b * 0.15f) };
            }

            auto card_path = create_rounded_rect_path(
                supplier, card_x, cards_y, CARD_W, CARD_H, 4.0f);
            canvas.fill_path(card_path, supplier.create_brush(card_bg_color));
            canvas.stroke_path(card_path,
                supplier.create_pen(card_border_color, card_hovered ? 1.5f : 1.0f));
            // Orb Styling
            float orb_cx = card_x + (CARD_W / 2.0f);
            float orb_cy = cards_y + 20.0f;
            core::color<> orb_color = disabled
                ? core::color<> { 1.0f, r * 0.3f, g * 0.3f, b * 0.3f }
                : class_color;

            auto orb_path = create_circle_path(supplier, orb_cx, orb_cy, ORB_RADIUS);
            canvas.fill_path(orb_path, supplier.create_brush(orb_color));
            canvas.stroke_path(orb_path,
                supplier.create_pen(
                    orb_hovered ? t.accent : core::color<> { 1.0f, 0.1f, 0.1f, 0.1f },
                    orb_hovered ? 2.0f : 1.0f));

            // Edit Pencil Icon Button (Top-Right of card)
            bool edit_hovered = (m_hovered_index == i && m_hover_type == hover_type::edit_icon);
            bool has_alias = !m_map.get_alias(name).empty();
            core::color<> edit_btn_col = edit_hovered ? t.accent
                                       : (has_alias ? t.accent : core::color<>{ 1.0f, 0.45f, 0.45f, 0.45f });

            float edit_x = card_x + CARD_W - 13.0f;
            float edit_y = cards_y + 4.0f;
            auto edit_bg_path = create_rounded_rect_path(supplier, edit_x, edit_y, 10.0f, 10.0f, 2.0f);
            canvas.stroke_path(edit_bg_path, supplier.create_pen(edit_btn_col, edit_hovered ? 1.5f : 1.0f));
            if (has_alias) {
                canvas.fill_path(edit_bg_path, supplier.create_brush(core::color<>{ 0.25f, t.accent.red, t.accent.green, t.accent.blue }));
            }

            // --- TEXT CENTERING ---
            core::color<> text_color = disabled ? core::color<> { 1.0f, 0.5f, 0.5f, 0.5f }
                                              : core::color<> { 1.0f, 0.9f, 0.9f, 0.9f };

            std::string display_name = m_map.get_display_name(name);
            if (!m_map.get_alias(name).empty() && !disabled) {
                text_color = t.accent;
            }

            if (display_name.length() > 8)
                display_name = display_name.substr(0, 7) + ".";

            float estimated_text_width = display_name.length() * 5.2f;
            float text_x = card_x + (CARD_W / 2.0f) - (estimated_text_width / 2.0f);

            canvas.draw_text(display_name, font_title, supplier.create_brush(text_color),
                core::vec2(text_x, cards_y + 46.0f));
        }

        // Horizontal Scrollbar
        if (has_scrollbar) {
            float sb_x = bounds.x;
            float sb_y = bounds.y + bounds.h - 14.0f;
            float track_w = visible_w;

            canvas.fill_rect(sb_x, sb_y, track_w, 4.0f,
                core::color<>(1.0f, 0.15f, 0.15f, 0.15f)); // Darker track

            float ratio = visible_w / total_w;
            float thumb_w = (std::max)(24.0f, track_w * ratio);
            float travel_x = track_w - thumb_w;
            float thumb_x = sb_x + (m_scroll_x / max_scroll) * travel_x;

            core::color<> thumb_col
                = (m_hover_type == hover_type::scrollbar_thumb || m_is_dragging_scrollbar)
                ? t.accent
                : core::color<>(1.0f, 0.35f, 0.35f, 0.35f); // Brighter thumb

            auto thumb_path = create_rounded_rect_path(
                supplier, thumb_x, sb_y - 1.0f, thumb_w, 6.0f, 3.0f);
            canvas.fill_path(thumb_path, supplier.create_brush(thumb_col));
        }
    }

    virtual bool on_click_impl(float lx, float ly, uint32_t mods) override {
        float total_w = m_sorted_classes.size() * (CARD_W + 6.0f);

        // 1. FIX: Remove the - 16.0f here
        float visible_w = bounds.w;
        float max_scroll = (std::max)(0.0f, total_w - visible_w);
        bool has_scrollbar = max_scroll > 0.0f;

        // Check Scrollbar first
        if (has_scrollbar && ly >= bounds.y + bounds.h - 20.0f) {

            // 2. FIX: Remove the + 8.0f here
            float sb_x = bounds.x;

            float track_w = visible_w;
            float ratio = visible_w / total_w;
            float thumb_w = (std::max)(24.0f, track_w * ratio);
            float travel_x = track_w - thumb_w;
            float thumb_x = sb_x + (m_scroll_x / max_scroll) * travel_x;

            if (lx >= thumb_x && lx <= thumb_x + thumb_w) {
                m_is_dragging_scrollbar = true;
                m_drag_start_x = lx;
                m_drag_start_scroll_x = m_scroll_x;
            } else {
                float relative_click_x = lx - sb_x - thumb_w * 0.5f;
                float pct = relative_click_x / travel_x;
                m_scroll_x = std::clamp(pct * max_scroll, 0.0f, max_scroll);
                m_is_dragging_scrollbar = true;
                m_drag_start_x = lx;
                m_drag_start_scroll_x = m_scroll_x;
            }
            return true;
        }

        // Check Cards
        // 3. FIX: Remove the + 8.0f here
        float start_x = bounds.x;
        float cards_y = bounds.y + 4.0f;

        if (ly >= cards_y && ly <= cards_y + CARD_H) {
            for (int i = 0; i < (int)m_sorted_classes.size(); ++i) {
                float card_x = start_x + i * (CARD_W + 6.0f) - m_scroll_x;

                if (lx >= card_x && lx <= card_x + CARD_W) {
                    std::string name = m_sorted_classes[i];

                    // --- UPDATE: Align hit-box Y coordinate ---
                    float orb_cx = card_x + (CARD_W / 2.0f);
                    float orb_cy = cards_y + 20.0f; // <--- Changed from 24.0f

                    float dist_sq
                        = (lx - orb_cx) * (lx - orb_cx) + (ly - orb_cy) * (ly - orb_cy);

                    // 1. Edit Pencil Icon Hit Area (top-right of card) OR Shift/Ctrl Click (mods 0x01 or 0x02)
                    bool is_edit_click = (lx >= card_x + 30.0f && lx <= card_x + CARD_W && ly >= cards_y && ly <= cards_y + 18.0f);
                    if (is_edit_click || (mods & 0x03) != 0) {
                        m_rename_pick_class = name;
                        m_needs_rename = true;
                        m_needs_commit = true;
                        return true;
                    }

                    // 2. Color Orb Hit Area
                    if (dist_sq <= ORB_RADIUS * ORB_RADIUS) {
                        m_color_pick_class = name;
                        m_needs_color_pick = true;
                        m_needs_commit = true;
                        return true;
                    }

                    // 3. Card Body -> toggle enable/disable
                    bool cur = m_map.get_disabled(name);
                    m_map.set_disabled(name, !cur);
                    m_needs_commit = true;
                    return true;
                }
            }

            // Clicked empty space in carousel row -> drag to scroll
            m_is_dragging_list = true;
            m_drag_start_x = lx;
            m_drag_start_scroll_x = m_scroll_x;
            return true;
        }

        return false;
    }

    virtual bool on_drag_impl(float lx, float /*ly*/, uint32_t /*mods*/) override {
        float total_w = m_sorted_classes.size() * (CARD_W + 6.0f);
        float visible_w = bounds.w;
        float max_scroll = (std::max)(0.0f, total_w - visible_w);

        if (m_is_dragging_scrollbar && max_scroll > 0.0f) {
            float track_w = visible_w;
            float ratio = visible_w / total_w;
            float thumb_w = (std::max)(24.0f, track_w * ratio);
            float travel_x = track_w - thumb_w;

            if (travel_x > 0.0f) {
                float dx = lx - m_drag_start_x;
                float delta_scroll = (dx / travel_x) * max_scroll;
                m_scroll_x
                    = std::clamp(m_drag_start_scroll_x + delta_scroll, 0.0f, max_scroll);
            }
            return true;
        } else if (m_is_dragging_list && max_scroll > 0.0f) {
            float dx = lx - m_drag_start_x;
            // Reverse direction for natural "grab and pan" feel
            m_scroll_x = std::clamp(m_drag_start_scroll_x - dx, 0.0f, max_scroll);
            return true;
        }
        return false;
    }

    virtual void on_release_impl() override {
        m_is_dragging_scrollbar = false;
        m_is_dragging_list = false;
    }

    virtual bool on_hover_move_impl(float lx, float ly) override {
        hover_type old_type = m_hover_type;
        int old_index = m_hovered_index;
        m_hover_type = hover_type::none;
        m_hovered_index = -1;

        float total_w = m_sorted_classes.size() * (CARD_W + 6.0f);
        float visible_w = bounds.w;
        float max_scroll = (std::max)(0.0f, total_w - visible_w);
        bool has_scrollbar = max_scroll > 0.0f;

        if (has_scrollbar && ly >= bounds.y + bounds.h - 20.0f) {
            float sb_x = bounds.x;
            float track_w = visible_w;
            float ratio = visible_w / total_w;
            float thumb_w = (std::max)(24.0f, track_w * ratio);
            float travel_x = track_w - thumb_w;
            float thumb_x = sb_x + (m_scroll_x / max_scroll) * travel_x;

            if (lx >= thumb_x && lx <= thumb_x + thumb_w) {
                m_hover_type = hover_type::scrollbar_thumb;
            } else {
                m_hover_type = hover_type::scrollbar_track;
            }
            return m_hover_type != old_type || m_hovered_index != old_index;
        }

        float start_x = bounds.x;
        float cards_y = bounds.y + 4.0f;

        if (ly >= cards_y && ly <= cards_y + CARD_H) {
            for (int i = 0; i < (int)m_sorted_classes.size(); ++i) {
                float card_x = start_x + i * (CARD_W + 6.0f) - m_scroll_x;
                if (lx >= card_x && lx <= card_x + CARD_W) {
                    m_hovered_index = i;

                    // --- UPDATE: Align hover-box Y coordinate ---
                    float orb_cx = card_x + (CARD_W / 2.0f);
                    float orb_cy = cards_y + 20.0f; // <--- Changed from 24.0f

                    float dist_sq
                        = (lx - orb_cx) * (lx - orb_cx) + (ly - orb_cy) * (ly - orb_cy);

                    bool is_edit_hover = (lx >= card_x + 30.0f && lx <= card_x + CARD_W && ly >= cards_y && ly <= cards_y + 18.0f);
                    if (is_edit_hover) {
                        m_hover_type = hover_type::edit_icon;
                    } else if (dist_sq <= ORB_RADIUS * ORB_RADIUS) {
                        m_hover_type = hover_type::color_orb;
                    } else {
                        m_hover_type = hover_type::card_bg;
                    }
                    break;
                }
            }
        }
        return m_hover_type != old_type || m_hovered_index != old_index;
    }

    virtual void on_hover_exit_impl() override {
        m_hover_type = hover_type::none;
        m_hovered_index = -1;
    }

    virtual void sync_state_impl(const interaction_context& ctx) override {
        auto lock = ctx.arb_data<class_color_map>(m_param_index);
        if (lock)
            m_map = *lock;
    }

    virtual void commit_state_impl(const interaction_context& ctx) override {
        bool changed = false;
        if (m_needs_color_pick) {
            m_needs_color_pick = false;
            float r = 1.0f, g = 1.0f, b = 1.0f;
            m_map.get_color(m_color_pick_class, r, g, b);
            if (auto result = ctx.show_color_picker(
                    "Customize Class Color", core::color<>(1.0, r, g, b))) {
                m_map.set_color(
                    m_color_pick_class, result->red, result->green, result->blue);
                changed = true;
            }
        }
        if (m_needs_rename) {
            m_needs_rename = false;
            std::string cur_alias = m_map.get_alias(m_rename_pick_class);
            if (auto result = prompt_class_alias_dialog(m_rename_pick_class, cur_alias)) {
                m_map.set_alias(m_rename_pick_class, *result);
                changed = true;
            }
        }
        if (m_needs_commit) {
            m_needs_commit = false;
            changed = true;
        }
        if (changed) {
            auto lock = ctx.arb_data<class_color_map>(m_param_index);
            if (lock) {
                *lock = m_map;
                ctx.mark_param_changed(m_param_index);
            }
        }
    }

    virtual int32_t cursor_type_impl() const override {
        return 0;
    }

    virtual void paint_title_impl(drawbot::canvas& canvas, drawbot::supplier& supplier,
        const theme& t, float x, float y, float w, float h) override {
        if (supplier.supports_text()) {
            int active_count = 0;
            for (const auto& cls : m_sorted_classes) {
                if (!m_map.get_disabled(cls)) {
                    active_count++;
                }
            }
            char buf[64] = {0};
            snprintf(buf, sizeof(buf), "%d active / %zu total", active_count, m_sorted_classes.size());

            auto font = supplier.create_font(t.font_size * 0.9f);
            auto brush = supplier.create_brush(t.text_dim);
            float ty = y + h * 0.5f + t.font_size * 0.28f;
            canvas.draw_text(buf, font, brush,
                core::vec2(x + w - 4.0f, ty), kDRAWBOT_TextAlignment_Right);
        }
    }
};

} // namespace aetk::effect::ui

namespace aetk::effect {

template <> struct arb_traits<ui::class_color_map> {
    using T = ui::class_color_map;

    static void init(T* ptr) {
        new (ptr) T();
    }

    // --- ADD THIS COMPARE FUNCTION ---
    // --- CORRECTED COMPARE FUNCTION ---
    static bool compare(const T* a, const T* b) {
        return *a == *b; // True means equal in aetk!
    }

    static void dispose(T* ptr) {
        ptr->~T();
    }
    static void copy(T* dst, const T* src) {
        new (dst) T(*src);
    }
    static size_t flat_size(const T* ptr) {
        return sizeof(T);
    }

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
        T::interpolate_into(dst, left, right, t);
    }

    static void print(const T* ptr, char* str, size_t max_len) {
        if (max_len > 0) {
            std::string out = "YOLOClasses:";
            for (int i = 0; i < ptr->num_entries; ++i) {
                char entry_buf[128];
                aetk::core::c_snprintf(entry_buf, sizeof(entry_buf), " %s=(%.4f,%.4f,%.4f,%d)%s", 
                         ptr->entries[i].name, ptr->entries[i].r, ptr->entries[i].g, ptr->entries[i].b, 
                         ptr->entries[i].disabled ? 1 : 0,
                         (i + 1 < ptr->num_entries ? ";" : ""));
                out += entry_buf;
            }
            std::strncpy(str, out.c_str(), max_len);
            str[max_len - 1] = '\0';
        }
    }

    static size_t print_size(const T* ptr) {
        return 32 + ptr->num_entries * 128;
    }

    static bool scan(T* ptr, const char* str) {
        if (std::strncmp(str, "YOLOClasses:", 12) != 0) return false;
        
        T map;
        const char* p = str + 12;
        while (map.num_entries < T::MAX_ENTRIES) {
            while (*p == ' ') p++;
            if (*p == '\0') break;
            
            const char* eq = std::strchr(p, '=');
            if (!eq) break;
            size_t name_len = eq - p;
            if (name_len >= sizeof(ui::class_color_entry::name)) {
                name_len = sizeof(ui::class_color_entry::name) - 1;
            }
            ui::class_color_entry entry;
            std::memcpy(entry.name, p, name_len);
            entry.name[name_len] = '\0';
            
            p = eq + 1;
            float r = 0.0f, g = 0.0f, b = 0.0f;
            int disabled_int = 0;
            if (aetk::core::c_sscanf(p, "(%f,%f,%f,%d)", &r, &g, &b, &disabled_int) == 4) {
                entry.r = r;
                entry.g = g;
                entry.b = b;
                entry.disabled = (disabled_int != 0);
                map.entries[map.num_entries++] = entry;
            }
            
            const char* semi = std::strchr(p, ';');
            if (!semi) break;
            p = semi + 1;
        }
        
        if (map.num_entries > 0) {
            *ptr = map;
            return true;
        }
        return false;
    }
};

} // namespace aetk::effect
