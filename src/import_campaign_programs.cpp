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
constexpr std::array<std::size_t, 3> kStarterPromptTextOffsets{
    0x01D19AU, 0x01D1A4U, 0x01D1AEU};
constexpr std::size_t kStarterEnergeticTextOffset = 0x01D222U;
constexpr std::size_t kPlayerReceivedStarterTextOffset = 0x094EA0U;
constexpr std::size_t kRivalTakesStarterTextOffset = 0x095444U;
constexpr std::size_t kRivalReceivedStarterTextOffset = 0x095461U;
constexpr std::size_t kRivalChallengeTextOffset = 0x095477U;
constexpr std::size_t kRivalPlayerVictoryTextOffset = 0x0954B6U;
constexpr std::size_t kRivalPlayerLossTextOffset = 0x0954E4U;
constexpr std::size_t kRivalExitTextOffset = 0x095502U;
constexpr std::array<std::size_t, 3> kStarterChoiceCodeOffsets{
    0x01D102U, 0x01D113U, 0x01D124U};
constexpr std::array<std::size_t, 3> kRivalStarterPathOffsets{
    0x01CC9CU, 0x01CCB7U, 0x01CCEFU};
constexpr std::size_t kRivalBattleCodeOffset = 0x01CDB9U;
constexpr std::size_t kRivalExitPathOffset = 0x01CE66U;
constexpr std::size_t kPokedexOrderOffset = 0x041024U;
constexpr std::size_t kInternalSpeciesCount = 190U;
constexpr std::uint8_t kTrainerOpponentOffset = 0xC8U;
constexpr std::uint32_t kFollowedOakFlag = static_cast<std::uint32_t>(0xD747U * 8U);
constexpr std::uint32_t kFollowedOakSecondFlag = kFollowedOakFlag + 32U;
constexpr std::uint32_t kOakAskedToChooseFlag = kFollowedOakFlag + 33U;
constexpr std::uint32_t kGotStarterFlag = kFollowedOakFlag + 34U;
constexpr std::uint32_t kBattledLabRivalFlag =
    kFollowedOakFlag + 35U;
constexpr std::uint32_t kOakAppearedFlag = static_cast<std::uint32_t>(0xD74BU * 8U + 7U);
constexpr std::uint8_t kPlayerStarterVariable = 0U;
constexpr std::uint8_t kRivalStarterVariable = 1U;

enum class TriggerKind : std::uint8_t {
    player_y,
    actor_activation,
};

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
    ask_yes_no,
    end_if_choice_no,
    set_variable,
    give_pokemon,
    wait_ticks,
    actor_path_by_player_x,
    start_trainer_battle,
    say_if_player_won,
    say_if_player_lost,
    heal_party,
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

struct Program {
    std::string key;
    TriggerKind trigger_kind{TriggerKind::player_y};
    std::uint8_t trigger_map{};
    std::uint8_t trigger_value{};
    std::uint32_t required_flag{0xFFFFFFFFU};
    std::uint32_t absent_flag{0xFFFFFFFFU};
    std::uint16_t required_variable{0xFFFFU};
    std::uint16_t required_variable_value{};
    std::vector<std::pair<std::uint8_t, std::uint8_t>> initially_hidden;
    std::vector<Instruction> instructions;
};

struct StarterChoice {
    std::uint8_t player_species{};
    std::uint8_t rival_species{};
    std::uint8_t selected_ball{};
    std::uint8_t rival_ball{};
};

