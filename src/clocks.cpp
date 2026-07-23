#include "clocks.hpp"

#include <algorithm>

namespace pokered {

void advance_unscaled_clocks(GameClocks& clocks, double elapsed_seconds) {
    const double bounded = std::clamp(elapsed_seconds, 0.0, 0.25);
    clocks.real_time += bounded;
    clocks.presentation_time += bounded;
    clocks.audio_time += bounded;
    clocks.music_time += bounded;
}

void advance_game_clock(GameClocks& clocks, double fixed_step_seconds) {
    clocks.game_time += fixed_step_seconds;
    ++clocks.game_steps;
}

} // namespace pokered
