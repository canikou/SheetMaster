cmake_minimum_required(VERSION 3.21)

# ---- Edit this ONE variable per project ----
set(APP_NAME "SheetMaster")
# --------------------------------------------
project(${APP_NAME} VERSION 1.1.0 LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

option(ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(ENABLE_SANITIZERS "Enable sanitizers for Debug builds (GCC/Clang)" ON)
option(ENABLE_IPO "Enable interprocedural optimization in Release builds" ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CORE_TARGET "${APP_NAME}Core")

add_library(${CORE_TARGET}
    include/piano_assist/floating_overlay_window.hpp
    include/piano_assist/keyboard.hpp
    include/piano_assist/main_window.hpp
    include/piano_assist/settings_store.hpp
    include/piano_assist/song_parser.hpp
    include/piano_assist/song_repository.hpp
    include/piano_assist/tag_store.hpp
    include/piano_assist/types.hpp
    src/floating_overlay_window.cpp
    src/keyboard.cpp
    src/main_window.cpp
    src/settings_store.cpp
    src/song_parser.cpp
    src/song_repository.cpp
    src/tag_store.cpp
)
target_include_directories(${CORE_TARGET} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(${CORE_TARGET} PUBLIC cxx_std_20)
target_link_libraries(${CORE_TARGET} PUBLIC Qt6::Widgets)
if (WIN32)
    target_compile_definitions(${CORE_TARGET} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

add_executable(${APP_NAME}
    src/main.cpp
)
target_link_libraries(${APP_NAME} PRIVATE ${CORE_TARGET})
target_compile_definitions(${APP_NAME} PRIVATE APP_VERSION="${PROJECT_VERSION}")

# Stable dev binary name: Debug/Dev builds -> app(.exe), Release -> APP_NAME
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(${APP_NAME} PROPERTIES OUTPUT_NAME "${APP_NAME}")
else()
    set_target_properties(${APP_NAME} PROPERTIES OUTPUT_NAME "app")
endif()

# ---- Qt6 resources ----
set(APP_RC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
set(APP_QRC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.qrc")

if (WIN32 AND EXISTS "${APP_RC_PATH}")
    target_sources(${APP_NAME} PRIVATE ${APP_RC_PATH})
endif()

if (EXISTS "${APP_QRC_PATH}")
    target_sources(${APP_NAME} PRIVATE ${APP_QRC_PATH})
endif()

if (ENABLE_WARNINGS)
    if (MSVC)
        target_compile_options(${CORE_TARGET} PRIVATE /W4 /permissive- /Zc:__cplusplus)
        target_compile_options(${APP_NAME} PRIVATE /W4 /permissive- /Zc:__cplusplus)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${CORE_TARGET} PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(${APP_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

set(SANITIZERS_ACTIVE OFF)
if (ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT WIN32)
    set(SANITIZERS_ACTIVE ON)
    target_compile_options(${CORE_TARGET} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${CORE_TARGET} PRIVATE -fsanitize=address,undefined)
    target_compile_options(${APP_NAME} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${APP_NAME} PRIVATE -fsanitize=address,undefined)
endif()

if (ENABLE_IPO AND CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if (ipo_supported)
        set_property(TARGET ${CORE_TARGET} ${APP_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

if (BUILD_TESTING)
    add_executable(${APP_NAME}_tests
        tests/core_tests.cpp
    )
    target_link_libraries(${APP_NAME}_tests PRIVATE ${CORE_TARGET})
    target_compile_features(${APP_NAME}_tests PRIVATE cxx_std_20)

    if (ENABLE_WARNINGS)
        if (MSVC)
            target_compile_options(${APP_NAME}_tests PRIVATE /W4 /permissive- /Zc:__cplusplus)
        elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${APP_NAME}_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endif()

    if (SANITIZERS_ACTIVE)
        target_compile_options(${APP_NAME}_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${APP_NAME}_tests PRIVATE -fsanitize=address,undefined)
    endif()

    add_test(NAME ${APP_NAME}.core COMMAND ${APP_NAME}_tests)
endif()

install(TARGETS ${CORE_TARGET} ${APP_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
