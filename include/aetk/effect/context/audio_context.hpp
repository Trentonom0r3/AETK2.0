#pragma once

#include <AE_Effect.h>
#include <aetk/core/error.hpp>
#include <aetk/effect/audio/layer_audio_checkout.hpp>
#include <aetk/effect/audio/smart_audio_world.hpp>
#include <cstdint>

namespace aetk::effect {

/**
 * @brief Context wrapper for PF_Cmd_AUDIO_SETUP.
 *
 * @details Delivered to plugin during audio setup to configure sample bounds or tail expansion.
 */
class audio_setup_context {
private:
    PF_InData* m_in_data = nullptr;
    PF_OutData* m_out_data = nullptr;
    PF_ParamDef** m_params = nullptr;

public:
    audio_setup_context(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[])
        : m_in_data(in_data)
        , m_out_data(out_data)
        , m_params(params) {
        if (m_in_data && m_out_data) {
            m_out_data->start_sampL = m_in_data->start_sampL;
            m_out_data->dur_sampL = m_in_data->dur_sampL;
        }
    }

    PF_InData* in_data() const noexcept { return m_in_data; }
    PF_OutData* out_data() const noexcept { return m_out_data; }

    /** @brief Get requested input start sample index. */
    A_long start_sample() const noexcept { return m_in_data ? m_in_data->start_sampL : 0; }

    /** @brief Get requested input duration in samples. */
    A_long duration_samples() const noexcept { return m_in_data ? m_in_data->dur_sampL : 0; }

    /** @brief Get total sample duration of the layer. */
    A_long total_samples() const noexcept { return m_in_data ? m_in_data->total_sampL : 0; }

    /** @brief Set output start sample index. */
    void set_output_start_sample(A_long start) noexcept {
        if (m_out_data) m_out_data->start_sampL = start;
    }

    /** @brief Set output duration in samples (e.g. expanding for reverb/delay tails). */
    void set_output_duration_samples(A_long dur) noexcept {
        if (m_out_data) m_out_data->dur_sampL = dur;
    }
};

/**
 * @brief Context wrapper for PF_Cmd_AUDIO_RENDER.
 *
 * @details Provides type-safe access to incoming audio (`src_snd`), output audio destination
 * (`dest_snd`), parameters, and layer audio checkouts.
 */
class audio_render_context {
private:
    PF_InData* m_in_data = nullptr;
    PF_OutData* m_out_data = nullptr;
    PF_ParamDef** m_params = nullptr;
    PF_LayerDef* m_output = nullptr;

public:
    audio_render_context(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output)
        : m_in_data(in_data)
        , m_out_data(out_data)
        , m_params(params)
        , m_output(output) {}

    PF_InData* in_data() const noexcept { return m_in_data; }
    PF_OutData* out_data() const noexcept { return m_out_data; }

    /** @brief Get input audio sound world view (src_snd). */
    smart_audio_world input_sound() const noexcept {
        return smart_audio_world(m_in_data ? const_cast<PF_SoundWorld*>(&m_in_data->src_snd) : nullptr,
                                 m_in_data,
                                 smart_audio_world::ownership::NONE);
    }

    /** @brief Get output audio sound world view (dest_snd). */
    smart_audio_world output_sound() const noexcept {
        return smart_audio_world(m_out_data ? &m_out_data->dest_snd : nullptr,
                                 m_in_data,
                                 smart_audio_world::ownership::NONE);
    }

    /** @brief Get start sample index for this audio render pass. */
    A_long start_sample() const noexcept { return m_in_data ? m_in_data->start_sampL : 0; }

    /** @brief Get sample count for this audio render pass. */
    A_long duration_samples() const noexcept { return m_in_data ? m_in_data->dur_sampL : 0; }

    /** @brief Get total samples in layer timeline. */
    A_long total_samples() const noexcept { return m_in_data ? m_in_data->total_sampL : 0; }

    /**
     * @brief Perform an RAII checkout of audio from a layer parameter across a time window.
     */
    layer_audio_checkout checkout_layer_audio(
        PF_ParamIndex param_index,
        A_long start_time,
        A_long duration,
        A_u_long rate = 44100,
        PF_SoundSampleSize bytes_per_sample = PF_SSS_4,
        PF_SoundChannels num_channels = PF_Channels_STEREO,
        PF_SoundFormat fmt = PF_SIGNED_FLOAT) const {
        return layer_audio_checkout(
            m_in_data,
            param_index,
            start_time,
            duration,
            m_in_data ? m_in_data->time_scale : 1,
            rate,
            bytes_per_sample,
            num_channels,
            fmt);
    }

    /** @brief Get raw parameter definition by 1-based index. */
    PF_ParamDef* param(size_t index) const {
        if (!m_params || !m_params[index]) {
            throw aetk::core::exception(PF_Err_BAD_CALLBACK_PARAM, "Invalid parameter index");
        }
        return m_params[index];
    }

    /** @brief Get float parameter value by 1-based index. */
    float float_val(size_t index) const {
        return static_cast<float>(param(index)->u.fs_d.value);
    }

    /** @brief Get integer parameter value by 1-based index. */
    int int_val(size_t index) const {
        return static_cast<int>(param(index)->u.sd.value);
    }

    /** @brief Get boolean parameter value by 1-based index. */
    bool bool_val(size_t index) const {
        return param(index)->u.bd.value != 0;
    }
};

/**
 * @brief Context wrapper for PF_Cmd_AUDIO_SETDOWN.
 */
class audio_setdown_context {
private:
    PF_InData* m_in_data = nullptr;
    PF_OutData* m_out_data = nullptr;

public:
    audio_setdown_context(PF_InData* in_data, PF_OutData* out_data)
        : m_in_data(in_data)
        , m_out_data(out_data) {}

    PF_InData* in_data() const noexcept { return m_in_data; }
    PF_OutData* out_data() const noexcept { return m_out_data; }
};

} // namespace aetk::effect
