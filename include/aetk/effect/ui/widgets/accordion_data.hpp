#pragma once

#include <string>

namespace aetk::effect::ui {

/**
 * @brief Expansion state container for accordion UI widgets.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Replaces raw custom interface boolean flags with a structured type-safe UI expansion state container.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct accordion_data {
    /// Expanded state flag status.
    bool expanded = true;

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
    bool operator==(const accordion_data& other) const {
        return expanded == other.expanded;
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
    bool operator!=(const accordion_data& other) const {
        return !(*this == other);
    }
};

} // namespace aetk::effect::ui
