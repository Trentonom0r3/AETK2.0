#pragma once

#include <aetk/core/suite.hpp>
#include <aetk/core/handle.hpp>
#include <aetk/aegp/dom/item.hpp>
#include <aetk/aegp/dom/stream.hpp>
#include <string>
#include <vector>

// Forward declare layer so comp can return them
namespace aetk::aegp {
    class layer;
    class mask;
    class owned_mask;
}

namespace aetk::aegp {

// ============================================================
//  Traits & Suites
// ============================================================

/**
 * @brief Traits helper for standard composition items.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Establishes type trait mappings for `AEGP_CompH` handles.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct comp_traits {
    using type = AEGP_CompH;
};

using comp_suite  = aetk::core::suite<AEGP_CompSuite12,
    aetk::core::fixed_string(kAEGPCompSuite), kAEGPCompSuiteVersion12>;
using layer_suite = aetk::core::suite<AEGP_LayerSuite9,
    aetk::core::fixed_string(kAEGPLayerSuite), kAEGPLayerSuiteVersion9>;

// ============================================================
//  comp — Inherits from item (Comp IS-A Item)
// ============================================================

/**
 * @brief Represents a standard composition DOM tree node inside After Effects.
 * 
 * @details This class is the core object-oriented wrapper around `AEGP_CompH`. 
 * It inherits from `aetk::aegp::item` (since a Composition is also an Item in the Project Panel). 
 * It provides extensive properties to query and modify composition settings (including background 
 * color, frame rates, shutter options, motion blur, and layer sets).
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, `AEGP_CompH` is a separate opaque handle 
 * from `AEGP_ItemH`. Accessing its settings requires checking out `AEGP_CompSuite12` and 
 * managing handle conversions manually. `aetk::aegp::comp` automatically resolves the 
 * backing `item` handle upon construction and handles suite dispatches transparently.
 *
 * @warning <b>Memory & Lifecycles:</b> This is a <b>borrowed</b> handle representation wrapping `AEGP_CompH`. 
 * Composition lifetimes are governed entirely by the After Effects host. Standard composition operations 
 * (such as changing framerates or adding layers) should only be executed on the main UI thread. 
 * Invoking modification methods during render phases (e.g. inside an effect's Smart Render loop) 
 * will result in host errors. Relies on `aetk::core::suite` which automatically decrements the host 
 * reference count via `ReleaseSuite` when it goes out of scope.
 */
class comp : public item {
public:
    /**
     * @brief Null constructor.
     *
     * @details Instantiates an uninitialized, null composition handle wrapper.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Creates standard safe null initializers.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    comp() : item(), m_comp_handle(nullptr) {}

    /**
     * @brief Construct from a CompH. Automatically resolves the backing ItemH.
     *
     * @details Explicitly binds a raw `AEGP_CompH` and queries the parent `AEGP_ItemH` via `AEGP_GetItemFromComp`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automatically establishes parent-child relationship mappings inside the object model.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param ch Target raw composition handle.
     */
    explicit comp(AEGP_CompH ch) : item(), m_comp_handle(ch) {
        if (ch) {
            AEGP_ItemH ih = nullptr;
            comp_suite::call<&AEGP_CompSuite12::AEGP_GetItemFromComp>(ch, &ih);
            this->m_handle = ih; // Set the parent item handle
        }
    }

    /**
     * @brief Construct from a base item.
     * 
     * @details Resolves the composition handle if the item type is a composition.
     */
    explicit comp(const item& it) : item(it), m_comp_handle(nullptr) {
        if (it && it.get_type() == AEGP_ItemType_COMP) {
            AEGP_CompH ch = nullptr;
            comp_suite::call<&AEGP_CompSuite12::AEGP_GetCompFromItem>(it.get(), &ch);
            m_comp_handle = ch;
        }
    }

    comp& operator=(const item& it) {
        if (it && it.get_type() == AEGP_ItemType_COMP) {
            this->m_handle = it.get();
            AEGP_CompH ch = nullptr;
            comp_suite::call<&AEGP_CompSuite12::AEGP_GetCompFromItem>(it.get(), &ch);
            m_comp_handle = ch;
        } else {
            this->m_handle = nullptr;
            m_comp_handle = nullptr;
        }
        return *this;
    }

