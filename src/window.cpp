#include "window.hpp"

#include "controls.hpp"
#include "src/imgui_layer.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>

#include <cstdint>
#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

namespace pokered {
namespace {

bool refresh_new_browser_gamepads(HostWindow& window) {
#ifdef __EMSCRIPTEN__
    const std::uint64_t now = SDL_GetTicks();
    if (now < window.next_browser_gamepad_scan_ms) return false;
    window.next_browser_gamepad_scan_ms = now + 500;

    // Browsers expose a connected controller only after its first button
    // gesture. Detect that transition independently of SDL, then let Gubsy
    // rebuild SDL's gamepad subsystem and claim the newly visible device.
    if (emscripten_sample_gamepad_data() != EMSCRIPTEN_RESULT_SUCCESS)
        return false;
    int connected = 0;
    const int slots = emscripten_get_num_gamepads();
    for (int index = 0; index < slots; ++index) {
        EmscriptenGamepadEvent gamepad{};
        if (emscripten_get_gamepad_status(index, &gamepad) ==
                EMSCRIPTEN_RESULT_SUCCESS &&
            gamepad.connected)
            ++connected;
    }

    const int opened =
        static_cast<int>(gubsy_get_gamepads(window.runtime).size());
    if (connected <= opened) {
        if (opened > 0) assign_unclaimed_gamepads(window.runtime);
        return false;
    }

    std::fprintf(stderr,
                 "[input] Browser exposed %d controller(s); refreshing SDL/Gubsy\n",
                 connected);
    gubsy_refresh_gamepads(window.runtime);
    assign_unclaimed_gamepads(window.runtime);
    return static_cast<int>(
               gubsy_get_gamepads(window.runtime).size()) >
           opened;
#else
    (void)window;
    return false;
#endif
}

} // namespace

int effective_render_rate(const PresentationSettings& settings) {
    return settings.motion_interpolation ? settings.render_rate_limit : 60;
}

bool initialize_window(HostWindow& window, const std::filesystem::path& data_root,
                       int control_profile) {
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
    if (!apply_window_vsync(window, true))
        std::fprintf(stderr, "could not enable VSync: %s\n", SDL_GetError());

    if (!register_controls(window.runtime, control_profile)) {
        std::fprintf(stderr, "could not register the control profiles\n");
        cleanup_gubsy_runtime(window.runtime);
        return false;
    }
    assign_unclaimed_gamepads(window.runtime);

    if (!init_imgui_layer(window.frame.window, window.frame.renderer)) {
        std::fprintf(stderr, "could not initialize ImGui\n");
        cleanup_gubsy_runtime(window.runtime);
        return false;
    }
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    ImGui::GetIO().Fonts->Flags |= ImFontAtlasFlags_NoBakedLines;
    ImGui::GetIO().Fonts->TexGlyphPadding = 0;
    ImGui::GetStyle().AntiAliasedLines = false;
    ImGui::GetStyle().AntiAliasedLinesUseTex = false;
    apply_nearest_sampling(window);
    return true;
}

bool apply_window_vsync(HostWindow& window, bool enabled) {
    if (!SDL_SetRenderVSync(window.frame.renderer, enabled ? 1 : 0)) return false;
    window.vsync = enabled;
    return true;
}

bool set_window_text_input(HostWindow& window, bool enabled) {
    if (SDL_TextInputActive(window.frame.window) == enabled) return true;
    return enabled ? SDL_StartTextInput(window.frame.window)
                   : SDL_StopTextInput(window.frame.window);
}

WindowInput poll_window_events(HostWindow& window) {
    WindowInput input;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        gubsy_process_sdl_event(window.runtime, event);
        if (event.type == SDL_EVENT_QUIT) input.quit = true;
        if (event.type == SDL_EVENT_TEXT_INPUT &&
            event.text.text != nullptr)
            input.text += event.text.text;
        if (event.type == SDL_EVENT_MOUSE_WHEEL) {
            float amount = event.wheel.y;
            if (event.wheel.direction ==
                SDL_MOUSEWHEEL_FLIPPED)
                amount = -amount;
            input.zoom_world_steps += amount;
        }
        if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
            assign_unclaimed_gamepads(window.runtime);
            input.gamepad_changed = true;
        } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
            input.gamepad_changed = true;
        }
        if (event.type != SDL_EVENT_KEY_DOWN) continue;
        if (event.key.key == SDLK_A || event.key.key == SDLK_LEFT)
            input.move_player_left = true;
        if (event.key.key == SDLK_D || event.key.key == SDLK_RIGHT)
            input.move_player_right = true;
        if (event.key.key == SDLK_W || event.key.key == SDLK_UP)
            input.move_player_up = true;
        if (event.key.key == SDLK_S || event.key.key == SDLK_DOWN)
            input.move_player_down = true;
        if (!event.key.repeat && event.key.key == SDLK_BACKSPACE)
            input.erase_text = true;
        if (!event.key.repeat &&
            (event.key.key == SDLK_RETURN ||
             event.key.key == SDLK_KP_ENTER))
            input.submit_text = true;
        if (!event.key.repeat &&
            (event.key.key == SDLK_E || event.key.key == SDLK_Z ||
             event.key.key == SDLK_SPACE))
            input.confirm_pressed = true;
        if (!event.key.repeat && event.key.key == SDLK_X)
            input.back_pressed = true;
        if (!event.key.repeat &&
            (event.key.key == SDLK_RETURN ||
             event.key.key == SDLK_KP_ENTER))
            input.start_pressed = true;
        if (!event.key.repeat && event.key.key == SDLK_BACKSPACE)
            input.select_pressed = true;
        if (event.key.key == SDLK_EQUALS || event.key.key == SDLK_PLUS ||
                 event.key.key == SDLK_KP_PLUS)
            input.zoom_world_in = true;
        else if (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS)
            input.zoom_world_out = true;
        if (event.key.repeat) continue;
        if (event.key.key == SDLK_F1)
            input.toggle_player_tools = true;
        else if (event.key.key == SDLK_F2)
            input.toggle_developer_tools = true;
        else if (event.key.key == SDLK_F3)
            input.toggle_world_annotations = true;
        else if (event.key.key == SDLK_B)
            input.toggle_lab_view = true;
        else if (event.key.key == SDLK_LEFTBRACKET)
            input.previous_animation = true;
        else if (event.key.key == SDLK_RIGHTBRACKET)
            input.next_animation = true;
        else if (event.key.key == SDLK_UP)
            input.previous_species = true;
        else if (event.key.key == SDLK_DOWN)
            input.next_species = true;
        else if (event.key.key == SDLK_M)
            input.cycle_battle_ui = true;
        else if (event.key.key == SDLK_PAGEUP)
            input.previous_battle_ui_selection = true;
        else if (event.key.key == SDLK_PAGEDOWN)
            input.next_battle_ui_selection = true;
        else if (event.key.key == SDLK_S)
            input.cycle_battle_status = true;
        else if (event.key.key == SDLK_R)
            input.restart_animation = true;
        else if (event.key.key == SDLK_F5)
            input.reload_animation_sources = true;
        else if (event.key.key == SDLK_SPACE)
            input.toggle_animation_auto_advance = true;
        else if (event.key.key == SDLK_TAB)
            input.toggle_world_view = true;
        else if (event.key.key == SDLK_0 || event.key.key == SDLK_KP_0)
            input.reset_world_view = true;
        else if (event.key.key == SDLK_F11) {
            const bool fullscreen =
                (SDL_GetWindowFlags(window.frame.window) & SDL_WINDOW_FULLSCREEN) != 0;
            (void)SDL_SetWindowFullscreen(window.frame.window, !fullscreen);
        }
    }
    input.gamepad_changed |= refresh_new_browser_gamepads(window);
    const bool* keyboard = SDL_GetKeyboardState(nullptr);
    input.move_player_left = keyboard[SDL_SCANCODE_A] || keyboard[SDL_SCANCODE_LEFT];
    input.move_player_right = keyboard[SDL_SCANCODE_D] || keyboard[SDL_SCANCODE_RIGHT];
    input.move_player_up = keyboard[SDL_SCANCODE_W] || keyboard[SDL_SCANCODE_UP];
    input.move_player_down = keyboard[SDL_SCANCODE_S] || keyboard[SDL_SCANCODE_DOWN];
    input.pan_world_left = keyboard[SDL_SCANCODE_J];
    input.pan_world_right = keyboard[SDL_SCANCODE_L];
    input.pan_world_up = keyboard[SDL_SCANCODE_I];
    input.pan_world_down = keyboard[SDL_SCANCODE_K];
    input.zoom_world_in = keyboard[SDL_SCANCODE_EQUALS] || keyboard[SDL_SCANCODE_KP_PLUS];
    input.zoom_world_out = keyboard[SDL_SCANCODE_MINUS] || keyboard[SDL_SCANCODE_KP_MINUS];
    input.keyboard_wasd_left = keyboard[SDL_SCANCODE_A];
    input.keyboard_wasd_right = keyboard[SDL_SCANCODE_D];
    input.keyboard_wasd_up = keyboard[SDL_SCANCODE_W];
    input.keyboard_wasd_down = keyboard[SDL_SCANCODE_S];
    input.naming_left = keyboard[SDL_SCANCODE_LEFT];
    input.naming_right = keyboard[SDL_SCANCODE_RIGHT];
    input.naming_up = keyboard[SDL_SCANCODE_UP];
    input.naming_down = keyboard[SDL_SCANCODE_DOWN];
    return input;
}

void update_window(HostWindow& window, double elapsed) {
    // SDL events maintain the device list; this snapshot supplies the current
    // buttons, triggers, and sticks consumed by semantic control bindings.
    gubsy_update_device_state(window.runtime);
    gubsy_update_runtime(window.runtime, static_cast<float>(elapsed));
    window.frame = gubsy_get_frame(window.runtime);
    apply_nearest_sampling(window);
}

void apply_nearest_sampling(HostWindow& window) {
    if (window.frame.render_target != nullptr)
        (void)SDL_SetTextureScaleMode(
            window.frame.render_target, SDL_SCALEMODE_NEAREST);
    if (!imgui_is_initialized()) return;
    const ImTextureID texture_id =
        ImGui::GetIO().Fonts->TexRef.GetTexID();
    if (texture_id == ImTextureID_Invalid) return;
    SDL_Texture* font_texture = reinterpret_cast<SDL_Texture*>(
        static_cast<std::uintptr_t>(texture_id));
    if (font_texture != nullptr)
        (void)SDL_SetTextureScaleMode(
            font_texture, SDL_SCALEMODE_NEAREST);
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
