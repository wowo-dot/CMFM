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

# The commit goes into the DLL's log line, so run logs name the build.
find_package(Git QUIET)
set(CMFM_COMMIT "unknown")
if(GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" describe --always --dirty --abbrev=7
                    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE CMFM_COMMIT
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()

set(CMFM_WARNINGS /W4 /WX /permissive- /utf-8)

# Pure logic, shared by the plugin and the unit tests.
add_library(cmfm_core STATIC
    src/native/system_items.cpp
    src/native/options_model.cpp)
target_include_directories(cmfm_core PUBLIC src)
target_compile_options(cmfm_core PRIVATE ${CMFM_WARNINGS})

add_library(cmfm SHARED
    src/plugin.cpp
    src/native/data_patch.cpp
    src/native/options_view.cpp
    src/native/start_menu.cpp)
# "al/al_plugin.h" is third_party/al/al_plugin.h.
target_include_directories(cmfm PRIVATE third_party src)
target_link_libraries(cmfm PRIVATE cmfm_core)
target_compile_definitions(cmfm PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
                           CMFM_COMMIT="${CMFM_COMMIT}")
target_compile_options(cmfm PRIVATE ${CMFM_WARNINGS})

enable_testing()
add_executable(test_system_items tests/test_system_items.cpp)
target_link_libraries(test_system_items PRIVATE cmfm_core)
target_compile_options(test_system_items PRIVATE ${CMFM_WARNINGS})
add_test(NAME system_items COMMAND test_system_items)
add_executable(test_options_model tests/test_options_model.cpp)
target_link_libraries(test_options_model PRIVATE cmfm_core)
target_compile_options(test_options_model PRIVATE ${CMFM_WARNINGS})
add_test(NAME options_model COMMAND test_options_model)