    /**
     * @brief Raw comp handle access.
     *
     * @details Provides direct access to the underlying `AEGP_CompH` pointer.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard pointer extraction.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Raw composition handle pointer.
     */
    AEGP_CompH get_comp_handle() const { return m_comp_handle; }

    // --- Comp-specific properties ---

    /**
     * @brief Returns the number of layers in this composition.
     *
     * @details Queries the layer count of the composition via `AEGP_GetCompNumLayers`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe integer conversion.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Total number of layers inside the composition.
     */
    int32_t get_num_layers() const {
        A_long count = 0;
        layer_suite::call<&AEGP_LayerSuite9::AEGP_GetCompNumLayers>(m_comp_handle, &count);
        return static_cast<int32_t>(count);
    }

    /**
     * @brief Returns the comp's background color.
     *
     * @details Queries the solid background color setting inside After Effects using `AEGP_GetCompBGColor`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automatically converts the raw host color structure into a modern high-fidelity `aetk::core::color<>` wrapper.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Background color class wrapper.
     */
    aetk::core::color<> get_bg_color() const {
        AEGP_ColorVal c{};
        comp_suite::call<&AEGP_CompSuite12::AEGP_GetCompBGColor>(m_comp_handle, &c);
        return aetk::core::color<>(c);
    }

    /**
     * @brief Sets the comp's background color. (Undoable)
     *
     * @details Updates composition workspace visual properties inside AE using `AEGP_SetCompBGColor`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct color class conversions.
     *
     * @warning <b>Memory & Lifecycles:</b> Must be called on the primary thread. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param color High-fidelity color settings.
     */
    void set_bg_color(const aetk::core::color<>& color) {
        AEGP_ColorVal c = color;
        comp_suite::call<&AEGP_CompSuite12::AEGP_SetCompBGColor>(m_comp_handle, &c);
    }

    /**
     * @brief Returns the downsample factor (e.g. half-res, quarter-res).
     *
     * @details Queries viewport scaling configurations via `AEGP_GetCompDownsampleFactor`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Returns host factor ratios.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Target resolution downsampling factor struct.
     */
    AEGP_DownsampleFactor get_downsample_factor() const {
        AEGP_DownsampleFactor dsf{};
        comp_suite::call<&AEGP_CompSuite12::AEGP_GetCompDownsampleFactor>(m_comp_handle, &dsf);
        return dsf;
    }

    /**
     * @brief Sets the downsample factor for rendering this comp.
     *
     * @details Modifies viewport rendering factor limits using `AEGP_SetCompDownsampleFactor`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Easy viewport adjustments.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param dsf Downsample configurations.
     */
    void set_downsample_factor(const AEGP_DownsampleFactor& dsf) {
        comp_suite::call<&AEGP_CompSuite12::AEGP_SetCompDownsampleFactor>(m_comp_handle, &dsf);
    }

    /**
     * @brief Returns the comp's flags (e.g. motion blur enabled, draft 3D).
     *
     * @details Queries core configuration bitmasks via `AEGP_GetCompFlags`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean bitmask extraction.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Composition settings bitmask flags.
     */
    AEGP_CompFlags get_flags() const {
        AEGP_CompFlags flags = 0;
        comp_suite::call<&AEGP_CompSuite12::AEGP_GetCompFlags>(m_comp_handle, &flags);
        return flags;
    }

    /**
     * @brief Returns a layer by its index (0-based).
     *
     * @details Resolves layers in comp timeline hierarchies via `AEGP_GetCompLayerByIndex`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> OOP layer creation wrappers.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param index Layer index in stack order.
     * @return Borrowed `layer` wrapper.
     */
    layer get_layer(int32_t index) const;

    /**
     * @brief Returns all layers in this composition.
     *
     * @details Recursively fetches all layers present inside the composition.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw loop iterations with modern `std::vector` lists.
     *
     * @warning <b>Memory & Lifecycles:</b> Allocates a vector of borrowed layers. Memory managed by calling scopes.
     *
     * @return Collection array of borrowed layers.
     */
    std::vector<layer> get_all_layers() const;

