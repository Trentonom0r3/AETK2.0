#pragma once

#include <aetk/aegp/dom/item.hpp>
#include <aetk/core/handle.hpp>
#include <aetk/core/mem_handle.hpp>
#include <aetk/core/suite.hpp>
#include <string>


namespace aetk::aegp {

// ============================================================
//  Traits & Suites
// ============================================================

/**
 * @brief Traits helper for standard footage items.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Defines traits for mapping
 * `AEGP_FootageH` lifecycle operations, standardizing how borrowed and owned
 * wrappers handle memory deallocations.
 *
 * @warning <b>Memory & Lifecycles:</b> The `dispose` method triggers
 * `AEGP_DisposeFootage` internally using `aetk::core::suite` which
 * automatically decrements the host reference count via `ReleaseSuite` when it
 * goes out of scope.
 */
struct footage_traits {
  using type = AEGP_FootageH;
  static void dispose(AEGP_FootageH h) {
    using suite = aetk::core::suite<AEGP_FootageSuite5,
                                    aetk::core::fixed_string(kAEGPFootageSuite),
                                    kAEGPFootageSuiteVersion5>;
    suite::call<&AEGP_FootageSuite5::AEGP_DisposeFootage>(h);
  }
};

using footage_suite =
    aetk::core::suite<AEGP_FootageSuite5,
                      aetk::core::fixed_string(kAEGPFootageSuite),
                      kAEGPFootageSuiteVersion5>;

// ============================================================
//  footage — Wraps AEGP_FootageH (Borrowed)
//
//  A footage object represents media (files, solids, etc).
//  It IS-A item, but we don't inherit from item directly because
//  AEGP_FootageH is a different handle type than AEGP_ItemH.
//  Instead, we provide a `.get_item()` method to access the
//  underlying item properties if this footage is part of a project.
// ============================================================

/**
 * @brief Represents a standard, borrowed media asset (footage) inside After
 * Effects.
 *
 * @details This class is the core object-oriented representation of
 * `AEGP_FootageH`. It models physical or generated media backing standard
 * project items (such as image sequences, solid color blocks, nested
 * compositions, or placeholder graphics).
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, developers query footage
 * paths and signatures using `AEGP_FootageSuite5` directly, passing opaque
 * handles. `aetk::aegp::footage` wraps these operations into simple,
 * exception-safe methods, returning type-safe `std::string` and standard
 * containers.
 *
 * @warning <b>Memory & Lifecycles:</b> This is a <b>borrowed</b> representation
 * wrapping `AEGP_FootageH`. The lifecycle of the underlying footage structure
 * is maintained by the host application. Interacting with this object after the
 * backing project item is destroyed is invalid and will cause crashes. Calls to
 * After Effects APIs internally rely on `aetk::core::suite` which automatically
 * decrements the host reference count via `ReleaseSuite` when the suite goes
 * out of scope.
 */
class footage : public aetk::core::borrowed<footage_traits> {
public:
  using borrowed::borrowed;

  /**
   * @brief Construct from a base item.
   */
  explicit footage(const item &it) : borrowed(nullptr) {
    if (it && it.get_type() == AEGP_ItemType_FOOTAGE) {
      AEGP_FootageH h = nullptr;
      footage_suite::call<&AEGP_FootageSuite5::AEGP_GetMainFootageFromItem>(
          it.get(), &h);
      m_handle = h;
    }
  }

  footage &operator=(const item &it) {
    if (it && it.get_type() == AEGP_ItemType_FOOTAGE) {
      AEGP_FootageH h = nullptr;
      footage_suite::call<&AEGP_FootageSuite5::AEGP_GetMainFootageFromItem>(
          it.get(), &h);
      m_handle = h;
    } else {
      m_handle = nullptr;
    }
    return *this;
  }

  // --- Properties ---

  /**
   * @brief Returns the number of files backing this footage.
   *
   * @details Queries physical files linked to the media asset using
   * `AEGP_GetFootageNumFiles`.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Returns file counts organized inside
   * simple pair wrappers.
   *
   * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which
   * automatically decrements the host reference count via `ReleaseSuite` when
   * it goes out of scope.
   *
   * @return Pair of {num_main_files, files_per_frame}.
   */
  std::pair<int32_t, int32_t> get_num_files() const {
    A_long main_files = 0, per_frame = 0;
    footage_suite::call<&AEGP_FootageSuite5::AEGP_GetFootageNumFiles>(
        m_handle, &main_files, &per_frame);
    return {static_cast<int32_t>(main_files), static_cast<int32_t>(per_frame)};
  }

