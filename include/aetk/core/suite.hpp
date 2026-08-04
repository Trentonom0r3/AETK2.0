#pragma once

#include <AE_AdvEffectSuites.h>
#include <AE_ComputeCacheSuite.h>
#include <AE_Effect.h>
#include <AE_EffectCBSuites.h>
#include <AE_EffectPixelFormat.h>
#include <AE_EffectGPUSuites.h>
#include <AE_EffectSuites.h>
#include <AE_GeneralPlug.h>
#include <PrSDKAESupport.h>
#include <PrSDKPixelFormat.h>
#include <adobesdk/DrawbotSuite.h>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

#include <aetk/core/context.hpp>
#include <aetk/core/error.hpp>

namespace aetk::core {

/**
 * @brief Compile-time string for template parameters.
 *
 * @details Represents a fixed-size char array that is constexpr-compatible.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Provides a compile-time string container
 * so that string literals can be passed as template arguments, which is key for
 * declarative, zero-cost suite checkouts.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 *
 * @tparam N The length of the string literal.
 */
template <size_t N> struct fixed_string {
  /// Internal constexpr char array.
  char data[N]{};

  /**
   * @brief Compile-time literal constructor.
   *
   * @details Binds a static array of characters.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Safe compile-time extraction.
   *
   * @warning <b>Memory & Lifecycles:</b> None.
   *
   * @param str Literal reference array.
   */
  constexpr fixed_string(const char (&str)[N]) {
    for (size_t i = 0; i < N; ++i)
      data[i] = str[i];
  }

  /**
   * @brief Conversion operator to raw const char*.
   *
   * @details Exposes the inner data address array.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Seamless implicit casting.
   *
   * @warning <b>Memory & Lifecycles:</b> None.
   */
  constexpr operator const char *() const { return data; }
};

template <size_t N> fixed_string(const char (&)[N]) -> fixed_string<N>;

template <typename T> struct suite_premiere_support {
  static constexpr bool value = true;
};

// AEGP/Drawbot/Custom UI/Sampling/Color specializations (unsupported on
// Premiere Pro)
template <> struct suite_premiere_support<AEGP_MemorySuite1> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<AEGP_ComputeCacheSuite1> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<AEGP_UtilitySuite3> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<AEGP_KeyframeSuite4> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<AEGP_MaskSuite6> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<AEGP_MaskOutlineSuite3> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<AEGP_PFInterfaceSuite1> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<AEGP_StreamSuite2> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<AEGP_DynamicStreamSuite4> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<AEGP_EffectSuite4> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<AEGP_RenderSuite5> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<AEGP_LayerRenderOptionsSuite2> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<AEGP_WorldSuite3> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<DRAWBOT_DrawbotSuite1> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<DRAWBOT_SupplierSuite1> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<DRAWBOT_SurfaceSuite2> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<DRAWBOT_PathSuite1> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<DRAWBOT_PenSuite1> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<PF_EffectCustomUIOverlayThemeSuite1> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<PF_Sampling8Suite1> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<PF_Sampling16Suite1> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<PF_SamplingFloatSuite1> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<PF_ColorCallbacksSuite1> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<PF_ColorCallbacks16Suite1> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<PF_ColorCallbacksFloatSuite1> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<PF_Iterate16Suite1> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<PF_Iterate16Suite2> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<PF_IterateFloatSuite1> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<PF_IterateFloatSuite2> {
  static constexpr bool value = false;
};
template <> struct suite_premiere_support<PF_WorldTransformSuite1> {
  static constexpr bool value = false;
};


/**
 * @brief RAII and Static Call wrapper for After Effects suites.
 *
 * @details Manages checked-out host suites using either scoped RAII wrappers or
 * zero-allocation static method calls.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, acquiring suites requires
 * manually calling `AcquireSuite` on `SPBasicSuite`, explicitly checking for
 * non-zero HRESULT error codes, casting returned `void**` values, and balancing
 * every checkout with `ReleaseSuite`. `aetk::core::suite` automates this entire
 * process: it either manages lifetimes through standard RAII (acquires on
 * construction, releases on destruction) or allows declarative, zero-allocation
 * static execution via `suite<T>::call<&Suite::Method>(...)` which handles
 * checking and releases the suite immediately.
 *
 * @warning <b>Memory & Lifecycles:</b> Double check that dynamic instances do
 * not outlive the `SPBasicSuite` lifetime. Premiere Pro compatibility rules
 * statically block AE-exclusive suites (`AEGP_PFInterfaceSuite1`,
 * `DRAWBOT_DrawbotSuite1`, etc.) at compile time if `AETK_PREMIERE_COMPAT` is
 * defined. Every checkout is balanced on scope exit.
 *
 * @tparam T The exact C-struct interface of the suite.
 * @param Name Optional unique global suite identifier string literal.
 * @param Version Optional major version index.
 */
template <typename T, auto Name = fixed_string(""), int Version = 0>
class suite {
public:
  /**
   * @brief Retrieve the global unique suite string name.
   *
   * @details Evaluates traits or template arguments at compile-time to return
   * the identifier.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Zero-overhead compile-time name
   * resolution.
   *
   * @warning <b>Memory & Lifecycles:</b> None.
   *
   * @return Raw static string identifier.
   */
  static constexpr const char *get_name() {
    if constexpr (!std::is_same_v<decltype(Name), fixed_string<1>>) {
      return Name.data;
    } else {
      if constexpr (std::is_same_v<T, PF_AdvAppSuite2>)
        return kPFAdvAppSuite;
      else if constexpr (std::is_same_v<T, PF_Iterate8Suite2> ||
                         std::is_same_v<T, PF_Iterate8Suite1>)
        return kPFIterate8Suite;
      else if constexpr (std::is_same_v<T, PF_IterateFloatSuite2> ||
                         std::is_same_v<T, PF_IterateFloatSuite1>)
        return kPFIterateFloatSuite;
      else if constexpr (std::is_same_v<T, PF_Iterate16Suite2> ||
                         std::is_same_v<T, PF_Iterate16Suite1>)
        return kPFIterate16Suite;
      else if constexpr (std::is_same_v<T, PF_PixelFormatSuite1>)
        return kPFPixelFormatSuite;
      else if constexpr (std::is_same_v<T, PF_WorldSuite2>)
        return kPFWorldSuite;
      else if constexpr (std::is_same_v<T, PF_WorldTransformSuite1>)
        return kPFWorldTransformSuite;
      else if constexpr (std::is_same_v<T, PF_GPUDeviceSuite1>)
        return kPFGPUDeviceSuite;
      else if constexpr (std::is_same_v<T, AEGP_MemorySuite1>)
        return kAEGPMemorySuite;
      else if constexpr (std::is_same_v<T, AEGP_ComputeCacheSuite1>)
        return kAEGPComputeCacheSuite;
      else if constexpr (std::is_same_v<T, AEGP_UtilitySuite3>)
        return "AEGP Utility Suite";
      else if constexpr (std::is_same_v<T, AEGP_KeyframeSuite4>)
        return kAEGPKeyframeSuite;
      else if constexpr (std::is_same_v<T, AEGP_MaskSuite6>)
        return kAEGPMaskSuite;
      else if constexpr (std::is_same_v<T, AEGP_MaskOutlineSuite3>)
        return kAEGPMaskOutlineSuite;
      else if constexpr (std::is_same_v<T, AEGP_PFInterfaceSuite1>)
        return "AEGP PF Interface Suite";
      else if constexpr (std::is_same_v<T, AEGP_StreamSuite2>)
        return "AEGP Stream Suite";
      else if constexpr (std::is_same_v<T, AEGP_DynamicStreamSuite4>)
        return "AEGP Dynamic Stream Suite";
      else if constexpr (std::is_same_v<T, AEGP_EffectSuite4>)
        return "AEGP Effect Suite";
      else if constexpr (std::is_same_v<T, AEGP_RenderSuite5>)
        return kAEGPRenderSuite;
      else if constexpr (std::is_same_v<T, AEGP_LayerRenderOptionsSuite2>)
        return kAEGPLayerRenderOptionsSuite;
      else if constexpr (std::is_same_v<T, AEGP_WorldSuite3>)
        return kAEGPWorldSuite;
      else if constexpr (std::is_same_v<T, PF_ParamUtilsSuite3>)
        return "PF Param Utils Suite";
      else if constexpr (std::is_same_v<T, DRAWBOT_DrawbotSuite1>)
        return kDRAWBOT_DrawSuite;
      else if constexpr (std::is_same_v<T, DRAWBOT_SupplierSuite1>)
        return kDRAWBOT_SupplierSuite;
      else if constexpr (std::is_same_v<T, DRAWBOT_SurfaceSuite2>)
        return kDRAWBOT_SurfaceSuite;
      else if constexpr (std::is_same_v<T, DRAWBOT_PathSuite1>)
        return kDRAWBOT_PathSuite;
      else if constexpr (std::is_same_v<T, DRAWBOT_PenSuite1>)
        return kDRAWBOT_PenSuite;
      else if constexpr (std::is_same_v<T, PF_EffectCustomUIOverlayThemeSuite1>)
        return kPFEffectCustomUIOverlayThemeSuite;
      else if constexpr (std::is_same_v<T, PF_Sampling8Suite1>)
        return kPFSampling8Suite;
      else if constexpr (std::is_same_v<T, PF_Sampling16Suite1>)
        return kPFSampling16Suite;
      else if constexpr (std::is_same_v<T, PF_SamplingFloatSuite1>)
        return kPFSamplingFloatSuite;
      else if constexpr (std::is_same_v<T, PF_ColorCallbacksSuite1>)
        return kPFColorCallbacksSuite;
      else if constexpr (std::is_same_v<T, PF_ColorCallbacks16Suite1>)
        return kPFColorCallbacks16Suite;
      else if constexpr (std::is_same_v<T, PF_ColorCallbacksFloatSuite1>)
        return kPFColorCallbacksFloatSuite;
      else if constexpr (std::is_same_v<T, PF_FillMatteSuite2>)
        return kPFFillMatteSuite;
      else
        return "UnknownSuite";
    }
  }