    /**
     * @brief Returns the comp's frame rate (fps).
     *
     * @details Queries compositions project frame rates via `AEGP_GetCompFramerate`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Returns double value directly.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Frame rate value.
     */
    double get_framerate() const {
        A_FpLong fps = 0;
        comp_suite::call<&AEGP_CompSuite12::AEGP_GetCompFramerate>(m_comp_handle, &fps);
        return static_cast<double>(fps);
    }

    /**
     * @brief Sets the comp's frame rate (fps).
     *
     * @details Updates composition timing properties inside After Effects using `AEGP_SetCompFrameRate`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Straightforward fps updates.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param fps Target frame rate.
     */
    void set_framerate(double fps) {
        A_FpLong raw = static_cast<A_FpLong>(fps);
        comp_suite::call<&AEGP_CompSuite12::AEGP_SetCompFrameRate>(m_comp_handle, &raw);
    }

    /**
     * @brief Returns the shutter angle and phase.
     *
     * @details Queries physical rendering motion camera properties using `AEGP_GetCompShutterAnglePhase`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Translates fractional numbers cleanly to ratio classes.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Pair containing angle and phase ratio wrappers.
     */
    std::pair<aetk::core::ratio, aetk::core::ratio> get_shutter_angle_phase() const {
        A_Ratio angle{}, phase{};
        comp_suite::call<&AEGP_CompSuite12::AEGP_GetCompShutterAnglePhase>(m_comp_handle, &angle, &phase);
        return {aetk::core::ratio(angle), aetk::core::ratio(phase)};
    }

    /**
     * @brief Returns the shutter frame range (start, duration).
     *
     * @details Resolves motion camera opening ranges inside composition timelines via `AEGP_GetCompShutterFrameRange`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> High-fidelity time conversions.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param comp_time Active composition timeline frame index.
     * @return Pair containing opening duration time wrappers.
     */
    std::pair<aetk::core::time, aetk::core::time> get_shutter_frame_range(const aetk::core::time& comp_time) const {
        A_Time raw_time = comp_time;
        A_Time start{}, duration{};
        comp_suite::call<&AEGP_CompSuite12::AEGP_GetCompShutterFrameRange>(
            m_comp_handle, &raw_time, &start, &duration);
        return {aetk::core::time(start), aetk::core::time(duration)};
    }

    /**
     * @brief Returns the suggested number of motion blur samples for the comp.
     *
     * @details Queries rendering recommendations using `AEGP_GetCompSuggestedMotionBlurSamples`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Integer type normalization.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Motion blur samples count.
     */
    int32_t get_suggested_motion_blur_samples() const {
        A_long samples = 0;
        comp_suite::call<&AEGP_CompSuite12::AEGP_GetCompSuggestedMotionBlurSamples>(
            m_comp_handle, &samples);
        return static_cast<int32_t>(samples);
    }

    // --- UI State ---

    /**
     * @brief Returns whether layer names or source names are shown in the timeline.
     *
     * @details Queries timeline track display toggles via `AEGP_GetShowLayerNameOrSourceName`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simple boolean translation.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return True if layer names are prioritized.
     */
    bool get_show_layer_names() const {
        A_Boolean shown = FALSE;
        comp_suite::call<&AEGP_CompSuite12::AEGP_GetShowLayerNameOrSourceName>(m_comp_handle, &shown);
        return shown != FALSE;
    }

    /**
     * @brief Sets whether layer names or source names are shown. (Opens the comp)
     *
     * @details Configures timeline tracks UI attributes via `AEGP_SetShowLayerNameOrSourceName`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Seamless UI switch.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param show_layer_names True to activate layer titles.
     */
    void set_show_layer_names(bool show_layer_names) {
        comp_suite::call<&AEGP_CompSuite12::AEGP_SetShowLayerNameOrSourceName>(
            m_comp_handle, show_layer_names ? TRUE : FALSE);
    }

    /**
     * @brief Returns whether blend modes are shown in the timeline.
     *
     * @details Queries UI layout columns statuses via `AEGP_GetShowBlendModes`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw host checks.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return True if visible.
     */
    bool get_show_blend_modes() const {
        A_Boolean shown = FALSE;
        comp_suite::call<&AEGP_CompSuite12::AEGP_GetShowBlendModes>(m_comp_handle, &shown);
        return shown != FALSE;
    }

