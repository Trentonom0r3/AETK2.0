cmake_minimum_required(VERSION 3.20)
project(AETK_CustomUI CXX)

# Standalone support: include AETK bootstrap when opened directly
# cmake -B build -DAETK_ROOT=... -DAE_SDK_ROOT=...
if(NOT DEFINED AETK_ROOT)
    set(AETK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
endif()
include("${AETK_ROOT}/cmake/aetk_standalone.cmake")

aetk_add_effect(AETK_CustomUI
    SOURCES
        main.cpp
    NAME        "AETK Custom UI"
    CATEGORY    "AETK Samples"
    MATCH_NAME  "AETK Custom UI V4"
    ENTRY       "EffectMain"
    VERSION     1 4 0
    OUT_FLAGS   0x06008440
    OUT_FLAGS2  0x09001400
)
