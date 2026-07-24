#include "battle_ui.hpp"

#include "source_loader.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pokered {
namespace {

std::size_t tile_index(std::size_t x, std::size_t y) {
    return y * 20U + x;
}

const sexpr::Atom* argument(const sexpr::Form& form, std::size_t index, sexpr::AtomKind kind,
                            Diagnostics& diagnostics) {
    const sexpr::Atom* value = sexpr::argument(form, index);
    if (value == nullptr || value->kind != kind) {
        add_error(diagnostics, form.source, "invalid_battle_ui_field",
                  "'" + form.head.symbol.text + "' has an invalid argument");
        return nullptr;
    }
    return value;
}

bool exact_arguments(const sexpr::Form& form, std::size_t count, Diagnostics& diagnostics) {
    if (form.arguments.size() == count && form.children.empty()) return true;
    add_error(diagnostics, form.source, "invalid_battle_ui_field",
              "'" + form.head.symbol.text + "' has the wrong number of arguments");
    return false;
}

bool integer_u8(const sexpr::Form& form, std::size_t index, std::uint8_t& result,
                Diagnostics& diagnostics) {
    const sexpr::Atom* value = argument(form, index, sexpr::AtomKind::integer, diagnostics);
    if (value == nullptr) return false;
    if (value->integer < 0 ||
        value->integer > static_cast<std::int64_t>(std::numeric_limits<std::uint8_t>::max())) {
        add_error(diagnostics, value->source, "battle_ui_integer_out_of_range",
                  "battle UI byte value is outside 0..255");
        return false;
    }
    result = static_cast<std::uint8_t>(value->integer);
    return true;
}

bool integer_u16(const sexpr::Form& form, std::size_t index, std::uint16_t& result,
                 Diagnostics& diagnostics) {
    const sexpr::Atom* value = argument(form, index, sexpr::AtomKind::integer, diagnostics);
    if (value == nullptr) return false;
    if (value->integer < 0 ||
        value->integer > static_cast<std::int64_t>(std::numeric_limits<std::uint16_t>::max())) {
        add_error(diagnostics, value->source, "battle_ui_integer_out_of_range",
                  "battle UI value is outside 0..65535");
        return false;
    }
    result = static_cast<std::uint16_t>(value->integer);
    return true;
}

bool point(const sexpr::Form& form, BattleUiPoint& result, Diagnostics& diagnostics) {
    return exact_arguments(form, 2, diagnostics) && integer_u8(form, 0, result.x, diagnostics) &&
           integer_u8(form, 1, result.y, diagnostics);
}

bool box(const sexpr::Form& form, BattleUiBox& result, Diagnostics& diagnostics) {
    if (!exact_arguments(form, 4, diagnostics) ||
        !integer_u8(form, 0, result.left, diagnostics) ||
        !integer_u8(form, 1, result.top, diagnostics) ||
        !integer_u8(form, 2, result.right, diagnostics) ||
        !integer_u8(form, 3, result.bottom, diagnostics))
        return false;
    if (result.left >= result.right || result.top >= result.bottom || result.right >= 20 ||
        result.bottom >= 18) {
        add_error(diagnostics, form.source, "invalid_battle_ui_box",
                  "battle UI box is outside the 20x18 native canvas");
        return false;
    }
    return true;
}

bool parse_tiles(const sexpr::Form& form, BattleUiTileStyle& result, Diagnostics& diagnostics) {
    if (!exact_arguments(form, 14, diagnostics)) return false;
    std::array<std::uint8_t*, 14> fields{
        &result.top_left,  &result.horizontal, &result.top_right, &result.vertical,
        &result.bottom_left, &result.bottom_right, &result.blank, &result.cursor,
        &result.hp_label, &result.hp_left, &result.hp_empty, &result.hp_full,
        &result.hp_right, &result.level,
    };
    for (std::size_t index = 0; index < fields.size(); ++index) {
        if (!integer_u8(form, index, *fields[index], diagnostics)) return false;
    }
    return true;
}

bool parse_hud(const sexpr::Form& form, BattleHudLayout& result, Diagnostics& diagnostics) {
    bool have_name = false;
    bool have_level = false;
    bool have_condition = false;
    bool have_hp_bar = false;
    for (const sexpr::Form& child : form.children) {
        if (sexpr::is_head(child, "name")) {
            have_name = point(child, result.name, diagnostics);
        } else if (sexpr::is_head(child, "level")) {
            have_level = point(child, result.level, diagnostics);
        } else if (sexpr::is_head(child, "condition")) {
            have_condition = point(child, result.condition, diagnostics);
        } else if (sexpr::is_head(child, "hp_bar")) {
            have_hp_bar = point(child, result.hp_bar, diagnostics);
        } else if (sexpr::is_head(child, "frame_tile")) {
            BattleUiTilePlacement tile;
            if (exact_arguments(child, 3, diagnostics) &&
                integer_u8(child, 0, tile.position.x, diagnostics) &&
                integer_u8(child, 1, tile.position.y, diagnostics) &&
                integer_u8(child, 2, tile.tile, diagnostics))
                result.frame.push_back(tile);
        } else if (sexpr::is_head(child, "hp_numbers")) {
            result.show_hp_numbers =
                exact_arguments(child, 5, diagnostics) &&
                integer_u8(child, 0, result.current_hp_right, diagnostics) &&
                integer_u8(child, 1, result.hp_number_y, diagnostics) &&
                integer_u8(child, 2, result.hp_separator.x, diagnostics) &&
                integer_u8(child, 3, result.hp_separator.y, diagnostics) &&
                integer_u8(child, 4, result.maximum_hp_right, diagnostics);
        } else {
            add_error(diagnostics, child.source, "unknown_battle_hud_field",
                      "unknown battle HUD field '" + child.head.symbol.text + "'");
        }
    }
    if (!have_name || !have_level || !have_condition || !have_hp_bar || result.frame.empty()) {
        add_error(diagnostics, form.source, "incomplete_battle_hud",
                  "battle HUD is missing a required field");
        return false;
    }
    return diagnostics.ok();
}

bool parse_command(const sexpr::Form& form, BattleCommandSlot& result,
                   Diagnostics& diagnostics) {
    if (form.arguments.size() != 1 ||
        form.arguments.front().kind != sexpr::AtomKind::symbol) {
        add_error(diagnostics, form.source, "invalid_battle_command",
                  "command requires one semantic key");
        return false;
    }
    result.key = form.arguments.front().symbol;
    bool have_text = false;
    bool have_position = false;
    bool have_script = false;
    for (const sexpr::Form& child : form.children) {
        if (sexpr::is_head(child, "text") && exact_arguments(child, 1, diagnostics)) {
            const sexpr::Atom* value =
                argument(child, 0, sexpr::AtomKind::string, diagnostics);
            if (value != nullptr) {
                result.label = value->string;
                have_text = true;
            }
        } else if (sexpr::is_head(child, "position")) {
            BattleUiPoint position;
            if (point(child, position, diagnostics)) {
                result.x = position.x;
                result.y = position.y;
                have_position = true;
            }
        } else if (sexpr::is_head(child, "on_select") &&
                   exact_arguments(child, 1, diagnostics)) {
            const sexpr::Atom* value =
                argument(child, 0, sexpr::AtomKind::symbol, diagnostics);
            if (value != nullptr) {
                result.on_select = value->symbol;
                have_script = true;
            }
        } else if (sexpr::is_head(child, "count_right") &&
                   exact_arguments(child, 1, diagnostics)) {
            (void)integer_u8(child, 0, result.count_right, diagnostics);
        } else {
            add_error(diagnostics, child.source, "unknown_battle_command_field",
                      "unknown battle command field '" + child.head.symbol.text + "'");
        }
    }
    if (!have_text || !have_position || !have_script || result.x == 0) {
        add_error(diagnostics, form.source, "incomplete_battle_command",
                  "battle command is missing text, position, or on_select");
        return false;
    }
    return diagnostics.ok();
}

bool parse_command_menu(const sexpr::Form& form, BattleCommandMenu& result,
                        Diagnostics& diagnostics) {
    if (form.arguments.size() != 1 ||
        form.arguments.front().kind != sexpr::AtomKind::symbol) {
        add_error(diagnostics, form.source, "invalid_battle_command_menu",
                  "command_menu requires one key");
        return false;
    }
    result.key = form.arguments.front().symbol;
    bool have_box = false;
    bool have_visibility = false;
    for (const sexpr::Form& child : form.children) {
        if (sexpr::is_head(child, "box")) {
            have_box = box(child, result.box, diagnostics);
        } else if (sexpr::is_head(child, "show_player") &&
                   exact_arguments(child, 1, diagnostics)) {
            const sexpr::Atom* value =
                argument(child, 0, sexpr::AtomKind::boolean, diagnostics);
            if (value != nullptr) {
                result.show_player = value->boolean;
                have_visibility = true;
            }
        } else if (sexpr::is_head(child, "command")) {
            BattleCommandSlot slot;
            if (parse_command(child, slot, diagnostics)) result.slots.push_back(std::move(slot));
        } else {
            add_error(diagnostics, child.source, "unknown_battle_command_menu_field",
                      "unknown command menu field '" + child.head.symbol.text + "'");
        }
    }
    if (!have_box || !have_visibility || result.slots.empty()) {
        add_error(diagnostics, form.source, "incomplete_battle_command_menu",
                  "command menu is missing its box, visibility, or commands");
        return false;
    }
    return diagnostics.ok();
}

bool parse_move_menu(const sexpr::Form& form, BattleMoveMenuLayout& result,
                     Diagnostics& diagnostics) {
    bool have_information_box = false;
    bool have_list_box = false;
    bool have_first_move = false;
    bool have_cursor = false;
    bool have_type_label = false;
    bool have_type_slash = false;
    bool have_type_value = false;
    bool have_pp = false;
    for (const sexpr::Form& child : form.children) {
        if (sexpr::is_head(child, "information_box")) {
            have_information_box = box(child, result.information_box, diagnostics);
        } else if (sexpr::is_head(child, "list_box")) {
            have_list_box = box(child, result.list_box, diagnostics);
        } else if (sexpr::is_head(child, "join_tile")) {
            BattleUiTilePlacement tile;
            if (exact_arguments(child, 3, diagnostics) &&
                integer_u8(child, 0, tile.position.x, diagnostics) &&
                integer_u8(child, 1, tile.position.y, diagnostics) &&
                integer_u8(child, 2, tile.tile, diagnostics))
                result.joins.push_back(tile);
        } else if (sexpr::is_head(child, "first_move")) {
            have_first_move = point(child, result.first_move, diagnostics);
        } else if (sexpr::is_head(child, "cursor")) {
            have_cursor = point(child, result.cursor, diagnostics);
        } else if (sexpr::is_head(child, "type_label") &&
                   exact_arguments(child, 3, diagnostics)) {
            have_type_label = integer_u8(child, 0, result.type_label.x, diagnostics) &&
                              integer_u8(child, 1, result.type_label.y, diagnostics);
            const sexpr::Atom* value =
                argument(child, 2, sexpr::AtomKind::string, diagnostics);
            if (value != nullptr) result.type_label_text = value->string;
            else have_type_label = false;
        } else if (sexpr::is_head(child, "type_slash")) {
            have_type_slash = point(child, result.type_slash, diagnostics);
        } else if (sexpr::is_head(child, "type_value")) {
            have_type_value = point(child, result.type_value, diagnostics);
        } else if (sexpr::is_head(child, "pp") && exact_arguments(child, 5, diagnostics)) {
            have_pp = integer_u8(child, 0, result.current_pp_right, diagnostics) &&
                      integer_u8(child, 1, result.pp_slash.x, diagnostics) &&
                      integer_u8(child, 2, result.pp_slash.y, diagnostics) &&
                      integer_u8(child, 3, result.maximum_pp_right, diagnostics);
            const sexpr::Atom* disabled =
                argument(child, 4, sexpr::AtomKind::string, diagnostics);
            if (disabled != nullptr) result.disabled_text = disabled->string;
            else have_pp = false;
        } else {
            add_error(diagnostics, child.source, "unknown_battle_move_menu_field",
                      "unknown move menu field '" + child.head.symbol.text + "'");
        }
    }
    if (!have_information_box || !have_list_box || result.joins.empty() || !have_first_move ||
        !have_cursor || !have_type_label || !have_type_slash || !have_type_value || !have_pp) {
        add_error(diagnostics, form.source, "incomplete_battle_move_menu",
                  "move menu is missing a required layout field");
        return false;
    }
    return diagnostics.ok();
}

bool parse_message_layout(const sexpr::Form& form, BattleMessageLayout& result,
                          Diagnostics& diagnostics) {
    bool have_box = false;
    std::size_t line_count = 0;
    for (const sexpr::Form& child : form.children) {
        if (sexpr::is_head(child, "box")) {
            have_box = box(child, result.box, diagnostics);
        } else if (sexpr::is_head(child, "line") && line_count < result.lines.size()) {
            if (point(child, result.lines[line_count], diagnostics)) ++line_count;
        } else {
            add_error(diagnostics, child.source, "unknown_battle_message_field",
                      "unknown battle message field '" + child.head.symbol.text + "'");
        }
    }
    if (!have_box || line_count != result.lines.size()) {
        add_error(diagnostics, form.source, "incomplete_battle_message_layout",
                  "battle message layout requires a box and two lines");
        return false;
    }
    return diagnostics.ok();
}

bool parse_definition(const sexpr::Form& form, BattleUiDefinition& result,
                      Diagnostics& diagnostics) {
    if (form.arguments.size() != 1 ||
        form.arguments.front().kind != sexpr::AtomKind::symbol) {
        add_error(diagnostics, form.source, "invalid_battle_ui",
                  "battle_ui requires one definition key");
        return false;
    }
    bool have_tiles = false;
    bool have_enemy = false;
    bool have_player = false;
    bool have_standard = false;
    bool have_safari = false;
    bool have_move_menu = false;
    bool have_message = false;
    bool have_zero_hp_condition = false;
    for (const sexpr::Form& child : form.children) {
        if (sexpr::is_head(child, "tiles")) {
            have_tiles = parse_tiles(child, result.tiles, diagnostics);
        } else if (sexpr::is_head(child, "zero_hp_condition") &&
                   exact_arguments(child, 1, diagnostics)) {
            const sexpr::Atom* text =
                argument(child, 0, sexpr::AtomKind::string, diagnostics);
            if (text != nullptr) {
                result.zero_hp_condition = text->string;
                have_zero_hp_condition = true;
            }
        } else if (sexpr::is_head(child, "condition_text") &&
                   exact_arguments(child, 2, diagnostics)) {
            const sexpr::Atom* status =
                argument(child, 0, sexpr::AtomKind::symbol, diagnostics);
            const sexpr::Atom* text =
                argument(child, 1, sexpr::AtomKind::string, diagnostics);
            if (status != nullptr && text != nullptr) {
                const auto existing = std::find_if(
                    result.conditions.begin(), result.conditions.end(),
                    [&](const BattleConditionText& value) {
                        return value.status == status->symbol;
                    });
                if (existing != result.conditions.end()) {
                    add_error(diagnostics, status->source, "duplicate_battle_status",
                              "battle status presentation is defined more than once");
                } else {
                    result.conditions.push_back({status->symbol, text->string});
                }
            }
        } else if (sexpr::is_head(child, "enemy_hud")) {
            have_enemy = parse_hud(child, result.enemy_hud, diagnostics);
        } else if (sexpr::is_head(child, "player_hud")) {
            have_player = parse_hud(child, result.player_hud, diagnostics);
        } else if (sexpr::is_head(child, "command_menu")) {
            BattleCommandMenu menu;
            if (parse_command_menu(child, menu, diagnostics)) {
                if (menu.key.text == "standard") {
                    result.standard_commands = std::move(menu);
                    have_standard = true;
                } else if (menu.key.text == "safari") {
                    result.safari_commands = std::move(menu);
                    have_safari = true;
                } else {
                    add_error(diagnostics, child.source, "unknown_battle_command_menu",
                              "battle lab expects standard and safari command menus");
                }
            }
        } else if (sexpr::is_head(child, "move_menu")) {
            have_move_menu = parse_move_menu(child, result.move_menu, diagnostics);
        } else if (sexpr::is_head(child, "message_layout")) {
            have_message = parse_message_layout(child, result.message, diagnostics);
        } else {
            add_error(diagnostics, child.source, "unknown_battle_ui_field",
                      "unknown battle UI field '" + child.head.symbol.text + "'");
        }
    }
    if (!have_tiles || !have_zero_hp_condition || result.conditions.empty() || !have_enemy ||
        !have_player || !have_standard || !have_safari || !have_move_menu || !have_message) {
        add_error(diagnostics, form.source, "incomplete_battle_ui",
                  "battle UI definition is missing a required layout");
        return false;
    }
    return diagnostics.ok();
}

bool parse_hud_state(const sexpr::Form& form, BattleHudState& result,
                     Diagnostics& diagnostics) {
    if (!exact_arguments(form, 6, diagnostics)) return false;
    const sexpr::Atom* name = argument(form, 0, sexpr::AtomKind::string, diagnostics);
    const sexpr::Atom* status = argument(form, 4, sexpr::AtomKind::symbol, diagnostics);
    const sexpr::Atom* visible = argument(form, 5, sexpr::AtomKind::boolean, diagnostics);
    std::uint8_t level = 0;
    std::uint16_t current_hp = 0;
    std::uint16_t maximum_hp = 0;
    if (name == nullptr || status == nullptr || visible == nullptr ||
        !integer_u8(form, 1, level, diagnostics) ||
        !integer_u16(form, 2, current_hp, diagnostics) ||
        !integer_u16(form, 3, maximum_hp, diagnostics))
        return false;
    if (level == 0 || level > 100 || maximum_hp == 0 || current_hp > maximum_hp) {
        add_error(diagnostics, form.source, "invalid_battle_hud_state",
                  "battle HUD fixture contains invalid state");
        return false;
    }
    result = {
        .name = name->string,
        .level = level,
        .current_hp = current_hp,
        .maximum_hp = maximum_hp,
        .status = status->symbol,
        .visible = visible->boolean,
    };
    return true;
}

BattleCommandMenu* menu_by_key(BattleUiDefinition& definition, const Symbol& key) {
    if (definition.standard_commands.key == key) return &definition.standard_commands;
    if (definition.safari_commands.key == key) return &definition.safari_commands;
    return nullptr;
}

bool parse_lab_state(const sexpr::Form& form, BattleUiState& result,
                     Diagnostics& diagnostics) {
    std::size_t move_count = 0;
    bool have_player = false;
    bool have_enemy = false;
    bool have_message = false;
    for (const sexpr::Form& child : form.children) {
        if (sexpr::is_head(child, "player")) {
            have_player = parse_hud_state(child, result.player, diagnostics);
        } else if (sexpr::is_head(child, "enemy")) {
            have_enemy = parse_hud_state(child, result.enemy, diagnostics);
        } else if (sexpr::is_head(child, "move") && move_count < result.moves.size()) {
            if (!exact_arguments(child, 5, diagnostics)) continue;
            const sexpr::Atom* name =
                argument(child, 0, sexpr::AtomKind::string, diagnostics);
            const sexpr::Atom* type =
                argument(child, 1, sexpr::AtomKind::string, diagnostics);
            const sexpr::Atom* enabled =
                argument(child, 4, sexpr::AtomKind::boolean, diagnostics);
            std::uint8_t current_pp = 0;
            std::uint8_t maximum_pp = 0;
            if (name != nullptr && type != nullptr && enabled != nullptr &&
                integer_u8(child, 2, current_pp, diagnostics) &&
                integer_u8(child, 3, maximum_pp, diagnostics)) {
                result.moves[move_count++] = {
                    .name = name->string,
                    .type = type->string,
                    .current_pp = current_pp,
                    .maximum_pp = maximum_pp,
                    .enabled = enabled->boolean,
                };
            }
        } else if (sexpr::is_head(child, "message") &&
                   exact_arguments(child, 2, diagnostics)) {
            const sexpr::Atom* first =
                argument(child, 0, sexpr::AtomKind::string, diagnostics);
            const sexpr::Atom* second =
                argument(child, 1, sexpr::AtomKind::string, diagnostics);
            if (first != nullptr && second != nullptr) {
                result.message_lines = {first->string, second->string};
                have_message = true;
            }
        } else if (sexpr::is_head(child, "command_count") &&
                   exact_arguments(child, 3, diagnostics)) {
            const sexpr::Atom* menu =
                argument(child, 0, sexpr::AtomKind::symbol, diagnostics);
            const sexpr::Atom* command =
                argument(child, 1, sexpr::AtomKind::symbol, diagnostics);
            std::uint16_t count = 0;
            if (menu == nullptr || command == nullptr ||
                !integer_u16(child, 2, count, diagnostics))
                continue;
            BattleCommandMenu* target = menu_by_key(result.definition, menu->symbol);
            if (target == nullptr) {
                add_error(diagnostics, menu->source, "unknown_battle_command_menu",
                          "command_count references an unknown menu");
                continue;
            }
            const auto slot = std::find_if(
                target->slots.begin(), target->slots.end(),
                [&](const BattleCommandSlot& value) { return value.key == command->symbol; });
            if (slot == target->slots.end()) {
                add_error(diagnostics, command->source, "unknown_battle_command",
                          "command_count references an unknown command");
                continue;
            }
            slot->count = count;
        } else {
            add_error(diagnostics, child.source, "unknown_battle_lab_state_field",
                      "unknown battle lab state field '" + child.head.symbol.text + "'");
        }
    }
    if (!have_player || !have_enemy || move_count != result.moves.size() || !have_message) {
        add_error(diagnostics, form.source, "incomplete_battle_lab_state",
                  "battle lab state requires two HUDs, four moves, and two message lines");
        return false;
    }
    return diagnostics.ok();
}

bool encode_text(std::string_view text, const BattleUiTileStyle& style,
                 std::vector<std::uint8_t>& result, std::string& error) {
    result.clear();
    for (std::size_t index = 0; index < text.size();) {
        const std::string_view remaining = text.substr(index);
        if (remaining.starts_with("<PK>")) {
            result.push_back(0xE1);
            index += 4;
            continue;
        }
        if (remaining.starts_with("<MN>")) {
            result.push_back(0xE2);
            index += 4;
            continue;
        }
        if (remaining.starts_with("×")) {
            result.push_back(0xF1);
            index += std::string_view("×").size();
            continue;
        }
        if (remaining.starts_with("♂")) {
            result.push_back(0xEF);
            index += std::string_view("♂").size();
            continue;
        }
        if (remaining.starts_with("♀")) {
            result.push_back(0xF5);
            index += std::string_view("♀").size();
            continue;
        }

        const auto character = static_cast<unsigned char>(text[index++]);
        if (character >= 'A' && character <= 'Z') {
            result.push_back(static_cast<std::uint8_t>(0x80U + character - 'A'));
            continue;
        }
        if (character >= 'a' && character <= 'z') {
            result.push_back(static_cast<std::uint8_t>(0xA0U + character - 'a'));
            continue;
        }
        if (character >= '0' && character <= '9') {
            result.push_back(static_cast<std::uint8_t>(0xF6U + character - '0'));
            continue;
        }
        switch (character) {
        case ' ':
            result.push_back(style.blank);
            break;
        case '\'':
            result.push_back(0xE0);
            break;
        case '-':
            result.push_back(0xE3);
            break;
        case '?':
            result.push_back(0xE6);
            break;
        case '!':
            result.push_back(0xE7);
            break;
        case '.':
            result.push_back(0xE8);
            break;
        case '/':
            result.push_back(0xF3);
            break;
        case ',':
            result.push_back(0xF4);
            break;
        default:
            error = "battle UI text contains an unsupported character";
            return false;
        }
    }
    return true;
}

bool place_text(BattleTileMap& tiles, const BattleUiTileStyle& style, std::string_view text,
                std::size_t x, std::size_t y, std::string& error) {
    std::vector<std::uint8_t> glyphs;
    if (!encode_text(text, style, glyphs, error)) return false;
    if (y >= 18 || x + glyphs.size() > 20) {
        error = "battle UI text extends outside the native tile map";
        return false;
    }
    for (std::size_t index = 0; index < glyphs.size(); ++index)
        tiles[tile_index(x + index, y)] = glyphs[index];
    return true;
}

bool place_number_right(BattleTileMap& tiles, const BattleUiTileStyle& style,
                        std::uint16_t value, std::size_t right, std::size_t width,
                        std::size_t y, std::string& error) {
    const std::string digits = std::to_string(value);
    if (digits.size() > width || right + 1U < digits.size()) {
        error = "battle UI number exceeds its field";
        return false;
    }
    return place_text(tiles, style, digits, right + 1U - digits.size(), y, error);
}

void draw_box(BattleTileMap& tiles, const BattleUiTileStyle& style, const BattleUiBox& box) {
    tiles[tile_index(box.left, box.top)] = style.top_left;
    tiles[tile_index(box.right, box.top)] = style.top_right;
    tiles[tile_index(box.left, box.bottom)] = style.bottom_left;
    tiles[tile_index(box.right, box.bottom)] = style.bottom_right;
    for (std::size_t x = box.left + 1U; x < box.right; ++x) {
        tiles[tile_index(x, box.top)] = style.horizontal;
        tiles[tile_index(x, box.bottom)] = style.horizontal;
    }
    for (std::size_t y = box.top + 1U; y < box.bottom; ++y) {
        tiles[tile_index(box.left, y)] = style.vertical;
        tiles[tile_index(box.right, y)] = style.vertical;
        for (std::size_t x = box.left + 1U; x < box.right; ++x)
            tiles[tile_index(x, y)] = style.blank;
    }
}

bool place_level(BattleTileMap& tiles, const BattleUiTileStyle& style, std::uint8_t level,
                 const BattleUiPoint& position, std::string& error) {
    if (level == 0 || level > 100) {
        error = "battle UI level is outside 1..100";
        return false;
    }
    if (level < 100) {
        tiles[tile_index(position.x, position.y)] = style.level;
        return place_number_right(tiles, style, level, position.x + 2U, 2, position.y, error);
    }
    return place_number_right(tiles, style, level, position.x + 2U, 3, position.y, error);
}

std::string_view condition(const BattleUiDefinition& definition, const BattleHudState& hud) {
    if (hud.current_hp == 0) return definition.zero_hp_condition;
    const auto found =
        std::find_if(definition.conditions.begin(), definition.conditions.end(),
                     [&](const BattleConditionText& value) { return value.status == hud.status; });
    return found == definition.conditions.end() ? std::string_view{} : found->text;
}

bool place_centered_name(BattleTileMap& tiles, const BattleUiTileStyle& style,
                         std::string_view name, const BattleUiPoint& position,
                         std::string& error) {
    std::vector<std::uint8_t> glyphs;
    if (!encode_text(name, style, glyphs, error)) return false;
    const std::size_t offset = glyphs.size() <= 2 ? 2U : glyphs.size() <= 4 ? 1U : 0U;
    return place_text(tiles, style, name, position.x + offset, position.y, error);
}

std::uint8_t hp_pixels(const BattleHudState& hud) {
    if (hud.current_hp == 0 || hud.maximum_hp == 0) return 0;
    const std::uint32_t scaled = static_cast<std::uint32_t>(hud.current_hp) * 48U / hud.maximum_hp;
    return static_cast<std::uint8_t>(std::max<std::uint32_t>(scaled, 1U));
}

void draw_hp_bar(BattleTileMap& tiles, const BattleUiTileStyle& style,
                 const BattleHudState& hud, const BattleUiPoint& position) {
    tiles[tile_index(position.x, position.y)] = style.hp_label;
    tiles[tile_index(position.x + 1U, position.y)] = style.hp_left;
    const std::uint8_t pixels = hp_pixels(hud);
    for (std::size_t index = 0; index < 6; ++index) {
        const int remaining = static_cast<int>(pixels) - static_cast<int>(index * 8U);
        tiles[tile_index(position.x + 2U + index, position.y)] =
            remaining >= 8
                ? style.hp_full
                : remaining > 0 ? static_cast<std::uint8_t>(style.hp_empty + remaining)
                                : style.hp_empty;
    }
    tiles[tile_index(position.x + 8U, position.y)] = style.hp_right;
}

void draw_frame(BattleTileMap& tiles, const std::vector<BattleUiTilePlacement>& frame) {
    for (const BattleUiTilePlacement& tile : frame)
        tiles[tile_index(tile.position.x, tile.position.y)] = tile.tile;
}

bool draw_one_hud(const BattleUiDefinition& definition, const BattleHudLayout& layout,
                  const BattleHudState& hud, BattleTileMap& tiles, std::string& error) {
    if (!hud.visible) return true;
    draw_frame(tiles, layout.frame);
    const std::string_view status = condition(definition, hud);
    if (!place_centered_name(tiles, definition.tiles, hud.name, layout.name, error) ||
        (!status.empty()
             ? !place_text(tiles, definition.tiles, status, layout.condition.x,
                           layout.condition.y, error)
             : !place_level(tiles, definition.tiles, hud.level, layout.level, error)))
        return false;
    draw_hp_bar(tiles, definition.tiles, hud, layout.hp_bar);
    if (!layout.show_hp_numbers) return true;
    return place_number_right(tiles, definition.tiles, hud.current_hp,
                              layout.current_hp_right, 3, layout.hp_number_y, error) &&
           place_text(tiles, definition.tiles, "/", layout.hp_separator.x,
                      layout.hp_separator.y, error) &&
           place_number_right(tiles, definition.tiles, hud.maximum_hp,
                              layout.maximum_hp_right, 3, layout.hp_number_y, error);
}

bool draw_hud(const BattleUiState& state, BattleTileMap& tiles, bool player_visible,
              std::string& error) {
    if (!draw_one_hud(state.definition, state.definition.enemy_hud, state.enemy, tiles, error))
        return false;
    return !player_visible ||
           draw_one_hud(state.definition, state.definition.player_hud, state.player, tiles, error);
}

bool draw_command_menu(const BattleUiTileStyle& style, const BattleCommandMenu& menu,
                       BattleTileMap& tiles, std::string& error) {
    if (menu.slots.empty() || menu.selected >= menu.slots.size()) {
        error = "battle command menu has an invalid selection";
        return false;
    }
    draw_box(tiles, style, menu.box);
    for (const BattleCommandSlot& slot : menu.slots) {
        if (!place_text(tiles, style, slot.label, slot.x, slot.y, error)) return false;
        if (slot.count &&
            !place_number_right(tiles, style, *slot.count, slot.count_right, 2, slot.y, error))
            return false;
    }
    const BattleCommandSlot& selected = menu.slots[menu.selected];
    tiles[tile_index(selected.x - 1U, selected.y)] = style.cursor;
    return true;
}

bool draw_move_menu(const BattleUiState& state, BattleTileMap& tiles, std::string& error) {
    if (state.selected_move >= state.moves.size()) {
        error = "battle move menu has an invalid selection";
        return false;
    }
    const BattleMoveMenuLayout& layout = state.definition.move_menu;
    const BattleUiTileStyle& style = state.definition.tiles;
    draw_box(tiles, style, layout.information_box);
    draw_box(tiles, style, layout.list_box);
    draw_frame(tiles, layout.joins);
    for (std::size_t index = 0; index < state.moves.size(); ++index) {
        if (!place_text(tiles, style, state.moves[index].name, layout.first_move.x,
                        layout.first_move.y + index, error))
            return false;
    }
    tiles[tile_index(layout.cursor.x, layout.cursor.y + state.selected_move)] = style.cursor;

    const BattleMoveOption& move = state.moves[state.selected_move];
    if (!move.enabled)
        return place_text(tiles, style, layout.disabled_text, layout.type_value.x,
                          layout.type_value.y, error);
    return place_text(tiles, style, layout.type_label_text, layout.type_label.x,
                      layout.type_label.y, error) &&
           place_text(tiles, style, "/", layout.type_slash.x, layout.type_slash.y, error) &&
           place_text(tiles, style, move.type, layout.type_value.x, layout.type_value.y, error) &&
           place_number_right(tiles, style, move.current_pp, layout.current_pp_right, 2,
                              layout.pp_slash.y, error) &&
           place_text(tiles, style, "/", layout.pp_slash.x, layout.pp_slash.y, error) &&
           place_number_right(tiles, style, move.maximum_pp, layout.maximum_pp_right, 2,
                              layout.pp_slash.y, error);
}

} // namespace

