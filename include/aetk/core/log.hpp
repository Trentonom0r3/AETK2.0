#pragma once

#include "error.hpp"
#include <AE_EffectCB.h>
#include <aetk/core/suite.hpp>
#include <atomic>
#include <chrono>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>

#endif

/**
 *  Thread-safe deduplicating logging helper
static void log_unique(const std::string &msg) {
 static std::unordered_set<std::string> logged_messages;
 static std::mutex log_mutex;

 std::lock_guard<std::mutex> lock(log_mutex);
 if (logged_messages.find(msg) == logged_messages.end()) {
   logged_messages.insert(msg);
   AETK_LOG_INFO(msg);
 }
}
*/

namespace aetk::core {

/**
 * @brief Logger severity level categories.
 */
enum class log_level : std::uint8_t {
    trace = 0,
    debug = 1,
    info = 2,
    warning = 3,
    error = 4,
    critical = 5,
    off = 6
};

/**
 * @brief Thread-safe logging subsystem for AETK.
 *
 * @details Implements a unified diagnostics logger singleton with multi-sink
 * dispatches. Formatted outputs are concurrently directed to a personal
 * Documents file, standard OS debug pipes, and the native After Effects Info
 * panel.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, logging messages or
 * debugging requires writing host-specific `PF_InfoDrawText` calls, which crash
 * if stack variables are stale, or using brittle output scripts.
 * `aetk::core::logger` implements a safe, unified logging singleton that
 * outputs concurrently to a persistent text file in user Documents, the OS
 * Debug Stream (`OutputDebugStringA`), and the After Effects Info Panel
 * (`PF_AdvAppSuite2::PF_InfoDrawText`) safely.
 *
 * @warning <b>Memory & Lifecycles:</b> The logger is a global singleton. Ensure
 * `init` is called on plugin startup with a unique name to initialize the log
 * file channel. Custom suite checkouts for drawing to the Info Panel utilize
 * `aetk::core::suite` which automatically decrements the host reference count
 * via `ReleaseSuite` when it goes out of scope.
 */
class logger {
public:
    /**
     * @brief Returns the global logger instance reference.
     *
     * @details Accesses the static unified logger singleton.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Unified static access.
     *
     * @warning <b>Memory & Lifecycles:</b> The static instance is persisted for
     * the entire process duration.
     *
     * @return Reference to the logger singleton.
     */
    static logger& instance() {
        static logger inst;
        return inst;
    }

    /**
     * @brief Initialize the log file path.
     *
     * @details Resolves platform personal paths and opens file handlers.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automates platform directory
     * resolution.
     *
     * @warning <b>Memory & Lifecycles:</b> Opens standard file output handlers.
     * Automatically closed on exit.
     *
     * @param filename Target log file basename.
     */
    void init(const std::string& filename) {
#ifdef _WIN32
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, path))) {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, filename.c_str(), (int)filename.size(), nullptr, 0);
            std::wstring w_filename(wlen, 0);
            MultiByteToWideChar(CP_UTF8, 0, filename.c_str(), (int)filename.size(), &w_filename[0], wlen);
            std::wstring log_path = std::wstring(path) + L"\\" + w_filename;
            m_file.open(log_path.c_str(), std::ios::app);
        }
#else
        m_file.open(filename, std::ios::app);
#endif
    }

    /**
     * @brief Log a formatted diagnostic message.
     *
     * @details Prepends timestamps and dispatches concurrently to file channels,
     * the OS debug console, and AE's Info Panel.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces custom, thread-unsafe, and
     * crash-prone console dispatches with a unified multi-sink framework.
     *
     * @warning <b>Memory & Lifecycles:</b> Uses `aetk::core::suite` which
     * automatically decrements the host reference count via `ReleaseSuite` when
     * it goes out of scope to draw to the AE Info panel. Captures all exception
     * events internally to prevent infinite recursions or crashes.
     *
     * @param level Severity level of the log entry.
     * @param message Text content of the log message.
     */
    void log(log_level level, const std::string& message,
        const std::source_location& loc = std::source_location::current()) {
        if (level < m_level.load(std::memory_order_relaxed))
            return;
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string prefix;
        switch (level) {
        case log_level::trace:
            prefix = "[TRACE] ";
            break;
        case log_level::debug:
            prefix = "[DEBUG] ";
            break;
        case log_level::info:
            prefix = "[INFO]  ";
            break;
        case log_level::warning:
            prefix = "[WARN]  ";
            break;
        case log_level::error:
            prefix = "[ERROR] ";
            break;
        case log_level::critical:
            prefix = "[CRIT]  ";
            break;
        default:
            break;
        }
        std::string unique_key = std::string(loc.file_name()) + ":"
            + std::to_string(loc.line()) + ":" + message;
        if (logged_messages.find(unique_key) != logged_messages.end()) {
            return;
        }
        logged_messages.insert(unique_key);
        auto now_time
            = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        struct tm time_info;
#ifdef _WIN32
        localtime_s(&time_info, &now_time);
#else
        localtime_r(&now_time, &time_info);
#endif

        std::stringstream ss;
        ss << std::put_time(&time_info, "%H:%M:%S") << " " << prefix << message;
        std::string formatted = ss.str();

        // 1. Write to file (User Documents)
        if (m_file.is_open()) {
            m_file << formatted << "\n";
            m_file.flush();
        }

        // 2. Write to OS Debug Stream (Viewable in VS or DebugView)
#ifdef _WIN32
        OutputDebugStringA((formatted + "\n").c_str());
#endif

        // 3. Write to AE Info Panel (UI)
        if (level > log_level::debug) {
            try {
                suite<PF_AdvAppSuite2> app_suite;
                PF_Err err = app_suite->PF_InfoDrawText(message.c_str(), "");
                check_err(err, "Failed to draw to Info Panel");
            } catch (const std::exception&) {
                std::cerr << formatted << "\n";
            }
        }
    }

    void set_level(log_level level) {
        m_level.store(level, std::memory_order_relaxed);
    }

    log_level get_level() const noexcept {
        return m_level.load(std::memory_order_relaxed);
    }

    template <typename... Args>
    void log_format(log_level level, std::string_view fmt, Args&&... args) {
        if (level < m_level.load(std::memory_order_relaxed))
            return;
        if constexpr (sizeof...(Args) == 0) {
            log(level, std::string(fmt));
        } else {
            log(level, std::vformat(fmt, std::make_format_args(args...)));
        }
    }

    template <typename... Args>
    void log_format_loc(log_level level, const std::source_location& loc,
        std::string_view fmt, Args&&... args) {
        if (level < m_level.load(std::memory_order_relaxed))
            return;
        if constexpr (sizeof...(Args) == 0) {
            log(level, std::string(fmt), loc);
        } else {
            log(level, std::vformat(fmt, std::make_format_args(args...)), loc);
        }
    }

