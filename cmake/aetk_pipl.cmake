# aetk_pipl.cmake — Auto-generate PiPL resources and create effect plugin targets
#
# Usage:
#   include(cmake/aetk_pipl.cmake)
#   aetk_add_effect(MyEffect
#       SOURCES     src/main.cpp
#       NAME        "My Cool Effect"
#       CATEGORY    "MyPlugins"
#       MATCH_NAME  "MyCoolEffect_v1"
#       ENTRY       "EffectMain"
#       VERSION     1 0 0
#       OUT_FLAGS   0x02808010
#       OUT_FLAGS2  0x0A801400
#       EXTRA_LIBS  CUDA::cudart
#   )

set(AETK_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(aetk_add_effect TARGET_NAME)
    cmake_parse_arguments(
        AETK
        ""                          # options (none)
        "NAME;CATEGORY;MATCH_NAME;ENTRY;STAGE;BUILD"  # single-value
        "SOURCES;VERSION;OUT_FLAGS;OUT_FLAGS2;EXTRA_LIBS"  # multi-value
        ${ARGN}
    )

    # --- Validate required args ---
    if(NOT AETK_NAME)
        message(FATAL_ERROR "aetk_add_effect: NAME is required")
    endif()
    if(NOT AETK_MATCH_NAME)
        message(FATAL_ERROR "aetk_add_effect: MATCH_NAME is required")
    endif()
    if(NOT AETK_SOURCES)
        message(FATAL_ERROR "aetk_add_effect: SOURCES is required")
    endif()

    # --- Defaults ---
    if(NOT AETK_CATEGORY)
        set(AETK_CATEGORY "AETK")
    endif()
    if(NOT AETK_ENTRY)
        set(AETK_ENTRY "EffectMain")
    endif()
    if(NOT AETK_BUILD)
        set(AETK_BUILD 0)
    endif()

    # --- Map STAGE to numeric value ---
    # PF_Stage_DEVELOP = 0, PF_Stage_ALPHA = 1, PF_Stage_BETA = 2, PF_Stage_RELEASE = 3
    if(NOT AETK_STAGE)
        set(_stage_num 3) # default: RELEASE
    elseif(AETK_STAGE STREQUAL "DEVELOP")
        set(_stage_num 0)
    elseif(AETK_STAGE STREQUAL "ALPHA")
        set(_stage_num 1)
    elseif(AETK_STAGE STREQUAL "BETA")
        set(_stage_num 2)
    elseif(AETK_STAGE STREQUAL "RELEASE")
        set(_stage_num 3)
    else()
        set(_stage_num ${AETK_STAGE}) # Allow passing raw int (0-3)
    endif()

    # --- Parse VERSION (list of 3 numbers) ---
    list(LENGTH AETK_VERSION _ver_len)
    if(_ver_len LESS 3)
        set(AETK_VERSION_MAJOR 1)
        set(AETK_VERSION_MINOR 0)
        set(AETK_VERSION_PATCH 0)
    else()
        list(GET AETK_VERSION 0 AETK_VERSION_MAJOR)
        list(GET AETK_VERSION 1 AETK_VERSION_MINOR)
        list(GET AETK_VERSION 2 AETK_VERSION_PATCH)
    endif()

    # --- Compute PF_VERSION ---
    # Formula: (major << 19) | (minor << 15) | (bugfix << 11) | (stage << 9) | (build << 0)
    # Note: Using patch version as "bugfix" to map gracefully to typical semantic versioning.
    math(EXPR _pf_version "(${AETK_VERSION_MAJOR} << 19) | (${AETK_VERSION_MINOR} << 15) | (${AETK_VERSION_PATCH} << 11) | (${_stage_num} << 9) | (${AETK_BUILD})")
    set(AETK_PIPL_VERSION ${_pf_version})

    # --- OutFlags ---
    list(LENGTH AETK_OUT_FLAGS _of_len)
    if(_of_len LESS 1)
        set(_explicit_out_flags FALSE)
        set(AETK_PIPL_OUT_FLAGS 0)
    else()
        list(GET AETK_OUT_FLAGS 0 AETK_PIPL_OUT_FLAGS)
        set(_explicit_out_flags TRUE)
    endif()

    list(LENGTH AETK_OUT_FLAGS2 _of2_len)
    if(_of2_len LESS 1)
        set(_explicit_out_flags2 FALSE)
        set(AETK_PIPL_OUT_FLAGS2 0)
    else()
        list(GET AETK_OUT_FLAGS2 0 AETK_PIPL_OUT_FLAGS2)
        set(_explicit_out_flags2 TRUE)
    endif()

    if(NOT _explicit_out_flags OR NOT _explicit_out_flags2)
        set(_auto_out_flags 0)
        set(_auto_out_flags2 0)

        # Read and compile source content
        set(_source_contents "")
        foreach(_src ${AETK_SOURCES})
            if(IS_ABSOLUTE "${_src}")
                set(_src_path "${_src}")
            else()
                set(_src_path "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
            endif()
            if(EXISTS "${_src_path}")
                file(READ "${_src_path}" _content)
                string(APPEND _source_contents "${_content}")
            endif()
        endforeach()

        # Helper macro to check patterns and update flags
        macro(check_flag _pattern _bit_shift _is_flags2)
            if("${_source_contents}" MATCHES "${_pattern}")
                if(${_is_flags2})
                    math(EXPR _auto_out_flags2 "${_auto_out_flags2} | (1 << ${_bit_shift})")
                else()
                    math(EXPR _auto_out_flags "${_auto_out_flags} | (1 << ${_bit_shift})")
                endif()
            endif()
        endmacro()

        # Check OutFlags
        check_flag("PF_OutFlag_KEEP_RESOURCE_OPEN" 0 FALSE)
        check_flag("PF_OutFlag_WIDE_TIME_INPUT|enable_temporal_checkouts" 1 FALSE)
        check_flag("PF_OutFlag_NON_PARAM_VARY|enable_non_param_varying" 2 FALSE)
        check_flag("PF_OutFlag_SEQUENCE_DATA_NEEDS_FLATTENING" 4 FALSE)
        check_flag("PF_OutFlag_I_DO_DIALOG|enable_options_button" 5 FALSE)
        check_flag("PF_OutFlag_USE_OUTPUT_EXTENT|enable_output_extent" 6 FALSE)
        check_flag("PF_OutFlag_SEND_DO_DIALOG" 7 FALSE)
        check_flag("PF_OutFlag_DISPLAY_ERROR_MESSAGE" 8 FALSE)
        check_flag("PF_OutFlag_I_EXPAND_BUFFER" 9 FALSE)
        check_flag("PF_OutFlag_PIX_INDEPENDENT" 10 FALSE)
        check_flag("PF_OutFlag_I_WRITE_INPUT_BUFFER" 11 FALSE)
        check_flag("PF_OutFlag_I_SHRINK_BUFFER" 12 FALSE)
        check_flag("PF_OutFlag_WORKS_IN_PLACE" 13 FALSE)
        check_flag("PF_OutFlag_CUSTOM_UI|enable_custom_ui" 15 FALSE)
        check_flag("PF_OutFlag_REFRESH_UI" 17 FALSE)
        check_flag("PF_OutFlag_NOP_RENDER" 18 FALSE)
        check_flag("PF_OutFlag_I_USE_SHUTTER_ANGLE" 19 FALSE)
        check_flag("PF_OutFlag_I_USE_AUDIO" 20 FALSE)
        check_flag("PF_OutFlag_I_AM_OBSOLETE" 21 FALSE)
        check_flag("PF_OutFlag_FORCE_RERENDER" 22 FALSE)
        check_flag("PF_OutFlag_PiPL_OVERRIDES_OUTDATA_OUTFLAGS|set_pipl_overrides" 23 FALSE)
        check_flag("PF_OutFlag_I_HAVE_EXTERNAL_DEPENDENCIES" 24 FALSE)
        check_flag("PF_OutFlag_DEEP_COLOR_AWARE" 25 FALSE)
        check_flag("PF_OutFlag_SEND_UPDATE_PARAMS_UI|enable_param_supervision" 26 FALSE)
        check_flag("PF_OutFlag_AUDIO_FLOAT_ONLY|enable_audio_float_only" 27 FALSE)
        check_flag("PF_OutFlag_AUDIO_IIR" 28 FALSE)
        check_flag("PF_OutFlag_I_SYNTHESIZE_AUDIO" 29 FALSE)
        check_flag("PF_OutFlag_AUDIO_EFFECT_TOO|enable_audio_too" 30 FALSE)
        check_flag("PF_OutFlag_AUDIO_EFFECT_ONLY|enable_audio_only" 31 FALSE)

        # Check OutFlags2
        check_flag("PF_OutFlag2_SUPPORTS_QUERY_DYNAMIC_FLAGS" 0 TRUE)
        check_flag("PF_OutFlag2_I_USE_3D_CAMERA" 1 TRUE)
        check_flag("PF_OutFlag2_I_USE_3D_LIGHTS" 2 TRUE)
        check_flag("PF_OutFlag2_I_AM_THREADSAFE" 4 TRUE)
        check_flag("PF_OutFlag2_CAN_COMBINE_WITH_DESTINATION" 5 TRUE)
        check_flag("PF_OutFlag2_DOESNT_NEED_EMPTY_PIXELS" 6 TRUE)
        check_flag("PF_OutFlag2_REVEALS_ZERO_ALPHA" 7 TRUE)
        check_flag("PF_OutFlag2_PRESERVES_FULLY_OPAQUE_PIXELS" 8 TRUE)
        check_flag("PF_OutFlag2_SUPPORTS_SMART_RENDER|enable_smart_render" 10 TRUE)
        check_flag("PF_OutFlag2_FLOAT_COLOR_AWARE" 12 TRUE)
        check_flag("PF_OutFlag2_I_USE_COLORSPACE_ENUMERATION" 13 TRUE)
        check_flag("PF_OutFlag2_I_AM_DEPRECATED" 14 TRUE)
        check_flag("PF_OutFlag2_PPRO_DO_NOT_CLONE_SEQUENCE_DATA_FOR_RENDER" 15 TRUE)
        check_flag("PF_OutFlag2_AUTOMATIC_WIDE_TIME_INPUT" 17 TRUE)
        check_flag("PF_OutFlag2_I_USE_TIMECODE" 18 TRUE)
        check_flag("PF_OutFlag2_DEPENDS_ON_UNREFERENCED_MASKS" 19 TRUE)
        check_flag("PF_OutFlag2_OUTPUT_IS_WATERMARKED" 20 TRUE)
        check_flag("PF_OutFlag2_I_MIX_GUID_DEPENDENCIES" 21 TRUE)
        check_flag("PF_OutFlag2_SUPPORTS_GET_FLATTENED_SEQUENCE_DATA" 23 TRUE)
        check_flag("PF_OutFlag2_CUSTOM_UI_ASYNC_MANAGER" 24 TRUE)
        check_flag("PF_OutFlag2_SUPPORTS_GPU_RENDER_F32|enable_gpu_rendering" 25 TRUE)
        check_flag("PF_OutFlag2_SUPPORTS_THREADED_RENDERING|enable_threaded_rendering|enable_mfr" 27 TRUE)
        check_flag("PF_OutFlag2_MUTABLE_RENDER_SEQUENCE_DATA_SLOWER" 28 TRUE)
        check_flag("PF_OutFlag2_SUPPORTS_DIRECTX_RENDERING" 29 TRUE)

        # Apply implicit / logical out flag dependencies
        if("${_source_contents}" MATCHES "enable_smart_render|PF_OutFlag2_SUPPORTS_SMART_RENDER")
            # Smart render requires float color aware and deep color support
            math(EXPR _auto_out_flags "${_auto_out_flags} | (1 << 25)")   # PF_OutFlag_DEEP_COLOR_AWARE
            math(EXPR _auto_out_flags2 "${_auto_out_flags2} | (1 << 12)") # PF_OutFlag2_FLOAT_COLOR_AWARE
        endif()
        if("${_source_contents}" MATCHES "enable_gpu_rendering|PF_OutFlag2_SUPPORTS_GPU_RENDER_F32")
            # GPU render on Windows also requires DirectX rendering
            math(EXPR _auto_out_flags2 "${_auto_out_flags2} | (1 << 29)") # PF_OutFlag2_SUPPORTS_DIRECTX_RENDERING
        endif()
        if("${_source_contents}" MATCHES "enable_custom_ui|PF_OutFlag_CUSTOM_UI")
            # Custom UI benefits from async manager support
            math(EXPR _auto_out_flags2 "${_auto_out_flags2} | (1 << 24)") # PF_OutFlag2_CUSTOM_UI_ASYNC_MANAGER
        endif()

        if(NOT _explicit_out_flags)
            set(AETK_PIPL_OUT_FLAGS ${_auto_out_flags})
        endif()
        if(NOT _explicit_out_flags2)
            set(AETK_PIPL_OUT_FLAGS2 ${_auto_out_flags2})
        endif()
    endif()

    # --- Set template variables ---
    set(AETK_PIPL_NAME "${AETK_NAME}")
    set(AETK_PIPL_CATEGORY "${AETK_CATEGORY}")
    set(AETK_PIPL_MATCH_NAME "${AETK_MATCH_NAME}")
    set(AETK_PIPL_ENTRY "${AETK_ENTRY}")

    # --- Output paths ---
    set(_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_generated")
    file(MAKE_DIRECTORY "${_gen_dir}")

    set(_pipl_r     "${_gen_dir}/${TARGET_NAME}_PiPL.r")
    set(_pipl_rr    "${_gen_dir}/${TARGET_NAME}_PiPL.rr")
    set(_pipl_rrc   "${_gen_dir}/${TARGET_NAME}_PiPL.rrc")
    set(_pipl_rc    "${_gen_dir}/${TARGET_NAME}_PiPL.rc")
    set(_config_h   "${_gen_dir}/plugin_config.h")

    # --- Generate .r and plugin_config.h from templates ---
    configure_file(
        "${AETK_CMAKE_DIR}/pipl_template.r.in"
        "${_pipl_r}"
        @ONLY
    )
    configure_file(
        "${AETK_CMAKE_DIR}/plugin_config.h.in"
        "${_config_h}"
        @ONLY
    )

    # --- PiPL pipeline: .r -> .rr (preprocess) -> .rrc (PiPLtool) -> .rc ---
    add_custom_command(
        OUTPUT "${_pipl_rr}"
        COMMAND cl.exe /D "AE_OS_WIN"
            /I "${AE_SDK_ROOT}/Examples/Headers"
            /I "${_gen_dir}"
            /P /EP "${_pipl_r}" /Fi"${_pipl_rr}"
        DEPENDS "${_pipl_r}" "${_config_h}"
        COMMENT "Preprocessing PiPL for ${TARGET_NAME}"
    )

    add_custom_command(
        OUTPUT "${_pipl_rrc}"
        COMMAND "${PIPL_TOOL}" "${_pipl_rr}" "${_pipl_rrc}"
        DEPENDS "${_pipl_rr}"
        VERBATIM
        COMMENT "Running PiPLtool for ${TARGET_NAME}"
    )

    add_custom_command(
        OUTPUT "${_pipl_rc}"
        COMMAND ${CMAKE_COMMAND} -E copy "${_pipl_rrc}" "${_pipl_rc}"
        DEPENDS "${_pipl_rrc}"
        COMMENT "Generating RC for ${TARGET_NAME}"
    )

    # --- Resolve source paths relative to source dir ---
    set(_resolved_sources)
    foreach(_src ${AETK_SOURCES})
        if(IS_ABSOLUTE "${_src}")
            list(APPEND _resolved_sources "${_src}")
        else()
            list(APPEND _resolved_sources "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
        endif()
    endforeach()

    # --- Create the .aex shared library ---
    add_library(${TARGET_NAME} SHARED ${_resolved_sources} "${_pipl_rc}")

    target_link_libraries(${TARGET_NAME} PRIVATE aetk_core user32.lib gdi32.lib)
    if(AETK_EXTRA_LIBS)
        target_link_libraries(${TARGET_NAME} PRIVATE ${AETK_EXTRA_LIBS})
    endif()

    # Add the generated dir to includes so #include "plugin_config.h" works
    target_include_directories(${TARGET_NAME} PRIVATE "${_gen_dir}")

    # Pass metadata to C++ as definitions
    target_compile_definitions(${TARGET_NAME} PRIVATE 
        AETK_CODE_VERSION=${_pf_version}
        AETK_OUT_FLAGS=${AETK_PIPL_OUT_FLAGS}
        AETK_OUT_FLAGS2=${AETK_PIPL_OUT_FLAGS2}
    )

    set_target_properties(${TARGET_NAME} PROPERTIES
        SUFFIX ".aex"
        OUTPUT_NAME "${TARGET_NAME}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    # --- Deploy to AE plugins folder ---
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${TARGET_NAME}>" "${AE_PLUGIN_DIR}/${TARGET_NAME}.aex"
        COMMENT "Deploying ${TARGET_NAME}.aex to AE plugins folder"
    )

    message(STATUS "[AETK] ${TARGET_NAME}: ${AETK_NAME} v${AETK_VERSION_MAJOR}.${AETK_VERSION_MINOR}.${AETK_VERSION_PATCH} (PF_VERSION=${_pf_version})")

endfunction()

function(aetk_add_aegp TARGET_NAME)
    cmake_parse_arguments(
        AETK
        ""
        "NAME;ENTRY;STAGE;BUILD"
        "SOURCES;VERSION;EXTRA_LIBS"
        ${ARGN}
    )

    if(NOT AETK_NAME)
        message(FATAL_ERROR "aetk_add_aegp: NAME is required")
    endif()
    if(NOT AETK_SOURCES)
        message(FATAL_ERROR "aetk_add_aegp: SOURCES is required")
    endif()

    if(NOT AETK_ENTRY)
        set(AETK_ENTRY "EntryPointFunc")
    endif()
    if(NOT AETK_BUILD)
        set(AETK_BUILD 0)
    endif()

    if(NOT AETK_STAGE)
        set(_stage_num 3) # default: RELEASE
    elseif(AETK_STAGE STREQUAL "DEVELOP")
        set(_stage_num 0)
    elseif(AETK_STAGE STREQUAL "ALPHA")
        set(_stage_num 1)
    elseif(AETK_STAGE STREQUAL "BETA")
        set(_stage_num 2)
    elseif(AETK_STAGE STREQUAL "RELEASE")
        set(_stage_num 3)
    else()
        set(_stage_num ${AETK_STAGE})
    endif()

    list(LENGTH AETK_VERSION _ver_len)
    if(_ver_len LESS 3)
        set(AETK_VERSION_MAJOR 1)
        set(AETK_VERSION_MINOR 0)
        set(AETK_VERSION_PATCH 0)
    else()
        list(GET AETK_VERSION 0 AETK_VERSION_MAJOR)
        list(GET AETK_VERSION 1 AETK_VERSION_MINOR)
        list(GET AETK_VERSION 2 AETK_VERSION_PATCH)
    endif()

    math(EXPR _pf_version "(${AETK_VERSION_MAJOR} << 19) | (${AETK_VERSION_MINOR} << 15) | (${AETK_VERSION_PATCH} << 11) | (${_stage_num} << 9) | (${AETK_BUILD})")
    set(AETK_PIPL_VERSION ${_pf_version})

    set(AETK_PIPL_NAME "${AETK_NAME}")
    set(AETK_PIPL_ENTRY "${AETK_ENTRY}")

    set(_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_generated")
    file(MAKE_DIRECTORY "${_gen_dir}")

    set(_pipl_r     "${_gen_dir}/${TARGET_NAME}_PiPL.r")
    set(_pipl_rr    "${_gen_dir}/${TARGET_NAME}_PiPL.rr")
    set(_pipl_rrc   "${_gen_dir}/${TARGET_NAME}_PiPL.rrc")
    set(_pipl_rc    "${_gen_dir}/${TARGET_NAME}_PiPL.rc")
    set(_config_h   "${_gen_dir}/plugin_config.h")

    configure_file(
        "${AETK_CMAKE_DIR}/aegp_pipl_template.r.in"
        "${_pipl_r}"
        @ONLY
    )
    configure_file(
        "${AETK_CMAKE_DIR}/plugin_config.h.in"
        "${_config_h}"
        @ONLY
    )

    add_custom_command(
        OUTPUT "${_pipl_rr}"
        COMMAND cl.exe /D "AE_OS_WIN"
            /I "${AE_SDK_ROOT}/Examples/Headers"
            /I "${_gen_dir}"
            /P /EP "${_pipl_r}" /Fi"${_pipl_rr}"
        DEPENDS "${_pipl_r}" "${_config_h}"
        COMMENT "Preprocessing AEGP PiPL for ${TARGET_NAME}"
    )

    add_custom_command(
        OUTPUT "${_pipl_rrc}"
        COMMAND "${PIPL_TOOL}" "${_pipl_rr}" "${_pipl_rrc}"
        DEPENDS "${_pipl_rr}"
        VERBATIM
        COMMENT "Running PiPLtool for ${TARGET_NAME}"
    )

    add_custom_command(
        OUTPUT "${_pipl_rc}"
        COMMAND ${CMAKE_COMMAND} -E copy "${_pipl_rrc}" "${_pipl_rc}"
        DEPENDS "${_pipl_rrc}"
        COMMENT "Generating RC for ${TARGET_NAME}"
    )

    set(_resolved_sources)
    foreach(_src ${AETK_SOURCES})
        if(IS_ABSOLUTE "${_src}")
            list(APPEND _resolved_sources "${_src}")
        else()
            list(APPEND _resolved_sources "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
        endif()
    endforeach()

    add_library(${TARGET_NAME} SHARED ${_resolved_sources} "${_pipl_rc}")

    target_link_libraries(${TARGET_NAME} PRIVATE aetk_core user32.lib gdi32.lib)
    if(AETK_EXTRA_LIBS)
        target_link_libraries(${TARGET_NAME} PRIVATE ${AETK_EXTRA_LIBS})
    endif()

    target_include_directories(${TARGET_NAME} PRIVATE "${_gen_dir}")

    # Pass metadata to C++ as definitions
    target_compile_definitions(${TARGET_NAME} PRIVATE 
        AETK_CODE_VERSION=${_pf_version}
    )

    set_target_properties(${TARGET_NAME} PROPERTIES
        SUFFIX ".aex"
        OUTPUT_NAME "${TARGET_NAME}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${TARGET_NAME}>" "${AE_PLUGIN_DIR}/${TARGET_NAME}.aex"
        COMMENT "Deploying ${TARGET_NAME}.aex to AE plugins folder"
    )

    message(STATUS "[AETK AEGP] ${TARGET_NAME}: ${AETK_NAME} v${AETK_VERSION_MAJOR}.${AETK_VERSION_MINOR}.${AETK_VERSION_PATCH} (PF_VERSION=${_pf_version})")

endfunction()

# Standalone PiPL generation for legacy targets
function(aetk_add_pipl TARGET_NAME)
    cmake_parse_arguments(AETK "" "SOURCE" "" ${ARGN})
    
    if(NOT AETK_SOURCE)
        message(FATAL_ERROR "aetk_add_pipl: SOURCE is required")
    endif()

    set(_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_pipl_gen")
    file(MAKE_DIRECTORY "${_gen_dir}")
    
    get_filename_component(_src_name "${AETK_SOURCE}" NAME_WE)
    set(_pipl_rr    "${_gen_dir}/${_src_name}.rr")
    set(_pipl_rrc   "${_gen_dir}/${_src_name}.rrc")
    set(_pipl_rc    "${_gen_dir}/${_src_name}.rc")

    add_custom_command(
        OUTPUT "${_pipl_rr}"
        COMMAND cl.exe /D "AE_OS_WIN"
            /I "${AE_SDK_ROOT}/Examples/Headers"
            /I "${_gen_dir}"
            /P /EP "${CMAKE_CURRENT_SOURCE_DIR}/${AETK_SOURCE}" /Fi"${_pipl_rr}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${AETK_SOURCE}"
        COMMENT "Preprocessing legacy PiPL for ${TARGET_NAME}"
    )

    add_custom_command(
        OUTPUT "${_pipl_rrc}"
        COMMAND "${PIPL_TOOL}" "${_pipl_rr}" "${_pipl_rrc}"
        DEPENDS "${_pipl_rr}"
        COMMENT "Running PiPLtool for ${TARGET_NAME}"
    )

    add_custom_command(
        OUTPUT "${_pipl_rc}"
        COMMAND ${CMAKE_COMMAND} -E copy "${_pipl_rrc}" "${_pipl_rc}"
        DEPENDS "${_pipl_rrc}"
        COMMENT "Generating RC for ${TARGET_NAME}"
    )

    target_sources(${TARGET_NAME} PRIVATE "${_pipl_rc}")
endfunction()