bool load_battle_ui_source(const std::filesystem::path& root, BattleUiState& result,
                           Diagnostics& diagnostics) {
    std::vector<SourceDocument> sources;
    if (!load_source_directory(root, sources, diagnostics)) return false;

    BattleUiState loaded;
    const sexpr::Form* definition = nullptr;
    const sexpr::Form* lab_state = nullptr;
    for (const SourceDocument& source : sources) {
        for (const sexpr::Form& form : source.document.forms) {
            if (sexpr::is_head(form, "battle_ui")) {
                if (definition != nullptr) {
                    add_error(diagnostics, form.source, "duplicate_battle_ui",
                              "battle UI source contains more than one definition");
                } else {
                    definition = &form;
                }
            } else if (sexpr::is_head(form, "battle_ui_lab_state")) {
                if (lab_state != nullptr) {
                    add_error(diagnostics, form.source, "duplicate_battle_ui_lab_state",
                              "battle UI source contains more than one lab state");
                } else {
                    lab_state = &form;
                }
            } else {
                add_error(diagnostics, form.source, "unexpected_battle_ui_form",
                          "battle UI source contains an unsupported top-level form");
            }
        }
    }
    if (definition == nullptr || lab_state == nullptr) {
        add_error(diagnostics, {root.string(), 1, 1}, "incomplete_battle_ui_source",
                  "battle UI source requires one definition and one lab state");
        return false;
    }
    if (!parse_definition(*definition, loaded.definition, diagnostics) ||
        !parse_lab_state(*lab_state, loaded, diagnostics))
        return false;
    result = std::move(loaded);
    return true;
}

