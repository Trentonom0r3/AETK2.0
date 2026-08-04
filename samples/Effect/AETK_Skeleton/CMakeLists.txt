cmake_minimum_required(VERSION 3.20)
project(AETK_Skeleton CXX)

# Standalone support: include AETK bootstrap when opened directly
if(NOT DEFINED AETK_ROOT)
    set(AETK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
endif()
include("${AETK_ROOT}/cmake/aetk_standalone.cmake")

aetk_add_effect(AETK_Skeleton
    SOURCES
        main.cpp
    NAME        "AETK Skeleton"
    CATEGORY    "AETK Samples"
    MATCH_NAME  "AETK Skeleton"
    ENTRY       "EffectMain"
    VERSION     1 1 0
    OUT_FLAGS   0x02808010
    OUT_FLAGS2  0x0A801400
)