    /**
     * @brief Sets whether blend modes are shown. (Opens the comp)
     *
     * @details Modifies timeline panel columns using `AEGP_SetShowBlendModes`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simplified viewport controls.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param show_blend_modes True to display modes.
     */
    void set_show_blend_modes(bool show_blend_modes) {
        comp_suite::call<&AEGP_CompSuite12::AEGP_SetShowBlendModes>(
            m_comp_handle, show_blend_modes ? TRUE : FALSE);
    }

    // --- Static helpers ---

    /**
     * @brief Creates a comp from an item handle.
     *
     * @details Standard promotion to composition wrappers using `AEGP_GetCompFromItem`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Static promotion.
     *
     * @warning <b>Memory & Lifecycles:</b> Throws if the backing item is not a composition. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param it Target project item.
     * @return Composition wrapper.
     */
    static comp from_item(const item& it) {
        AEGP_CompH h = nullptr;
        comp_suite::call<&AEGP_CompSuite12::AEGP_GetCompFromItem>(it.get(), &h);
        return comp(h);
    }

private:
    AEGP_CompH m_comp_handle;
};

// ============================================================
//  layer — Wraps AEGP_LayerH (Borrowed)
// ============================================================

/**
 * @brief Traits helper for layers.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Establishes standard trait structures for mapping `AEGP_LayerH` handles.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct layer_traits {
    using type = AEGP_LayerH;
};

/**
 * @brief Represents a borrowed timeline layer node inside an After Effects composition.
 * 
 * @details Wraps the raw `AEGP_LayerH` handle, modeling individual timeline tracks within 
 * a composition hierarchy. Exposes layout details, flags (Solo, Shy, Locked), blend modes, 
 * time manipulation, and property stream hooks (Position, Scale, Anchor Point).
 *
 * @note <b>AE SDK Paradigm Shift:</b> Replaces raw `AEGP_LayerSuite9` dispatches with direct, 
 * object-oriented property methods and returns type-safe time wrappers.
 *
 * @warning <b>Memory & Lifecycles:</b> Layers are owned by their parent compositions. If a layer 
 * is deleted in the UI or by another routine, subsequent calls on its `layer` instance 
 * will trigger access violations. Acquires properties and stream accessors using `aetk::core::suite` 
 * (which automatically decrements the host reference count via `ReleaseSuite` when the suite 
 * goes out of scope). Property checkouts like `get_stream()` return an `owned_stream` which 
 * automatically disposes the stream reference via `AEGP_DisposeStream` upon exiting scope.
 */
class layer : public aetk::core::borrowed<layer_traits> {
public:
    using borrowed::borrowed;

    // --- Properties ---

    /**
     * @brief Returns the layer's name as UTF-8.
     *
     * @details Accesses name and source strings inside After Effects using `AEGP_GetLayerName`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Resolves name parameters cleanly, returning an automated standard string.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses temporary string allocators via `aetk::core::mem_handle`, which deallocate the host string buffer via `AEGP_FreeMemHandle` on exit. Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return UTF-8 name string.
     */
    std::string get_name() const {
        aetk::core::mem_handle layer_name_h;
        aetk::core::mem_handle source_name_h;
        layer_suite::call<&AEGP_LayerSuite9::AEGP_GetLayerName>(
            aetk::core::context::get_plugin_id(),
            m_handle,
            layer_name_h.get_ptr(),
            source_name_h.get_ptr());

        std::string name = layer_name_h.to_string();
        if (name.empty()) {
            name = source_name_h.to_string();
        }
        return name;
    }

    /**
     * @brief Returns the layer's index in its parent comp (0 = topmost).
     *
     * @details Queries index configurations inside AE using `AEGP_GetLayerIndex`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Straightforward integer mapping.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Layer stack index.
     */
    int32_t get_index() const {
        A_long idx = 0;
        layer_suite::call<&AEGP_LayerSuite9::AEGP_GetLayerIndex>(m_handle, &idx);
        return static_cast<int32_t>(idx);
    }

    /**
     * @brief Returns the layer's unique persistent ID.
     *
     * @details Queries database ID configurations inside AE using `AEGP_GetLayerID`.
     *
     * @return Layer unique ID.
     */
    int32_t get_id() const {
        A_long val = 0;
        layer_suite::call<&AEGP_LayerSuite9::AEGP_GetLayerID>(m_handle, &val);
        return static_cast<int32_t>(val);
    }

