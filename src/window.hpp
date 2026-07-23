#pragma once

#include <filesystem>

#include <gubsy/runtime.hpp>

namespace pokered {

struct WindowInput {
    bool quit{};
    bool toggle_player_tools{};
    bool toggle_developer_tools{};
    bool previous_animation{};
    bool next_animation{};
    bool restart_animation{};
    bool reload_animation_sources{};
    bool toggle_animation_auto_advance{};
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
