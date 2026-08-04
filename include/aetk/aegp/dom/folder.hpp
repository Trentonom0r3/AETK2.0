#pragma once

#include <aetk/aegp/dom/item.hpp>
#include <vector>

namespace aetk::aegp {

/**
 * @brief Represents a folder item in the After Effects Project panel.
 * 
 * @note <b>AE SDK Paradigm Shift:</b> Offers a clean OOP model of project directories,
 * resolving parent-child item relationships through high-level query methods.
 */
class folder : public item {
public:
    using item::item; // Inherit constructors

    /**
     * @brief Construct from a base item.
     */
    explicit folder(const item& it) : item(it) {
        if (it && it.get_type() != AEGP_ItemType_FOLDER) {
            this->m_handle = nullptr;
        }
    }

    folder& operator=(const item& it) {
        if (it && it.get_type() == AEGP_ItemType_FOLDER) {
            this->m_handle = it.get();
        } else {
            this->m_handle = nullptr;
        }
        return *this;
    }

    /**
     * @brief Traverses the project and returns all immediate child items inside this folder.
     */
    std::vector<item> children() const {
        std::vector<item> list;
        item current = item::get_first();
        while (current) {
            if (current.get_parent_folder().get() == m_handle) {
                list.push_back(current);
            }
            current = current.get_next();
        }
        return list;
    }
};

} // namespace aetk::aegp
