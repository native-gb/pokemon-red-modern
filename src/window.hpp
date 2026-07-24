#pragma once

#include "settings.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

#include <gubsy/runtime.hpp>

namespace pokered {

struct WindowInput {
    bool quit{};
    bool toggle_player_tools{};
    bool toggle_developer_tools{};
    bool toggle_lab_view{};
    bool previous_animation{};
    bool next_animation{};
    bool previous_species{};
    bool next_species{};
    bool cycle_battle_ui{};
    bool previous_battle_ui_selection{};
    bool next_battle_ui_selection{};
    bool cycle_battle_status{};
    bool restart_animation{};
    bool reload_animation_sources{};
    bool toggle_animation_auto_advance{};
    bool toggle_world_view{};
    bool toggle_world_annotations{};
    bool zoom_world_in{};
    bool zoom_world_out{};
    float zoom_world_steps{};
    bool reset_world_view{};
    bool move_player_left{};
    bool move_player_right{};
    bool move_player_up{};
    bool move_player_down{};
    bool confirm_pressed{};
    bool back_pressed{};
    bool start_pressed{};
    bool select_pressed{};
    bool pan_world_left{};
    bool pan_world_right{};
    bool pan_world_up{};
    bool pan_world_down{};
    bool erase_text{};
    bool submit_text{};
    bool keyboard_wasd_left{};
    bool keyboard_wasd_right{};
    bool keyboard_wasd_up{};
    bool keyboard_wasd_down{};
    bool naming_left{};
    bool naming_right{};
    bool naming_up{};
    bool naming_down{};
    std::string text;
    bool gamepad_changed{};
};

struct HostWindow {
    GubsyRuntime runtime;
    GubsyFrame frame;
    bool vsync{true};
    std::uint64_t next_browser_gamepad_scan_ms{};
};

int effective_render_rate(const PresentationSettings& settings);
bool initialize_window(HostWindow& window, const std::filesystem::path& data_root,
                       int control_profile);
bool apply_window_vsync(HostWindow& window, bool enabled);
bool set_window_text_input(HostWindow& window, bool enabled);
WindowInput poll_window_events(HostWindow& window);
void update_window(HostWindow& window, double elapsed);
void apply_nearest_sampling(HostWindow& window);
bool draw_window(HostWindow& window);
void present_window(HostWindow& window);
void shutdown_window(HostWindow& window);

} // namespace pokered
