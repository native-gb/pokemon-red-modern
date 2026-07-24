#include "import_text.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace pokered::import {
namespace {

constexpr std::size_t kBankSize = 0x4000;
constexpr std::size_t kMaximumProgramBytes = 4096;
constexpr unsigned kMaximumFarDepth = 4;

struct DecodeState {
    std::span<const std::uint8_t> rom;
    std::ostringstream operations;
    std::vector<std::string> pages{1};
    std::string next_operation{"write"};
    std::size_t bytes{};
    bool dynamic{};
    std::string reason;
};

bool has_range(std::span<const std::uint8_t> rom, std::size_t offset, std::size_t size) {
    return offset <= rom.size() && size <= rom.size() - offset;
}

std::uint16_t read_u16(std::span<const std::uint8_t> rom, std::size_t offset) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(rom[offset]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(rom[offset + 1U]) << 8U));
}

bool visible_pointer_to_offset(std::span<const std::uint8_t> rom, std::uint8_t bank,
                               std::uint16_t pointer, std::size_t& result) {
    if (pointer < 0x4000) {
        result = pointer;
    } else {
        if (pointer >= 0x8000 || bank == 0) return false;
        result = static_cast<std::size_t>(bank) * kBankSize +
                 static_cast<std::size_t>(pointer - 0x4000U);
    }
    return result < rom.size();
}

void append_escaped(std::ostringstream& output, std::string_view text) {
    for (const char character : text) {
        if (character == '\\' || character == '"') output << '\\';
        output << character;
    }
}

std::string glyph(std::uint8_t value) {
    if (value >= 0x80U && value <= 0x99U)
        return std::string(1, static_cast<char>('A' + value - 0x80U));
    if (value >= 0xA0U && value <= 0xB9U)
        return std::string(1, static_cast<char>('a' + value - 0xA0U));
    if (value >= 0xF6U) return std::string(1, static_cast<char>('0' + value - 0xF6U));

    switch (value) {
    case 0x54:
        return "POKé";
    case 0x56:
        return "……";
    case 0x59:
        return "{target_name}";
    case 0x5A:
        return "{user_name}";
    case 0x5B:
        return "PC";
    case 0x5C:
        return "TM";
    case 0x5D:
        return "TRAINER";
    case 0x5E:
        return "ROCKET";
    case 0x7F:
        return " ";
    case 0x9A:
        return "(";
    case 0x9B:
        return ")";
    case 0x9C:
        return ":";
    case 0x9D:
        return ";";
    case 0x9E:
        return "[";
    case 0x9F:
        return "]";
    case 0xBA:
        return "é";
    case 0xBB:
        return "'d";
    case 0xBC:
        return "'l";
    case 0xBD:
        return "'s";
    case 0xBE:
        return "'t";
    case 0xBF:
        return "'v";
    case 0xE0:
        return "'";
    case 0xE1:
        return "PK";
    case 0xE2:
        return "MN";
    case 0xE3:
        return "-";
    case 0xE4:
        return "'r";
    case 0xE5:
        return "'m";
    case 0xE6:
        return "?";
    case 0xE7:
        return "!";
    case 0xE8:
        return ".";
    case 0xEC:
        return "▷";
    case 0xED:
        return "▶";
    case 0xEE:
        return "▼";
    case 0xEF:
        return "♂";
    case 0xF0:
        return "¥";
    case 0xF1:
        return "×";
    case 0xF2:
        return ".";
    case 0xF3:
        return "/";
    case 0xF4:
        return ",";
    case 0xF5:
        return "♀";
    default:
        break;
    }

    std::ostringstream token;
    token << "{glyph_" << std::hex << std::setfill('0') << std::setw(2)
          << static_cast<unsigned>(value) << '}';
    return token.str();
}

bool emit_write(DecodeState& state, std::string_view operation, const std::string& text) {
    if (text.empty()) return false;
    state.operations << "    " << operation << " \"";
    append_escaped(state.operations, text);
    state.operations << "\"\n";
    if ((operation == "line" || operation == "continue") && !state.pages.back().empty())
        state.pages.back().push_back('\n');
    state.pages.back() += text;
    return true;
}

