set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime (/MT, /MTd) — required for AE plugins
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Static CUDA runtime (cudart_static) — must match the CRT linkage
set(CMAKE_CUDA_RUNTIME_LIBRARY Static)

# Find CUDA (Optional)
find_package(CUDAToolkit QUIET)
if(CUDAToolkit_FOUND)
    message(STATUS "AETK: CUDA Toolkit found (${CUDAToolkit_VERSION}). Enabling GPU features.")
else()
    message(STATUS "AETK: CUDA Toolkit not found. Building CPU-only features.")
endif()

# Define SDK Root
if(NOT DEFINED AE_SDK_ROOT)
    if(DEFINED ENV{AE_SDK_ROOT})
        set(AE_SDK_ROOT "$ENV{AE_SDK_ROOT}")
    else()
        message(FATAL_ERROR "AE_SDK_ROOT is not set. Please define it as an environment variable or pass it to CMake (-DAE_SDK_ROOT=...).")
    endif()
endif()
file(TO_CMAKE_PATH "${AE_SDK_ROOT}" AE_SDK_ROOT)
set(PIPL_TOOL "${AE_SDK_ROOT}/Examples/Resources/PiPLtool.exe")

# Define Plugin Dir (Fallback if not set in environment)
if(NOT AE_PLUGIN_DIR)
    set(AE_PLUGIN_DIR "${CMAKE_SOURCE_DIR}/build/ae_plugins")
endif()
file(MAKE_DIRECTORY "${AE_PLUGIN_DIR}")

# AETK Core Library (Header-only for now)
add_library(aetk_core INTERFACE)
target_include_directories(aetk_core INTERFACE 
    "${CMAKE_SOURCE_DIR}/include"
    "${AE_SDK_ROOT}/Examples/Headers"
    "${AE_SDK_ROOT}/Examples/Headers/SP"
    "${AE_SDK_ROOT}/Examples/Util"
)

target_compile_definitions(aetk_core INTERFACE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    MSWindows
    WIN32
    _WINDOWS
    AE_OS_WIN
)

# ── Auto-PiPL ──────────────────────────────────────────────────────
include(cmake/aetk_pipl.cmake)

# ── Samples ────────────────────────────────────────────────────────

# Effects (Public Open-Source Samples)
add_subdirectory(samples/Effect/AETK_Checkout)
add_subdirectory(samples/Effect/AETK_Convolutrix)
add_subdirectory(samples/Effect/AETK_Supervisor)
add_subdirectory(samples/Effect/Psychedelia)
add_subdirectory(samples/Effect/AETK_CustomUI)
add_subdirectory(samples/Effect/AETK_Skeleton)
add_subdirectory(samples/Effect/AETK_CrossHost)
add_subdirectory(samples/Effect/AETK_SmartyPants)
add_subdirectory(samples/Effect/AETK_Resizer)
add_subdirectory(samples/Effect/AETK_GPUEffect)
add_subdirectory(samples/Effect/AETK_TestSuite)
add_subdirectory(samples/Effect/SWARM)

# Private / Proprietary Plugin Subdirectories (Included if present locally)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/Effect/Splat!/CMakeLists.txt")
    add_subdirectory("samples/Effect/Splat!")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/Effect/AETK_StemSplitter/CMakeLists.txt")
    add_subdirectory(samples/Effect/AETK_StemSplitter)
endif()

# AEGP (Public Open-Source Samples)
add_subdirectory(samples/AEGP/AETK_Sweetie)

# AEGP (Private / Proprietary Subdirectories)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/AEGP/FramesmithStudio/CMakeLists.txt")
    add_subdirectory(samples/AEGP/FramesmithStudio)
endif()

