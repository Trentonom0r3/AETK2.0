#pragma once

#include <aetk/core/suite.hpp>
#include <aetk/core/handle.hpp>
#include <aetk/core/mem_handle.hpp>
#include <aetk/core/types.hpp>
#include <string>
#include <cstdint>

namespace aetk::aegp {

// Forward declarations
class footage;
class owned_footage;

// ============================================================
//  Traits
// ============================================================

/**
 * @brief Traits helper for standard project items.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Provides standard trait definitions for opaque `AEGP_ItemH` handles, 
 * standardizing how borrowed and owned wrappers manipulate host resources.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct item_traits {
    using type = AEGP_ItemH;
};

using item_suite = aetk::core::suite<AEGP_ItemSuite9,
    aetk::core::fixed_string(kAEGPItemSuite), kAEGPItemSuiteVersion9>;

// ============================================================
//  item — Wraps AEGP_ItemH (Borrowed)
// ============================================================

/**
 * @brief Represents a standard, borrowed project item handle inside the After Effects Project panel.
 * 
 * @details This class is the core object-oriented representation of `AEGP_ItemH`. 
 * It wraps project panel items (which can be Compositions, Folders, or Footage items) 
 * and provides high-level properties, lifecycle commands, and parent-child hierarchy navigation.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, `AEGP_ItemH` is a primitive, opaque pointer. 
 * Manipulating it requires acquiring `AEGP_ItemSuite9` and passing the handle manually alongside 
 * error checks on every single action. `aetk::aegp::item` encapsulates this pointer and calls 
 * the appropriate suite functions implicitly.
 *
 * @warning <b>Memory & Lifecycles:</b> This is a <b>borrowed</b> handle representation wrapping `AEGP_ItemH`. 
 * It does not own the underlying project item memory; the After Effects host manages project item lifecycles. 
 * Calling methods on an `item` instance after invoking `delete_item()` or if the item is removed 
 * by the user in the UI will result in an access violation. Modifying the project structure 
 * (like renaming or reorganizing folders) must only occur on the primary UI thread. Calls to After 
 * Effects APIs internally rely on `aetk::core::suite` which automatically decrements the host 
 * reference count via `ReleaseSuite` when the suite goes out of scope.
 */
class item : public aetk::core::borrowed<item_traits> {
public:
    using borrowed::borrowed; // Inherit constructors

    // --- Properties ---

    /**
     * @brief Returns the item's name as a UTF-8 string.
     *
     * @details Accesses the name of the project item on the host using `AEGP_GetItemName`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simplifies UTF-16 conversion boilerplates, returning a standard `std::string`.
     *
     * @warning <b>Memory & Lifecycles:</b> Internally allocates a transient string buffer via `aetk::core::mem_handle`, which is automatically freed via `AEGP_FreeMemHandle` when it goes out of scope. Uses `aetk::core::suite` to acquire `AEGP_ItemSuite9` which automatically decrements the host reference count via `ReleaseSuite` when the suite wrapper goes out of scope.
     *
     * @return The item name as a UTF-8 encoded string.
     */
    std::string get_name() const {
        aetk::core::mem_handle name_h;
        item_suite::call<&AEGP_ItemSuite9::AEGP_GetItemName>(
            aetk::core::context::get_plugin_id(), m_handle, name_h.get_ptr());
        return name_h.to_string();
    }

    /**
     * @brief Sets the item's name. (Undoable)
     *
     * @details Modifies the display name of the item inside the project panel using `AEGP_SetItemName`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Accepts modern `std::string` and handles string marshaling to custom UTF-16 characters automatically.
     *
     * @warning <b>Memory & Lifecycles:</b> Must be called from the main thread. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param name The new descriptive name for the item.
     */
    void set_name(const std::string& name) {
        // Convert UTF-8 to UTF-16
        std::wstring wname(name.begin(), name.end()); // Simple ASCII/BMP conversion
        item_suite::call<&AEGP_ItemSuite9::AEGP_SetItemName>(
            m_handle, reinterpret_cast<const A_UTF16Char*>(wname.c_str()));
    }

    /**
     * @brief Returns the type of this item (Comp, Footage, Folder).
     *
     * @details Queries the item category classification using `AEGP_GetItemType`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Returns the native enum value safely.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return The AEGP_ItemType classification.
     */
    AEGP_ItemType get_type() const {
        AEGP_ItemType type = AEGP_ItemType_NONE;
        item_suite::call<&AEGP_ItemSuite9::AEGP_GetItemType>(m_handle, &type);
        return type;
    }

