#include "OrtEngine.h"
#include <cassert>
#include <numeric>
#include <sstream>


#ifdef _WIN32
#include <d3d12.h>
#include <dml_provider_factory.h>
#endif

#include <aetk/core/log.hpp>
#include <aetk/ui/message.hpp>

#ifdef _WIN32
#include <windows.h>
static void DebugLog(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OutputDebugStringA("[OrtEngine] ");
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
    ::aetk::core::logger::instance().log(::aetk::core::log_level::debug, std::string("[OrtEngine] ") + buf);
}
#else
#include <cstdio>
#include <cstdarg>
static void DebugLog(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    fprintf(stderr, "[OrtEngine] %s\n", buf);
    va_end(args);
    ::aetk::core::logger::instance().log(::aetk::core::log_level::debug, std::string("[OrtEngine] ") + buf);
}
#endif

extern std::string g_last_load_error;

namespace OrtEngine {

struct ModelSession {
    std::unique_ptr<Ort::Session> session;
    std::vector<TensorInfo> inputs;
    std::vector<TensorInfo> outputs;
    std::vector<std::string> input_names;
    std::vector<std::string> output_names;
    std::string device;
    std::unique_ptr<Ort::IoBinding> io_binding;
};

static std::unique_ptr<Ort::Env> g_env;
static std::unordered_map<std::string, std::shared_ptr<ModelSession>> g_sessions;
static std::mutex g_mutex;
static bool g_ort_available = false;
static bool g_cuda_supported = false;
static bool g_init_called = false;

#ifdef _WIN32
static std::wstring GetThisModuleDirectory() {
    HMODULE hMod = NULL;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&GetThisModuleDirectory, &hMod);
    if (!hMod) return L"";

    wchar_t path[MAX_PATH];
    GetModuleFileNameW(hMod, path, MAX_PATH);
    wchar_t* last_slash = wcsrchr(path, L'\\');
    if (last_slash) *last_slash = L'\0';
    return path;
}

