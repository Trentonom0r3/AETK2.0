#pragma once

/**
 * @file hash.hpp
 * @brief Compile-time stable hashing utilities.
 */

#include <cstdint>
#include <string_view>

/**
 * @brief Root namespace for the AETK framework.
 */
namespace aetk {
/**
 * @brief Namespace containing low-level core utilities, suite managers, and RAII components.
 */
namespace core {

/**
 * @brief Compile-time FNV-1a 32-bit string hashing.
 * Used for generating stable DISK_IDs for parameters from their string names.
 *
 * @details Computes a lightweight, non-cryptographic hash index entirely at compile time.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Replaces hardcoded, error-prone integer constants for parameter `DISK_ID` tags with stable compile-time FNV-1a hashes derived directly from parameter names.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 *
 * @param str The string view to generate a hash from.
 * @return 31-bit positive integer index value.
 */
constexpr uint32_t hash_string(std::string_view str) {
    uint32_t hash = 0x811c9dc5; // FNV offset basis
    for (char c : str) {
        hash ^= static_cast<uint32_t>(c);
        hash *= 0x01000193; // FNV prime
    }
    // We mask the high bit so it safely fits in an A_long (int32_t)
    return hash & 0x7FFFFFFF;
}

} // namespace core
} // namespace aetk