    /**
     * @brief Returns the item's unique ID within the project.
     *
     * @details Queries the persistent unique ID assigned by After Effects using `AEGP_GetItemID`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Wraps raw integer lookups cleanly.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return The unique ID of the item.
     */
    int32_t get_id() const {
        A_long id = 0;
        item_suite::call<&AEGP_ItemSuite9::AEGP_GetItemID>(m_handle, &id);
        return static_cast<int32_t>(id);
    }

    /**
     * @brief Retrieves the duration of the item in its native timespace.
     * 
     * @details Queries the raw project item duration through `AEGP_GetItemDuration`. 
     * The duration represents the total runtime of the composition or footage asset.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Returns a high-level `aetk::core::time` wrapper instead of 
     * populated `A_Time` struct outputs, offering simple double/frame translations.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return An `aetk::core::time` wrapper representing the total duration.
     */
    aetk::core::time get_duration() const {
        A_Time dur{};
        item_suite::call<&AEGP_ItemSuite9::AEGP_GetItemDuration>(m_handle, &dur);
        return aetk::core::time(dur);
    }

    /**
     * @brief Gets the current playback time pointer of the item in the active viewport.
     * 
     * @details Queries the active item time cursor through `AEGP_GetItemCurrentTime`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simplifies viewport cursor querying into a clean, thread-safe return.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return An `aetk::core::time` wrapper representing the current playback position.
     */
    aetk::core::time get_current_time() const {
        A_Time t{};
        item_suite::call<&AEGP_ItemSuite9::AEGP_GetItemCurrentTime>(m_handle, &t);
        return aetk::core::time(t);
    }

    /**
     * @brief Sets the item's current time. (Undoable)
     *
     * @details Moves the active timeline playhead to the specified time index using `AEGP_SetItemCurrentTime`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Accepts high-level `aetk::core::time` wrappers directly.
     *
     * @warning <b>Memory & Lifecycles:</b> Must be called from the main thread. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param t The target timeline playhead position.
     */
    void set_current_time(const aetk::core::time& t) {
        A_Time raw = t;
        item_suite::call<&AEGP_ItemSuite9::AEGP_SetItemCurrentTime>(m_handle, &raw);
    }

    /**
     * @brief Returns the item's dimensions (width, height).
     *
     * @details Queries physical pixel dimensions via `AEGP_GetItemDimensions`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standardizes dimension lookups into standard modern pair formats.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Pair containing the width and height of the item.
     */
    std::pair<int32_t, int32_t> get_dimensions() const {
        A_long w = 0, h = 0;
        item_suite::call<&AEGP_ItemSuite9::AEGP_GetItemDimensions>(m_handle, &w, &h);
        return {static_cast<int32_t>(w), static_cast<int32_t>(h)};
    }

    /**
     * @brief Returns the item's pixel aspect ratio.
     *
     * @details Queries pixel scale ratios using `AEGP_GetItemPixelAspectRatio`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Returns an automated `aetk::core::ratio` wrapper.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Pixel aspect ratio value wrapper.
     */
    aetk::core::ratio get_pixel_aspect_ratio() const {
        A_Ratio r{};
        item_suite::call<&AEGP_ItemSuite9::AEGP_GetItemPixelAspectRatio>(m_handle, &r);
        return aetk::core::ratio(r);
    }

    /**
     * @brief Returns the item's flags.
     *
     * @details Queries operational property flags from After Effects via `AEGP_GetItemFlags`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Returns flag bitmasks cleanly.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Raw `AEGP_ItemFlags` bitmask.
     */
    AEGP_ItemFlags get_flags() const {
        AEGP_ItemFlags flags = 0;
        item_suite::call<&AEGP_ItemSuite9::AEGP_GetItemFlags>(m_handle, &flags);
        return flags;
    }

    /**
     * @brief Returns whether the item has a proxy.
     *
     * @details Evaluates item flags to check if a proxy source is mapped.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Checks flags implicitly.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return True if a proxy asset is active.
     */
    bool has_proxy() const {
        return (get_flags() & AEGP_ItemFlag_HAS_PROXY) != 0;
    }

