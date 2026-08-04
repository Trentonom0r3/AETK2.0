# 👖 AETK_SmartyPants Sample Plugin

`AETK_SmartyPants` is a developer sample demonstrating parameter supervision, UI change callbacks, and dynamic parameter updates (enabling/disabling, showing/hiding parameter UI controls) in After Effects.

## 🛠️ Developer Reference

### Core Concepts Demonstrated
* **UI Supervision**: Intercepts UI changes and intercepts param validation calls when the user interacts with parameters.
* **Dynamic Control Toggles**: Hides or disables dependent sliders or groups based on checkbox inputs or dropdown choices.
* **Param Update Hooks**: Forces redraws of parameter layouts inside `on_ui_update`.

### Key API Usages
* `ctx.enable_param_supervision()`
* `user_changed_param_context::param_index`
* `param_utils->PF_UpdateParamUI`
