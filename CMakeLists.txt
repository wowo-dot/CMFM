# CMFM: the in-game settings menu for Monster Hunter: World mods, an
# Astera Loader plugin. Windows, MSVC, 64-bit:
#
#   cmake -S . -B local/build -A x64
#   cmake --build local/build --config Release
#
# The plugin is local/build/Release/cmfm.dll. Install it as
# <game>\AsteraLoader\plugins\cmfm\cmfm.dll (under MO2: its own mod with that
# layout).
cmake_minimum_required(VERSION 3.25)
project(cmfm VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "CMFM is 64-bit: configure with -A x64")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# The static C runtime: players need no Visual C++ runtime installed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(cmfm SHARED src/plugin.cpp)
# "al/al_plugin.h" is third_party/al/al_plugin.h.
target_include_directories(cmfm PRIVATE third_party)
target_compile_definitions(cmfm PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(cmfm PRIVATE /W4 /WX /permissive- /utf-8)
