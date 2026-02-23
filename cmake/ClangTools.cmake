# SPDX-License-Identifier: MIT

include_guard(GLOBAL)

function(project_add_clang_tool_targets)
    if (NOT ENABLE_CLANG_TOOLS)
        return()
    endif()

    set(options)
    set(oneValueArgs)
    set(multiValueArgs FORMAT_SOURCES LINT_SOURCES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT ARG_FORMAT_SOURCES)
        message(FATAL_ERROR "project_add_clang_tool_targets requires FORMAT_SOURCES.")
    endif()

    if (NOT ARG_LINT_SOURCES)
        set(ARG_LINT_SOURCES ${ARG_FORMAT_SOURCES})
    endif()

    find_program(CLANG_FORMAT_BIN NAMES clang-format)
    find_program(CLANG_TIDY_BIN NAMES clang-tidy)

    if (CLANG_FORMAT_BIN)
        add_custom_target(format
            COMMAND ${CLANG_FORMAT_BIN} -i ${ARG_FORMAT_SOURCES}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Formatting source files with clang-format"
            VERBATIM
        )

        add_custom_target(format-check
            COMMAND ${CLANG_FORMAT_BIN} --dry-run --Werror ${ARG_FORMAT_SOURCES}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Checking source formatting with clang-format"
            VERBATIM
        )
    else()
        message(STATUS "clang-format not found; format targets will fail when invoked.")
        add_custom_target(format
            COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found."
            COMMAND ${CMAKE_COMMAND} -E false
            VERBATIM
        )
        add_custom_target(format-check
            COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found."
            COMMAND ${CMAKE_COMMAND} -E false
            VERBATIM
        )
    endif()

    if (CLANG_TIDY_BIN)
        add_custom_target(lint
            COMMAND ${CLANG_TIDY_BIN} -p ${CMAKE_BINARY_DIR} ${ARG_LINT_SOURCES}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running clang-tidy"
            VERBATIM
        )
    else()
        message(STATUS "clang-tidy not found; lint target will fail when invoked.")
        add_custom_target(lint
            COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy not found."
            COMMAND ${CMAKE_COMMAND} -E false
            VERBATIM
        )
    endif()
endfunction()