  /**
   * @brief Retrieve the global suite version number.
   *
   * @details Resolves version constants at compile-time.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Zero-overhead compile-time version
   * resolution.
   *
   * @warning <b>Memory & Lifecycles:</b> None.
   *
   * @return Version number index.
   */
  static constexpr int get_version() {
    if constexpr (!std::is_same_v<decltype(Name), fixed_string<1>>) {
      return Version;
    } else {
      if constexpr (std::is_same_v<T, PF_AdvAppSuite2>)
        return kPFAdvAppSuiteVersion2;
      else if constexpr (std::is_same_v<T, PF_Iterate8Suite2>)
        return kPFIterate8SuiteVersion2;
      else if constexpr (std::is_same_v<T, PF_Iterate8Suite1>)
        return kPFIterate8SuiteVersion1;
      else if constexpr (std::is_same_v<T, PF_IterateFloatSuite2>)
        return kPFIterateFloatSuiteVersion2;
      else if constexpr (std::is_same_v<T, PF_IterateFloatSuite1>)
        return kPFIterateFloatSuiteVersion1;
      else if constexpr (std::is_same_v<T, PF_Iterate16Suite2>)
        return kPFIterate16SuiteVersion2;
      else if constexpr (std::is_same_v<T, PF_Iterate16Suite1>)
        return kPFIterate16SuiteVersion1;
      else if constexpr (std::is_same_v<T, PF_PixelFormatSuite1>)
        return kPFPixelFormatSuiteVersion1;
      else if constexpr (std::is_same_v<T, PF_WorldSuite2>)
        return kPFWorldSuiteVersion2;
      else if constexpr (std::is_same_v<T, PF_WorldTransformSuite1>)
        return kPFWorldTransformSuiteVersion1;
      else if constexpr (std::is_same_v<T, PF_GPUDeviceSuite1>)
        return kPFGPUDeviceSuiteVersion1;
      else if constexpr (std::is_same_v<T, AEGP_MemorySuite1>)
        return kAEGPMemorySuiteVersion1;
      else if constexpr (std::is_same_v<T, AEGP_ComputeCacheSuite1>)
        return kAEGPComputeCacheSuiteVersion1;
      else if constexpr (std::is_same_v<T, AEGP_UtilitySuite3>)
        return 7;
      else if constexpr (std::is_same_v<T, AEGP_KeyframeSuite4>)
        return kAEGPKeyframeSuiteVersion4;
      else if constexpr (std::is_same_v<T, AEGP_MaskSuite6>)
        return kAEGPMaskSuiteVersion6;
      else if constexpr (std::is_same_v<T, AEGP_MaskOutlineSuite3>)
        return kAEGPMaskOutlineSuiteVersion3;
      else if constexpr (std::is_same_v<T, AEGP_PFInterfaceSuite1>)
        return 1;
      else if constexpr (std::is_same_v<T, AEGP_StreamSuite2>)
        return 7;
      else if constexpr (std::is_same_v<T, AEGP_DynamicStreamSuite4>)
        return 5; // kAEGPDynamicStreamSuiteVersion4 is 5
      else if constexpr (std::is_same_v<T, AEGP_EffectSuite4>)
        return 4;
      else if constexpr (std::is_same_v<T, AEGP_RenderSuite5>)
        return kAEGPRenderSuiteVersion5;
      else if constexpr (std::is_same_v<T, AEGP_LayerRenderOptionsSuite2>)
        return kAEGPLayerRenderOptionsSuiteVersion2;
      else if constexpr (std::is_same_v<T, AEGP_WorldSuite3>)
        return kAEGPWorldSuiteVersion3;
      else if constexpr (std::is_same_v<T, PF_ParamUtilsSuite3>)
        return 3;
      else if constexpr (std::is_same_v<T, DRAWBOT_DrawbotSuite1>)
        return kDRAWBOT_DrawSuite_Version1;
      else if constexpr (std::is_same_v<T, DRAWBOT_SupplierSuite1>)
        return kDRAWBOT_SupplierSuite_Version1;
      else if constexpr (std::is_same_v<T, DRAWBOT_SurfaceSuite2>)
        return kDRAWBOT_SurfaceSuite_Version2;
      else if constexpr (std::is_same_v<T, DRAWBOT_PathSuite1>)
        return kDRAWBOT_PathSuite_Version1;
      else if constexpr (std::is_same_v<T, DRAWBOT_PenSuite1>)
        return kDRAWBOT_PenSuite_Version1;
      else if constexpr (std::is_same_v<T, PF_EffectCustomUIOverlayThemeSuite1>)
        return kPFEffectCustomUIOverlayThemeSuiteVersion1;
      else if constexpr (std::is_same_v<T, PF_Sampling8Suite1>)
        return kPFSampling8SuiteVersion1;
      else if constexpr (std::is_same_v<T, PF_Sampling16Suite1>)
        return kPFSampling16SuiteVersion1;
      else if constexpr (std::is_same_v<T, PF_SamplingFloatSuite1>)
        return kPFSamplingFloatSuiteVersion1;
      else if constexpr (std::is_same_v<T, PF_ColorCallbacksSuite1>)
        return kPFColorCallbacksSuiteVersion1;
      else if constexpr (std::is_same_v<T, PF_ColorCallbacks16Suite1>)
        return kPFColorCallbacks16SuiteVersion1;
      else if constexpr (std::is_same_v<T, PF_ColorCallbacksFloatSuite1>)
        return kPFColorCallbacksFloatSuiteVersion1;
      else if constexpr (std::is_same_v<T, PF_FillMatteSuite2>)
        return kPFFillMatteSuiteVersion2;
      else
        return 0;
    }
  }

