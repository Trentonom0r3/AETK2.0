cmake_minimum_required(VERSION 3.20)
project(AETK_CrossHost CXX)

if(NOT DEFINED AETK_ROOT)
    set(AETK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
endif()
include("${AETK_ROOT}/cmake/aetk_standalone.cmake")

aetk_add_effect(AETK_CrossHost
    SOURCES
        main.cpp
    NAME        "AETK CrossHost"
    CATEGORY    "AETK Samples"
    MATCH_NAME  "AETK CrossHost"
    ENTRY       "EffectMain"
    VERSION     1 1 0
    OUT_FLAGS   0x02800000
    OUT_FLAGS2  0x08801400
)
