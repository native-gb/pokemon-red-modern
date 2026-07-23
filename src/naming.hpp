#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pokered {

constexpr std::size_t kNamingRows = 5U;
constexpr std::size_t kNamingColumns = 9U;
constexpr std::size_t kNamingCells = kNamingRows * kNamingColumns;

struct NamingProfile {
    std::array<std::string, kNamingCells> uppercase;
    std::array<std::string, kNamingCells> lowercase;
    std::string uppercase_action;
    std::string lowercase_action;
    std::uint8_t maximum_length{};
};

struct NamingInput {
    bool left{};
    bool right{};
    bool up{};
    bool down{};
    bool confirm{};
    bool erase{};
    bool submit{};
    bool toggle_case{};
    const char* text{};
};

struct NamingState {
    NamingProfile profile;
    std::string heading;
    std::string value;
    std::vector<std::string> glyphs;
    std::uint8_t row{};
    std::uint8_t column{};
    std::uint8_t input_cooldown{};
    bool lowercase{};
    bool open{};
    bool decided{};
};

bool valid_naming_profile(const NamingProfile& profile);
void begin_naming(const NamingProfile& profile, std::string heading,
                  NamingState& state);
void step_naming(const NamingInput& input, NamingState& state);
const std::string& naming_cell(const NamingState& state, std::uint8_t row,
                               std::uint8_t column);
std::size_t naming_character_count(std::string_view value);
void append_typed_naming_text(std::string& value, std::string_view text,
                              std::size_t maximum_length);
void erase_last_naming_character(std::string& value);

} // namespace pokered
