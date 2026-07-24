#include "import_maps.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pokered::import {
namespace {

constexpr std::size_t kBankSize = 0x4000;
constexpr std::size_t kMapSlotCount = 248;
constexpr std::size_t kActiveMapCount = 226;
constexpr std::size_t kMapHeaderPointersOffset = 0x001AE;
constexpr std::size_t kMapHeaderBanksOffset = 0x0C23D;
constexpr std::size_t kTilesetHeadersOffset = 0x0C7BE;
constexpr std::size_t kTilesetHeaderSize = 12;
constexpr std::size_t kTilesPerBlock = 16;
constexpr std::size_t kTownMapExternalEntriesOffset = 0x71313;
constexpr std::size_t kTownMapExternalEntryCount = 37;
constexpr std::size_t kTownMapExternalEntrySize = 3;
constexpr std::uint8_t kTownMapBank = 0x1C;
constexpr std::size_t kFlowerAnimationFramesOffset = 0x1F19;
constexpr std::size_t kLedgesOffset = 0x1A6CF;
constexpr std::size_t kLedgesEnd = 0x1A6F0;
constexpr std::size_t kAnimationTileBytes = 16;
constexpr std::size_t kWaterAnimationFrameCount = 8;
constexpr std::size_t kFlowerAnimationFrameCount = 3;
constexpr std::size_t kOverworldSpriteTableOffset = 0x17B27;
constexpr std::size_t kOverworldSpriteCount = 72;
constexpr std::size_t kOverworldSpriteEntrySize = 4;
constexpr std::size_t kOverworldFacingTableOffset = 0x04000;
constexpr std::size_t kOverworldFacingEntrySize = 4;
constexpr std::size_t kOverworldFacingCount = 32;
constexpr std::size_t kOverworldTileSequenceBegin = 0x04080;
constexpr std::size_t kOverworldTileSequenceEnd = 0x04098;
constexpr std::size_t kOverworldNormalOam = 0x04098;
constexpr std::size_t kOverworldFlippedOam = 0x040A4;
constexpr std::array<std::string_view, kOverworldSpriteCount> kOverworldSpriteKeys{{
    "red",
    "blue",
    "oak",
    "youngster",
    "monster",
    "cooltrainer_f",
    "cooltrainer_m",
    "little_girl",
    "bird",
    "middle_aged_man",
    "gambler",
    "super_nerd",
    "girl",
    "hiker",
    "beauty",
    "gentleman",
    "daisy",
    "biker",
    "sailor",
    "cook",
    "bike_shop_clerk",
    "mr_fuji",
    "giovanni",
    "rocket",
    "channeler",
    "waiter",
    "silph_worker_f",
    "middle_aged_woman",
    "brunette_girl",
    "lance",
    "unused_scientist",
    "scientist",
    "rocker",
    "swimmer",
    "safari_zone_worker",
    "gym_guide",
    "gramps",
    "clerk",
    "fishing_guru",
    "granny",
    "nurse",
    "link_receptionist",
    "silph_president",
    "silph_worker_m",
    "warden",
    "captain",
    "fisher",
    "koga",
    "guard",
    "unused_guard",
    "mom",
    "balding_guy",
    "little_boy",
    "unused_gameboy_kid",
    "gameboy_kid",
    "fairy",
    "agatha",
    "bruno",
    "lorelei",
    "seel",
    "poke_ball",
    "fossil",
    "boulder",
    "paper",
    "pokedex",
    "clipboard",
    "snorlax",
    "unused_old_amber",
    "old_amber",
    "unused_gambler_asleep_1",
    "unused_gambler_asleep_2",
    "gambler_asleep",
}};

struct MapProfile {
    const char* name;
    std::uint8_t width;
    std::uint8_t height;
};

constexpr std::array<MapProfile, kMapSlotCount> kMapProfiles{{
#include "pokemon_red_map_profile.inc"
}};

struct MapIdentity {
    std::uint8_t id{};
    std::string key;
    std::string display_name;
    std::uint8_t town_x{};
    std::uint8_t town_y{};
    std::uint8_t expected_width{};
    std::uint8_t expected_height{};
    bool surface{};
};

struct ImportedConnection {
    std::uint8_t direction{};
    std::uint8_t target_map_id{};
    std::int16_t player_x{};
    std::int16_t player_y{};
};

struct ImportedLedge {
    std::uint8_t direction{};
    std::uint8_t standing_tile{};
    std::uint8_t ledge_tile{};
    std::uint8_t required_input_mask{};
};

enum class ImportedCameraFraming : std::uint8_t {
    fixed_zoom,
    fit_map,
    fit_width,
    fit_height,
    fit_space,
};

struct ImportedMap {
    MapIdentity identity;
    std::uint8_t header_bank{};
    std::uint8_t tileset_id{};
    std::uint8_t width_blocks{};
    std::uint8_t height_blocks{};
    std::uint16_t blocks_pointer{};
    std::size_t header_offset{};
    std::size_t blocks_offset{};
    std::size_t objects_offset{};
    std::int32_t global_x_cells{};
    std::int32_t global_y_cells{};
    std::uint16_t component{};
    ImportedCameraFraming camera_framing{
        ImportedCameraFraming::fixed_zoom};
    float camera_zoom{2.0F};
    bool placed{};
    std::vector<std::uint8_t> blocks;
    std::vector<std::uint8_t> tiles;
    std::vector<ImportedConnection> connections;
    struct Warp {
        std::uint8_t index{};
        std::uint8_t x{};
        std::uint8_t y{};
        std::uint8_t destination_map_id{};
        std::uint8_t destination_warp_index{};
    };
    struct Actor {
        std::uint8_t index{};
        std::uint8_t sprite_id{};
        std::uint8_t x{};
        std::uint8_t y{};
        std::uint8_t movement{};
        std::uint8_t direction_or_axis{};
        std::uint8_t text_id{};
        std::uint8_t parameter_a{};
        std::uint8_t parameter_b{};
        std::uint8_t kind{};
    };
    std::vector<Warp> warps;
    std::vector<Actor> actors;
};

struct ImportedWorldSpace {
    std::uint16_t id{};
    std::string key;
    bool outdoor{};
};

struct ImportedTileset {
    std::uint8_t id{};
    std::uint8_t bank{};
    std::uint16_t blocks_pointer{};
    std::uint16_t graphics_pointer{};
    std::uint16_t collision_pointer{};
    std::size_t header_offset{};
    std::size_t blocks_offset{};
    std::size_t graphics_offset{};
    std::size_t collision_offset{};
    std::size_t block_count{};
    std::uint8_t grass_tile{};
    std::uint8_t animation_mode{};
    std::vector<std::uint8_t> passable_tiles;
    std::vector<std::uint8_t> block_tiles;
    std::vector<std::uint8_t> pixels;
    std::vector<std::uint8_t> animation_pixels;
};

struct ImportedSprite {
    std::uint8_t id{};
    std::string key;
    bool still{};
    std::size_t table_offset{};
    std::size_t graphics_offset{};
    std::vector<std::uint8_t> pixels;
};

bool has_range(std::span<const std::uint8_t> rom, std::size_t offset, std::size_t size) {
    return offset <= rom.size() && size <= rom.size() - offset;
}

std::uint16_t read_u16(std::span<const std::uint8_t> rom, std::size_t offset) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(rom[offset]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(rom[offset + 1U]) << 8U));
}

bool bank_pointer_to_offset(std::span<const std::uint8_t> rom, std::uint8_t bank,
                            std::uint16_t pointer, std::size_t& result) {
    if (bank == 0) {
        if (pointer >= 0x4000) return false;
        result = pointer;
    } else {
        if (pointer < 0x4000 || pointer >= 0x8000) return false;
        result = static_cast<std::size_t>(bank) * kBankSize +
                 static_cast<std::size_t>(pointer - 0x4000U);
    }
    return result < rom.size();
}

