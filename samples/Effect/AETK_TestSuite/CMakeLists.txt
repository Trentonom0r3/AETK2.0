cmake_minimum_required(VERSION 3.20)

find_package(CUDAToolkit QUIET)

if(CUDAToolkit_FOUND)
    project(AETK_TestSuite CXX CUDA)
else()
    project(AETK_TestSuite CXX)
endif()

# Standalone support: include AETK bootstrap when opened directly
if(NOT DEFINED AETK_ROOT)
    set(AETK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
endif()
include("${AETK_ROOT}/cmake/aetk_standalone.cmake")

set(TEST_SUITE_SOURCES main.cpp)
if(CUDAToolkit_FOUND)
    list(APPEND TEST_SUITE_SOURCES "${CMAKE_SOURCE_DIR}/include/aetk/effect/draw/cuda_canvas_kernels.cu")
endif()

aetk_add_effect(AETK_TestSuite
    SOURCES
        ${TEST_SUITE_SOURCES}
    NAME        "AETK Test Suite"
    CATEGORY    "AETK Samples"
    MATCH_NAME  "AETK Test Suite"
    ENTRY       "EffectMain"
    VERSION     2 6 0
    OUT_FLAGS   0x02808010
    OUT_FLAGS2  0x2A801402 # MFR + Float Color Aware + GPU Render F32 + DirectX Rendering + SmartFX
)

target_include_directories(AETK_TestSuite PRIVATE "${AE_SDK_ROOT}/Examples/Util")

if(CUDAToolkit_FOUND)
    target_compile_definitions(AETK_TestSuite PRIVATE AETK_ENABLE_CUDA=1)
    target_link_libraries(AETK_TestSuite PRIVATE CUDA::cudart_static)
endif()

