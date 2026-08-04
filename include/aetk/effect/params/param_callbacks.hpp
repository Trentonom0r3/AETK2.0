#pragma once

#include <functional>
#include <unordered_map>
#include <string>
#include <aetk/effect/params/arb_traits.hpp>

namespace aetk::effect {

struct user_changed_param_context;

/**
 * @brief Type alias for parameter change callbacks.
 * 
 * @note <b>AE SDK Paradigm Shift:</b> Replaces naked function pointers with type-safe `std::function` signatures.
 *
 * @warning <b>Memory & Lifecycles:</b> None.
 */
using param_change_callback_t = std::function<void(const user_changed_param_context&)>;

/**
 * @brief Static registry for parameter callbacks per plugin class.
 * 
 * @details This completely eliminates switch statements inside PF_Cmd_USER_CHANGED_PARAM.
 *
 * @note <b>AE SDK Paradigm Shift:</b> In the raw SDK, handling parameter change events requires building large, nested `switch (param_index)` structures inside `PF_Cmd_USER_CHANGED_PARAM`, which is highly error-prone and hard to maintain. `aetk::effect::param_callback_registry` provides an elegant, static map-based callback registry that binds parameter names/indices to type-safe functional callbacks (`param_change_callback_t`), executing dispatches instantly without bulky branching.
 *
 * @warning <b>Memory & Lifecycles:</b> The registry stores `std::function` callbacks inside global static variables. These map references must be initialized during setup registration and remain valid for the lifespan of the plugin process.
 *
 * @tparam PluginClass The specific plugin subclass executing the registry.
 */
template <typename PluginClass>
struct param_callback_registry {
    /// Maps parameter index to its callback.
    static inline std::unordered_map<int, param_change_callback_t> callbacks_by_index;
    
    /// Maps parameter name to its index.
    static inline std::unordered_map<std::string, int> index_by_name;
    
    /// Maps custom parameter key string to its index.
    static inline std::unordered_map<std::string, int> index_by_key;
    
    /// Maps custom integer parameter key to its index.
    static inline std::unordered_map<int32_t, int> index_by_int_key;
    
    /**
     * @brief Registers a parameter change callback by name/index.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Automates event binding.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param name Unique string layout name.
     * @param index Parameter index identifier.
     * @param cb Target change callback.
     */
    static void register_callback(const std::string& name, int index, param_change_callback_t cb) {
        index_by_name[name] = index;
        if (cb) {
            callbacks_by_index[index] = std::move(cb);
        }
    }

    /**
     * @brief Registers a custom string key mapping to a parameter index.
     *
     * @param key Unique custom string key.
     * @param index Parameter index.
     */
    static void register_key(const std::string& key, int index) {
        index_by_key[key] = index;
    }

    /**
     * @brief Registers a custom integer/enum key mapping to a parameter index.
     *
     * @param key Unique custom integer/enum key.
     * @param index Parameter index.
     */
    static void register_int_key(int32_t key, int index) {
        index_by_int_key[key] = index;
    }