void set_battle_ui_species(BattleUiState& state, std::string_view species_name) {
    state.player.name = species_name;
    state.enemy.name = species_name;
    state.message_lines[0] = std::string(species_name) + " gained";
}

void next_battle_ui_mode(BattleUiState& state) {
    const auto value = static_cast<unsigned>(state.mode);
    state.mode = static_cast<BattleUiMode>((value + 1U) % 4U);
}

void next_battle_ui_selection(BattleUiState& state) {
    if (state.mode == BattleUiMode::command && !state.definition.standard_commands.slots.empty())
        state.definition.standard_commands.selected =
            (state.definition.standard_commands.selected + 1U) %
            state.definition.standard_commands.slots.size();
    else if (state.mode == BattleUiMode::safari &&
             !state.definition.safari_commands.slots.empty())
        state.definition.safari_commands.selected =
            (state.definition.safari_commands.selected + 1U) %
            state.definition.safari_commands.slots.size();
    else if (state.mode == BattleUiMode::moves)
        state.selected_move = (state.selected_move + 1U) % state.moves.size();
}

void previous_battle_ui_selection(BattleUiState& state) {
    const auto previous = [](std::size_t value, std::size_t count) {
        return value == 0 ? count - 1U : value - 1U;
    };
    if (state.mode == BattleUiMode::command && !state.definition.standard_commands.slots.empty())
        state.definition.standard_commands.selected =
            previous(state.definition.standard_commands.selected,
                     state.definition.standard_commands.slots.size());
    else if (state.mode == BattleUiMode::safari &&
             !state.definition.safari_commands.slots.empty())
        state.definition.safari_commands.selected =
            previous(state.definition.safari_commands.selected,
                     state.definition.safari_commands.slots.size());
    else if (state.mode == BattleUiMode::moves)
        state.selected_move = previous(state.selected_move, state.moves.size());
}