  /**
   * @brief RAII suite checkout constructor.
   *
   * @details Queries `SPBasicSuite` to acquire the requested API pointer,
   * throwing if compatible versions aren't supported by the host.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Automates standard `AcquireSuite` with
   * C++ exception checking.
   *
   * @warning <b>Memory & Lifecycles:</b> Safe RAII acquisition of target PICA
   * suite.
   *
   * @param pica Optional basic suite pointer (resolves to global context if
   * null).
   */
  static constexpr bool supports_premiere() {
    return suite_premiere_support<T>::value;
  }

  explicit suite(SPBasicSuite *pica = nullptr)
      : suite(pica, get_name(), get_version()) {
#ifdef AETK_PREMIERE_COMPAT
    static_assert(supports_premiere(),
                  "AETK Error: This suite is an After Effects "
                  "exclusive and incompatible with Premiere Pro.");
#endif
  }

  /**
   * @brief Parameterized RAII checkout constructor.
   *
   * @details Allows checking out dynamic, non-standard or version-override
   * suites.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Dynamic version override mapping.
   *
   * @warning <b>Memory & Lifecycles:</b> Safe RAII acquisition.
   *
   * @param pica Raw basic suite pointer.
   * @param name Specific string name identifier of the suite.
   * @param version Specific version number index.
   */
  suite(SPBasicSuite *pica, const char *name, int version) {
    m_pica = pica ? pica : context::get_basic_suite();
    m_name = name;
    m_version = version;

    // Check for stale/uninitialized Basic Suite pointer
    uintptr_t pica_val = reinterpret_cast<uintptr_t>(m_pica);
    if (pica_val == 0xababababababababULL ||
        pica_val == 0xccccccccccccccccULL ||
        pica_val == 0xfdfdfdfdfdfdfdfdULL ||
        pica_val == 0xddddddddddddddddULL ||
        pica_val == 0xfeeefeeefeeefeeeULL || pica_val == 0) {
      throw exception(PF_Err_OUT_OF_MEMORY,
                      "Failed to resolve SPBasicSuite (pointer is stale or "
                      "uninitialized) for suite: " +
                          std::string(m_name));
    }

    // Runtime Premiere Pro compatibility guard
    if (context::is_premiere() && !supports_premiere()) {
      throw exception(PF_Err_OUT_OF_MEMORY,
                      "AETK Error: Suite " + std::string(m_name) +
                          " is unsupported under Premiere Pro.");
    }

    PF_Err err = m_pica->AcquireSuite(m_name, m_version, (const void **)&m_ptr);
    check_err(err, std::string("Failed to acquire suite RAII: ") + m_name);
  }

