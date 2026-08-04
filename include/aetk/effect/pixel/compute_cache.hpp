#pragma once

#include <aetk/core/context.hpp>
#include <aetk/core/log.hpp>
#include <aetk/core/suite.hpp>
#include <functional>
#include <map>
#include <string>

#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

namespace aetk::effect {

/**
 * @brief Forward declaration of size estimator to resolve cyclic template
 * dependencies.
 */
template <typename T> size_t get_approx_size(const T* val);

/**
 * @brief Helper to measure capacity-based footprint of a vector or string.
 */
template <typename Container> size_t approx_size_heap_bytes(const Container& c) {
    using item_type = typename Container::value_type;
    size_t size = c.capacity() * sizeof(item_type);
    if constexpr (requires { get_approx_size(&c[0]); }) {
        for (const auto& item : c) {
            size += get_approx_size(&item) - sizeof(item_type);
        }
    }
    return size;
}

/**
 * @brief Helper to sum up multiple sizes.
 */
template <typename... Args> size_t approx_size_sum(Args&&... args) {
    return (0 + ... + args);
}

/**
 * @brief Traits class to customize size estimation for a type.
 */
template <typename T> struct compute_cache_traits {
    static size_t approx_size(const T* val) {
        if (!val)
            return 0;
        if constexpr (requires { val->capacity(); }) {
            return sizeof(T) + approx_size_heap_bytes(*val);
        } else {
            return sizeof(T);
        }
    }
};

/**
 * @brief Estimates the approximate heap memory footprint of a cached value.
 *
 * @details Recursively traverses container capacities and string allocations
 * to provide AE's compute cache with an accurate eviction pressure signal.
 * Without accurate sizing, AE will never evict old entries and memory
 * usage will grow unbounded.
 *
 * @note <b>AE SDK Paradigm Shift:</b> The host uses this value to decide
 * when to purge stale compute cache entries. Underreporting causes AE to
 * hoard entries indefinitely, leading to multi-GB memory bloat.
 *
 * @tparam T Cached value type.
 * @param val Pointer to the cached value.
 * @return Estimated heap allocation in bytes.
 */
template <typename T> size_t get_approx_size(const T* val) {
    return compute_cache_traits<T>::approx_size(val);
}

/**
 * @brief RAII wrapper for an AEGP Compute Cache receipt.
 *
 * Ensures that the compute cache receipt is checked in automatically on
 * destruction, preventing memory leaks and thread locks.
 */
template <typename T> class receipt_lock {
public:
    receipt_lock() = default;

    receipt_lock(AEGP_CCCheckoutReceiptP receipt, T* val, SPBasicSuite* pica)
        : m_pica(pica)
        , m_receipt(receipt)
        , m_val(val) {
    }

    receipt_lock(std::shared_ptr<T> val)
        : m_val(val.get())
        , m_local_val(std::move(val)) {
    }

    ~receipt_lock() {
#ifndef AETK_PREMIERE_COMPAT
        if (m_receipt && m_pica) {
            aetk::core::suite<AEGP_ComputeCacheSuite1> suite(m_pica);
            aetk::core::check_err(suite->AEGP_CheckinComputeReceipt(m_receipt));
        }
#endif
    }

    // Move-only semantics
    receipt_lock(const receipt_lock&) = delete;
    receipt_lock& operator=(const receipt_lock&) = delete;

    receipt_lock(receipt_lock&& other) noexcept
        : m_pica(other.m_pica)
        , m_receipt(other.m_receipt)
        , m_val(other.m_val)
        , m_local_val(std::move(other.m_local_val)) {
        other.m_receipt = nullptr;
        other.m_val = nullptr;
        other.m_pica = nullptr;
    }

    receipt_lock& operator=(receipt_lock&& other) noexcept {
        if (this != &other) {
#ifndef AETK_PREMIERE_COMPAT
            if (m_receipt && m_pica) {
                aetk::core::suite<AEGP_ComputeCacheSuite1> suite(m_pica);
                aetk::core::check_err(suite->AEGP_CheckinComputeReceipt(m_receipt));
            }
#endif
            m_pica = other.m_pica;
            m_receipt = other.m_receipt;
            m_val = other.m_val;
            m_local_val = std::move(other.m_local_val);
            other.m_receipt = nullptr;
            other.m_val = nullptr;
            other.m_pica = nullptr;
        }
        return *this;
    }

    T* get() const noexcept {
        return m_val;
    }
    T* operator->() const noexcept {
        return m_val;
    }
    explicit operator bool() const noexcept {
        return m_val != nullptr;
    }

private:
    SPBasicSuite* m_pica = nullptr;
    AEGP_CCCheckoutReceiptP m_receipt = nullptr;
    T* m_val = nullptr;
    std::shared_ptr<T> m_local_val;
};

/**
 * @brief RAII wrapper for the After Effects Compute Cache.
 *
 * @details Type-safe modern template wrapping AEGP_ComputeCacheCallbacks and
 * AEGP_ClassRegister / AEGP_ClassUnregister.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, utilizing After Effects'
 * high-performance background frame and computation caching mechanisms via
 * `AEGP_ComputeCacheSuite1` requires registering global callback structs,
 * managing untyped receipts, and executing manual check-in steps
 * (`AEGP_CheckinComputeReceipt`). Any missing or misordered check-in locks up
 * worker rendering threads and thrashes sequence rendering playback.
 * `aetk::effect::compute_cache` unifies these C-style callbacks under a
 * type-safe modern template that manages class registrations and automatically
 * wraps receipt contexts cleanly.
 *
 * @warning <b>Memory & Lifecycles:</b> The cache internally owns dynamic
 * allocations mapped to AE receipts. Registered custom computation classes must
 * be unregistered on destruction via `AEGP_ClassUnregister` to prevent host
 * leaks.
 *
 * @tparam T The type of the cached data.
 */
template <typename T> class compute_cache {
private:
    // Thread-local context struct passed as options_refcon to prevent data races
    // during MFR multi-threaded render calls.
    struct CheckoutContext {
        AEGP_GUID key;
        std::function<std::unique_ptr<T>()> func;
    };

    struct GuidCompare {
        bool operator()(const AEGP_GUID& a, const AEGP_GUID& b) const {
            for (int i = 0; i < 4; ++i) {
                if (a.bytes[i] != b.bytes[i]) {
                    return a.bytes[i] < b.bytes[i];
                }
            }
            return false;
        }
    };

public:
    /// Functor computing the cached value on a miss.
    using ComputeFunc = std::function<std::unique_ptr<T>()>;

    /**
     * @brief Registers a custom cache class.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces procedural registry hooks with
     * compile-time registered class IDs.
     *
     * @warning <b>Memory & Lifecycles:</b> Registers persistent functional
     * callbacks in the host. The class ID must remain unique across plugins.
     *
     * @param pica Pointer to the basic PICA suite.
     * @param class_id Unique string identifying the cached object class.
     */
    compute_cache(
        std::string class_id, bool force_local = false, size_t max_capacity = 1000)
        : m_class_id(std::move(class_id))
        , m_force_local(force_local)
        , m_max_capacity(max_capacity) {

        m_pica = aetk::core::context::get_basic_suite();
#ifndef AETK_PREMIERE_COMPAT
        if (m_pica && !m_force_local) {
            A_Err err = m_pica->AcquireSuite(kAEGPComputeCacheSuite,
                kAEGPComputeCacheSuiteVersion1, (const void**)&m_suite_ptr);
            if (err == PF_Err_NONE && m_suite_ptr) {
                AETK_TRACE(
                    "[compute_cache] Successfully acquired AEGPComputeCacheSuite for {}",
                    m_class_id);
                m_pica->ReleaseSuite(
                    kAEGPComputeCacheSuite, kAEGPComputeCacheSuiteVersion1);
            } else {
                AETK_TRACE("[compute_cache] Failed to acquire AEGPComputeCacheSuite for "
                           "{}, err: {}",
                    m_class_id, err);
                m_suite_ptr = nullptr;
            }
        } else {
            if (m_force_local) {
                AETK_TRACE("[compute_cache] Force-local requested, skipping "
                           "AEGPComputeCacheSuite for {}",
                    m_class_id);
            } else {
                AETK_TRACE(
                    "[compute_cache] No PICA suite pointer available for {}", m_class_id);
            }
            m_suite_ptr = nullptr;
        }
#else
        AETK_TRACE(
            "[compute_cache] Premiere Pro compat mode enabled, using local cache for {}",
            m_class_id);
        m_suite_ptr = nullptr;
#endif

#ifndef AETK_PREMIERE_COMPAT
        if (m_suite_ptr) {
            m_callbacks.generate_key
                = [](AEGP_CCComputeOptionsRefconP opt, AEGP_CCComputeKeyP key) -> A_Err {
                auto* ctx_loc = static_cast<CheckoutContext*>(opt);
                *key = ctx_loc->key;
                return A_Err_NONE;
            };

            m_callbacks.compute = [](AEGP_CCComputeOptionsRefconP opt,
                                      AEGP_CCComputeValueRefconP* val) -> A_Err {
                auto* ctx_loc = static_cast<CheckoutContext*>(opt);
                AETK_TRACE("[compute_cache] compute callback triggered for GUID: {:08x}",
                    ctx_loc->key.bytes[0]);
                try {
                    auto result = ctx_loc->func();
                    *val = result.release();
                    return A_Err_NONE;
                } catch (const std::exception& e) {
                    AETK_ERROR("[compute_cache] Exception during compute callback: {}",
                        e.what());
                    return A_Err_GENERIC;
                }
            };

            m_callbacks.approx_size_value = [](AEGP_CCComputeValueRefconP val) -> size_t {
                size_t size = get_approx_size(static_cast<const T*>(val));
                AETK_TRACE("[compute_cache] approx_size_value called for pointer {}, "
                           "size: {} bytes",
                    val, size);
                return size;
            };

            m_callbacks.delete_compute_value = [](AEGP_CCComputeValueRefconP val) {
                AETK_TRACE(
                    "[compute_cache] delete_compute_value called for pointer {}", val);
                delete static_cast<T*>(val);
            };

            // Acquire once more to keep the suite reference alive
            m_pica->AcquireSuite(kAEGPComputeCacheSuite, kAEGPComputeCacheSuiteVersion1,
                (const void**)&m_suite_ptr);
            aetk::core::check_err(
                m_suite_ptr->AEGP_ClassRegister(m_class_id.c_str(), &m_callbacks));
        }
#endif
    }

    /**
     * @brief Unregisters the cache class.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Safe RAII teardown.
     *
     * @warning <b>Memory & Lifecycles:</b> Invokes `AEGP_ClassUnregister` without
     * throwing exceptions.
     */
    ~compute_cache() {
#ifndef AETK_PREMIERE_COMPAT
        if (m_suite_ptr && m_pica) {
            m_suite_ptr->AEGP_ClassUnregister(m_class_id.c_str());
            m_pica->ReleaseSuite(kAEGPComputeCacheSuite, kAEGPComputeCacheSuiteVersion1);
        }
#endif
    }

    /**
     * @brief Get the cached value or compute it if missing.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Streamlines double-checked checkout and
     * lock-free thread waiting.
     *
     * @warning <b>Memory & Lifecycles:</b> Returns a type-safe receipt_lock
     * managing receipt lifetime.
     *
     * @param key Bounded unique cache GUID.
     * @param func Target compute callback functor.
     * @return type-safe receipt_lock container.
     */
    receipt_lock<T> checkout(const AEGP_GUID& key, ComputeFunc func) {
        if (m_suite_ptr) {
            AETK_TRACE(
                "[compute_cache] checkout USING SUITE for GUID: {:08x}", key.bytes[0]);
            CheckoutContext ctx_loc { key, std::move(func) };

            AEGP_CCCheckoutReceiptP receipt = nullptr;
            aetk::core::check_err(
                m_suite_ptr->AEGP_ComputeIfNeededAndCheckout(m_class_id.c_str(),
                    &ctx_loc, // Thread-safe local refcon
                    true, // Wait for other threads
                    &receipt));

            void* val = nullptr;
            aetk::core::check_err(
                m_suite_ptr->AEGP_GetReceiptComputeValue(receipt, &val));

            return receipt_lock<T>(receipt, static_cast<T*>(val), m_pica);
        } else {
            AETK_TRACE("[compute_cache] checkout USING LOCAL CACHE for GUID: {:08x}",
                key.bytes[0]);
            std::lock_guard<std::mutex> lock(m_local_mutex);
            m_access_counter++;
            auto it = m_local_cache.find(key);
            if (it != m_local_cache.end()) {
                it->second.last_accessed = m_access_counter;
                return receipt_lock<T>(it->second.value);
            }

            auto computed = func();
            std::shared_ptr<T> shared_val = std::move(computed);

            // Capped LRU Eviction to prevent leaks
            if (m_local_cache.size() >= m_max_capacity) {
                auto oldest_it = m_local_cache.end();
                uint64_t oldest_access = (std::numeric_limits<uint64_t>::max)();
                for (auto map_it = m_local_cache.begin(); map_it != m_local_cache.end();
                    ++map_it) {
                    if (map_it->second.last_accessed < oldest_access) {
                        oldest_access = map_it->second.last_accessed;
                        oldest_it = map_it;
                    }
                }
                if (oldest_it != m_local_cache.end()) {
                    AETK_TRACE(
                        "[compute_cache] Evicting oldest GUID from local cache: {:08x}",
                        oldest_it->first.bytes[0]);
                    m_local_cache.erase(oldest_it);
                }
            }

            m_local_cache[key] = { shared_val, m_access_counter };
            return receipt_lock<T>(shared_val);
        }
    }

    receipt_lock<T> checkout_existing(const AEGP_GUID& key) {
        if (m_suite_ptr) {
            AETK_TRACE("[compute_cache] checkout_existing USING SUITE for GUID: {:08x}",
                key.bytes[0]);
            CheckoutContext ctx_loc { key, nullptr };
            AEGP_CCCheckoutReceiptP receipt = nullptr;

            A_Err err = m_suite_ptr->AEGP_ComputeIfNeededAndCheckout(m_class_id.c_str(),
                &ctx_loc,
                false, // Do not compute if needed
                &receipt);

            if (err != A_Err_NONE || !receipt) {
                return receipt_lock<T>();
            }

            void* val = nullptr;
            A_Err val_err = m_suite_ptr->AEGP_GetReceiptComputeValue(receipt, &val);
            if (val_err != A_Err_NONE || !val) {
                m_suite_ptr->AEGP_CheckinComputeReceipt(receipt);
                return receipt_lock<T>();
            }

            return receipt_lock<T>(receipt, static_cast<T*>(val), m_pica);
        } else {
            AETK_TRACE(
                "[compute_cache] checkout_existing USING LOCAL CACHE for GUID: {:08x}",
                key.bytes[0]);
            std::lock_guard<std::mutex> lock(m_local_mutex);
            m_access_counter++;
            auto it = m_local_cache.find(key);
            if (it != m_local_cache.end()) {
                it->second.last_accessed = m_access_counter;
                return receipt_lock<T>(it->second.value);
            }
            return receipt_lock<T>();
        }
    }

private:
    struct CacheEntry {
        std::shared_ptr<T> value;
        uint64_t last_accessed = 0;
    };

    bool m_force_local = false;
    size_t m_max_capacity = 1000;

    SPBasicSuite* m_pica = nullptr;
    const AEGP_ComputeCacheSuite1* m_suite_ptr = nullptr;
    std::string m_class_id;
    AEGP_ComputeCacheCallbacks m_callbacks { };

    std::map<AEGP_GUID, CacheEntry, GuidCompare> m_local_cache;
    std::mutex m_local_mutex;
    uint64_t m_access_counter = 0;
};

} // namespace aetk::effect
