cmake_minimum_required(VERSION 3.20)
project(AETK_Sweetie CXX)

# Standalone support: include AETK bootstrap when opened directly
# cmake -B build -DAETK_ROOT=... -DAE_SDK_ROOT=...
if(NOT DEFINED AETK_ROOT)
    set(AETK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
endif()
include("${AETK_ROOT}/cmake/aetk_standalone.cmake")

include_directories("${AE_SDK_ROOT}/Examples/Util")

aetk_add_aegp(AETK_Sweetie
    SOURCES     main.cpp
    NAME        "AETK Sweetie"
    VERSION     2 0 0
    STAGE       RELEASE
)
