# aetk_standalone.cmake
#
# Include this at the top of any sample or generated plugin CMakeLists.txt
# to allow standalone builds (i.e. building without the parent AETK repo).
#
# Usage:
#   cmake -B build -DAETK_ROOT="C:/path/to/AETK2.0" -DAE_SDK_ROOT="C:/path/to/SDK"
#
# When built via add_subdirectory from the AETK root, this file does nothing
# because aetk_core and aetk_pipl are already set up by the parent.

# Only bootstrap if we are the top-level project
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.20)

    # Static MSVC runtime (/MT, /MTd) — required for AE plugins
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    # Static CUDA runtime (cudart_static) — must match the CRT linkage
    set(CMAKE_CUDA_RUNTIME_LIBRARY Static)

    # Resolve AETK_ROOT
    if(NOT DEFINED AETK_ROOT)
        if(DEFINED ENV{AETK_ROOT})
            set(AETK_ROOT "$ENV{AETK_ROOT}")
        else()
            message(FATAL_ERROR
                "AETK_ROOT is not set.\n"
                "Pass it to CMake: -DAETK_ROOT=C:/path/to/AETK2.0\n"
                "or set the AETK_ROOT environment variable.")
        endif()
    endif()
    file(TO_CMAKE_PATH "${AETK_ROOT}" AETK_ROOT)

    # Resolve AE_SDK_ROOT
    if(NOT DEFINED AE_SDK_ROOT)
        if(DEFINED ENV{AE_SDK_ROOT})
            set(AE_SDK_ROOT "$ENV{AE_SDK_ROOT}")
        else()
            message(FATAL_ERROR
                "AE_SDK_ROOT is not set.\n"
                "Pass it to CMake: -DAE_SDK_ROOT=C:/path/to/SDK\n"
                "or set the AE_SDK_ROOT environment variable.")
        endif()
    endif()
    file(TO_CMAKE_PATH "${AE_SDK_ROOT}" AE_SDK_ROOT)

    set(PIPL_TOOL "${AE_SDK_ROOT}/Examples/Resources/PiPLtool.exe")

    # Plugin output dir
    if(NOT AE_PLUGIN_DIR)
        set(AE_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/ae_plugins")
    endif()
    file(MAKE_DIRECTORY "${AE_PLUGIN_DIR}")

    # AETK Core interface library
    add_library(aetk_core INTERFACE)
    target_include_directories(aetk_core INTERFACE
        "${AETK_ROOT}/include"
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

    target_compile_features(aetk_core INTERFACE cxx_std_20)

    # Pull in PiPL and AEGP macros
    include("${AETK_ROOT}/cmake/aetk_pipl.cmake")
endif()