enum class StringEnd {
    terminator,
    done,
    prompt,
    dex,
    malformed,
};

StringEnd decode_string(DecodeState& state, std::size_t& cursor) {
    std::string text;
    const auto flush_text = [&] {
        if (emit_write(
                state, state.next_operation, text))
            state.next_operation = "write";
        text.clear();
    };
    for (std::size_t count = 0; count < kMaximumProgramBytes; ++count) {
        if (!has_range(state.rom, cursor, 1)) return StringEnd::malformed;
        const std::uint8_t value = state.rom[cursor++];
        ++state.bytes;
        switch (value) {
        case 0x49:
            flush_text();
            state.operations << "    page\n";
            if (!state.pages.back().empty()) state.pages.emplace_back();
            state.next_operation = "write";
            break;
        case 0x4A:
            text += "PKMN";
            break;
        case 0x4B:
            flush_text();
            state.operations << "    continue_no_pause\n";
            state.next_operation = "write";
            break;
        case 0x4C:
            flush_text();
            state.operations << "    scroll\n";
            state.next_operation = "write";
            break;
        case 0x4E:
            flush_text();
            state.next_operation = "next";
            break;
        case 0x4F:
            flush_text();
            state.next_operation = "line";
            break;
        case 0x50:
            flush_text();
            return StringEnd::terminator;
        case 0x51:
            flush_text();
            state.operations << "    paragraph\n";
            if (!state.pages.back().empty()) state.pages.emplace_back();
            state.next_operation = "write";
            break;
        case 0x52:
            text += "{player_name}";
            break;
        case 0x53:
            text += "{rival_name}";
            break;
        case 0x55:
            flush_text();
            state.next_operation = "continue";
            break;
        case 0x57:
            flush_text();
            state.operations << "    finish done\n";
            return StringEnd::done;
        case 0x58:
            flush_text();
            state.operations << "    finish prompt\n";
            return StringEnd::prompt;
        case 0x5F:
            flush_text();
            state.operations << "    finish dex\n";
            return StringEnd::dex;
        default:
            text += glyph(value);
            break;
        }
    }
    return StringEnd::malformed;
}

std::string_view sound_name(std::uint8_t command) {
    constexpr std::array<std::string_view, 12> names{
        "get_item_1",       "dots",         "wait_button",  "pokedex_rating",
        "get_item_1_again", "get_item_2",   "get_key_item", "caught_pokemon",
        "dex_page_added",   "cry_nidorina", "cry_pidgeot",  "cry_dewgong",
    };
    return names[static_cast<std::size_t>(command - 0x0BU)];
}