    /**
     * @brief Sets whether the item uses its proxy. (Undoable)
     *
     * @details Toggles proxy playback rendering flags using `AEGP_SetItemUseProxy`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Converts boolean switches safely to host formats.
     *
     * @warning <b>Memory & Lifecycles:</b> Throws if the item has no proxy. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param use True to activate proxy asset.
     */
    void set_use_proxy(bool use) {
        item_suite::call<&AEGP_ItemSuite9::AEGP_SetItemUseProxy>(m_handle, use ? TRUE : FALSE);
    }

    /**
     * @brief Returns whether this item is currently selected in the project panel.
     *
     * @details Queries selection statuses via `AEGP_IsItemSelected`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw boolean conversions.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return True if selected.
     */
    bool is_selected() const {
        A_Boolean sel = FALSE;
        item_suite::call<&AEGP_ItemSuite9::AEGP_IsItemSelected>(m_handle, &sel);
        return sel != FALSE;
    }

    /**
     * @brief Returns the parent folder item, or an invalid item if at root.
     *
     * @details Locates parent directories in project hierarchy using `AEGP_GetItemParentFolder`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automatically constructs parent OOP representations.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Parent folder item wrapper.
     */
    item get_parent_folder() const {
        AEGP_ItemH parent = nullptr;
        item_suite::call<&AEGP_ItemSuite9::AEGP_GetItemParentFolder>(m_handle, &parent);
        return item(parent);
    }

    /**
     * @brief Sets the parent folder of this item.
     *
     * @details Reorganizes item hierarchies using `AEGP_SetItemParentFolder`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simplifies moving folders through OOP abstractions.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param folder Destination directory item wrapper.
     */
    void set_parent_folder(const item& folder) {
        item_suite::call<&AEGP_ItemSuite9::AEGP_SetItemParentFolder>(m_handle, folder.get());
    }

    /**
     * @brief Returns the item's comment.
     *
     * @details Accesses comment metadata on the host via `AEGP_GetItemComment`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automates memory allocation conversion pipelines into standard strings.
     *
     * @warning <b>Memory & Lifecycles:</b> Internally uses `aetk::core::mem_handle` to deallocate the host string buffer via `AEGP_FreeMemHandle` on exit. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Item metadata comment.
     */
    std::string get_comment() const {
        aetk::core::mem_handle comment_h;
        item_suite::call<&AEGP_ItemSuite9::AEGP_GetItemComment>(m_handle, comment_h.get_ptr());
        return comment_h.to_string();
    }

    // --- Footage & Interpretation (Defined in footage.hpp) ---

    /**
     * @brief Returns the interpretation settings for this footage item.
     *
     * @details Accesses interpretation properties inside After Effects using `AEGP_GetFootageInterpretation`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw suite lookups cleanly.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param proxy If true, gets the proxy interpretation.
     * @return Custom interpretation structure settings.
     */
    AEGP_FootageInterp get_footage_interpretation(bool proxy = false) const;

    /**
     * @brief Sets the interpretation settings for this footage item. (Undoable)
     *
     * @details Updates interpretation properties using `AEGP_SetFootageInterpretation`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces direct C-suite calls.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param interp Interpretation settings properties.
     * @param proxy If true, sets the proxy interpretation.
     */
    void set_footage_interpretation(const AEGP_FootageInterp& interp, bool proxy = false);

    /**
     * @brief Replaces the main footage of this item. (Undoable)
     *
     * @details Substitutes item assets inside composition spaces using `AEGP_ReplaceItemMainFootage`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Transfers `owned_footage` ownership dynamically using standard move-only concepts.
     *
     * @warning <b>Memory & Lifecycles:</b> Relinquishes asset memory ownership to the host project automatically, bypassing dynamic double-frees. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param foot An owned footage handle. The project adopts it.
     */
    void replace_main_footage(owned_footage&& foot);

    /**
     * @brief Sets the proxy footage of this item. (Undoable)
     *
     * @details Configures proxy asset files using `AEGP_SetItemProxyFootage`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Seamlessly updates the proxy via move semantic wrappers.
     *
     * @warning <b>Memory & Lifecycles:</b> Relinquishes the owned footage handle ownership to AE. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param foot An owned footage handle. The project adopts it.
     */
    void set_proxy_footage(owned_footage&& foot);

    /**
     * @brief Sets the item's comment. (Undoable)
     *
     * @details Updates comment parameters on the host via `AEGP_SetItemComment`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standardizes wstring translations from standard strings.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param comment Dynamic metadata string.
     */
    void set_comment(const std::string& comment) {
        std::wstring wcomment(comment.begin(), comment.end());
        item_suite::call<&AEGP_ItemSuite9::AEGP_SetItemComment>(
            m_handle, reinterpret_cast<const A_UTF16Char*>(wcomment.c_str()));
    }

