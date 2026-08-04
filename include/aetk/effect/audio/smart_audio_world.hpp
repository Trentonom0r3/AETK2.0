#pragma once

#include <AE_Effect.h>
#include <aetk/core/error.hpp>
#include <aetk/effect/pixel/tensor_view.hpp>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace aetk::effect {

/**
 * @brief RAII resource manager and zero-copy tensor adapter for After Effects sound worlds (`PF_SoundWorld`).
 *
 * @details Mirrors `smart_world` for audio data. Wraps `PF_SoundWorld` buffers, ensuring safe
 * lifecycle management, format inspection, and zero-copy 2D `tensor_view` indexing over interleaved PCM audio.
 */
class smart_audio_world {
public:
    enum class ownership : std::uint8_t {
        NONE,           ///< Non-owning view (e.g. src_snd or dest_snd provided by AE host)
        LAYER_CHECKOUT, ///< Checked out from PF_CHECKOUT_LAYER_AUDIO -> requires checkin
        SCRATCH_CPU     ///< Dynamically allocated CPU sound buffer -> requires host dispose
    };

private:
    PF_SoundWorld* m_world = nullptr;
    PF_InData* m_in_data = nullptr;
    ownership m_ownership = ownership::NONE;
    PF_LayerAudio m_layer_audio = nullptr;

    void cleanup() noexcept {
        if (!m_world) return;

        if (m_ownership == ownership::LAYER_CHECKOUT && m_in_data && m_layer_audio) {
            PF_CHECKIN_LAYER_AUDIO(m_in_data, m_layer_audio);
        }
        m_world = nullptr;
        m_in_data = nullptr;
        m_layer_audio = nullptr;
        m_ownership = ownership::NONE;
    }

public:
    ~smart_audio_world() {
        cleanup();
    }

    smart_audio_world() noexcept = default;

    /**
     * @brief Construct a non-owning or managed smart_audio_world wrapping a PF_SoundWorld pointer.
     */
    explicit smart_audio_world(PF_SoundWorld* w, PF_InData* in_data = nullptr, ownership type = ownership::NONE, PF_LayerAudio layer_audio = nullptr)
        : m_world(w)
        , m_in_data(in_data)
        , m_ownership(type)
        , m_layer_audio(layer_audio) {}

    // Deleted copy construction/assignment to prevent double-free host buffer crashes
    smart_audio_world(const smart_audio_world&) = delete;
    smart_audio_world& operator=(const smart_audio_world&) = delete;

    // Move constructor & assignment
    smart_audio_world(smart_audio_world&& other) noexcept
        : m_world(other.m_world)
        , m_in_data(other.m_in_data)
        , m_ownership(other.m_ownership)
        , m_layer_audio(other.m_layer_audio) {
        other.m_world = nullptr;
        other.m_in_data = nullptr;
        other.m_layer_audio = nullptr;
        other.m_ownership = ownership::NONE;
    }

    smart_audio_world& operator=(smart_audio_world&& other) noexcept {
        if (this != &other) {
            cleanup();
            m_world = other.m_world;
            m_in_data = other.m_in_data;
            m_ownership = other.m_ownership;
            m_layer_audio = other.m_layer_audio;

            other.m_world = nullptr;
            other.m_in_data = nullptr;
            other.m_layer_audio = nullptr;
            other.m_ownership = ownership::NONE;
        }
        return *this;
    }

    /** @brief Check if underlying audio world is valid. */
    bool is_valid() const noexcept { return m_world != nullptr && m_world->dataP != nullptr; }
    explicit operator bool() const noexcept { return is_valid(); }

    /** @brief Get underlying raw PF_SoundWorld pointer. */
    PF_SoundWorld* world() const noexcept { return m_world; }

    /** @brief Get sample rate (Hz). */
    double sample_rate() const noexcept {
        return m_world ? m_world->fi.rateF : 0.0;
    }

    /** @brief Get channel count (1 = Mono, 2 = Stereo). */
    size_t num_channels() const noexcept {
        return m_world ? static_cast<size_t>(m_world->fi.num_channels) : 0;
    }

    /** @brief Get total number of audio samples. */
    size_t num_samples() const noexcept {
        if (!m_world) return 0;
        if (m_world->num_samples > 0) return static_cast<size_t>(m_world->num_samples);
        if (m_in_data) return static_cast<size_t>(m_in_data->dur_sampL);
        return 0;
    }

    /** @brief Get sample size in bytes. */
    size_t sample_size() const noexcept {
        return m_world ? static_cast<size_t>(m_world->fi.sample_size) : 0;
    }

    /** @brief Get audio sample format enum. */
    PF_SoundFormat format() const noexcept {
        return m_world ? m_world->fi.format : static_cast<PF_SoundFormat>(0);
    }

    /** @brief Return true if audio sample format is 32-bit floating point. */
    bool is_float() const noexcept {
        return format() == PF_SIGNED_FLOAT;
    }

    /** @brief Access raw void* audio buffer pointer. */
    void* data_ptr() const noexcept {
        return m_world ? m_world->dataP : nullptr;
    }

    /** @brief Access typed audio sample pointer. */
    template <typename T = float>
    T* samples() const noexcept {
        return reinterpret_cast<T*>(data_ptr());
    }

    /**
     * @brief Obtain a zero-copy 2D tensor view over interleaved PCM audio samples [channels, samples].
     *
     * @tparam T Sample type (defaults to float).
     * @return tensor_view<T, 2> with shape [num_channels, num_samples].
     */
    template <typename T = float>
    tensor_view<T, 2> tensor_view_2d() const {
        if (!is_valid()) {
            throw aetk::core::exception(PF_Err_BAD_CALLBACK_PARAM, "Invalid sound world pointer in tensor_view");
        }
        size_t chans = num_channels();
        size_t samps = num_samples();
        
        size_t shape[2] = { chans, samps };
        // AE audio buffers are interleaved: sample 0 ch 0, sample 0 ch 1, sample 1 ch 0, etc.
        // Stride for channel index is 1, stride for sample index is num_channels.
        ptrdiff_t strides[2] = { 1, static_cast<ptrdiff_t>(chans) };

        return tensor_view<T, 2>(samples<T>(), shape, strides);
    }
};

} // namespace aetk::effect
