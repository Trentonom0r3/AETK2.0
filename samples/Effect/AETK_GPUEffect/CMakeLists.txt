cmake_minimum_required(VERSION 3.20)
 
find_package(CUDAToolkit QUIET)
 
if(CUDAToolkit_FOUND)
    project(AETK_GPUEffect CXX CUDA)
else()
    project(AETK_GPUEffect CXX)
endif()
 
# Standalone support: include AETK bootstrap when opened directly
if(NOT DEFINED AETK_ROOT)
    set(AETK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
endif()
include("${AETK_ROOT}/cmake/aetk_standalone.cmake")
 
set(GPUTEST_SOURCES
    main.cpp
)
 
if(WIN32)
    # Attempt to find shader compiler fxc
    find_program(FXC_EXECUTABLE NAMES fxc fxc.exe 
        PATHS 
        "$ENV{WindowsSdkDir}/bin/$ENV{WindowsSDKVersion}/x64"
        "$ENV{WindowsSdkVerBinPath}/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/*/x64"
    )
    if(NOT FXC_EXECUTABLE)
        set(FXC_EXECUTABLE "fxc")
    endif()
endif()
 
if(CUDAToolkit_FOUND)
    list(APPEND GPUTEST_SOURCES gpu_test.cu)
endif()
 
aetk_add_effect(AETK_GPUEffect
    SOURCES     ${GPUTEST_SOURCES}
    NAME        "AETK GPUEffect"
    CATEGORY    "AETK Samples"
    MATCH_NAME  "AETK GPUEffect"
    ENTRY       "EffectMain"
    VERSION     1 0 0
)
 
if(WIN32)
    target_compile_definitions(AETK_GPUEffect PRIVATE AETK_ENABLE_D3D12=1 AETK_ENABLE_DIRECTX=1)
    target_link_libraries(AETK_GPUEffect PRIVATE d3d12.lib dxgi.lib d3dcompiler.lib)
    
    # Pre-build command to compile hlsl to header
    add_custom_command(
        TARGET AETK_GPUEffect PRE_BUILD
        COMMAND ${FXC_EXECUTABLE} /T cs_5_0 /E CSMain 
                /Fh "${CMAKE_CURRENT_BINARY_DIR}/gpu_test_cso.h" 
                /Vn g_gpu_test_cso 
                "${CMAKE_CURRENT_LIST_DIR}/gpu_test.hlsl"
        COMMENT "Compiling HLSL to C++ Headers"
        VERBATIM
    )
    
    target_include_directories(AETK_GPUEffect PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endif()
 
if(CUDAToolkit_FOUND)
    target_compile_definitions(AETK_GPUEffect PRIVATE AETK_ENABLE_CUDA=1)
    target_link_libraries(AETK_GPUEffect PRIVATE CUDA::cudart_static)
endif()
 
set_target_properties(AETK_GPUEffect PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(AETK_GPUEffect PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
