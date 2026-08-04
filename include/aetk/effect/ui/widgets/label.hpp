#pragma once

#include <aetk/effect/ui/widget.hpp>
#include <aetk/ui/theme.hpp>

namespace aetk::effect::ui {

// ══════════════════════════════════════════════════════════════════════
//  Label — Static text display widget
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief A non-interactive static text label.
 * 
 * @details Renders a single line of text using the theme's default font.
 * Supports optional color and font size overrides.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, drawing a static text label requires calling raw drawing suites, calculating text height offsets, and implementing font registration hooks procedurally. `aetk::effect::ui::label` encapsulates this into a lightweight, declarative component that automatically integrates with the layout engine and active color themes, handling baseline descents cleanly.
 *
 * @warning <b>Memory & Lifecycles:</b> The label component is a non-interactive presenter containing non-owning views. Text string buffers are owned locally and managed safely during destruction passes.
 */
class label : public widget {
public:
    /// Text display content.
    std::string text;
    
    /// Bounded font size override.
    float font_size_override = 0;  // 0 = use theme default
    
    /// Bounded color override.
    core::color<> color_override = { -1, 0, 0, 0 }; // negative red = no override

    /**
     * @brief Constructs a text label.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Static text presentation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param txt Static text content.
     */
    explicit label(std::string txt) : text(std::move(txt)) {
        layout.min_height = 18.0f;
    }

    /**
     * @brief Constructs a text label with customized font size.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Sized static text presentation.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param txt Static text content.
     * @param font_sz Custom font size.
     */
    label(std::string txt, float font_sz)
        : text(std::move(txt)), font_size_override(font_sz) {
        layout.min_height = font_sz + 6.0f;
    }

protected:
    /**
     * @brief Measures label dimensions.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Font-size measured sizing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param avail_w Available width bounds.
     * @return Size vector.
     */
    core::vec2 measure_impl(float avail_w, float) override {
        float h = (font_size_override > 0 ? font_size_override : 11.0f) + 6.0f;
        return { avail_w, h };
    }

    /**
     * @brief Paints the label text baseline.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Drawbot text canvas drawing.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param canvas Drawbot canvas.
     * @param supplier Drawbot supplier.
     * @param t Theme color parameters.
     */
    void paint_impl(drawbot::canvas& canvas, drawbot::supplier& supplier, const theme& t) override {
        if (text.empty()) return;

        float fs = (font_size_override > 0) ? font_size_override : t.font_size;
        auto font = supplier.create_font(fs);

        core::color<> c = (color_override.red >= 0) ? color_override : t.text;
        auto brush = supplier.create_brush(c);

        // Draw text baseline-aligned (Drawbot draws up from the origin)
        float baseline = bounds.y + fs + 2.0f; // Approx baseline descent
        canvas.draw_text(text, font, brush, core::vec2(bounds.x + t.padding, baseline));
    }
};

} // namespace aetk::effect::ui
