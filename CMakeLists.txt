cmake_minimum_required(VERSION 3.20)

project(SheetMaster LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/include/*.h"
    "${CMAKE_SOURCE_DIR}/include/*.hpp"
)

set(APP_RESOURCES
    "${CMAKE_SOURCE_DIR}/resources/app.qrc"
)

set(APP_PLATFORM_RESOURCES)
if (WIN32)
    list(APPEND APP_PLATFORM_RESOURCES
        "${CMAKE_SOURCE_DIR}/resources/app.rc"
    )
endif()

if (WIN32)
    add_executable(app WIN32 ${SOURCES} ${HEADERS} ${APP_RESOURCES} ${APP_PLATFORM_RESOURCES})
else()
    add_executable(app ${SOURCES} ${HEADERS} ${APP_RESOURCES})
endif()
target_include_directories(app PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(app PRIVATE Qt6::Widgets)

if (WIN32)
    target_compile_definitions(app PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if (MSVC)
    target_compile_options(app PRIVATE /W4 /permissive-)
else()
    target_compile_options(app PRIVATE -Wall -Wextra -Wpedantic)
endif()
