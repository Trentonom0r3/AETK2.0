#pragma once

#include <AE_Effect.h>
#include <aetk/core/error.hpp>
#include <aetk/effect/audio/smart_audio_world.hpp>
#include <utility>

namespace aetk::effect {

/**
 * @brief Scope-locked RAII guard for checking out layer audio via PF_CHECKOUT_LAYER_AUDIO.
 *
 * @details Automatically performs host checkout upon construction and ensures host check-in
 * (`PF_CHECKIN_LAYER_AUDIO`) on scope destruction.
 */
class layer_audio_checkout {
private:
    PF_InData* m_in_data = nullptr;
    PF_LayerAudio m_audio_handle = nullptr;
    PF_SoundWorld m_sound_world_struct{};
    smart_audio_world m_smart_audio{};

    void cleanup() noexcept {
        if (m_audio_handle && m_in_data) {
            PF_CHECKIN_LAYER_AUDIO(m_in_data, m_audio_handle);
            m_audio_handle = nullptr;
            m_in_data = nullptr;
        }
    }

public:
    ~layer_audio_checkout() {
        cleanup();
    }

    layer_audio_checkout() noexcept = default;

    /**
     * @brief Perform layer audio checkout for specified param index and time range.
     */
    layer_audio_checkout(
        PF_InData* in_data,
        PF_ParamIndex param_index,
        A_long start_time,
        A_long duration,
        A_u_long time_scale,
        A_u_long rate = 44100,
        PF_SoundSampleSize bytes_per_sample = PF_SSS_4,
        PF_SoundChannels num_channels = PF_Channels_STEREO,
        PF_SoundFormat fmt = PF_SIGNED_FLOAT)
        : m_in_data(in_data) {
        if (!m_in_data) {
            throw aetk::core::exception(PF_Err_BAD_CALLBACK_PARAM, "Null in_data provided to layer_audio_checkout");
        }

        PF_Err err = PF_CHECKOUT_LAYER_AUDIO(
            m_in_data,
            param_index,
            start_time,
            duration,
            time_scale,
            rate,
            bytes_per_sample,
            num_channels,
            fmt,
            &m_audio_handle);

        if (err != PF_Err_NONE || !m_audio_handle) {
            throw aetk::core::exception(err, "Failed to checkout layer audio");
        }

        PF_SndSamplePtr data_ptr = nullptr;
        A_long num_samples = 0;
        PF_UFixed rate_fixed = 0;
        A_long bytes_per_sample_out = 0;
        A_long num_channels_out = 0;
        A_long fmt_out = 0;

        err = PF_GET_AUDIO_DATA(
            m_in_data,
            m_audio_handle,
            &data_ptr,
            &num_samples,
            &rate_fixed,
            &bytes_per_sample_out,
            &num_channels_out,
            &fmt_out);

        if (err != PF_Err_NONE) {
            cleanup();
            throw aetk::core::exception(err, "Failed to get audio data from checked out audio handle");
        }

        // Fill SoundWorld structure
        m_sound_world_struct.fi.rateF = rate_fixed ? (static_cast<PF_FpLong>(rate_fixed) / 65536.0) : static_cast<PF_FpLong>(rate);
        m_sound_world_struct.fi.num_channels = num_channels_out ? static_cast<PF_SoundChannels>(num_channels_out) : num_channels;
        m_sound_world_struct.fi.format = fmt_out ? static_cast<PF_SoundFormat>(fmt_out) : fmt;
        m_sound_world_struct.fi.sample_size = bytes_per_sample_out ? static_cast<PF_SoundSampleSize>(bytes_per_sample_out) : bytes_per_sample;
        m_sound_world_struct.num_samples = num_samples;
        m_sound_world_struct.dataP = data_ptr;

        m_smart_audio = smart_audio_world(&m_sound_world_struct, m_in_data, smart_audio_world::ownership::NONE, m_audio_handle);
    }

    // Move-only semantics
    layer_audio_checkout(const layer_audio_checkout&) = delete;
    layer_audio_checkout& operator=(const layer_audio_checkout&) = delete;

    layer_audio_checkout(layer_audio_checkout&& other) noexcept
        : m_in_data(other.m_in_data)
        , m_audio_handle(other.m_audio_handle)
        , m_sound_world_struct(other.m_sound_world_struct)
        , m_smart_audio(std::move(other.m_smart_audio)) {
        other.m_in_data = nullptr;
        other.m_audio_handle = nullptr;
    }

    layer_audio_checkout& operator=(layer_audio_checkout&& other) noexcept {
        if (this != &other) {
            cleanup();
            m_in_data = other.m_in_data;
            m_audio_handle = other.m_audio_handle;
            m_sound_world_struct = other.m_sound_world_struct;
            m_smart_audio = std::move(other.m_smart_audio);

            other.m_in_data = nullptr;
            other.m_audio_handle = nullptr;
        }
        return *this;
    }

    /** @brief Get reference to modern smart_audio_world wrapper. */
    const smart_audio_world& sound_world() const noexcept { return m_smart_audio; }
    smart_audio_world& sound_world() noexcept { return m_smart_audio; }

    /** @brief Arrow operator to access smart_audio_world methods directly. */
    const smart_audio_world* operator->() const noexcept { return &m_smart_audio; }
    smart_audio_world* operator->() noexcept { return &m_smart_audio; }
};

} // namespace aetk::effect
