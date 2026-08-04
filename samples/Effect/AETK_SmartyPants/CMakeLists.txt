cmake_minimum_required(VERSION 3.20)
project(AETK_SmartyPants CXX)

# Standalone support: include AETK bootstrap when opened directly
if(NOT DEFINED AETK_ROOT)
    set(AETK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
endif()
include("${AETK_ROOT}/cmake/aetk_standalone.cmake")

aetk_add_effect(AETK_SmartyPants
    SOURCES
        main.cpp
    NAME        "AETK SmartyPants"
    CATEGORY    "AETK Samples"
    MATCH_NAME  "AETK SmartyPants"
    ENTRY       "EffectMain"
    VERSION     1 4 0
    OUT_FLAGS   0x02800440 # PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_PiPL_OVERRIDES_OUTDATA_OUTFLAGS | PF_OutFlag_PIX_INDEPENDENT | PF_OutFlag_USE_OUTPUT_EXTENT
    OUT_FLAGS2  0x08201409 # PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG | PF_OutFlag2_SUPPORTS_SMART_RENDER | PF_OutFlag2_FLOAT_COLOR_AWARE | PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS | PF_OutFlag2_I_MIX_GUID_DEPENDENCIES | PF_OutFlag2_SUPPORTS_THREADED_RENDERING
)