    /**
     * @brief Returns the source item for this layer.
     *
     * @details Locates physical backing assets inside the project using `AEGP_GetLayerSourceItem`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Wraps C handles inside our modern `item` OOP structures.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Backing project item wrapper.
     */
    item get_source_item() const {
        AEGP_ItemH h = nullptr;
        layer_suite::call<&AEGP_LayerSuite9::AEGP_GetLayerSourceItem>(m_handle, &h);
        return item(h);
    }

    /**
     * @brief Returns the unique ID of the source item.
     *
     * @details Queries persistent IDs using `AEGP_GetLayerSourceItemID`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean integer mapping.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Persistent project item ID.
     */
    int32_t get_source_item_id() const {
        A_long id = 0;
        layer_suite::call<&AEGP_LayerSuite9::AEGP_GetLayerSourceItemID>(m_handle, &id);
        return static_cast<int32_t>(id);
    }

    /**
     * @brief Returns the parent composition of this layer.
     *
     * @details Traverses hierarchy up to compositions level using `AEGP_GetLayerParentComp`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automatically constructs `comp` OOP wrappers.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Parent composition wrapper.
     */
    comp get_parent_comp() const {
        AEGP_CompH h = nullptr;
        layer_suite::call<&AEGP_LayerSuite9::AEGP_GetLayerParentComp>(m_handle, &h);
        return comp(h);
    }

    /**
     * @brief Returns whether this layer's video is actually visible.
     *
     * @details Evaluates stack priorities to resolve real visibility status via `AEGP_IsLayerVideoReallyOn`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Straightforward boolean mapping.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return True if visible.
     */
    bool is_video_on() const {
        A_Boolean on = FALSE;
        layer_suite::call<&AEGP_LayerSuite9::AEGP_IsLayerVideoReallyOn>(m_handle, &on);
        return on != FALSE;
    }

    /**
     * @brief Returns whether this layer's audio is actually audible.
     * 
     * @quirk Accounts for solo status of other layers.
     *
     * @details Evaluates composition states via `AEGP_IsLayerAudioReallyOn`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Encapsulates complicated host tracking parameters.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return True if audible.
     */
    bool is_audio_on() const {
        A_Boolean on = FALSE;
        layer_suite::call<&AEGP_LayerSuite9::AEGP_IsLayerAudioReallyOn>(m_handle, &on);
        return on != FALSE;
    }

    // --- Properties & State ---

    /**
     * @brief Returns the layer's rendering quality (Draft, Best, Wireframe).
     *
     * @details Queries UI quality flags via `AEGP_GetLayerQuality`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean enum mapping.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Quality status enum.
     */
    AEGP_LayerQuality get_quality() const {
        AEGP_LayerQuality q{};
        layer_suite::call<&AEGP_LayerSuite9::AEGP_GetLayerQuality>(m_handle, &q);
        return q;
    }

    /**
     * @brief Sets the layer's rendering quality. (Undoable)
     *
     * @details Modifies rendering parameters using `AEGP_SetLayerQuality`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct quality switches.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param quality Quality status enum.
     */
    void set_quality(AEGP_LayerQuality quality) {
        layer_suite::call<&AEGP_LayerSuite9::AEGP_SetLayerQuality>(m_handle, quality);
    }

    /**
     * @brief Returns the layer's flags (e.g. Solo, Shy, Locked, 3D).
     *
     * @details Queries general timeline options via `AEGP_GetLayerFlags`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bitmask flag resolution.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Layer flags bitmask.
     */
    AEGP_LayerFlags get_flags() const {
        AEGP_LayerFlags flags = 0;
        layer_suite::call<&AEGP_LayerSuite9::AEGP_GetLayerFlags>(m_handle, &flags);
        return flags;
    }

    /**
     * @brief Sets or clears a single layer flag.
     *
     * @details Updates parameters like locked or shy statuses using `AEGP_SetLayerFlag`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simplifies bit operations to boolean triggers.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param single_flag Target flag enum.
     * @param value True to activate.
     */
    void set_flag(AEGP_LayerFlags single_flag, bool value) {
        layer_suite::call<&AEGP_LayerSuite9::AEGP_SetLayerFlag>(m_handle, single_flag, value ? TRUE : FALSE);
    }

