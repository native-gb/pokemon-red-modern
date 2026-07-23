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

        const auto now = Clock::now();
        const double elapsed = std::chrono::duration<double>(now - previous).count();
        previous = now;
        accumulator = std::min(accumulator + elapsed, 0.25);
        while (accumulator >= step_seconds) {
            pokered::step_game(game);
            accumulator -= step_seconds;
        }

        pokered::update_window(window, elapsed);
        imgui_new_frame();
        if (!pokered::render::render_frame(window.frame.renderer, window.frame.render_target,
                                           window.frame.render_width, window.frame.render_height,
                                           game, catalog) ||
            !pokered::draw_window(window)) {
            std::fprintf(stderr, "could not render frame: %s\n", SDL_GetError());
            render_failed = true;
            break;
        }

        pokered::draw_tools(tools, game, catalog,
                            renderer_name != nullptr ? renderer_name : "unknown");
        imgui_render_layer();
        pokered::present_window(window);

        ++rendered_frames;
        if (options.render_smoke && rendered_frames >= 3) running = false;
    }

    pokered::shutdown_window(window);
    return render_failed ? 1 : 0;
}
