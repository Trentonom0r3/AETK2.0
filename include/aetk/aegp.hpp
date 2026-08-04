#pragma once

/**
 * @file aegp.hpp
 * @brief Master inclusion header for After Effects General Plugins (AEGPs).
 * 
 * @details Exports all managed AEGP plugin models, menu hook command registrations, 
 * timeline compositions, and stream accessors.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, writing an AEGP requires loading a massive array of disparate C headers and managing dynamic callbacks using global state arrays. `aetk/aegp.hpp` unifies the entire AEGP ecosystem into a clean OOP-centric include boundary.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */

#include <aetk/core.hpp>
#include <aetk/aegp/command.hpp>
#include <aetk/aegp/hooks.hpp>
#include <aetk/aegp/plugin.hpp>
#include <aetk/aegp/dom/item.hpp>
#include <aetk/aegp/dom/footage.hpp>
#include <aetk/aegp/dom/comp.hpp>
#include <aetk/aegp/dom/stream.hpp>
#include <aetk/aegp/dom/keyframe.hpp>
#include <aetk/aegp/dom/mask.hpp>
#include <aetk/aegp/dom/folder.hpp>
#include <aetk/aegp/dom/layer_subclasses.hpp>
#include <aetk/aegp/dom/world.hpp>
#include <aetk/aegp/guards.hpp>
#include <aetk/aegp/scheduler.hpp>
