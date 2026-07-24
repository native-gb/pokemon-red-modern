cmake_minimum_required(VERSION 3.21)

project(native_gb_pokemon_red_modern VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(POKERED_MODERN_STRICT "Enable strict compiler warnings" ON)
option(POKERED_MODERN_WARN_AS_ERROR "Treat compiler warnings as errors" ON)
option(POKERED_MODERN_SANITIZERS "Enable address and undefined sanitizers" OFF)
option(POKERED_MODERN_BUILD_IMPORTER "Build the standalone ROM importer" ON)

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
        GIT_TAG f95d5644fc254d85d8406a47df23def8f89dc0dd)
    FetchContent_MakeAvailable(gubsy)
    set(POKERED_MODERN_GUBSY_DIR "${gubsy_SOURCE_DIR}")
endif()

if(POKERED_MODERN_BUILD_IMPORTER)
    add_executable(native-gb-pokemon-red-modern-import
        src/gen1_picture_codec.cpp
        src/import_battle_animations.cpp
        src/import_battle_animations_io.cpp
        src/import_battle_animations_main.cpp
        src/import_battle_rules.cpp
        src/import_boot.cpp
        src/import_campaign_programs.cpp
        src/import_encounters.cpp
        src/import_maps.cpp
        src/import_pictures.cpp
        src/import_rules.cpp
        src/import_rules_emit.cpp
        src/import_trainers.cpp
        src/import_scripts.cpp
        src/import_text.cpp
        src/naming.cpp
    )
    target_include_directories(native-gb-pokemon-red-modern-import PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    pokered_modern_target_defaults(native-gb-pokemon-red-modern-import)
endif()

add_executable(native-gb-pokemon-red-modern
    src/accuracy_rules.cpp
    src/animations.cpp
    src/battle.cpp
    src/battle_animation_lab.cpp
    src/battle_controller.cpp
    src/battle_rules.cpp
    src/battle_ui.cpp
    src/battle_view.cpp
    src/boot.cpp
    src/campaign_programs.cpp
    src/catalog.cpp
    src/capture_rules.cpp
    src/clocks.cpp
    src/controls.cpp
    src/controls_menu.cpp
    src/dev_input.cpp
    src/diagnostics.cpp
    src/encounters.cpp
    src/experience_rules.cpp
    src/field_menu.cpp
    src/interactions.cpp
    src/inventory.cpp
    src/main.cpp
    src/maps.cpp
    src/naming.cpp
    src/overlays.cpp
    src/predicates.cpp
    src/pokemon.cpp
    src/rules.cpp
    src/save.cpp
    src/render/frame.cpp
    src/render/boot.cpp
    src/render/dialogue.cpp
    src/render/field_menu.cpp
    src/render/maps.cpp
    src/render/naming.cpp
    src/sexpr.cpp
    src/source_loader.cpp
    src/state.cpp
    src/settings.cpp
    src/stat_rules.cpp
    src/symbols.cpp
    src/tools.cpp
    src/trainers.cpp
    src/window.cpp
)
target_include_directories(native-gb-pokemon-red-modern PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${POKERED_MODERN_GUBSY_DIR}/include"
    "${POKERED_MODERN_GUBSY_DIR}"
    "${POKERED_MODERN_GUBSY_DIR}/imgui"
    "${POKERED_MODERN_GUBSY_DIR}/imgui/backends")
target_compile_definitions(native-gb-pokemon-red-modern PRIVATE
    POKERED_MODERN_SOURCE_DIR="${POKERED_MODERN_RUNTIME_PROJECT_ROOT}"
    POKERED_MODERN_GUBSY_ASSETS_DIR="${POKERED_MODERN_GUBSY_DIR}/src/assets")
target_link_libraries(native-gb-pokemon-red-modern PRIVATE
    gubsy::engine SDL3::SDL3)
pokered_modern_target_defaults(native-gb-pokemon-red-modern)

if(BUILD_TESTING)
    add_executable(pokered-modern-foundation-tests
        tests/foundation_tests.cpp
        src/accuracy_rules.cpp
        src/animations.cpp
        src/battle.cpp
        src/battle_animation_lab.cpp
        src/battle_controller.cpp
        src/battle_rules.cpp
        src/battle_ui.cpp
        src/battle_view.cpp
        src/boot.cpp
        src/campaign_programs.cpp
        src/catalog.cpp
        src/capture_rules.cpp
        src/clocks.cpp
        src/diagnostics.cpp
        src/encounters.cpp
        src/experience_rules.cpp
        src/field_menu.cpp
        src/interactions.cpp
        src/inventory.cpp
        src/maps.cpp
        src/naming.cpp
        src/overlays.cpp
        src/predicates.cpp
        src/pokemon.cpp
        src/rules.cpp
        src/save.cpp
        src/sexpr.cpp
        src/source_loader.cpp
        src/state.cpp
        src/settings.cpp
        src/stat_rules.cpp
        src/symbols.cpp
        src/trainers.cpp
    )
    target_include_directories(pokered-modern-foundation-tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(pokered-modern-foundation-tests PRIVATE
        POKERED_MODERN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    pokered_modern_target_defaults(pokered-modern-foundation-tests)
    add_test(NAME pokered_modern_foundation COMMAND pokered-modern-foundation-tests)
endif()
