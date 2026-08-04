#pragma once

#include <aetk/effect/context/context.hpp>

namespace aetk::effect {

/**
 * @brief Context wrapper for PF_Cmd_GLOBAL_SETUP.
 *
 * @details Provides helpers to configure plugin flags, capabilities, version
 * info, and allocate global states.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, setting global flags or
 * allocating `global_data` handles requires manually bit-masking fields on
 * `out_data`, calling raw `host_new_handle` callbacks, locking handles, and
 * performing placement-new operations carefully.
 * `aetk::effect::global_setup_context` provides elegant, type-safe member
 * helpers like `enable_mfr()`, `enable_smart_render()`, and templated
 * `set_global_data<T>(...)` to automate this.
 *
 * @warning <b>Memory & Lifecycles:</b> The context does not own the allocated
 * handles. Memory stored in `set_global_data` persists across the entire
 * process lifetime and must be released in `on_global_setdown` to prevent
 * severe memory leaks. Handles use AE's host memory manager.
 */
struct global_setup_context : public context {
    /**
     * @brief Base constructor.
     *
     * @details Initialized from the parent stack-allocated context.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Promotes a raw context reference.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param ctx Parent base context.
     */
    global_setup_context(const context& ctx)
        : context(ctx) {
    }

    /**
     * @brief Set the plugin version identifier.
     *
     * @details Packs major/minor/build values using standard `PF_VERSION`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces direct `my_version` packing
     * math.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param major Major version number.
     * @param minor Minor version number.
     * @param build Build index.
     * @param stage Target release stage (e.g. `PF_Stage_RELEASE`).
     */
    void set_version(uint32_t major, uint32_t minor, uint32_t build,
        uint32_t stage = PF_Stage_RELEASE) const {
        m_out_data->my_version = PF_VERSION(major, minor, build, stage, 1);
    }

    /**
     * @brief Parse and set the plugin version from a string.
     *
     * @details Parses semantic strings using standard `sscanf_s`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces procedural version parsing
     * routines.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param version_str Semantic version string (e.g., "1.2.3").
     */
    void set_version(const char* version_str) const {
        uint32_t major = 0, minor = 0, build = 0;
        if (sscanf_s(version_str, "%u.%u.%u", &major, &minor, &build) >= 2) {
            set_version(major, minor, build);
        }
    }

    /**
     * @brief Directly mask out_flags setup flags.
     *
     * @details Manipulates standard `out_flags` fields.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct flag bitmasking.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param flags Bit flags to enable.
     */
    void add_out_flags(int32_t flags) const {
        m_out_data->out_flags |= flags;
    }

    /**
     * @brief Directly mask out_flags2 setup flags.
     *
     * @details Manipulates standard `out_flags2` fields.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct flag bitmasking.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param flags Bit flags to enable.
     */
    void add_out_flags2(int32_t flags) const {
        m_out_data->out_flags2 |= flags;
    }

    /**
     * @brief Enable support for looking at frames other than the current one.
     *
     * @details Masks `PF_OutFlag_WIDE_TIME_INPUT`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard time checkout flag wrapper.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void enable_temporal_checkouts() const {
        add_out_flags(PF_OutFlag_WIDE_TIME_INPUT);
    }

    /**
     * @brief Signal that output may vary even if parameters don't change (e.g.
     * time-based).
     *
     * @details Masks `PF_OutFlag_NON_PARAM_VARY`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Setup non-param timelines.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void enable_non_param_varying() const {
        add_out_flags(PF_OutFlag_NON_PARAM_VARY);
    }

    /**
     * @brief Signal that the effect uses the output extent for rendering.
     *
     * @details Masks `PF_OutFlag_USE_OUTPUT_EXTENT`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard output extent flag wrapper.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void enable_output_extent() const {
        add_out_flags(PF_OutFlag_USE_OUTPUT_EXTENT);
    }

    /** @brief Enable audio-only effect processing (skips image rendering calls). */
    void enable_audio_only() const {
        add_out_flags(PF_OutFlag_AUDIO_EFFECT_ONLY);
    }

    /** @brief Enable both audio and video effect processing. */
    void enable_audio_too() const {
        add_out_flags(PF_OutFlag_AUDIO_EFFECT_TOO);
    }

    /** @brief Signal that the plugin uses layer audio checkouts. */
    void enable_audio_checkouts() const {
        add_out_flags(PF_OutFlag_I_USE_AUDIO);
    }

