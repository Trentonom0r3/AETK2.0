#pragma once

/**
 * @file ort_helper.hpp
 * @brief Utility for checking and handling ONNX Runtime dynamic library dependencies.
 */

#include <string>
#include <vector>
#include <cwchar>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <shellapi.h>
#else
    #include <cstdlib>
    #include <unistd.h>
#endif

#include <aetk/core/log.hpp>

namespace aetk::core {

/**
 * @brief Helper for managing ONNX Runtime DLL verification and user download redirects.
 * 
 * @details Adobe After Effects 2025+ ships its own global version of `onnxruntime.dll`. To avoid 
 * name collisions in the host process, AETK plugins use isolated, custom-named DLLs (e.g. `onnxruntime_fsl_1.0.0.dll`) 
 * and intercept dynamic loads via MSVC delay-load hooks.
 * 
 * `ort_helper` provides a centralized utility to verify that these renamed libraries and their 
 * adjacent dependencies are present before ORT environment initialization. If they are missing, 
 * it triggers UAC-elevated execution of the installer or redirects the user to the installer download page.
 */
class ort_helper {
private:
#ifdef _WIN32
    /**
     * @brief Resolves the absolute directory of the current plugin module.
     * Uses GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS with a local function address to locate the active DLL.
     */
    static std::wstring get_current_module_directory() {
        HMODULE hMod = NULL;
        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)&get_current_module_directory, &hMod);
        if (!hMod) return L"";

        wchar_t path[MAX_PATH];
        GetModuleFileNameW(hMod, path, MAX_PATH);
        wchar_t* last_slash = wcsrchr(path, L'\\');
        if (last_slash) *last_slash = L'\0';
        return path;
    }

    /**
     * @brief Helper to check if a file exists on disk.
     */
    static bool file_exists(const std::wstring& path) {
        DWORD attribs = GetFileAttributesW(path.c_str());
        return (attribs != INVALID_FILE_ATTRIBUTES && !(attribs & FILE_ATTRIBUTE_DIRECTORY));
    }

    /**
     * @brief Converts wide string to UTF-8 encoded narrow string.
     */
    static std::string wstr_to_utf8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string str(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size_needed, NULL, NULL);
        return str;
    }
#endif