static bool SafeInitApi() {
    __try {
        const OrtApi* api = nullptr;
        for (int ver = 17; ver >= 1; --ver) {
            api = OrtGetApiBase()->GetApi(ver);
            if (api) break;
        }
        if (!api) {
            api = OrtGetApiBase()->GetApi(1);
        }
        if (api) {
            Ort::InitApi(api);
            return true;
        }
        return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
#endif

void Init() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_init_called) return;
    g_init_called = true;

    if (!g_env) {
#ifdef _WIN32
        std::wstring dir = GetThisModuleDirectory();
        if (!dir.empty()) {
            // Check candidate dependency folders in order of precedence:
            // 1. framesmith_core next to plugin
            // 2. framesmith_core in parent directory (MediaCore\Framesmith\SWARM.aex -> MediaCore\framesmith_core)
            // 3. framesmith_core in grandparent directory (MediaCore\Framesmith\Effects\SWARM.aex -> MediaCore\framesmith_core)
            // 4. Default: fallback to plugin directory itself
            std::wstring resolved_deps_dir = dir;
            std::wstring candidates[] = {
                dir + L"\\framesmith_core",
                dir + L"\\..\\framesmith_core",
                dir + L"\\..\\..\\framesmith_core"
            };
            for (const auto& path : candidates) {
                DWORD attribs = GetFileAttributesW(path.c_str());
                if (attribs != INVALID_FILE_ATTRIBUTES && (attribs & FILE_ATTRIBUTE_DIRECTORY)) {
                    resolved_deps_dir = path;
                    break;
                }
            }

            SetDllDirectoryW(resolved_deps_dir.c_str());
            DebugLog("SetDllDirectoryW to: %ls", resolved_deps_dir.c_str());

            // Dynamically construct extra search paths to avoid hardcoded developer routes
            std::wstring extra_paths = resolved_deps_dir + L";";

            // Query system CUDA_PATH environment variable if available
            wchar_t env_buf[1024];
            DWORD env_len = GetEnvironmentVariableW(L"CUDA_PATH", env_buf, 1024);
            if (env_len > 0 && env_len < 1024) {
                extra_paths += std::wstring(env_buf) + L"\\bin;";
            }

            // Query Conda environment variable as developer fallback
            env_len = GetEnvironmentVariableW(L"CONDA_PREFIX", env_buf, 1024);
            if (env_len > 0 && env_len < 1024) {
                extra_paths += std::wstring(env_buf) + L"\\Library\\bin;";
            }

            // Append to process PATH variable
            wchar_t path_buf[32768];
            DWORD len = GetEnvironmentVariableW(L"PATH", path_buf, 32768);
            if (len > 0 && len < 32768) {
                std::wstring new_path = extra_paths + path_buf;
                SetEnvironmentVariableW(L"PATH", new_path.c_str());
                DebugLog("Appended dynamic dependencies search paths to process PATH: %ls", extra_paths.c_str());
            }
        }

        if (SafeInitApi()) {
            g_ort_available = true;
        } else {
            g_ort_available = false;
            std::string msg = "You are missing plugin dependencies. Please reinstall.";
            if (!g_last_load_error.empty()) {
                msg += "\n\nDetails:\n" + g_last_load_error;
            }
            aetk::ui::alert(msg.c_str(), "Framesmith SWARM");
            DebugLog("Failed to delay-load onnxruntime DLL: %s", g_last_load_error.c_str());
        }
#else
        Ort::InitApi();
        g_ort_available = true;
#endif

        if (g_ort_available) {
            try {
                g_env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "AETKSwarm");
                DebugLog("ORT environment initialized successfully");

                try {
                    auto providers = Ort::GetAvailableProviders();
                    bool found_cuda = false;
                    for (const auto& p : providers) {
                        if (p == "CUDAExecutionProvider") {
                            found_cuda = true;
                            break;
                        }
                    }
                    if (found_cuda) {
                        try {
                            Ort::SessionOptions test_opts;
                            OrtCUDAProviderOptions cuda_opts{};
                            cuda_opts.device_id = 0;
                            test_opts.AppendExecutionProvider_CUDA(cuda_opts);
                            g_cuda_supported = true;
                        } catch (const Ort::Exception& e) {
                            g_cuda_supported = false;
                            DebugLog("CUDA EP listed but failed to append: %s", e.what());
                        } catch (const std::exception& e) {
                            g_cuda_supported = false;
                            DebugLog("CUDA EP listed but failed to append: %s", e.what());
                        } catch (...) {
                            g_cuda_supported = false;
                            DebugLog("CUDA EP listed but failed to append");
                        }
                    }
                    DebugLog("ORT CUDA execution provider support status: %s", g_cuda_supported ? "AVAILABLE" : "NOT AVAILABLE");
                } catch (...) {
                    g_cuda_supported = false;
                    DebugLog("Failed to query ORT execution providers");
                }
            } catch (...) {
                g_ort_available = false;
                DebugLog("Failed to create ORT Environment");
            }
        }
    }
}

