#include "battle_animation_lab.hpp"
#include "catalog.hpp"
#include "render/frame.hpp"
#include "src/imgui_layer.hpp"
#include "state.hpp"
#include "tools.hpp"
#include "window.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
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
    const std::filesystem::path fixture_animation_root =
        std::filesystem::path(POKERED_MODERN_SOURCE_DIR) / "data" / "dev" / "battle_animations";
    const std::filesystem::path animation_root = std::filesystem::exists(generated_animation_root)
                                                     ? generated_animation_root
                                                     : fixture_animation_root;
    if (!pokered::load_battle_animation_lab(animation_root, animation_lab, animation_diagnostics)) {
        for (const pokered::Diagnostic& diagnostic : animation_diagnostics.entries)
            std::fprintf(stderr, "%s\n", pokered::format_diagnostic(diagnostic).c_str());
    } else {
        game.mode = pokered::Mode::battle;
        std::printf("Loaded %zu battle animation lab programs from %s\n",
                    animation_lab.entries.size(), animation_root.c_str());
    }
    pokered::HostWindow window;
    if (!pokered::initialize_window(window, data_root)) return 1;

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
        const pokered::WindowInput input = pokered::poll_window_events(window);
        if (input.quit) break;
        pokered::apply_tool_shortcuts(tools, input);
        if (input.previous_animation) pokered::previous_battle_animation_lab(animation_lab);
        if (input.next_animation) pokered::next_battle_animation_lab(animation_lab);
        if (input.previous_species) pokered::previous_battle_species(animation_lab);
        if (input.next_species) pokered::next_battle_species(animation_lab);
        if (input.cycle_battle_ui) pokered::cycle_battle_ui_mode(animation_lab);
        if (input.previous_battle_ui_selection)
            pokered::previous_battle_ui_menu_selection(animation_lab);
        if (input.next_battle_ui_selection) pokered::next_battle_ui_menu_selection(animation_lab);
        if (input.cycle_battle_status) pokered::cycle_battle_ui_status(animation_lab);
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

        const auto now = Clock::now();
        const double elapsed = std::chrono::duration<double>(now - previous).count();
        previous = now;
        accumulator = std::min(accumulator + elapsed, 0.25);
        while (accumulator >= step_seconds) {
            pokered::step_game(game);
            if (!game.paused) pokered::step_battle_animation_lab(animation_lab);
            accumulator -= step_seconds;
        }

        pokered::update_window(window, elapsed);
        imgui_new_frame();
        if (!pokered::render::render_frame(window.frame.renderer, window.frame.render_target,
                                           window.frame.render_width, window.frame.render_height,
                                           game, catalog, animation_lab) ||
            !pokered::draw_window(window)) {
            std::fprintf(stderr, "could not render frame: %s\n", SDL_GetError());
            render_failed = true;
            break;
        }

        pokered::draw_tools(tools, game, catalog, animation_lab,
                            renderer_name != nullptr ? renderer_name : "unknown");
        imgui_render_layer();
        pokered::present_window(window);

        ++rendered_frames;
        if (options.render_smoke && rendered_frames >= 3) running = false;
    }

    pokered::shutdown_window(window);
    return render_failed ? 1 : 0;
}
