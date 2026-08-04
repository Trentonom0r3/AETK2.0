cmake_minimum_required(VERSION 3.20)
project(AETK_Resizer CXX)

if(NOT DEFINED AETK_ROOT)
    set(AETK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
endif()
include("${AETK_ROOT}/cmake/aetk_standalone.cmake")

aetk_add_effect(AETK_Resizer
    SOURCES
        main.cpp
    NAME        "AETK Resizer"
    CATEGORY    "AETK Samples"
    MATCH_NAME  "ADBE Resizer"
    ENTRY       "EffectMain"
    VERSION     2 4 0
    STAGE       DEVELOP
    BUILD       1
    OUT_FLAGS   0x03800200
    OUT_FLAGS2  0x08000007
)
