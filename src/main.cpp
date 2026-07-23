#include "battle_animation_lab.hpp"
#include "boot.hpp"
#include "catalog.hpp"
#include "clocks.hpp"
#include "controls.hpp"
#include "interactions.hpp"
#include "maps.hpp"
#include "render/dialogue.hpp"
#include "render/boot.hpp"
#include "render/frame.hpp"
#include "render/maps.hpp"
#include "rules.hpp"
#include "settings.hpp"
#include "src/imgui_layer.hpp"
#include "state.hpp"
#include "tools.hpp"
#include "window.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
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

    // Host choices load before platform initialization because the selected
    // control profile must be active for the first input frame.
    const std::filesystem::path settings_path = data_root / "settings.cfg";
    pokered::PresentationSettings presentation;
    std::string settings_error;
    bool settings_writable = pokered::load_settings(settings_path, presentation, settings_error);
    if (!settings_writable)
        std::fprintf(stderr, "could not load settings: %s\n", settings_error.c_str());
    pokered::PresentationSettings saved_presentation = presentation;

    pokered::content::CatalogSummary catalog;
    pokered::GameState game;
    pokered::BootContent boot_content;
    pokered::BootState boot;
    std::string boot_error;
    const std::filesystem::path boot_cache =
        data_root / "imports" / "pokemon_red_us_rev_0" / "compiled" / "boot_content.bin";
    if (!pokered::load_boot_content(boot_cache, boot_content, boot_error))
        std::fprintf(stderr, "%s\n", boot_error.c_str());

    pokered::BattleAnimationLab animation_lab;
    pokered::Diagnostics animation_diagnostics;
    const std::filesystem::path generated_animation_root =
        data_root / "imports" / "pokemon_red_us_rev_0" / "source" / "animations" / "battle_moves";
    if (!pokered::load_battle_animation_lab(generated_animation_root, animation_lab,
                                            animation_diagnostics)) {
        for (const pokered::Diagnostic& diagnostic : animation_diagnostics.entries)
            std::fprintf(stderr, "%s\n", pokered::format_diagnostic(diagnostic).c_str());
    } else {
        std::printf("Loaded %zu battle animation lab programs from %s\n",
                    animation_lab.entries.size(), generated_animation_root.c_str());
    }

    pokered::WorldState world;
    pokered::RuleCatalog rules;
    pokered::InteractionCatalog interactions;
    std::string map_error;
    const std::filesystem::path map_cache =
        data_root / "imports" / "pokemon_red_us_rev_0" / "compiled" / "world_maps.bin";
    if (!pokered::load_world(map_cache, world, map_error))
        std::fprintf(stderr, "%s\n", map_error.c_str());
    const std::filesystem::path interaction_cache =
        data_root / "imports" / "pokemon_red_us_rev_0" / "compiled" / "world_interactions.bin";
    std::string interaction_error;
    if (!pokered::load_interactions(interaction_cache, interactions, interaction_error))
        std::fprintf(stderr, "%s\n", interaction_error.c_str());
    const std::filesystem::path rule_cache =
        data_root / "imports" / "pokemon_red_us_rev_0" / "compiled" / "pokemon_rules.bin";
    std::string rule_error;
    if (!pokered::load_rules(rule_cache, rules, rule_error))
        std::fprintf(stderr, "%s\n", rule_error.c_str());
    if (world.loaded && interactions.loaded &&
        !pokered::initialize_world_runtime(world, interactions, interaction_error))
        std::fprintf(stderr, "%s\n", interaction_error.c_str());
    if (world.loaded && interactions.loaded && rules.loaded && boot_content.loaded) {
        catalog.state = pokered::content::PackState::partial;
        catalog.campaign = "Pokemon Red";
        catalog.source = "Compiled local campaign pack";
        catalog.maps = world.maps.size();
        catalog.scripts = interactions.maps.size();
        catalog.species = rules.species.size();
        catalog.moves = rules.moves.size();
    }

    pokered::HostWindow window;
    if (!pokered::initialize_window(window, data_root, presentation.control_profile)) return 1;
    pokered::render::BootRenderResources boot_resources;
    if (boot_content.loaded &&
        !pokered::render::upload_boot_textures(window.frame.renderer, boot_content,
                                               boot_resources)) {
        std::fprintf(stderr, "could not upload imported boot textures: %s\n", SDL_GetError());
        boot_content.loaded = false;
    }
    pokered::render::WorldRenderResources world_resources;
    if (world.loaded) {
        if (!pokered::render::upload_world_textures(window.frame.renderer, world,
                                                    world_resources)) {
            std::fprintf(stderr, "could not upload imported map textures: %s\n", SDL_GetError());
            world.loaded = false;
        } else {
            std::printf("World renderer: %zu chunks, %zu texture pages, %zu animated tiles\n",
                        world_resources.terrain_chunks.size(), world_resources.terrain_pages.size(),
                        world_resources.animated_tiles.size());
        }
    }
    if (boot_content.loaded) {
        if (!pokered::begin_boot(boot_content, boot, boot_error)) {
            std::fprintf(stderr, "%s\n", boot_error.c_str());
            pokered::render::destroy_boot_textures(boot_resources);
            pokered::render::destroy_world_textures(world_resources);
            pokered::shutdown_window(window);
            return 1;
        }
        game.mode = pokered::Mode::title;
    } else if (world.loaded) {
        game.mode = pokered::Mode::overworld;
    }

    pokered::ToolState tools{
        .layout =
            options.developer_tools ? pokered::ToolLayout::developer : pokered::ToolLayout::closed,
        .arrange = options.developer_tools,
        .controller_navigation = false,
        .control_status = {},
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
    bool pending_world_activation = false;
    pokered::ControlButtons previous_controls;
    pokered::GameClocks clocks;
    bool fast_forward_toggle_active = false;
    bool tools_chord_down = false;
    pokered::BootInput pending_boot_input;

    while (running) {
        const std::uint64_t frame_started = SDL_GetTicksNS();
        if (presentation.vsync != window.vsync &&
            !pokered::apply_window_vsync(window, presentation.vsync)) {
            std::fprintf(stderr, "could not change VSync: %s\n", SDL_GetError());
            presentation.vsync = window.vsync;
        }
        const auto now = Clock::now();
        const double elapsed = std::chrono::duration<double>(now - previous).count();
        previous = now;
        const double bounded_elapsed = std::clamp(elapsed, 0.0, 0.1);
        const float frame_seconds = static_cast<float>(bounded_elapsed);
        pokered::advance_unscaled_clocks(clocks, elapsed);

        const pokered::WindowInput input = pokered::poll_window_events(window);
        if (input.quit) break;
        pokered::update_window(window, bounded_elapsed);
        pokered::apply_tool_shortcuts(tools, input);

        // Semantic controls are shared by keyboard and every assigned
        // controller. Menu and Start+Select provide controller-only tool access.
        const pokered::ControlButtons controls = pokered::read_controls(window.runtime);
        const bool menu_pressed = controls.menu && !previous_controls.menu;
        const bool tools_chord = controls.start && controls.select;
        const bool tools_chord_pressed = tools_chord && !tools_chord_down;
        if (menu_pressed || tools_chord_pressed) {
            tools.layout = tools.layout == pokered::ToolLayout::player
                               ? pokered::ToolLayout::closed
                               : pokered::ToolLayout::player;
            tools.arrange = tools.layout != pokered::ToolLayout::closed;
        }
        tools_chord_down = tools_chord;
        if (controls.quit) break;
        const bool tools_own_input = tools.layout != pokered::ToolLayout::closed;
        const bool confirm_pressed = controls.confirm && !previous_controls.confirm;
        if (!tools_own_input) pending_world_activation |= confirm_pressed;
        if (!tools_own_input && game.mode == pokered::Mode::title) {
            pending_boot_input.up_pressed |= controls.up && !previous_controls.up;
            pending_boot_input.down_pressed |= controls.down && !previous_controls.down;
            pending_boot_input.left_pressed |= controls.left && !previous_controls.left;
            pending_boot_input.right_pressed |= controls.right && !previous_controls.right;
            pending_boot_input.confirm_pressed |= confirm_pressed;
            pending_boot_input.cancel_pressed |=
                controls.back && !previous_controls.back;
            pending_boot_input.start_pressed |=
                controls.start && !previous_controls.start;
            pending_boot_input.select_pressed |=
                controls.select && !previous_controls.select;
            pending_boot_input.random =
                static_cast<std::uint8_t>((game.step * 73U + 41U) & 0xFFU);
        }

        // Fast-forward scales deterministic game work only. Real,
        // presentation, audio, and future music clocks remain wall-clock based.
        const bool fast_forward_pressed = controls.fast_forward && !previous_controls.fast_forward;
        if (!presentation.fast_forward_enabled) {
            fast_forward_toggle_active = false;
        } else if (presentation.fast_forward_toggle && !tools_own_input && fast_forward_pressed) {
            fast_forward_toggle_active = !fast_forward_toggle_active;
        }
        const bool fast_forward =
            presentation.fast_forward_enabled && !tools_own_input &&
            (presentation.fast_forward_toggle ? fast_forward_toggle_active : controls.fast_forward);
        clocks.fast_forward = fast_forward;

        if (input.toggle_lab_view && world.loaded && animation_lab.loaded)
            game.mode = game.mode == pokered::Mode::battle ? pokered::Mode::overworld
                                                           : pokered::Mode::battle;
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
            if (input.toggle_world_annotations) world.show_annotations = !world.show_annotations;
            constexpr float zoom_speed = 2.0F;
            if (input.zoom_world_in)
                pokered::zoom_world_view(world, std::exp(zoom_speed * frame_seconds));
            if (input.zoom_world_out)
                pokered::zoom_world_view(world, std::exp(-zoom_speed * frame_seconds));
            if (input.reset_world_view) pokered::reset_world_view(world);
            const float pan_speed = world.view == pokered::WorldView::world ? 3000.0F : 180.0F;
            const float pan_step = pan_speed * frame_seconds;
            if (input.pan_world_left) pokered::pan_world_view(world, -pan_step, 0.0F);
            if (input.pan_world_right) pokered::pan_world_view(world, pan_step, 0.0F);
            if (input.pan_world_up) pokered::pan_world_view(world, 0.0F, -pan_step);
            if (input.pan_world_down) pokered::pan_world_view(world, 0.0F, pan_step);
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

        const double game_elapsed =
            bounded_elapsed * (fast_forward ? presentation.fast_forward_multiplier : 1);
        accumulator = std::min(accumulator + game_elapsed, 1.0);
        while (accumulator >= step_seconds) {
            if (!game.paused) {
                pokered::step_game(game);
                if (game.mode == pokered::Mode::overworld)
                    pokered::step_world_animation(world);
                pokered::advance_game_clock(clocks, step_seconds);
            }
            if (!game.paused && !tools_own_input &&
                game.mode == pokered::Mode::title) {
                pokered::BootStepResult boot_result;
                if (!pokered::step_boot(boot_content, pending_boot_input, boot,
                                        boot_result, boot_error)) {
                    std::fprintf(stderr, "%s\n", boot_error.c_str());
                    render_failed = true;
                    running = false;
                    break;
                }
                pending_boot_input = {};
                if (boot_result.new_game_requested) {
                    if (!pokered::enter_world_at(
                            world, boot_content.new_game_map_id,
                            boot_content.new_game_x, boot_content.new_game_y,
                            boot_error)) {
                        std::fprintf(stderr, "%s\n", boot_error.c_str());
                        render_failed = true;
                        running = false;
                        break;
                    }
                    game.mode = pokered::Mode::overworld;
                }
            }
            if (!game.paused && !tools_own_input && game.mode == pokered::Mode::overworld) {
                pokered::step_world(world, interactions,
                                    {
                                        .left = controls.left,
                                        .right = controls.right,
                                        .up = controls.up,
                                        .down = controls.down,
                                        .activate = pending_world_activation,
                                    });
                pending_world_activation = false;
            }
            if (!game.paused && game.mode == pokered::Mode::battle)
                pokered::step_battle_animation_lab(animation_lab);
            accumulator -= step_seconds;
        }

        pokered::update_world_view(world, bounded_elapsed);
        imgui_new_frame();
        if (!pokered::render::render_frame(window.frame.renderer, window.frame.render_target,
                                           window.frame.render_width, window.frame.render_height,
                                           game, catalog, boot_content, boot, boot_resources,
                                           animation_lab, world, world_resources) ||
            !pokered::draw_window(window)) {
            std::fprintf(stderr, "could not render frame: %s\n", SDL_GetError());
            render_failed = true;
            break;
        }

        pokered::draw_tools(tools, window.runtime, game, catalog, boot, animation_lab,
                            world, rules, presentation, clocks,
                            renderer_name != nullptr ? renderer_name : "unknown");
        pokered::render::draw_dialogue_overlay(world);
        imgui_render_layer();
        pokered::present_window(window);

        const int render_rate = pokered::effective_render_rate(presentation);
        const std::uint64_t target_frame_nanoseconds =
            std::uint64_t{1'000'000'000} / static_cast<std::uint64_t>(render_rate);
        const std::uint64_t frame_elapsed = SDL_GetTicksNS() - frame_started;
        if (frame_elapsed < target_frame_nanoseconds)
            SDL_DelayPrecise(target_frame_nanoseconds - frame_elapsed);

        ++rendered_frames;
        if (options.render_smoke && rendered_frames >= 3) running = false;

        // Persist settings at the point of change. Smoke checks remain
        // side-effect free, and a failed destination is not hammered per frame.
        if (!options.render_smoke && settings_writable && presentation != saved_presentation) {
            if (pokered::save_settings(settings_path, presentation, settings_error))
                saved_presentation = presentation;
            else {
                std::fprintf(stderr, "could not save settings: %s\n", settings_error.c_str());
                settings_writable = false;
            }
        }
        previous_controls = controls;
    }

    pokered::render::destroy_boot_textures(boot_resources);
    pokered::render::destroy_world_textures(world_resources);
    pokered::shutdown_window(window);
    if (!options.render_smoke && settings_writable && presentation != saved_presentation &&
        !pokered::save_settings(settings_path, presentation, settings_error))
        std::fprintf(stderr, "could not save final settings: %s\n", settings_error.c_str());
    return render_failed ? 1 : 0;
}
