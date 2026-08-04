#pragma once

#include <aetk/core/suite.hpp>
#include <aetk/core/types.hpp>
#include <adobesdk/DrawbotSuite.h>
#include <string>
#include <vector>
#include <cstring>

namespace aetk::ui::drawbot {

/**
 * @brief Specifies the memory layout of source image buffers.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Type alias matching kDRAWBOT_PixelLayout enums.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
enum class pixel_layout {
    rgb_24 = kDRAWBOT_PixelLayout_24RGB,
    bgr_24 = kDRAWBOT_PixelLayout_24BGR,
    rgb_32 = kDRAWBOT_PixelLayout_32RGB,
    bgr_32 = kDRAWBOT_PixelLayout_32BGR,
    argb_32_straight = kDRAWBOT_PixelLayout_32ARGB_Straight,
    argb_32_premul = kDRAWBOT_PixelLayout_32ARGB_Premul,
    bgra_32_straight = kDRAWBOT_PixelLayout_32BGRA_Straight,
    bgra_32_premul = kDRAWBOT_PixelLayout_32BGRA_Premul
};

/**
 * @brief Specifies the render quality for antialiasing and image interpolation.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Direct type mapping to Drawbot interpolation policy flags.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
enum class render_quality {
    none = 0,
    medium = 1,
    high = 2
};

/**
 * @brief A thread-safe, auto-retaining RAII resource wrapper for Drawbot drawing objects.
 * 
 * @details Wraps `DRAWBOT_ObjectRef` objects such as `DRAWBOT_BrushRef`, `DRAWBOT_PenRef`, `DRAWBOT_PathRef`, `DRAWBOT_FontRef`, and `DRAWBOT_ImageRef`.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, managing custom-drawing overlays via Drawbot requires manual reference counting using procedural calls (`RetainObject` and `ReleaseObject`) on the supplier suite pointer. Any missing release causes persistent memory leaks that remain resident in After Effects' drawing cache. `aetk::ui::drawbot::resource` uses move-safe RAII wrappers that retain objects on copy/construction and call `ReleaseObject` on scope exit.
 *
 * @warning <b>Memory & Lifecycles:</b> Exclusive ownership or safe cooperative reference counting. The supplier suite pointer must remain valid for the lifespan of the resource.
 *
 * @tparam REF_T The raw Drawbot reference handle type.
 */
template <typename REF_T>
class resource {
public:
    /**
     * @brief Default constructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Initializes empty resource.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    resource() = default;

    /**
     * @brief Direct handle constructor.
     *
     * @details Binds the raw handle, optionally retaining it immediately.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Type-safe promotion to RAII.
     *
     * @warning <b>Memory & Lifecycles:</b> Takes ownership of the raw handle. If `retain` is true, increments reference count on the Drawbot supplier suite.
     *
     * @param supplier Pointer to the basic Drawbot supplier suite.
     * @param ref Raw resource reference handle.
     * @param retain True to increment reference count.
     */
    resource(DRAWBOT_SupplierSuite1* supplier, REF_T ref, bool retain = false)
        : m_supplier(supplier), m_ref(ref) {
        if (m_ref && retain) {
            m_supplier->RetainObject(reinterpret_cast<DRAWBOT_ObjectRef>(m_ref));
        }
    }

    /**
     * @brief Releases the reference automatically on destruction.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe RAII cleanup.
     *
     * @warning <b>Memory & Lifecycles:</b> Calls `ReleaseObject` on the supplier if valid. Never throws.
     */
    ~resource() { release(); }

    /**
     * @brief Copy constructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Handles reference counts on copy.
     *
     * @warning <b>Memory & Lifecycles:</b> Calls `RetainObject` on the supplier.
     *
     * @param other Source resource to copy.
     */
    resource(const resource& other) : m_supplier(other.m_supplier), m_ref(other.m_ref) {
        if (m_ref && m_supplier) {
            m_supplier->RetainObject(reinterpret_cast<DRAWBOT_ObjectRef>(m_ref));
        }
    }

    /**
     * @brief Copy assignment operator.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe copying.
     *
     * @warning <b>Memory & Lifecycles:</b> Releases the existing object before retaining the new reference.
     *
     * @param other Source resource.
     * @return Reference to this resource.
     */
    resource& operator=(const resource& other) {
        if (this != &other) {
            release();
            m_supplier = other.m_supplier;
            m_ref = other.m_ref;
            if (m_ref && m_supplier) {
                m_supplier->RetainObject(reinterpret_cast<DRAWBOT_ObjectRef>(m_ref));
            }
        }
        return *this;
    }

