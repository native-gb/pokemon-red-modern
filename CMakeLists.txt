cmake_minimum_required(VERSION 3.21)

project(native_gb_pokemon_red_modern VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(POKERED_MODERN_STRICT "Enable strict compiler warnings" ON)
option(POKERED_MODERN_WARN_AS_ERROR "Treat compiler warnings as errors" ON)
option(POKERED_MODERN_SANITIZERS "Enable address and undefined sanitizers" OFF)

function(pokered_modern_target_defaults target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(POKERED_MODERN_WARN_AS_ERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
        return()
    endif()

    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(POKERED_MODERN_STRICT)
        target_compile_options(${target} PRIVATE
            -Wconversion -Wsign-conversion -Wshadow -Wnon-virtual-dtor)
    endif()
    if(POKERED_MODERN_WARN_AS_ERROR)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
    if(POKERED_MODERN_SANITIZERS)
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

set(POKERED_MODERN_GUBSY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Splonks/gubsy"
    CACHE PATH "Gubsy source tree")
set(POKERED_MODERN_RUNTIME_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
    CACHE STRING "Project path exposed to the runtime")

set(GUB_MODE consumer CACHE STRING "" FORCE)
set(GUB_BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
set(GUB_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(GUB_ENABLE_LUA_MOD_HOST OFF CACHE BOOL "" FORCE)
set(GUB_ENABLE_STEAM OFF CACHE BOOL "" FORCE)
set(GUB_FETCH_DEPS ON CACHE BOOL "" FORCE)
set(GUB_SDL_DEPS fetch CACHE STRING "" FORCE)
if(EXISTS "${POKERED_MODERN_GUBSY_DIR}/CMakeLists.txt")
    add_subdirectory("${POKERED_MODERN_GUBSY_DIR}" gubsy)
else()
    FetchContent_Declare(gubsy
        GIT_REPOSITORY https://github.com/wegfawefgawefg/gubsy.git
        GIT_TAG c5e8140d14ff7f92da2f83e2d71e3208bae2b575)
    FetchContent_MakeAvailable(gubsy)
    set(POKERED_MODERN_GUBSY_DIR "${gubsy_SOURCE_DIR}")
endif()

add_library(pokered-modern-content STATIC
    src/content/catalog.cpp
)
target_include_directories(pokered-modern-content PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
pokered_modern_target_defaults(pokered-modern-content)

add_library(pokered-modern-runtime STATIC
    src/runtime/state.cpp
)
target_include_directories(pokered-modern-runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(pokered-modern-runtime PUBLIC pokered-modern-content)
pokered_modern_target_defaults(pokered-modern-runtime)

add_library(pokered-modern-presentation STATIC
    src/presentation/game_view.cpp
)
target_include_directories(pokered-modern-presentation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(pokered-modern-presentation PUBLIC
    pokered-modern-runtime SDL3::SDL3)
pokered_modern_target_defaults(pokered-modern-presentation)

add_library(pokered-modern-host STATIC
    src/host/tools.cpp
    src/host/window.cpp
)
target_include_directories(pokered-modern-host PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${POKERED_MODERN_GUBSY_DIR}/include"
    "${POKERED_MODERN_GUBSY_DIR}"
    "${POKERED_MODERN_GUBSY_DIR}/imgui"
    "${POKERED_MODERN_GUBSY_DIR}/imgui/backends")
target_compile_definitions(pokered-modern-host PUBLIC
    POKERED_MODERN_SOURCE_DIR="${POKERED_MODERN_RUNTIME_PROJECT_ROOT}"
    POKERED_MODERN_GUBSY_ASSETS_DIR="${POKERED_MODERN_GUBSY_DIR}/src/assets")
target_link_libraries(pokered-modern-host PUBLIC
    pokered-modern-content pokered-modern-runtime gubsy::engine SDL3::SDL3)
pokered_modern_target_defaults(pokered-modern-host)

add_executable(native-gb-pokemon-red-modern src/main.cpp)
target_compile_definitions(native-gb-pokemon-red-modern PRIVATE
    POKERED_MODERN_SOURCE_DIR="${POKERED_MODERN_RUNTIME_PROJECT_ROOT}")
target_link_libraries(native-gb-pokemon-red-modern PRIVATE
    pokered-modern-content pokered-modern-host pokered-modern-presentation
    pokered-modern-runtime)
pokered_modern_target_defaults(native-gb-pokemon-red-modern)