    /** @brief Enforce 32-bit floating point audio sample delivery (PF_SIGNED_FLOAT). */
    void enable_audio_float_only() const {
        add_out_flags(PF_OutFlag_AUDIO_FLOAT_ONLY);
    }

    /** @brief Signal that the effect is an Infinite Impulse Response (IIR) filter. */
    void enable_audio_iir() const {
        add_out_flags(PF_OutFlag_AUDIO_IIR);
    }

    /** @brief Signal that the effect synthesizes audio even when handed silence. */
    void enable_audio_synthesis() const {
        add_out_flags(PF_OutFlag_I_SYNTHESIZE_AUDIO);
    }

    /**
     * @brief Enable the "Options..." button in the Effect Controls panel.
     *
     * @details Masks `PF_OutFlag_I_DO_DIALOG`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Enables dynamic dialog button support.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void enable_options_button() const {
        add_out_flags(PF_OutFlag_I_DO_DIALOG);
    }

    /**
     * @brief Enable Multi-Frame Rendering (MFR) support.
     *
     * @details Masks `PF_OutFlag2_SUPPORTS_THREADED_RENDERING`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Enforces standard MFR safety
     * compatibility.
     *
     * @warning <b>Memory & Lifecycles:</b> Plugin must strictly avoid global or
     * static variables for per-instance data to be MFR safe!
     */
    void enable_mfr() const {
        add_out_flags2(PF_OutFlag2_SUPPORTS_THREADED_RENDERING);
    }

    /**
     * @brief Enable SmartFX pipeline.
     *
     * @details Masks `PF_OutFlag2_SUPPORTS_SMART_RENDER`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Enables high-fidelity 32-bit float
     * SmartFX pipelines.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void enable_smart_render() const {
        add_out_flags2(PF_OutFlag2_SUPPORTS_SMART_RENDER);
    }

    /**
     * @brief Enable PF_Cmd_UPDATE_PARAMS_UI dispatch.
     *
     * @details Masks `PF_OutFlag_SEND_UPDATE_PARAMS_UI`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Activates UI change callback
     * dispatches.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void enable_param_supervision() const {
        add_out_flags(PF_OutFlag_SEND_UPDATE_PARAMS_UI);
    }

    /**
     * @brief Enable threaded rendering support.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard threaded rendering alias.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void enable_threaded_rendering() const {
        add_out_flags2(PF_OutFlag2_SUPPORTS_THREADED_RENDERING);
    }

    /**
     * @brief Enable 32-bit Float GPU rendering.
     *
     * @details Masks `PF_OutFlag2_SUPPORTS_GPU_RENDER_F32`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Activates native GPU render dispatch
     * hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void enable_gpu_rendering() const {
        add_out_flags2(PF_OutFlag2_SUPPORTS_GPU_RENDER_F32);
#if defined(_WIN32) || defined(AE_OS_WIN)
        add_out_flags2(PF_OutFlag2_SUPPORTS_DIRECTX_RENDERING);
#endif
    }

    /**
     * @brief Enable PiPL configuration overrides.
     *
     * @details Masks `PF_OutFlag_PiPL_OVERRIDES_OUTDATA_OUTFLAGS`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Overrides package PiPL properties.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void set_pipl_overrides() const {
        add_out_flags(PF_OutFlag_PiPL_OVERRIDES_OUTDATA_OUTFLAGS);
    }

    /**
     * @brief Enable custom UI drawing in the Effect Controls panel.
     *
     * @details Masks `PF_OutFlag_CUSTOM_UI` and handles async flags.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Promotes simple overlay draw
     * registrations.
     *
     * @warning <b>Memory & Lifecycles:</b> Custom UI overlays are incompatible
     * with Premiere Pro.
     *
     * @param async_manager If true, enables asynchronous custom UI managers.
     */
    void enable_custom_ui(bool async_manager = true) const {
        add_out_flags(PF_OutFlag_CUSTOM_UI);
        if (async_manager)
            add_out_flags2(PF_OutFlag2_CUSTOM_UI_ASYNC_MANAGER);
    }

    /**
     * @brief Register this plugin with AEGP to get a plugin ID for suite access.
     * Required for DynamicStreamSuite parameter visibility control.
     *
     * @details Acquires plugin ID via `AEGP_RegisterWithAEGP`.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automates standard registration to
     * enable advanced suite APIs.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which
     * automatically decrements the host reference count via `ReleaseSuite` when
     * it goes out of scope.
     *
     * @param name Name identifier for registration.
     * @return Unique dynamic `AEGP_PluginID` value.
     */
    AEGP_PluginID register_with_aegp(const char* name) const {
        AEGP_PluginID id = 0;
        aetk::core::suite<AEGP_UtilitySuite3> util(
            ::aetk::core::context::get_basic_suite());
        util->AEGP_RegisterWithAEGP(nullptr, name, &id);

        return id;
    }

