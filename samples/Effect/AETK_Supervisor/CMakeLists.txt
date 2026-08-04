cmake_minimum_required(VERSION 3.20)
project(AETK_Supervisor CXX)

# Standalone support: include AETK bootstrap when opened directly
# cmake -B build -DAETK_ROOT=... -DAE_SDK_ROOT=...
if(NOT DEFINED AETK_ROOT)
    set(AETK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
endif()
include("${AETK_ROOT}/cmake/aetk_standalone.cmake")

aetk_add_effect(AETK_Supervisor
    SOURCES     main.cpp
    NAME        "AETK Supervisor"
    CATEGORY    "AETK Samples"
    MATCH_NAME  "AETK_Supervisor"
    VERSION     5 11 0
    BUILD       1
    # PF_OutFlag_SEND_UPDATE_PARAMS_UI | PF_OutFlag_PIX_INDEPENDENT | PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_USE_OUTPUT_EXTENT | PF_OutFlag_WIDE_TIME_INPUT
    OUT_FLAGS   0x07000003
    # PF_OutFlag2_SUPPORTS_SMART_RENDER | PF_OutFlag2_FLOAT_COLOR_AWARE | PF_OutFlag2_DOESNT_NEED_EMPTY_PIXELS | PF_OutFlag2_AUTOMATIC_WIDE_TIME_INPUT | PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG
    OUT_FLAGS2  0x00001402
)
