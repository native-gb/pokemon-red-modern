#pragma once

#include <filesystem>

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
    bool zoom_world_in{};
    bool zoom_world_out{};
    bool reset_world_view{};
    bool pan_world_left{};
    bool pan_world_right{};
    bool pan_world_up{};
    bool pan_world_down{};
};

struct HostWindow {
    GubsyRuntime runtime;
    GubsyFrame frame;
};

bool initialize_window(HostWindow& window, const std::filesystem::path& data_root);
WindowInput poll_window_events(HostWindow& window);
void update_window(HostWindow& window, double elapsed);
bool draw_window(HostWindow& window);
void present_window(HostWindow& window);
void shutdown_window(HostWindow& window);

} // namespace pokered