    /**
     * @brief Move constructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Move semantics.
     *
     * @warning <b>Memory & Lifecycles:</b> Transfers ownership without changing reference count, invalidating other.
     *
     * @param other Source resource.
     */
    resource(resource&& other) noexcept : m_supplier(other.m_supplier), m_ref(other.m_ref) {
        other.m_ref = nullptr;
    }

    /**
     * @brief Move assignment.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Move semantics.
     *
     * @warning <b>Memory & Lifecycles:</b> Releases the existing object and transfers ownership.
     *
     * @param other Source resource.
     * @return Reference to this resource.
     */
    resource& operator=(resource&& other) noexcept {
        if (this != &other) {
            release();
            m_supplier = other.m_supplier;
            m_ref = other.m_ref;
            other.m_ref = nullptr;
        }
        return *this;
    }

    /** @brief Get raw Drawbot handle. */
    REF_T get() const { return m_ref; }
    
    /** @brief Implicit conversion to raw handle. */
    operator REF_T() const { return m_ref; }
    
    /** @brief Valid state check. */
    bool valid() const { return m_ref != nullptr; }

private:
    void release() {
        if (m_ref && m_supplier) {
            m_supplier->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(m_ref));
            m_ref = nullptr;
        }
    }

    DRAWBOT_SupplierSuite1* m_supplier = nullptr;
    REF_T m_ref = nullptr;
};

using path = resource<DRAWBOT_PathRef>;
using brush = resource<DRAWBOT_BrushRef>;
using pen = resource<DRAWBOT_PenRef>;
using font = resource<DRAWBOT_FontRef>;
using image = resource<DRAWBOT_ImageRef>;

/**
 * @brief High-level builder to draw complex vector path outlines using DRAWBOT_PathSuite.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Procedural path additions are replaced with a modern fluent builder pattern.
 *
 * @warning <b>Memory & Lifecycles:</b> The path builder constructs a persistent `aetk::ui::drawbot::path` resource.
 */
class path_builder {
public:
    /**
     * @brief Path builder constructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Path construction start.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param supplier Supplier suite.
     * @param path_suite Path suite.
     * @param supplier_ref Supplier reference handle.
     */
    path_builder(DRAWBOT_SupplierSuite1* supplier, DRAWBOT_PathSuite1* path_suite, DRAWBOT_SupplierRef supplier_ref)
        : m_path_suite(path_suite) {
        DRAWBOT_PathRef p = nullptr;
        supplier->NewPath(supplier_ref, &p);
        m_path = path(supplier, p);
    }

    path_builder& move_to(float x, float y) {
        m_path_suite->MoveTo(m_path, x, y);
        return *this;
    }

    path_builder& line_to(float x, float y) {
        m_path_suite->LineTo(m_path, x, y);
        return *this;
    }

    path_builder& bezier_to(core::vec2 p1, core::vec2 p2, core::vec2 p3) {
        DRAWBOT_PointF32 pt1{ (float)p1.x, (float)p1.y };
        DRAWBOT_PointF32 pt2{ (float)p2.x, (float)p2.y };
        DRAWBOT_PointF32 pt3{ (float)p3.x, (float)p3.y };
        m_path_suite->BezierTo(m_path, &pt1, &pt2, &pt3);
        return *this;
    }

    path_builder& add_rect(float x, float y, float w, float h) {
        DRAWBOT_RectF32 rect{ x, y, w, h };
        m_path_suite->AddRect(m_path, &rect);
        return *this;
    }

    path_builder& add_arc(core::vec2 center, float radius, float start_angle, float sweep) {
        DRAWBOT_PointF32 c{ (float)center.x, (float)center.y };
        m_path_suite->AddArc(m_path, &c, radius, start_angle, sweep);
        return *this;
    }

    path_builder& close() {
        m_path_suite->Close(m_path);
        return *this;
    }

    path build() { return m_path; }

private:
    DRAWBOT_PathSuite1* m_path_suite;
    path m_path;
};

