#include "state.hpp"

namespace pokered {

void step_game(GameState& game) {
    if (!game.paused) ++game.step;
}

const char* label(Mode mode) {
    switch (mode) {
    case Mode::no_campaign:
        return "No campaign";
    case Mode::title:
        return "Title";
    case Mode::overworld:
        return "Overworld";
    case Mode::battle:
        return "Battle";
    }
    return "Unknown";
}

} // namespace pokered