    /**
     * @brief Allocate and store global data in one call.
     *
     * @details Allocates host handle, locks, constructs target state struct `T`
     * using placement-new, unlocks, and assigns to `global_data` output.
     *
     * Usage:
     *   ctx.set_global_data<MyGlobalState>(constructor_args...);
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces raw C handle allocations and
     * lock-construct boilerplates with type-safe, exception-safe placement-new
     * templates.
     *
     * @warning <b>Memory & Lifecycles:</b> Allocates dynamic handles on After
     * Effects' host memory allocator. Developer must ensure that
     * `on_global_setdown` disposes this handle.
     *
     * @tparam T The dynamic state struct type.
     * @param args Parameter pack forwarded directly to the constructor of `T`.
     */
    template <typename T, typename... Args> void set_global_data(Args&&... args) const {
        PF_Handle handle = (*m_in_data->utils->host_new_handle)(sizeof(T));
        if (!handle)
            throw core::exception(PF_Err_OUT_OF_MEMORY, "Failed to allocate global data");
        T* ptr = reinterpret_cast<T*>((*m_in_data->utils->host_lock_handle)(handle));
        if (ptr) {
            new (ptr) T(std::forward<Args>(args)...);
            (*m_in_data->utils->host_unlock_handle)(handle);
        }
        m_out_data->global_data = handle;
    }
};

/**
 * @brief Context wrapper for PF_Cmd_SEQUENCE_SETUP and SEQUENCE_RESETUP.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Modernized timeline sequence setup
 * context helper. Sequence data handles persist with the active layer instance.
 * `aetk::effect::sequence_setup_context` provides placement-new wrappers
 * (`set_sequence_data`) to allocate and initialize sequences safely.
 *
 * @warning <b>Memory & Lifecycles:</b> Sequence data handles must be disposed
 * in `sequence_setdown_context` calls to prevent resource leaks during layer
 * deletion.
 */
struct sequence_setup_context : public context {
    /**
     * @brief Setup constructor.
     *
     * @param ctx Parent base context.
     */
    sequence_setup_context(const context& ctx)
        : context(ctx) {
    }

    /**
     * @brief Get raw sequence handle.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct handle query.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Underlying `PF_Handle` representing active sequence state.
     */
    PF_Handle sequence_data() const {
        return m_in_data->sequence_data;
    }

    /**
     * @brief Assigns raw sequence handle.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct handle assignment.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param handle Target raw sequence handle.
     */
    void set_sequence_data(PF_Handle handle) const {
        m_out_data->sequence_data = handle;
    }

    /**
     * @brief Allocate and store sequence data in one call.
     *
     * @details Allocates host handle, locks, constructs sequence struct `T`,
     * unlocks, and assigns to `sequence_data`.
     *
     * Usage:
     *   ctx.set_sequence_data<MySeqData>(constructor_args...);
     *
     * @note <b>AE SDK Paradigm Shift:</b> Elegant type-safe sequence setup.
     *
     * @warning <b>Memory & Lifecycles:</b> Allocates dynamic host handles. Must
     * be freed on setdown.
     *
     * @tparam T The dynamic sequence state type.
     * @param args Parameter pack forwarded to the constructor of `T`.
     */
    template <typename T, typename... Args> void set_sequence_data(Args&&... args) const {
        PF_Handle handle = (*m_in_data->utils->host_new_handle)(sizeof(T));
        if (!handle)
            throw core::exception(
                PF_Err_OUT_OF_MEMORY, "Failed to allocate sequence data");
        T* ptr = reinterpret_cast<T*>((*m_in_data->utils->host_lock_handle)(handle));
        if (ptr) {
            new (ptr) T(std::forward<Args>(args)...);
            (*m_in_data->utils->host_unlock_handle)(handle);
        }
        m_out_data->sequence_data = handle;
    }
};

/**
 * @brief Context wrapper for PF_Cmd_SEQUENCE_SETDOWN.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Enforces sequence setdown context
 * interfaces.
 *
 * @warning <b>Memory & Lifecycles:</b> Used to release dynamic handles stored
 * inside sequence states.
 */
struct sequence_setdown_context : public context {
    /**
     * @brief Setdown constructor.
     *
     * @param ctx Parent base context.
     */
    sequence_setdown_context(const context& ctx)
        : context(ctx) {
    }

