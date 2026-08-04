// =============================================================================
// OrtDelayLoadHook.cpp — MSVC delay-load hook for ONNX Runtime DLL isolation
//
// AE 2025+ ships its own onnxruntime.dll. Windows won't load two DLLs with
// the same basename, so we rename ours to onnxruntime_swarm.dll and intercept
// the delay-load to redirect to it.
// =============================================================================

#ifdef _WIN32

#include <windows.h>
#include <delayimp.h>
#include <string.h>
#include <string>

#include <vector>

std::string g_last_load_error = "";

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

#ifdef ORT_DLL_NAME
static std::wstring GetOrtDllName() {
    std::string narrow = ORT_DLL_NAME;
    std::wstring wide(narrow.begin(), narrow.end());
    return wide;
}
#else
static std::wstring GetOrtDllName() {
    return L"onnxruntime_fsl_1.0.0.dll";
}
#endif

static std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

static bool FileExists(const std::wstring& path) {
    DWORD attribs = GetFileAttributesW(path.c_str());
    return (attribs != INVALID_FILE_ATTRIBUTES && !(attribs & FILE_ATTRIBUTE_DIRECTORY));
}

static FARPROC WINAPI OrtDelayLoadHook(unsigned dliNotify, PDelayLoadInfo pdli) {
    if (dliNotify == dliNotePreLoadLibrary) {
        if (pdli->szDll && _stricmp(pdli->szDll, "onnxruntime.dll") == 0) {
            g_last_load_error = "";
            std::wstring dir = GetThisModuleDirectory();
            if (!dir.empty()) {
                std::wstring dll_name = GetOrtDllName();
                std::wstring candidates[] = {
                    dir + L"\\framesmith_core\\" + dll_name,
                    dir + L"\\..\\framesmith_core\\" + dll_name,
                    dir + L"\\..\\..\\framesmith_core\\" + dll_name,
                    dir + L"\\" + dll_name
                };

                bool found_candidate = false;
                std::wstring found_candidate_path;

                for (const auto& path : candidates) {
                    if (FileExists(path)) {
                        found_candidate = true;
                        found_candidate_path = path;
                        HMODULE hMod = LoadLibraryExW(path.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
                        if (hMod) {
                            return (FARPROC)hMod;
                        }
                        break;
                    }
                }

                if (!found_candidate) {
                    std::string dll_name_a = WStringToString(dll_name);
                    g_last_load_error = "Could not find target loader DLL: " + dll_name_a + "\nPaths checked:\n";
                    for (const auto& path : candidates) {
                        std::string path_a = WStringToString(path);
                        g_last_load_error += " - " + path_a + "\n";
                    }
                } else {
                    // LoadLibraryExW failed. Check adjacent dependencies.
                    std::wstring parent_dir = found_candidate_path;
                    size_t pos = parent_dir.find_last_of(L"\\/");
                    if (pos != std::wstring::npos) {
                        parent_dir = parent_dir.substr(0, pos);
                    }

                    std::wstring deps[] = {
                        L"onnxruntime_providers_shared.dll",
                        L"onnxruntime_providers_cuda.dll",
                        L"DirectML.dll"
                    };

                    std::vector<std::string> missing_deps;
                    for (const auto& dep : deps) {
                        std::wstring dep_path = parent_dir + L"\\" + dep;
                        if (!FileExists(dep_path)) {
                            missing_deps.push_back(WStringToString(dep));
                        }
                    }

                    std::string dll_name_a = WStringToString(dll_name);
                    g_last_load_error = "Failed to load " + dll_name_a + " (Error Code: " + std::to_string(GetLastError()) + ").\n";
                    if (!missing_deps.empty()) {
                        g_last_load_error += "Missing adjacent dependency file(s):\n";
                        for (const auto& m : missing_deps) {
                            g_last_load_error += " - " + m + "\n";
                        }
                    } else {
                        g_last_load_error += "All adjacent files are present. You might be missing system CUDA Toolkit / cuDNN libraries (e.g. cudart64_12.dll, cublas64_12.dll, or cudnn64_9.dll) in your system PATH.";
                    }
                }
            } else {
                g_last_load_error = "Failed to locate plugin directory.";
            }
            // If we fail to load our custom renamed DLL, raise a structured exception.
            RaiseException(0xC0000135, 0, 0, NULL);
        }
    }
    return NULL;
}

extern "C" const PfnDliHook __pfnDliNotifyHook2 = (PfnDliHook)OrtDelayLoadHook;

#endif
