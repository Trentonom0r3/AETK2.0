cmake_minimum_required(VERSION 3.20)

find_package(CUDAToolkit QUIET)

if(CUDAToolkit_FOUND)
    project(SWARM CXX CUDA)
else()
    project(SWARM CXX)
endif()

# Standalone support: include AETK bootstrap when opened directly
if(NOT DEFINED AETK_ROOT)
    set(AETK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
endif()
include("${AETK_ROOT}/cmake/aetk_standalone.cmake")

# Setup ONNX Runtime search
if(NOT DEFINED ORT_ROOT)
    if(DEFINED ENV{ORT_ROOT})
        set(ORT_ROOT "$ENV{ORT_ROOT}")
    else()
        message(FATAL_ERROR "ORT_ROOT is not set. Please define it as an environment variable or pass it to CMake (-DORT_ROOT=...).")
    endif()
endif()
file(TO_CMAKE_PATH "${ORT_ROOT}" ORT_ROOT)

if(NOT EXISTS "${ORT_ROOT}/include/onnxruntime_cxx_api.h")
    message(FATAL_ERROR "ONNX Runtime not found at ORT_ROOT: ${ORT_ROOT}")
endif()
message(STATUS "ONNX Runtime found at: ${ORT_ROOT}")

set(SWARM_SOURCES
    main.cpp
    OrtEngine.cpp
    OrtDelayLoadHook.cpp
    shape_designer.hpp
    yolo_class_customizer.hpp
)

# -----------------------------------------------------------------------
# 1. FIND SHADER COMPILER & SET SOURCES
# -----------------------------------------------------------------------


if(CUDAToolkit_FOUND)
    # If the build machine has CUDA, we ALSO compile the CUDA kernels
    list(APPEND SWARM_SOURCES 
        swarm_kernels.cu
        "${CMAKE_SOURCE_DIR}/include/aetk/effect/draw/cuda_canvas_kernels.cu"
    )
    message(STATUS "CUDA Toolkit found. Adding CUDA runtime acceleration.")
endif()

set(SWARM_OUT_FLAGS2 0x2A801C08)

# -----------------------------------------------------------------------
# 2. CREATE TARGET
# -----------------------------------------------------------------------
aetk_add_effect(SWARM
    SOURCES     ${SWARM_SOURCES}
    NAME        "SWARM"
    CATEGORY    "Framesmith Labs"
    MATCH_NAME  "SWARM"
    ENTRY       "EffectMain"
    VERSION     1 0 0
)

# -----------------------------------------------------------------------
# 3. CONFIGURE TARGET PROPERTIES & SHADER COMPILATION
# -----------------------------------------------------------------------


if(CUDAToolkit_FOUND)
    # Link CUDA libraries
    target_compile_definitions(SWARM PRIVATE AETK_ENABLE_CUDA=1)
    target_link_libraries(SWARM PRIVATE CUDA::cudart_static)
endif()

# Compile options & standards
set_target_properties(SWARM PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(SWARM PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

# Link ORT + MSVC delay load
target_include_directories(SWARM PRIVATE "${ORT_ROOT}/include")
target_link_directories(SWARM PRIVATE "${ORT_ROOT}")
target_link_libraries(SWARM PRIVATE onnxruntime delayimp.lib)
target_link_options(SWARM PRIVATE "/DELAYLOAD:onnxruntime.dll")

# Define FSL ORT naming scheme and compile definition
set(ORT_DLL_NAME "onnxruntime_fsl_1.0.0.dll")
target_compile_definitions(SWARM PRIVATE ORT_DLL_NAME="${ORT_DLL_NAME}")

# Copy and rename DLL post-build
add_custom_command(TARGET SWARM POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ORT_ROOT}/onnxruntime.dll" "$<TARGET_FILE_DIR:SWARM>/${ORT_DLL_NAME}"
    COMMENT "Deploying and renaming onnxruntime.dll -> ${ORT_DLL_NAME} next to plugin"
)

# Copy the renamed DLL to AE's plugin directory as well so AE loads it
add_custom_command(TARGET SWARM POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE_DIR:SWARM>/${ORT_DLL_NAME}" "${AE_PLUGIN_DIR}/${ORT_DLL_NAME}"
    COMMENT "Deploying ${ORT_DLL_NAME} to AE plugins folder"
)

# Copy CUDA provider and shared DLLs, and DirectML DLL to build output and AE plugins
add_custom_command(TARGET SWARM POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ORT_ROOT}/onnxruntime_providers_cuda.dll" "$<TARGET_FILE_DIR:SWARM>/onnxruntime_providers_cuda.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ORT_ROOT}/onnxruntime_providers_shared.dll" "$<TARGET_FILE_DIR:SWARM>/onnxruntime_providers_shared.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ORT_ROOT}/DirectML.dll" "$<TARGET_FILE_DIR:SWARM>/DirectML.dll"
    COMMENT "Deploying ORT support DLLs next to plugin"
)

add_custom_command(TARGET SWARM POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ORT_ROOT}/onnxruntime_providers_cuda.dll" "${AE_PLUGIN_DIR}/onnxruntime_providers_cuda.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ORT_ROOT}/onnxruntime_providers_shared.dll" "${AE_PLUGIN_DIR}/onnxruntime_providers_shared.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ORT_ROOT}/DirectML.dll" "${AE_PLUGIN_DIR}/DirectML.dll"
    COMMENT "Deploying ORT support DLLs to AE plugins folder"
)

foreach(MODEL_NAME yolox_s.onnx yolox_m.onnx yolox_l.onnx)
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/${MODEL_NAME}")
        add_custom_command(TARGET SWARM POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_LIST_DIR}/${MODEL_NAME}" "$<TARGET_FILE_DIR:SWARM>/${MODEL_NAME}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_LIST_DIR}/${MODEL_NAME}" "${AE_PLUGIN_DIR}/${MODEL_NAME}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${AE_PLUGIN_DIR}/models"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_LIST_DIR}/${MODEL_NAME}" "${AE_PLUGIN_DIR}/models/${MODEL_NAME}"
            COMMENT "Deploying ${MODEL_NAME} to build output and AE plugins folders"
        )
    endif()
endforeach()



