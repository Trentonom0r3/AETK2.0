#pragma once

namespace aetk::effect::ui {

/**
 * @brief Dynamic layout dimension container for resizable container widgets.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Replaces raw custom interface floating-point dimensions with a structured type-safe UI resizable state container.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct resizable_data {
    /// Resized horizontal width in pixels.
    float width = -1.0f;
    
    /// Resized vertical height in pixels.
    float height = -1.0f;

    /**
     * @brief Operator comparison.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard structure comparison.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param other Target to compare.
     * @return True if equal.
     */
    bool operator==(const resizable_data& other) const {
        return width == other.width && height == other.height;
    }

    /**
     * @brief Operator inequality comparison.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard structure comparison.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param other Target to compare.
     * @return True if unequal.
     */
    bool operator!=(const resizable_data& other) const {
        return !(*this == other);
    }
};

} // namespace aetk::effect::ui
