#pragma once

class GubsyRuntime;

namespace pokered {

enum class ControlAction : int {
    left = 1,
    right = 2,
    up = 3,
    down = 4,
    confirm = 5,
    back = 6,
    start = 7,
    select = 8,
    menu = 9,
    quit = 10,
    fast_forward = 11,
};

enum class AnalogControl : int {
    fast_forward = 1,
};

inline constexpr int kControlProfileCount = 2;

struct ControlButtons {
    bool left{};
    bool right{};
    bool up{};
    bool down{};
    bool confirm{};
    bool back{};
    bool start{};
    bool select{};
    bool menu{};
    bool quit{};
    bool fast_forward{};
};

bool register_controls(GubsyRuntime& runtime, int profile);
int control_profile_id(int profile);
bool select_control_profile(GubsyRuntime& runtime, int profile);
bool reset_controls(GubsyRuntime& runtime, int profile);
void assign_unclaimed_gamepads(GubsyRuntime& runtime);
bool control_down(GubsyRuntime& runtime, ControlAction action);
ControlButtons read_controls(GubsyRuntime& runtime);

} // namespace pokered