/**
 * @brief High-level wrapper for creating Drawbot drawing brushes, pens, and fonts.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Clean C++ resource construction helper.
 *
 * @warning <b>Memory & Lifecycles:</b> Resources returned take ownership of the underlying handles. Supplier suite pointer must remain valid.
 */
class supplier {
public:
    /**
     * @brief supplier constructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Binds Drawbot factory reference.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param supplier_suite Supplier suite.
     * @param ref Supplier reference.
     */
    supplier(DRAWBOT_SupplierSuite1* supplier_suite, DRAWBOT_SupplierRef ref)
        : m_suite(supplier_suite), m_ref(ref) {}

    /** @brief Create a solid brush. */
    brush create_brush(const core::color<core::pixel_range::tkfloat>& c) {
        DRAWBOT_ColorRGBA rgba{ (float)c.red, (float)c.green, (float)c.blue, (float)c.alpha };
        DRAWBOT_BrushRef b = nullptr;
        m_suite->NewBrush(m_ref, &rgba, &b);
        return {m_suite, b};
    }

    /** @brief Create a pen with thickness. */
    pen create_pen(const core::color<core::pixel_range::tkfloat>& c, float width) {
        DRAWBOT_ColorRGBA rgba{ (float)c.red, (float)c.green, (float)c.blue, (float)c.alpha };
        DRAWBOT_PenRef p = nullptr;
        m_suite->NewPen(m_ref, &rgba, width, &p);
        return {m_suite, p};
    }

    /** @brief Create a default font of specific height size. */
    font create_font(float size) {
        DRAWBOT_FontRef f = nullptr;
        m_suite->NewDefaultFont(m_ref, size, &f);
        return {m_suite, f};
    }

    /** @brief Create a pixel image from buffer array. */
    image create_image(int width, int height, int row_bytes, pixel_layout layout, const void* data) {
        DRAWBOT_ImageRef img = nullptr;
        m_suite->NewImageFromBuffer(m_ref, width, height, row_bytes, static_cast<DRAWBOT_PixelLayout>(layout), data, &img);
        return {m_suite, img};
    }

    /** @brief Sets scale factor for high-DPI displays. */
    void set_image_scale(const image& img, float scale) {
        if (auto image_suite = core::suite<DRAWBOT_ImageSuite1>::get()) {
            image_suite->SetScaleFactor(img.get(), scale);
        }
    }

    /** @brief Sets dash spacing patterns for pens. */
    void set_dash_pattern(const pen& p, const std::vector<float>& dashes) {
        if (auto pen_suite = core::suite<DRAWBOT_PenSuite1>::get()) {
            pen_suite->SetDashPattern(p.get(), dashes.data(), static_cast<int>(dashes.size()));
        }
    }

    /** @brief Verify if canvas supports text drawings. */
    bool supports_text() const {
        DRAWBOT_Boolean out = false;
        m_suite->SupportsText(m_ref, &out);
        return out != 0;
    }

    /** @brief Retrieve host font scale sizing. */
    float get_default_font_size() const {
        float size = 0.0f;
        m_suite->GetDefaultFontSize(m_ref, &size);
        return size;
    }

    /** @brief Initiates custom vector builder. */
    path_builder create_path() {
        auto path_suite = core::suite<DRAWBOT_PathSuite1>::get();
        return {m_suite, path_suite, m_ref};
    }

private:
    DRAWBOT_SupplierSuite1* m_suite;
    DRAWBOT_SupplierRef m_ref;
};

/**
 * @brief High-level surface drawing interface encapsulating Drawbot drawing suites.
 * 
 * @details Unifies DRAWBOT_DrawRef, DRAWBOT_SupplierRef, and DRAWBOT_SurfaceRef in a single context wrapper.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, custom overlay drawings (Custom UI) requires calling procedural functions from multiple Drawbot suites like `DRAWBOT_SurfaceSuite2`, `DRAWBOT_DrawbotSuite1`, etc., while managing pointer reference casting. `aetk::ui::drawbot::canvas` implements an integrated class wrapper that extracts suppliers, surfaces, handles, and paths automatically, providing native overlay support via `PF_EffectCustomUIOverlayThemeSuite1`.
 *
 * @warning <b>Memory & Lifecycles:</b> High-performance overlay drawing on host-provided windows. Ensure drawing calls are strictly confined to custom events.
 */
