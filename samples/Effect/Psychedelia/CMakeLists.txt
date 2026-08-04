cmake_minimum_required(VERSION 3.20)
project(Psychedelia CXX CUDA)

# Standalone support: include AETK bootstrap when opened directly
if(NOT DEFINED AETK_ROOT)
    set(AETK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
endif()
include("${AETK_ROOT}/cmake/aetk_standalone.cmake")

find_package(CUDAToolkit REQUIRED)

aetk_add_effect(Psychedelia
    SOURCES
        main.cpp
        render_shared.cpp
        kernels.cu
    NAME        "Psychedelia"
    CATEGORY    "Framesmith Labs"
    MATCH_NAME  "Psychedelia"
    ENTRY       "EffectMain"
    VERSION     1 0 0
    EXTRA_LIBS  CUDA::cudart_static
)
