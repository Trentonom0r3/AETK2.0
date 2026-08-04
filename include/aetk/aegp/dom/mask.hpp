#pragma once

#include <aetk/aegp/dom/stream.hpp>
#include <aetk/core/handle.hpp>
#include <aetk/core/suite.hpp>
#include <aetk/core/types.hpp>


namespace aetk::aegp {

/**
 * @brief Traits helper for layer masks.
 */
struct mask_traits {
  using type = AEGP_MaskRefH;
  static void dispose(AEGP_MaskRefH h) {
    using suite = aetk::core::suite<AEGP_MaskSuite6,
                                    aetk::core::fixed_string(kAEGPMaskSuite),
                                    kAEGPMaskSuiteVersion6>;
    suite::call<&AEGP_MaskSuite6::AEGP_DisposeMask>(h);
  }
};

using mask_suite =
    aetk::core::suite<AEGP_MaskSuite6, aetk::core::fixed_string(kAEGPMaskSuite),
                      kAEGPMaskSuiteVersion6>;

/**
 * @brief Represents a borrowed timeline mask channel handle inside After
 * Effects.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, accessing mask parameters
 * involves manual calls to `AEGP_MaskSuite6` passing raw opaque handles.
 * `aetk::aegp::mask` wraps these operations into type-safe modern C++
 * properties and manages streams automatically.
 */
class mask : public aetk::core::borrowed<mask_traits> {
public:
  using borrowed::borrowed;

  // --- Properties ---

  PF_MaskMode get_mode() const {
    PF_MaskMode mode = PF_MaskMode_NONE;
    mask_suite::call<&AEGP_MaskSuite6::AEGP_GetMaskMode>(m_handle, &mode);
    return mode;
  }

  void set_mode(PF_MaskMode mode) const {
    mask_suite::call<&AEGP_MaskSuite6::AEGP_SetMaskMode>(m_handle, mode);
  }

  bool get_invert() const {
    A_Boolean inv = FALSE;
    mask_suite::call<&AEGP_MaskSuite6::AEGP_GetMaskInvert>(m_handle, &inv);
    return inv != FALSE;
  }

  void set_invert(bool invert) const {
    mask_suite::call<&AEGP_MaskSuite6::AEGP_SetMaskInvert>(
        m_handle, invert ? TRUE : FALSE);
  }

  AEGP_MaskMBlur get_motion_blur_state() const {
    AEGP_MaskMBlur state = AEGP_MaskMBlur_SAME_AS_LAYER;
    mask_suite::call<&AEGP_MaskSuite6::AEGP_GetMaskMotionBlurState>(m_handle,
                                                                    &state);
    return state;
  }

  void set_motion_blur_state(AEGP_MaskMBlur state) const {
    mask_suite::call<&AEGP_MaskSuite6::AEGP_SetMaskMotionBlurState>(m_handle,
                                                                    state);
  }

  AEGP_MaskFeatherFalloff get_feather_falloff() const {
    AEGP_MaskFeatherFalloff falloff = AEGP_MaskFeatherFalloff_SMOOTH;
    mask_suite::call<&AEGP_MaskSuite6::AEGP_GetMaskFeatherFalloff>(m_handle,
                                                                   &falloff);
    return falloff;
  }

  void set_feather_falloff(AEGP_MaskFeatherFalloff falloff) const {
    mask_suite::call<&AEGP_MaskSuite6::AEGP_SetMaskFeatherFalloff>(m_handle,
                                                                   falloff);
  }

  int32_t get_id() const {
    AEGP_MaskIDVal id = 0;
    mask_suite::call<&AEGP_MaskSuite6::AEGP_GetMaskID>(m_handle, &id);
    return static_cast<int32_t>(id);
  }

  aetk::core::color<> get_color() const {
    AEGP_ColorVal c{};
    mask_suite::call<&AEGP_MaskSuite6::AEGP_GetMaskColor>(m_handle, &c);
    return aetk::core::color<>(c);
  }

  void set_color(const aetk::core::color<> &color) const {
    AEGP_ColorVal c = color;
    mask_suite::call<&AEGP_MaskSuite6::AEGP_SetMaskColor>(m_handle, &c);
  }

  bool get_lock_state() const {
    A_Boolean lock = FALSE;
    mask_suite::call<&AEGP_MaskSuite6::AEGP_GetMaskLockState>(m_handle, &lock);
    return lock != FALSE;
  }

  void set_lock_state(bool lock) const {
    mask_suite::call<&AEGP_MaskSuite6::AEGP_SetMaskLockState>(
        m_handle, lock ? TRUE : FALSE);
  }

  bool is_roto_bezier() const {
    A_Boolean roto = FALSE;
    mask_suite::call<&AEGP_MaskSuite6::AEGP_GetMaskIsRotoBezier>(m_handle,
                                                                 &roto);
    return roto != FALSE;
  }

  void set_is_roto_bezier(bool roto) const {
    mask_suite::call<&AEGP_MaskSuite6::AEGP_SetMaskIsRotoBezier>(
        m_handle, roto ? TRUE : FALSE);
  }

  // --- Streams ---

  owned_stream get_stream(AEGP_MaskStream stream_type) const {
    using stream_suite =
        aetk::core::suite<AEGP_StreamSuite6,
                          aetk::core::fixed_string(kAEGPStreamSuite),
                          kAEGPStreamSuiteVersion6>;
    AEGP_StreamRefH streamH = nullptr;
    stream_suite::call<&AEGP_StreamSuite6::AEGP_GetNewMaskStream>(
        aetk::core::context::get_plugin_id(), m_handle, stream_type, &streamH);
    return owned_stream(streamH);
  }

  owned_stream outline() const { return get_stream(AEGP_MaskStream_OUTLINE); }
  owned_stream opacity() const { return get_stream(AEGP_MaskStream_OPACITY); }
  owned_stream feather() const { return get_stream(AEGP_MaskStream_FEATHER); }
  owned_stream expansion() const {
    return get_stream(AEGP_MaskStream_EXPANSION);
  }
};

/**
 * @brief Represents an allocated, owned mask handle that disposes itself
 * automatically.
 */
class owned_mask : public mask {
public:
  owned_mask() : mask(nullptr) {}
  explicit owned_mask(AEGP_MaskRefH h) : mask(h) {}

  owned_mask(const owned_mask &) = delete;
  owned_mask &operator=(const owned_mask &) = delete;

  owned_mask(owned_mask &&other) noexcept : mask(other.m_handle) {
    other.m_handle = nullptr;
  }

  owned_mask &operator=(owned_mask &&other) noexcept {
    if (this != &other) {
      free();
      this->m_handle = other.m_handle;
      other.m_handle = nullptr;
    }
    return *this;
  }

  ~owned_mask() { free(); }

  AEGP_MaskRefH release() {
    AEGP_MaskRefH temp = this->m_handle;
    this->m_handle = nullptr;
    return temp;
  }

private:
  void free() {
    if (this->m_handle) {
      mask_traits::dispose(this->m_handle);
      this->m_handle = nullptr;
    }
  }
};

} // namespace aetk::aegp