    /**
     * @brief Returns the transfer mode (blend mode, track matte, etc).
     *
     * @details Queries composition blend states using `AEGP_GetLayerTransferMode`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces C struct queries.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Transfer mode configuration structure.
     */
    AEGP_LayerTransferMode get_transfer_mode() const {
        AEGP_LayerTransferMode mode{};
        layer_suite::call<&AEGP_LayerSuite9::AEGP_GetLayerTransferMode>(m_handle, &mode);
        return mode;
    }

    /**
     * @brief Sets the transfer mode. (Undoable)
     *
     * @details Modifies composition blending algorithms via `AEGP_SetLayerTransferMode`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard mode setups.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param mode Transfer mode structure.
     */
    void set_transfer_mode(const AEGP_LayerTransferMode& mode) {
        layer_suite::call<&AEGP_LayerSuite9::AEGP_SetLayerTransferMode>(m_handle, &mode);
    }

    // --- Time Manipulation ---

    /**
     * @brief Returns the layer's current time based on the specified mode (Comp or Layer time).
     *
     * @details Resolves the timeline frame indices via `AEGP_GetLayerCurrentTime`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Translates internal structures into high-fidelity `aetk::core::time` classes.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param time_mode Time frame index layout mode.
     * @return Time index wrapper.
     */
    aetk::core::time get_current_time(AEGP_LTimeMode time_mode = AEGP_LTimeMode_CompTime) const {
        A_Time t{};
        layer_suite::call<&AEGP_LayerSuite9::AEGP_GetLayerCurrentTime>(m_handle, time_mode, &t);
        return aetk::core::time(t);
    }

    /**
     * @brief Returns the layer's in-point.
     *
     * @details Queries start time offsets using `AEGP_GetLayerInPoint`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Returns automated time wrappers.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param time_mode Time frame index layout mode.
     * @return Time index wrapper.
     */
    aetk::core::time get_in_point(AEGP_LTimeMode time_mode = AEGP_LTimeMode_CompTime) const {
        A_Time t{};
        layer_suite::call<&AEGP_LayerSuite9::AEGP_GetLayerInPoint>(m_handle, time_mode, &t);
        return aetk::core::time(t);
    }

    /**
     * @brief Returns the layer's duration.
     *
     * @details Queries timeline lengths inside AE using `AEGP_GetLayerDuration`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard time wrappers.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param time_mode Time frame index layout mode.
     * @return Time index wrapper.
     */
    aetk::core::time get_duration(AEGP_LTimeMode time_mode = AEGP_LTimeMode_CompTime) const {
        A_Time t{};
        layer_suite::call<&AEGP_LayerSuite9::AEGP_GetLayerDuration>(m_handle, time_mode, &t);
        return aetk::core::time(t);
    }

    /**
     * @brief Sets the layer's in-point and duration. (Undoable)
     *
     * @details Updates active timeline boundaries using `AEGP_SetLayerInPointAndDuration`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Accepts modern time wrappers seamlessly.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param time_mode Time frame index layout mode.
     * @param in_point Target start time offset.
     * @param duration Target timeline duration.
     */
    void set_in_point_and_duration(AEGP_LTimeMode time_mode, const aetk::core::time& in_point, const aetk::core::time& duration) {
        A_Time raw_in = in_point, raw_dur = duration;
        layer_suite::call<&AEGP_LayerSuite9::AEGP_SetLayerInPointAndDuration>(
            m_handle, time_mode, &raw_in, &raw_dur);
    }

    /**
     * @brief Returns the layer's time offset (always in comp time).
     *
     * @details Queries layer offsets using `AEGP_GetLayerOffset`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct time wrappers.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Time index wrapper.
     */
    aetk::core::time get_offset() const {
        A_Time t{};
        layer_suite::call<&AEGP_LayerSuite9::AEGP_GetLayerOffset>(m_handle, &t);
        return aetk::core::time(t);
    }

    /**
     * @brief Sets the layer's time offset. (Undoable)
     *
     * @details Updates timeline location properties using `AEGP_SetLayerOffset`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Easy offset modifications.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @param offset Target offset location.
     */
    void set_offset(const aetk::core::time& offset) {
        A_Time raw = offset;
        layer_suite::call<&AEGP_LayerSuite9::AEGP_SetLayerOffset>(m_handle, &raw);
    }

