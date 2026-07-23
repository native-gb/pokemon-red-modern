#include "import_battle_animations.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>

namespace pokered::import {
namespace {

constexpr std::string_view kExpectedSha1 = "ea9bcae617fdf159b045185467ae58b2e4a48b9a";
constexpr std::size_t kBankOffset = 0x78000;
constexpr std::size_t kBankEnd = 0x7C000;
constexpr std::size_t kProgramPointers = 0x7A07D;
constexpr std::size_t kProgramCount = 203;
constexpr std::size_t kSubanimationPointers = 0x7A76D;
constexpr std::size_t kSubanimationCount = 86;
constexpr std::size_t kFrameBlockPointers = 0x7AF74;
constexpr std::size_t kFrameBlockCount = 122;
constexpr std::size_t kBaseCoordinates = 0x7BC85;
constexpr std::size_t kBaseCoordinateCount = 177;
constexpr std::size_t kMoveNamesBegin = 0xB0000;
constexpr std::size_t kMoveNamesEnd = 0xB060F;
constexpr std::size_t kMoveCount = 165;
constexpr std::size_t kTileSet0 = 0x781FE;
constexpr std::size_t kTileSet1 = 0x786EE;
constexpr std::size_t kTileCount = 79;
constexpr std::uint8_t kFirstSpecialEffect = 0xD8;
constexpr std::size_t kDelayRoutine = 0x79150;
constexpr std::size_t kFlashRoutine = 0x791BE;
constexpr std::size_t kBlinkRoutine = 0x7936F;
constexpr std::size_t kMoveHorizontalRoutine = 0x793F9;
constexpr std::size_t kSpiralRoutine = 0x79424;
constexpr std::size_t kBounceRoutine = 0x7977A;

constexpr std::array<std::string_view, 38> kExtraAnimationNames = {
    "show_pic",          "enemy_flash",        "player_flash",
    "enemy_hud_shake",   "trade_ball_drop",    "trade_ball_appear_1",
    "trade_ball_appear_2", "trade_ball_poof",  "player_x_stat_item",
    "enemy_x_stat_item", "player_shrink",       "enemy_shrink",
    "player_x_stat_black", "enemy_x_stat_black", "player_shrink_black",
    "enemy_shrink_black", "player_unused",      "enemy_unused",
    "player_paralyze",   "enemy_paralyze",      "player_poison",
    "enemy_poison",      "player_sleep",         "enemy_sleep",
    "player_confused",   "enemy_confused",      "slide_down",
    "ball_toss",         "ball_shake",           "ball_poof",
    "ball_block",        "great_ball_toss",      "ultra_ball_toss",
    "shake_screen",      "hide_picture",         "safari_rock",
    "safari_bait",       "zig_zag_screen",
};

constexpr std::array<std::string_view, 39> kSpecialEffectNames = {
    "wavy_screen",             "substitute_mon",          "shake_back_and_forth",
    "slide_enemy_mon_off",     "show_enemy_mon_pic",      "show_mon_pic",
    "blink_enemy_mon",         "hide_enemy_mon_pic",      "flash_enemy_mon_pic",
    "delay_animation_10",      "spiral_balls_inward",     "shake_enemy_hud_2",
    "shake_enemy_hud",         "slide_mon_half_off",      "petals_falling",
    "leaves_falling",          "transform_mon",           "slide_mon_down_and_hide",
    "minimize_mon",            "bounce_up_and_down",      "shoot_many_balls_upward",
    "shoot_balls_upward",      "squish_mon_pic",          "hide_mon_pic",
    "light_screen_palette",    "reset_mon_position",      "move_mon_horizontally",
    "blink_mon",               "slide_mon_off",           "flash_mon_pic",
    "slide_mon_down",          "slide_mon_up",            "flash_screen_long",
    "darken_mon_palette",      "water_droplets_everywhere", "shake_screen",
    "reset_screen_palette",    "dark_screen_palette",     "dark_screen_flash",
};

struct Sha1 {
    std::array<std::uint32_t, 5> state{
        0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U, 0xC3D2E1F0U,
    };

