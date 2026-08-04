# 🍬 AETK_Sweetie AEGP Sample Plugin

`AETK_Sweetie` is a developer sample demonstrating AEGP (After Effects General Plugin) lifecycle hooks, custom automation, and dynamic menu item registration.

## 🛠️ Developer Reference

### Core Concepts Demonstrated
* **AEGP Menu Hooks**: Registers custom commands into After Effects' main application menus (e.g. Utility/File menu).
* **Automation callbacks**: Listens to user interactions and manipulates project items (composition creations, layer property shifts) procedurally.
* **Basic Suite Access**: Acquires AEGP-specific suites (ItemSuite, CompSuite, LayerSuite) type-safely.

### Key API Usages
* `AEGP_RegisterCommand`
* `AEGP_SuiteHandler`
* AEGP entry point hook registrations.
