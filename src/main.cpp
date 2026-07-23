#include "battle_animation_lab.hpp"
#include "catalog.hpp"
#include "maps.hpp"
#include "render/frame.hpp"
#include "render/maps.hpp"
#include "src/imgui_layer.hpp"
#include "state.hpp"
#include "tools.hpp"
#include "window.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string_view>

namespace {

struct LaunchOptions {
    bool help{};
    bool render_smoke{};
    bool developer_tools{};
};

bool parse_options(int argc, char** argv, LaunchOptions& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument == "--help" || argument == "-h")
            options.help = true;
        else if (argument == "--render-smoke")
            options.render_smoke = true;
        else if (argument == "--tools")
            options.developer_tools = true;
        else
            return false;
    }
    return true;
}

void print_usage(const char* executable) {
    std::printf("Usage: %s [--render-smoke] [--tools]\n", executable);
}

} // namespace

int main(int argc, char** argv) {
    LaunchOptions options;
    if (!parse_options(argc, argv, options)) {
        print_usage(argv[0]);
        return 2;
    }
    if (options.help) {
        print_usage(argv[0]);
        return 0;
    }

    const std::filesystem::path data_root =
        std::filesystem::path(POKERED_MODERN_SOURCE_DIR) / "data" / "runtime";
    std::error_code directory_error;
    std::filesystem::create_directories(data_root, directory_error);
    if (directory_error) {
        std::fprintf(stderr, "could not create runtime data directory: %s\n",
                     directory_error.message().c_str());
        return 1;
    }

    pokered::content::CatalogSummary catalog;
    pokered::GameState game;
    pokered::BattleAnimationLab animation_lab;
    pokered::Diagnostics animation_diagnostics;
    const std::filesystem::path generated_animation_root =
        data_root / "imports" / "pokemon_red_us_rev_0" / "source" / "animations" / "battle_moves";
    if (!pokered::load_battle_animation_lab(generated_animation_root, animation_lab,
                                            animation_diagnostics)) {
        for (const pokered::Diagnostic& diagnostic : animation_diagnostics.entries)
            std::fprintf(stderr, "%s\n", pokered::format_diagnostic(diagnostic).c_str());
    } else {
        game.mode = pokered::Mode::battle;
        std::printf("Loaded %zu battle animation lab programs from %s\n",
                    animation_lab.entries.size(), generated_animation_root.c_str());
    }

    pokered::WorldState world;
    std::string map_error;
    const std::filesystem::path map_cache =
        data_root / "imports" / "pokemon_red_us_rev_0" / "compiled" /
        "world_maps.bin";
    if (!pokered::load_world(map_cache, world, map_error))
        std::fprintf(stderr, "%s\n", map_error.c_str());

    pokered::HostWindow window;
    if (!pokered::initialize_window(window, data_root)) return 1;
    pokered::render::WorldRenderResources world_resources;
    if (world.loaded) {
        if (!pokered::render::upload_world_textures(window.frame.renderer, world,
                                                    world_resources)) {
            std::fprintf(stderr, "could not upload imported map textures: %s\n",
                         SDL_GetError());
            world.loaded = false;
        } else {
            game.mode = pokered::Mode::overworld;
        }
    }

    pokered::ToolState tools{
        .layout =
            options.developer_tools ? pokered::ToolLayout::developer : pokered::ToolLayout::closed,
        .arrange = options.developer_tools,
    };
    const char* renderer_name = SDL_GetRendererName(window.frame.renderer);
    std::printf("SDL renderer: %s\n", renderer_name != nullptr ? renderer_name : "unknown");
    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();
    double accumulator = 0.0;
    constexpr double step_seconds = 1.0 / 60.0;
    int rendered_frames = 0;
    bool running = true;
    bool render_failed = false;

    while (running) {
        const auto now = Clock::now();
        const double elapsed =
            std::chrono::duration<double>(now - previous).count();
        previous = now;
        const float frame_seconds =
            static_cast<float>(std::clamp(elapsed, 0.0, 0.1));

        const pokered::WindowInput input = pokered::poll_window_events(window);
        if (input.quit) break;
        pokered::apply_tool_shortcuts(tools, input);
        if (input.toggle_lab_view && world.loaded && animation_lab.loaded)
            game.mode = game.mode == pokered::Mode::overworld ? pokered::Mode::battle
                                                              : pokered::Mode::overworld;
        if (input.previous_animation) {
            if (game.mode == pokered::Mode::overworld)
                pokered::select_previous_map(world);
            else
                pokered::previous_battle_animation_lab(animation_lab);
        }
        if (input.next_animation) {
            if (game.mode == pokered::Mode::overworld)
                pokered::select_next_map(world);
            else
                pokered::next_battle_animation_lab(animation_lab);
        }
        if (game.mode == pokered::Mode::overworld) {
            if (input.toggle_world_view) pokered::toggle_world_view(world);
            constexpr float zoom_speed = 2.0F;
            if (input.zoom_world_in)
                pokered::zoom_world_view(
                    world, std::exp(zoom_speed * frame_seconds));
            if (input.zoom_world_out)
                pokered::zoom_world_view(
                    world, std::exp(-zoom_speed * frame_seconds));
            if (input.reset_world_view) pokered::reset_world_view(world);
            const float pan_speed =
                world.view == pokered::WorldView::world ? 3000.0F : 180.0F;
            const float pan_step = pan_speed * frame_seconds;
            if (input.pan_world_left)
                pokered::pan_world_view(world, -pan_step, 0.0F);
            if (input.pan_world_right)
                pokered::pan_world_view(world, pan_step, 0.0F);
            if (input.pan_world_up)
                pokered::pan_world_view(world, 0.0F, -pan_step);
            if (input.pan_world_down)
                pokered::pan_world_view(world, 0.0F, pan_step);
        }
        if (game.mode == pokered::Mode::battle && input.previous_species)
            pokered::previous_battle_species(animation_lab);
        if (game.mode == pokered::Mode::battle && input.next_species)
            pokered::next_battle_species(animation_lab);
        if (input.cycle_battle_ui) pokered::cycle_battle_ui_mode(animation_lab);
        if (input.previous_battle_ui_selection)
            pokered::previous_battle_ui_menu_selection(animation_lab);
        if (input.next_battle_ui_selection) pokered::next_battle_ui_menu_selection(animation_lab);
        if (game.mode == pokered::Mode::battle && input.cycle_battle_status)
            pokered::cycle_battle_ui_status(animation_lab);
        if (input.restart_animation) pokered::restart_battle_animation_lab(animation_lab);
        if (input.reload_animation_sources) {
            pokered::Diagnostics reload_diagnostics;
            if (!pokered::reload_battle_animation_lab(animation_lab, reload_diagnostics)) {
                for (const pokered::Diagnostic& diagnostic : reload_diagnostics.entries)
                    std::fprintf(stderr, "%s\n", pokered::format_diagnostic(diagnostic).c_str());
            }
        }
        if (input.toggle_animation_auto_advance)
            animation_lab.auto_advance = !animation_lab.auto_advance;

        accumulator = std::min(accumulator + elapsed, 0.25);
        while (accumulator >= step_seconds) {
            pokered::step_game(game);
            pokered::step_world_animation(world);
            if (!game.paused && game.mode == pokered::Mode::battle)
                pokered::step_battle_animation_lab(animation_lab);
            accumulator -= step_seconds;
        }

        pokered::update_window(window, elapsed);
        pokered::update_world_view(world, elapsed);
        imgui_new_frame();
        if (!pokered::render::render_frame(window.frame.renderer, window.frame.render_target,
                                           window.frame.render_width, window.frame.render_height,
                                           game, catalog, animation_lab, world,
                                           world_resources) ||
            !pokered::draw_window(window)) {
            std::fprintf(stderr, "could not render frame: %s\n", SDL_GetError());
            render_failed = true;
            break;
        }

        pokered::draw_tools(tools, game, catalog, animation_lab, world,
                            renderer_name != nullptr ? renderer_name : "unknown");
        imgui_render_layer();
        pokered::present_window(window);

        ++rendered_frames;
        if (options.render_smoke && rendered_frames >= 3) running = false;
    }

    pokered::render::destroy_world_textures(world_resources);
    pokered::shutdown_window(window);
    return render_failed ? 1 : 0;
}