  /**
   * @brief Returns the file path for a specific frame/file index.
   *
   * @details Retrieves physical source directory locations via
   * `AEGP_GetFootagePath`.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Simplifies string allocations and
   * handles conversions to standard strings.
   *
   * @warning <b>Memory & Lifecycles:</b> Internally allocates a temporary
   * string handle via `aetk::core::mem_handle`, which is deallocated
   * automatically via `AEGP_FreeMemHandle` on exit. Uses `aetk::core::suite`
   * which automatically decrements the host reference count via `ReleaseSuite`
   * when it goes out of scope.
   *
   * @param frame_num Frame index (0 to num_main_files).
   * @param file_index Specific file index (usually
   * `AEGP_FOOTAGE_MAIN_FILE_INDEX`).
   * @return Source file path string.
   */
  std::string get_path(int32_t frame_num = 0, int32_t file_index = 0) const {
    aetk::core::mem_handle path_h;
    footage_suite::call<&AEGP_FootageSuite5::AEGP_GetFootagePath>(
        m_handle, static_cast<A_long>(frame_num),
        static_cast<A_long>(file_index), path_h.get_ptr());
    return path_h.to_string();
  }

  /**
   * @brief Returns the footage signature (e.g. SOLID, MISSING, or file type).
   *
   * @details Queries asset format type classifications via
   * `AEGP_GetFootageSignature`.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Returns format signature identifiers
   * cleanly.
   *
   * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which
   * automatically decrements the host reference count via `ReleaseSuite` when
   * it goes out of scope.
   *
   * @return Custom `AEGP_FootageSignature` signature format.
   */
  AEGP_FootageSignature get_signature() const {
    AEGP_FootageSignature sig = AEGP_FootageSignature_MISSING;
    footage_suite::call<&AEGP_FootageSuite5::AEGP_GetFootageSignature>(m_handle,
                                                                       &sig);
    return sig;
  }

  /**
   * @brief Returns the layer key for layered footage (like PSDs).
   *
   * @details Queries source layer descriptors inside PSD or TIFF format sets
   * using `AEGP_GetFootageLayerKey`.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Safe struct mapping wrapper.
   *
   * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which
   * automatically decrements the host reference count via `ReleaseSuite` when
   * it goes out of scope.
   *
   * @return Layered asset `AEGP_FootageLayerKey`.
   */
  AEGP_FootageLayerKey get_layer_key() const {
    AEGP_FootageLayerKey key{};
    footage_suite::call<&AEGP_FootageSuite5::AEGP_GetFootageLayerKey>(m_handle,
                                                                      &key);
    return key;
  }

  // --- Static helpers ---

  /**
   * @brief Gets the main footage from an item.
   *
   * @details Resolves media links from general items using
   * `AEGP_GetMainFootageFromItem`.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Replaces raw handle castings with OOP
   * static constructors.
   *
   * @warning <b>Memory & Lifecycles:</b> Throws if the item is not
   * `AEGP_ItemType_FOOTAGE`. Uses `aetk::core::suite` which automatically
   * decrements the host reference count via `ReleaseSuite` when it goes out of
   * scope.
   *
   * @param it Target project item.
   * @return Borrowed `footage` wrapper.
   */
  static footage from_item(const item &it) {
    AEGP_FootageH h = nullptr;
    footage_suite::call<&AEGP_FootageSuite5::AEGP_GetMainFootageFromItem>(
        it.get(), &h);
    return footage(h);
  }

