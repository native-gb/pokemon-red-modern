#include "window.hpp"

#include "src/imgui_layer.hpp"

#include <SDL3/SDL.h>

#include <cstdio>

namespace pokered {

bool initialize_window(HostWindow& window, const std::filesystem::path& data_root) {
    GubsyAppConfig config;
    config.enable_mods = false;
    config.project_root = POKERED_MODERN_SOURCE_DIR;
    config.data_root = data_root.string();
    config.engine_assets_root = POKERED_MODERN_GUBSY_ASSETS_DIR;
    config.window_title = "Native GB Pokemon Red Modern";
    config.window_width = 1280;
    config.window_height = 720;
    config.render_width = 1280;
    config.render_height = 720;
    config.utility_window = true;
    config.resizable_window = true;
    config.apply_display_settings = false;

    if (!init_gubsy_runtime(window.runtime, config) || !gubsy_init_sdl_renderer(window.runtime)) {
        std::fprintf(stderr, "could not initialize Gubsy/SDL: %s\n", SDL_GetError());
        cleanup_gubsy_runtime(window.runtime);
        return false;
    }
    window.frame = gubsy_get_frame(window.runtime);
    if (window.frame.window == nullptr || window.frame.renderer == nullptr ||
        window.frame.render_target == nullptr) {
        std::fprintf(stderr, "Gubsy did not provide a complete game window\n");
        cleanup_gubsy_runtime(window.runtime);
        return false;
    }

    (void)SDL_SetWindowFullscreen(window.frame.window, false);
    (void)SDL_SetWindowSize(window.frame.window, 1280, 720);
    (void)SDL_SetWindowPosition(window.frame.window, SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED);
    (void)SDL_SyncWindow(window.frame.window);
    (void)SDL_SetRenderVSync(window.frame.renderer, 1);

    if (!init_imgui_layer(window.frame.window, window.frame.renderer)) {
        std::fprintf(stderr, "could not initialize ImGui\n");
        cleanup_gubsy_runtime(window.runtime);
        return false;
    }
    return true;
}

WindowInput poll_window_events(HostWindow& window) {
    WindowInput input;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        gubsy_process_sdl_event(window.runtime, event);
        if (event.type == SDL_EVENT_QUIT) input.quit = true;
        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) continue;
        if (event.key.key == SDLK_F1)
            input.toggle_player_tools = true;
        else if (event.key.key == SDLK_F2)
            input.toggle_developer_tools = true;
        else if (event.key.key == SDLK_LEFT)
            input.previous_animation = true;
        else if (event.key.key == SDLK_RIGHT)
            input.next_animation = true;
        else if (event.key.key == SDLK_R)
            input.restart_animation = true;
        else if (event.key.key == SDLK_F5)
            input.reload_animation_sources = true;
        else if (event.key.key == SDLK_SPACE)
            input.toggle_animation_auto_advance = true;
        else if (event.key.key == SDLK_F11) {
            const bool fullscreen =
                (SDL_GetWindowFlags(window.frame.window) & SDL_WINDOW_FULLSCREEN) != 0;
            (void)SDL_SetWindowFullscreen(window.frame.window, !fullscreen);
        }
    }
    return input;
}

void update_window(HostWindow& window, double elapsed) {
    gubsy_update_runtime(window.runtime, static_cast<float>(elapsed));
    window.frame = gubsy_get_frame(window.runtime);
}

bool draw_window(HostWindow& window) {
    return gubsy_draw_frame_to_window(window.runtime);
}

void present_window(HostWindow& window) {
    gubsy_present_frame(window.runtime);
}

void shutdown_window(HostWindow& window) {
    shutdown_imgui_layer();
    cleanup_gubsy_runtime(window.runtime);
}

} // namespace pokered
