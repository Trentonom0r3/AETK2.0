#pragma once

/**
 * @file premiere_compat.hpp
 * @brief Compile-time compatibility helpers for Premiere Pro integrations.
 */

#ifdef AETK_PREMIERE_COMPAT

#include <type_traits>

namespace aetk::core {

/**
 * @brief Helper structure that always evaluates to std::false_type.
 * 
 * @details Useful for static assertions in template specializations to trigger
 * compile-time failures inside branch dispatches.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Provides a compile-time utility for static assertions inside templated compat code blocks, ensuring conditional compilation safety.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 * 
 * @tparam T Generic parameter type.
 */
template <typename T>
struct always_false : std::false_type {};

} // namespace aetk::core

#endif
