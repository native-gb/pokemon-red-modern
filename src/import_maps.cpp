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
constexpr std::size_t kAnimationTileBytes = 16;
constexpr std::size_t kWaterAnimationFrameCount = 8;
constexpr std::size_t kFlowerAnimationFrameCount = 3;
constexpr std::size_t kOverworldSpriteTableOffset = 0x17B27;
constexpr std::size_t kOverworldSpriteCount = 72;
constexpr std::size_t kOverworldSpriteEntrySize = 4;

struct MapIdentity {
    std::uint8_t id{};
    std::string key;
    std::string display_name;
    std::uint8_t town_x{};
    std::uint8_t town_y{};
};

struct ImportedConnection {
    std::uint8_t direction{};
    std::uint8_t target_map_id{};
    std::int16_t player_x{};
    std::int16_t player_y{};
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

struct ImportedTileset {
    std::uint8_t id{};
    std::uint8_t bank{};
    std::uint16_t blocks_pointer{};
    std::uint16_t graphics_pointer{};
    std::size_t header_offset{};
    std::size_t blocks_offset{};
    std::size_t graphics_offset{};
    std::size_t block_count{};
    std::uint8_t animation_mode{};
    std::vector<std::uint8_t> block_tiles;
    std::vector<std::uint8_t> pixels;
    std::vector<std::uint8_t> animation_pixels;
};

struct ImportedSprite {
    std::uint8_t id{};
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

bool discover_outdoor_maps(std::span<const std::uint8_t> rom, std::vector<MapIdentity>& result,
                           std::string& error) {
    const std::size_t table_size = kTownMapExternalEntryCount * kTownMapExternalEntrySize;
    if (!has_range(rom, kTownMapExternalEntriesOffset, table_size)) {
        error = "Town Map external-entry table extends outside the verified ROM";
        return false;
    }
    for (std::size_t index = 0; index < kTownMapExternalEntryCount; ++index) {
        const std::size_t offset =
            kTownMapExternalEntriesOffset + index * kTownMapExternalEntrySize;
        const std::uint8_t packed = rom[offset];
        if (packed == 0) continue;
        MapIdentity identity;
        identity.id = static_cast<std::uint8_t>(index);
        identity.town_x = static_cast<std::uint8_t>(packed & 0x0FU);
        identity.town_y = static_cast<std::uint8_t>(packed >> 4U);
        if (!decode_map_name(rom, read_u16(rom, offset + 1U), identity.display_name, error))
            return false;
        identity.key = make_map_key(identity.display_name, identity.id);
        result.push_back(std::move(identity));
    }
    if (result.empty()) {
        error = "Town Map table produced no outdoor maps";
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
    if (map.tileset_id >= 24 || map.width_blocks == 0 || map.height_blocks == 0) {
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
    result.reserve(kOverworldSpriteCount);
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

        const std::size_t tile_count = byte_count / 16U;
        std::vector<std::uint8_t> tiles;
        tiles.reserve(tile_count * 64U);
        for (std::size_t tile = 0; tile < tile_count; ++tile)
            decode_two_bit_tile(rom, graphics + tile * 16U, tiles);

        ImportedSprite sprite{
            .id = static_cast<std::uint8_t>(index + 1U),
            .still = tile_count == 4U,
            .table_offset = entry,
            .graphics_offset = graphics,
            .pixels = {},
        };
        sprite.pixels.resize(4U * 16U * 16U);
        for (std::size_t facing = 0; facing < 4U; ++facing) {
            const std::size_t source_facing = sprite.still ? 0U : facing == 3U ? 2U : facing;
            for (std::size_t part = 0; part < 4U; ++part) {
                const std::size_t tile = source_facing * 4U + part;
                const std::size_t part_x = part % 2U;
                const std::size_t part_y = part / 2U;
                for (std::size_t y = 0; y < 8U; ++y) {
                    for (std::size_t x = 0; x < 8U; ++x) {
                        const std::size_t output_x =
                            facing == 3U ? 15U - (part_x * 8U + x) : part_x * 8U + x;
                        const std::size_t destination =
                            facing * 256U + (part_y * 8U + y) * 16U + output_x;
                        sprite.pixels[destination] = tiles[tile * 64U + y * 8U + x];
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

void place_global_maps(std::vector<ImportedMap>& maps) {
    std::uint16_t component = 0;
    for (ImportedMap& root : maps) {
        if (root.placed) continue;
        const std::int32_t width = static_cast<std::int32_t>(root.width_blocks) * 2;
        const std::int32_t height = static_cast<std::int32_t>(root.height_blocks) * 2;
        root.global_x_cells = static_cast<std::int32_t>(root.identity.town_x) * 10 - width / 2;
        root.global_y_cells = static_cast<std::int32_t>(root.identity.town_y) * 9 - height / 2;
        root.component = component;
        root.placed = true;

        std::vector<std::uint8_t> pending{root.identity.id};
        for (std::size_t cursor = 0; cursor < pending.size(); ++cursor) {
            ImportedMap* source = find_imported_map(maps, pending[cursor]);
            if (source == nullptr) continue;
            for (const ImportedConnection& connection : source->connections) {
                ImportedMap* target = find_imported_map(maps, connection.target_map_id);
                if (target == nullptr || target->placed) continue;
                place_connection_target(*source, connection, target->global_x_cells,
                                        target->global_y_cells);
                target->component = component;
                target->placed = true;
                pending.push_back(target->identity.id);
            }
        }
        ++component;
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
    tileset.animation_mode = rom[tileset.header_offset + 11U];
    if (tileset.animation_mode > 2) {
        error = "tileset has an invalid background-animation mode";
        return false;
    }
    if (!bank_pointer_to_offset(rom, tileset.bank, tileset.blocks_pointer, tileset.blocks_offset) ||
        !bank_pointer_to_offset(rom, tileset.bank, tileset.graphics_pointer,
                                tileset.graphics_offset) ||
        tileset.graphics_offset >= tileset.blocks_offset) {
        error = "tileset graphics or block pointer is invalid";
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
    sprite_source << "; ROM-decoded overworld sprite sheets normalized to four standing views.\n";
    for (const ImportedSprite& sprite : sprites) {
        sprite_source << "sprite overworld_" << static_cast<unsigned>(sprite.id) << '\n'
                      << "    rom_id " << static_cast<unsigned>(sprite.id) << '\n'
                      << "    kind " << (sprite.still ? "still" : "directional") << '\n'
                      << "    table_source " << sprite.table_offset << ' '
                      << kOverworldSpriteEntrySize << '\n'
                      << "    graphics_source " << sprite.graphics_offset << ' '
                      << (sprite.still ? 4U : 12U) * 16U << '\n';
    }
    add_text_file(result, "source/world/overworld_sprites.sexpr", sprite_source.str());

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
               << "    world_component " << map.component << '\n'
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
            source << "    actor " << static_cast<unsigned>(actor.index) << " sprite overworld_"
                   << static_cast<unsigned>(actor.sprite_id) << " at "
                   << static_cast<unsigned>(actor.x) << ' ' << static_cast<unsigned>(actor.y)
                   << " kind " << kind << " movement " << static_cast<unsigned>(actor.movement)
                   << " direction_or_axis " << static_cast<unsigned>(actor.direction_or_axis)
                   << " text " << static_cast<unsigned>(actor.text_id);
            if (actor.kind != 0)
                source << " parameters " << static_cast<unsigned>(actor.parameter_a) << ' '
                       << static_cast<unsigned>(actor.parameter_b);
            source << '\n';
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
                        const std::vector<ImportedMap>& maps, MapImport& result) {
    std::vector<std::uint8_t> bytes{'P', 'M', 'V', '5'};
    write_u16(bytes, tilesets.size());
    for (const ImportedTileset& tileset : tilesets) {
        bytes.push_back(tileset.id);
        write_u16(bytes, tileset.pixels.size() / 64U);
        bytes.push_back(tileset.animation_mode);
        write_u32(bytes, tileset.pixels.size());
        bytes.insert(bytes.end(), tileset.pixels.begin(), tileset.pixels.end());
        write_u32(bytes, tileset.animation_pixels.size());
        bytes.insert(bytes.end(), tileset.animation_pixels.begin(), tileset.animation_pixels.end());
    }

    write_u16(bytes, sprites.size());
    for (const ImportedSprite& sprite : sprites) {
        bytes.push_back(sprite.id);
        bytes.push_back(sprite.still ? 1U : 0U);
        write_u32(bytes, sprite.pixels.size());
        bytes.insert(bytes.end(), sprite.pixels.begin(), sprite.pixels.end());
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
        }
    }
    result.files.push_back({"compiled/world_maps.bin", std::move(bytes)});
}

} // namespace

bool decode_map_import(std::span<const std::uint8_t> rom, MapImport& result, std::string& error) {
    result = {};
    if (!verify_pokemon_red_us_rev_0(rom, error)) return false;

    std::vector<MapIdentity> identities;
    if (!discover_outdoor_maps(rom, identities, error)) return false;
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
    place_global_maps(maps);

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

    emit_readable_source(tilesets, sprites, maps, result);
    emit_runtime_cache(tilesets, sprites, maps, result);
    result.maps = maps.size();
    result.tilesets = tilesets.size();
    result.sprites = sprites.size();
    for (const ImportedMap& map : maps) {
        result.warps += map.warps.size();
        result.actors += map.actors.size();
        result.expanded_tiles += map.tiles.size();
    }
    return true;
}

} // namespace pokered::import