private:
    logger() = default;
    ~logger() {
        if (m_file.is_open())
            m_file.close();
    }
    std::unordered_set<std::string> logged_messages;
    std::ofstream m_file;
    std::mutex m_mutex;
    std::atomic<log_level> m_level { log_level::info };
};

#define AETK_TRACE(fmt, ...)                                                             \
    ::aetk::core::logger::instance().log_format_loc(::aetk::core::log_level::trace,      \
        std::source_location::current(), fmt, ##__VA_ARGS__)
#define AETK_DEBUG(fmt, ...)                                                             \
    ::aetk::core::logger::instance().log_format_loc(::aetk::core::log_level::debug,      \
        std::source_location::current(), fmt, ##__VA_ARGS__)
#define AETK_INFO(fmt, ...)                                                              \
    ::aetk::core::logger::instance().log_format_loc(::aetk::core::log_level::info,       \
        std::source_location::current(), fmt, ##__VA_ARGS__)
#define AETK_WARN(fmt, ...)                                                              \
    ::aetk::core::logger::instance().log_format_loc(::aetk::core::log_level::warning,    \
        std::source_location::current(), fmt, ##__VA_ARGS__)
#define AETK_ERROR(fmt, ...)                                                             \
    ::aetk::core::logger::instance().log_format_loc(::aetk::core::log_level::error,      \
        std::source_location::current(), fmt, ##__VA_ARGS__)
#define AETK_CRITICAL(fmt, ...)                                                          \
    ::aetk::core::logger::instance().log_format_loc(::aetk::core::log_level::critical,   \
        std::source_location::current(), fmt, ##__VA_ARGS__)

#define AETK_LOG_DEBUG(fmt, ...) AETK_DEBUG(fmt, ##__VA_ARGS__)
#define AETK_LOG_INFO(fmt, ...) AETK_INFO(fmt, ##__VA_ARGS__)
#define AETK_LOG_WARN(fmt, ...) AETK_WARN(fmt, ##__VA_ARGS__)
#define AETK_LOG_ERR(fmt, ...) AETK_ERROR(fmt, ##__VA_ARGS__)

// quick helpers to set up logger
#define AETK_START_DEBUG(name)                                                           \
    ::aetk::core::logger::instance().init(std::string(name) + ".log");                   \
    ::aetk::core::logger::instance().set_level(::aetk::core::log_level::debug);
#define AETK_START_TRACE(name)                                                           \
    ::aetk::core::logger::instance().init(std::string(name) + ".log");                   \
    ::aetk::core::logger::instance().set_level(::aetk::core::log_level::trace);
#define AETK_START_INFO(name)                                                            \
    ::aetk::core::logger::instance().init(std::string(name) + ".log");                   \
    ::aetk::core::logger::instance().set_level(::aetk::core::log_level::info);
#define AETK_START_WARN(name)                                                            \
    ::aetk::core::logger::instance().init(std::string(name) + ".log");                   \
    ::aetk::core::logger::instance().set_level(::aetk::core::log_level::warning);
#define AETK_START_ERROR(name)                                                           \
    ::aetk::core::logger::instance().init(std::string(name) + ".log");                   \
    ::aetk::core::logger::instance().set_level(::aetk::core::log_level::error);
#define AETK_START_CRITICAL(name)                                                        \
    ::aetk::core::logger::instance().init(std::string(name) + ".log");                   \
    ::aetk::core::logger::instance().set_level(::aetk::core::log_level::critical);
} // namespace aetk::core
