#pragma once
// =============================================================================
// OrtEngine — ONNX Runtime inference engine
//
// Manages ONNX Runtime sessions.
// =============================================================================

#define ORT_API_MANUAL_INIT
#include <onnxruntime_cxx_api.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>

#ifdef _WIN32
struct ID3D12Resource;
#endif

namespace OrtEngine {

// Tensor descriptor for inputs/outputs
struct TensorInfo {
    std::string name;
    std::vector<int64_t> shape;
    ONNXTensorElementDataType dtype = ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
};

// Initialize ORT environment (call once at startup)
void Init();

// Load an ONNX model. model_key is a unique identifier (e.g., "yolo").
// device: "cpu" or "dml" or "cuda". Returns true on success.
bool LoadModel(const std::string& model_key,
               const std::string& onnx_path,
               const std::string& device = "cpu");

// Check if a model is loaded on a specific device
bool IsModelLoaded(const std::string& model_key, const std::string& device = "");

// Unload a model and free VRAM/session resources
void UnloadModel(const std::string& model_key);

// Get input/output info for a loaded model
std::vector<TensorInfo> GetInputInfo(const std::string& model_key);
std::vector<TensorInfo> GetOutputInfo(const std::string& model_key);

// Fast cached query for model input shape dimensions (returns true if found)
bool GetCachedModelShape(const std::string& model_key, int& out_w, int& out_h);

// Run inference.
struct IOTensor {
    std::string name;
    std::vector<int64_t> shape;
    std::vector<float> data;
};

std::vector<IOTensor> Run(const std::string& model_key,
                           const std::vector<IOTensor>& inputs);

#ifdef _WIN32
bool BindD3D12Input(const std::string& model_key,
                    const std::string& input_name,
                    ::ID3D12Resource* resource,
                    const std::vector<int64_t>& shape,
                    ONNXTensorElementDataType dtype = ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT);
#endif

bool BindCUDAInput(const std::string& model_key,
                   const std::string& input_name,
                   void* cuda_device_ptr,
                   size_t size_in_bytes,
                   const std::vector<int64_t>& shape,
                   ONNXTensorElementDataType dtype = ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT);

std::vector<IOTensor> RunWithBinding(const std::string& model_key);

// Thread-safe parallel CUDA inference using stack-allocated IoBinding
std::vector<IOTensor> RunCUDAInference(const std::string& model_key,
                                       const std::string& input_name,
                                       void* cuda_device_ptr,
                                       size_t size_in_bytes,
                                       const std::vector<int64_t>& shape);

// Get a metadata value from a loaded model by key
std::string GetMetadata(const std::string& model_key, const std::string& key);

// Check if CUDA execution provider is available in ONNX Runtime
bool IsCudaSupported();

// Check if ONNX Runtime was initialized successfully
bool IsOrtAvailable();

// Release all resources
void Shutdown();

// Diagnostics
std::string GetDiagnostics();

} // namespace OrtEngine
