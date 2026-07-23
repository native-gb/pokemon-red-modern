#pragma once

#include <cstdint>

namespace pokered {

struct GameClocks {
    double game_time{};
    double real_time{};
    double presentation_time{};
    double audio_time{};
    double music_time{};
    std::uint64_t game_steps{};
    bool fast_forward{};
};

void advance_unscaled_clocks(GameClocks& clocks, double elapsed_seconds);
void advance_game_clock(GameClocks& clocks, double fixed_step_seconds);

} // namespace pokered
