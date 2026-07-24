#pragma once

#include <cstddef>
#include <cstdint>

namespace pokered {

enum class FieldMenuPage : std::uint8_t {
    root,
    party,
    bag,
    trainer,
};

struct FieldMenuState {
    FieldMenuPage page{FieldMenuPage::root};
    std::size_t selected{};
    std::uint8_t input_cooldown{};
    bool open{};
};

struct FieldMenuInput {
    bool up{};
    bool down{};
    bool confirm{};
    bool back{};
    bool start{};
};

void open_field_menu(FieldMenuState& menu);
void step_field_menu(FieldMenuState& menu,
                     const FieldMenuInput& input);

} // namespace pokered