    /**
     * @brief Invokes registered callbacks for a specific index.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Direct index dispatch.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @param index Parameter index to query.
     * @param ctx Active user change context.
     */
    static void invoke(int index, const user_changed_param_context& ctx) {
        auto it = callbacks_by_index.find(index);
        if (it != callbacks_by_index.end() && it->second) {
            it->second(ctx);
        }
    }
};

/**
 * @brief Dispatcher and registry for custom arbitrary data callback operations.
 * 
 * @details Handlers for custom arbitrary parameters (like `PF_Cmd_ARBITRARY_CALLBACK`) require implementing an ugly, unified callback function that branches internally using `PF_Arbitrary_NEW_FUNC`, `PF_Arbitrary_DISPOSE_FUNC`, `PF_Arbitrary_COPY_FUNC`, etc. `aetk::effect::arb_data_registry` modernizes this completely by consolidating these operations into an automated dispatch router template that leverages `aetk::effect::arb_traits` to handle instantiation, deep-copy cloning, stream flattening, frame interpolation, and byte-wise comparison out-of-the-box.
 *
 * @note <b>AE SDK Paradigm Shift:</b> Handlers for custom arbitrary parameters (like `PF_Cmd_ARBITRARY_CALLBACK`) require implementing an ugly, unified callback function that branches internally using `PF_Arbitrary_NEW_FUNC`, `PF_Arbitrary_DISPOSE_FUNC`, `PF_Arbitrary_COPY_FUNC`, etc. `aetk::effect::arb_data_registry` modernizes this completely by consolidating these operations into an automated dispatch router template that leverages `aetk::effect::arb_traits` to handle instantiation, deep-copy cloning, stream flattening, frame interpolation, and byte-wise comparison out-of-the-box.
 *
 * @warning <b>Memory & Lifecycles:</b> Allocates dynamic handles on After Effects' host memory allocator via `host_new_handle`. Unlocks and disposes handles cleanly within each callback phase to avoid massive memory leaks.
 */
struct arb_data_registry {
    /// Maps the 16-bit parameter ID to a dispatch function.
    using dispatch_t = PF_Err (*)(PF_InData*, PF_OutData*, PF_ArbParamsExtra*);
    
    /// Static collection of registered dispatch handlers.
    static inline std::unordered_map<A_short, dispatch_t> dispatchers;

    /// Maps 16-bit parameter ID to a custom interpolator.
    using interp_func_t = std::function<void(void*, const void*, const void*, double)>;
    
    /// Static collection of custom interpolation functions.
    static inline std::unordered_map<A_short, interp_func_t> custom_interpolators;

    /**
     * @brief Registers an arbitrary data type by unique ID.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Replaces multi-branch C function callbacks with direct template-based registrations.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @tparam T The custom arbitrary data type.
     * @param id The unique 16-bit parameter identifier.
     */
    template <typename T>
    static void register_type(A_short id) {
        dispatchers[id] = &arb_data_registry::handle_arbitrary<T>;
    }

    /**
     * @brief Registers a custom interpolator for a type.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Seamless functional interpolation hooks.
     *
     * @warning <b>Memory & Lifecycles:</b> None.
     *
     * @tparam T The custom arbitrary data type.
     * @param id The unique 16-bit parameter identifier.
     * @param func Target interpolation function.
     */
    template <typename T>
    static void register_interpolator(A_short id, std::function<void(T*, const T*, const T*, double)> func) {
        custom_interpolators[id] = [func](void* dst, const void* left, const void* right, double t) {
            func(static_cast<T*>(dst), static_cast<const T*>(left), static_cast<const T*>(right), t);
        };
    }

