cmake_minimum_required(VERSION 3.20)
project(AETK_Convolutrix CXX)

# Standalone support: include AETK bootstrap when opened directly
# cmake -B build -DAETK_ROOT=... -DAE_SDK_ROOT=...
if(NOT DEFINED AETK_ROOT)
    set(AETK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
endif()
include("${AETK_ROOT}/cmake/aetk_standalone.cmake")

aetk_add_effect(AETK_Convolutrix
    SOURCES
        main.cpp
    NAME        "AETK Convolutrix"
    CATEGORY    "AETK Samples"
    MATCH_NAME  "AETK Convolutrix"
    ENTRY       "EffectMain"
    VERSION     3 2 0
    OUT_FLAGS   0x00000004
    OUT_FLAGS2  0x08000400
)
