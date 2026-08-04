#pragma once
#include <cstdint>

namespace aetk::effect {

enum class device_kind : std::uint8_t {
    cpu,          ///< Standard CPU pageable memory (`new` / `malloc`)
    cpu_pinned,   ///< CPU page-locked (pinned) memory — AE-tracked via `PF_GPUDeviceSuite1::AllocateHostMemory`
    cuda,         ///< CUDA Device VRAM — AE-tracked via `PF_GPUDeviceSuite1::AllocateDeviceMemory`
    metal,        ///< Metal Buffer (Reserved)
    opencl,       ///< OpenCL Buffer (Reserved)
    d3d12         ///< DirectX 12 Resource (Reserved)
};

struct device {
    device_kind kind;
    int index = 0;

    device(device_kind k, int idx = 0) : kind(k), index(idx) {}
};

} // namespace aetk::effect