    /**
     * @brief Get raw sequence handle.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct query.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Underlying `PF_Handle`.
     */
    PF_Handle sequence_data() const {
        return m_in_data->sequence_data;
    }

    /**
     * @brief Assigns raw sequence handle.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct assignment.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param handle Target raw sequence handle.
     */
    void set_sequence_data(PF_Handle handle) const {
        m_out_data->sequence_data = handle;
    }
};

/**
 * @brief Context wrapper for PF_Cmd_SEQUENCE_FLATTEN and
 * GET_FLATTENED_SEQUENCE_DATA.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Sequence serialization flattening
 * context. Binds sequence configurations to binary streams so that host caching
 * or project savings work.
 *
 * @warning <b>Memory & Lifecycles:</b> Serialized handles are owned by AE and
 * stored in project files.
 */
struct flattened_sequence_context : public context {
    /**
     * @brief Flatten constructor.
     *
     * @param ctx Parent base context.
     */
    flattened_sequence_context(const context& ctx)
        : context(ctx) {
    }

    /**
     * @brief Get raw handle.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct query.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Underlying `PF_Handle`.
     */
    PF_Handle sequence_data() const {
        return m_in_data->sequence_data;
    }

    /**
     * @brief Set flattened handle.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct assignment.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param handle Target flattened sequence handle.
     */
    void set_flattened_data(PF_Handle handle) const {
        m_out_data->sequence_data = handle;
    }

    /**
     * @brief Serialize sequence data.
     *
     * @details Copies data struct `T` into a clean host handle allocation.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Streamlines sequence data
     * serialization.
     *
     * @warning <b>Memory & Lifecycles:</b> Allocates a new host handle. AE takes
     * ownership to save this inside the project file.
     *
     * @tparam T The flat data struct type.
     * @param data Reference to instance of type `T` to serialize.
     */
    template <typename T> void serialize(const T& data) const {
        PF_Handle handle = (*(m_in_data)->utils->host_new_handle)(sizeof(T));
        if (handle) {
            T* ptr
                = reinterpret_cast<T*>((*(m_in_data)->utils->host_lock_handle)(handle));
            if (ptr) {
                *ptr = data;
                (*(m_in_data)->utils->host_unlock_handle)(handle);
                set_flattened_data(handle);
            }
        }
    }
};

/**
 * @brief Context wrapper for PF_Cmd_GPU_DEVICE_SETUP.
 *
 * @note <b>AE SDK Paradigm Shift:</b> GPU device setup hook context, mapping
 * device indexes for custom GPU calculations.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct gpu_device_setup_context : public context {
    /**
     * @brief GPU setup constructor.
     *
     * @param ctx Parent base context.
     */
    gpu_device_setup_context(const context& ctx)
        : context(ctx)
        , m_gpu_extra(static_cast<PF_GPUDeviceSetupExtra*>(ctx.extra_ptr())) {
    }
    void set_device_idx(int idx) {
        m_gpu_extra->input->device_index = idx;
    }
    int get_device_idx() const {
        return m_gpu_extra->input->device_index;
    }

    bool has_framework(PF_GPU_Framework framework) const {
        auto gpusuite = gpu_device_suite(in_data_ptr());
        PF_GPUDeviceInfo info = gpusuite.device_info(get_device_idx());
        // extra->input->what_gpu == framework && info.device_framework == framework
        return m_gpu_extra->input->what_gpu == framework
            && info.device_framework == framework;
    }

    bool is_cuda() const {
        return m_gpu_extra->input->what_gpu == PF_GPU_Framework_CUDA;
    }
    bool is_opencl() const {
        return m_gpu_extra->input->what_gpu == PF_GPU_Framework_OPENCL;
    }
    bool is_metal() const {
        return m_gpu_extra->input->what_gpu == PF_GPU_Framework_METAL;
    }
    bool is_none() const {
        return m_gpu_extra->input->what_gpu == PF_GPU_Framework_NONE;
    }

    bool is_directx() const {
        return m_gpu_extra->input->what_gpu == PF_GPU_Framework_DIRECTX;
    }