  /**
   * @brief RAII suite release.
   *
   * @details Decrements the host-side reference count via `ReleaseSuite`.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Guarantees automated `ReleaseSuite`
   * cleanup on scope exit.
   *
   * @warning <b>Memory & Lifecycles:</b> None.
   */
  ~suite() {
    if (m_pica && m_ptr)
      m_pica->ReleaseSuite(m_name, m_version);
  }

  // No copy allowed.
  suite(const suite &) = delete;
  suite &operator=(const suite &) = delete;

  // Move allowed.
  suite(suite &&other) noexcept
      : m_pica(other.m_pica), m_ptr(other.m_ptr), m_name(other.m_name),
        m_version(other.m_version) {
    other.m_ptr = nullptr;
    other.m_pica = nullptr;
  }

  suite &operator=(suite &&other) noexcept {
    if (this != &other) {
      if (m_pica && m_ptr)
        m_pica->ReleaseSuite(m_name, m_version);
      m_pica = other.m_pica;
      m_ptr = other.m_ptr;
      m_name = other.m_name;
      m_version = other.m_version;
      other.m_ptr = nullptr;
      other.m_pica = nullptr;
    }
    return *this;
  }

  /**
   * @brief Direct suite member dereference.
   *
   * @details Returns the typed suite struct pointer.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Smart pointer syntax access.
   *
   * @warning <b>Memory & Lifecycles:</b> None.
   *
   * @return Typed suite struct pointer.
   */
  T *operator->() const {
#ifndef NDEBUG
    assert(
        m_ptr != nullptr &&
        "AETK Debug Error: Dereferencing a NULL or moved-from suite pointer");
#endif
    return m_ptr;
  }

