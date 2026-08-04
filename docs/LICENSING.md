# AETK 2.0 Licensing Architecture & Custom Implementation Guide

This document explains the licensing framework in **AETK 2.0**, how the default licensing stub functions out of the box, and how developers can integrate custom licensing logic or commercial licensing providers (e.g. aescripts + aeplugins) in their own plugin projects.

## 1. Overview & Architectural Philosophy

AETK 2.0 provides a lightweight, flexible licensing interface located in `include/aetk/effect/licensing.hpp`.

The framework separates **licensing execution hooks** from **licensing business logic**:

- **Default Mode (Unlicensed / Free)**: Out of the box, AETK executes plugins with zero trial restrictions, no serial checks, and no watermark rendering.
- **Provider / Custom Mode**: Developers building commercial plugins can plug in their own validation routines, trial period checks, server activations, or 3rd-party licensing frameworks.

---

## 2. Default Licensing Stub Behavior

When no external licensing defines are passed during compilation, AETK uses the default stub implementation:

```cpp
namespace aetk::effect::licensing {

// Intercepts commands (e.g., PF_Cmd_ARBITRARY_CALLBACK / Options Button)
inline PF_Err check_and_intercept(PF_Cmd, PF_InData*, PF_OutData*) {
    return PF_Err_NONE;
}

// Configures the 'Register' options button name in AE/Premiere
inline void setup_ui_button(PF_InData*) {}

// Displays the registration dialog modal
inline void show_registration_dialog(PF_InData*, const char* = "1.0.0") {}

// Appends registration details to the native AE 'About' dialog
inline void append_about_info(PF_InData*, PF_OutData* out_data, const char* plugin_name, const char* desc) {
    if (out_data) {
        snprintf(out_data->return_msg, sizeof(out_data->return_msg), "%s\r%s", plugin_name, desc);
    }
}

// Returns true if render output should be watermarked (e.g., in trial mode)
inline bool should_draw_watermark() {
    return false; // Default: No watermark
}

} // namespace aetk::effect::licensing
```

---

## 3. Commercial Licensing Integration (aescripts + aeplugins, Lemonsqueezy, etc.)

AETK 2.0 is **licensing-provider agnostic by design**. The public framework repository includes zero 3rd-party vendor code or proprietary header requirements.

### For `aescripts + aeplugins` Vendors

- AETK includes a pre-built adapter header for the official aescripts C++ framework.
- **Availability**: The aescripts licensing integration stub for AETK is available upon request and confirmation of vendor status.
- Keep your product keys, private salt numbers, and vendor SDK headers in a private header (`licensing_private.hpp`), which is excluded from public Git tracking.

---

## 4. Implementing Custom Licensing (Keygen / Server Check / Watermarking)

To implement your own custom licensing logic (e.g. node-locked keys, online activation, or trial watermarking):

1. **Check License Status in `on_global_setup` / `on_render`**:
   Query your licensing state before rendering frames:

   ```cpp
   if (aetk::effect::licensing::should_draw_watermark()) {
       // Apply red cross or diagonal line watermark over output frame
   }
   ```

2. **Intercept Options / Register Button**:
   In your plugin's `on_global_setup`, enable the options button:

   ```cpp
   ctx.enable_options_button();
   ```

   When the user clicks the "Register..." or "Options" button in the Effect Controls panel, After Effects invokes `PF_Cmd_DO_DIALOG` / `PF_Cmd_USER_CHANGED_PARAM`. Delegate this to `show_registration_dialog(...)` to display your custom activation modal.

3. **Trial Watermarking Pattern**:

   ```cpp
   void render_watermark(smart_world& output_world) {
       // Draw a custom watermark pattern or text on top of the rendered frame
   }
   ```

---

## 5. Security Best Practices

- **Keep Secret Keys Private**: Never commit private licensing salt values, decryption keys, or private SDK libraries to public repositories.
- **Use Local Ignore Files**: Keep local configurations in `licensing_private.hpp` or environment variables.
- **Static Link CRT**: Always link the static MSVC runtime (`/MT` on Windows) so licensing hooks operate deterministically without DLL dependency conflicts inside Adobe host applications.