bool LoadModel(const std::string& model_key, const std::string& onnx_path,
    const std::string& device) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_ort_available) return false;

    if (!g_env) {
        return false;
    }

    auto it = g_sessions.find(model_key);
    if (it != g_sessions.end()) {
        DebugLog("Model '%s' already loaded — unloading first", model_key.c_str());
        g_sessions.erase(it);
    }

    try {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(4);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // Disable internal ONNX memory arenas. While arenas make inference slightly
        // faster, they hoard massive amounts of memory (often gigabytes) and prevent
        // the OS from reclaiming it. Disabling them forces ONNX to use standard
        // allocations, keeping AE's memory footprint much closer to baseline.
        opts.DisableMemPattern();
        opts.DisableCpuMemArena();

        std::string active_device = "cpu";
        if (device == "dml") {
#ifdef _WIN32
            try {
                auto status = OrtSessionOptionsAppendExecutionProvider_DML(opts, 0);
                if (status == nullptr) {
                    active_device = "dml";
                    DebugLog("Appended DirectML Execution Provider to model session");
                } else {
                    DebugLog("Failed to append DirectML Execution Provider, falling back "
                             "to CPU");
                }
            } catch (const std::exception& e) {
                DebugLog("Failed to append DirectML Execution Provider: %s, falling back "
                         "to CPU",
                    e.what());
            }
#else
            DebugLog("DirectML is not supported on non-Windows platforms");
#endif
        } else if (device == "cuda") {
            try {
                OrtCUDAProviderOptions cuda_opts { };
                cuda_opts.device_id = 0;
                opts.AppendExecutionProvider_CUDA(cuda_opts);
                active_device = "cuda";
                DebugLog("Appended CUDA Execution Provider to model session");
            } catch (const Ort::Exception& e) {
                g_cuda_supported = false;
                DebugLog(
                    "Ort::Exception appending CUDA EP: %s (code: %d)",
                    e.what(), e.GetOrtErrorCode());
            } catch (const std::exception& e) {
                g_cuda_supported = false;
                DebugLog(
                    "std::exception appending CUDA EP: %s",
                    e.what());
            }
        }

        // Convert path to wide string on Windows
#ifdef _WIN32
        int wlen = MultiByteToWideChar(CP_UTF8, 0, onnx_path.c_str(), -1, nullptr, 0);
        std::wstring wpath(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, onnx_path.c_str(), -1, &wpath[0], wlen);
        auto session = std::make_unique<Ort::Session>(*g_env, wpath.c_str(), opts);
#else
        auto session = std::make_unique<Ort::Session>(*g_env, onnx_path.c_str(), opts);
#endif

        auto ms = std::make_shared<ModelSession>();
        ms->device = active_device;

        Ort::AllocatorWithDefaultOptions alloc;

        // Input info
        size_t num_inputs = session->GetInputCount();
        for (size_t i = 0; i < num_inputs; i++) {
            TensorInfo ti;
            auto name = session->GetInputNameAllocated(i, alloc);
            ti.name = name.get();
            auto type_info = session->GetInputTypeInfo(i);
            auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
            ti.shape = tensor_info.GetShape();
            ti.dtype = tensor_info.GetElementType();
            ms->input_names.push_back(ti.name);
            ms->inputs.push_back(std::move(ti));
        }

        // Output info
        size_t num_outputs = session->GetOutputCount();
        for (size_t i = 0; i < num_outputs; i++) {
            TensorInfo ti;
            auto name = session->GetOutputNameAllocated(i, alloc);
            ti.name = name.get();
            auto type_info = session->GetOutputTypeInfo(i);
            auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
            ti.shape = tensor_info.GetShape();
            ti.dtype = tensor_info.GetElementType();
            ms->output_names.push_back(ti.name);
            ms->outputs.push_back(std::move(ti));
        }

        ms->session = std::move(session);
        try {
            ms->io_binding = std::make_unique<Ort::IoBinding>(*ms->session);
        } catch (const std::exception& e) {
            DebugLog(
                "Failed to create IoBinding for '%s': %s", model_key.c_str(), e.what());
        }
        g_sessions[model_key] = std::move(ms);

        DebugLog("Loaded '%s' from %s (%zu inputs, %zu outputs)", model_key.c_str(),
            onnx_path.c_str(), num_inputs, num_outputs);
        return true;

    } catch (const Ort::Exception& e) {
        DebugLog("Failed to load '%s': %s", model_key.c_str(), e.what());
        return false;
    } catch (const std::exception& e) {
        DebugLog("Failed to load '%s': %s", model_key.c_str(), e.what());
        return false;
    }
}