class canvas {
public:
    /**
     * @brief canvas constructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Extracts nested drawing references.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param draw_ref Raw host drawing reference.
     */
    canvas(DRAWBOT_DrawRef draw_ref) : m_draw_ref(draw_ref) {
        if (!m_draw_ref) return;

        auto draw_suite = core::suite<DRAWBOT_DrawbotSuite1>::get();
        if (draw_suite) {
            draw_suite->GetSupplier(m_draw_ref, &m_supplier_ref);
            draw_suite->GetSurface(m_draw_ref, &m_surface_ref);
        }
        
        m_supplier_suite = core::suite<DRAWBOT_SupplierSuite1>::get();
        m_surface_suite = core::suite<DRAWBOT_SurfaceSuite2>::get();
    }

    /** @brief Validation status check. */
    bool valid() const { return m_draw_ref != nullptr && m_supplier_ref != nullptr && m_surface_ref != nullptr; }

    /** @brief Acquires resource factory supplier. */
    supplier get_supplier() { return {m_supplier_suite, m_supplier_ref}; }

    /** @brief Fills the inside of a path using specific brush. */
    void fill_path(const path& p, const brush& b, DRAWBOT_FillType fill_type = kDRAWBOT_FillType_EvenOdd) {
        m_surface_suite->FillPath(m_surface_ref, b, p, fill_type);
    }

    /** @brief Strokes path outlines using specific pen. */
    void stroke_path(const path& p, const pen& pen_ref) {
        m_surface_suite->StrokePath(m_surface_ref, pen_ref, p);
    }

    /** @brief High-speed paint solid color rectangle. */
    void fill_rect(float x, float y, float w, float h, const core::color<core::pixel_range::tkfloat>& c) {
        DRAWBOT_ColorRGBA rgba{ (float)c.red, (float)c.green, (float)c.blue, (float)c.alpha };
        DRAWBOT_RectF32 rect{ x, y, w, h };
        m_surface_suite->PaintRect(m_surface_ref, &rgba, &rect);
    }

    // ── Native Overlay Theme Drawing ────────────────────────────────────────

    /** 
     * @brief Strokes a path using the native AE custom UI overlay theme (colors & width automatically applied).
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP drawing call mapping to PF_EffectCustomUIOverlayThemeSuite1.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param p Path ref to stroke.
     * @param draw_shadow True to paint drop shadow underneath.
     */
    void stroke_path_native(const path& p, bool draw_shadow = true) {
                    auto overlay = core::suite<PF_EffectCustomUIOverlayThemeSuite1>::get();
            if (overlay && m_draw_ref) {
                overlay->PF_StrokePath(m_draw_ref, p.get(), draw_shadow);
            }
        
    }

    /** 
     * @brief Fills a path using the native AE custom UI overlay theme.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP drawing call mapping to PF_EffectCustomUIOverlayThemeSuite1.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param p Path ref to fill.
     * @param draw_shadow True to paint shadow.
     */
    void fill_path_native(const path& p, bool draw_shadow = true) {
                    auto overlay = core::suite<PF_EffectCustomUIOverlayThemeSuite1>::get();
            if (overlay && m_draw_ref) {
                overlay->PF_FillPath(m_draw_ref, p.get(), draw_shadow);
            }
        
    }

    /** 
     * @brief Draws a square vertex point using native AE overlay theme size and color.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean OOP drawing call mapping to PF_EffectCustomUIOverlayThemeSuite1.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param center Spot coordinate.
     * @param draw_shadow True to draw drop shadow.
     */
    void fill_vertex_native(core::vec2 center, bool draw_shadow = true) {
                    auto overlay = core::suite<PF_EffectCustomUIOverlayThemeSuite1>::get();
            if (overlay && m_draw_ref) {
                A_FloatPoint p = { (float)center.x, (float)center.y };
                overlay->PF_FillVertex(m_draw_ref, &p, draw_shadow);
            }
        
    }

    /** @brief Draws UTF16-translated text onto surface. */
    void draw_text(const std::string& text, const font& f, const brush& b, core::vec2 origin, DRAWBOT_TextAlignment alignment = kDRAWBOT_TextAlignment_Left) {
        // Convert std::string to UTF-16
        std::vector<DRAWBOT_UTF16Char> utf16;
        for (char c : text) utf16.push_back(static_cast<DRAWBOT_UTF16Char>(c));
        utf16.push_back(0);

        DRAWBOT_PointF32 p{ (float)origin.x, (float)origin.y };
        m_surface_suite->DrawString(m_surface_ref, b, f, utf16.data(), &p, alignment, kDRAWBOT_TextTruncation_None, 0.0f);
    }