bool visible_pointer_to_offset(std::span<const std::uint8_t> rom, std::uint8_t bank,
                               std::uint16_t pointer, std::size_t& result) {
    if (pointer < 0x4000U) {
        result = pointer;
        return result < rom.size();
    }
    return bank_pointer_to_offset(rom, bank, pointer, result);
}

void write_u16(std::vector<std::uint8_t>& bytes, std::size_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void write_u32(std::vector<std::uint8_t>& bytes, std::size_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void write_i32(std::vector<std::uint8_t>& bytes, std::int32_t value) {
    const auto bits = static_cast<std::uint32_t>(value);
    write_u32(bytes, bits);
}

void add_text_file(MapImport& result, std::string path, std::string text) {
    GeneratedFile file;
    file.relative_path = std::move(path);
    file.bytes.assign(text.begin(), text.end());
    result.files.push_back(std::move(file));
}

bool append_map_name_character(std::uint8_t value, std::string& text) {
    if (value >= 0x80 && value <= 0x99) {
        text.push_back(static_cast<char>('A' + value - 0x80));
        return true;
    }
    if (value >= 0xF6) {
        text.push_back(static_cast<char>('0' + value - 0xF6));
        return true;
    }
    if (value == 0x7F) {
        text.push_back(' ');
        return true;
    }
    if (value == 0xE3) {
        text.push_back('-');
        return true;
    }
    if (value == 0xE8) {
        text.push_back('.');
        return true;
    }
    return false;
}

bool decode_map_name(std::span<const std::uint8_t> rom, std::uint16_t pointer, std::string& result,
                     std::string& error) {
    std::size_t cursor = 0;
    if (!bank_pointer_to_offset(rom, kTownMapBank, pointer, cursor)) {
        error = "Town Map name pointer is outside its declared bank";
        return false;
    }
    result.clear();
    for (std::size_t count = 0; count < 32 && cursor < rom.size(); ++count) {
        const std::uint8_t value = rom[cursor++];
        if (value == 0x50) return !result.empty();
        if (!append_map_name_character(value, result)) {
            error = "Town Map name contains an unsupported character";
            return false;
        }
    }
    error = "Town Map name has no bounded terminator";
    return false;
}

std::string make_map_key(std::string_view display_name, std::uint8_t id) {
    std::string result;
    for (const char character : display_name) {
        if (character >= 'A' && character <= 'Z') {
            result.push_back(static_cast<char>('a' + character - 'A'));
        } else if (character >= '0' && character <= '9') {
            result.push_back(character);
        } else if (!result.empty() && result.back() != '_') {
            result.push_back('_');
        }
    }
    while (!result.empty() && result.back() == '_')
        result.pop_back();
    if (!result.empty()) return result;
    return "map_" + std::to_string(static_cast<unsigned>(id));
}

std::string profile_display_name(std::string_view name) {
    std::string result;
    result.reserve(name.size());
    bool capitalize = true;
    for (const char character : name) {
        if (character == '_') {
            result.push_back(' ');
            capitalize = true;
        } else {
            const char lower =
                character >= 'A' && character <= 'Z'
                    ? static_cast<char>('a' + character - 'A')
                    : character;
            result.push_back(capitalize && lower >= 'a' && lower <= 'z'
                                 ? static_cast<char>('A' + lower - 'a')
                                 : lower);
            capitalize = false;
        }
    }
    return result;
}

bool discover_maps(std::span<const std::uint8_t> rom, std::vector<MapIdentity>& result,
                   std::string& error) {
    const std::size_t table_size = kTownMapExternalEntryCount * kTownMapExternalEntrySize;
    if (!has_range(rom, kTownMapExternalEntriesOffset, table_size)) {
        error = "Town Map external-entry table extends outside the verified ROM";
        return false;
    }
    std::array<std::string, kTownMapExternalEntryCount> outdoor_names;
    std::array<std::uint8_t, kTownMapExternalEntryCount> town_x{};
    std::array<std::uint8_t, kTownMapExternalEntryCount> town_y{};
    for (std::size_t index = 0; index < kTownMapExternalEntryCount; ++index) {
        const std::size_t offset =
            kTownMapExternalEntriesOffset + index * kTownMapExternalEntrySize;
        const std::uint8_t packed = rom[offset];
        if (packed == 0) continue;
        town_x[index] = static_cast<std::uint8_t>(packed & 0x0FU);
        town_y[index] = static_cast<std::uint8_t>(packed >> 4U);
        if (!decode_map_name(rom, read_u16(rom, offset + 1U), outdoor_names[index], error))
            return false;
    }

    result.reserve(kActiveMapCount);
    for (std::size_t index = 0; index < kMapProfiles.size(); ++index) {
        const MapProfile& profile = kMapProfiles[index];
        if (profile.width == 0 || profile.height == 0) continue;
        MapIdentity identity{
            .id = static_cast<std::uint8_t>(index),
            .key = make_map_key(profile.name, static_cast<std::uint8_t>(index)),
            .display_name = profile_display_name(profile.name),
            .town_x = index < town_x.size() ? town_x[index] : std::uint8_t{0},
            .town_y = index < town_y.size() ? town_y[index] : std::uint8_t{0},
            .expected_width = profile.width,
            .expected_height = profile.height,
            .surface = index < kTownMapExternalEntryCount,
        };
        if (index < outdoor_names.size() && !outdoor_names[index].empty())
            identity.display_name = outdoor_names[index];
        result.push_back(std::move(identity));
    }
    if (result.size() != kActiveMapCount) {
        error = "map profile does not classify exactly 226 active slots";
        return false;
    }
    return true;
}

bool coordinate_in_map(const ImportedMap& map, std::uint8_t x, std::uint8_t y) {
    return x < static_cast<std::uint8_t>(map.width_blocks * 2U) &&
           y < static_cast<std::uint8_t>(map.height_blocks * 2U);
}

bool decode_map_objects(std::span<const std::uint8_t> rom, std::uint16_t pointer, ImportedMap& map,
                        std::string& error) {
    std::size_t cursor = 0;
    if (!bank_pointer_to_offset(rom, map.header_bank, pointer, cursor) ||
        !has_range(rom, cursor, 2)) {
        error = "map object pointer is outside its declared bank";
        return false;
    }
    map.objects_offset = cursor;
    ++cursor; // border block

    const std::uint8_t warp_count = rom[cursor++];
    if (warp_count > 32 || !has_range(rom, cursor, static_cast<std::size_t>(warp_count) * 4U)) {
        error = "map object data has an invalid warp table";
        return false;
    }
    map.warps.reserve(warp_count);
    for (std::uint8_t index = 0; index < warp_count; ++index) {
        ImportedMap::Warp warp{
            .index = static_cast<std::uint8_t>(index + 1U),
            .x = rom[cursor + 1U],
            .y = rom[cursor],
            .destination_map_id = rom[cursor + 3U],
            .destination_warp_index = rom[cursor + 2U],
        };
        if (!coordinate_in_map(map, warp.x, warp.y)) {
            error = "map warp lies outside its map";
            return false;
        }
        map.warps.push_back(warp);
        cursor += 4U;
    }

    if (!has_range(rom, cursor, 1)) {
        error = "map object data is missing its background-event count";
        return false;
    }
    const std::uint8_t background_count = rom[cursor++];
    const std::size_t background_bytes = static_cast<std::size_t>(background_count) * 3U;
    if (background_count > 16 || !has_range(rom, cursor, background_bytes + 1U)) {
        error = "map object data has an invalid background-event table";
        return false;
    }
    cursor += background_bytes;

    const std::uint8_t actor_count = rom[cursor++];
    if (actor_count > 16) {
        error = "map object count exceeds Red's authored limit";
        return false;
    }
    map.actors.reserve(actor_count);
    for (std::uint8_t index = 0; index < actor_count; ++index) {
        if (!has_range(rom, cursor, 6)) {
            error = "map actor record crosses the verified ROM";
            return false;
        }
        ImportedMap::Actor actor;
        actor.index = static_cast<std::uint8_t>(index + 1U);
        actor.sprite_id = rom[cursor++];
        const std::uint8_t stored_y = rom[cursor++];
        const std::uint8_t stored_x = rom[cursor++];
        actor.movement = rom[cursor++];
        actor.direction_or_axis = rom[cursor++];
        const std::uint8_t encoded_text = rom[cursor++];
        if (stored_x < 4 || stored_y < 4) {
            error = "map actor has an invalid biased coordinate";
            return false;
        }
        actor.x = static_cast<std::uint8_t>(stored_x - 4U);
        actor.y = static_cast<std::uint8_t>(stored_y - 4U);
        if (!coordinate_in_map(map, actor.x, actor.y) || actor.sprite_id == 0 ||
            actor.sprite_id > kOverworldSpriteCount) {
            error = "map actor has an invalid coordinate or sprite";
            return false;
        }
        actor.text_id = static_cast<std::uint8_t>(encoded_text & 0x3FU);
        const std::uint8_t kind = static_cast<std::uint8_t>(encoded_text & 0xC0U);
        if (kind == 0x40U) {
            actor.kind = 1;
            if (!has_range(rom, cursor, 2)) {
                error = "trainer or Pokemon actor is missing parameters";
                return false;
            }
            actor.parameter_a = rom[cursor++];
            actor.parameter_b = rom[cursor++];
        } else if (kind == 0x80U) {
            actor.kind = 2;
            if (!has_range(rom, cursor, 1)) {
                error = "item actor is missing its item ID";
                return false;
            }
            actor.parameter_a = rom[cursor++];
        } else if (kind != 0) {
            error = "map actor combines trainer and item flags";
            return false;
        }
        map.actors.push_back(actor);
    }

    const std::size_t destination_bytes = static_cast<std::size_t>(warp_count) * 4U;
    if (!has_range(rom, cursor, destination_bytes)) {
        error = "map warp destination table crosses the verified ROM";
        return false;
    }
    return true;
}

bool decode_map(std::span<const std::uint8_t> rom, MapIdentity identity, ImportedMap& result,
                std::string& error) {
    const std::size_t pointer_record =
        kMapHeaderPointersOffset + static_cast<std::size_t>(identity.id) * 2U;
    const std::size_t bank_record = kMapHeaderBanksOffset + static_cast<std::size_t>(identity.id);
    if (!has_range(rom, pointer_record, 2) || !has_range(rom, bank_record, 1)) {
        error = "map lookup record extends outside the verified ROM";
        return false;
    }

    ImportedMap map;
    map.identity = std::move(identity);
    map.header_bank = rom[bank_record];
    const std::uint16_t header_pointer = read_u16(rom, pointer_record);
    if (!bank_pointer_to_offset(rom, map.header_bank, header_pointer, map.header_offset) ||
        !has_range(rom, map.header_offset, 10)) {
        error = "map header is outside its declared bank";
        return false;
    }

    map.tileset_id = rom[map.header_offset];
    map.height_blocks = rom[map.header_offset + 1U];
    map.width_blocks = rom[map.header_offset + 2U];
    map.blocks_pointer = read_u16(rom, map.header_offset + 3U);
    if (map.tileset_id >= 24 || map.width_blocks == 0 || map.height_blocks == 0 ||
        map.width_blocks != map.identity.expected_width ||
        map.height_blocks != map.identity.expected_height) {
        error = "map header has invalid tileset or dimensions";
        return false;
    }

    // Decode the connection transform records following the fixed map header.
    const std::uint8_t connection_mask = rom[map.header_offset + 9U];
    if ((connection_mask & 0xF0U) != 0) {
        error = "map header has an invalid connection mask";
        return false;
    }
    std::size_t connection_cursor = map.header_offset + 10U;
    constexpr std::array<std::uint8_t, 4> directions{8, 4, 2, 1};
    for (const std::uint8_t direction : directions) {
        if ((connection_mask & direction) == 0) continue;
        if (!has_range(rom, connection_cursor, 11)) {
            error = "map connection extends outside the verified ROM";
            return false;
        }
        const auto signed_byte = [](std::uint8_t value) {
            return value < 0x80
                       ? static_cast<std::int16_t>(value)
                       : static_cast<std::int16_t>(static_cast<std::int16_t>(value) - 0x100);
        };
        const std::uint8_t encoded_y = rom[connection_cursor + 7U];
        const std::uint8_t encoded_x = rom[connection_cursor + 8U];
        const bool vertical = direction == 8 || direction == 4;
        map.connections.push_back({
            .direction = direction,
            .target_map_id = rom[connection_cursor],
            .player_x = vertical ? signed_byte(encoded_x) : static_cast<std::int16_t>(encoded_x),
            .player_y = vertical ? static_cast<std::int16_t>(encoded_y) : signed_byte(encoded_y),
        });
        connection_cursor += 11U;
    }
    if (!has_range(rom, connection_cursor, 2) ||
        !decode_map_objects(rom, read_u16(rom, connection_cursor), map, error))
        return false;

    const std::size_t block_count = static_cast<std::size_t>(map.width_blocks) * map.height_blocks;
    if (!bank_pointer_to_offset(rom, map.header_bank, map.blocks_pointer, map.blocks_offset) ||
        !has_range(rom, map.blocks_offset, block_count)) {
        error = "map block grid is outside its declared bank";
        return false;
    }
    map.blocks.assign(rom.begin() + static_cast<std::ptrdiff_t>(map.blocks_offset),
                      rom.begin() + static_cast<std::ptrdiff_t>(map.blocks_offset + block_count));
    result = std::move(map);
    return true;
}

void decode_two_bit_tile(std::span<const std::uint8_t> rom, std::size_t offset,
                         std::vector<std::uint8_t>& pixels) {
    for (std::size_t y = 0; y < 8; ++y) {
        const std::uint8_t low = rom[offset + y * 2U];
        const std::uint8_t high = rom[offset + y * 2U + 1U];
        for (std::size_t x = 0; x < 8; ++x) {
            const unsigned bit = static_cast<unsigned>(7U - x);
            pixels.push_back(
                static_cast<std::uint8_t>(((high >> bit) & 1U) << 1U | ((low >> bit) & 1U)));
        }
    }
}

bool decode_overworld_sprites(std::span<const std::uint8_t> rom,
                              std::vector<ImportedSprite>& result, std::string& error) {
    const std::size_t table_size = kOverworldSpriteCount * kOverworldSpriteEntrySize;
    if (!has_range(rom, kOverworldSpriteTableOffset, table_size)) {
        error = "overworld sprite pointer table crosses the verified ROM";
        return false;
    }
    struct SheetSource {
        std::uint8_t standing_tiles{};
        std::uint8_t bank{};
        std::size_t table{};
        std::size_t graphics{};
    };
    std::array<SheetSource, kOverworldSpriteCount> sources{};
    for (std::size_t index = 0; index < kOverworldSpriteCount; ++index) {
        const std::size_t entry = kOverworldSpriteTableOffset + index * kOverworldSpriteEntrySize;
        const std::uint8_t byte_count = rom[entry + 2U];
        const std::uint8_t bank = rom[entry + 3U];
        if ((byte_count != 4U * 16U && byte_count != 12U * 16U)) {
            error = "overworld sprite has an invalid standing-frame size";
            return false;
        }
        std::size_t graphics = 0;
        if (!bank_pointer_to_offset(rom, bank, read_u16(rom, entry), graphics) ||
            !has_range(rom, graphics, byte_count)) {
            error = "overworld sprite graphic crosses its declared bank";
            return false;
        }

        sources[index] = {
            .standing_tiles =
                static_cast<std::uint8_t>(byte_count / 16U),
            .bank = bank,
            .table = entry,
            .graphics = graphics,
        };
    }
    if (!has_range(
            rom, kOverworldFacingTableOffset,
            kOverworldFacingCount * kOverworldFacingEntrySize)) {
        error = "overworld sprite facing table crosses the verified ROM";
        return false;
    }

    result.reserve(kOverworldSpriteCount);
    for (std::size_t index = 0; index < sources.size(); ++index) {
        const SheetSource& source = sources[index];
        std::size_t graphics_end =
            (static_cast<std::size_t>(source.bank) + 1U) * kBankSize;
        for (const SheetSource& candidate : sources) {
            if (candidate.bank == source.bank &&
                candidate.graphics > source.graphics)
                graphics_end =
                    std::min(graphics_end, candidate.graphics);
        }
        graphics_end = std::min(graphics_end, rom.size());
        const std::size_t available_tiles =
            graphics_end > source.graphics
                ? (graphics_end - source.graphics) / 16U
                : 0U;
        const std::size_t decoded_tiles =
            std::min<std::size_t>(
                available_tiles,
                static_cast<std::size_t>(source.standing_tiles) + 12U);
        if (decoded_tiles < source.standing_tiles) {
            error =
                "overworld sprite does not retain all declared standing tiles";
            return false;
        }
        std::vector<std::uint8_t> tiles;
        tiles.reserve(decoded_tiles * 64U);
        for (std::size_t tile = 0; tile < decoded_tiles; ++tile)
            decode_two_bit_tile(
                rom, source.graphics + tile * 16U, tiles);

        ImportedSprite sprite{
            .id = static_cast<std::uint8_t>(index + 1U),
            .key = std::string(kOverworldSpriteKeys[index]),
            .still = source.standing_tiles == 4U,
            .table_offset = source.table,
            .graphics_offset = source.graphics,
            .pixels = {},
        };
        sprite.pixels.resize(16U * 16U * 16U);
        for (std::size_t facing = 0U; facing < 4U; ++facing) {
            for (std::size_t requested_phase = 0U;
                 requested_phase < 4U; ++requested_phase) {
                std::size_t phase = requested_phase;
                if (!sprite.still && (phase & 1U) != 0U &&
                    decoded_tiles <
                        static_cast<std::size_t>(
                            source.standing_tiles) +
                            12U)
                    phase = 0U;
                const std::size_t frame_index =
                    (sprite.still ? 16U : 0U) +
                    facing * 4U + phase;
                const std::size_t frame =
                    kOverworldFacingTableOffset +
                    frame_index * kOverworldFacingEntrySize;
                std::size_t tile_sequence = 0U;
                std::size_t oam = 0U;
                if (!bank_pointer_to_offset(
                        rom, 1U, read_u16(rom, frame),
                        tile_sequence) ||
                    !bank_pointer_to_offset(
                        rom, 1U, read_u16(rom, frame + 2U), oam) ||
                    tile_sequence < kOverworldTileSequenceBegin ||
                    tile_sequence >= kOverworldTileSequenceEnd ||
                    (tile_sequence - kOverworldTileSequenceBegin) % 4U !=
                        0U ||
                    (oam != kOverworldNormalOam &&
                     oam != kOverworldFlippedOam) ||
                    !has_range(rom, tile_sequence, 4U) ||
                    !has_range(rom, oam, 12U)) {
                    error =
                        "overworld sprite facing record is malformed";
                    return false;
                }
                const std::size_t destination_frame =
                    facing * 4U + requested_phase;
                for (std::size_t part = 0U; part < 4U; ++part) {
                    const std::uint8_t encoded_tile =
                        rom[tile_sequence + part];
                    const std::size_t tile =
                        (encoded_tile & 0x80U) == 0U
                            ? encoded_tile
                            : static_cast<std::size_t>(
                                  source.standing_tiles) +
                                  (encoded_tile & 0x7FU);
                    const std::size_t oam_entry = oam + part * 3U;
                    const std::int32_t part_y =
                        static_cast<std::int8_t>(rom[oam_entry]);
                    const std::int32_t part_x =
                        static_cast<std::int8_t>(rom[oam_entry + 1U]);
                    const bool flip =
                        (rom[oam_entry + 2U] & 0x20U) != 0U;
                    if (tile >= decoded_tiles) {
                        error =
                            "overworld walking frame reads beyond its decoded sprite sheet";
                        return false;
                    }
                    for (std::int32_t y = 0; y < 8; ++y) {
                        for (std::int32_t x = 0; x < 8; ++x) {
                            const std::int32_t output_x = part_x + x;
                            const std::int32_t output_y = part_y + y;
                            if (output_x < 0 || output_x >= 16 ||
                                output_y < 0 || output_y >= 16)
                                continue;
                            const std::size_t source_x =
                                static_cast<std::size_t>(
                                    flip ? 7 - x : x);
                            sprite.pixels[
                                destination_frame * 256U +
                                static_cast<std::size_t>(output_y) * 16U +
                                static_cast<std::size_t>(output_x)] =
                                tiles[tile * 64U +
                                      static_cast<std::size_t>(y) * 8U +
                                      source_x];
                        }
                    }
                }
            }
        }
        result.push_back(std::move(sprite));
    }
    return true;
}

ImportedMap* find_imported_map(std::vector<ImportedMap>& maps, std::uint8_t id) {
    const auto found = std::find_if(maps.begin(), maps.end(),
                                    [&](const ImportedMap& map) { return map.identity.id == id; });
    return found == maps.end() ? nullptr : &*found;
}

void place_connection_target(const ImportedMap& source, const ImportedConnection& connection,
                             std::int32_t& x, std::int32_t& y) {
    const std::int32_t width = static_cast<std::int32_t>(source.width_blocks) * 2;
    const std::int32_t height = static_cast<std::int32_t>(source.height_blocks) * 2;
    if (connection.direction == 1) {
        x = source.global_x_cells + width - connection.player_x;
        y = source.global_y_cells - connection.player_y;
    } else if (connection.direction == 2) {
        x = source.global_x_cells - connection.player_x - 1;
        y = source.global_y_cells - connection.player_y;
    } else if (connection.direction == 4) {
        x = source.global_x_cells - connection.player_x;
        y = source.global_y_cells + height - connection.player_y;
    } else {
        x = source.global_x_cells - connection.player_x;
        y = source.global_y_cells - connection.player_y - 1;
    }
}

void place_connected_surface(std::vector<ImportedMap>& maps) {
    for (ImportedMap& root : maps) {
        if (!root.identity.surface || root.placed) continue;
        const std::int32_t width = static_cast<std::int32_t>(root.width_blocks) * 2;
        const std::int32_t height = static_cast<std::int32_t>(root.height_blocks) * 2;
        root.global_x_cells = static_cast<std::int32_t>(root.identity.town_x) * 10 - width / 2;
        root.global_y_cells = static_cast<std::int32_t>(root.identity.town_y) * 9 - height / 2;
        root.component = 0;
        root.placed = true;

        std::vector<std::uint8_t> pending{root.identity.id};
        for (std::size_t cursor = 0; cursor < pending.size(); ++cursor) {
            ImportedMap* source = find_imported_map(maps, pending[cursor]);
            if (source == nullptr) continue;
            for (const ImportedConnection& connection : source->connections) {
                ImportedMap* target = find_imported_map(maps, connection.target_map_id);
                if (target == nullptr || !target->identity.surface || target->placed) continue;
                place_connection_target(*source, connection, target->global_x_cells,
                                        target->global_y_cells);
                target->component = 0;
                target->placed = true;
                pending.push_back(target->identity.id);
            }
        }
    }
}

void place_interior_world_spaces(std::vector<ImportedMap>& maps) {
    std::uint16_t next_space = 1;
    for (ImportedMap& root : maps) {
        if (root.identity.surface || root.placed) continue;

        // Direct indoor warp topology defines a complex. Outdoor endpoints and
        // LAST_MAP returns do not merge independent buildings into the surface.
        std::vector<ImportedMap*> members;
        std::vector<std::uint8_t> pending{root.identity.id};
        root.placed = true;
        for (std::size_t cursor = 0; cursor < pending.size(); ++cursor) {
            ImportedMap* source = find_imported_map(maps, pending[cursor]);
            if (source == nullptr) continue;
            members.push_back(source);
            for (const ImportedMap::Warp& warp : source->warps) {
                ImportedMap* target = find_imported_map(maps, warp.destination_map_id);
                if (target == nullptr || target->identity.surface || target->placed) continue;
                target->placed = true;
                pending.push_back(target->identity.id);
            }
            for (ImportedMap& candidate : maps) {
                if (candidate.identity.surface || candidate.placed) continue;
                const bool links_to_source =
                    std::ranges::any_of(candidate.warps, [&](const ImportedMap::Warp& warp) {
                        return warp.destination_map_id == source->identity.id;
                    });
                if (links_to_source) {
                    candidate.placed = true;
                    pending.push_back(candidate.identity.id);
                }
            }
        }

        // Splay every map in the complex into one non-overlapping gameplay
        // surface. Warps retain authored endpoints while overview zoom can show
        // every floor at once.
        constexpr std::int32_t row_limit = 128;
        constexpr std::int32_t gap = 4;
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t row_height = 0;
        for (ImportedMap* map : members) {
            const std::int32_t width = static_cast<std::int32_t>(map->width_blocks) * 2;
            const std::int32_t height = static_cast<std::int32_t>(map->height_blocks) * 2;
            if (x != 0 && x + width > row_limit) {
                x = 0;
                y += row_height + gap;
                row_height = 0;
            }
            map->global_x_cells = x;
            map->global_y_cells = y;
            map->component = next_space;
            x += width + gap;
            row_height = std::max(row_height, height);
        }
        ++next_space;
    }
}

std::vector<ImportedWorldSpace> build_world_spaces(const std::vector<ImportedMap>& maps) {
    std::uint16_t maximum = 0;
    for (const ImportedMap& map : maps)
        maximum = std::max(maximum, map.component);
    std::vector<ImportedWorldSpace> spaces(static_cast<std::size_t>(maximum) + 1U);
    for (std::uint16_t id = 0; id <= maximum; ++id) {
        const auto first =
            std::find_if(maps.begin(), maps.end(),
                         [id](const ImportedMap& map) { return map.component == id; });
        spaces[id] = {
            .id = id,
            .key = id == 0 ? "kanto_surface"
                           : (first == maps.end() ? "world_space_" + std::to_string(id)
                                                : first->identity.key + "_space"),
            .outdoor = id == 0,
        };
    }
    return spaces;
}

void place_world_spaces(std::vector<ImportedMap>& maps) {
    place_connected_surface(maps);
    place_interior_world_spaces(maps);

    // Red's two house floors form one readable vertical cutaway with one
    // world-cell of air between them. Gameplay warps remain unchanged.
    ImportedMap* house_1f = nullptr;
    ImportedMap* house_2f = nullptr;
    for (ImportedMap& map : maps) {
        if (map.identity.key == "reds_house_1f")
            house_1f = &map;
        else if (map.identity.key == "reds_house_2f")
            house_2f = &map;
    }
    if (house_1f != nullptr && house_2f != nullptr &&
        house_1f->component == house_2f->component) {
        house_1f->global_x_cells = 0;
        house_1f->global_y_cells = 0;
        house_2f->global_x_cells = 0;
        house_2f->global_y_cells =
            -static_cast<std::int32_t>(house_2f->height_blocks) * 2 -
            1;
    }

    // Camera framing is authored by the Red importer and interpreted by the
    // generic camera director. Unlisted surface maps use the modern 2x
    // overview; interior complexes try to fit their complete shared space.
    for (ImportedMap& map : maps) {
        if (!map.identity.surface)
            map.camera_framing = ImportedCameraFraming::fit_space;
        if (map.identity.key == "pallet_town")
            map.camera_framing = ImportedCameraFraming::fit_map;
        else if (map.identity.key == "route_1")
            map.camera_framing = ImportedCameraFraming::fit_width;
        else if (map.identity.key == "viridian_city") {
            map.camera_framing =
                ImportedCameraFraming::fixed_zoom;
            map.camera_zoom = 3.0F;
        } else if (map.identity.key == "route_22")
            map.camera_framing = ImportedCameraFraming::fit_height;
    }
}

bool decode_tileset(std::span<const std::uint8_t> rom, const std::vector<ImportedMap>& maps,
                    std::uint8_t tileset_id, ImportedTileset& result, std::string& error) {
    if (maps.empty()) {
        error = "map slice contains no maps";
        return false;
    }

    ImportedTileset tileset;
    tileset.id = tileset_id;
    tileset.header_offset =
        kTilesetHeadersOffset + static_cast<std::size_t>(tileset.id) * kTilesetHeaderSize;
    if (!has_range(rom, tileset.header_offset, kTilesetHeaderSize)) {
        error = "tileset header extends outside the verified ROM";
        return false;
    }
    tileset.bank = rom[tileset.header_offset];
    tileset.blocks_pointer = read_u16(rom, tileset.header_offset + 1U);
    tileset.graphics_pointer = read_u16(rom, tileset.header_offset + 3U);
    tileset.collision_pointer = read_u16(rom, tileset.header_offset + 5U);
    tileset.grass_tile = rom[tileset.header_offset + 10U];
    tileset.animation_mode = rom[tileset.header_offset + 11U];
    if (tileset.animation_mode > 2) {
        error = "tileset has an invalid background-animation mode";
        return false;
    }
    if (!bank_pointer_to_offset(rom, tileset.bank, tileset.blocks_pointer, tileset.blocks_offset) ||
        !bank_pointer_to_offset(rom, tileset.bank, tileset.graphics_pointer,
                                tileset.graphics_offset) ||
        !visible_pointer_to_offset(rom, tileset.bank, tileset.collision_pointer,
                                   tileset.collision_offset) ||
        tileset.graphics_offset >= tileset.blocks_offset) {
        error = "tileset graphics, block, or collision pointer is invalid";
        return false;
    }

    for (std::size_t cursor = tileset.collision_offset;
         cursor < rom.size() && tileset.passable_tiles.size() < 256U; ++cursor) {
        if (rom[cursor] == 0xFFU) break;
        tileset.passable_tiles.push_back(rom[cursor]);
    }
    if (tileset.passable_tiles.empty() ||
        tileset.collision_offset + tileset.passable_tiles.size() >= rom.size() ||
        rom[tileset.collision_offset + tileset.passable_tiles.size()] != 0xFFU) {
        error = "tileset collision list is empty or unterminated";
        return false;
    }

    const std::size_t graphics_size = tileset.blocks_offset - tileset.graphics_offset;
    if (graphics_size == 0 || graphics_size % 16U != 0 ||
        !has_range(rom, tileset.graphics_offset, graphics_size)) {
        error = "tileset graphics do not form complete two-bit tiles";
        return false;
    }
    const std::size_t graphic_tile_count = graphics_size / 16U;
    if (graphic_tile_count > std::numeric_limits<std::uint16_t>::max()) {
        error = "tileset graphics contain too many tiles";
        return false;
    }

    std::uint8_t maximum_block = 0;
    bool used = false;
    for (const ImportedMap& map : maps) {
        if (map.tileset_id != tileset.id) continue;
        const auto found = std::max_element(map.blocks.begin(), map.blocks.end());
        if (found != map.blocks.end()) {
            maximum_block = used ? std::max(maximum_block, *found) : *found;
            used = true;
        }
    }
    if (!used) {
        error = "tileset has no map in the selected outdoor slice";
        return false;
    }
    tileset.block_count = static_cast<std::size_t>(maximum_block) + 1U;
    const std::size_t block_bytes = tileset.block_count * kTilesPerBlock;
    if (!has_range(rom, tileset.blocks_offset, block_bytes)) {
        error = "required tileset blocks extend outside the verified ROM";
        return false;
    }
    tileset.block_tiles.assign(
        rom.begin() + static_cast<std::ptrdiff_t>(tileset.blocks_offset),
        rom.begin() + static_cast<std::ptrdiff_t>(tileset.blocks_offset + block_bytes));

    // Decode the shared two-bit tile atlas once; runtime never reads cartridge encoding.
    tileset.pixels.resize(graphic_tile_count * 64U);
    for (std::size_t tile = 0; tile < graphic_tile_count; ++tile) {
        for (std::size_t y = 0; y < 8; ++y) {
            const std::uint8_t low = rom[tileset.graphics_offset + tile * 16U + y * 2U];
            const std::uint8_t high = rom[tileset.graphics_offset + tile * 16U + y * 2U + 1U];
            for (std::size_t x = 0; x < 8; ++x) {
                const unsigned bit = static_cast<unsigned>(7U - x);
                tileset.pixels[tile * 64U + y * 8U + x] =
                    static_cast<std::uint8_t>(((high >> bit) & 1U) << 1U | ((low >> bit) & 1U));
            }
        }
    }

    // Materialize the cartridge's water rotation and flower replacement frames.
    if (tileset.animation_mode != 0) {
        constexpr std::size_t water_tile = 0x14;
        const std::size_t water_source = tileset.graphics_offset + water_tile * kAnimationTileBytes;
        if (!has_range(rom, water_source, kAnimationTileBytes)) {
            error = "tileset water-animation tile is outside its graphics";
            return false;
        }
        std::array<std::uint8_t, kAnimationTileBytes> encoded{};
        std::copy_n(rom.begin() + static_cast<std::ptrdiff_t>(water_source), kAnimationTileBytes,
                    encoded.begin());
        std::uint8_t counter = 0;
        for (std::size_t frame = 0; frame < kWaterAnimationFrameCount; ++frame) {
            counter = static_cast<std::uint8_t>((counter + 1U) & 7U);
            for (std::uint8_t& byte : encoded) {
                if ((counter & 4U) == 0)
                    byte = static_cast<std::uint8_t>(static_cast<std::uint8_t>(byte >> 1U) |
                                                     static_cast<std::uint8_t>(byte << 7U));
                else
                    byte = static_cast<std::uint8_t>(static_cast<std::uint8_t>(byte << 1U) |
                                                     static_cast<std::uint8_t>(byte >> 7U));
            }
            for (std::size_t y = 0; y < 8; ++y) {
                const std::uint8_t low = encoded[y * 2U];
                const std::uint8_t high = encoded[y * 2U + 1U];
                for (std::size_t x = 0; x < 8; ++x) {
                    const unsigned bit = static_cast<unsigned>(7U - x);
                    tileset.animation_pixels.push_back(static_cast<std::uint8_t>(
                        ((high >> bit) & 1U) << 1U | ((low >> bit) & 1U)));
                }
            }
        }
    }
    if (tileset.animation_mode == 2) {
        const std::size_t flower_bytes = kFlowerAnimationFrameCount * kAnimationTileBytes;
        if (!has_range(rom, kFlowerAnimationFramesOffset, flower_bytes)) {
            error = "flower-animation frames extend outside the verified ROM";
            return false;
        }
        for (std::size_t frame = 0; frame < kFlowerAnimationFrameCount; ++frame) {
            const std::size_t source = kFlowerAnimationFramesOffset + frame * kAnimationTileBytes;
            for (std::size_t y = 0; y < 8; ++y) {
                const std::uint8_t low = rom[source + y * 2U];
                const std::uint8_t high = rom[source + y * 2U + 1U];
                for (std::size_t x = 0; x < 8; ++x) {
                    const unsigned bit = static_cast<unsigned>(7U - x);
                    tileset.animation_pixels.push_back(static_cast<std::uint8_t>(
                        ((high >> bit) & 1U) << 1U | ((low >> bit) & 1U)));
                }
            }
        }
    }
    result = std::move(tileset);
    return true;
}

bool expand_maps(const std::vector<ImportedTileset>& tilesets, std::vector<ImportedMap>& maps,
                 std::string& error) {
    for (ImportedMap& map : maps) {
        const auto found =
            std::find_if(tilesets.begin(), tilesets.end(),
                         [&](const ImportedTileset& value) { return value.id == map.tileset_id; });
        if (found == tilesets.end()) {
            error = "map has no decoded tileset";
            return false;
        }
        const ImportedTileset& tileset = *found;
        const std::size_t width = static_cast<std::size_t>(map.width_blocks) * 4U;
        const std::size_t height = static_cast<std::size_t>(map.height_blocks) * 4U;
        map.tiles.resize(width * height);
        for (std::size_t block_y = 0; block_y < map.height_blocks; ++block_y) {
            for (std::size_t block_x = 0; block_x < map.width_blocks; ++block_x) {
                const std::size_t map_block = block_y * map.width_blocks + block_x;
                const std::size_t definition =
                    static_cast<std::size_t>(map.blocks[map_block]) * kTilesPerBlock;
                if (definition + kTilesPerBlock > tileset.block_tiles.size()) {
                    error = "map references a block outside the decoded tileset";
                    return false;
                }
                for (std::size_t tile_y = 0; tile_y < 4; ++tile_y) {
                    for (std::size_t tile_x = 0; tile_x < 4; ++tile_x) {
                        const std::size_t destination =
                            (block_y * 4U + tile_y) * width + block_x * 4U + tile_x;
                        map.tiles[destination] =
                            tileset.block_tiles[definition + tile_y * 4U + tile_x];
                    }
                }
            }
        }
    }
    return true;
}

void emit_readable_source(const std::vector<ImportedTileset>& tilesets,
                          const std::vector<ImportedSprite>& sprites,
                          const std::vector<ImportedWorldSpace>& spaces,
                          const std::vector<ImportedLedge>& ledges,
                          const std::vector<ImportedMap>& maps, MapImport& result) {
    const auto direction_name = [](std::uint8_t direction) {
        if (direction == 8) return "north";
        if (direction == 4) return "south";
        if (direction == 2) return "west";
        return "east";
    };
    for (const ImportedTileset& tileset : tilesets) {
        std::ostringstream tileset_source;
        tileset_source << "; Locally decoded tileset used by the connected world renderer.\n"
                       << "tileset tileset_" << static_cast<unsigned>(tileset.id) << '\n'
                       << "    rom_id " << static_cast<unsigned>(tileset.id) << '\n'
                       << "    tile_count " << tileset.pixels.size() / 64U << '\n'
                       << "    block_count " << tileset.block_count << '\n'
                       << "    animation_mode " << static_cast<unsigned>(tileset.animation_mode)
                       << '\n'
                       << "    grass_tile "
                       << static_cast<unsigned>(tileset.grass_tile) << '\n'
                       << "    passable_tiles";
        for (const std::uint8_t tile : tileset.passable_tiles)
            tileset_source << ' ' << static_cast<unsigned>(tile);
        tileset_source
                       << '\n'
                       << "    animation_frames " << tileset.animation_pixels.size() / 64U << '\n'
                       << "    graphics_source " << tileset.graphics_offset << ' '
                       << tileset.blocks_offset - tileset.graphics_offset << '\n'
                       << "    block_source " << tileset.blocks_offset << ' '
                       << tileset.block_tiles.size() << '\n';
        add_text_file(result,
                      "source/world/tilesets/tileset_" +
                          std::to_string(static_cast<unsigned>(tileset.id)) + ".sexpr",
                      tileset_source.str());
    }

    std::ostringstream sprite_source;
    sprite_source << "; ROM-decoded overworld sprite sheets normalized to directional walk clips.\n";
    for (const ImportedSprite& sprite : sprites) {
        sprite_source << "sprite " << sprite.key << '\n'
                      << "    rom_id " << static_cast<unsigned>(sprite.id) << '\n'
                      << "    kind " << (sprite.still ? "still" : "directional") << '\n'
                      << "    table_source " << sprite.table_offset << ' '
                      << kOverworldSpriteEntrySize << '\n'
                      << "    declared_graphics_source " << sprite.graphics_offset << ' '
                      << (sprite.still ? 4U : 12U) * 16U << '\n'
                      << "    normalized_frames 16\n";
    }
    add_text_file(result, "source/world/overworld_sprites.sexpr", sprite_source.str());

    std::ostringstream space_source;
    space_source << "; Importer-derived continuous gameplay coordinate systems.\n";
    for (const ImportedWorldSpace& space : spaces) {
        space_source << "world_space " << space.key << '\n'
                     << "    id " << space.id << '\n'
                     << "    environment " << (space.outdoor ? "outdoor" : "interior") << '\n';
        for (const ImportedMap& map : maps) {
            if (map.component != space.id) continue;
            space_source << "    map_placement " << map.identity.key << " origin "
                         << map.global_x_cells << ' ' << map.global_y_cells
                         << " layer 0 global_cells\n";
        }
    }
    add_text_file(result, "source/world/world_spaces.sexpr", space_source.str());

    std::ostringstream ledge_source;
    ledge_source
        << "; ROM-decoded ledge traversal rules. Directions are semantic engine values.\n";
    constexpr std::array<std::string_view, 4> direction_names{
        "down", "up", "left", "right"};
    for (const ImportedLedge& ledge : ledges) {
        ledge_source << "ledge\n"
                     << "    direction "
                     << direction_names[ledge.direction] << '\n'
                     << "    standing_tile "
                     << static_cast<unsigned>(ledge.standing_tile) << '\n'
                     << "    ledge_tile "
                     << static_cast<unsigned>(ledge.ledge_tile) << '\n'
                     << "    required_input_mask "
                     << static_cast<unsigned>(ledge.required_input_mask) << '\n';
    }
    add_text_file(result, "source/world/ledges.sexpr",
                  ledge_source.str());

    for (const ImportedMap& map : maps) {
        const std::size_t width_tiles = static_cast<std::size_t>(map.width_blocks) * 4U;
        const std::size_t height_tiles = static_cast<std::size_t>(map.height_blocks) * 4U;
        std::ostringstream source;
        source << "; Complete block grid retained for readable local inspection.\n"
               << "map " << map.identity.key << '\n'
               << "    rom_id " << static_cast<unsigned>(map.identity.id) << '\n'
               << "    name \"" << map.identity.display_name << "\"\n"
               << "    town_map_position " << static_cast<unsigned>(map.identity.town_x) << ' '
               << static_cast<unsigned>(map.identity.town_y) << '\n'
               << "    tileset tileset_" << static_cast<unsigned>(map.tileset_id) << '\n'
               << "    size_blocks " << static_cast<unsigned>(map.width_blocks) << ' '
               << static_cast<unsigned>(map.height_blocks) << '\n'
               << "    size_tiles " << width_tiles << ' ' << height_tiles << '\n'
               << "    world_origin_cells " << map.global_x_cells << ' ' << map.global_y_cells
               << '\n'
               << "    world_space " << spaces[map.component].key << '\n'
               << "    camera_framing ";
        switch (map.camera_framing) {
        case ImportedCameraFraming::fixed_zoom:
            source << "fixed_zoom " << map.camera_zoom;
            break;
        case ImportedCameraFraming::fit_map:
            source << "fit_map";
            break;
        case ImportedCameraFraming::fit_width:
            source << "fit_width";
            break;
        case ImportedCameraFraming::fit_height:
            source << "fit_height";
            break;
        case ImportedCameraFraming::fit_space:
            source << "fit_space";
            break;
        }
        source << '\n'
               << "    header_source " << map.header_offset << '\n'
               << "    block_source " << map.blocks_offset << ' ' << map.blocks.size() << '\n'
               << "    objects_source " << map.objects_offset << '\n';
        for (const ImportedConnection& connection : map.connections) {
            source << "    connection " << direction_name(connection.direction) << " map_"
                   << static_cast<unsigned>(connection.target_map_id) << " player_offset "
                   << connection.player_x << ' ' << connection.player_y << '\n';
        }
        for (const ImportedMap::Warp& warp : map.warps) {
            source << "    warp " << static_cast<unsigned>(warp.index) << " at "
                   << static_cast<unsigned>(warp.x) << ' ' << static_cast<unsigned>(warp.y)
                   << " destination map_" << static_cast<unsigned>(warp.destination_map_id)
                   << " warp " << static_cast<unsigned>(warp.destination_warp_index) << '\n';
        }
        for (const ImportedMap::Actor& actor : map.actors) {
            const char* kind = actor.kind == 1   ? "trainer_or_pokemon"
                               : actor.kind == 2 ? "item"
                                                 : "npc";
            const ImportedSprite& sprite = sprites[static_cast<std::size_t>(actor.sprite_id) - 1U];
            source << "    actor " << static_cast<unsigned>(actor.index) << " sprite " << sprite.key
                   << " at " << static_cast<unsigned>(actor.x) << ' '
                   << static_cast<unsigned>(actor.y) << " kind " << kind << " movement "
                   << static_cast<unsigned>(actor.movement) << " direction_or_axis "
                   << static_cast<unsigned>(actor.direction_or_axis) << " text "
                   << static_cast<unsigned>(actor.text_id);
            if (actor.kind != 0)
                source << " parameters " << static_cast<unsigned>(actor.parameter_a) << ' '
                       << static_cast<unsigned>(actor.parameter_b);
            source << '\n';
            if (actor.movement == 0xFEU) {
                source << "        movement_bounds authored_map " << map.global_x_cells << ' '
                       << map.global_y_cells << ' ' << static_cast<unsigned>(map.width_blocks) * 2U
                       << ' ' << static_cast<unsigned>(map.height_blocks) * 2U << " global_cells\n";
            }
        }
        source << "    blocks\n";
        for (std::size_t y = 0; y < map.height_blocks; ++y) {
            source << "        row";
            for (std::size_t x = 0; x < map.width_blocks; ++x)
                source << ' ' << static_cast<unsigned>(map.blocks[y * map.width_blocks + x]);
            source << '\n';
        }
        add_text_file(result,
                      "source/world/maps/" +
                          std::to_string(static_cast<unsigned>(map.identity.id)) + "_" +
                          map.identity.key + ".sexpr",
                      source.str());
    }
}

void emit_runtime_cache(const std::vector<ImportedTileset>& tilesets,
                        const std::vector<ImportedSprite>& sprites,
                        const std::vector<ImportedWorldSpace>& spaces,
                        const std::vector<ImportedLedge>& ledges,
                        const std::vector<ImportedMap>& maps, MapImport& result) {
    std::vector<std::uint8_t> bytes{'P', 'M', 'V', 'C'};
    write_u16(bytes, tilesets.size());
    for (const ImportedTileset& tileset : tilesets) {
        bytes.push_back(tileset.id);
        write_u16(bytes, tileset.pixels.size() / 64U);
        bytes.push_back(tileset.grass_tile);
        bytes.push_back(tileset.animation_mode);
        bytes.push_back(static_cast<std::uint8_t>(tileset.passable_tiles.size()));
        bytes.insert(bytes.end(), tileset.passable_tiles.begin(), tileset.passable_tiles.end());
        write_u32(bytes, tileset.pixels.size());
        bytes.insert(bytes.end(), tileset.pixels.begin(), tileset.pixels.end());
        write_u32(bytes, tileset.animation_pixels.size());
        bytes.insert(bytes.end(), tileset.animation_pixels.begin(), tileset.animation_pixels.end());
    }

    write_u16(bytes, sprites.size());
    for (const ImportedSprite& sprite : sprites) {
        bytes.push_back(sprite.id);
        bytes.push_back(static_cast<std::uint8_t>(sprite.key.size()));
        bytes.insert(bytes.end(), sprite.key.begin(), sprite.key.end());
        bytes.push_back(sprite.still ? 1U : 0U);
        write_u32(bytes, sprite.pixels.size());
        bytes.insert(bytes.end(), sprite.pixels.begin(), sprite.pixels.end());
    }

    write_u16(bytes, spaces.size());
    for (const ImportedWorldSpace& space : spaces) {
        write_u16(bytes, space.id);
        bytes.push_back(static_cast<std::uint8_t>(space.key.size()));
        bytes.insert(bytes.end(), space.key.begin(), space.key.end());
        bytes.push_back(space.outdoor ? 1U : 0U);
    }

    bytes.push_back(static_cast<std::uint8_t>(ledges.size()));
    for (const ImportedLedge& ledge : ledges) {
        bytes.push_back(ledge.direction);
        bytes.push_back(ledge.standing_tile);
        bytes.push_back(ledge.ledge_tile);
        bytes.push_back(ledge.required_input_mask);
    }

    write_u16(bytes, maps.size());
    for (const ImportedMap& map : maps) {
        const std::string_view key = map.identity.key;
        const std::string_view name = map.identity.display_name;
        bytes.push_back(map.identity.id);
        bytes.push_back(map.tileset_id);
        bytes.push_back(map.width_blocks);
        bytes.push_back(map.height_blocks);
        write_u16(bytes, static_cast<std::size_t>(map.width_blocks) * 4U);
        write_u16(bytes, static_cast<std::size_t>(map.height_blocks) * 4U);
        bytes.push_back(static_cast<std::uint8_t>(key.size()));
        bytes.insert(bytes.end(), key.begin(), key.end());
        bytes.push_back(static_cast<std::uint8_t>(name.size()));
        bytes.insert(bytes.end(), name.begin(), name.end());
        write_i32(bytes, map.global_x_cells * 2);
        write_i32(bytes, map.global_y_cells * 2);
        write_u16(bytes, map.component);
        bytes.push_back(
            static_cast<std::uint8_t>(map.camera_framing));
        write_u16(
            bytes,
            static_cast<std::uint16_t>(map.camera_zoom * 100.0F));
        write_u32(bytes, map.tiles.size());
        bytes.insert(bytes.end(), map.tiles.begin(), map.tiles.end());
        bytes.push_back(static_cast<std::uint8_t>(map.warps.size()));
        for (const ImportedMap::Warp& warp : map.warps) {
            bytes.push_back(warp.index);
            bytes.push_back(warp.x);
            bytes.push_back(warp.y);
            bytes.push_back(warp.destination_map_id);
            bytes.push_back(warp.destination_warp_index);
        }
        bytes.push_back(static_cast<std::uint8_t>(map.actors.size()));
        for (const ImportedMap::Actor& actor : map.actors) {
            bytes.push_back(actor.index);
            bytes.push_back(actor.sprite_id);
            bytes.push_back(actor.x);
            bytes.push_back(actor.y);
            bytes.push_back(actor.movement);
            bytes.push_back(actor.direction_or_axis);
            bytes.push_back(actor.text_id);
            bytes.push_back(actor.parameter_a);
            bytes.push_back(actor.parameter_b);
            bytes.push_back(actor.kind);
            const bool roams = actor.movement == 0xFEU;
            bytes.push_back(roams ? 1U : 0U);
            if (roams) {
                write_i32(bytes, map.global_x_cells);
                write_i32(bytes, map.global_y_cells);
                write_u16(bytes, static_cast<std::size_t>(map.width_blocks) * 2U);
                write_u16(bytes, static_cast<std::size_t>(map.height_blocks) * 2U);
            }
        }
    }
    result.files.push_back({"compiled/world_maps.bin", std::move(bytes)});
}

} // namespace

bool decode_map_import(std::span<const std::uint8_t> rom, MapImport& result, std::string& error) {
    result = {};
    if (!verify_pokemon_red_us_rev_0(rom, error)) return false;

    std::vector<MapIdentity> identities;
    if (!discover_maps(rom, identities, error)) return false;
    std::vector<ImportedSprite> sprites;
    if (!decode_overworld_sprites(rom, sprites, error)) return false;
    std::vector<ImportedMap> maps;
    maps.reserve(identities.size());
    for (MapIdentity& identity : identities) {
        ImportedMap map;
        const std::string key = identity.key;
        if (!decode_map(rom, std::move(identity), map, error)) {
            error = key + ": " + error;
            return false;
        }
        maps.push_back(std::move(map));
    }
    place_world_spaces(maps);
    const std::vector<ImportedWorldSpace> spaces = build_world_spaces(maps);
    std::vector<ImportedLedge> ledges;
    for (std::size_t cursor = kLedgesOffset;
         cursor < kLedgesEnd && rom[cursor] != 0xFFU;
         cursor += 4U) {
        if (cursor + 4U > kLedgesEnd ||
            (rom[cursor] & 3U) != 0U || rom[cursor] > 12U) {
            error = "LedgeTiles contains an invalid record";
            return false;
        }
        ledges.push_back({
            .direction =
                static_cast<std::uint8_t>(rom[cursor] / 4U),
            .standing_tile = rom[cursor + 1U],
            .ledge_tile = rom[cursor + 2U],
            .required_input_mask = rom[cursor + 3U],
        });
    }
    if (kLedgesEnd == 0U || rom[kLedgesEnd - 1U] != 0xFFU) {
        error = "LedgeTiles is missing its exact sentinel";
        return false;
    }

    std::vector<std::uint8_t> tileset_ids;
    for (const ImportedMap& map : maps) {
        if (std::find(tileset_ids.begin(), tileset_ids.end(), map.tileset_id) == tileset_ids.end())
            tileset_ids.push_back(map.tileset_id);
    }
    std::vector<ImportedTileset> tilesets;
    tilesets.reserve(tileset_ids.size());
    for (const std::uint8_t id : tileset_ids) {
        ImportedTileset tileset;
        if (!decode_tileset(rom, maps, id, tileset, error)) return false;
        tilesets.push_back(std::move(tileset));
    }
    if (!expand_maps(tilesets, maps, error)) return false;

    emit_readable_source(tilesets, sprites, spaces, ledges, maps, result);
    emit_runtime_cache(tilesets, sprites, spaces, ledges, maps, result);
    result.maps = maps.size();
    result.world_spaces = spaces.size();
    result.unused_map_slots = kMapSlotCount - maps.size();
    result.tilesets = tilesets.size();
    result.sprites = sprites.size();
    result.ledges = ledges.size();
    for (const ImportedMap& map : maps) {
        result.warps += map.warps.size();
        result.actors += map.actors.size();
        result.expanded_tiles += map.tiles.size();
    }
    return true;
}

} // namespace pokered::import