bool decode_commands(DecodeState& state, std::uint8_t bank, std::size_t offset, unsigned depth) {
    if (depth > kMaximumFarDepth) {
        state.reason = "text_far_depth_exceeded";
        return false;
    }
    std::size_t cursor = offset;
    for (std::size_t commands = 0; commands < kMaximumProgramBytes; ++commands) {
        if (!has_range(state.rom, cursor, 1)) {
            state.reason = "text_command_out_of_range";
            return false;
        }
        const std::uint8_t command = state.rom[cursor++];
        ++state.bytes;
        if (command == 0x50U) return true;

        if (command == 0x00U) {
            const StringEnd end = decode_string(state, cursor);
            if (end == StringEnd::malformed) {
                state.reason = "unterminated_text_string";
                return false;
            }
            if (end != StringEnd::terminator) return true;
            continue;
        }
        if (command == 0x17U) {
            if (!has_range(state.rom, cursor, 3)) {
                state.reason = "text_far_operand_out_of_range";
                return false;
            }
            const std::uint16_t pointer = read_u16(state.rom, cursor);
            const std::uint8_t far_bank = state.rom[cursor + 2U];
            cursor += 3U;
            state.bytes += 3U;
            std::size_t far_offset = 0;
            if (!visible_pointer_to_offset(state.rom, far_bank, pointer, far_offset)) {
                state.reason = "text_far_target_out_of_range";
                return false;
            }
            if (!decode_commands(state, far_bank, far_offset, depth + 1U)) return false;
            continue;
        }
        if (command == 0x08U) {
            state.operations << "    dynamic_native_script " << "bank_" << std::hex
                             << std::setfill('0') << std::setw(2) << static_cast<unsigned>(bank)
                             << " 0x" << std::setw(6) << cursor << std::dec << '\n';
            state.dynamic = true;
            return true;
        }
        if (command == 0x01U || command == 0x03U) {
            if (!has_range(state.rom, cursor, 2)) {
                state.reason = "text_pointer_operand_out_of_range";
                return false;
            }
            const std::uint16_t address = read_u16(state.rom, cursor);
            state.operations << "    " << (command == 0x01U ? "insert_ram" : "move_cursor") << " 0x"
                             << std::hex << std::setfill('0') << std::setw(4)
                             << address << std::dec << '\n';
            if (command == 0x01U) {
                if ((state.next_operation == "line" ||
                     state.next_operation == "continue") &&
                    !state.pages.back().empty())
                    state.pages.back().push_back('\n');
                state.next_operation = "write";
                if (address == 0xCD6DU)
                    state.pages.back() += "{name_buffer}";
                else {
                    std::ostringstream token;
                    token << "{ram_" << std::hex << std::setfill('0')
                          << std::setw(4) << address << '}';
                    state.pages.back() += token.str();
                }
            }
            cursor += 2U;
            state.bytes += 2U;
            continue;
        }
        if (command == 0x02U || command == 0x09U) {
            if (!has_range(state.rom, cursor, 3)) {
                state.reason = "text_number_operand_out_of_range";
                return false;
            }
            state.operations << "    " << (command == 0x02U ? "insert_bcd" : "insert_decimal")
                             << " address 0x" << std::hex << std::setfill('0') << std::setw(4)
                             << read_u16(state.rom, cursor) << " format 0x" << std::setw(2)
                             << static_cast<unsigned>(state.rom[cursor + 2U]) << std::dec << '\n';
            cursor += 3U;
            state.bytes += 3U;
            continue;
        }
        if (command == 0x04U) {
            if (!has_range(state.rom, cursor, 4)) {
                state.reason = "text_box_operand_out_of_range";
                return false;
            }
            state.operations << "    draw_box address 0x" << std::hex << std::setfill('0')
                             << std::setw(4) << read_u16(state.rom, cursor) << std::dec << " size "
                             << static_cast<unsigned>(state.rom[cursor + 2U]) << ' '
                             << static_cast<unsigned>(state.rom[cursor + 3U]) << '\n';
            cursor += 4U;
            state.bytes += 4U;
            continue;
        }
        if (command == 0x0CU) {
            if (!has_range(state.rom, cursor, 1)) {
                state.reason = "text_dots_operand_out_of_range";
                return false;
            }
            state.operations << "    dots " << static_cast<unsigned>(state.rom[cursor++]) << '\n';
            ++state.bytes;
            continue;
        }
        if (command == 0x0DU) {
            state.operations << "    wait_button\n";
            continue;
        }
        if (command == 0x0BU || (command >= 0x0EU && command <= 0x16U)) {
            state.operations << "    play_text_sound " << sound_name(command) << '\n';
            continue;
        }

        constexpr std::array<std::string_view, 6> simple_commands{
            "low_line", "prompt_button", "scroll", "unused", "unused", "pause",
        };
        if (command >= 0x05U && command <= 0x0AU && command != 0x08U && command != 0x09U) {
            state.operations << "    " << simple_commands[static_cast<std::size_t>(command - 0x05U)]
                             << '\n';
            continue;
        }

        state.reason = "unknown_text_command";
        return false;
    }
    state.reason = "text_command_budget_exceeded";
    return false;
}