bool IsModelLoaded(const std::string& model_key, const std::string& device) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_ort_available) return false;
    auto it = g_sessions.find(model_key);
    if (it == g_sessions.end())
        return false;
    if (device.empty())
        return true;
    return it->second->device == device;
}

void UnloadModel(const std::string& model_key) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_sessions.erase(model_key);
}

std::vector<TensorInfo> GetInputInfo(const std::string& model_key) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_ort_available) return { };
    auto it = g_sessions.find(model_key);
    if (it == g_sessions.end())
        return { };
    return it->second->inputs;
}

std::vector<TensorInfo> GetOutputInfo(const std::string& model_key) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_ort_available) return { };
    auto it = g_sessions.find(model_key);
    if (it == g_sessions.end())
        return { };
    return it->second->outputs;
}

// Fast float32 to float16 conversion (bitwise)
static uint16_t float_to_half(float f) {
    uint32_t i;
    std::memcpy(&i, &f, 4);
    int s = (i >> 16) & 0x00008000;
    int e = ((i >> 23) & 0x000000ff) - (127 - 15);
    int m = i & 0x007fffff;
    if (e <= 0) {
        if (e < -10)
            return s;
        m = m | 0x00800000;
        int t = 14 - e;
        int a = (1 << (t - 1)) - 1;
        int b = (m >> t) & 1;
        m = (m + a + b) >> t;
        return s | m;
    } else if (e == 0xff - (127 - 15)) {
        if (m == 0)
            return s | 0x7c00;
        else {
            m >>= 13;
            return s | 0x7c00 | m | (m == 0);
        }
    } else {
        m = m + 0x00000fff + ((m >> 13) & 1);
        if (m & 0x00800000) {
            m = 0;
            e++;
        }
        if (e >= 31)
            return s | 0x7c00;
        return s | (e << 10) | (m >> 13);
    }
}

// Fast float16 to float32 conversion (bitwise)
static float half_to_float(uint16_t h) {
    int s = (h >> 15) & 0x00000001;
    int e = (h >> 10) & 0x0000001f;
    int m = h & 0x000003ff;
    uint32_t i;
    if (e == 0) {
        if (m == 0) {
            i = s << 31;
        } else {
            while (!(m & 0x00000400)) {
                m <<= 1;
                e--;
            }
            e++;
            m &= ~0x00000400;
            i = (s << 31) | ((e - 15 + 127) << 23) | (m << 13);
        }
    } else if (e == 31) {
        i = (s << 31) | (0xff << 23) | (m << 13);
    } else {
        i = (s << 31) | ((e - 15 + 127) << 23) | (m << 13);
    }
    float f;
    std::memcpy(&f, &i, 4);
    return f;
}