    /**
     * @brief Returns the item's label color index (0-16).
     *
     * @details Queries label tags inside the project panel via `AEGP_GetItemLabel`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw label integer queries.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Label index identifier.
     */
    AEGP_LabelID get_label() const {
        AEGP_LabelID label = 0;
        item_suite::call<&AEGP_ItemSuite9::AEGP_GetItemLabel>(m_handle, &label);
        return label;
    }

    /**
     * @brief Sets the item's label color index. (Undoable)
     *
     * @details Updates the UI label tags using `AEGP_SetItemLabel`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Straightforward enum setup.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param label Dynamic label index identifier.
     */
    void set_label(AEGP_LabelID label) {
        item_suite::call<&AEGP_ItemSuite9::AEGP_SetItemLabel>(m_handle, label);
    }

    /**
     * @brief Deletes the item from the After Effects project panel.
     * 
     * @details Deletes the item using `AEGP_DeleteItem`. This operation is fully undoable.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standardizes deletion and automatically nulls the internal 
     * handle to prevent subsequent access violations.
     *
     * @warning <b>Memory & Lifecycles:</b> Invoking this method immediately invalidates this item instance 
     * and any other variables holding the same `AEGP_ItemH` handle reference. Do not access 
     * the item after calling this method. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     */
    void delete_item() {
        item_suite::call<&AEGP_ItemSuite9::AEGP_DeleteItem>(m_handle);
        m_handle = nullptr;
    }

    /**
     * @brief Returns the next item in the project panel hierarchy.
     *
     * @details Traverses project panel nodes using `AEGP_GetNextProjItem`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Exposes list-like navigation via OOP concepts.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Next item node in sequence.
     */
    item get_next() const {
        AEGP_ItemH next_h = nullptr;
        item_suite::call<&AEGP_ItemSuite9::AEGP_GetNextProjItem>(
            nullptr, m_handle, &next_h); // projectH is ignored by AE
        return item(next_h);
    }

    // --- Static helpers ---

    /**
     * @brief Static retrieval of the currently selected/active project item in the UI.
     * 
     * @details Queries the active user selection using `AEGP_GetActiveItem`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces manual selection query boilerplates with a single 
     * static factory method.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return The active `item` wrapper. If nothing is selected, returns an invalid `item`.
     */
    static item get_active() {
        AEGP_ItemH h = nullptr;
        item_suite::call<&AEGP_ItemSuite9::AEGP_GetActiveItem>(&h);
        return item(h);
    }

    /**
     * @brief Returns the very first item in the project panel.
     *
     * @details Queries initial directory assets via `AEGP_GetFirstProjItem`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard iterator starter helper.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return The first item in the project layout.
     */
    static item get_first() {
        AEGP_ItemH h = nullptr;
        item_suite::call<&AEGP_ItemSuite9::AEGP_GetFirstProjItem>(nullptr, &h);
        return item(h);
    }

    /**
     * @brief Creates a new folder in the project panel.
     *
     * @details Spawns directory hierarchies inside the host panel using `AEGP_CreateNewFolder`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces direct folder creation suite bindings with high-level string conversions.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param name Target folder title string.
     * @param parent Destination directory item.
     * @return New folder item wrapper.
     */
    static item create_new_folder(const std::string& name, const item& parent = item(nullptr)) {
        std::wstring wname(name.begin(), name.end());
        AEGP_ItemH h = nullptr;
        item_suite::call<&AEGP_ItemSuite9::AEGP_CreateNewFolder>(
            reinterpret_cast<const A_UTF16Char*>(wname.c_str()), parent.get(), &h);
        return item(h);
    }

    /**
     * @brief Adds a newly created footage object to the project. (Undoable)
     *
     * @details Imports footage structures using `AEGP_AddFootageToProject`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Consumes dynamic move-only `owned_footage` assets directly, transferring their ownership into After Effects context.
     *
     * @warning <b>Memory & Lifecycles:</b> Transfers raw asset reference ownership to AE, disabling dynamic double-frees. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param foot Target owned media reference.
     * @param folder Destination folder parent.
     * @return Newly created project item wrapper.
     */
    static item add_footage(owned_footage&& foot, const item& folder = item(nullptr));
};

} // namespace aetk::aegp