  /**
   * @brief Gets the proxy footage from an item.
   *
   * @details Resolves proxy media tracks via `AEGP_GetProxyFootageFromItem`.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Replaces raw handle castings.
   *
   * @warning <b>Memory & Lifecycles:</b> Throws if the item does not have a
   * proxy. Uses `aetk::core::suite` which automatically decrements the host
   * reference count via `ReleaseSuite` when it goes out of scope.
   *
   * @param it Target project item.
   * @return Borrowed `footage` wrapper.
   */
  static footage from_item_proxy(const item &it) {
    AEGP_FootageH h = nullptr;
    footage_suite::call<&AEGP_FootageSuite5::AEGP_GetProxyFootageFromItem>(
        it.get(), &h);
    return footage(h);
  }
};

// ============================================================
//  owned_footage — Wraps AEGP_FootageH (Owned)
//
//  Used for footage created via AEGP_NewFootage.
//  Automatically disposes the footage if it is destroyed before
//  being adopted into the project.
// ============================================================

/**
 * @brief Represents an allocated, owned media asset handle that has not yet
 * been added to a project.
 *
 * @details This class wraps raw footage structures created dynamically via
 * `AEGP_NewSolidFootage` or `AEGP_NewPlaceholderFootage`. It utilizes strict
 * move-only RAII semantics: if the object goes out of scope before being
 * adopted into the project, it automatically triggers clean disposal via
 * `AEGP_DisposeFootage` to prevent host-side memory leaks.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Standardizes the management of transient
 * footage creations. Replaces manual, error-prone `AEGP_DisposeFootage` checks
 * during allocation failures or boundary exceptions with strict C++ RAII
 * semantics.
 *
 * @warning <b>Memory & Lifecycles:</b> Upon passing this footage object to
 * `item::add_footage`, `item::replace_main_footage`, or
 * `item::set_proxy_footage`, ownership is transferred directly to the After
 * Effects project. The internal handle is released, preventing double-disposal.
 * Relies on the standard `aetk::core::suite` which automatically decrements the
 * host reference count via `ReleaseSuite` when it goes out of scope.
 */
class owned_footage : public footage {
public:
  /**
   * @brief Null constructor.
   *
   * @details Instantiates an uninitialized, null owned footage handle.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Creates standard safe null
   * initializers.
   *
   * @warning <b>Memory & Lifecycles:</b> None.
   */
  owned_footage() : footage() {}

  /**
   * @brief Handle constructor.
   *
   * @details Takes raw ownership of an allocated `AEGP_FootageH`.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Provides simple pointer promotion.
   *
   * @warning <b>Memory & Lifecycles:</b> Safe RAII acquisition of target
   * pointer.
   *
   * @param h Target raw footage handle.
   */
  explicit owned_footage(AEGP_FootageH h) : footage(h) {}

  // No copy allowed. Memory must be uniquely owned.
  owned_footage(const owned_footage &) = delete;
  owned_footage &operator=(const owned_footage &) = delete;

  // Move semantics
  owned_footage(owned_footage &&other) noexcept : footage(other.m_handle) {
    other.m_handle = nullptr;
  }

  owned_footage &operator=(owned_footage &&other) noexcept {
    if (this != &other) {
      free();
      this->m_handle = other.m_handle;
      other.m_handle = nullptr;
    }
    return *this;
  }

  ~owned_footage() { free(); }

  /**
   * @brief Relinquishes ownership of the handle.
   *
   * @details Releases control of the underlying `AEGP_FootageH` handle without
   * disposing it, returning the raw pointer.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Bypasses RAII deletion when
   * transferring handle control to host APIs like `AEGP_AddFootageToProject`.
   *
   * @warning <b>Memory & Lifecycles:</b> The caller or the AE host takes over
   * memory management. The internal handle is nulled.
   *
   * @return Raw `AEGP_FootageH` handle pointer.
   */
  AEGP_FootageH release() {
    AEGP_FootageH temp = this->m_handle;
    this->m_handle = nullptr;
    return temp;
  }

  // --- Static Creation Methods ---

  /**
   * @brief Creates a new solid color media block.
   *
   * @details Allocates a new solid color footage resource on the host using
   * `AEGP_NewSolidFootage`.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Accepts standard modern
   * `aetk::core::color<>` color classes and returns an RAII container.
   *
   * @warning <b>Memory & Lifecycles:</b> None.
   *
   * @param name The descriptive name of the solid.
   * @param width The pixel width of the solid block.
   * @param height The pixel height of the solid block.
   * @param color The high-fidelity color settings.
   * @return An `owned_footage` wrapper wrapping the new solid block resource.
   */
  static owned_footage create_solid(const std::string &name, int32_t width,
                                    int32_t height,
                                    const aetk::core::color<> &color) {
    AEGP_FootageH h = nullptr;
    AEGP_ColorVal raw_color = color;
    footage_suite::call<&AEGP_FootageSuite5::AEGP_NewSolidFootage>(
        name.c_str(), static_cast<A_long>(width), static_cast<A_long>(height),
        &raw_color, &h);
    return owned_footage(h);
  }

