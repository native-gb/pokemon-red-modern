#pragma once

#include "diagnostics.hpp"
#include "symbols.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pokered {

enum class BattleUiMode {
    command,
    moves,
    safari,
    message,
};

struct BattleUiPoint {
    std::uint8_t x{};
    std::uint8_t y{};
};

struct BattleUiBox {
    std::uint8_t left{};
    std::uint8_t top{};
    std::uint8_t right{};
    std::uint8_t bottom{};
};

struct BattleUiTileStyle {
    std::uint8_t top_left{};
    std::uint8_t horizontal{};
    std::uint8_t top_right{};
    std::uint8_t vertical{};
    std::uint8_t bottom_left{};
    std::uint8_t bottom_right{};
    std::uint8_t blank{};
    std::uint8_t cursor{};
    std::uint8_t hp_label{};
    std::uint8_t hp_left{};
    std::uint8_t hp_empty{};
    std::uint8_t hp_full{};
    std::uint8_t hp_right{};
    std::uint8_t level{};
};

struct BattleUiTilePlacement {
    BattleUiPoint position;
    std::uint8_t tile{};
};

struct BattleHudLayout {
    BattleUiPoint name;
    BattleUiPoint level;
    BattleUiPoint condition;
    BattleUiPoint hp_bar;
    std::vector<BattleUiTilePlacement> frame;
    bool show_hp_numbers{};
    std::uint8_t current_hp_right{};
    std::uint8_t hp_number_y{};
    BattleUiPoint hp_separator;
    std::uint8_t maximum_hp_right{};
};

struct BattleCommandSlot {
    Symbol key;
    Symbol on_select;
    std::string label;
    std::uint8_t x{};
    std::uint8_t y{};
    bool enabled{true};
    std::optional<std::uint16_t> count;
    std::uint8_t count_right{};
};

struct BattleCommandMenu {
    Symbol key;
    BattleUiBox box;
    bool show_player{true};
    std::vector<BattleCommandSlot> slots;
    std::size_t selected{};
};

struct BattleMoveMenuLayout {
    BattleUiBox information_box;
    BattleUiBox list_box;
    std::vector<BattleUiTilePlacement> joins;
    BattleUiPoint first_move;
    BattleUiPoint cursor;
    BattleUiPoint type_label;
    BattleUiPoint type_slash;
    BattleUiPoint type_value;
    std::uint8_t current_pp_right{};
    BattleUiPoint pp_slash;
    std::uint8_t maximum_pp_right{};
    std::string type_label_text;
    std::string disabled_text;
};

struct BattleMessageLayout {
    BattleUiBox box;
    std::array<BattleUiPoint, 2> lines;
};

struct BattleConditionText {
    Symbol status;
    std::string text;
};

struct BattleUiDefinition {
    BattleUiTileStyle tiles;
    BattleHudLayout enemy_hud;
    BattleHudLayout player_hud;
    BattleCommandMenu standard_commands;
    BattleCommandMenu safari_commands;
    BattleMoveMenuLayout move_menu;
    BattleMessageLayout message;
    std::string zero_hp_condition;
    std::vector<BattleConditionText> conditions;
};

struct BattleHudState {
    std::string name;
    std::uint8_t level{25};
    std::uint16_t current_hp{63};
    std::uint16_t maximum_hp{83};
    Symbol status;
    bool visible{true};
};

struct BattleMoveOption {
    std::string name;
    std::string type;
    std::uint8_t current_pp{};
    std::uint8_t maximum_pp{};
    bool enabled{true};
};

struct BattleUiState {
    BattleUiMode mode{BattleUiMode::command};
    BattleUiDefinition definition;
    BattleHudState player;
    BattleHudState enemy;
    std::array<BattleMoveOption, 4> moves;
    std::size_t selected_move{};
    std::array<std::string, 2> message_lines;
};

using BattleTileMap = std::array<std::uint8_t, 20U * 18U>;

bool load_battle_ui_source(const std::filesystem::path& root, BattleUiState& result,
                           Diagnostics& diagnostics);
void set_battle_ui_species(BattleUiState& state, std::string_view species_name);
void next_battle_ui_mode(BattleUiState& state);
void next_battle_ui_selection(BattleUiState& state);
void previous_battle_ui_selection(BattleUiState& state);
void move_battle_ui_selection(BattleUiState& state, int x, int y);
void next_battle_ui_status(BattleUiState& state);
bool compose_battle_ui(const BattleUiState& state, BattleTileMap& result, std::string& error);
std::string_view label(BattleUiMode mode);

} // namespace pokered