  /**
   * @brief Direct pointer access.
   *
   * @details Exposes raw address.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Raw pointer extraction.
   *
   * @warning <b>Memory & Lifecycles:</b> None.
   *
   * @return Typed raw pointer.
   */
  T *ptr() const {
#ifndef NDEBUG
    assert(m_ptr != nullptr &&
           "AETK Debug Error: Accessing a NULL or moved-from suite pointer");
#endif
    return m_ptr;
  }

  /**
   * @brief Manual static suite acquisition.
   *
   * @details Acquires suite temporarily.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Replaces raw `AcquireSuite` calls.
   *
   * @warning <b>Memory & Lifecycles:</b> The caller is responsible for
   * balancing this checkout with `ReleaseSuite`!
   *
   * @return Checked-out suite struct pointer.
   */
  static T *get() {
#ifdef AETK_PREMIERE_COMPAT
    static_assert(supports_premiere(),
                  "AETK Error: This suite is an After Effects "
                  "exclusive and incompatible with Premiere Pro.");
#endif

    if (context::is_premiere() && !supports_premiere()) {
      throw exception(PF_Err_OUT_OF_MEMORY,
                      "AETK Error: Suite " + std::string(get_name()) +
                          " is unsupported under Premiere Pro.");
    }

    T *res = nullptr;
    SPBasicSuite *pica = context::get_basic_suite();
    PF_Err err =
        pica->AcquireSuite(get_name(), get_version(), (const void **)&res);
    check_err(err,
              std::string("Failed to acquire static suite: ") + get_name());
    return res;
  }