    /** @brief Draws buffer image onto canvas. */
    void draw_image(DRAWBOT_ImageRef image, float x, float y, float alpha = 1.0f) {
        if (!image || !m_surface_suite) return;
        DRAWBOT_PointF32 origin{ x, y };
        m_surface_suite->DrawImage(m_surface_ref, image, &origin, alpha);
    }

    /** @brief Save drawing state. */
    void push_state() { m_surface_suite->PushStateStack(m_surface_ref); }
    
    /** @brief Restores saved drawing state. */
    void pop_state() { m_surface_suite->PopStateStack(m_surface_ref); }

    /** @brief Binds clip limits. */
    void clip(const core::rect& r) {
        DRAWBOT_Rect32 rect = { r.left, r.top, r.width(), r.height() };
        m_surface_suite->Clip(m_surface_ref, m_supplier_ref, &rect);
    }

    /** @brief Retrieve current clip boundaries. */
    core::rect get_clip_bounds() {
        DRAWBOT_Rect32 rect;
        m_surface_suite->GetClipBounds(m_surface_ref, &rect);
        return { rect.left, rect.top, rect.left + rect.width, rect.top + rect.height };
    }

    /** @brief Verify if rectangular bounds fall within clip limits. */
    bool is_within_clip(const core::rect& r) {
        DRAWBOT_Rect32 rect = { r.left, r.top, r.width(), r.height() };
        DRAWBOT_Boolean within = false;
        m_surface_suite->IsWithinClipBounds(m_surface_ref, &rect, &within);
        return within != 0;
    }

    /** @brief Applies 2D transform matrix to surface context. */
    void transform(const core::matrix2d& matrix) {
        DRAWBOT_MatrixF32 mat;
        std::memcpy(mat.mat, matrix.m, sizeof(mat.mat));
        m_surface_suite->Transform(m_surface_ref, &mat);
    }

    /** @brief Retrieves display scaling factors. */
    float get_screen_scale() {
        float scale = 1.0f;
        m_surface_suite->GetTransformToScreenScale(m_surface_ref, &scale);
        return scale;
    }

    /** @brief Sets image interpolation filters. */
    void set_interpolation(render_quality quality) {
        m_surface_suite->SetInterpolationPolicy(m_surface_ref, static_cast<DRAWBOT_InterpolationPolicy>(quality));
    }

    /** @brief Retrieves interpolation settings. */
    render_quality get_interpolation() {
        DRAWBOT_InterpolationPolicy out;
        m_surface_suite->GetInterpolationPolicy(m_surface_ref, &out);
        return static_cast<render_quality>(out);
    }

    /** @brief Sets anti-alias policies. */
    void set_anti_alias(render_quality quality) {
        m_surface_suite->SetAntiAliasPolicy(m_surface_ref, static_cast<DRAWBOT_AntiAliasPolicy>(quality));
    }

    /** @brief Retrieves anti-alias settings. */
    render_quality get_anti_alias() {
        DRAWBOT_AntiAliasPolicy out;
        m_surface_suite->GetAntiAliasPolicy(m_surface_ref, &out);
        return static_cast<render_quality>(out);
    }

    DRAWBOT_DrawRef raw_draw_ref() const { return m_draw_ref; }
    DRAWBOT_SupplierRef raw_supplier_ref() const { return m_supplier_ref; }
    DRAWBOT_SurfaceRef raw_surface_ref() const { return m_surface_ref; }
    DRAWBOT_SupplierSuite1* supplier_suite() const { return m_supplier_suite; }
    DRAWBOT_SurfaceSuite2* surface_suite() const { return m_surface_suite; }

private:
    DRAWBOT_DrawRef m_draw_ref;
    DRAWBOT_SupplierRef m_supplier_ref;
    DRAWBOT_SurfaceRef m_surface_ref;
    DRAWBOT_SupplierSuite1* m_supplier_suite;
    DRAWBOT_SurfaceSuite2* m_surface_suite;
};

} // namespace aetk::ui::drawbot
