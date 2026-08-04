cmake_minimum_required(VERSION 3.20)
project(AETK_Checkout CXX)

# Standalone support: include AETK bootstrap when opened directly
# cmake -B build -DAETK_ROOT=... -DAE_SDK_ROOT=...
if(NOT DEFINED AETK_ROOT)
    set(AETK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
endif()
include("${AETK_ROOT}/cmake/aetk_standalone.cmake")

aetk_add_effect(AETK_Checkout
    SOURCES
        main.cpp
    NAME        "AETK Checkout"
    CATEGORY    "AETK Samples"
    MATCH_NAME  "AETK Checkout"
    ENTRY       "EffectMain"
    VERSION     2 6 0
    OUT_FLAGS   0x00000166
    OUT_FLAGS2  0x08400400
)

target_include_directories(AETK_Checkout PRIVATE "${AE_SDK_ROOT}/Examples/Util")