  /**
   * @brief Zero-allocation static suite call helper.
   *
   * @details Checks out the target suite, executes the designated procedure,
   * releases the suite immediately, and verifies status return codes.
   *
   * @note <b>AE SDK Paradigm Shift:</b> Replaces bulky stack-allocated suite
   * handlers with elegant, zero-allocation inline transactions.
   *
   * @warning <b>Memory & Lifecycles:</b> Automatically acquires and releases
   * the suite inside the function body, ensuring a leak-free inline checkout.
   *
   * @tparam Method Member function pointer to invoke.
   * @param args Parameter pack forwarded directly to the suite procedure.
   * @return The standard procedure output value.
   */
  template <auto Method, typename... Args> static auto call(Args &&...args) {
#ifdef AETK_PREMIERE_COMPAT
    static_assert(supports_premiere(),
                  "AETK Error: This suite is an After Effects "
                  "exclusive and incompatible with Premiere Pro.");
#endif
    T *s = get();
    auto result = (s->*Method)(std::forward<Args>(args)...);
    context::get_basic_suite()->ReleaseSuite(get_name(), get_version());
    if constexpr (std::is_integral_v<decltype(result)>) {
      check_err(static_cast<PF_Err>(result),
                std::string("Error in suite: ") + get_name());
    }
    return result;
  }

private:
  SPBasicSuite *m_pica = nullptr;
  T *m_ptr = nullptr;
  const char *m_name = nullptr;
  int m_version = 0;
};

} // namespace aetk::core
