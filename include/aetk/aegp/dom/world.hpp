#pragma once

#include <AE_GeneralPlug.h>
#include <aetk/core/error.hpp>
#include <aetk/core/suite.hpp>

namespace aetk::aegp {
/*AEGP_WorldSuite3¶
Function

Purpose

AEGP_New


Returns an allocated, initialized AEGP_WorldH.



AEGP_New(
  AEGP_PluginID   plugin_id,
  AEGP_WorldType  type,
  A_long          widthL,
  A_long          heightL,
  AEGP_WorldH     *worldPH);
AEGP_Dispose


Disposes of an AEGP_WorldH. Use this on every world you allocate.



AEGP_Dispose(
  AEGP_WorldH  worldH);
AEGP_GetType








Returns the type of a given AEGP_WorldH.



AEGP_GetType(
  AEGP_WorldH worldH,
  AEGP_WorldType *typeP);


AEGP_WorldType will be one of the following:

- AEGP_WorldType_8
- AEGP_WorldType_16
- AEGP_WorldType_32
AEGP_GetSize


Returns the width and height of the given AEGP_WorldH.



AEGP_GetSize(
  AEGP_WorldH  worldH,
  A_long       *widthPL,
  A_long       *heightPL);
AEGP_GetRowBytes


Returns the rowbytes for the given AEGP_WorldH.



AEGP_GetRowBytes(
  AEGP_WorldH  worldH,
  A_u_long     *row_bytesPL);
AEGP_GetBaseAddr8




Returns the base address of the AEGP_WorldH for use in pixel iteration functions.

Will return an error if used on a non-8bpc world.



AEGP_GetBaseAddr8(
  AEGP_WorldH  worldH,
  PF_Pixel8    *base_addrP);
AEGP_GetBaseAddr16




Returns the base address of the AEGP_WorldH for use in pixel iteration functions.

Will return an error if used on a non-16bpc world.



AEGP_GetBaseAddr16(
  AEGP_WorldH  worldH,
  PF_Pixel16   *base_addrP);
AEGP_GetBaseAddr32




Returns the base address of the AEGP_WorldH for use in pixel iteration functions.

Will return an error if used on a non-32bpc world.



AEGP_GetBaseAddr32(
  AEGP_WorldH    worldH,
  PF_PixelFloat  *base_addrP);
AEGP_FillOutPFEffectWorld




Populates and returns a PF_EffectWorld representing the given AEGP_WorldH, for use with
numerous pixel processing callbacks.

NOTE: This does not give your plug-in ownership of the world referenced; destroy the
source AEGP_WorldH only if you allocated it. It just fills out the provided PF_EffectWorld
to point to the same pixel buffer.



AEGP_FillOutPFEffectWorld(
  AEGP_WorldH     worldH,
  PF_EffectWorld  *pf_worldP);
AEGP_FastBlur


Performs a fast blur on a given AEGP_WorldH.



AEGP_FastBlur(
  A_FpLong      radiusF,
  PF_ModeFlags  mode,
  PF_Quality    quality,
  AEGP_WorldH   worldH);
AEGP_NewPlatformWorld


Creates a new AEGP_PlatformWorldH (a pixel world native to the execution platform).



AEGP_NewPlatformWorld(
  AEGP_PluginID        plugin_id,
  AEGP_WorldType       type,
  A_long               widthL,
  A_long               heightL,
  AEGP_PlatformWorldH  *worldPH);
AEGP_DisposePlatformWorld


Disposes of an AEGP_PlatformWorldH.



AEGP_DisposePlatformWorld(
  AEGP_PlatformWorldH  worldH);
AEGP_NewReferenceFromPlatformWorld




Retrieves an AEGP_WorldH referring to the given AEGP_PlatformWorldH.

NOTE: This doesn't allocate a new world, it simply provides a reference to an existing
one.



AEGP_NewReferenceFromPlatformWorld(
  AEGP_PluginID        plugin_id,
  AEGP_PlatformWorldH  plat_worldH,
  AEGP_WorldH          *worldPH);
  */
/**
 * @brief RAII wrapper for AEGP_WorldH.
 */
class aegp_world {
public:
    aegp_world() = default;

    aegp_world(AEGP_WorldH h, bool owned)
        : m_handle(h)
        , m_owned(owned) {
    }

    ~aegp_world() {
        if (m_handle && m_owned) {

                aetk::core::suite<AEGP_WorldSuite3> suite;
                suite->AEGP_Dispose(m_handle);

        }
    }

