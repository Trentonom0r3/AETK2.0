# 🏁 AETK_TestSuite Verification Plugin

`AETK_TestSuite` is a built-in automated test suite plugin used to verify the mathematical and logical correctness of AETK's core APIs (such as allocation, conversion, swizzling, vector drawing, and tensor DMA round-trips).

## 🛠️ Developer Reference

### Test Coverage Includes:
1. **`smart_world` Allocation & Clearing**: Verifies transparent clearing of `smart_world::zeros` and dimension correctness of `zeros_like` / `empty`.
2. **`aegp_world` Interop**: Checks AEGP and Effect world conversion constructors.
3. **Format Conversions**: Verifies 8-bit, 16-bit, and 32-bit float color mappings and bitwise conversions.
4. **`pixel_accessor` Accuracy**: Evaluates layout swizzles (ARGB vs. BGRA) and value bounds verification.
5. **Cloning**: Tests deep-copy cloning safety (`.clone()`).
6. **Tensor Pipeline**: Verifies multi-dimensional view indexing, pinned host memory allocations, and CUDA DMA transfers.
