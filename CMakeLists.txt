cmake_minimum_required(VERSION 3.21)
set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum macOS version" FORCE)

project(govorirc
    VERSION 0.1.0
    DESCRIPTION "govorIRC server and client"
    LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are disabled. Run cmake from a separate build dir:\n"
        "  cmake -B build -S .")
endif()

get_property(_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_is_multi_config)
    set(CMAKE_CONFIGURATION_TYPES "Debug;RelWithDebInfo;Release"
        CACHE STRING "" FORCE)
else()
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
            "Build type: Debug, RelWithDebInfo, Release" FORCE)
    endif()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug RelWithDebInfo Release)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

foreach(_cfg Debug RelWithDebInfo Release)
    string(TOUPPER ${_cfg} _CFG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_CFG} ${CMAKE_BINARY_DIR}/bin/${_cfg})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_CFG} ${CMAKE_BINARY_DIR}/lib/${_cfg})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_CFG} ${CMAKE_BINARY_DIR}/lib/${_cfg})
endforeach()

add_library(govorirc_common INTERFACE)
add_library(govorirc::common ALIAS govorirc_common)

target_compile_features(govorirc_common INTERFACE cxx_std_23)

if(MSVC)
    target_compile_options(govorirc_common INTERFACE
        /W4 /permissive- /Zc:__cplusplus /utf-8 /EHsc /MP)
    target_compile_definitions(govorirc_common INTERFACE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0A00)
    target_compile_options(govorirc_common INTERFACE
        $<$<CONFIG:Debug>:/Od /Zi /RTC1>
        $<$<CONFIG:RelWithDebInfo>:/O2 /Zi>
        $<$<CONFIG:Release>:/O2>)
else()
    target_compile_options(govorirc_common INTERFACE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wconversion -Wsign-conversion
        -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual
        $<$<CONFIG:Debug>:-O0 -g3>
        $<$<CONFIG:RelWithDebInfo>:-O2 -g>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>)
endif()

option(GOVORIRC_WERROR "Treat warnings as errors" OFF)
if(GOVORIRC_WERROR)
    if(MSVC)
        target_compile_options(govorirc_common INTERFACE /WX)
    else()
        target_compile_options(govorirc_common INTERFACE -Werror)
    endif()
endif()

option(GOVORIRC_SANITIZE "Enable ASan+UBSan (Debug, Clang/GCC)" OFF)
if(GOVORIRC_SANITIZE AND NOT MSVC)
    target_compile_options(govorirc_common INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined -fno-omit-frame-pointer>)
    target_link_options(govorirc_common INTERFACE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_msg)
if(_ipo_ok)
    set_property(TARGET govorirc_common PROPERTY
        INTERFACE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

set(GOVORIRC_SHARED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shared)
target_include_directories(govorirc_common INTERFACE ${GOVORIRC_SHARED_DIR})

find_package(OpenSSL REQUIRED)

add_library(govorirc_curses INTERFACE)
add_library(govorirc::curses ALIAS govorirc_curses)

if(WIN32)
    find_path(PDCURSES_INCLUDE_DIR curses.h REQUIRED)
    find_library(PDCURSES_LIBRARY NAMES pdcurses REQUIRED)
    target_include_directories(govorirc_curses INTERFACE ${PDCURSES_INCLUDE_DIR})
    target_link_libraries(govorirc_curses INTERFACE ${PDCURSES_LIBRARY})
    target_compile_definitions(govorirc_curses INTERFACE GOVOR_PDCURSES)
else()
    if(APPLE)
        list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/ncurses")
    endif()
    set(CURSES_NEED_NCURSES TRUE)
    set(CURSES_NEED_WIDE TRUE)
    find_package(Curses REQUIRED)
    target_include_directories(govorirc_curses INTERFACE ${CURSES_INCLUDE_DIRS})
    target_link_libraries(govorirc_curses INTERFACE ${CURSES_LIBRARIES})
endif()

add_library(govorirc_net INTERFACE)
add_library(govorirc::net ALIAS govorirc_net)
if(WIN32)
    target_link_libraries(govorirc_net INTERFACE ws2_32 mswsock)
endif()
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(govorirc_net INTERFACE Threads::Threads)
endif()

add_subdirectory(govord)
add_subdirectory(govorirc)
