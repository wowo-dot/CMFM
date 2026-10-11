# CMFM: the in-game settings menu for Monster Hunter: World mods, an
# Astera Loader plugin. Windows, MSVC, 64-bit:
#
#   cmake -S . -B local/build -A x64
#   cmake --build local/build --config Release
#
# The plugin is local/build/Release/cmfm.dll. Install it as
# <game>\AsteraLoader\plugins\cmfm\cmfm.dll (under MO2: its own mod with that
# layout). The demo client (examples/cmfm_demo, plain C) goes to
# <game>\AsteraLoader\plugins\cmfm_demo\cmfm_demo.dll.
cmake_minimum_required(VERSION 3.25)
project(cmfm VERSION 0.1.0 LANGUAGES C CXX)

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

# Pure logic, shared by the plugin and the unit tests. include/cmfm/cmfm.h is
# the published plugin interface (cmfm.menu).
add_library(cmfm_core STATIC
    src/menu/menu.cpp
    src/native/system_items.cpp
    src/native/options_model.cpp)
target_include_directories(cmfm_core PUBLIC src include)
target_compile_options(cmfm_core PRIVATE ${CMFM_WARNINGS})

add_library(cmfm SHARED
    src/plugin.cpp
    src/menu/service.cpp
    src/native/data_patch.cpp
    src/native/gui.cpp
    src/native/mods_page.cpp
    src/native/options_view.cpp
    src/native/page_class.cpp
    src/native/start_menu.cpp
    src/native/title_menu.cpp)
# "al/al_plugin.h" is third_party/al/al_plugin.h.
target_include_directories(cmfm PRIVATE third_party src)
target_link_libraries(cmfm PRIVATE cmfm_core)
target_compile_definitions(cmfm PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
                           CMFM_COMMIT="${CMFM_COMMIT}")
target_compile_options(cmfm PRIVATE ${CMFM_WARNINGS})

# The example client: a plugin page with one setting of each type, in C.
add_library(cmfm_demo SHARED examples/cmfm_demo/cmfm_demo.c)
target_include_directories(cmfm_demo PRIVATE third_party include)
target_compile_definitions(cmfm_demo PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(cmfm_demo PRIVATE ${CMFM_WARNINGS})

enable_testing()
add_executable(test_system_items tests/test_system_items.cpp)
target_link_libraries(test_system_items PRIVATE cmfm_core)
target_compile_options(test_system_items PRIVATE ${CMFM_WARNINGS})
add_test(NAME system_items COMMAND test_system_items)
add_executable(test_options_model tests/test_options_model.cpp)
target_link_libraries(test_options_model PRIVATE cmfm_core)
target_compile_options(test_options_model PRIVATE ${CMFM_WARNINGS})
add_test(NAME options_model COMMAND test_options_model)
add_executable(test_menu tests/test_menu.cpp)
target_link_libraries(test_menu PRIVATE cmfm_core)
target_compile_options(test_menu PRIVATE ${CMFM_WARNINGS})
add_test(NAME menu COMMAND test_menu)
