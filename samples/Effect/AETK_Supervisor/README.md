# 👁️ AETK_Supervisor Sample Plugin

`AETK_Supervisor` is a developer sample demonstrating low-level parameter supervision and custom message-routing switches in After Effects.

## 🛠️ Developer Reference

### Core Concepts Demonstrated
* **Low-level Message Supervision**: Safely intercepts and overrides standard host message command events.
* **Synchronized Values**: Shows how to synchronize parameter changes immediately across thread queues to avoid rendering outdated frames.
* **Supervised Buttons**: Demonstrates linking a custom button callback to parameter updates.

### Key API Usages
* `PF_Cmd_USER_CHANGED_PARAM` hook overrides.
* `PF_Cmd_UPDATE_PARAMS_UI` hook overrides.
* Parameter registration flags setup.