    void debug_log_frameworks() {
        PF_GPU_Framework what_gpu = m_gpu_extra->input->what_gpu;
        switch (what_gpu) {
        case PF_GPU_Framework_CUDA:
            AETK_LOG_INFO("GPU Framework: CUDA");
            break;
        case PF_GPU_Framework_OPENCL:
            AETK_LOG_INFO("GPU Framework: OpenCL");
            break;
        case PF_GPU_Framework_METAL:
            AETK_LOG_INFO("GPU Framework: Metal");
            break;
        case PF_GPU_Framework_DIRECTX:
            AETK_LOG_INFO("GPU Framework: DirectX");
            break;
        default:
            AETK_LOG_INFO("GPU Framework: Unknown");
            break;
        }
    }

    /**
     * @brief Enable GPU rendering.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Standard GPU rendering flag wrapper.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    void enable_gpu_rendering() const {
        m_out_data->out_flags2 |= PF_OutFlag2_SUPPORTS_GPU_RENDER_F32;
#if defined(_WIN32) || defined(AE_OS_WIN)
        m_out_data->out_flags2 |= PF_OutFlag2_SUPPORTS_DIRECTX_RENDERING;
#endif
    }

    int32_t gpu_device_index() const {
        return m_gpu_extra->input->device_index;
    }

    void set_gpu_data(void* ptr) const {
        m_gpu_extra->output->gpu_data = ptr;
    }
    void* get_gpu_data() const {
        return m_gpu_extra->output->gpu_data;
    }

    PF_GPUDeviceSetupExtra* get_gpu_extra() const {
        return m_gpu_extra;
    }

    // gpu_device_index

private:
    PF_GPUDeviceSetupExtra* m_gpu_extra;
};

/**
 * @brief Context wrapper for PF_Cmd_GPU_DEVICE_SETDOWN.
 *
 * @note <b>AE SDK Paradigm Shift:</b> GPU device cleanup hook context.
 *
 * @warning <b>Memory & Lifecycles:</b> Used to release GPU buffers or contexts.
 */
struct gpu_device_setdown_context : public context {
    /**
     * @brief GPU setdown constructor.
     *
     * @param ctx Parent base context.
     */
    gpu_device_setdown_context(const context& ctx)
        : context(ctx)
        , m_gpu_extra(static_cast<PF_GPUDeviceSetdownExtra*>(ctx.extra_ptr())) {
    }

    /**
     * @brief Get GPU device index.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct index query.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Physical device index identifier.
     */
    int32_t device_index() const {
        return m_gpu_extra->input->device_index;
    }

    /**
     * @brief Retrieve the GPU data pointer allocated in setup.
     *
     * @return Raw GPU data pointer.
     */
    void* get_gpu_data() const {
        return m_gpu_extra->input->gpu_data;
    }

private:
    PF_GPUDeviceSetdownExtra* m_gpu_extra;
};

/**
 * @brief Context wrapper for PF_Cmd_QUERY_DYNAMIC_FLAGS.
 */
struct query_dynamic_flags_context : public context {
    explicit query_dynamic_flags_context(const context& ctx)
        : context(ctx) {
    }

    void add_out_flags(int32_t flags) const {
        m_out_data->out_flags |= flags;
    }
    void add_out_flags2(int32_t flags) const {
        m_out_data->out_flags2 |= flags;
    }
    void remove_out_flags(int32_t flags) const {
        m_out_data->out_flags &= ~flags;
    }
    void remove_out_flags2(int32_t flags) const {
        m_out_data->out_flags2 &= ~flags;
    }
};

struct frame_setup_context : public context {
    explicit frame_setup_context(const context& ctx)
        : context(ctx) {
    }

    int32_t width() const {
        return m_out_data->width;
    }
    void set_width(int32_t w) const {
        m_out_data->width = w;
    }

    int32_t height() const {
        return m_out_data->height;
    }
    void set_height(int32_t h) const {
        m_out_data->height = h;
    }

    int16_t origin_h() const {
        return m_out_data->origin.h;
    }
    void set_origin_h(int16_t h) const {
        m_out_data->origin.h = h;
    }

    int16_t origin_v() const {
        return m_out_data->origin.v;
    }
    void set_origin_v(int16_t v) const {
        m_out_data->origin.v = v;
    }
};

/**
 * @brief Context wrapper for PF_Cmd_COMPLETELY_GENERAL.
 */
struct completely_general_context : public context {
    void* extra;

    completely_general_context(const context& ctx, void* extra_ptr)
        : context(ctx)
        , extra(extra_ptr) {
    }
};

} // namespace aetk::effect
