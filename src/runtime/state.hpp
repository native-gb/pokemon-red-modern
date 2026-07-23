#pragma once

#include <cstdint>

namespace pokered {

enum class Mode {
    no_campaign,
    title,
    overworld,
    battle,
};

struct GameState {
    Mode mode{Mode::no_campaign};
    std::uint64_t step{};
    bool paused{};
};

void step_game(GameState& game);
const char* label(Mode mode);

} // namespace pokered
