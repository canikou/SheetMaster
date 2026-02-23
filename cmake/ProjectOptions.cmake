# SPDX-License-Identifier: MIT

include_guard(GLOBAL)

function(project_apply_warnings target)
    if (NOT ENABLE_WARNINGS)
        return()
    endif()

    if (MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:__cplusplus)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(project_apply_sanitizers target)
    if (NOT ENABLE_SANITIZERS)
        return()
    endif()

    if (WIN32 OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        return()
    endif()

    target_compile_options(${target} PRIVATE
        "$<$<CONFIG:Debug>:-fsanitize=address,undefined>"
        "$<$<CONFIG:Debug>:-fno-omit-frame-pointer>"
    )
    target_link_options(${target} PRIVATE
        "$<$<CONFIG:Debug>:-fsanitize=address,undefined>"
    )
endfunction()

function(project_enable_ipo_for_target target)
    if (NOT ENABLE_IPO)
        return()
    endif()

    if (NOT DEFINED PROJECT_IPO_SUPPORTED)
        include(CheckIPOSupported)
        check_ipo_supported(RESULT _ipo_supported OUTPUT _ipo_error)
        set(PROJECT_IPO_SUPPORTED "${_ipo_supported}" CACHE INTERNAL "IPO support status")
        set(PROJECT_IPO_ERROR "${_ipo_error}" CACHE INTERNAL "IPO support error")
    endif()

    if (PROJECT_IPO_SUPPORTED)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    else()
        message(STATUS "IPO not enabled for ${target}: ${PROJECT_IPO_ERROR}")
    endif()
endfunction()

function(project_apply_default_build_options target)
    project_apply_warnings(${target})
    project_apply_sanitizers(${target})
    project_enable_ipo_for_target(${target})
endfunction()