  /**
   * @brief Creates a placeholder media asset.
   *
   * @details Allocates a placeholder footage resource using
   * `AEGP_NewPlaceholderFootage`.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Wraps raw `A_Time` requirements in
   * `aetk::core::time`.
   *
   * @warning <b>Memory & Lifecycles:</b> None.
   *
   * @param name The name of the placeholder.
   * @param width The target pixel width.
   * @param height The target pixel height.
   * @param duration The timeline duration of the placeholder.
   * @return An `owned_footage` wrapper wrapping the new placeholder.
   */
  static owned_footage create_placeholder(const std::string &name,
                                          int32_t width, int32_t height,
                                          const aetk::core::time &duration) {
    AEGP_FootageH h = nullptr;
    A_Time raw_duration = duration;
    footage_suite::call<&AEGP_FootageSuite5::AEGP_NewPlaceholderFootage>(
        aetk::core::context::get_plugin_id(), name.c_str(),
        static_cast<A_long>(width), static_cast<A_long>(height), &raw_duration,
        &h);
    return owned_footage(h);
  }

  /**
   * @brief Creates a new footage asset from a file path.
   *
   * @details Allocates a new footage resource from a file path using
   * `AEGP_NewFootage`.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Accepts modern `std::string` paths and
   * returns an RAII container.
   *
   * @param path The absolute or relative file path to import.
   * @param interp_style Alpha/field guess preference.
   * @return An `owned_footage` wrapper wrapping the new footage asset.
   */
  static owned_footage
  create_from_path(const std::string &path,
                   AEGP_InterpretationStyle interp_style =
                       AEGP_InterpretationStyle_NO_DIALOG_GUESS) {
    AEGP_FootageH h = nullptr;
#ifdef AE_OS_WIN
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
    std::wstring wpath(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], size_needed);
    if (!wpath.empty() && wpath.back() == L'\0') {
        wpath.pop_back();
    }
#else
    std::wstring wpath(path.begin(), path.end());
#endif
    footage_suite::call<&AEGP_FootageSuite5::AEGP_NewFootage>(
        aetk::core::context::get_plugin_id(),
        reinterpret_cast<const A_UTF16Char *>(wpath.c_str()),
        nullptr, // layer_info
        nullptr, // sequence_options
        interp_style,
        nullptr, // reserved
        &h);
    return owned_footage(h);
  }

private:
  void free() {
    if (this->m_handle) {
      footage_traits::dispose(this->m_handle);
      this->m_handle = nullptr;
    }
  }
};

// ============================================================
//  item <-> footage implementations
// ============================================================

inline AEGP_FootageInterp item::get_footage_interpretation(bool proxy) const {
  AEGP_FootageInterp interp{};
  footage_suite::call<&AEGP_FootageSuite5::AEGP_GetFootageInterpretation>(
      m_handle, proxy ? TRUE : FALSE, &interp);
  return interp;
}

inline void item::set_footage_interpretation(const AEGP_FootageInterp &interp,
                                             bool proxy) {
  footage_suite::call<&AEGP_FootageSuite5::AEGP_SetFootageInterpretation>(
      m_handle, proxy ? TRUE : FALSE, &interp);
}

inline void item::replace_main_footage(owned_footage &&foot) {
  footage_suite::call<&AEGP_FootageSuite5::AEGP_ReplaceItemMainFootage>(
      foot.release(), m_handle);
}

inline void item::set_proxy_footage(owned_footage &&foot) {
  footage_suite::call<&AEGP_FootageSuite5::AEGP_SetItemProxyFootage>(
      foot.release(), m_handle);
}

inline item item::add_footage(owned_footage &&foot, const item &folder) {
  AEGP_ItemH h = nullptr;
  footage_suite::call<&AEGP_FootageSuite5::AEGP_AddFootageToProject>(
      foot.release(), folder.get(), &h);
  return item(h);
}

} // namespace aetk::aegp
