#pragma once

/**
 * @file locale_utils.hpp
 * @brief Thread-safe, locale-independent floating point formatting and parsing utilities.
 */

#include <clocale>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

#ifdef _WIN32
#include <locale.h>
#else
#include <xlocale.h>
#endif

namespace aetk::core {

/**
 * @brief Manager for Windows/POSIX "C" locale handles used in floating point formatting/scanning.
 */
class c_locale_guard {
public:
#ifdef _WIN32
    static _locale_t get() {
        static _locale_t s_c_locale = _create_locale(LC_NUMERIC, "C");
        return s_c_locale;
    }
#endif
};

/**
 * @brief Safely converts European decimal commas (e.g., 3,14) to dots (3.14) in a string while preserving delimiters.
 */
inline std::string normalize_decimal_points(const std::string& input) {
    std::string result = input;
    for (size_t i = 0; i < result.size(); ++i) {
        if (result[i] == ',') {
            bool prev_digit = (i > 0 && (result[i - 1] >= '0' && result[i - 1] <= '9'));
            bool next_digit = (i + 1 < result.size() && (result[i + 1] >= '0' && result[i + 1] <= '9'));
            if (prev_digit && next_digit) {
                result[i] = '.';
            }
        }
    }
    return result;
}

/**
 * @brief Formats floating point and string arguments using the standard "C" locale (dot decimal point).
 */
inline int c_snprintf(char* buffer, size_t count, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result;
#ifdef _WIN32
    result = _vsnprintf_l(buffer, count, format, c_locale_guard::get(), args);
#else
    result = std::vsnprintf(buffer, count, format, args);
#endif
    va_end(args);
    return result;
}

/**
 * @brief Parses input strings using standard "C" locale with automatic European decimal comma normalization.
 */
template <typename... Args>
inline int c_sscanf(const char* buffer, const char* format, Args... args) {
    if (!buffer || !format) return 0;
    std::string normalized = normalize_decimal_points(buffer);
#ifdef _WIN32
    return _sscanf_l(normalized.c_str(), format, c_locale_guard::get(), args...);
#else
    return std::sscanf(normalized.c_str(), format, args...);
#endif
}

} // namespace aetk::core