std::vector<IOTensor> Run(
    const std::string& model_key, const std::vector<IOTensor>& inputs) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_ort_available) return { };
    auto it = g_sessions.find(model_key);
    if (it == g_sessions.end()) {
        DebugLog("Run: model '%s' not loaded", model_key.c_str());
        return { };
    }

    auto& ms = *it->second;
    auto& session = *ms.session;

    try {
        Ort::MemoryInfo mem_info
            = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        std::vector<Ort::Value> input_tensors;
        std::vector<const char*> input_names;
        std::vector<std::vector<uint16_t>> fp16_inputs_storage;

        for (size_t i = 0; i < inputs.size(); i++) {
            auto& inp = inputs[i];
            int64_t total = 1;
            for (auto s : inp.shape)
                total *= s;

            ONNXTensorElementDataType expected_type = ms.inputs[i].dtype;
            if (expected_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16) {
                std::vector<uint16_t> half_data(total);
                for (int64_t j = 0; j < total; j++) {
                    half_data[j] = float_to_half(inp.data[j]);
                }
                fp16_inputs_storage.push_back(std::move(half_data));

                input_tensors.push_back(
                    Ort::Value::CreateTensor(mem_info, fp16_inputs_storage.back().data(),
                        total * sizeof(uint16_t), inp.shape.data(), inp.shape.size(),
                        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16));
            } else {
                input_tensors.push_back(Ort::Value::CreateTensor<float>(mem_info,
                    const_cast<float*>(inp.data.data()), static_cast<size_t>(total),
                    inp.shape.data(), inp.shape.size()));
            }
            input_names.push_back(inp.name.c_str());
        }

        std::vector<const char*> output_names;
        for (auto& name : ms.output_names) {
            output_names.push_back(name.c_str());
        }

        auto output_tensors = session.Run(Ort::RunOptions { nullptr }, input_names.data(),
            input_tensors.data(), input_tensors.size(), output_names.data(),
            output_names.size());

        std::vector<IOTensor> results;
        for (size_t i = 0; i < output_tensors.size(); i++) {
            IOTensor out;
            out.name = ms.output_names[i];

            auto& tensor = output_tensors[i];
            auto info = tensor.GetTensorTypeAndShapeInfo();
            out.shape = info.GetShape();

            int64_t total = 1;
            for (auto s : out.shape)
                total *= s;

            ONNXTensorElementDataType out_type = ms.outputs[i].dtype;
            if (out_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16) {
                const uint16_t* half_data = tensor.GetTensorData<uint16_t>();
                out.data.resize(total);
                for (int64_t j = 0; j < total; j++) {
                    out.data[j] = half_to_float(half_data[j]);
                }
            } else {
                const float* data = tensor.GetTensorData<float>();
                out.data.assign(data, data + total);
            }

            results.push_back(std::move(out));
        }

        return results;

    } catch (const Ort::Exception& e) {
        DebugLog("Run '%s' failed: %s", model_key.c_str(), e.what());
        return { };
    }
}

#ifdef _WIN32
bool BindD3D12Input(const std::string& model_key, const std::string& input_name,
    ID3D12Resource* resource, const std::vector<int64_t>& shape,
    ONNXTensorElementDataType dtype) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_ort_available) return false;
    auto it = g_sessions.find(model_key);
    if (it == g_sessions.end()) {
        DebugLog("BindD3D12Input: model '%s' not loaded", model_key.c_str());
        return false;
    }

    auto& ms = *it->second;
    if (!ms.io_binding) {
        DebugLog("BindD3D12Input: no IoBinding for model '%s'", model_key.c_str());
        return false;
    }

    try {
        // DirectML expects ID3D12Resource* passed as void*
        // with "DML" MemoryInfo name.
        Ort::MemoryInfo mem_info("DML", OrtAllocatorType::OrtDeviceAllocator, 0,
            OrtMemType::OrtMemTypeDefault);

        int64_t total = 1;
        for (auto s : shape)
            total *= s;
        size_t size_in_bytes = total
            * (dtype == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16 ? sizeof(uint16_t)
                                                              : sizeof(float));

        Ort::Value input_tensor = Ort::Value::CreateTensor(
            mem_info, resource, size_in_bytes, shape.data(), shape.size(), dtype);
        ms.io_binding->BindInput(input_name.c_str(), input_tensor);
        return true;
    } catch (const Ort::Exception& e) {
        DebugLog("BindD3D12Input failed: %s", e.what());
        return false;
    } catch (const std::exception& e) {
        DebugLog("BindD3D12Input failed: %s", e.what());
        return false;
    }
}
#endif

