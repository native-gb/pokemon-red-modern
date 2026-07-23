#include "import_campaign_programs.hpp"

#include "import_text.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pokered::import {
namespace {

constexpr std::size_t kPalletDefaultScriptOffset = 0x018E81U;
constexpr std::size_t kPalletOakAppearedOffset = 0x018EA7U;
constexpr std::size_t kPalletHeyWaitTextOffset = 0x018FB0U;
constexpr std::size_t kPalletUnsafeTextOffset = 0x018FCEU;
constexpr std::size_t kPalletOakPathOffset = 0x01A4DCU;
constexpr std::size_t kPalletPlayerPathOffset = 0x01A4E9U;
constexpr std::size_t kOakLabEntryPathOffset = 0x01CB7EU;
constexpr std::size_t kOakLabPlayerPathOffset = 0x01CBCFU;
constexpr std::array<std::size_t, 4> kOakLabChoiceTextOffsets{0x01D34FU, 0x01D35EU, 0x01D36DU,
                                                              0x01D37CU};
constexpr std::uint32_t kFollowedOakFlag = static_cast<std::uint32_t>(0xD747U * 8U);
constexpr std::uint32_t kFollowedOakSecondFlag = kFollowedOakFlag + 32U;
constexpr std::uint32_t kOakAskedToChooseFlag = kFollowedOakFlag + 33U;
constexpr std::uint32_t kOakAppearedFlag = static_cast<std::uint32_t>(0xD74BU * 8U + 7U);

enum class Opcode : std::uint8_t {
    lock_input,
    set_flag,
    show_actor,
    hide_actor,
    say,
    face_actor,
    face_player,
    move_actor_to_player,
    align_pair_x,
    parallel_path,
    unlock_input,
    end,
};

enum class PathCommand : std::uint8_t {
    down,
    up,
    left,
    right,
    wait,
    face_down,
};

struct Instruction {
    Opcode opcode{Opcode::end};
    std::uint8_t a{};
    std::uint8_t b{};
    std::uint32_t value{};
    std::vector<std::string> pages;
    std::vector<PathCommand> actor_path;
    std::vector<PathCommand> player_path;
};

Instruction operation(Opcode opcode, std::uint8_t a = 0U, std::uint8_t b = 0U,
                      std::uint32_t value = 0U) {
    Instruction result;
    result.opcode = opcode;
    result.a = a;
    result.b = b;
    result.value = value;
    return result;
}

Instruction dialogue(std::vector<std::string> pages) {
    Instruction result = operation(Opcode::say);
    result.pages = std::move(pages);
    return result;
}

bool has_bytes(std::span<const std::uint8_t> rom, std::size_t offset,
               std::span<const std::uint8_t> expected) {
    return offset <= rom.size() && expected.size() <= rom.size() - offset &&
           std::equal(expected.begin(), expected.end(),
                      rom.begin() + static_cast<std::ptrdiff_t>(offset));
}

void write_u16(std::vector<std::uint8_t>& bytes, std::size_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void write_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (unsigned shift = 0U; shift < 32U; shift += 8U)
        bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
}

void write_string(std::vector<std::uint8_t>& bytes, std::string_view value) {
    write_u16(bytes, value.size());
    bytes.insert(bytes.end(), value.begin(), value.end());
}

void write_pages(std::vector<std::uint8_t>& bytes, const std::vector<std::string>& pages) {
    write_u16(bytes, pages.size());
    for (const std::string& page : pages)
        write_string(bytes, page);
}

void write_path(std::vector<std::uint8_t>& bytes, const std::vector<PathCommand>& path) {
    write_u16(bytes, path.size());
    for (const PathCommand command : path)
        bytes.push_back(static_cast<std::uint8_t>(command));
}

bool decode_rle_path(std::span<const std::uint8_t> rom, std::size_t offset, bool joypad,
                     std::vector<PathCommand>& result, std::string& error) {
    result.clear();
    std::size_t cursor = offset;
    for (std::size_t records = 0U; records < 64U; ++records) {
        if (cursor >= rom.size()) {
            error = "campaign movement RLE extends outside the verified ROM";
            return false;
        }
        const std::uint8_t encoded = rom[cursor++];
        if (encoded == 0xFFU) return !result.empty();
        if (cursor >= rom.size() || rom[cursor] == 0U) {
            error = "campaign movement RLE has an invalid run";
            return false;
        }
        PathCommand command;
        if (joypad) {
            switch (encoded) {
            case 0x40U:
                command = PathCommand::up;
                break;
            case 0x80U:
                command = PathCommand::down;
                break;
            case 0x20U:
                command = PathCommand::left;
                break;
            case 0x10U:
                command = PathCommand::right;
                break;
            default:
                error = "campaign joypad RLE contains an unknown command";
                return false;
            }
        } else
            switch (encoded) {
            case 0x00U:
                command = PathCommand::down;
                break;
            case 0x40U:
                command = PathCommand::up;
                break;
            case 0x80U:
                command = PathCommand::left;
                break;
            case 0xC0U:
                command = PathCommand::right;
                break;
            case 0xE0U:
                command = PathCommand::face_down;
                break;
            default:
                error = "campaign movement RLE contains an unknown command";
                return false;
            }
        const std::uint8_t count = rom[cursor++];
        result.insert(result.end(), count, command);
    }
    error = "campaign movement RLE is missing its terminator";
    return false;
}

bool decode_direct_npc_path(std::span<const std::uint8_t> rom, std::size_t offset,
                            std::vector<PathCommand>& result, std::string& error) {
    result.clear();
    for (std::size_t cursor = offset; cursor < rom.size() && result.size() < 1024U; ++cursor) {
        const std::uint8_t encoded = rom[cursor];
        if (encoded == 0xFFU) {
            if (!result.empty()) return true;
            break;
        }
        if (encoded == 0x00U)
            result.push_back(PathCommand::down);
        else if (encoded == 0x40U)
            result.push_back(PathCommand::up);
        else if (encoded == 0x80U)
            result.push_back(PathCommand::left);
        else if (encoded == 0xC0U)
            result.push_back(PathCommand::right);
        else {
            error = "campaign direct NPC path contains an unknown command";
            return false;
        }
    }
    error = "campaign direct NPC path is missing its terminator";
    return false;
}

std::string page_source(const std::vector<std::string>& pages, std::string_view indentation) {
    const auto quote = [](std::string_view value) {
        std::string result{"\""};
        for (const char character : value) {
            if (character == '\\')
                result += "\\\\";
            else if (character == '"')
                result += "\\\"";
            else if (character == '\n')
                result += "\\n";
            else if (character == '\r')
                result += "\\r";
            else if (character == '\t')
                result += "\\t";
            else
                result.push_back(character);
        }
        result.push_back('"');
        return result;
    };
    std::ostringstream output;
    for (const std::string& page : pages)
        output << indentation << "page " << quote(page) << '\n';
    return output.str();
}

GeneratedFile readable_pallet_source(const std::vector<std::string>& hey_wait,
                                     const std::vector<std::string>& unsafe,
                                     const std::vector<PathCommand>& oak_path,
                                     const std::vector<PathCommand>& player_path,
                                     const std::vector<PathCommand>& lab_oak_path,
                                     const std::vector<PathCommand>& lab_player_path,
                                     const std::array<DecodedTextProgram, 4>& lab_choice_text) {
    const auto path_name = [](PathCommand command) {
        switch (command) {
        case PathCommand::down:
            return "down";
        case PathCommand::up:
            return "up";
        case PathCommand::left:
            return "left";
        case PathCommand::right:
            return "right";
        case PathCommand::wait:
            return "wait";
        case PathCommand::face_down:
            return "face_down";
        }
        return "invalid";
    };
    std::ostringstream source;
    source << "; Lifted from the verified Pokemon Red US rev 0 campaign program.\n"
           << "; Numeric source addresses are importer evidence, not runtime dependencies.\n\n"
           << "campaign_program pallet_oak_interception\n"
           << "    source bank_06 0x018e81\n"
           << "    trigger map pallet_town player_y 1\n"
           << "    absent_flag 0x" << std::hex << kFollowedOakFlag << std::dec << "\n"
           << "    initially_hide actor pallet_town 1\n"
           << "    initially_hide actor oaks_lab 5\n"
           << "    initially_hide actor oaks_lab 8\n"
           << "    lock_input\n"
           << "    set_flag 0x" << std::hex << kOakAppearedFlag << std::dec << "\n"
           << "    say\n"
           << page_source(hey_wait, "        ") << "    show_actor 1\n"
           << "    face_actor 1 up\n"
           << "    move_actor_to_player 1 stop_below 1\n"
           << "    face_player down\n"
           << "    say\n"
           << page_source(unsafe, "        ") << "    align_pair_x actor 1 x 10\n"
           << "    parallel_path actor 1 hide_at_end\n"
           << "        actor";
    for (const PathCommand command : oak_path)
        source << ' ' << path_name(command);
    source << "\n        player";
    for (const PathCommand command : player_path)
        source << ' ' << path_name(command);
    source << "\n"
           << "    show_actor map oaks_lab actor 8\n"
           << "    parallel_path map oaks_lab actor 8 hide_at_end\n"
           << "        actor";
    for (const PathCommand command : lab_oak_path)
        source << ' ' << path_name(command);
    source << "\n        player";
    for (std::size_t index = 0U; index < lab_oak_path.size(); ++index)
        source << " wait";
    source << "\n"
           << "    show_actor map oaks_lab actor 5\n"
           << "    face_actor map oaks_lab actor 5 down\n"
           << "    face_actor map oaks_lab actor 1 up\n"
           << "    parallel_path map oaks_lab actor 5\n"
           << "        actor";
    for (std::size_t index = 0U; index < lab_player_path.size(); ++index)
        source << " wait";
    source << "\n        player";
    for (const PathCommand command : lab_player_path)
        source << ' ' << path_name(command);
    source << "\n"
           << "    set_flag 0x" << std::hex << kFollowedOakFlag << "\n"
           << "    set_flag 0x" << kFollowedOakSecondFlag << std::dec << "\n"
           << "    say\n"
           << page_source(lab_choice_text[0].pages, "        ") << "    say\n"
           << page_source(lab_choice_text[1].pages, "        ") << "    say\n"
           << page_source(lab_choice_text[2].pages, "        ") << "    say\n"
           << page_source(lab_choice_text[3].pages, "        ") << "    set_flag 0x" << std::hex
           << kOakAskedToChooseFlag << std::dec << "\n"
           << "    unlock_input\n"
           << "    end\n";
    const std::string text = source.str();
    return {
        .relative_path = "source/scripts/campaign/pallet_oak_interception.sexpr",
        .bytes = std::vector<std::uint8_t>(text.begin(), text.end()),
    };
}

} // namespace