struct RivalBattleChoice {
    std::uint8_t rival_species{};
    std::uint8_t trainer_class{};
    std::uint16_t trainer_party{};
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

Instruction species_dialogue(std::vector<std::string> pages,
                             std::uint8_t species_dex) {
    Instruction result = dialogue(std::move(pages));
    result.value = species_dex;
    return result;
}

Instruction ask_yes_no(std::vector<std::string> pages,
                       std::uint8_t species_dex) {
    Instruction result = operation(Opcode::ask_yes_no, 0U, 0U,
                                   species_dex);
    result.pages = std::move(pages);
    return result;
}

bool has_bytes(std::span<const std::uint8_t> rom, std::size_t offset,
               std::span<const std::uint8_t> expected) {
    return offset <= rom.size() && expected.size() <= rom.size() - offset &&
           std::equal(expected.begin(), expected.end(),
                      rom.begin() + static_cast<std::ptrdiff_t>(offset));
}

std::uint8_t dex_for_internal_species(
    std::span<const std::uint8_t> rom, std::uint8_t internal) {
    if (internal == 0U || internal > kInternalSpeciesCount ||
        kPokedexOrderOffset + internal > rom.size())
        return 0U;
    return rom[kPokedexOrderOffset +
               static_cast<std::size_t>(internal - 1U)];
}

bool decode_starter_choice(std::span<const std::uint8_t> rom,
                           std::size_t offset, StarterChoice& result,
                           std::string& error) {
    constexpr std::array<std::uint8_t, 12> signature{
        0x08U, 0x3EU, 0x00U, 0xEAU, 0x3DU, 0xCDU,
        0x3EU, 0x00U, 0xEAU, 0x3EU, 0xCDU, 0x3EU};
    if (offset > rom.size() || 17U > rom.size() - offset) {
        error = "starter choice program extends outside the verified ROM";
        return false;
    }
    for (std::size_t index = 0U; index < signature.size(); ++index)
        if ((index != 2U && index != 7U) &&
            rom[offset + index] != signature[index]) {
            error = "starter choice program signature does not match the pinned ROM";
            return false;
        }
    if (rom[offset + 13U] != 0x06U ||
        (rom[offset + 15U] != 0x18U &&
         rom[offset + 15U] != 0xEAU)) {
        error = "starter choice program has an unknown selection tail";
        return false;
    }

    result = {
        .player_species =
            dex_for_internal_species(rom, rom[offset + 12U]),
        .rival_species =
            dex_for_internal_species(rom, rom[offset + 2U]),
        .selected_ball = rom[offset + 14U],
        .rival_ball = rom[offset + 7U],
    };
    if (result.player_species == 0U || result.player_species > 151U ||
        result.rival_species == 0U || result.rival_species > 151U ||
        result.selected_ball == 0U || result.rival_ball == 0U) {
        error = "starter choice program resolves invalid species or actors";
        return false;
    }
    return true;
}

bool decode_rival_battle_choices(
    std::span<const std::uint8_t> rom,
    const std::array<StarterChoice, 3>& starters,
    std::array<RivalBattleChoice, 3>& result, std::string& error) {
    constexpr std::array<std::pair<std::size_t, std::uint8_t>, 20>
        signature{{
            {0U, 0xFAU},  {1U, 0x30U},  {2U, 0xD7U},
            {3U, 0xCBU},  {4U, 0x47U},  {5U, 0xC0U},
            {6U, 0x3EU},  {8U, 0xEAU},  {9U, 0x59U},
            {10U, 0xD0U}, {11U, 0xFAU}, {12U, 0x15U},
            {13U, 0xD7U}, {14U, 0xFEU}, {16U, 0x20U},
            {18U, 0x3EU}, {22U, 0xFEU}, {24U, 0x20U},
            {26U, 0x3EU}, {30U, 0x3EU},
        }};
    if (kRivalBattleCodeOffset > rom.size() ||
        33U > rom.size() - kRivalBattleCodeOffset) {
        error = "lab rival battle selector extends outside the verified ROM";
        return false;
    }
    for (const auto& [relative, expected] : signature)
        if (rom[kRivalBattleCodeOffset + relative] != expected) {
            error =
                "lab rival battle selector does not match the pinned ROM";
            return false;
        }

    const std::uint8_t opponent =
        rom[kRivalBattleCodeOffset + 7U];
    if (opponent <= kTrainerOpponentOffset) {
        error = "lab rival battle selector has an invalid opponent";
        return false;
    }
    const std::uint8_t trainer_class =
        static_cast<std::uint8_t>(opponent - kTrainerOpponentOffset);
    const std::uint8_t first_species = dex_for_internal_species(
        rom, rom[kRivalBattleCodeOffset + 15U]);
    const std::uint8_t second_species = dex_for_internal_species(
        rom, rom[kRivalBattleCodeOffset + 23U]);
    const std::array<std::uint8_t, 3> one_based_parties{
        rom[kRivalBattleCodeOffset + 19U],
        rom[kRivalBattleCodeOffset + 27U],
        rom[kRivalBattleCodeOffset + 31U],
    };
    if (first_species == 0U || second_species == 0U ||
        first_species == second_species ||
        std::ranges::any_of(one_based_parties,
                           [](std::uint8_t party) {
                               return party == 0U;
                           })) {
        error = "lab rival battle selector resolves invalid content";
        return false;
    }
    const auto count_rival_species = [&](std::uint8_t species) {
        return std::ranges::count_if(
            starters, [species](const StarterChoice& starter) {
                return starter.rival_species == species;
            });
    };
    if (count_rival_species(first_species) != 1 ||
        count_rival_species(second_species) != 1) {
        error =
            "lab rival battle selector does not cover the starter branches";
        return false;
    }

    for (std::size_t index = 0U; index < starters.size(); ++index) {
        const std::uint8_t rival_species =
            starters[index].rival_species;
        const std::size_t party =
            rival_species == first_species
                ? 0U
                : rival_species == second_species ? 1U : 2U;
        result[index] = {
            .rival_species = rival_species,
            .trainer_class = trainer_class,
            .trainer_party = static_cast<std::uint16_t>(
                one_based_parties[party] - 1U),
        };
    }
    return true;
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

void write_program(std::vector<std::uint8_t>& bytes,
                   const Program& program) {
    write_string(bytes, program.key);
    bytes.push_back(static_cast<std::uint8_t>(program.trigger_kind));
    bytes.push_back(program.trigger_map);
    bytes.push_back(program.trigger_value);
    write_u32(bytes, program.required_flag);
    write_u32(bytes, program.absent_flag);
    write_u16(bytes, program.required_variable);
    write_u16(bytes, program.required_variable_value);
    write_u16(bytes, program.initially_hidden.size());
    for (const auto& [map_id, actor_index] : program.initially_hidden) {
        bytes.push_back(map_id);
        bytes.push_back(actor_index);
    }
    write_u16(bytes, program.instructions.size());
    for (const Instruction& instruction : program.instructions) {
        bytes.push_back(static_cast<std::uint8_t>(instruction.opcode));
        bytes.push_back(instruction.a);
        bytes.push_back(instruction.b);
        write_u32(bytes, instruction.value);
        write_pages(bytes, instruction.pages);
        write_path(bytes, instruction.actor_path);
        write_path(bytes, instruction.player_path);
    }
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
        else if (encoded == 0xE0U)
            result.push_back(PathCommand::face_down);
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

GeneratedFile readable_starter_source(
    std::string_view key, const StarterChoice& choice,
    const DecodedTextProgram& prompt,
    const DecodedTextProgram& energetic,
    const DecodedTextProgram& player_received,
    const DecodedTextProgram& rival_takes,
    const DecodedTextProgram& rival_received,
    const std::vector<PathCommand>& rival_path) {
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
    source << "; Lifted from the verified Pokemon Red US rev 0 starter program.\n"
           << "; Species and ball actor IDs below were decoded from the ROM program.\n\n"
           << "campaign_program " << key << '\n'
           << "    source bank_07 actor " << static_cast<unsigned>(choice.selected_ball)
           << '\n'
           << "    trigger map oaks_lab actor_activation "
           << static_cast<unsigned>(choice.selected_ball) << '\n'
           << "    required_flag 0x" << std::hex << kOakAskedToChooseFlag << '\n'
           << "    absent_flag 0x" << kGotStarterFlag << std::dec << '\n'
           << "    lock_input\n"
           << "    ask_yes_no species_dex "
           << static_cast<unsigned>(choice.player_species) << '\n'
           << page_source(prompt.pages, "        ")
           << "    end_if_choice_no\n"
           << "    hide_actor map oaks_lab actor "
           << static_cast<unsigned>(choice.selected_ball) << '\n'
           << "    say species_dex "
           << static_cast<unsigned>(choice.player_species) << '\n'
           << page_source(energetic.pages, "        ")
           << "    say species_dex "
           << static_cast<unsigned>(choice.player_species) << '\n'
           << page_source(player_received.pages, "        ")
           << "    give_pokemon species_dex "
           << static_cast<unsigned>(choice.player_species)
           << " level 5\n"
           << "    set_variable player_starter "
           << static_cast<unsigned>(choice.player_species) << '\n'
           << "    set_variable rival_starter "
           << static_cast<unsigned>(choice.rival_species) << '\n'
           << "    parallel_path map oaks_lab actor 1\n"
           << "        actor";
    for (const PathCommand command : rival_path)
        source << ' ' << path_name(command);
    source << "\n        player";
    for (std::size_t index = 0U; index < rival_path.size(); ++index)
        source << " wait";
    source << "\n"
           << "    face_actor map oaks_lab actor 1 up\n"
           << "    say\n"
           << page_source(rival_takes.pages, "        ")
           << "    hide_actor map oaks_lab actor "
           << static_cast<unsigned>(choice.rival_ball) << '\n'
           << "    say species_dex "
           << static_cast<unsigned>(choice.rival_species) << '\n'
           << page_source(rival_received.pages, "        ")
           << "    set_flag 0x" << std::hex << kGotStarterFlag << std::dec << '\n'
           << "    unlock_input\n"
           << "    end\n";
    const std::string text = source.str();
    return {
        .relative_path =
            "source/scripts/campaign/" + std::string(key) + ".sexpr",
        .bytes = std::vector<std::uint8_t>(text.begin(), text.end()),
    };
}

GeneratedFile readable_rival_battle_source(
    std::string_view key, const RivalBattleChoice& battle,
    const DecodedTextProgram& challenge,
    const DecodedTextProgram& player_victory,
    const DecodedTextProgram& player_loss,
    const DecodedTextProgram& exit_text,
    const std::vector<PathCommand>& exit_path) {
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
    source << "; Lifted from the verified Pokemon Red US rev 0 lab-rival program.\n"
           << "; Trainer class, party, text, flags, and movement are ROM-derived.\n\n"
           << "campaign_program " << key << '\n'
           << "    source bank_07 0x01cdb9\n"
           << "    trigger map oaks_lab player_y 6\n"
           << "    required_flag 0x" << std::hex << kGotStarterFlag << '\n'
           << "    absent_flag 0x" << kBattledLabRivalFlag << std::dec << '\n'
           << "    required_variable rival_starter "
           << static_cast<unsigned>(battle.rival_species) << '\n'
           << "    lock_input\n"
           << "    face_actor map oaks_lab actor 1 down\n"
           << "    face_player up\n"
           << "    say\n"
           << page_source(challenge.pages, "        ")
           << "    move_actor_to_player map oaks_lab actor 1 stop_above 1\n"
           << "    start_trainer_battle class "
           << static_cast<unsigned>(battle.trainer_class)
           << " party " << battle.trainer_party << '\n'
           << "    say_if_player_won\n"
           << page_source(player_victory.pages, "        ")
           << "    say_if_player_lost\n"
           << page_source(player_loss.pages, "        ")
           << "    face_actor map oaks_lab actor 1 down\n"
           << "    heal_party\n"
           << "    set_flag 0x" << std::hex << kBattledLabRivalFlag << std::dec << '\n'
           << "    wait_ticks 20\n"
           << "    say\n"
           << page_source(exit_text.pages, "        ")
           << "    actor_path_by_player_x map oaks_lab actor 1 equals 4\n"
           << "        equal";
    for (const PathCommand command : exit_path)
        source << ' ' << path_name(command);
    source << " right\n"
           << "        otherwise";
    for (const PathCommand command : exit_path)
        source << ' ' << path_name(command);
    source << " left\n"
           << "    hide_actor map oaks_lab actor 1\n"
           << "    unlock_input\n"
           << "    end\n";
    const std::string text = source.str();
    return {
        .relative_path =
            "source/scripts/campaign/" + std::string(key) + ".sexpr",
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
    std::array<DecodedTextProgram, 3> starter_prompts;
    for (std::size_t index = 0U; index < starter_prompts.size(); ++index) {
        if (!decode_text_program(rom, 0x07U,
                                 kStarterPromptTextOffsets[index],
                                 starter_prompts[index]) ||
            starter_prompts[index].pages.empty()) {
            error =
                "Oak's Lab starter prompt could not be decoded from the pinned ROM";
            return false;
        }
    }
    DecodedTextProgram starter_energetic;
    DecodedTextProgram player_received;
    DecodedTextProgram rival_takes;
    DecodedTextProgram rival_received;
    DecodedTextProgram rival_challenge;
    DecodedTextProgram rival_player_victory;
    DecodedTextProgram rival_player_loss;
    DecodedTextProgram rival_exit_text;
    if (!decode_text_program(rom, 0x07U, kStarterEnergeticTextOffset,
                             starter_energetic) ||
        !decode_text_program(rom, 0x25U,
                             kPlayerReceivedStarterTextOffset,
                             player_received) ||
        !decode_text_program(rom, 0x25U,
                             kRivalTakesStarterTextOffset, rival_takes) ||
        !decode_text_program(rom, 0x25U,
                             kRivalReceivedStarterTextOffset,
                             rival_received) ||
        !decode_text_program(rom, 0x25U, kRivalChallengeTextOffset,
                             rival_challenge) ||
        !decode_text_program(rom, 0x25U,
                             kRivalPlayerVictoryTextOffset,
                             rival_player_victory) ||
        !decode_text_program(rom, 0x25U,
                             kRivalPlayerLossTextOffset,
                             rival_player_loss) ||
        !decode_text_program(rom, 0x25U, kRivalExitTextOffset,
                             rival_exit_text) ||
        starter_energetic.pages.empty() ||
        player_received.pages.empty() || rival_takes.pages.empty() ||
        rival_received.pages.empty() || rival_challenge.pages.empty() ||
        rival_player_victory.pages.empty() ||
        rival_player_loss.pages.empty() || rival_exit_text.pages.empty()) {
        error =
            "Oak's Lab starter result dialogue could not be decoded from the pinned ROM";
        return false;
    }

    std::array<StarterChoice, 3> starter_choices;
    for (std::size_t index = 0U; index < starter_choices.size(); ++index)
        if (!decode_starter_choice(rom, kStarterChoiceCodeOffsets[index],
                                   starter_choices[index], error))
            return false;
    std::array<RivalBattleChoice, 3> rival_battles;
    if (!decode_rival_battle_choices(rom, starter_choices,
                                     rival_battles, error))
        return false;

    std::vector<PathCommand> oak_path;
    std::vector<PathCommand> player_path;
    std::vector<PathCommand> lab_oak_path;
    std::vector<PathCommand> lab_player_path;
    if (!decode_rle_path(rom, kPalletOakPathOffset, false, oak_path, error) ||
        !decode_rle_path(rom, kPalletPlayerPathOffset, true, player_path, error) ||
        !decode_direct_npc_path(rom, kOakLabEntryPathOffset, lab_oak_path, error) ||
        !decode_rle_path(rom, kOakLabPlayerPathOffset, true, lab_player_path, error))
        return false;
    std::array<std::vector<PathCommand>, 3> rival_starter_paths;
    for (std::size_t index = 0U; index < rival_starter_paths.size();
         ++index)
        if (!decode_direct_npc_path(
                rom, kRivalStarterPathOffsets[index],
                rival_starter_paths[index], error))
            return false;
    std::vector<PathCommand> rival_exit_path;
    if (!decode_direct_npc_path(rom, kRivalExitPathOffset,
                                rival_exit_path, error))
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

    Program opening;
    opening.key = "pallet_oak_interception";
    opening.trigger_map = 0U;
    opening.trigger_value = 1U;
    opening.absent_flag = kFollowedOakFlag;
    opening.initially_hidden = {{0U, 1U}, {40U, 5U}, {40U, 8U}};
    opening.instructions = std::move(instructions);

    constexpr std::array<std::string_view, 3> starter_keys{
        "oaks_lab_choose_charmander",
        "oaks_lab_choose_squirtle",
        "oaks_lab_choose_bulbasaur",
    };
    constexpr std::array<std::string_view, 3> rival_battle_keys{
        "oaks_lab_first_rival_after_charmander",
        "oaks_lab_first_rival_after_squirtle",
        "oaks_lab_first_rival_after_bulbasaur",
    };
    std::vector<Program> programs;
    programs.reserve(1U + starter_choices.size() +
                     rival_battles.size());
    programs.push_back(std::move(opening));
    for (std::size_t index = 0U; index < starter_choices.size(); ++index) {
        const StarterChoice& choice = starter_choices[index];
        Program starter;
        starter.key = starter_keys[index];
        starter.trigger_kind = TriggerKind::actor_activation;
        starter.trigger_map = 40U;
        starter.trigger_value = choice.selected_ball;
        starter.required_flag = kOakAskedToChooseFlag;
        starter.absent_flag = kGotStarterFlag;

        starter.instructions.push_back(operation(Opcode::lock_input));
        starter.instructions.push_back(
            ask_yes_no(starter_prompts[index].pages,
                       choice.player_species));
        starter.instructions.push_back(
            operation(Opcode::end_if_choice_no));
        starter.instructions.push_back(operation(
            Opcode::hide_actor, choice.selected_ball, 0U, 40U));
        starter.instructions.push_back(species_dialogue(
            starter_energetic.pages, choice.player_species));
        starter.instructions.push_back(species_dialogue(
            player_received.pages, choice.player_species));
        starter.instructions.push_back(operation(
            Opcode::give_pokemon, 5U, 0U, choice.player_species));
        starter.instructions.push_back(operation(
            Opcode::set_variable, kPlayerStarterVariable, 0U,
            choice.player_species));
        starter.instructions.push_back(operation(
            Opcode::set_variable, kRivalStarterVariable, 0U,
            choice.rival_species));

        Instruction rival_walk =
            operation(Opcode::parallel_path, 1U, 0U, 40U);
        rival_walk.actor_path = rival_starter_paths[index];
        rival_walk.player_path = std::vector<PathCommand>(
            rival_walk.actor_path.size(), PathCommand::wait);
        starter.instructions.push_back(std::move(rival_walk));
        starter.instructions.push_back(
            operation(Opcode::face_actor, 1U, 1U, 40U));
        starter.instructions.push_back(dialogue(rival_takes.pages));
        starter.instructions.push_back(operation(
            Opcode::hide_actor, choice.rival_ball, 0U, 40U));
        starter.instructions.push_back(species_dialogue(
            rival_received.pages, choice.rival_species));
        starter.instructions.push_back(
            operation(Opcode::set_flag, 0U, 0U, kGotStarterFlag));
        starter.instructions.push_back(operation(Opcode::unlock_input));
        starter.instructions.push_back(operation(Opcode::end));
        programs.push_back(std::move(starter));
    }

    for (std::size_t index = 0U; index < rival_battles.size();
         ++index) {
        const RivalBattleChoice& battle = rival_battles[index];
        Program rival;
        rival.key = rival_battle_keys[index];
        rival.trigger_map = 40U;
        rival.trigger_value = 6U;
        rival.required_flag = kGotStarterFlag;
        rival.absent_flag = kBattledLabRivalFlag;
        rival.required_variable = kRivalStarterVariable;
        rival.required_variable_value = battle.rival_species;

        rival.instructions.push_back(operation(Opcode::lock_input));
        rival.instructions.push_back(
            operation(Opcode::face_actor, 1U, 0U, 40U));
        rival.instructions.push_back(
            operation(Opcode::face_player, 1U));
        rival.instructions.push_back(dialogue(rival_challenge.pages));
        rival.instructions.push_back(operation(
            Opcode::move_actor_to_player, 1U, 0xFFU, 40U));
        rival.instructions.push_back(operation(
            Opcode::start_trainer_battle, battle.trainer_class, 0U,
            battle.trainer_party));

        Instruction victory =
            operation(Opcode::say_if_player_won);
        victory.pages = rival_player_victory.pages;
        rival.instructions.push_back(std::move(victory));
        Instruction loss = operation(Opcode::say_if_player_lost);
        loss.pages = rival_player_loss.pages;
        rival.instructions.push_back(std::move(loss));
        rival.instructions.push_back(
            operation(Opcode::face_actor, 1U, 0U, 40U));
        rival.instructions.push_back(operation(Opcode::heal_party));
        rival.instructions.push_back(operation(
            Opcode::set_flag, 0U, 0U, kBattledLabRivalFlag));
        rival.instructions.push_back(
            operation(Opcode::wait_ticks, 0U, 0U, 20U));
        rival.instructions.push_back(dialogue(rival_exit_text.pages));

        Instruction exit_path = operation(
            Opcode::actor_path_by_player_x, 1U, 4U, 40U);
        exit_path.actor_path = rival_exit_path;
        exit_path.actor_path.push_back(PathCommand::right);
        exit_path.player_path = rival_exit_path;
        exit_path.player_path.push_back(PathCommand::left);
        rival.instructions.push_back(std::move(exit_path));
        rival.instructions.push_back(
            operation(Opcode::hide_actor, 1U, 0U, 40U));
        rival.instructions.push_back(operation(Opcode::unlock_input));
        rival.instructions.push_back(operation(Opcode::end));
        programs.push_back(std::move(rival));
    }

    std::vector<std::uint8_t> cache{'P', 'C', 'P', '3'};
    write_u16(cache, programs.size());
    for (const Program& program : programs) write_program(cache, program);

    result.files.push_back(readable_pallet_source(hey_wait.pages, unsafe.pages, oak_path,
                                                  player_path, lab_oak_path, lab_player_path,
                                                  lab_choice_text));
    for (std::size_t index = 0U; index < starter_choices.size(); ++index)
        result.files.push_back(readable_starter_source(
            starter_keys[index], starter_choices[index],
            starter_prompts[index], starter_energetic,
            player_received, rival_takes, rival_received,
            rival_starter_paths[index]));
    for (std::size_t index = 0U; index < rival_battles.size(); ++index)
        result.files.push_back(readable_rival_battle_source(
            rival_battle_keys[index], rival_battles[index],
            rival_challenge, rival_player_victory, rival_player_loss,
            rival_exit_text, rival_exit_path));
    result.files.push_back({"compiled/campaign_programs.bin", std::move(cache)});
    result.programs = programs.size();
    error.clear();
    return true;
}

} // namespace pokered::import