void move_battle_ui_selection(BattleUiState& state, int x, int y) {
    if (state.mode == BattleUiMode::moves) {
        if (y > 0)
            state.selected_move =
                (state.selected_move + 1U) % state.moves.size();
        else if (y < 0)
            state.selected_move =
                state.selected_move == 0U
                    ? state.moves.size() - 1U
                    : state.selected_move - 1U;
        return;
    }
    BattleCommandMenu* menu =
        state.mode == BattleUiMode::safari
            ? &state.definition.safari_commands
            : state.mode == BattleUiMode::command
                  ? &state.definition.standard_commands
                  : nullptr;
    if (menu == nullptr || menu->slots.empty() ||
        menu->selected >= menu->slots.size() ||
        (x == 0 && y == 0))
        return;

    const BattleCommandSlot& current = menu->slots[menu->selected];
    std::size_t best = menu->selected;
    int best_primary = std::numeric_limits<int>::max();
    int best_secondary = std::numeric_limits<int>::max();
    for (std::size_t index = 0U; index < menu->slots.size(); ++index) {
        if (index == menu->selected) continue;
        const BattleCommandSlot& candidate = menu->slots[index];
        const int delta_x =
            static_cast<int>(candidate.x) -
            static_cast<int>(current.x);
        const int delta_y =
            static_cast<int>(candidate.y) -
            static_cast<int>(current.y);
        const bool forward =
            (x < 0 && delta_x < 0) ||
            (x > 0 && delta_x > 0) ||
            (y < 0 && delta_y < 0) ||
            (y > 0 && delta_y > 0);
        if (!forward) continue;
        const int primary =
            x == 0 ? std::abs(delta_y) : std::abs(delta_x);
        const int secondary =
            x == 0 ? std::abs(delta_x) : std::abs(delta_y);
        if (primary < best_primary ||
            (primary == best_primary &&
             secondary < best_secondary)) {
            best = index;
            best_primary = primary;
            best_secondary = secondary;
        }
    }
    if (best != menu->selected) menu->selected = best;
}

