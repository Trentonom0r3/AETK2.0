#pragma once

/**
 * @file device.hpp
 * @brief GPU device utilities.
 * 
 * @details The gpu::device and gpu::world classes that previously lived here have been
 * consolidated into gpu_device_suite (gpu/suite.hpp) and smart_world (effect/world.hpp).
 * 
 * This file is kept for the device_suite typedef which provides a convenient
 * static-call interface to the GPU device suite.
 */

#include <aetk/core/suite.hpp>
#include <AE_EffectGPUSuites.h>

namespace aetk::effect::gpu {

/**
 * @brief Convenience typedef for static suite access pattern.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Replaces raw procedural PICA suite loading macro lookups for GPU devices with a type-safe static-call interface.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
using device_suite = core::suite<PF_GPUDeviceSuite1, core::fixed_string(kPFGPUDeviceSuite), kPFGPUDeviceSuiteVersion1>;

} // namespace aetk::effect::gpu
