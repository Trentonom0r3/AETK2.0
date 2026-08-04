#pragma once

#include <aetk/core/log.hpp>
#include <aetk/core/attributes.hpp>
#include <chrono>
#include <string>
#include <atomic>

#ifdef AETK_HAS_CUDA
#include <cuda_runtime.h>
#endif

namespace aetk::core {

// ══════════════════════════════════════════════════════════════════════
//  scoped_timer — RAII profiler that logs elapsed time on destruction
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief RAII timer that logs elapsed time when it goes out of scope.
 *
 * @details Computes timing intervals between construction and destruction, 
 * pushing formatted summaries directly to the logger.
 *
 * Usage:
 *   {
 *       aetk::core::scoped_timer t("blur_pass_3");
 *       // ... expensive work ...
 *   }  // logs "blur_pass_3: 2.31ms"
 *
 * For conditional profiling, use the AETK_PROFILE macro.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Eliminates manual time measuring code blocks in critical smart-render or effect loops, utilizing standard modern RAII scope triggers.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
class scoped_timer {
public:
    /**
     * @brief Constructor for the scoped timer.
     *
     * @details Registers the start timestamp using high resolution clock.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automates standard scope profiling via RAII.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param label Identifying name for the profiling block.
     * @param level Target log severity tier.
     */
    explicit scoped_timer(std::string label, log_level level = log_level::info)
        : m_label(std::move(label)), m_level(level),
          m_start(std::chrono::high_resolution_clock::now()) {}

    /**
     * @brief Destructor that logs elapsed time.
     *
     * @details Calculates elapsed time and writes it to the logger interface.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Computes duration and writes outputs dynamically on scope exit.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    ~scoped_timer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start).count();

        std::string msg;
        if (us < 1000) {
            msg = m_label + ": " + std::to_string(us) + "us";
        } else if (us < 1000000) {
            msg = m_label + ": " + std::to_string(us / 1000.0).substr(0, 6) + "ms";
        } else {
            msg = m_label + ": " + std::to_string(us / 1000000.0).substr(0, 5) + "s";
        }

        logger::instance().log(m_level, msg);
    }

    // Non-copyable, non-movable
    scoped_timer(const scoped_timer&) = delete;
    scoped_timer& operator=(const scoped_timer&) = delete;

    /**
     * @brief Get elapsed time so far without stopping the timer.
     *
     * @details Queries elapsed time without terminating the timer.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Non-destructive timing queries.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Duration in milliseconds.
     */
    double elapsed_ms() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - m_start).count();
    }

private:
    std::string m_label;
    log_level m_level;
    std::chrono::high_resolution_clock::time_point m_start;
};

// ══════════════════════════════════════════════════════════════════════
//  memory_tracker — Atomic resource counters for leak detection
// ══════════════════════════════════════════════════════════════════════