bool BindCUDAInput(const std::string& model_key, const std::string& input_name,
    void* cuda_device_ptr, size_t size_in_bytes, const std::vector<int64_t>& shape,
    ONNXTensorElementDataType dtype) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_ort_available) return false;
    auto it = g_sessions.find(model_key);
    if (it == g_sessions.end()) {
        DebugLog("BindCUDAInput: model '%s' not loaded", model_key.c_str());
        return false;
    }

    auto& ms = *it->second;
    if (!ms.io_binding) {
        DebugLog("BindCUDAInput: no IoBinding for model '%s'", model_key.c_str());
        return false;
    }

    try {
        Ort::MemoryInfo mem_info("Cuda", OrtAllocatorType::OrtDeviceAllocator, 0,
            OrtMemType::OrtMemTypeDefault);
        Ort::Value input_tensor = Ort::Value::CreateTensor(
            mem_info, cuda_device_ptr, size_in_bytes, shape.data(), shape.size(), dtype);
        ms.io_binding->BindInput(input_name.c_str(), input_tensor);
        return true;
    } catch (const Ort::Exception& e) {
        DebugLog("BindCUDAInput failed: %s", e.what());
        return false;
    } catch (const std::exception& e) {
        DebugLog("BindCUDAInput failed: %s", e.what());
        return false;
    }
}

std::vector<IOTensor> RunWithBinding(const std::string& model_key) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_ort_available) return { };
    auto it = g_sessions.find(model_key);
    if (it == g_sessions.end()) {
        DebugLog("RunWithBinding: model '%s' not loaded", model_key.c_str());
        return { };
    }

    auto& ms = *it->second;
    if (!ms.io_binding) {
        DebugLog("RunWithBinding: no IoBinding for model '%s'", model_key.c_str());
        return { };
    }

    try {
        // Bind outputs to CPU memory so ONNX Runtime copies output tensors back to host
        Ort::MemoryInfo cpu_mem_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        for (auto& name : ms.output_names) {
            ms.io_binding->BindOutput(name.c_str(), cpu_mem_info);
        }

        // Run with IoBinding
        ms.session->Run(Ort::RunOptions { nullptr }, *ms.io_binding);

        // Get the outputs
        std::vector<Ort::Value> output_tensors = ms.io_binding->GetOutputValues();
        std::vector<IOTensor> results;

        for (size_t i = 0; i < output_tensors.size(); i++) {
            IOTensor out;
            out.name = ms.output_names[i];

            auto& tensor = output_tensors[i];
            auto info = tensor.GetTensorTypeAndShapeInfo();
            out.shape = info.GetShape();

            int64_t total = 1;
            for (auto s : out.shape)
                total *= s;

            ONNXTensorElementDataType out_type = ms.outputs[i].dtype;
            if (out_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16) {
                const uint16_t* half_data = tensor.GetTensorData<uint16_t>();
                out.data.resize(total);
                for (int64_t j = 0; j < total; j++) {
                    out.data[j] = half_to_float(half_data[j]);
                }
            } else {
                const float* data = tensor.GetTensorData<float>();
                out.data.assign(data, data + total);
            }

            results.push_back(std::move(out));
        }

        // Clear bindings for next frame
        ms.io_binding->ClearBoundInputs();
        ms.io_binding->ClearBoundOutputs();

        return results;
    } catch (const Ort::Exception& e) {
        DebugLog("RunWithBinding '%s' failed: %s", model_key.c_str(), e.what());
        return { };
    } catch (const std::exception& e) {
        DebugLog("RunWithBinding '%s' failed: %s", model_key.c_str(), e.what());
        return { };
    }
}

