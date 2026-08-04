#pragma once

/**
 * @file effect.hpp
 * @brief Master inclusion header for the AETK Effect Framework.
 * 
 * @details Exports CRTP plugin bases, SmartFX render wrappers, pixel converters, 
 * timeline cache structures, and parameter setup classes.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Procedural C SDK effect structures and `EffectMain` entry points are modern wrapped in type-safe class architectures.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */

#include <aetk/core.hpp>
#include <aetk/effect/context/context.hpp>
#include <aetk/effect/context/setup_contexts.hpp>
#include <aetk/effect/context/state.hpp>
#include <aetk/effect/params/param.hpp>
#include <aetk/effect/params/param_setup.hpp>
#include <aetk/effect/params/param_callbacks.hpp>
#include <aetk/effect/params/param_modifier.hpp>
#include <aetk/effect/params/serialization.hpp>
#include <aetk/effect/params/arb_traits.hpp>
#include <aetk/effect/params/interpolators.hpp>
#include <aetk/effect/params/dependencies.hpp>
#include <aetk/effect/pixel/smart_world.hpp>
#include <aetk/effect/pixel/compute_cache.hpp>
#include <aetk/effect/colorspaces.hpp>
#include <aetk/effect/sampling.hpp>
#include <aetk/effect/gpu.hpp>
#include <aetk/effect/comp_ui.hpp>
#include <aetk/effect/plugin.hpp>