/**
 * @brief Global atomic counters for tracking resource lifetimes.
 *
 * @details Integrated into smart_world via increment/decrement calls.
 *
 * Usage:
 *   AETK_LOG_INFO("Worlds: " + std::to_string(memory_tracker::worlds_alive()));
 *   AETK_LOG_INFO("Peak:   " + std::to_string(memory_tracker::worlds_peak()));
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, detecting memory leaks across renders requires external diagnostic trackers. `aetk::core::memory_tracker` provides thread-safe, lock-free atomic counters integrated directly into AETK objects for real-time diagnostics.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
struct memory_tracker {
    /**
     * @brief Internal worlds alive atomic reference helper.
     *
     * @details Provides access to the static atomic alive counter.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Internal static counter.
     *
     * @warning <b>Memory & Lifecycles:</b> Persists for the lifetime of the process.
     *
     * @return Atomic alive count.
     */
    static std::atomic<int64_t>& worlds_alive_ref() {
        static std::atomic<int64_t> count{0};
        return count;
    }
    
    /**
     * @brief Internal peak worlds atomic reference helper.
     *
     * @details Provides access to the static atomic peak counter.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Internal static counter.
     *
     * @warning <b>Memory & Lifecycles:</b> Persists for the lifetime of the process.
     *
     * @return Atomic peak count.
     */
    static std::atomic<int64_t>& worlds_peak_ref() {
        static std::atomic<int64_t> peak{0};
        return peak;
    }
    
    /**
     * @brief Internal GPU worlds atomic reference helper.
     *
     * @details Provides access to the static atomic GPU worlds counter.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Internal static counter.
     *
     * @warning <b>Memory & Lifecycles:</b> Persists for the lifetime of the process.
     *
     * @return Atomic GPU count.
     */
    static std::atomic<int64_t>& gpu_worlds_alive_ref() {
        static std::atomic<int64_t> count{0};
        return count;
    }

    /**
     * @brief Returns the active world count.
     *
     * @details Atomically loads the count of instantiated smart_worlds.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe atomic reads.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Instantiated worlds count.
     */
    static int64_t worlds_alive()     { return worlds_alive_ref().load(); }
    
    /**
     * @brief Returns peak worlds count.
     *
     * @details Queries historical peak world counts.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Atomic peak querying.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return Max peak worlds reached.
     */
    static int64_t worlds_peak()      { return worlds_peak_ref().load(); }
    
    /**
     * @brief Returns GPU world count.
     *
     * @details Queries active GPU allocations.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Atomic GPU tracking.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @return GPU worlds count.
     */
    static int64_t gpu_worlds_alive() { return gpu_worlds_alive_ref().load(); }

    /**
     * @brief Increment world counter.
     *
     * @details Atomically increments worlds count and updates peaks using weak compare-and-swap (CAS).
     *
     * @note <b>AE SDK Paradigm Shift:</b> Lock-free creation notification.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param is_gpu Mark if it resides in GPU VRAM space.
     */
    static void world_created(bool is_gpu = false) {
        auto current = ++worlds_alive_ref();
        // Update peak using CAS
        auto peak = worlds_peak_ref().load();
        while (current > peak && !worlds_peak_ref().compare_exchange_weak(peak, current)) {}
        if (is_gpu) ++gpu_worlds_alive_ref();
    }

    /**
     * @brief Decrement world counter.
     *
     * @details Atomically decrements the active world count on object deletion.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Lock-free destruction notification.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param is_gpu Mark if it resides in GPU VRAM space.
     */
    static void world_destroyed(bool is_gpu = false) {
        --worlds_alive_ref();
        if (is_gpu) --gpu_worlds_alive_ref();
    }

    /**
     * @brief Writes a diagnostic report to the logger.
     *
     * @details Formats and logs current atomic stats.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean diagnostic formatting.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    static void report() {
        AETK_LOG_INFO("[DIAG] Worlds alive: " + std::to_string(worlds_alive()) +
                      " | Peak: " + std::to_string(worlds_peak()) +
                      " | GPU: " + std::to_string(gpu_worlds_alive()));
    }

    /**
     * @brief Resets the peak worlds counter.
     *
     * @details Resets peak to active levels.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Clean baseline setup.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     */
    static void reset_peak() { worlds_peak_ref().store(worlds_alive()); }
};

// ══════════════════════════════════════════════════════════════════════
//  CUDA Error Checking
// ══════════════════════════════════════════════════════════════════════

#ifdef AETK_HAS_CUDA

/**
 * @brief Check a CUDA return code and throw with diagnostic info on failure.
 *
 * @details Evaluates CUDA runtime call structures and propagates failed statuses directly to standard exceptions.
 *
 * Usage:
 *   aetk::core::cuda_check(cudaMemcpy(...), "memcpy src→dst", __FILE__, __LINE__);
 *   // Or via macro:
 *   AETK_CUDA_CHECK(cudaMemcpy(...));
 *
 * @note <b>AE SDK Paradigm Shift:</b> Bridges standard CUDA runtime status codes directly with After Effects exception frameworks, translating failures to standard `PF_Err_INTERNAL_STRUCT_DAMAGED` exceptional responses.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 *
 * @param err The CUDA runtime error status to check.
 * @param operation Description text of the operation.
 * @param file Invoker file path.
 * @param line Invoker line number.
 */
inline void cuda_check(cudaError_t err, const char* operation = "",
                        const char* file = "", int line = 0) {
    if (err != cudaSuccess) {
        std::string msg = std::string("CUDA Error: ") + cudaGetErrorString(err);
        if (operation && operation[0]) msg += " | Op: " + std::string(operation);
        if (file && file[0]) msg += " | " + std::string(file) + ":" + std::to_string(line);

        AETK_LOG_ERR(msg);
        throw exception(PF_Err_INTERNAL_STRUCT_DAMAGED, msg);
    }
}

#define AETK_CUDA_CHECK(call) \
    aetk::core::cuda_check((call), #call, __FILE__, __LINE__)

#else

// No-op when CUDA is not available
#define AETK_CUDA_CHECK(call) (call)

#endif

// ══════════════════════════════════════════════════════════════════════
//  Convenience Macros
// ══════════════════════════════════════════════════════════════════════

/** @brief Profile a scope. Compiles to nothing in release if AETK_ENABLE_PROFILING is not defined. */
#ifdef AETK_ENABLE_PROFILING
    #define AETK_PROFILE(label) aetk::core::scoped_timer _aetk_timer_##__LINE__(label)
#else
    #define AETK_PROFILE(label) ((void)0)
#endif

/** @brief Always-on profiling (not gated by AETK_ENABLE_PROFILING). */
#define AETK_TIMER(label) aetk::core::scoped_timer _aetk_timer_##__LINE__(label)

} // namespace aetk::core