    // --- Streams ---

    /**
     * @brief Resolves and checks out a target property stream (e.g. Position, Scale, Anchor Point).
     * 
     * @details Invokes `AEGP_GetNewLayerStream` through `AEGP_StreamSuite6` to retrieve an 
     * owned, managed property stream wrapper.
     *
     * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, checking out a layer stream yields an 
     * `AEGP_StreamRefH` which <b>must</b> be manually checked in/disposed via `AEGP_DisposeStream` 
     * to avoid memory leaks. This method returns an `owned_stream` RAII class that automatically 
     * disposes itself when it goes out of scope.
     *
     * @warning <b>Memory & Lifecycles:</b> The returned `owned_stream` resource MUST not outlive the 
     * parent `layer` or the composition lifetime.
     * 
     * @param stream_type The target property stream type to acquire.
     * @return An `owned_stream` RAII wrapper managing the checked-out stream lifetime.
     */
    owned_stream get_stream(AEGP_LayerStream stream_type) const;

    AEGP_ObjectType get_object_type() const {
        AEGP_ObjectType type = AEGP_ObjectType_NONE;
        layer_suite::call<&AEGP_LayerSuite9::AEGP_GetLayerObjectType>(m_handle, &type);
        return type;
    }

    int32_t get_num_masks() const;
    owned_mask get_mask(int32_t index) const;

    // --- Static helpers ---

    /**
     * @brief Returns the currently active (selected) layer.
     *
     * @details Queries active user viewport layer selection via `AEGP_GetActiveLayer`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Static shortcut creation.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which automatically decrements the host reference count via `ReleaseSuite` when it goes out of scope.
     *
     * @return Active selected layer wrapper.
     */
    static layer get_active() {
        AEGP_LayerH h = nullptr;
        layer_suite::call<&AEGP_LayerSuite9::AEGP_GetActiveLayer>(&h);
        return layer(h);
    }
};

// --- Deferred implementations ---

inline layer comp::get_layer(int32_t index) const {
    AEGP_LayerH h = nullptr;
    layer_suite::call<&AEGP_LayerSuite9::AEGP_GetCompLayerByIndex>(m_comp_handle, static_cast<A_long>(index), &h);
    return layer(h);
}

inline std::vector<layer> comp::get_all_layers() const {
    auto count = get_num_layers();
    std::vector<layer> layers;
    layers.reserve(count);
    for (int32_t i = 0; i < count; ++i) {
        layers.push_back(get_layer(i));
    }
    return layers;
}

inline owned_stream layer::get_stream(AEGP_LayerStream stream_type) const {
    using stream_suite = aetk::core::suite<AEGP_StreamSuite6, 
        aetk::core::fixed_string(kAEGPStreamSuite), kAEGPStreamSuiteVersion6>;
    
    AEGP_StreamRefH streamH = nullptr;
    stream_suite::call<&AEGP_StreamSuite6::AEGP_GetNewLayerStream>(
        aetk::core::context::get_plugin_id(), m_handle, stream_type, &streamH);
    return owned_stream(streamH);
}

} // namespace aetk::aegp

#include <aetk/aegp/dom/mask.hpp>

namespace aetk::aegp {

inline int32_t layer::get_num_masks() const {
    using mask_suite = aetk::core::suite<AEGP_MaskSuite6, 
        aetk::core::fixed_string(kAEGPMaskSuite), kAEGPMaskSuiteVersion6>;
    A_long num_masks = 0;
    mask_suite::call<&AEGP_MaskSuite6::AEGP_GetLayerNumMasks>(m_handle, &num_masks);
    return static_cast<int32_t>(num_masks);
}

inline owned_mask layer::get_mask(int32_t index) const {
    using mask_suite = aetk::core::suite<AEGP_MaskSuite6, 
        aetk::core::fixed_string(kAEGPMaskSuite), kAEGPMaskSuiteVersion6>;
    AEGP_MaskRefH maskH = nullptr;
    mask_suite::call<&AEGP_MaskSuite6::AEGP_GetLayerMaskByIndex>(m_handle, static_cast<A_long>(index), &maskH);
    return owned_mask(maskH);
}

} // namespace aetk::aegp