    void block(std::span<const std::uint8_t, 64> bytes) {
        std::array<std::uint32_t, 80> words{};
        for (std::size_t index = 0; index < 16; ++index) {
            const std::size_t offset = index * 4;
            words[index] = static_cast<std::uint32_t>(bytes[offset]) << 24U |
                           static_cast<std::uint32_t>(bytes[offset + 1]) << 16U |
                           static_cast<std::uint32_t>(bytes[offset + 2]) << 8U |
                           static_cast<std::uint32_t>(bytes[offset + 3]);
        }
        for (std::size_t index = 16; index < words.size(); ++index)
            words[index] = std::rotl(words[index - 3] ^ words[index - 8] ^
                                         words[index - 14] ^ words[index - 16],
                                     1);

        std::uint32_t a = state[0];
        std::uint32_t b = state[1];
        std::uint32_t c = state[2];
        std::uint32_t d = state[3];
        std::uint32_t e = state[4];
        for (std::size_t index = 0; index < words.size(); ++index) {
            std::uint32_t function = 0;
            std::uint32_t constant = 0;
            if (index < 20) {
                function = (b & c) | (~b & d);
                constant = 0x5A827999U;
            } else if (index < 40) {
                function = b ^ c ^ d;
                constant = 0x6ED9EBA1U;
            } else if (index < 60) {
                function = (b & c) | (b & d) | (c & d);
                constant = 0x8F1BBCDCU;
            } else {
                function = b ^ c ^ d;
                constant = 0xCA62C1D6U;
            }
            const std::uint32_t temporary =
                std::rotl(a, 5) + function + e + constant + words[index];
            e = d;
            d = c;
            c = std::rotl(b, 30);
            b = a;
            a = temporary;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
    }
};

std::string sha1(std::span<const std::uint8_t> bytes) {
    std::vector<std::uint8_t> padded(bytes.begin(), bytes.end());
    const std::uint64_t bit_count = static_cast<std::uint64_t>(bytes.size()) * 8U;
    padded.push_back(0x80);
    while (padded.size() % 64U != 56U)
        padded.push_back(0);
    for (int shift = 56; shift >= 0; shift -= 8)
        padded.push_back(static_cast<std::uint8_t>(bit_count >> static_cast<unsigned>(shift)));

    Sha1 hash;
    for (std::size_t offset = 0; offset < padded.size(); offset += 64) {
        std::array<std::uint8_t, 64> block{};
        std::copy_n(padded.begin() + static_cast<std::ptrdiff_t>(offset), block.size(),
                    block.begin());
        hash.block(block);
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const std::uint32_t value : hash.state)
        output << std::setw(8) << value;
    return output.str();
}

bool has_range(std::span<const std::uint8_t> rom, std::size_t offset, std::size_t size) {
    return offset <= rom.size() && size <= rom.size() - offset;
}

bool require_range(std::span<const std::uint8_t> rom, std::size_t offset, std::size_t size,
                   std::string_view label, std::string& error) {
    if (has_range(rom, offset, size)) return true;
    error = std::string(label) + " extends outside the verified ROM";
    return false;
}

std::uint16_t read_u16(std::span<const std::uint8_t> rom, std::size_t offset) {
    return static_cast<std::uint16_t>(rom[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(rom[offset + 1]) << 8U);
}

std::optional<std::size_t> bank_offset(std::uint16_t pointer) {
    if (pointer < 0x4000 || pointer >= 0x8000) return std::nullopt;
    return kBankOffset + pointer - 0x4000U;
}

std::string hex_offset(std::size_t value) {
    std::ostringstream output;
    output << "0x" << std::hex << std::setfill('0') << std::setw(5) << value;
    return output.str();
}

std::optional<char> decode_character(std::uint8_t value) {
    if (value >= 0x80 && value <= 0x99)
        return static_cast<char>('A' + value - 0x80);
    if (value >= 0xA0 && value <= 0xB9)
        return static_cast<char>('a' + value - 0xA0);
    if (value >= 0xF6)
        return static_cast<char>('0' + value - 0xF6);
    switch (value) {
    case 0x7F: return ' ';
    case 0x9A: return '(';
    case 0x9B: return ')';
    case 0x9C: return ':';
    case 0x9D: return ';';
    case 0x9E: return '[';
    case 0x9F: return ']';
    case 0xBA: return 'e';
    case 0xE0: return '\'';
    case 0xE3: return '-';
    case 0xE6: return '?';
    case 0xE7: return '!';
    case 0xE8: return '.';
    case 0xF3: return '/';
    case 0xF4: return ',';
    default: return std::nullopt;
    }
}

std::string snake_case(std::string_view text) {
    std::string result;
    bool separator = false;
    for (const char raw_character : text) {
        const auto character = static_cast<unsigned char>(raw_character);
        const bool alpha = (character >= 'A' && character <= 'Z') ||
                           (character >= 'a' && character <= 'z');
        const bool digit = character >= '0' && character <= '9';
        if (!alpha && !digit) {
            separator = !result.empty();
            continue;
        }
        if (separator && result.back() != '_') result.push_back('_');
        separator = false;
        result.push_back(character >= 'A' && character <= 'Z'
                             ? static_cast<char>(character - 'A' + 'a')
                             : static_cast<char>(character));
    }
    if (!result.empty() && result.front() >= '0' && result.front() <= '9')
        result.insert(0, "animation_");
    return result;
}

struct Command {
    bool special{};
    std::uint8_t effect{};
    std::uint8_t sound{};
    std::uint8_t tile_set{};
    std::uint8_t delay{};
    std::uint8_t subanimation{};
};

struct Program {
    std::size_t id{};
    std::string name;
    std::size_t begin{};
    std::size_t end{};
    std::vector<Command> commands;
};

struct Subanimation {
    std::uint8_t transform{};
    std::vector<std::array<std::uint8_t, 3>> frames;
};

struct BlockPiece {
    std::int8_t y{};
    std::int8_t x{};
    std::uint8_t tile{};
    std::uint8_t attributes{};
};

using FrameBlock = std::vector<BlockPiece>;

struct VisualPiece {
    std::int16_t x{};
    std::int16_t y{};
    std::uint8_t tile_set{};
    std::uint8_t tile{};
    std::uint8_t attributes{};
    auto operator<=>(const VisualPiece&) const = default;
};

using Visual = std::vector<VisualPiece>;

struct ProceduralParameters {
    std::uint8_t delay_frames{};
    std::uint8_t flash_delay{};
    std::uint8_t blink_repetitions{};
    std::uint8_t blink_delay{};
    std::uint8_t move_horizontal_delay{};
    std::int8_t spiral_enemy_y{};
    std::uint8_t spiral_enemy_x{};
    std::uint8_t spiral_tile{};
    std::uint8_t spiral_objects{};
    std::uint8_t spiral_delay{};
    std::uint8_t bounce_repetitions{};
};

bool decode_move_names(std::span<const std::uint8_t> rom, std::vector<std::string>& names,
                       std::string& error) {
    std::size_t cursor = kMoveNamesBegin;
    while (cursor < kMoveNamesEnd && names.size() < kMoveCount) {
        std::string name;
        while (cursor < kMoveNamesEnd && rom[cursor] != 0x50) {
            const auto character = decode_character(rom[cursor++]);
            if (!character) {
                error = "move-name table contains an unsupported character";
                return false;
            }
            name.push_back(*character);
        }
        if (cursor >= kMoveNamesEnd) {
            error = "move-name table is missing a terminator";
            return false;
        }
        ++cursor;
        names.push_back(std::move(name));
    }
    if (names.size() == kMoveCount) return true;
    error = "move-name table has an unexpected record count";
    return false;
}

bool decode_programs(std::span<const std::uint8_t> rom, const std::vector<std::string>& move_names,
                     std::vector<Program>& programs, std::string& error) {
    programs.reserve(kProgramCount);
    for (std::size_t index = 0; index < kProgramCount; ++index) {
        const auto source = bank_offset(read_u16(rom, kProgramPointers + index * 2U));
        if (!source) {
            error = "animation program has an invalid bank pointer";
            return false;
        }
        Program program;
        program.id = index + 1;
        program.name = snake_case(index < move_names.size()
                                      ? std::string_view(move_names[index])
                                      : kExtraAnimationNames[index - move_names.size()]);
        program.begin = *source;
        std::size_t cursor = *source;
        while (cursor < kBankEnd && rom[cursor] != 0xFF) {
            Command command;
            const std::uint8_t opcode = rom[cursor];
            if (opcode >= kFirstSpecialEffect) {
                if (!require_range(rom, cursor, 2, "special-effect command", error)) return false;
                command.special = true;
                command.effect = opcode;
                command.sound = static_cast<std::uint8_t>(rom[cursor + 1] + 1U);
                cursor += 2;
            } else {
                if (!require_range(rom, cursor, 3, "subanimation command", error)) return false;
                command.tile_set = static_cast<std::uint8_t>(opcode >> 6U);
                command.delay = static_cast<std::uint8_t>(opcode & 0x3FU);
                command.sound = static_cast<std::uint8_t>(rom[cursor + 1] + 1U);
                command.subanimation = rom[cursor + 2];
                if (command.tile_set >= 3 || command.subanimation >= kSubanimationCount) {
                    error = "animation command references invalid frame data";
                    return false;
                }
                cursor += 3;
            }
            program.commands.push_back(command);
        }
        if (cursor >= kBankEnd) {
            error = "unterminated animation program";
            return false;
        }
        program.end = cursor + 1;
        programs.push_back(std::move(program));
    }
    return true;
}

bool decode_subanimations(std::span<const std::uint8_t> rom,
                          std::vector<Subanimation>& subanimations, std::string& error) {
    subanimations.reserve(kSubanimationCount);
    for (std::size_t index = 0; index < kSubanimationCount; ++index) {
        const auto source = bank_offset(read_u16(rom, kSubanimationPointers + index * 2U));
        if (!source) {
            error = "subanimation has an invalid bank pointer";
            return false;
        }
        const std::uint8_t header = rom[*source];
        const std::size_t count = header & 0x1FU;
        if (count == 0 ||
            !require_range(rom, *source + 1U, count * 3U, "subanimation frames", error)) {
            if (error.empty()) error = "subanimation has no frames";
            return false;
        }
        Subanimation subanimation;
        subanimation.transform = static_cast<std::uint8_t>(header >> 5U);
        for (std::size_t frame = 0; frame < count; ++frame) {
            const std::size_t offset = *source + 1U + frame * 3U;
            const std::array entry{rom[offset], rom[offset + 1], rom[offset + 2]};
            if (entry[0] >= kFrameBlockCount || entry[1] >= kBaseCoordinateCount ||
                entry[2] >= 5) {
                error = "subanimation references invalid frame content";
                return false;
            }
            subanimation.frames.push_back(entry);
        }
        subanimations.push_back(std::move(subanimation));
    }
    return true;
}

bool decode_frame_blocks(std::span<const std::uint8_t> rom,
                         std::vector<FrameBlock>& frame_blocks, std::string& error) {
    frame_blocks.reserve(kFrameBlockCount);
    for (std::size_t index = 0; index < kFrameBlockCount; ++index) {
        const auto source = bank_offset(read_u16(rom, kFrameBlockPointers + index * 2U));
        if (!source) {
            error = "frame block has an invalid bank pointer";
            return false;
        }
        const std::size_t count = rom[*source];
        if (count > 19 ||
            !require_range(rom, *source + 1U, count * 4U, "frame-block pieces", error)) {
            if (error.empty()) error = "frame block has too many pieces";
            return false;
        }
        FrameBlock block;
        for (std::size_t piece = 0; piece < count; ++piece) {
            const std::size_t offset = *source + 1U + piece * 4U;
            block.push_back({
                .y = static_cast<std::int8_t>(rom[offset]),
                .x = static_cast<std::int8_t>(rom[offset + 1]),
                .tile = rom[offset + 2],
                .attributes = rom[offset + 3],
            });
        }
        frame_blocks.push_back(std::move(block));
    }
    return true;
}

bool decode_coordinates(std::span<const std::uint8_t> rom,
                        std::vector<std::array<std::uint8_t, 2>>& base_coordinates,
                        std::string& error) {
    if (!require_range(rom, kBaseCoordinates, kBaseCoordinateCount * 2U,
                       "base coordinates", error))
        return false;
    for (std::size_t index = 0; index < kBaseCoordinateCount; ++index)
        base_coordinates.push_back(
            {rom[kBaseCoordinates + index * 2U], rom[kBaseCoordinates + index * 2U + 1U]});

    return true;
}

bool decode_procedural_parameters(
    std::span<const std::uint8_t> rom, ProceduralParameters& parameters,
    std::vector<std::array<std::uint8_t, 2>>& spiral_coordinates, std::string& error) {
    constexpr std::array<std::pair<std::size_t, std::uint8_t>, 12> required_opcodes{{
        {kDelayRoutine, 0x0E},
        {kFlashRoutine + 7U, 0x0E},
        {kFlashRoutine + 15U, 0x0E},
        {kBlinkRoutine + 1U, 0x0E},
        {kBlinkRoutine + 7U, 0x0E},
        {kMoveHorizontalRoutine + 23U, 0x0E},
        {kSpiralRoutine + 5U, 0x3E},
        {kSpiralRoutine + 10U, 0x3E},
        {kSpiralRoutine + 24U, 0x16},
        {kSpiralRoutine + 26U, 0x0E},
        {kSpiralRoutine + 65U, 0x0E},
        {kBounceRoutine, 0x0E},
    }};
    for (const auto& [offset, opcode] : required_opcodes) {
        if (!require_range(rom, offset, 2, "procedural routine operand", error)) return false;
        if (rom[offset] != opcode) {
            error = "verified procedural routine does not match the supported semantic profile";
            return false;
        }
    }

    parameters.delay_frames = rom[kDelayRoutine + 1U];
    parameters.flash_delay = rom[kFlashRoutine + 8U];
    if (parameters.flash_delay != rom[kFlashRoutine + 16U]) {
        error = "short flash routine uses asymmetric delays";
        return false;
    }
    parameters.blink_repetitions = rom[kBlinkRoutine + 2U];
    parameters.blink_delay = rom[kBlinkRoutine + 8U];
    if (parameters.blink_delay != rom[kBlinkRoutine + 16U]) {
        error = "blink routine uses asymmetric delays";
        return false;
    }
    parameters.move_horizontal_delay = rom[kMoveHorizontalRoutine + 24U];
    parameters.spiral_enemy_y = static_cast<std::int8_t>(rom[kSpiralRoutine + 6U]);
    parameters.spiral_enemy_x = rom[kSpiralRoutine + 11U];
    const std::uint8_t oam_tile = rom[kSpiralRoutine + 25U];
    if (oam_tile < 0x31) {
        error = "spiral routine references a tile below the imported animation tile base";
        return false;
    }
    parameters.spiral_tile = static_cast<std::uint8_t>(oam_tile - 0x31U);
    parameters.spiral_objects = rom[kSpiralRoutine + 27U];
    parameters.spiral_delay = rom[kSpiralRoutine + 66U];
    parameters.bounce_repetitions = rom[kBounceRoutine + 1U];

    if (rom[kSpiralRoutine + 32U] != 0x21) {
        error = "spiral routine is missing its coordinate-table pointer";
        return false;
    }
    const std::uint16_t pointer = read_u16(rom, kSpiralRoutine + 33U);
    const auto source = bank_offset(pointer);
    if (!source) {
        error = "spiral routine has an invalid coordinate-table pointer";
        return false;
    }
    std::size_t cursor = *source;
    while (cursor < kBankEnd && rom[cursor] != 0xFF) {
        if (!require_range(rom, cursor, 2, "spiral coordinate", error)) return false;
        spiral_coordinates.push_back({rom[cursor], rom[cursor + 1]});
        cursor += 2;
    }
    if (cursor < kBankEnd && spiral_coordinates.size() >= 3) return true;
    error = "spiral coordinate table is invalid";
    return false;
}

std::uint8_t resolved_transform(std::uint8_t authored, bool enemy_turn) {
    if (authored == 5) return enemy_turn ? 0 : 2;
    return enemy_turn ? authored : 0;
}

VisualPiece transformed_piece(const std::array<std::uint8_t, 2>& base,
                              const BlockPiece& piece, std::uint8_t transform,
                              std::uint8_t tile_set) {
    const int base_y = base[0];
    const int base_x = base[1];
    int y = 0;
    int x = 0;
    std::uint8_t attributes = piece.attributes;
    if (transform == 1) {
        y = 136 - (base_y + piece.y);
        x = 168 - (base_x + piece.x);
        attributes ^= 0x60U;
    } else if (transform == 2) {
        y = base_y + piece.y + 40;
        x = 168 - (base_x + piece.x);
        attributes ^= 0x20U;
    } else if (transform == 3) {
        y = 136 - base_y + piece.y;
        x = 168 - base_x + piece.x;
    } else {
        y = base_y + piece.y;
        x = base_x + piece.x;
    }
    return {
        .x = static_cast<std::int16_t>(x - 8),
        .y = static_cast<std::int16_t>(y - 16),
        .tile_set = tile_set,
        .tile = piece.tile,
        .attributes = attributes,
    };
}

void append_line(std::vector<std::string>& lines, std::string line) {
    lines.push_back("    " + std::move(line));
}

std::string three_digits(std::size_t value) {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(3) << value;
    return output.str();
}

std::string hex_byte(std::uint8_t value) {
    std::ostringstream output;
    output << "0x" << std::hex << std::setfill('0') << std::setw(2)
           << static_cast<unsigned>(value);
    return output.str();
}

using InternVisual = std::function<std::string(const Visual&)>;

std::string append_special_effect(
    std::vector<std::string>& lines, std::uint8_t effect, std::uint8_t sound,
    bool enemy_turn, std::string screen_palette, const ProceduralParameters& parameters,
    const std::vector<std::array<std::uint8_t, 2>>& spiral_coordinates,
    const InternVisual& intern_visual, std::set<std::string>& unresolved) {
    const std::string name(kSpecialEffectNames[effect - kFirstSpecialEffect]);
    const std::string actor = enemy_turn ? "defender" : "attacker";
    const int toward_opponent = enemy_turn ? -24 : 24;
    const int offscreen = enemy_turn ? 64 : -64;
    append_line(lines, "; special_effect " + name + " id " + hex_byte(effect) +
                           " sound " + std::to_string(sound));
    if (sound != 0) append_line(lines, "signal imported_sound_" + three_digits(sound));

    if (name == "light_screen_palette") {
        append_line(lines, "set_palette battle_screen light");
        return "light";
    }
    if (name == "dark_screen_palette") {
        append_line(lines, "set_palette battle_screen dark");
        return "dark";
    }
    if (name == "darken_mon_palette") {
        append_line(lines, "set_palette " + actor + " darkened");
        return screen_palette;
    }
    if (name == "reset_screen_palette") {
        append_line(lines, "set_palette battle_screen normal");
        append_line(lines, "set_palette " + actor + " normal");
        return "normal";
    }
    if (name == "dark_screen_flash") {
        append_line(lines, "set_palette battle_screen inverted");
        append_line(lines, "wait " + std::to_string(parameters.flash_delay));
        append_line(lines, "set_palette battle_screen white");
        append_line(lines, "wait " + std::to_string(parameters.flash_delay));
        append_line(lines, "set_palette battle_screen " + screen_palette);
        return screen_palette;
    }
    if (name == "flash_screen_long") {
        // The exact twelve-palette curve remains import data; this clean view uses three phases.
        for (const int delay : {2, 1, 1}) {
            append_line(lines, "set_palette battle_screen dark");
            append_line(lines, "wait " + std::to_string(delay * 4));
            append_line(lines, "set_palette battle_screen white");
            append_line(lines, "wait " + std::to_string(delay * 4));
            append_line(lines, "set_palette battle_screen " + screen_palette);
            append_line(lines, "wait " + std::to_string(delay * 4));
        }
        return screen_palette;
    }
    if (name == "spiral_balls_inward") {
        const int base_y = enemy_turn ? parameters.spiral_enemy_y : 0;
        const int base_x = enemy_turn ? parameters.spiral_enemy_x : 0;
        const std::size_t object_count = parameters.spiral_objects;
        if (spiral_coordinates.size() >= object_count) {
            for (std::size_t frame = 0; frame + object_count <= spiral_coordinates.size();
                 ++frame) {
                Visual pieces;
                for (std::size_t object = 0; object < object_count; ++object) {
                    const auto coordinate = spiral_coordinates[frame + object];
                    pieces.push_back({
                        .x = static_cast<std::int16_t>(
                            static_cast<int>(coordinate[1]) + base_x - 8),
                        .y = static_cast<std::int16_t>(
                            static_cast<int>(coordinate[0]) + base_y - 16),
                        .tile_set = 0,
                        .tile = parameters.spiral_tile,
                        .attributes = 0,
                    });
                }
                append_line(lines, "spawn procedural_frame " + intern_visual(pieces));
                append_line(lines, "set_position procedural_frame 0 0 native_canvas");
                append_line(lines, "wait " + std::to_string(parameters.spiral_delay));
                append_line(lines, "destroy procedural_frame");
            }
        }
        append_line(lines, "set_palette battle_screen inverted");
        append_line(lines, "wait " + std::to_string(parameters.flash_delay));
        append_line(lines, "set_palette battle_screen white");
        append_line(lines, "wait " + std::to_string(parameters.flash_delay));
        append_line(lines, "set_palette battle_screen " + screen_palette);
        return screen_palette;
    }
    if (name == "delay_animation_10") {
        append_line(lines, "wait " + std::to_string(parameters.delay_frames));
    } else if (name == "reset_mon_position") {
        append_line(lines, "set_offset " + actor + " 0 0 native_canvas");
    } else if (name == "move_mon_horizontally") {
        append_line(lines, "tween_offset " + actor + " " + std::to_string(toward_opponent) +
                               " 0 " + std::to_string(parameters.move_horizontal_delay * 2U) +
                               " ease_in native_canvas");
        append_line(lines, "tween_offset " + actor + " 0 0 " +
                               std::to_string(parameters.move_horizontal_delay * 2U) +
                               " ease_out native_canvas");
    } else if (name == "blink_mon" || name == "blink_enemy_mon") {
        const std::string subject = name == "blink_enemy_mon" ? "defender" : actor;
        for (std::size_t repeat = 0; repeat < parameters.blink_repetitions; ++repeat) {
            append_line(lines, "hide " + subject);
            append_line(lines, "wait " + std::to_string(parameters.blink_delay));
            append_line(lines, "show " + subject);
            append_line(lines, "wait " + std::to_string(parameters.blink_delay));
        }
    } else if (name == "flash_mon_pic") {
        append_line(lines, "show " + actor);
    } else if (name == "flash_enemy_mon_pic") {
        append_line(lines, "show defender");
    } else if (name == "bounce_up_and_down") {
        for (std::size_t repeat = 0; repeat < parameters.bounce_repetitions; ++repeat) {
            append_line(lines, "tween_offset " + actor + " 0 -12 5 ease_out native_canvas");
            append_line(lines, "tween_offset " + actor + " 0 0 5 ease_in native_canvas");
        }
    } else if (name == "slide_enemy_mon_off") {
        append_line(lines, "tween_offset defender 64 0 16 linear native_canvas");
        append_line(lines, "hide defender");
    } else if (name == "show_enemy_mon_pic") {
        append_line(lines, "show defender");
    } else if (name == "show_mon_pic") {
        append_line(lines, "show " + actor);
    } else if (name == "hide_enemy_mon_pic") {
        append_line(lines, "hide defender");
    } else if (name == "hide_mon_pic") {
        append_line(lines, "hide " + actor);
    } else if (name == "slide_mon_down_and_hide") {
        append_line(lines, "tween_offset " + actor + " 0 48 12 linear native_canvas");
        append_line(lines, "hide " + actor);
    } else if (name == "slide_mon_half_off") {
        append_line(lines, "tween_offset " + actor + " " + std::to_string(offscreen / 2) +
                               " 0 12 linear native_canvas");
    } else if (name == "slide_mon_off") {
        append_line(lines, "tween_offset " + actor + " " + std::to_string(offscreen) +
                               " 0 16 linear native_canvas");
        append_line(lines, "hide " + actor);
    } else if (name == "slide_mon_down") {
        append_line(lines, "tween_offset " + actor + " 0 16 8 linear native_canvas");
    } else if (name == "slide_mon_up") {
        append_line(lines, "tween_offset " + actor + " 0 -16 8 linear native_canvas");
    } else {
        unresolved.insert(name);
        append_line(lines, "signal special_" + name);
        append_line(lines, "wait 1");
    }
    return screen_palette;
}

using NamedVisual = std::pair<std::string, Visual>;

std::string join_lines(const std::vector<std::string>& lines) {
    std::string text;
    for (const std::string& line : lines) {
        text += line;
        text.push_back('\n');
    }
    return text;
}

void add_text_file(BattleAnimationImport& result, std::string path, std::string text) {
    GeneratedFile file;
    file.relative_path = std::move(path);
    file.bytes.assign(text.begin(), text.end());
    result.files.push_back(std::move(file));
}

bool emit_programs(
    const std::vector<Program>& programs, const std::vector<Subanimation>& subanimations,
    const std::vector<FrameBlock>& frame_blocks,
    const std::vector<std::array<std::uint8_t, 2>>& base_coordinates,
    const ProceduralParameters& parameters,
    const std::vector<std::array<std::uint8_t, 2>>& spiral_coordinates,
    BattleAnimationImport& result, std::vector<NamedVisual>& visuals,
    std::set<std::string>& unresolved, std::string& error) {
    std::map<Visual, std::string> visual_names;
    const InternVisual intern_visual = [&](const Visual& pieces) {
        const auto existing = visual_names.find(pieces);
        if (existing != visual_names.end()) return existing->second;
        std::ostringstream name;
        name << "red_frame_" << std::setfill('0') << std::setw(5) << visuals.size();
        const std::string symbol = name.str();
        visual_names.emplace(pieces, symbol);
        visuals.emplace_back(symbol, pieces);
        return symbol;
    };

    for (const Program& program : programs) {
        const bool enemy_turn = program.name.starts_with("enemy_");
        std::vector<std::string> lines{
            "; Generated locally from Pokemon Red US Rev 0. Do not distribute.",
            "; animation_id " + std::to_string(program.id) + " rom " +
                hex_offset(program.begin) + ".." + hex_offset(program.end),
            std::string("; playback_side ") + (enemy_turn ? "enemy" : "player"),
            "animation " + program.name,
        };
        std::array<std::optional<VisualPiece>, 40> oam{};
        bool active_visual = false;
        std::string screen_palette = "normal";

        for (std::size_t command_index = 0; command_index < program.commands.size();
             ++command_index) {
            const Command& command = program.commands[command_index];
            if (command.special) {
                if (active_visual) {
                    append_line(lines, "destroy imported_frame");
                    active_visual = false;
                }
                screen_palette = append_special_effect(
                    lines, command.effect, command.sound, enemy_turn, screen_palette,
                    parameters, spiral_coordinates, intern_visual, unresolved);
                continue;
            }

            const std::uint8_t tile_set = command.tile_set == 2 ? 0 : command.tile_set;
            const Subanimation& subanimation = subanimations[command.subanimation];
            const std::uint8_t transform =
                resolved_transform(subanimation.transform, enemy_turn);
            append_line(lines, "; command " + std::to_string(command_index) +
                                   " subanimation " +
                                   std::to_string(command.subanimation) + " tileset " +
                                   std::to_string(command.tile_set) + " delay " +
                                   std::to_string(command.delay) + " sound " +
                                   std::to_string(command.sound));
            if (command.sound != 0)
                append_line(lines,
                            "signal imported_sound_" + three_digits(command.sound));

            std::size_t destination = 0;
            for (std::size_t frame_index = 0;
                 frame_index < subanimation.frames.size(); ++frame_index) {
                const auto frame = subanimation.frames[frame_index];
                const FrameBlock& block = frame_blocks[frame[0]];
                const auto& base = base_coordinates[frame[1]];
                const std::uint8_t mode = frame[2];
                if (block.size() > oam.size() - destination) {
                    error = program.name + " writes beyond the 40-entry OAM buffer";
                    return false;
                }
                for (std::size_t piece = 0; piece < block.size(); ++piece)
                    oam[destination + piece] =
                        transformed_piece(base, block[piece], transform, tile_set);

                if (mode != 2) {
                    Visual visible;
                    for (const auto& piece : oam)
                        if (piece) visible.push_back(*piece);
                    const std::string visual = intern_visual(visible);
                    if (active_visual) append_line(lines, "destroy imported_frame");
                    append_line(lines, "; frame " + std::to_string(frame_index) +
                                           " block " + std::to_string(frame[0]) +
                                           " base " + std::to_string(frame[1]) +
                                           " mode " + std::to_string(mode));
                    append_line(lines, "spawn imported_frame " + visual);
                    append_line(lines,
                                "set_position imported_frame 0 0 native_canvas");
                    append_line(lines, "wait " + std::to_string(command.delay));
                    active_visual = true;
                }

                destination += block.size();
                if (mode != 2 && mode != 3 && mode != 4) {
                    oam.fill(std::nullopt);
                    destination = 0;
                } else if (mode == 4) {
                    destination -= block.size();
                }
            }
        }

        if (active_visual) append_line(lines, "destroy imported_frame");
        append_line(lines, "signal animation_finished");
        add_text_file(
            result,
            "source/animations/battle_moves/" + three_digits(program.id) + "_" +
                program.name + ".sexpr",
            join_lines(lines));
    }
    return true;
}

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void append_i16(std::vector<std::uint8_t>& bytes, std::int16_t value) {
    append_u16(bytes, static_cast<std::uint16_t>(value));
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>(value >> 24U));
}

bool emit_frame_assets(std::span<const std::uint8_t> rom,
                       const std::vector<NamedVisual>& visuals,
                       BattleAnimationImport& result, std::string& error) {
    constexpr std::size_t tile_bytes = kTileCount * 16U;
    if (!require_range(rom, kTileSet0, tile_bytes, "battle animation tile set 0",
                       error) ||
        !require_range(rom, kTileSet1, tile_bytes, "battle animation tile set 1",
                       error))
        return false;
    if (visuals.size() > std::numeric_limits<std::uint32_t>::max()) {
        error = "battle animation import contains too many visual frames";
        return false;
    }

    GeneratedFile file;
    file.relative_path = "compiled/battle_animation_frames.bin";
    file.bytes.insert(file.bytes.end(), {'P', 'R', 'A', '1'});
    append_u16(file.bytes, static_cast<std::uint16_t>(kTileCount));
    append_u16(file.bytes, static_cast<std::uint16_t>(kTileCount));
    append_u32(file.bytes, static_cast<std::uint32_t>(visuals.size()));
    file.bytes.insert(file.bytes.end(), rom.begin() + static_cast<std::ptrdiff_t>(kTileSet0),
                      rom.begin() +
                          static_cast<std::ptrdiff_t>(kTileSet0 + tile_bytes));
    file.bytes.insert(file.bytes.end(), rom.begin() + static_cast<std::ptrdiff_t>(kTileSet1),
                      rom.begin() +
                          static_cast<std::ptrdiff_t>(kTileSet1 + tile_bytes));
    for (const auto& [name, pieces] : visuals) {
        if (name.size() > std::numeric_limits<std::uint16_t>::max() ||
            pieces.size() > std::numeric_limits<std::uint16_t>::max()) {
            error = "battle animation visual exceeds the cache format";
            return false;
        }
        append_u16(file.bytes, static_cast<std::uint16_t>(name.size()));
        append_u16(file.bytes, static_cast<std::uint16_t>(pieces.size()));
        file.bytes.insert(file.bytes.end(), name.begin(), name.end());
        for (const VisualPiece& piece : pieces) {
            append_i16(file.bytes, piece.x);
            append_i16(file.bytes, piece.y);
            file.bytes.push_back(piece.tile_set);
            file.bytes.push_back(piece.tile);
            file.bytes.push_back(piece.attributes);
        }
    }
    result.files.push_back(std::move(file));
    return true;
}

} // namespace

bool decode_battle_animation_import(std::span<const std::uint8_t> rom,
                                    BattleAnimationImport& result, std::string& error) {
    result = {};
    error.clear();
    const std::string digest = sha1(rom);
    if (digest != kExpectedSha1) {
        error = "unsupported ROM SHA1 " + digest +
                "; expected Pokemon Red US Rev 0 " + std::string(kExpectedSha1);
        return false;
    }
    if (!require_range(rom, kProgramPointers, kProgramCount * 2U,
                       "animation pointer table", error) ||
        !require_range(rom, kSubanimationPointers, kSubanimationCount * 2U,
                       "subanimation pointer table", error) ||
        !require_range(rom, kFrameBlockPointers, kFrameBlockCount * 2U,
                       "frame-block pointer table", error) ||
        !require_range(rom, kMoveNamesBegin, kMoveNamesEnd - kMoveNamesBegin,
                       "move-name table", error))
        return false;

    std::vector<std::string> move_names;
    std::vector<Program> programs;
    std::vector<Subanimation> subanimations;
    std::vector<FrameBlock> frame_blocks;
    std::vector<std::array<std::uint8_t, 2>> base_coordinates;
    std::vector<std::array<std::uint8_t, 2>> spiral_coordinates;
    ProceduralParameters parameters;
    if (!decode_move_names(rom, move_names, error) ||
        !decode_programs(rom, move_names, programs, error) ||
        !decode_subanimations(rom, subanimations, error) ||
        !decode_frame_blocks(rom, frame_blocks, error) ||
        !decode_coordinates(rom, base_coordinates, error) ||
        !decode_procedural_parameters(rom, parameters, spiral_coordinates, error))
        return false;

    std::vector<NamedVisual> visuals;
    std::set<std::string> unresolved;
    if (!emit_programs(programs, subanimations, frame_blocks, base_coordinates,
                       parameters, spiral_coordinates, result, visuals, unresolved,
                       error) ||
        !emit_frame_assets(rom, visuals, result, error)) {
        result = {};
        return false;
    }

    result.animation_programs = programs.size();
    result.subanimations = subanimations.size();
    result.frame_blocks = frame_blocks.size();
    result.base_coordinates = base_coordinates.size();
    result.visual_frames = visuals.size();
    result.unresolved_effects.assign(unresolved.begin(), unresolved.end());

    std::ostringstream manifest;
    manifest << "profile pokemon_red_us_rev_0\n"
             << "rom_sha1 " << digest << '\n'
             << "importer battle_animations_v2_cpp\n"
             << "animation_programs " << result.animation_programs << '\n'
             << "subanimations " << result.subanimations << '\n'
             << "frame_blocks " << result.frame_blocks << '\n'
             << "visual_frames " << result.visual_frames << "\n";
    add_text_file(result, "import_manifest", manifest.str());

    std::ostringstream report;
    report << "programs: " << result.animation_programs << '\n'
           << "subanimations: " << result.subanimations << '\n'
           << "frame blocks: " << result.frame_blocks << '\n'
           << "base coordinates: " << result.base_coordinates << '\n'
           << "deduplicated visual frames: " << result.visual_frames << '\n'
           << "unresolved procedural effect types: "
           << result.unresolved_effects.size() << '\n';
    for (const std::string& name : result.unresolved_effects)
        report << "  " << name << '\n';
    add_text_file(result, "reports/battle_animation_summary.txt", report.str());
    return true;
}

} // namespace pokered::import
