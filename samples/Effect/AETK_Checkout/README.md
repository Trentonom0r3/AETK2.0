# 📥 AETK_Checkout Sample Plugin

`AETK_Checkout` is a developer sample demonstrating the basic parameter checkout mechanics of the AETK 2.0 framework.

## 🛠️ Developer Reference

### Core Concepts Demonstrated
* **Classic Input Checkout**: Shows how to safely check out pixels from target layers during render passes.
* **Basic Color Scaling**: Modulates pixel channels based on slider parameter values.
* **RAII Scoping**: Clean memory lifecycles using AETK's automatically managed wrappers.

### Key API Usages
* `ctx.checkout_pixels(index)`
* `smart_world::copy_to(dest)`
* Dynamic slider parameter queries.