    // Move-only semantics
    aegp_world(const aegp_world&) = delete;
    aegp_world& operator=(const aegp_world&) = delete;

    aegp_world(aegp_world&& other) noexcept
        : m_handle(other.m_handle)
        , m_owned(other.m_owned) {
        other.m_handle = nullptr;
        other.m_owned = false;
    }

    aegp_world& operator=(aegp_world&& other) noexcept {
        if (this != &other) {
            if (m_handle && m_owned) {
                    aetk::core::suite<AEGP_WorldSuite3> suite;
                    suite->AEGP_Dispose(m_handle);

            }
            m_handle = other.m_handle;
            m_owned = other.m_owned;
            other.m_handle = nullptr;
            other.m_owned = false;
        }
        return *this;
    }

    AEGP_WorldH get() const {
        return m_handle;
    }
    explicit operator bool() const {
        return m_handle != nullptr;
    }
    static aegp_world create(AEGP_WorldType type, A_long width, A_long height, AEGP_PluginID plugin_id = 0, SPBasicSuite* pica = nullptr) {
        AEGP_WorldH handle = nullptr;
        SPBasicSuite* active_pica = pica ? pica : aetk::core::context::get_basic_suite();
        aetk::core::suite<AEGP_WorldSuite3> suite(active_pica);
        aetk::core::check_err(suite->AEGP_New(plugin_id, type, width, height, &handle), "AEGP_New failed");
        return aegp_world(handle, true);
    }

    int32_t width() const {
        if (!m_handle)
            return 0;
        A_long w = 0, h = 0;
        aetk::core::suite<AEGP_WorldSuite3>::call<&AEGP_WorldSuite3::AEGP_GetSize>(
            m_handle, &w, &h);
        return static_cast<int32_t>(w);
    }

    int32_t height() const {
        if (!m_handle)
            return 0;
        A_long w = 0, h = 0;
        aetk::core::suite<AEGP_WorldSuite3>::call<&AEGP_WorldSuite3::AEGP_GetSize>(
            m_handle, &w, &h);
        return static_cast<int32_t>(h);
    }

    uint32_t row_bytes() const {
        if (!m_handle)
            return 0;
        A_u_long rb = 0;
        aetk::core::suite<AEGP_WorldSuite3>::call<&AEGP_WorldSuite3::AEGP_GetRowBytes>(
            m_handle, &rb);
        return static_cast<uint32_t>(rb);
    }

private:
    AEGP_WorldH m_handle = nullptr;
    bool m_owned = false;
};

/**
 * @brief RAII wrapper for AEGP_FrameReceiptH.
 */
class frame_receipt {
public:
    frame_receipt() = default;

    explicit frame_receipt(AEGP_FrameReceiptH receipt)
        : m_receipt(receipt) {
    }

    ~frame_receipt() {
        if (m_receipt) {

                aetk::core::suite<AEGP_RenderSuite5> suite;
                suite->AEGP_CheckinFrame(m_receipt);

        }
    }

    // Move-only semantics
    frame_receipt(const frame_receipt&) = delete;
    frame_receipt& operator=(const frame_receipt&) = delete;

    frame_receipt(frame_receipt&& other) noexcept
        : m_receipt(other.m_receipt) {
        other.m_receipt = nullptr;
    }

    frame_receipt& operator=(frame_receipt&& other) noexcept {
        if (this != &other) {
            if (m_receipt) {

                    aetk::core::suite<AEGP_RenderSuite5> suite;
                    suite->AEGP_CheckinFrame(m_receipt);

            }
            m_receipt = other.m_receipt;
            other.m_receipt = nullptr;
        }
        return *this;
    }

    AEGP_FrameReceiptH get() const {
        return m_receipt;
    }
    explicit operator bool() const {
        return m_receipt != nullptr;
    }

    aegp_world get_world() const {
        if (!m_receipt)
            return aegp_world { };
        AEGP_WorldH w = nullptr;
        aetk::core::suite<AEGP_RenderSuite5>::call<
            &AEGP_RenderSuite5::AEGP_GetReceiptWorld>(m_receipt, &w);
        return aegp_world { w,
            false }; // Non-owned view (checked in when receipt is destroyed)
    }

private:
    AEGP_FrameReceiptH m_receipt = nullptr;
};

} // namespace aetk::aegp