std::vector<IOTensor> RunCUDAInference(const std::string& model_key,
                                       const std::string& input_name,
                                       void* cuda_device_ptr,
                                       size_t size_in_bytes,
                                       const std::vector<int64_t>& shape) {
    std::shared_ptr<ModelSession> session_ptr;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_ort_available) return { };
        auto it = g_sessions.find(model_key);
        if (it == g_sessions.end()) {
            DebugLog("RunCUDAInference: model '%s' not loaded", model_key.c_str());
            return { };
        }
        session_ptr = it->second;
    }

    try {
        Ort::IoBinding io_binding(*session_ptr->session);
        Ort::MemoryInfo cuda_mem("Cuda", OrtAllocatorType::OrtDeviceAllocator, 0, OrtMemTypeDefault);
        ONNXTensorElementDataType input_dtype = session_ptr->inputs.empty() ? ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT : session_ptr->inputs[0].dtype;
        Ort::Value input_tensor = Ort::Value::CreateTensor(
            cuda_mem, cuda_device_ptr, size_in_bytes, shape.data(), shape.size(), input_dtype);
        io_binding.BindInput(input_name.c_str(), input_tensor);

        Ort::MemoryInfo cpu_mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        for (const auto& name : session_ptr->output_names) {
            io_binding.BindOutput(name.c_str(), cpu_mem);
        }

        // Lock-free session execution across MFR threads
        session_ptr->session->Run(Ort::RunOptions { nullptr }, io_binding);

        std::vector<Ort::Value> output_tensors = io_binding.GetOutputValues();
        std::vector<IOTensor> results;

        for (size_t i = 0; i < output_tensors.size(); i++) {
            IOTensor out;
            out.name = session_ptr->output_names[i];
            auto info = output_tensors[i].GetTensorTypeAndShapeInfo();
            out.shape = info.GetShape();

            int64_t total = 1;
            for (auto s : out.shape)
                total *= s;

            ONNXTensorElementDataType out_type = session_ptr->outputs[i].dtype;
            if (out_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16) {
                const uint16_t* half_data = output_tensors[i].GetTensorData<uint16_t>();
                out.data.resize(total);
                for (int64_t j = 0; j < total; j++) {
                    out.data[j] = half_to_float(half_data[j]);
                }
            } else {
                const float* data = output_tensors[i].GetTensorData<float>();
                out.data.assign(data, data + total);
            }

            results.push_back(std::move(out));
        }

        return results;
    } catch (const Ort::Exception& e) {
        DebugLog("RunCUDAInference '%s' failed: %s", model_key.c_str(), e.what());
        return { };
    } catch (const std::exception& e) {
        DebugLog("RunCUDAInference '%s' failed: %s", model_key.c_str(), e.what());
        return { };
    }
}

std::string GetMetadata(const std::string& model_key, const std::string& key) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_ort_available) return "";
    auto it = g_sessions.find(model_key);
    if (it == g_sessions.end())
        return "";
    try {
        Ort::AllocatorWithDefaultOptions alloc;
        Ort::ModelMetadata metadata = it->second->session->GetModelMetadata();
        auto value = metadata.LookupCustomMetadataMapAllocated(key.c_str(), alloc);
        return value ? value.get() : "";
    } catch (const std::exception&) {
        return "";
    }
}

bool GetCachedModelShape(const std::string& model_key, int& out_w, int& out_h) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_ort_available) return false;
    auto it = g_sessions.find(model_key);
    if (it == g_sessions.end()) return false;
    auto& info = it->second->inputs;
    if (!info.empty() && info[0].shape.size() == 4) {
        if (info[0].shape[2] > 0) out_h = (int)info[0].shape[2];
        if (info[0].shape[3] > 0) out_w = (int)info[0].shape[3];
        return true;
    }
    return false;
}

bool IsCudaSupported() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_ort_available && g_cuda_supported;
}

bool IsOrtAvailable() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_ort_available;
}

void Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_ort_available) return;
    g_sessions.clear();
    g_env.reset();
    DebugLog("Shutdown complete");
}

std::string GetDiagnostics() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::ostringstream ss;
    ss << "OrtEngine Diagnostics:\n";
    ss << "  Environment: " << (g_env ? "initialized" : "not initialized") << "\n";
    ss << "  Models loaded: " << g_sessions.size() << "\n";
    for (auto& [key, ms] : g_sessions) {
        ss << "    " << key << ": " << ms->inputs.size() << " inputs, "
           << ms->outputs.size() << " outputs, device=" << ms->device << "\n";
    }
    return ss.str();
}

} // namespace OrtEngine
