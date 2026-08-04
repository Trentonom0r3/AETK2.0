# Backward Compatibility & Versioning in AETK 2.0

When updating an After Effects plugin, ensuring that projects saved with older versions of your plugin continue to load correctly is critical. This guide documents the core After Effects SDK compatibility principles, potential pitfalls, and how **AETK 2.0** automates safety nets under the hood.

---

## 1. Core AE SDK Principles & Pitfalls

### A. The `PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS` Flag
When adding new parameters to an existing plugin:
* **The Rule**: Always set the `PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS` (bit value `0x80`) flag. When After Effects opens an older project that doesn't contain this new parameter, it instructs the host to initialize it using the parameter's **`value`** field (your default value) instead of failing or using uninitialized values.
* **The Pitfall**: The bit value `0x80` is shared between `PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS` and `PF_ParamFlag_LAYER_PARAM_IS_TRACKMATTE`. 
  > [!WARNING]
  > **Do not apply `0x80` to Layer parameters.** Applying this flag to a layer parameter will cause After Effects to treat it as a trackmatte flag instead of a versioning fallback.

### B. Stable Parameter Disk IDs
After Effects matches parameters saved in a project file to your plugin's parameters using unique **Disk IDs** (stored in `def_p.uu.id`).
* **The Rule**: Always explicitly define parameter IDs starting from **`1`** (never `0`, which is reserved for the default input layer).
* **The Pitfall**: If you don't explicitly assign unique Disk IDs, After Effects auto-assigns sequential IDs. If you later reorder parameters or insert a new parameter in the middle of your layout, the auto-assigned IDs will shift. This will map existing keyframes to incorrect parameters, corrupting the saved project when opened.

### C. Manual Version Serialization
* **The Rule**: The After Effects SDK does not automatically record your plugin's version number in project files. For complex migration scenarios (e.g., changes to internal data structures or layout refactoring), manually serialize a version number into custom blocks (like `sequence_data` or `arb_data`) or track it in a hidden parameter. Parse this version during initialization to run migration logic.

---

## 2. How AETK 2.0 Solves Compatibility

AETK 2.0 is architected to eliminate these manual bookkeeping steps, preventing accidental versioning mismatches:

### A. Automatic Hashed Disk IDs
AETK handles the setup of `uu.id` automatically within [param_setup.hpp](file:///d:/dev/Projects/Repos/AETK2.0/include/aetk/effect/params/param_setup.hpp):
```cpp
def_p.uu.id = static_cast<A_long>(core::hash_string(name));
```
By generating the Disk ID from a stable compile-time FNV-1a hash of the parameter's **`name`** string, the Disk ID remains completely stable across updates, regardless of parameter reordering, insertion, or deletion.

### B. Decoupled Key-Based Lookups
Instead of querying parameters by their layout index (which shifts when parameters are added or reordered), AETK registers stable enum or string keys:
```cpp
float radius = ctx.float_val(param_id::radius);
```
AETK dynamically maps the key to the correct index at runtime, ensuring that backend code doesn't require index updates when the UI is refactored.

### C. Direct Flag Support in Builders
AETK's parameter builders easily accept standard SDK flags. When adding a new parameter in an update, specify the backward compatibility flag directly in the builder:
```cpp
ctx.add_slider("New Option", 0.0f, 100.0f, 50.0f, PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS)
   .set_key<param_id>(param_id::new_option);
```

---

## 3. Checklist for Updating Parameters

Use this checklist when deploying an update for an AETK plugin:

- [ ] **Are there new parameters?** Apply `PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS` to the new parameter builders.
- [ ] **Is it a Layer parameter?** Ensure you **do not** set the `USE_VALUE_FOR_OLD_PROJECTS` flag (leave flags default).
- [ ] **Is the parameter name unique?** Since AETK uses the parameter name for hashing Disk IDs, ensuring name uniqueness is required to avoid ID collisions.
- [ ] **Do you have custom sequence data?** If the memory layout of your `sequence_data` struct changes, serialize a version number in `sequence_data` and write a migration block inside `on_sequence_flatten` / `on_sequence_reset`.