public:
    /**
     * @brief Silently tests whether the main ORT library is present in any search path.
     * Does not display dialog popups or run installers.
     */
    static bool test_dependencies_present(
        const std::string& dll_name,
        const std::vector<std::wstring>& relative_search_dirs = { L"framesmith_core", L"..\\framesmith_core", L"..\\..\\framesmith_core", L"" })
    {
#ifdef _WIN32
        std::wstring w_dll_name(dll_name.begin(), dll_name.end());
        std::wstring dir = get_current_module_directory();
        if (dir.empty()) return false;

        for (const auto& rel_dir : relative_search_dirs) {
            std::wstring path;
            if (rel_dir.empty()) {
                path = dir + L"\\" + w_dll_name;
            } else {
                path = dir + L"\\" + rel_dir + L"\\" + w_dll_name;
            }
            if (file_exists(path)) {
                return true;
            }
        }
        return false;
#else
        return true;
#endif
    }
    /**
     * @brief Verifies that the required ONNX Runtime libraries and dependencies are present.
     * If they are missing, it pops a native confirmation dialog asking the user if they wish 
     * to run the setup installer.
     * 
     * If they accept:
     * - The utility scans for a local `FramesmithInstaller.exe` next to the plugin and launches it with admin/UAC elevation (`runas`).
     * - If no local installer is found, it launches the default web browser to the provided download page.
     * 
     * @param plugin_name The user-facing name of the plugin displaying the dialog.
     * @param dll_name The filename of the main ORT library (e.g., "onnxruntime_fsl_1.0.0.dll").
     * @param required_deps List of adjacent required dynamic library dependencies.
     * @param download_url The URL pointing to the installer download page.
     * @param relative_search_dirs Optional subdirectories relative to the plugin folder to scan.
     *                             Defaults to typical Framesmith plugin layout search spaces.
     * @return true if all libraries are found and validated; false otherwise.
     */
    static bool ensure_dependencies(
        const std::string& plugin_name,
        const std::string& dll_name,
        const std::vector<std::string>& required_deps,
        const std::string& download_url,
        const std::vector<std::wstring>& relative_search_dirs = { L"framesmith_core", L"..\\framesmith_core", L"..\\..\\framesmith_core", L"" })
    {
#ifdef _WIN32
        std::wstring w_dll_name(dll_name.begin(), dll_name.end());
        std::wstring dir = get_current_module_directory();
        if (dir.empty()) {
            aetk::core::logger::instance().log(aetk::core::log_level::error, "[ORT Helper] Failed to locate plugin directory.");
            return false;
        }

        bool found = false;
        std::wstring found_dir = L"";

        // Scan candidate paths for the target main DLL
        for (const auto& rel_dir : relative_search_dirs) {
            std::wstring path;
            if (rel_dir.empty()) {
                path = dir + L"\\" + w_dll_name;
            } else {
                path = dir + L"\\" + rel_dir + L"\\" + w_dll_name;
            }

            if (file_exists(path)) {
                // Determine the parent directory containing this candidate DLL
                size_t pos = path.find_last_of(L"\\/");
                if (pos != std::wstring::npos) {
                    std::wstring parent_dir = path.substr(0, pos);
                    bool all_deps_found = true;
                    
                    // Verify all adjacent dependencies are also present in this location
                    for (const auto& dep : required_deps) {
                        std::wstring w_dep(dep.begin(), dep.end());
                        std::wstring dep_path = parent_dir + L"\\" + w_dep;
                        if (!file_exists(dep_path)) {
                            all_deps_found = false;
                            aetk::core::logger::instance().log(
                                aetk::core::log_level::warning, 
                                "[ORT Helper] Candidate main DLL found but adjacent dependency is missing: " + dep
                            );
                            break;
                        }
                    }

                    if (all_deps_found) {
                        found = true;
                        found_dir = parent_dir;
                        break;
                    }
                }
            }
        }

        if (found) {
            aetk::core::logger::instance().log(
                aetk::core::log_level::info, 
                "[ORT Helper] All ORT dependencies successfully verified in: " + wstr_to_utf8(found_dir)
            );
            return true;
        }

        // If dependencies are missing, alert and offer installer launch or redirect
        std::string title = plugin_name + " - Missing Dependencies";
        std::string msg = "The required ONNX Runtime libraries are missing or incomplete.\n\n"
                          "Would you like to run the installer setup now to download and install the required components?";

        int choice = MessageBoxA(NULL, msg.c_str(), title.c_str(), MB_YESNO | MB_ICONQUESTION | MB_SETFOREGROUND);
        if (choice == IDYES) {
            std::wstring w_url(download_url.begin(), download_url.end());
            
            // Try to find a local installer next to the plugin first
            std::wstring local_installer_candidates[] = {
                dir + L"\\FramesmithInstaller.exe",
                dir + L"\\..\\FramesmithInstaller.exe",
                dir + L"\\..\\..\\FramesmithInstaller.exe"
            };

            bool ran_installer = false;
            for (const auto& installer_path : local_installer_candidates) {
                if (file_exists(installer_path)) {
                    // Execute with admin privileges (runas)
                    HINSTANCE inst = ShellExecuteW(NULL, L"runas", installer_path.c_str(), NULL, NULL, SW_SHOWNORMAL);
                    if ((uintptr_t)inst > 32) {
                        ran_installer = true;
                        aetk::core::logger::instance().log(aetk::core::log_level::info, "[ORT Helper] Successfully launched local elevated installer.");
                        break;
                    } else {
                        aetk::core::logger::instance().log(
                            aetk::core::log_level::error, 
                            "[ORT Helper] Failed to run elevated local installer. Error code: " + std::to_string((uintptr_t)inst)
                        );
                    }
                }
            }

            // Fallback: If no local installer was found or failed to execute, open download page in browser
            if (!ran_installer) {
                HMODULE hShell = LoadLibraryA("shell32.dll");
                if (hShell) {
                    typedef HINSTANCE(WINAPI* LPFNSHELLEXECUTEW)(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);
                    auto pShellExecuteW = (LPFNSHELLEXECUTEW)GetProcAddress(hShell, "ShellExecuteW");
                    if (pShellExecuteW) {
                        pShellExecuteW(NULL, L"open", w_url.c_str(), NULL, NULL, SW_SHOWNORMAL);
                    }
                    FreeLibrary(hShell);
                } else {
                    aetk::core::logger::instance().log(aetk::core::log_level::error, "[ORT Helper] Failed to load shell32.dll to open download URL.");
                }
            }
        }
        return false;
#else
        // macOS / Unix fallback path: Typically, frameworks are bundled directly in the .plugin package.
        // We can optionally run `system("open <url>")` if needed, but defaults to true.
        (void)dll_name;
        (void)required_deps;
        (void)download_url;
        (void)relative_search_dirs;
        return true;
#endif
    }
};

} // namespace aetk::core