void next_battle_ui_status(BattleUiState& state) {
    if (state.definition.conditions.empty()) return;
    const auto found = std::find_if(
        state.definition.conditions.begin(), state.definition.conditions.end(),
        [&](const BattleConditionText& value) { return value.status == state.player.status; });
    const std::size_t index =
        found == state.definition.conditions.end()
            ? 0U
            : (static_cast<std::size_t>(
                   std::distance(state.definition.conditions.begin(), found)) +
               1U) %
                  state.definition.conditions.size();
    state.player.status = state.definition.conditions[index].status;
}

bool compose_battle_ui(const BattleUiState& state, BattleTileMap& result, std::string& error) {
    result.fill(0);
    const BattleCommandMenu* active_menu =
        state.mode == BattleUiMode::safari ? &state.definition.safari_commands
                                          : &state.definition.standard_commands;
    if (!draw_hud(state, result, active_menu->show_player, error)) return false;
    switch (state.mode) {
    case BattleUiMode::command:
        return draw_command_menu(state.definition.tiles, state.definition.standard_commands,
                                 result, error);
    case BattleUiMode::moves:
        return draw_move_menu(state, result, error);
    case BattleUiMode::safari:
        return draw_command_menu(state.definition.tiles, state.definition.safari_commands,
                                 result, error);
    case BattleUiMode::message:
        draw_box(result, state.definition.tiles, state.definition.message.box);
        return place_text(result, state.definition.tiles, state.message_lines[0],
                          state.definition.message.lines[0].x,
                          state.definition.message.lines[0].y, error) &&
               place_text(result, state.definition.tiles, state.message_lines[1],
                          state.definition.message.lines[1].x,
                          state.definition.message.lines[1].y, error);
    }
    error = "battle UI mode is invalid";
    return false;
}

std::string_view label(BattleUiMode mode) {
    switch (mode) {
    case BattleUiMode::command:
        return "command";
    case BattleUiMode::moves:
        return "moves";
    case BattleUiMode::safari:
        return "safari";
    case BattleUiMode::message:
        return "message";
    }
    return "unknown";
}

} // namespace pokered