bool decode_campaign_program_import(std::span<const std::uint8_t> rom,
                                    CampaignProgramImport& result, std::string& error) {
    result = {};
    if (!verify_pokemon_red_us_rev_0(rom, error)) return false;

    constexpr std::array<std::uint8_t, 12> default_signature{
        0xFAU, 0x47U, 0xD7U, 0xCBU, 0x47U, 0xC0U, 0xFAU, 0x61U, 0xD3U, 0xFEU, 0x01U, 0xC0U};
    constexpr std::array<std::uint8_t, 5> appeared_signature{0x21U, 0x4BU, 0xD7U, 0xCBU, 0xFEU};
    if (!has_bytes(rom, kPalletDefaultScriptOffset, default_signature) ||
        !has_bytes(rom, kPalletOakAppearedOffset, appeared_signature)) {
        error = "Pallet campaign control-flow signatures do not match the pinned ROM";
        return false;
    }

    DecodedTextProgram hey_wait;
    DecodedTextProgram unsafe;
    if (!decode_text_program(rom, 0x06U, kPalletHeyWaitTextOffset, hey_wait) ||
        !decode_text_program(rom, 0x06U, kPalletUnsafeTextOffset, unsafe) ||
        hey_wait.pages.empty() || unsafe.pages.empty()) {
        error = "Pallet campaign dialogue could not be decoded from the pinned ROM";
        return false;
    }
    std::array<DecodedTextProgram, 4> lab_choice_text;
    for (std::size_t index = 0U; index < lab_choice_text.size(); ++index) {
        if (!decode_text_program(rom, 0x07U, kOakLabChoiceTextOffsets[index],
                                 lab_choice_text[index]) ||
            lab_choice_text[index].pages.empty()) {
            error = "Oak's Lab choice dialogue could not be decoded from the pinned ROM";
            return false;
        }
    }

    std::vector<PathCommand> oak_path;
    std::vector<PathCommand> player_path;
    std::vector<PathCommand> lab_oak_path;
    std::vector<PathCommand> lab_player_path;
    if (!decode_rle_path(rom, kPalletOakPathOffset, false, oak_path, error) ||
        !decode_rle_path(rom, kPalletPlayerPathOffset, true, player_path, error) ||
        !decode_direct_npc_path(rom, kOakLabEntryPathOffset, lab_oak_path, error) ||
        !decode_rle_path(rom, kOakLabPlayerPathOffset, true, lab_player_path, error))
        return false;

    // The simulated joypad buffer is consumed from its final index toward zero.
    // Reverse its decoded command stream; MoveSprite consumes Oak's stream forward.
    std::reverse(player_path.begin(), player_path.end());
    std::reverse(lab_player_path.begin(), lab_player_path.end());
    if (player_path.size() < 2U || player_path[player_path.size() - 1U] != PathCommand::up ||
        player_path[player_path.size() - 2U] != PathCommand::up) {
        error = "Pallet player path no longer ends in the verified lab transition";
        return false;
    }
    // Native warps materialize directly on their destination cell. The second
    // original UP is consumed by the Game Boy transition, so the normalized
    // native path stops after crossing the warp.
    player_path.pop_back();

    std::vector<Instruction> instructions;
    instructions.push_back(operation(Opcode::lock_input));
    instructions.push_back(operation(Opcode::set_flag, 0U, 0U, kOakAppearedFlag));
    instructions.push_back(dialogue(hey_wait.pages));
    instructions.push_back(operation(Opcode::show_actor, 1U, 0U, 0U));
    instructions.push_back(operation(Opcode::face_actor, 1U, 1U, 0U));
    instructions.push_back(operation(Opcode::move_actor_to_player, 1U, 1U, 0U));
    instructions.push_back(operation(Opcode::face_player));
    instructions.push_back(dialogue(unsafe.pages));
    instructions.push_back(operation(Opcode::align_pair_x, 1U, 10U, 0U));
    Instruction walk = operation(Opcode::parallel_path, 1U, 1U, 0U);
    walk.actor_path = oak_path;
    walk.player_path = player_path;
    instructions.push_back(std::move(walk));
    instructions.push_back(operation(Opcode::show_actor, 8U, 0U, 40U));
    Instruction oak_enters = operation(Opcode::parallel_path, 8U, 1U, 40U);
    oak_enters.actor_path = lab_oak_path;
    oak_enters.player_path = std::vector<PathCommand>(lab_oak_path.size(), PathCommand::wait);
    instructions.push_back(std::move(oak_enters));
    instructions.push_back(operation(Opcode::show_actor, 5U, 0U, 40U));
    instructions.push_back(operation(Opcode::face_actor, 5U, 0U, 40U));
    instructions.push_back(operation(Opcode::face_actor, 1U, 1U, 40U));
    Instruction player_enters = operation(Opcode::parallel_path, 5U, 0U, 40U);
    player_enters.actor_path = std::vector<PathCommand>(lab_player_path.size(), PathCommand::wait);
    player_enters.player_path = lab_player_path;
    instructions.push_back(std::move(player_enters));
    instructions.push_back(operation(Opcode::set_flag, 0U, 0U, kFollowedOakFlag));
    instructions.push_back(operation(Opcode::set_flag, 0U, 0U, kFollowedOakSecondFlag));
    for (const DecodedTextProgram& text : lab_choice_text)
        instructions.push_back(dialogue(text.pages));
    instructions.push_back(operation(Opcode::set_flag, 0U, 0U, kOakAskedToChooseFlag));
    instructions.push_back(operation(Opcode::unlock_input));
    instructions.push_back(operation(Opcode::end));

    std::vector<std::uint8_t> cache{'P', 'C', 'P', '1'};
    write_u16(cache, 1U);
    write_string(cache, "pallet_oak_interception");
    cache.push_back(0U);
    cache.push_back(1U);
    write_u32(cache, kFollowedOakFlag);
    write_u16(cache, 3U);
    cache.push_back(0U);
    cache.push_back(1U);
    cache.push_back(40U);
    cache.push_back(5U);
    cache.push_back(40U);
    cache.push_back(8U);
    write_u16(cache, instructions.size());
    for (const Instruction& instruction : instructions) {
        cache.push_back(static_cast<std::uint8_t>(instruction.opcode));
        cache.push_back(instruction.a);
        cache.push_back(instruction.b);
        write_u32(cache, instruction.value);
        write_pages(cache, instruction.pages);
        write_path(cache, instruction.actor_path);
        write_path(cache, instruction.player_path);
    }

    result.files.push_back(readable_pallet_source(hey_wait.pages, unsafe.pages, oak_path,
                                                  player_path, lab_oak_path, lab_player_path,
                                                  lab_choice_text));
    result.files.push_back({"compiled/campaign_programs.bin", std::move(cache)});
    result.programs = 1U;
    error.clear();
    return true;
}

} // namespace pokered::import