    /**
     * @brief Invokes the dynamic dispatcher for a raw arbitrary callback.
     *
     * @note <b>AE SDK Paradigm Shift:</b> Maps raw `PF_Cmd_ARBITRARY_CALLBACK` dispatches.
     *
     * @warning <b>Memory & Lifecycles:</b> Allocates and unlocks intermediate handles.
     *
     * @param in_data Input struct parameter.
     * @param out_data Output struct parameter.
     * @param extra Raw arbitrary callback details.
     * @return Standard `PF_Err` result.
     */
    static PF_Err invoke(PF_InData* in_data, PF_OutData* out_data, PF_ArbParamsExtra* extra) {
        auto it = dispatchers.find(extra->id);
        if (it != dispatchers.end()) {
            return it->second(in_data, out_data, extra);
        }
        return PF_Err_NONE;
    }

private:
    template <typename T>
    static PF_Err handle_arbitrary(PF_InData* in_data, PF_OutData* out_data, PF_ArbParamsExtra* extra) {
        auto utils = in_data->utils;
        switch (extra->which_function) {
            case PF_Arbitrary_NEW_FUNC: {
                PF_Handle h = utils->host_new_handle(sizeof(T));
                if (!h) return PF_Err_OUT_OF_MEMORY;
                T* ptr = reinterpret_cast<T*>(utils->host_lock_handle(h));
                if (ptr) {
                    arb_traits<T>::init(ptr);
                    utils->host_unlock_handle(h);
                }
                *extra->u.new_func_params.arbPH = h;
                break;
            }
            case PF_Arbitrary_DISPOSE_FUNC: {
                PF_Handle h = extra->u.dispose_func_params.arbH;
                if (h) {
                    T* ptr = reinterpret_cast<T*>(utils->host_lock_handle(h));
                    if (ptr) {
                        arb_traits<T>::dispose(ptr);
                        utils->host_unlock_handle(h);
                    }
                    utils->host_dispose_handle(h);
                }
                break;
            }
            case PF_Arbitrary_COPY_FUNC: {
                PF_Handle src = extra->u.copy_func_params.src_arbH;
                if (!src) return PF_Err_INTERNAL_STRUCT_DAMAGED;
                PF_Handle dst = utils->host_new_handle(sizeof(T));
                if (!dst) return PF_Err_OUT_OF_MEMORY;
                
                T* src_ptr = reinterpret_cast<T*>(utils->host_lock_handle(src));
                T* dst_ptr = reinterpret_cast<T*>(utils->host_lock_handle(dst));
                if (src_ptr && dst_ptr) {
                    arb_traits<T>::copy(dst_ptr, src_ptr);
                }
                if (src_ptr) utils->host_unlock_handle(src);
                if (dst_ptr) utils->host_unlock_handle(dst);
                
                *extra->u.copy_func_params.dst_arbPH = dst;
                break;
            }
            case PF_Arbitrary_FLAT_SIZE_FUNC: {
                PF_Handle h = extra->u.flat_size_func_params.arbH;
                if (!h) return PF_Err_INTERNAL_STRUCT_DAMAGED;
                T* src_ptr = reinterpret_cast<T*>(utils->host_lock_handle(h));
                if (src_ptr) {
                    *extra->u.flat_size_func_params.flat_data_sizePLu = static_cast<A_u_long>(arb_traits<T>::flat_size(src_ptr));
                    utils->host_unlock_handle(h);
                } else {
                    return PF_Err_INTERNAL_STRUCT_DAMAGED;
                }
                break;
            }
            case PF_Arbitrary_FLATTEN_FUNC: {
                PF_Handle h = extra->u.flatten_func_params.arbH;
                if (!h) return PF_Err_INTERNAL_STRUCT_DAMAGED;
                T* src_ptr = reinterpret_cast<T*>(utils->host_lock_handle(h));
                if (src_ptr) {
                    arb_traits<T>::flatten(src_ptr, extra->u.flatten_func_params.flat_dataPV, extra->u.flatten_func_params.buf_sizeLu);
                    utils->host_unlock_handle(h);
                }
                break;
            }
            case PF_Arbitrary_UNFLATTEN_FUNC: {
                PF_Handle h = utils->host_new_handle(sizeof(T));
                if (!h) return PF_Err_OUT_OF_MEMORY;
                T* dst_ptr = reinterpret_cast<T*>(utils->host_lock_handle(h));
                if (dst_ptr) {
                    arb_traits<T>::unflatten(dst_ptr, extra->u.unflatten_func_params.flat_dataPV, extra->u.unflatten_func_params.buf_sizeLu);
                    utils->host_unlock_handle(h);
                }
                *extra->u.unflatten_func_params.arbPH = h;
                break;
            }
            case PF_Arbitrary_INTERP_FUNC: {
                PF_Handle left_h = extra->u.interp_func_params.left_arbH;
                PF_Handle right_h = extra->u.interp_func_params.right_arbH;
                if (!left_h || !right_h) return PF_Err_INTERNAL_STRUCT_DAMAGED;
                
                PF_Handle dst_h = utils->host_new_handle(sizeof(T));
                if (!dst_h) return PF_Err_OUT_OF_MEMORY;

                T* left_ptr = reinterpret_cast<T*>(utils->host_lock_handle(left_h));
                T* right_ptr = reinterpret_cast<T*>(utils->host_lock_handle(right_h));
                T* dst_ptr = reinterpret_cast<T*>(utils->host_lock_handle(dst_h));

                if (left_ptr && right_ptr && dst_ptr) {
                    auto it = custom_interpolators.find(extra->id);
                    if (it != custom_interpolators.end() && it->second) {
                        it->second(dst_ptr, left_ptr, right_ptr, extra->u.interp_func_params.tF);
                    } else {
                        arb_traits<T>::interpolate(dst_ptr, left_ptr, right_ptr, extra->u.interp_func_params.tF);
                    }
                }

                if (left_ptr) utils->host_unlock_handle(left_h);
                if (right_ptr) utils->host_unlock_handle(right_h);
                if (dst_ptr) utils->host_unlock_handle(dst_h);

                *extra->u.interp_func_params.interpPH = dst_h;
                break;
            }
            case PF_Arbitrary_COMPARE_FUNC: {
                PF_Handle a_h = extra->u.compare_func_params.a_arbH;
                PF_Handle b_h = extra->u.compare_func_params.b_arbH;
                if (!a_h || !b_h) return PF_Err_INTERNAL_STRUCT_DAMAGED;
                
                T* a_ptr = reinterpret_cast<T*>(utils->host_lock_handle(a_h));
                T* b_ptr = reinterpret_cast<T*>(utils->host_lock_handle(b_h));
                
                if (a_ptr && b_ptr) {
                    if (arb_traits<T>::compare(a_ptr, b_ptr)) {
                        *extra->u.compare_func_params.compareP = PF_ArbCompare_EQUAL;
                    } else {
                        *extra->u.compare_func_params.compareP = PF_ArbCompare_NOT_EQUAL;
                    }
                }
                if (a_ptr) utils->host_unlock_handle(a_h);
                if (b_ptr) utils->host_unlock_handle(b_h);
                break;
            }
            case PF_Arbitrary_PRINT_SIZE_FUNC: {
                PF_Handle h = extra->u.print_size_func_params.arbH;
                if (!h) return PF_Err_INTERNAL_STRUCT_DAMAGED;
                T* ptr = reinterpret_cast<T*>(utils->host_lock_handle(h));
                if (ptr) {
                    *extra->u.print_size_func_params.print_sizePLu = static_cast<A_u_long>(arb_traits<T>::print_size(ptr));
                    utils->host_unlock_handle(h);
                } else {
                    return PF_Err_INTERNAL_STRUCT_DAMAGED;
                }
                break;
            }
            case PF_Arbitrary_PRINT_FUNC: {
                PF_Handle h = extra->u.print_func_params.arbH;
                if (h) {
                    T* ptr = reinterpret_cast<T*>(utils->host_lock_handle(h));
                    if (ptr) {
                        arb_traits<T>::print(ptr, extra->u.print_func_params.print_bufferPC, extra->u.print_func_params.print_sizeLu);
                        utils->host_unlock_handle(h);
                    }
                }
                break;
            }
            case PF_Arbitrary_SCAN_FUNC: {
                PF_Handle h = utils->host_new_handle(sizeof(T));
                if (!h) return PF_Err_OUT_OF_MEMORY;
                T* dst_ptr = reinterpret_cast<T*>(utils->host_lock_handle(h));
                if (dst_ptr) {
                    arb_traits<T>::init(dst_ptr);
                    std::string str(extra->u.scan_func_params.bufPC, extra->u.scan_func_params.bytes_to_scanLu);
                    if (arb_traits<T>::scan(dst_ptr, str.c_str())) {
                        utils->host_unlock_handle(h);
                        *extra->u.scan_func_params.arbPH = h;
                    } else {
                        utils->host_unlock_handle(h);
                        utils->host_dispose_handle(h);
                        return PF_Err_CANNOT_PARSE_KEYFRAME_TEXT;
                    }
                } else {
                    utils->host_dispose_handle(h);
                    return PF_Err_OUT_OF_MEMORY;
                }
                break;
            }
            default:
                break;
        }
        return PF_Err_NONE;
    }
};

} // namespace aetk::effect
