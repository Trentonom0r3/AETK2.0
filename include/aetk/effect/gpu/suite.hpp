#pragma once

#include <AE_Effect.h>
#include <AE_EffectGPUSuites.h>
#include <aetk/core/suite.hpp>

namespace aetk::effect {
/*typedef struct
{
    PF_GPU_Framework device_framework;
    PF_Boolean compatibleB;	// device meets minimum requirement for acceleration

    void* platformPV; // cl_platform_id
    void* devicePV; // CUdevice or cl_device_id or MTLDevice or ID3D12Device
    void* contextPV; // CUcontext or cl_context
    void* command_queuePV; // CUstream or cl_command_queue or MTLCommandQueue or
ID3D12CommandQueue void* offscreen_opengl_contextPV; // CGLContextObj or HGLRC - only
available on the primary device void* offscreen_opengl_devicePV; // HDC - only available
on the primary device

} PF_GPUDeviceInfo;
 */

/**
 * @brief Managed access to the After Effects GPU Device Suite.
 *
 * @details This class uses RAII to acquire and release the PF_GPUDeviceSuite1.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, allocating GPU memory or acquiring
 * exclusive device locks during smart rendering requires direct, manually ordered calls
 * to `PF_GPUDeviceSuite1`. Any unmatched allocate/free or lock/unlock thrashes graphic
 * threads. `aetk::effect::gpu_device_suite` wraps these operations in a clean RAII suite
 * container.
 *
 * @warning <b>Memory & Lifecycles:</b> The suite queries device index bounds. Allocations
 * made via `allocate_device_memory` must be paired with `free_device_memory` to avoid
 * massive physical VRAM leaks.
 */
class gpu_device_suite {
public:
    /**
     * @brief GPU suite constructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Simple suite lookup binding.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param in_data Input struct parameter.
     */
    gpu_device_suite(PF_InData* in_data)
        : m_suite(in_data->pica_basicP)
        , m_in_data(in_data) {
    }

    /**
     * @brief Query count of active GPU devices.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces procedural checks with type-safe
     * properties.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Bounded device count.
     */
    A_u_long device_count() const {
        A_u_long count = 0;
        core::check_err(m_suite->GetDeviceCount(m_in_data->effect_ref, &count));
        return count;
    }

    /**
     * @brief Retrieve detailed info for a specific GPU device.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces procedural checks with type-safe
     * properties.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param index GPU device index.
     * @return Device info structure.
     */
    PF_GPUDeviceInfo device_info(A_u_long index) const {
        PF_GPUDeviceInfo info { };
        core::check_err(m_suite->GetDeviceInfo(m_in_data->effect_ref, index, &info));
        return info;
    }

    /**
     * @brief Allocate persistent buffer on GPU device.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bypasses macro-level VRAM allocation.
     *
     * @warning <b>Memory & Lifecycles:</b> Must be carefully paired to prevent VRAM
     * memory leaks.
     *
     * @param index Device index.
     * @param size Buffer size in bytes.
     * @return Device pointer.
     */
    void* allocate_device_memory(A_u_long index, std::size_t size) const {
        void* ptr = nullptr;
        core::check_err(
            m_suite->AllocateDeviceMemory(m_in_data->effect_ref, index, size, &ptr));
        return ptr;
    }

    /**
     * @brief Free allocated device memory buffer.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Bypasses macro-level VRAM allocation.
     *
     * @warning <b>Memory & Lifecycles:</b> Must be carefully paired to prevent VRAM
     * memory leaks.
     *
     * @param index Device index.
     * @param ptr Buffer pointer.
     */
    void free_device_memory(A_u_long index, void* ptr) const {
        core::check_err(m_suite->FreeDeviceMemory(m_in_data->effect_ref, index, ptr));
    }

    /**
     * @brief Acquire exclusive locks on the target device.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Thread access synchronization.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param index Device index.
     */
    void acquire_exclusive_access(A_u_long index) const {
        core::check_err(
            m_suite->AcquireExclusiveDeviceAccess(m_in_data->effect_ref, index));
    }

    /**
     * @brief Release exclusive locks on the target device.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Thread access synchronization.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param index Device index.
     */
    void release_exclusive_access(A_u_long index) const {
        core::check_err(
            m_suite->ReleaseExclusiveDeviceAccess(m_in_data->effect_ref, index));
    }

    /**
     * @brief Access the raw GPUDeviceSuite pointer.
     *
     * @note <b>AE SDK Paradigm Shift:</b> None.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Raw suite structure pointer.
     */
    const PF_GPUDeviceSuite1* operator->() const {
        return m_suite.get();
    }

private:
    core::suite<PF_GPUDeviceSuite1> m_suite;
    PF_InData* m_in_data;
};

/**
 * @brief RAII helper for exclusive GPU device access.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Standard RAII helper for exclusive GPU device
 * access synchronization, automatically unlocking in destructor.
 *
 * @warning <b>Memory & Lifecycles:</b> Acquires lock on scope entry and automatically
 * releases lock in the destructor via `release_exclusive_access`. Destructor must never
 * throw exceptions.
 */
class gpu_device_lock {
public:
    /**
     * @brief Lock constructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Scoped GPU synchronization.
     *
     * @warning <b>Memory & Lifecycles:</b> Binds lock and blocks other render threads
     * until scope exits.
     *
     * @param suite Device suite helper.
     * @param index Target device index.
     */
    gpu_device_lock(const gpu_device_suite& suite, A_u_long index)
        : m_suite(suite)
        , m_index(index) {
        m_suite.acquire_exclusive_access(m_index);
    }

    /**
     * @brief Lock destructor.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe RAII release.
     *
     * @warning <b>Memory & Lifecycles:</b> Releases GPU device lock without throwing
     * exceptions.
     */
    ~gpu_device_lock() {
        m_suite.release_exclusive_access(m_index);
    }

private:
    const gpu_device_suite& m_suite;
    A_u_long m_index;
};

} // namespace aetk::effect