InteractionBuiltin special_script_kind(std::uint8_t command) {
    switch (command) {
    case 0xFF:
        return InteractionBuiltin::pokecenter_nurse;
    case 0xFD:
        return InteractionBuiltin::bills_pc;
    case 0xFC:
        return InteractionBuiltin::players_pc;
    case 0xF9:
        return InteractionBuiltin::pokecenter_pc;
    case 0xF7:
        return InteractionBuiltin::prize_vendor;
    case 0xF6:
        return InteractionBuiltin::cable_club_receptionist;
    case 0xF5:
        return InteractionBuiltin::vending_machine;
    default:
        return InteractionBuiltin::none;
    }
}

std::string_view special_script_name(InteractionBuiltin kind) {
    switch (kind) {
    case InteractionBuiltin::pokecenter_nurse:
        return "pokecenter_nurse";
    case InteractionBuiltin::bills_pc:
        return "bills_pc";
    case InteractionBuiltin::players_pc:
        return "players_pc";
    case InteractionBuiltin::pokecenter_pc:
        return "pokecenter_pc";
    case InteractionBuiltin::prize_vendor:
        return "prize_vendor";
    case InteractionBuiltin::cable_club_receptionist:
        return "cable_club_receptionist";
    case InteractionBuiltin::vending_machine:
        return "vending_machine";
    case InteractionBuiltin::shop:
        return "shop";
    case InteractionBuiltin::none:
        return {};
    }
    return {};
}

} // namespace

std::string decode_text_glyph(std::uint8_t value) {
    return glyph(value);
}

bool decode_text_program(std::span<const std::uint8_t> rom, std::uint8_t bank, std::size_t offset,
                         DecodedTextProgram& result) {
    result = {};
    if (!has_range(rom, offset, 1)) {
        result.unresolved_reason = "text_entry_out_of_range";
        return false;
    }

    DecodeState state{
        .rom = rom,
        .operations = {},
        .pages = std::vector<std::string>(1),
        .next_operation = "write",
        .bytes = 0,
        .dynamic = false,
        .reason = {},
    };
    const std::uint8_t first = rom[offset];
    result.builtin = special_script_kind(first);
    if (result.builtin != InteractionBuiltin::none) {
        state.operations << "    invoke_builtin "
                         << special_script_name(result.builtin) << '\n';
        state.bytes = 1;
        result.interaction = true;
    } else if (first == 0xFEU) {
        if (!has_range(rom, offset, 2)) {
            result.unresolved_reason = "mart_header_out_of_range";
            return false;
        }
        const std::uint8_t count = rom[offset + 1U];
        if (!has_range(rom, offset + 2U, static_cast<std::size_t>(count) + 1U)) {
            result.unresolved_reason = "mart_items_out_of_range";
            return false;
        }
        state.operations << "    open_shop\n";
        result.interaction = true;
        result.builtin = InteractionBuiltin::shop;
        result.item_ids.reserve(count);
        for (std::uint8_t index = 0; index < count; ++index) {
            const std::uint8_t item_id = rom[offset + 2U + index];
            result.item_ids.push_back(item_id);
            state.operations << "    item_id "
                             << static_cast<unsigned>(item_id) << '\n';
        }
        if (rom[offset + 2U + count] != 0xFFU) {
            result.unresolved_reason = "mart_items_missing_terminator";
            return false;
        }
        state.bytes = static_cast<std::size_t>(count) + 3U;
    } else if (!decode_commands(state, bank, offset, 0)) {
        result.operations = state.operations.str();
        result.source_bytes = state.bytes;
        result.dynamic = state.dynamic;
        result.unresolved_reason = state.reason;
        return false;
    }

    result.operations = state.operations.str();
    result.pages = std::move(state.pages);
    if (result.pages.size() == 1U && result.pages.front().empty()) result.pages.clear();
    result.source_bytes = state.bytes;
    result.complete = true;
    result.dynamic = state.dynamic;
    result.interaction = result.interaction || state.dynamic;
    return true;
}

} // namespace pokered::import
