#pragma once

/**
 * @file core.hpp
 * @brief Master inclusion header for the AETK Core Subsystem.
 * 
 * @details Exports RAII suite managers, memory block trackers, type-safe handles, 
 * math vector helpers, and diagnostic utilities.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Unifies basic low-level C memory allocations and suite lookups under managed C++ constructs.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */

#include <aetk/core/types.hpp>
#include <aetk/core/suite.hpp>
#include <aetk/core/handle.hpp>
#include <aetk/core/mem_handle.hpp>
#include <aetk/core/log.hpp>
#include <aetk/core/error.hpp>
#include <aetk/core/math.hpp>
#include <aetk/core/diagnostics.hpp>
#include <aetk/core/utility.hpp>
#include <aetk/core/ort_helper.hpp>
#include <aetk/core/context.hpp>
#include <aetk/core/hash.hpp>
#include <aetk/core/kernel.hpp>
#include <aetk/core/premiere_compat.hpp>
#include <aetk/core/locale_utils.hpp>

