Currenty, stores;

```
    PF_EffectWorld* m_world = nullptr;
    PF_InData* m_in_data = nullptr; // Full context (preferred)
    PF_ProgPtr m_effect_ref = nullptr; // Bare callback ref (fallback)
    ownership m_ownership = ownership::NONE;
    PF_SmartRenderCallbacks* m_cb = nullptr; // For LAYER_PIXELS cleanup
    A_long m_index = -1; // Checkout index
    PF_ParamDef m_owned_def { }; // Saved checkout param definition for classic mode
    mutable PF_PixelFormat m_cached_format = PF_PixelFormat_INVALID;
    bool m_is_bgra = false; ///< True when host is Premiere Pro (BGRA byte order)
    void* m_raw_data = nullptr;
    size_t m_raw_size = 0;
    int m_device_index = 0;
```

We ca drop it down to

```
    PF_EffectWorld* m_world = nullptr;
    PF_InData* m_in_data = nullptr; // Full context (preferred)
    ownership m_ownership = ownership::NONE;
    PF_SmartRenderCallbacks* m_cb = nullptr; // For LAYER_PIXELS cleanup
    A_long m_index = -1; // Checkout index
    PF_ParamDef m_owned_def { }; //for classic mode
```

The other stuff can be accessed via either the in_data pointer and callbacks, or utilizing suites and the in_data pointer. This just cleans things up a little bit.

Unify copy and To variants, or move to private with single public API. refactor to have public -> private, top down  remove .from.

Where possible refactor and condense code down to a smaller header.
user visitor pattern for fill. remove clone_view, keep clone.

add cpp20 attributes to label cpu only calls. - compile time so it helps devs, but we'll also add checks to the funcs themselves.

What functions share calls? I'm fine with a full refactor with multiple helper calls, if it makes the header look cleaner in general It is definitely doing a lot (and that is good!) but it is also 3k lines.

unify ".data" to work for cpu and gpu automatically - same concept, should do the same thing.

```cpp
class Color;
class smart_world {
    PF_EffectWorld* m_world = nullptr;
    PF_InData* m_in_data = nullptr; // Full context (preferred)
    ownership m_ownership = ownership::NONE;
    PF_SmartRenderCallbacks* m_cb = nullptr; // For LAYER_PIXELS cleanup
    A_long m_id = -1; // Checkout id
    PF_ParamDef m_owned_def { }; //for classic mode
     mutable PF_PixelFormat m_cached_format = PF_PixelFormat_INVALID;
public:
    // templated constructors and destructors, copy and move semantics.
    // ideally, we remove "from_raw" "from_etc" and have ctors do the work.
    smart_world(args...) {};
    ~smart_world() {};
    //small helpers
    smart_world to(args...) const;
    smart_world clone() const;
    smart_world copy_to(args...);
    void fill(args...);
    void blend(args...);
    void convolve(args...);
    Color sample(samplekind, args..); //currently only bilinear, but will add nnf ++

    //not sure how to write it, but instead of "get pixel or set pixel", user operators? We should also utilize the sampling class in AETK for best performance. 
    //
    PF_InData* in_data() const {return m_in_data; }
    PF_EffectWorld* world() const {return m_world; }
    A_long id() const {return m_id; }
    PF_SmartRenderCallbacks* cb() const {return m_cb; }
    bool bgra() const {return m_is_bgra; }

    void set_bgra(bool is_bgra) { m_is_bgra = is_bgra; }
    void set_ownership(ownership ownership) { m_ownership = ownership; }
    void set_id(A_long id) { m_id = id; }
    void set_cb(PF_SmartRenderCallbacks* cb) { m_cb = cb; }
    void set_format(PF_PixelFormat format) { m_cached_format = format; }
    int device_index() const { return m_device_index; }
    void set_device_index(int device_index) { m_device_index = device_index; }

    Device device() const;

    void* data() const;
    size_t size() const;
    int height() const;
    int width() const;
    PF_PixelFormat format() const;
    //rowbytes/stride. 
    size_t stride() const;
    ownership ownership() const;
    
    //we would still have all the same functionality of smart_world, but public facing api would be similar to above (I may not have everything perfect, this is pseudocode). smart_world (current) would remove and refactor all old methods, combine as necessary, and move to a private namespace to separate from the public user API (though they still will see all if they scroll, lol).
};
```

};

```
