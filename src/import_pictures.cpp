#include "import_pictures.hpp"

#include "gen1_picture_codec.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <map>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pokered::import {
namespace {

constexpr std::size_t kBaseStatsOffset = 0x383DE;
constexpr std::size_t kBaseStatsSize = 28;
constexpr std::size_t kMewBaseStatsOffset = 0x0425B;
constexpr std::size_t kMonsterNamesOffset = 0x1C21E;
constexpr std::size_t kMonsterNameSize = 10;
constexpr std::size_t kInternalSpeciesCount = 190;
constexpr std::size_t kPokedexOrderOffset = 0x41024;
constexpr std::size_t kTrainerCount = 47;
constexpr std::size_t kTrainerPresentationOffset = 0x39914;
constexpr std::size_t kTrainerNamesOffset = 0x399FF;
constexpr std::size_t kTrainerNamesEnd = 0x39B87;
constexpr std::size_t kTrainerPictureBank = 0x13;
constexpr std::size_t kFontGraphicsOffset = 0x11A80;
constexpr std::size_t kFontTileCount = 128;
constexpr std::size_t kHpStatusGraphicsOffset = 0x11EA0;
constexpr std::size_t kHpStatusTileCount = 30;
constexpr std::size_t kBattleHudTiles1Offset = 0x12080;
constexpr std::size_t kBattleHudTiles2Offset = 0x12098;
constexpr std::size_t kBattleHudTiles3Offset = 0x120B0;
constexpr std::size_t kTextBoxTableOffset = 0x73B0;
constexpr std::size_t kTextBoxRecordSize = 9;
constexpr std::size_t kBattleMenuRecord = 4;
constexpr std::size_t kSafariMenuRecord = 5;
constexpr std::size_t kTextBoxCoordTableOffset = 0x7391;
constexpr std::size_t kTileMapAddress = 0xC3A0;
constexpr std::size_t kScreenWidthTiles = 20;
constexpr std::size_t kScreenHeightTiles = 18;
constexpr std::size_t kTextBoxBorderOffset = 0x1922;
constexpr std::size_t kDrawHpBarOffset = 0x1336;
constexpr std::size_t kPrintLevelOffset = 0x150B;
constexpr std::size_t kPlaceMenuCursorOffset = 0x3B7C;
constexpr std::size_t kPlaceMenuCursorEnd = 0x3BEC;
constexpr std::size_t kPlayerHudOffset = 0x3CD60;
constexpr std::size_t kPlayerHudEnd = 0x3CDEC;
constexpr std::size_t kEnemyHudOffset = 0x3CDEC;
constexpr std::size_t kEnemyHudEnd = 0x3CE97;
constexpr std::size_t kSafariMenuInputOffset = 0x3CF42;
constexpr std::size_t kSafariMenuInputEnd = 0x3CF95;
constexpr std::size_t kPlacePlayerHudOffset = 0x3A902;
constexpr std::size_t kPlacePlayerHudEnd = 0x3A919;
constexpr std::size_t kPlayerHudTilesOffset = 0x3A916;
constexpr std::size_t kPlaceEnemyHudOffset = 0x3A919;
constexpr std::size_t kPlaceEnemyHudEnd = 0x3A930;
constexpr std::size_t kEnemyHudTilesOffset = 0x3A92D;
constexpr std::size_t kPlaceHudTilesOffset = 0x3A930;
constexpr std::size_t kMoveMenuOffset = 0x3D249;
constexpr std::size_t kMoveMenuEnd = 0x3D275;
constexpr std::size_t kMoveInformationOffset = 0x3D4B6;
constexpr std::size_t kMoveInformationEnd = 0x3D555;
constexpr std::size_t kDisabledTextOffset = 0x3D555;
constexpr std::size_t kTypeTextOffset = 0x3D55F;
constexpr std::size_t kFaintedTextRoutineOffset = 0x14E1;
constexpr std::size_t kFaintedTextRoutineEnd = 0x14F6;
constexpr std::size_t kStatusTextRoutineOffset = 0x747DE;
constexpr std::size_t kStatusTextRoutineEnd = 0x7481F;

struct Picture {
    std::uint8_t width{};
    std::uint8_t height{};
    std::uint32_t rom_offset{};
    std::uint32_t compressed_size{};
    std::vector<std::uint8_t> pixels;
};

struct SpeciesPictures {
    std::string name;
    Picture front;
    Picture back;
};

struct TrainerPicture {
    std::string name;
    Picture picture;
};

bool has_range(std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t size) {
    return offset <= bytes.size() && size <= bytes.size() - offset;
}

std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

bool append_character(std::uint8_t value, std::string& text) {
    if (value >= 0x80 && value <= 0x99) {
        text.push_back(static_cast<char>('A' + value - 0x80));
        return true;
    }
    if (value >= 0xA0 && value <= 0xB9) {
        text.push_back(static_cast<char>('a' + value - 0xA0));
        return true;
    }
    if (value >= 0xF6) {
        text.push_back(static_cast<char>('0' + value - 0xF6));
        return true;
    }
    switch (value) {
    case 0x4A:
        text += "PKMN";
        return true;
    case 0x7F:
        text.push_back(' ');
        return true;
    case 0x9A:
        text.push_back('(');
        return true;
    case 0x9B:
        text.push_back(')');
        return true;
    case 0x9C:
        text.push_back(':');
        return true;
    case 0xBA:
        text += "é";
        return true;
    case 0xE0:
        text.push_back('\'');
        return true;
    case 0xE1:
        text += "<PK>";
        return true;
    case 0xE2:
        text += "<MN>";
        return true;
    case 0xE3:
        text.push_back('-');
        return true;
    case 0xE6:
        text.push_back('?');
        return true;
    case 0xE7:
        text.push_back('!');
        return true;
    case 0xE8:
        text.push_back('.');
        return true;
    case 0xEF:
        text += "♂";
        return true;
    case 0xF1:
        text += "×";
        return true;
    case 0xF5:
        text += "♀";
        return true;
    default:
        return false;
    }
}

bool decode_fixed_name(std::span<const std::uint8_t> rom, std::size_t offset, std::string& name,
                       std::string& error) {
    name.clear();
    bool terminated = false;
    for (std::size_t index = 0; index < kMonsterNameSize; ++index) {
        const std::uint8_t value = rom[offset + index];
        if (value == 0x50) {
            terminated = true;
            continue;
        }
        if (terminated || !append_character(value, name)) {
            error = "MonsterNames contains malformed text";
            return false;
        }
    }
    return true;
}

bool decode_terminated_name(std::span<const std::uint8_t> rom, std::size_t& cursor,
                            std::string& name, std::string& error) {
    name.clear();
    while (cursor < kTrainerNamesEnd) {
        const std::uint8_t value = rom[cursor++];
        if (value == 0x50) return true;
        if (!append_character(value, name)) {
            error = "TrainerNames contains an unsupported character";
            return false;
        }
    }
    error = "TrainerNames is missing a terminator";
    return false;
}

std::string source_symbol(std::string_view prefix, std::string_view name, std::size_t ordinal) {
    std::ostringstream output;
    output << prefix << '_' << std::setfill('0') << std::setw(3) << ordinal << '_';
    bool separator = false;
    for (const char raw_character : name) {
        const auto character = static_cast<unsigned char>(raw_character);
        if (std::isalnum(character) != 0) {
            output << static_cast<char>(std::tolower(character));
            separator = false;
        } else if (!separator) {
            output << '_';
            separator = true;
        }
    }
    std::string result = output.str();
    while (!result.empty() && result.back() == '_')
        result.pop_back();
    return result;
}

std::size_t picture_bank(std::uint8_t internal_id) {
    if (internal_id == 0x15) return 1;
    if (internal_id < 0x1F) return 9;
    if (internal_id < 0x4A) return 10;
    if (internal_id < 0x74) return 11;
    if (internal_id < 0x99) return 12;
    return 13;
}

bool pointer_offset(std::size_t bank, std::uint16_t pointer, std::size_t& result,
                    std::string& error) {
    if (pointer < 0x4000 || pointer >= 0x8000) {
        error = "picture pointer is outside $4000..$7fff";
        return false;
    }
    result = bank * 0x4000U + pointer - 0x4000U;
    return true;
}

std::vector<std::uint8_t> unpack_pixels(const DecodedGen1Picture& decoded) {
    const std::size_t width = static_cast<std::size_t>(decoded.width_tiles) * 8U;
    std::vector<std::uint8_t> pixels(width * width);
    for (std::size_t tile_y = 0; tile_y < decoded.height_tiles; ++tile_y) {
        for (std::size_t tile_x = 0; tile_x < decoded.width_tiles; ++tile_x) {
            const std::size_t tile =
                tile_y * static_cast<std::size_t>(decoded.width_tiles) + tile_x;
            for (std::size_t y = 0; y < 8; ++y) {
                const std::uint8_t low = decoded.two_bpp_bytes[tile * 16U + y * 2U];
                const std::uint8_t high = decoded.two_bpp_bytes[tile * 16U + y * 2U + 1U];
                for (std::size_t x = 0; x < 8; ++x) {
                    const unsigned bit = static_cast<unsigned>(7U - x);
                    pixels[(tile_y * 8U + y) * width + tile_x * 8U + x] =
                        static_cast<std::uint8_t>(((high >> bit) & 1U) << 1U | ((low >> bit) & 1U));
                }
            }
        }
    }
    return pixels;
}

bool decode_picture(std::span<const std::uint8_t> rom, std::size_t offset,
                    std::uint8_t expected_width, Picture& result, std::string& error) {
    if (offset >= rom.size()) {
        error = "picture offset extends outside the verified ROM";
        return false;
    }
    DecodedGen1Picture decoded;
    if (!decode_gen1_picture(rom.subspan(offset), decoded, error)) return false;
    if (decoded.width_tiles != expected_width || decoded.height_tiles != expected_width) {
        error = "picture dimensions disagree with their cartridge table";
        return false;
    }
    const std::size_t compressed_size = (decoded.bits_consumed + 7U) / 8U;
    if (compressed_size == 0 || compressed_size > 0xFFFFFFFFU || offset > 0xFFFFFFFFU) {
        error = "picture ROM provenance exceeds the cache format";
        return false;
    }
    result = {
        .width = decoded.width_tiles,
        .height = decoded.height_tiles,
        .rom_offset = static_cast<std::uint32_t>(offset),
        .compressed_size = static_cast<std::uint32_t>(compressed_size),
        .pixels = unpack_pixels(decoded),
    };
    return true;
}

void write_u16(std::vector<std::uint8_t>& output, std::size_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void write_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        output.push_back(static_cast<std::uint8_t>(value >> shift));
}

void write_name(std::vector<std::uint8_t>& output, std::string_view name) {
    write_u16(output, name.size());
    output.insert(output.end(), name.begin(), name.end());
}

void write_picture(std::vector<std::uint8_t>& output, const Picture& picture) {
    output.push_back(picture.width);
    output.push_back(picture.height);
    write_u32(output, picture.rom_offset);
    write_u32(output, picture.compressed_size);
    write_u32(output, static_cast<std::uint32_t>(picture.pixels.size()));
    output.insert(output.end(), picture.pixels.begin(), picture.pixels.end());
}

void add_text_file(PictureImport& result, std::string path, const std::string& text) {
    result.files.push_back({std::move(path), std::vector<std::uint8_t>(text.begin(), text.end())});
}

struct TextBoxRecord {
    std::uint8_t left{};
    std::uint8_t top{};
    std::uint8_t right{};
    std::uint8_t bottom{};
    std::uint16_t text_pointer{};
    std::uint8_t text_x{};
    std::uint8_t text_y{};
};

struct DecodedMenu {
    TextBoxRecord box;
    std::array<std::string, 4> labels;
    std::uint8_t second_column{};
};

struct UiPoint {
    std::uint8_t x{};
    std::uint8_t y{};
};

struct UiBox {
    std::uint8_t left{};
    std::uint8_t top{};
    std::uint8_t right{};
    std::uint8_t bottom{};
};

struct UiTile {
    UiPoint position;
    std::uint8_t tile{};
};

struct ImportedTileStyle {
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

struct ImportedHud {
    UiPoint name;
    UiPoint level;
    UiPoint condition;
    UiPoint hp_bar;
    std::vector<UiTile> frame;
};

struct ImportedMoveMenu {
    UiBox information_box;
    UiBox list_box;
    std::vector<UiTile> joins;
    UiPoint first_move;
    UiPoint cursor;
    UiPoint type_label;
    UiPoint type_slash;
    UiPoint type_value;
    UiPoint pp_slash;
    UiPoint current_pp;
    UiPoint maximum_pp;
    std::string type_text;
    std::string disabled_text;
};

struct ImportedBattleUi {
    ImportedTileStyle tiles;
    ImportedHud enemy;
    ImportedHud player;
    ImportedMoveMenu moves;
    UiBox message;
    std::string fainted_text;
    std::array<std::pair<std::string, std::string>, 5> statuses;
};

bool byte_at(std::span<const std::uint8_t> rom, std::size_t offset, std::uint8_t opcode,
             std::uint8_t& result, std::string& error) {
    if (!has_range(rom, offset, 2) || rom[offset] != opcode) {
        error = "battle UI routine does not match the verified import profile";
        return false;
    }
    result = rom[offset + 1U];
    return true;
}

bool tilemap_point(std::uint16_t address, UiPoint& result) {
    const std::size_t tile_count = kScreenWidthTiles * kScreenHeightTiles;
    if (address < kTileMapAddress ||
        address >= static_cast<std::uint16_t>(kTileMapAddress + tile_count))
        return false;
    const std::size_t index = address - kTileMapAddress;
    result.x = static_cast<std::uint8_t>(index % kScreenWidthTiles);
    result.y = static_cast<std::uint8_t>(index / kScreenWidthTiles);
    return true;
}

bool collect_tilemap_points(std::span<const std::uint8_t> rom, std::size_t begin,
                            std::size_t end, std::vector<UiPoint>& result,
                            std::string& error) {
    if (begin > end || !has_range(rom, begin, end - begin)) {
        error = "battle UI routine extends outside the verified ROM";
        return false;
    }
    result.clear();
    for (std::size_t cursor = begin; cursor + 2U < end; ++cursor) {
        if (rom[cursor] != 0x21) continue;
        UiPoint point;
        if (tilemap_point(read_u16(rom, cursor + 1U), point)) result.push_back(point);
    }
    return true;
}

bool tilemap_operand(std::span<const std::uint8_t> rom, std::size_t begin, std::size_t end,
                     std::size_t wanted, UiPoint& result, std::size_t& instruction,
                     std::string& error) {
    if (begin > end || !has_range(rom, begin, end - begin)) {
        error = "battle UI routine extends outside the verified ROM";
        return false;
    }
    std::size_t index = 0;
    for (std::size_t cursor = begin; cursor + 2U < end; ++cursor) {
        if (rom[cursor] != 0x21) continue;
        UiPoint point;
        if (!tilemap_point(read_u16(rom, cursor + 1U), point)) continue;
        if (index++ != wanted) continue;
        result = point;
        instruction = cursor;
        return true;
    }
    error = "battle UI routine is missing an expected tilemap operand";
    return false;
}

bool stored_tile_after_point(std::span<const std::uint8_t> rom, std::size_t instruction,
                             std::uint8_t& result, std::string& error) {
    if (!has_range(rom, instruction, 5) || rom[instruction] != 0x21 ||
        rom[instruction + 3U] != 0x36) {
        error = "battle UI tile write does not match the verified import profile";
        return false;
    }
    result = rom[instruction + 4U];
    return true;
}

bool find_immediate_store(std::span<const std::uint8_t> rom, std::size_t begin, std::size_t end,
                          std::uint8_t& result, std::string& error) {
    if (begin > end || !has_range(rom, begin, end - begin)) {
        error = "battle UI routine extends outside the verified ROM";
        return false;
    }
    std::optional<std::uint8_t> found;
    for (std::size_t cursor = begin; cursor + 2U < end; ++cursor) {
        if (rom[cursor] != 0x3E || rom[cursor + 2U] != 0x77) continue;
        if (found) {
            error = "battle UI routine has an ambiguous immediate tile";
            return false;
        }
        found = rom[cursor + 1U];
    }
    if (!found) {
        error = "battle UI routine is missing an immediate tile";
        return false;
    }
    result = *found;
    return true;
}

bool decode_inline_three_glyphs(std::span<const std::uint8_t> rom, std::size_t begin,
                                std::size_t end, std::vector<std::string>& result,
                                std::string& error) {
    if (begin > end || !has_range(rom, begin, end - begin)) {
        error = "battle status routine extends outside the verified ROM";
        return false;
    }
    result.clear();
    for (std::size_t cursor = begin; cursor + 7U < end; ++cursor) {
        if (rom[cursor] != 0x3E || rom[cursor + 2U] != 0x22 ||
            rom[cursor + 3U] != 0x3E || rom[cursor + 5U] != 0x22 ||
            rom[cursor + 6U] != 0x36)
            continue;
        std::string text;
        if (!append_character(rom[cursor + 1U], text) ||
            !append_character(rom[cursor + 4U], text) ||
            !append_character(rom[cursor + 7U], text)) {
            error = "battle status routine contains unsupported text";
            return false;
        }
        result.push_back(std::move(text));
        cursor += 7U;
    }
    return true;
}

bool decode_terminated_text(std::span<const std::uint8_t> rom, std::size_t offset,
                            std::string& result, std::string& error) {
    result.clear();
    for (std::size_t cursor = offset; cursor < rom.size() && cursor < offset + 64U; ++cursor) {
        if (rom[cursor] == 0x50) return true;
        if (!append_character(rom[cursor], result)) {
            error = "battle UI text contains an unsupported character";
            return false;
        }
    }
    error = "battle UI text is missing its terminator";
    return false;
}

void append_frame(std::vector<UiTile>& result, const UiPoint& anchor, std::int8_t direction,
                  std::uint8_t top_tile, std::uint8_t corner_tile,
                  std::uint8_t horizontal_tile, std::uint8_t triangle_tile,
                  std::uint8_t horizontal_count) {
    result.push_back({anchor, top_tile});
    UiPoint position{anchor.x, static_cast<std::uint8_t>(anchor.y + 1U)};
    result.push_back({position, corner_tile});
    for (std::size_t index = 0; index < horizontal_count; ++index) {
        position.x = static_cast<std::uint8_t>(
            static_cast<int>(position.x) + static_cast<int>(direction));
        result.push_back({position, horizontal_tile});
    }
    position.x =
        static_cast<std::uint8_t>(static_cast<int>(position.x) + static_cast<int>(direction));
    result.push_back({position, triangle_tile});
}

bool read_text_box_record(std::span<const std::uint8_t> rom, std::size_t index,
                          TextBoxRecord& result, std::string& error) {
    const std::size_t offset = kTextBoxTableOffset + index * kTextBoxRecordSize;
    if (!has_range(rom, offset, kTextBoxRecordSize)) {
        error = "battle menu text-box record extends outside the verified ROM";
        return false;
    }
    result = {
        .left = rom[offset + 1U],
        .top = rom[offset + 2U],
        .right = rom[offset + 3U],
        .bottom = rom[offset + 4U],
        .text_pointer = read_u16(rom, offset + 5U),
        .text_x = rom[offset + 7U],
        .text_y = rom[offset + 8U],
    };
    if (result.left >= result.right || result.top >= result.bottom || result.right >= 20 ||
        result.bottom >= 18 || result.text_pointer < 0x4000 ||
        result.text_pointer >= 0x8000) {
        error = "battle menu text-box record is malformed";
        return false;
    }
    return true;
}

bool decode_menu_lines(std::span<const std::uint8_t> rom, std::uint16_t pointer,
                       std::array<std::vector<std::string>, 2>& result, std::string& error) {
    std::size_t cursor = pointer;
    std::size_t line = 0;
    for (std::size_t count = 0; count < 64 && cursor < rom.size(); ++count) {
        const std::uint8_t value = rom[cursor++];
        if (value == 0x50) return line == 1;
        if (value == 0x4E) {
            if (line != 0) {
                error = "battle menu text contains too many lines";
                return false;
            }
            line = 1;
            continue;
        }
        std::string glyph;
        if (!append_character(value, glyph)) {
            error = "battle menu text contains an unsupported character";
            return false;
        }
        result[line].push_back(std::move(glyph));
    }
    error = "battle menu text is missing its terminator";
    return false;
}

std::string label_between(const std::vector<std::string>& glyphs, std::size_t begin,
                          std::size_t end) {
    while (begin < end && glyphs[begin] == " ")
        ++begin;
    while (end > begin && glyphs[end - 1U] == " ")
        --end;
    std::string result;
    for (std::size_t index = begin; index < end; ++index)
        result += glyphs[index];
    return result;
}

bool decode_menu(std::span<const std::uint8_t> rom, std::size_t record_index,
                 DecodedMenu& result, std::string& error) {
    if (!read_text_box_record(rom, record_index, result.box, error)) return false;
    std::array<std::vector<std::string>, 2> lines;
    if (!decode_menu_lines(rom, result.box.text_pointer, lines, error)) return false;
    if (lines[0].empty() || lines[1].empty()) {
        error = "battle menu text has an empty line";
        return false;
    }

    // Both cartridge menus are two-column grids. The first line establishes
    // the second column; the second line may contain a space within its label.
    std::size_t cursor = 0;
    while (cursor < lines[0].size() && lines[0][cursor] == " ")
        ++cursor;
    while (cursor < lines[0].size() && lines[0][cursor] != " ")
        ++cursor;
    while (cursor < lines[0].size() && lines[0][cursor] == " ")
        ++cursor;
    if (cursor == 0 || cursor >= lines[0].size() || cursor > 255 ||
        lines[1].size() < cursor) {
        error = "battle menu text does not form a two-column grid";
        return false;
    }
    result.second_column = static_cast<std::uint8_t>(cursor);
    result.labels = {
        label_between(lines[0], 0, cursor),
        label_between(lines[0], cursor, lines[0].size()),
        label_between(lines[1], 0, cursor),
        label_between(lines[1], cursor, lines[1].size()),
    };
    if (std::any_of(result.labels.begin(), result.labels.end(),
                    [](const std::string& value) { return value.empty(); })) {
        error = "battle menu text contains an empty command";
        return false;
    }
    return true;
}

bool extract_battle_ui(std::span<const std::uint8_t> rom, ImportedBattleUi& result,
                       std::string& error) {
    ImportedBattleUi imported;

    // Recover the text-box and HP-bar tile vocabulary from the routines that draw them.
    if (!byte_at(rom, kTextBoxBorderOffset + 1U, 0x3E, imported.tiles.top_left, error) ||
        !byte_at(rom, kTextBoxBorderOffset + 0x10U, 0x3E, imported.tiles.vertical, error) ||
        !byte_at(rom, kTextBoxBorderOffset + 0x13U, 0x3E, imported.tiles.blank, error) ||
        !byte_at(rom, kTextBoxBorderOffset + 0x22U, 0x3E, imported.tiles.bottom_left, error) ||
        !byte_at(rom, kTextBoxBorderOffset + 0x25U, 0x3E, imported.tiles.horizontal, error) ||
        !byte_at(rom, kTextBoxBorderOffset + 0x2AU, 0x36, imported.tiles.bottom_right, error) ||
        !byte_at(rom, kDrawHpBarOffset + 3U, 0x3E, imported.tiles.hp_label, error) ||
        !byte_at(rom, kDrawHpBarOffset + 6U, 0x3E, imported.tiles.hp_left, error) ||
        !byte_at(rom, kDrawHpBarOffset + 0x0AU, 0x3E, imported.tiles.hp_empty, error) ||
        !byte_at(rom, kDrawHpBarOffset + 0x14U, 0x3E, imported.tiles.hp_right, error) ||
        !byte_at(rom, kDrawHpBarOffset + 0x2BU, 0x3E, imported.tiles.hp_full, error) ||
        !byte_at(rom, kPrintLevelOffset, 0x3E, imported.tiles.level, error) ||
        !find_immediate_store(rom, kPlaceMenuCursorOffset, kPlaceMenuCursorEnd,
                              imported.tiles.cursor, error))
        return false;
    imported.tiles.top_right = static_cast<std::uint8_t>(imported.tiles.top_left + 2U);
    if (imported.tiles.horizontal != static_cast<std::uint8_t>(imported.tiles.top_left + 1U)) {
        error = "text-box border routine has inconsistent horizontal tiles";
        return false;
    }

    // The HUD routines carry semantic tilemap addresses as ld hl, immediates.
    std::vector<UiPoint> player_points;
    std::vector<UiPoint> enemy_points;
    if (!collect_tilemap_points(rom, kPlayerHudOffset, kPlayerHudEnd, player_points, error) ||
        !collect_tilemap_points(rom, kEnemyHudOffset, kEnemyHudEnd, enemy_points, error))
        return false;
    if (player_points.size() != 5 || enemy_points.size() != 4) {
        error = "battle HUD routines have an unexpected coordinate shape";
        return false;
    }
    imported.player.name = player_points[2];
    imported.player.level = player_points[3];
    imported.player.condition = {
        static_cast<std::uint8_t>(player_points[3].x + 1U),
        player_points[3].y,
    };
    imported.player.hp_bar = player_points[4];
    imported.enemy.name = enemy_points[1];
    imported.enemy.level = enemy_points[2];
    imported.enemy.condition = {
        static_cast<std::uint8_t>(enemy_points[2].x + 1U),
        enemy_points[2].y,
    };
    imported.enemy.hp_bar = enemy_points[3];

    // Re-run the small cartridge HUD construction algorithm against recovered operands.
    std::vector<UiPoint> player_frame_points;
    std::vector<UiPoint> enemy_frame_points;
    if (!collect_tilemap_points(rom, kPlacePlayerHudOffset, kPlacePlayerHudEnd,
                                player_frame_points, error) ||
        !collect_tilemap_points(rom, kPlaceEnemyHudOffset, kPlaceEnemyHudEnd,
                                enemy_frame_points, error))
        return false;
    if (player_frame_points.size() != 1 || enemy_frame_points.size() != 1 ||
        !has_range(rom, kPlayerHudTilesOffset, 3) ||
        !has_range(rom, kEnemyHudTilesOffset, 3) ||
        !has_range(rom, kPlaceHudTilesOffset, 15) ||
        rom[kPlaceHudTilesOffset] != 0x36 || rom[kPlaceHudTilesOffset + 0x0AU] != 0x3E ||
        rom[kPlaceHudTilesOffset + 0x0DU] != 0x36) {
        error = "HUD frame routine does not match the verified import profile";
        return false;
    }
    const std::uint8_t frame_top = rom[kPlaceHudTilesOffset + 1U];
    const std::uint8_t frame_count = rom[kPlaceHudTilesOffset + 0x0BU];
    const std::uint8_t frame_horizontal = rom[kPlaceHudTilesOffset + 0x0EU];
    append_frame(imported.player.frame, player_frame_points[0], -1, frame_top,
                 rom[kPlayerHudTilesOffset + 1U], frame_horizontal,
                 rom[kPlayerHudTilesOffset + 2U], frame_count);
    append_frame(imported.enemy.frame, enemy_frame_points[0], 1, frame_top,
                 rom[kEnemyHudTilesOffset + 1U], frame_horizontal,
                 rom[kEnemyHudTilesOffset + 2U], frame_count);

    std::size_t manual_frame_instruction = 0;
    UiPoint manual_frame_position;
    std::uint8_t manual_frame_tile = 0;
    if (!tilemap_operand(rom, kPlayerHudOffset, kPlayerHudEnd, 1, manual_frame_position,
                         manual_frame_instruction, error) ||
        !stored_tile_after_point(rom, manual_frame_instruction, manual_frame_tile, error))
        return false;
    imported.player.frame.insert(imported.player.frame.begin(),
                                 {manual_frame_position, manual_frame_tile});

    // Recover the move list, its joined information box, and every text anchor.
    std::vector<UiPoint> move_points;
    std::vector<UiPoint> information_points;
    if (!collect_tilemap_points(rom, kMoveMenuOffset, kMoveMenuEnd, move_points, error) ||
        !collect_tilemap_points(rom, kMoveInformationOffset, kMoveInformationEnd,
                                information_points, error))
        return false;
    if (move_points.size() != 4 || information_points.size() != 8) {
        error = "move menu routines have an unexpected coordinate shape";
        return false;
    }
    std::size_t list_instruction = 0;
    UiPoint ignored;
    if (!tilemap_operand(rom, kMoveMenuOffset, kMoveMenuEnd, 0, ignored, list_instruction,
                         error) ||
        !has_range(rom, list_instruction + 3U, 4) || rom[list_instruction + 3U] != 0x06 ||
        rom[list_instruction + 5U] != 0x0E) {
        error = "move list border does not match the verified import profile";
        return false;
    }
    const std::uint8_t list_height = rom[list_instruction + 4U];
    const std::uint8_t list_width = rom[list_instruction + 6U];
    imported.moves.list_box = {
        .left = move_points[0].x,
        .top = move_points[0].y,
        .right = static_cast<std::uint8_t>(move_points[0].x + list_width + 1U),
        .bottom = static_cast<std::uint8_t>(move_points[0].y + list_height + 1U),
    };
    for (std::size_t index : {1U, 2U}) {
        std::size_t instruction = 0;
        UiPoint position;
        std::uint8_t tile = 0;
        if (!tilemap_operand(rom, kMoveMenuOffset, kMoveMenuEnd, index, position, instruction,
                             error) ||
            !stored_tile_after_point(rom, instruction, tile, error))
            return false;
        imported.moves.joins.push_back({position, tile});
    }
    imported.moves.first_move = move_points[3];
    bool found_cursor = false;
    for (std::size_t cursor = list_instruction + 3U; cursor + 3U < kMoveMenuEnd; ++cursor) {
        if (rom[cursor] != 0x06 || rom[cursor + 2U] != 0x3E) continue;
        imported.moves.cursor = {
            rom[cursor + 1U],
            static_cast<std::uint8_t>(rom[cursor + 3U] + 1U),
        };
        found_cursor = true;
    }
    if (!found_cursor) {
        error = "move menu is missing its cursor coordinates";
        return false;
    }

    std::size_t information_instruction = 0;
    if (!tilemap_operand(rom, kMoveInformationOffset, kMoveInformationEnd, 0, ignored,
                         information_instruction, error) ||
        !has_range(rom, information_instruction + 3U, 4) ||
        rom[information_instruction + 3U] != 0x06 ||
        rom[information_instruction + 5U] != 0x0E) {
        error = "move information border does not match the verified import profile";
        return false;
    }
    imported.moves.information_box = {
        .left = information_points[0].x,
        .top = information_points[0].y,
        .right = static_cast<std::uint8_t>(
            information_points[0].x + rom[information_instruction + 6U] + 1U),
        .bottom = static_cast<std::uint8_t>(
            information_points[0].y + rom[information_instruction + 4U] + 1U),
    };
    imported.moves.type_label = information_points[2];
    imported.moves.pp_slash = information_points[3];
    imported.moves.type_slash = information_points[4];
    imported.moves.current_pp = information_points[5];
    imported.moves.maximum_pp = information_points[6];
    imported.moves.type_value = information_points[7];
    if (!decode_terminated_text(rom, kTypeTextOffset, imported.moves.type_text, error) ||
        !decode_terminated_text(rom, kDisabledTextOffset, imported.moves.disabled_text, error))
        return false;

    // Message geometry is table-driven. Inner line placement follows the box convention.
    if (!has_range(rom, kTextBoxCoordTableOffset, 5)) {
        error = "message box record extends outside the verified ROM";
        return false;
    }
    imported.message = {
        .left = rom[kTextBoxCoordTableOffset + 1U],
        .top = rom[kTextBoxCoordTableOffset + 2U],
        .right = rom[kTextBoxCoordTableOffset + 3U],
        .bottom = rom[kTextBoxCoordTableOffset + 4U],
    };

    // Status abbreviations are literal three-glyph writes inside the verified routine.
    std::vector<std::string> fainted;
    std::vector<std::string> statuses;
    if (!decode_inline_three_glyphs(rom, kFaintedTextRoutineOffset,
                                    kFaintedTextRoutineEnd, fainted, error) ||
        !decode_inline_three_glyphs(rom, kStatusTextRoutineOffset,
                                    kStatusTextRoutineEnd, statuses, error))
        return false;
    if (fainted.size() != 1 || statuses.size() != 5) {
        error = "battle status routines have an unexpected text shape";
        return false;
    }
    imported.fainted_text = fainted[0];
    const std::array<std::string_view, 5> status_keys{
        "sleep", "poison", "burn", "freeze", "paralysis",
    };
    for (std::size_t index = 0; index < status_keys.size(); ++index)
        imported.statuses[index] = {std::string(status_keys[index]), std::move(statuses[index])};

    result = std::move(imported);
    return true;
}

void emit_command(std::ostringstream& source, std::string_view key, std::string_view label,
                  std::uint8_t x, std::uint8_t y, std::string_view script,
                  std::optional<std::uint8_t> count_right = std::nullopt) {
    source << "        command " << key << '\n'
           << "            text \"" << label << "\"\n"
           << "            position " << static_cast<unsigned>(x) << ' '
           << static_cast<unsigned>(y) << '\n'
           << "            on_select " << script << '\n';
    if (count_right)
        source << "            count_right " << static_cast<unsigned>(*count_right) << '\n';
}

bool emit_battle_ui_source(std::span<const std::uint8_t> rom, PictureImport& result,
                           std::string& error) {
    DecodedMenu standard;
    DecodedMenu safari;
    ImportedBattleUi ui;
    if (!decode_menu(rom, kBattleMenuRecord, standard, error) ||
        !decode_menu(rom, kSafariMenuRecord, safari, error) ||
        !extract_battle_ui(rom, ui, error))
        return false;

    std::vector<UiPoint> safari_input_points;
    if (!collect_tilemap_points(rom, kSafariMenuInputOffset, kSafariMenuInputEnd,
                                safari_input_points, error))
        return false;
    const auto safari_count = std::find_if(
        safari_input_points.begin(), safari_input_points.end(),
        [&](const UiPoint& point) {
            return point.y == safari.box.text_y && point.x > safari.box.text_x &&
                   point.x < safari.box.text_x + safari.second_column;
        });
    if (safari_count == safari_input_points.end()) {
        error = "Safari menu input routine is missing its ball-count position";
        return false;
    }

    const auto emit_point = [](std::ostringstream& output, std::string_view name,
                               const UiPoint& point) {
        output << "        " << name << ' ' << static_cast<unsigned>(point.x) << ' '
               << static_cast<unsigned>(point.y) << '\n';
    };
    const auto emit_box = [](std::ostringstream& output, std::string_view name,
                             const UiBox& box) {
        output << "        " << name << ' ' << static_cast<unsigned>(box.left) << ' '
               << static_cast<unsigned>(box.top) << ' ' << static_cast<unsigned>(box.right)
               << ' ' << static_cast<unsigned>(box.bottom) << '\n';
    };
    const auto emit_hud = [&](std::ostringstream& output, std::string_view name,
                              const ImportedHud& hud, bool hp_numbers) {
        output << "    " << name << '\n';
        emit_point(output, "name", hud.name);
        emit_point(output, "level", hud.level);
        emit_point(output, "condition", hud.condition);
        emit_point(output, "hp_bar", hud.hp_bar);
        if (hp_numbers) {
            output << "        hp_numbers " << static_cast<unsigned>(hud.hp_bar.x + 3U) << ' '
                   << static_cast<unsigned>(hud.hp_bar.y + 1U) << ' '
                   << static_cast<unsigned>(hud.hp_bar.x + 4U) << ' '
                   << static_cast<unsigned>(hud.hp_bar.y + 1U) << ' '
                   << static_cast<unsigned>(hud.hp_bar.x + 7U) << '\n';
        }
        for (const UiTile& tile : hud.frame)
            output << "        frame_tile " << static_cast<unsigned>(tile.position.x) << ' '
                   << static_cast<unsigned>(tile.position.y) << ' '
                   << static_cast<unsigned>(tile.tile) << '\n';
        output << '\n';
    };

    std::ostringstream source;
    source << "; ROM-derived battle layout and script bindings for the isolated UI lab.\n"
           << "battle_ui red\n"
           << "    tiles " << static_cast<unsigned>(ui.tiles.top_left) << ' '
           << static_cast<unsigned>(ui.tiles.horizontal) << ' '
           << static_cast<unsigned>(ui.tiles.top_right) << ' '
           << static_cast<unsigned>(ui.tiles.vertical) << ' '
           << static_cast<unsigned>(ui.tiles.bottom_left) << ' '
           << static_cast<unsigned>(ui.tiles.bottom_right) << ' '
           << static_cast<unsigned>(ui.tiles.blank) << ' '
           << static_cast<unsigned>(ui.tiles.cursor) << ' '
           << static_cast<unsigned>(ui.tiles.hp_label) << ' '
           << static_cast<unsigned>(ui.tiles.hp_left) << ' '
           << static_cast<unsigned>(ui.tiles.hp_empty) << ' '
           << static_cast<unsigned>(ui.tiles.hp_full) << ' '
           << static_cast<unsigned>(ui.tiles.hp_right) << ' '
           << static_cast<unsigned>(ui.tiles.level) << '\n'
           << "    zero_hp_condition \"" << ui.fainted_text << "\"\n"
           << "    condition_text none \"\"\n";
    for (const auto& [key, text] : ui.statuses)
        source << "    condition_text " << key << " \"" << text << "\"\n";
    source << '\n';
    emit_hud(source, "enemy_hud", ui.enemy, false);
    emit_hud(source, "player_hud", ui.player, true);

    source << "    command_menu standard\n"
           << "        box " << static_cast<unsigned>(standard.box.left) << ' '
           << static_cast<unsigned>(standard.box.top) << ' '
           << static_cast<unsigned>(standard.box.right) << ' '
           << static_cast<unsigned>(standard.box.bottom) << '\n'
           << "        show_player true\n";
    emit_command(source, "fight", standard.labels[0], standard.box.text_x, standard.box.text_y,
                 "battle_choose_move");
    emit_command(source, "party", standard.labels[1],
                 static_cast<std::uint8_t>(standard.box.text_x + standard.second_column),
                 standard.box.text_y, "battle_choose_party");
    emit_command(source, "item", standard.labels[2], standard.box.text_x,
                 static_cast<std::uint8_t>(standard.box.text_y + 2U), "battle_choose_item");
    emit_command(source, "run", standard.labels[3],
                 static_cast<std::uint8_t>(standard.box.text_x + standard.second_column),
                 static_cast<std::uint8_t>(standard.box.text_y + 2U), "battle_attempt_escape");

    source << "\n    command_menu safari\n"
           << "        box " << static_cast<unsigned>(safari.box.left) << ' '
           << static_cast<unsigned>(safari.box.top) << ' '
           << static_cast<unsigned>(safari.box.right) << ' '
           << static_cast<unsigned>(safari.box.bottom) << '\n'
           << "        show_player false\n";
    emit_command(source, "ball", safari.labels[0], safari.box.text_x, safari.box.text_y,
                 "safari_throw_ball",
                 static_cast<std::uint8_t>(safari_count->x + 1U));
    emit_command(source, "bait", safari.labels[1],
                 static_cast<std::uint8_t>(safari.box.text_x + safari.second_column),
                 safari.box.text_y, "safari_throw_bait");
    emit_command(source, "rock", safari.labels[2], safari.box.text_x,
                 static_cast<std::uint8_t>(safari.box.text_y + 2U), "safari_throw_rock");
    emit_command(source, "run", safari.labels[3],
                 static_cast<std::uint8_t>(safari.box.text_x + safari.second_column),
                 static_cast<std::uint8_t>(safari.box.text_y + 2U), "safari_attempt_escape");

    source << "\n    move_menu\n";
    emit_box(source, "information_box", ui.moves.information_box);
    emit_box(source, "list_box", ui.moves.list_box);
    for (const UiTile& tile : ui.moves.joins)
        source << "        join_tile " << static_cast<unsigned>(tile.position.x) << ' '
               << static_cast<unsigned>(tile.position.y) << ' '
               << static_cast<unsigned>(tile.tile) << '\n';
    emit_point(source, "first_move", ui.moves.first_move);
    emit_point(source, "cursor", ui.moves.cursor);
    source << "        type_label " << static_cast<unsigned>(ui.moves.type_label.x) << ' '
           << static_cast<unsigned>(ui.moves.type_label.y) << " \"" << ui.moves.type_text
           << "\"\n";
    emit_point(source, "type_slash", ui.moves.type_slash);
    emit_point(source, "type_value", ui.moves.type_value);
    source << "        pp "
           << static_cast<unsigned>(ui.moves.current_pp.x + 1U) << ' '
           << static_cast<unsigned>(ui.moves.pp_slash.x) << ' '
           << static_cast<unsigned>(ui.moves.pp_slash.y) << ' '
           << static_cast<unsigned>(ui.moves.maximum_pp.x + 1U) << " \""
           << ui.moves.disabled_text << "\"\n\n"
           << "    message_layout\n";
    emit_box(source, "box", ui.message);
    source << "        line " << static_cast<unsigned>(ui.message.left + 1U) << ' '
           << static_cast<unsigned>(ui.message.top + 2U) << '\n'
           << "        line " << static_cast<unsigned>(ui.message.left + 1U) << ' '
           << static_cast<unsigned>(ui.message.top + 4U) << "\n\n"
           << "; Development values are parsed like runtime battle state; the selected\n"
           << "; species name is replaced by the picture browser after loading.\n"
           << "battle_ui_lab_state\n"
           << "    player \"POKEMON\" 25 63 83 none true\n"
           << "    enemy \"POKEMON\" 23 42 61 none true\n"
           << "    move \"MOVE ONE\" \"TYPE\" 14 15 true\n"
           << "    move \"MOVE TWO\" \"TYPE\" 31 35 true\n"
           << "    move \"MOVE THREE\" \"TYPE\" 40 40 true\n"
           << "    move \"MOVE FOUR\" \"TYPE\" 20 20 true\n"
           << "    message \"POKEMON gained\" \"123 EXP!\"\n"
           << "    command_count safari ball 30\n";
    add_text_file(result, "source/ui/battle_views.sexpr", source.str());
    return true;
}

void decode_one_bpp_tiles(std::span<const std::uint8_t> rom, std::size_t offset,
                          std::size_t tile_count, std::size_t first_tile,
                          std::vector<std::uint8_t>& pixels) {
    for (std::size_t tile = 0; tile < tile_count; ++tile) {
        for (std::size_t y = 0; y < 8; ++y) {
            const std::uint8_t row = rom[offset + tile * 8U + y];
            for (std::size_t x = 0; x < 8; ++x) {
                const unsigned bit = static_cast<unsigned>(7U - x);
                pixels[(first_tile + tile) * 64U + y * 8U + x] = ((row >> bit) & 1U) == 0 ? 0 : 3;
            }
        }
    }
}

void decode_two_bpp_tiles(std::span<const std::uint8_t> rom, std::size_t offset,
                          std::size_t tile_count, std::size_t first_tile,
                          std::vector<std::uint8_t>& pixels) {
    for (std::size_t tile = 0; tile < tile_count; ++tile) {
        for (std::size_t y = 0; y < 8; ++y) {
            const std::uint8_t low = rom[offset + tile * 16U + y * 2U];
            const std::uint8_t high = rom[offset + tile * 16U + y * 2U + 1U];
            for (std::size_t x = 0; x < 8; ++x) {
                const unsigned bit = static_cast<unsigned>(7U - x);
                pixels[(first_tile + tile) * 64U + y * 8U + x] =
                    static_cast<std::uint8_t>(((high >> bit) & 1U) << 1U | ((low >> bit) & 1U));
            }
        }
    }
}

bool emit_battle_ui_cache(std::span<const std::uint8_t> rom, PictureImport& result,
                          std::string& error) {
    if (!has_range(rom, kFontGraphicsOffset, kFontTileCount * 8U) ||
        !has_range(rom, kHpStatusGraphicsOffset, kHpStatusTileCount * 16U) ||
        !has_range(rom, kBattleHudTiles1Offset, 3U * 8U) ||
        !has_range(rom, kBattleHudTiles2Offset, 3U * 8U) ||
        !has_range(rom, kBattleHudTiles3Offset, 3U * 8U)) {
        error = "battle UI graphics extend outside the verified ROM";
        return false;
    }

    // Build the final battle VRAM view after the HUD-specific tile overwrites.
    constexpr std::size_t tile_count = 256;
    std::vector<std::uint8_t> pixels(tile_count * 64U);
    decode_one_bpp_tiles(rom, kFontGraphicsOffset, kFontTileCount, 0x80, pixels);
    decode_two_bpp_tiles(rom, kHpStatusGraphicsOffset, kHpStatusTileCount, 0x62, pixels);
    decode_one_bpp_tiles(rom, kBattleHudTiles1Offset, 3, 0x6D, pixels);
    decode_one_bpp_tiles(rom, kBattleHudTiles2Offset, 3, 0x73, pixels);
    decode_one_bpp_tiles(rom, kBattleHudTiles3Offset, 3, 0x76, pixels);

    std::vector<std::uint8_t> cache{'P', 'U', 'I', '1'};
    write_u16(cache, tile_count);
    cache.insert(cache.end(), pixels.begin(), pixels.end());
    result.files.push_back({"compiled/battle_ui_tiles.bin", std::move(cache)});

    std::ostringstream source;
    source << "; Final battle tile set after Red's load-time HUD overwrites.\n"
           << "battle_ui_tiles red_battle_ui\n"
           << "    font \"0x11A80\" 128 one_bpp\n"
           << "    hp_and_status \"0x11EA0\" 30 two_bpp\n"
           << "    hud_override_1 \"0x12080\" 3 one_bpp\n"
           << "    hud_override_2 \"0x12098\" 3 one_bpp\n"
           << "    hud_override_3 \"0x120B0\" 3 one_bpp\n";
    add_text_file(result, "source/graphics/battle_ui_visuals.sexpr", source.str());
    result.battle_ui_tiles = tile_count;
    return emit_battle_ui_source(rom, result, error);
}

} // namespace

bool decode_picture_import(std::span<const std::uint8_t> rom, PictureImport& result,
                           std::string& error) {
    result = {};
    if (!verify_pokemon_red_us_rev_0(rom, error)) return false;
    if (!has_range(rom, kBaseStatsOffset, 150U * kBaseStatsSize) ||
        !has_range(rom, kMewBaseStatsOffset, kBaseStatsSize) ||
        !has_range(rom, kMonsterNamesOffset, kInternalSpeciesCount * kMonsterNameSize) ||
        !has_range(rom, kPokedexOrderOffset, kInternalSpeciesCount) ||
        !has_range(rom, kTrainerPresentationOffset, kTrainerCount * 5U) ||
        !has_range(rom, kTrainerNamesOffset, kTrainerNamesEnd - kTrainerNamesOffset)) {
        error = "picture import tables extend outside the verified ROM";
        return false;
    }

    std::array<std::uint8_t, 151> internal_ids{};
    std::array<std::string, 151> species_names;
    // Join internal picture-bank IDs to stable Pokédex order and display names.
    for (std::size_t index = 0; index < kInternalSpeciesCount; ++index) {
        const std::uint8_t dex = rom[kPokedexOrderOffset + index];
        if (dex == 0) continue;
        if (dex > internal_ids.size() || internal_ids[dex - 1U] != 0) {
            error = "PokedexOrder does not uniquely map all species";
            return false;
        }
        internal_ids[dex - 1U] = static_cast<std::uint8_t>(index + 1U);
        if (!decode_fixed_name(rom, kMonsterNamesOffset + index * kMonsterNameSize,
                               species_names[dex - 1U], error))
            return false;
    }

    std::vector<SpeciesPictures> species;
    species.reserve(151);
    std::ostringstream species_source;
    species_source << "; ROM-derived display bindings. Pixel data remains in compiled cache.\n";
    // Decode both battle views once and retain only normalized shade pixels at runtime.
    for (std::size_t dex_index = 0; dex_index < 151; ++dex_index) {
        if (internal_ids[dex_index] == 0 || species_names[dex_index].empty()) {
            error = "PokedexOrder does not resolve every species picture";
            return false;
        }
        const std::size_t stats =
            dex_index == 150 ? kMewBaseStatsOffset : kBaseStatsOffset + dex_index * kBaseStatsSize;
        if (rom[stats] != dex_index + 1U) {
            error = "BaseStats is not in Pokédex order";
            return false;
        }
        const std::uint8_t dimensions = rom[stats + 10U];
        const std::uint8_t width = static_cast<std::uint8_t>(dimensions >> 4U);
        const std::uint8_t height = static_cast<std::uint8_t>(dimensions & 0x0FU);
        if (width == 0 || width != height) {
            error = "BaseStats contains invalid front-picture dimensions";
            return false;
        }
        const std::size_t bank = picture_bank(internal_ids[dex_index]);
        std::size_t front_offset = 0;
        std::size_t back_offset = 0;
        if (!pointer_offset(bank, read_u16(rom, stats + 11U), front_offset, error) ||
            !pointer_offset(bank, read_u16(rom, stats + 13U), back_offset, error))
            return false;

        SpeciesPictures pictures;
        pictures.name = species_names[dex_index];
        if (!decode_picture(rom, front_offset, width, pictures.front, error) ||
            !decode_picture(rom, back_offset, 4, pictures.back, error)) {
            error = pictures.name + ": " + error;
            return false;
        }
        const std::string symbol = source_symbol("pokemon", pictures.name, dex_index + 1U);
        species_source << "pokemon_visual " << symbol << '\n'
                       << "    display_name \"" << pictures.name << "\"\n"
                       << "    front " << static_cast<unsigned>(pictures.front.width) << ' '
                       << static_cast<unsigned>(pictures.front.height) << " \"0x" << std::hex
                       << std::uppercase << pictures.front.rom_offset << std::dec << "\"\n"
                       << "    back " << static_cast<unsigned>(pictures.back.width) << ' '
                       << static_cast<unsigned>(pictures.back.height) << " \"0x" << std::hex
                       << std::uppercase << pictures.back.rom_offset << std::dec << "\"\n\n";
        species.push_back(std::move(pictures));
    }

    std::vector<TrainerPicture> trainers;
    trainers.reserve(kTrainerCount);
    std::map<std::size_t, Picture> decoded_trainers;
    std::size_t name_cursor = kTrainerNamesOffset;
    std::ostringstream trainer_source;
    trainer_source << "; Trainer-class bindings may share one ROM portrait.\n";
    // Preserve all class bindings while decoding shared portrait offsets once.
    for (std::size_t index = 0; index < kTrainerCount; ++index) {
        TrainerPicture trainer;
        if (!decode_terminated_name(rom, name_cursor, trainer.name, error)) return false;
        std::size_t picture_offset = 0;
        if (!pointer_offset(kTrainerPictureBank,
                            read_u16(rom, kTrainerPresentationOffset + index * 5U), picture_offset,
                            error))
            return false;
        const auto found = decoded_trainers.find(picture_offset);
        if (found == decoded_trainers.end()) {
            Picture picture;
            if (!decode_picture(rom, picture_offset, 7, picture, error)) {
                error = trainer.name + ": " + error;
                return false;
            }
            trainer.picture = picture;
            decoded_trainers.emplace(picture_offset, std::move(picture));
        } else {
            trainer.picture = found->second;
        }
        trainer_source << "trainer_visual " << source_symbol("trainer", trainer.name, index + 1U)
                       << '\n'
                       << "    display_name \"" << trainer.name << "\"\n"
                       << "    portrait 7 7 \"0x" << std::hex << std::uppercase
                       << trainer.picture.rom_offset << std::dec << "\"\n\n";
        trainers.push_back(std::move(trainer));
    }
    if (name_cursor != kTrainerNamesEnd) {
        error = "TrainerNames did not consume its exact ROM range";
        return false;
    }

    // Write one compact, host-independent cache for direct upload by a renderer.
    std::vector<std::uint8_t> cache{'P', 'G', 'P', '1'};
    write_u16(cache, species.size());
    write_u16(cache, trainers.size());
    for (const SpeciesPictures& pictures : species) {
        write_name(cache, pictures.name);
        write_picture(cache, pictures.front);
        write_picture(cache, pictures.back);
    }
    for (const TrainerPicture& trainer : trainers) {
        write_name(cache, trainer.name);
        write_picture(cache, trainer.picture);
    }
    result.files.push_back({"compiled/battle_pictures.bin", std::move(cache)});
    add_text_file(result, "source/graphics/pokemon_visuals.sexpr", species_source.str());
    add_text_file(result, "source/graphics/trainer_visuals.sexpr", trainer_source.str());

    if (!emit_battle_ui_cache(rom, result, error)) return false;

    std::ostringstream report;
    report << "Pokemon Red US Rev 0 battle-picture import\n"
           << "species " << species.size() << '\n'
           << "front_pictures " << species.size() << '\n'
           << "back_pictures " << species.size() << '\n'
           << "trainer_classes " << trainers.size() << '\n'
           << "unique_trainer_portraits " << decoded_trainers.size() << '\n'
           << "battle_ui_tiles " << result.battle_ui_tiles << '\n'
           << "runtime_cache compiled/battle_pictures.bin\n";
    add_text_file(result, "reports/picture_import_summary.txt", report.str());

    result.species = species.size();
    result.front_pictures = species.size();
    result.back_pictures = species.size();
    result.trainer_classes = trainers.size();
    error.clear();
    return true;
}

} // namespace pokered::import
