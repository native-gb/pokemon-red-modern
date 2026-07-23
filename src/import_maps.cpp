#include "import_maps.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
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

struct MapProfile {
    std::uint8_t id;
    std::string_view key;
    std::uint8_t width_blocks;
    std::uint8_t height_blocks;
};

constexpr std::array<MapProfile, 4> kMapProfiles{{
    {0x00, "pallet_town", 10, 9},
    {0x0C, "route_1", 10, 18},
    {0x01, "viridian_city", 20, 18},
    {0x21, "route_22", 20, 9},
}};

struct ImportedMap {
    const MapProfile* profile{};
    std::uint8_t header_bank{};
    std::uint8_t tileset_id{};
    std::uint8_t width_blocks{};
    std::uint8_t height_blocks{};
    std::uint16_t blocks_pointer{};
    std::size_t header_offset{};
    std::size_t blocks_offset{};
    std::vector<std::uint8_t> blocks;
    std::vector<std::uint8_t> tiles;
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
    std::vector<std::uint8_t> block_tiles;
    std::vector<std::uint8_t> pixels;
};

bool has_range(std::span<const std::uint8_t> rom, std::size_t offset,
               std::size_t size) {
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

void add_text_file(MapImport& result, std::string path, std::string text) {
    GeneratedFile file;
    file.relative_path = std::move(path);
    file.bytes.assign(text.begin(), text.end());
    result.files.push_back(std::move(file));
}

bool decode_map(std::span<const std::uint8_t> rom, const MapProfile& profile,
                ImportedMap& result, std::string& error) {
    const std::size_t pointer_record =
        kMapHeaderPointersOffset + static_cast<std::size_t>(profile.id) * 2U;
    const std::size_t bank_record =
        kMapHeaderBanksOffset + static_cast<std::size_t>(profile.id);
    if (!has_range(rom, pointer_record, 2) || !has_range(rom, bank_record, 1)) {
        error = "map lookup record extends outside the verified ROM";
        return false;
    }

    ImportedMap map;
    map.profile = &profile;
    map.header_bank = rom[bank_record];
    const std::uint16_t header_pointer = read_u16(rom, pointer_record);
    if (!bank_pointer_to_offset(rom, map.header_bank, header_pointer,
                                map.header_offset) ||
        !has_range(rom, map.header_offset, 10)) {
        error = "map header is outside its declared bank";
        return false;
    }

    map.tileset_id = rom[map.header_offset];
    map.height_blocks = rom[map.header_offset + 1U];
    map.width_blocks = rom[map.header_offset + 2U];
    map.blocks_pointer = read_u16(rom, map.header_offset + 3U);
    if (map.width_blocks != profile.width_blocks ||
        map.height_blocks != profile.height_blocks) {
        error = "map dimensions do not match the verified profile";
        return false;
    }

    const std::size_t block_count =
        static_cast<std::size_t>(map.width_blocks) * map.height_blocks;
    if (!bank_pointer_to_offset(rom, map.header_bank, map.blocks_pointer,
                                map.blocks_offset) ||
        !has_range(rom, map.blocks_offset, block_count)) {
        error = "map block grid is outside its declared bank";
        return false;
    }
    map.blocks.assign(rom.begin() + static_cast<std::ptrdiff_t>(map.blocks_offset),
                      rom.begin() +
                          static_cast<std::ptrdiff_t>(map.blocks_offset + block_count));
    result = std::move(map);
    return true;
}

bool decode_tileset(std::span<const std::uint8_t> rom,
                    const std::vector<ImportedMap>& maps, ImportedTileset& result,
                    std::string& error) {
    if (maps.empty()) {
        error = "map slice contains no maps";
        return false;
    }
    const std::uint8_t tileset_id = maps.front().tileset_id;
    for (const ImportedMap& map : maps) {
        if (map.tileset_id != tileset_id) {
            error = "initial map slice unexpectedly spans multiple tilesets";
            return false;
        }
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
    if (!bank_pointer_to_offset(rom, tileset.bank, tileset.blocks_pointer,
                                tileset.blocks_offset) ||
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
    for (const ImportedMap& map : maps) {
        const auto found = std::max_element(map.blocks.begin(), map.blocks.end());
        if (found != map.blocks.end()) maximum_block = std::max(maximum_block, *found);
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
            const std::uint8_t low =
                rom[tileset.graphics_offset + tile * 16U + y * 2U];
            const std::uint8_t high =
                rom[tileset.graphics_offset + tile * 16U + y * 2U + 1U];
            for (std::size_t x = 0; x < 8; ++x) {
                const unsigned bit = static_cast<unsigned>(7U - x);
                tileset.pixels[tile * 64U + y * 8U + x] =
                    static_cast<std::uint8_t>(((high >> bit) & 1U) << 1U |
                                              ((low >> bit) & 1U));
            }
        }
    }
    result = std::move(tileset);
    return true;
}

bool expand_maps(const ImportedTileset& tileset, std::vector<ImportedMap>& maps,
                 std::string& error) {
    for (ImportedMap& map : maps) {
        const std::size_t width = static_cast<std::size_t>(map.width_blocks) * 4U;
        const std::size_t height = static_cast<std::size_t>(map.height_blocks) * 4U;
        map.tiles.resize(width * height);
        for (std::size_t block_y = 0; block_y < map.height_blocks; ++block_y) {
            for (std::size_t block_x = 0; block_x < map.width_blocks; ++block_x) {
                const std::size_t map_block =
                    block_y * map.width_blocks + block_x;
                const std::size_t definition =
                    static_cast<std::size_t>(map.blocks[map_block]) * kTilesPerBlock;
                if (definition + kTilesPerBlock > tileset.block_tiles.size()) {
                    error = "map references a block outside the decoded tileset";
                    return false;
                }
                for (std::size_t tile_y = 0; tile_y < 4; ++tile_y) {
                    for (std::size_t tile_x = 0; tile_x < 4; ++tile_x) {
                        const std::size_t destination =
                            (block_y * 4U + tile_y) * width +
                            block_x * 4U + tile_x;
                        map.tiles[destination] =
                            tileset.block_tiles[definition + tile_y * 4U + tile_x];
                    }
                }
            }
        }
    }
    return true;
}

void emit_readable_source(const ImportedTileset& tileset,
                          const std::vector<ImportedMap>& maps, MapImport& result) {
    std::ostringstream tileset_source;
    tileset_source
        << "; Locally decoded outdoor tileset used by the initial map browser.\n"
        << "tileset tileset_" << static_cast<unsigned>(tileset.id) << '\n'
        << "    rom_id " << static_cast<unsigned>(tileset.id) << '\n'
        << "    tile_count " << tileset.pixels.size() / 64U << '\n'
        << "    block_count " << tileset.block_count << '\n'
        << "    graphics_source " << tileset.graphics_offset << ' '
        << tileset.blocks_offset - tileset.graphics_offset << '\n'
        << "    block_source " << tileset.blocks_offset << ' '
        << tileset.block_tiles.size() << '\n';
    add_text_file(result, "source/world/tilesets/tileset_00.sexpr",
                  tileset_source.str());

    for (const ImportedMap& map : maps) {
        const std::size_t width_tiles =
            static_cast<std::size_t>(map.width_blocks) * 4U;
        const std::size_t height_tiles =
            static_cast<std::size_t>(map.height_blocks) * 4U;
        std::ostringstream source;
        source << "; Complete block grid retained for readable local inspection.\n"
               << "map " << map.profile->key << '\n'
               << "    rom_id " << static_cast<unsigned>(map.profile->id) << '\n'
               << "    tileset tileset_" << static_cast<unsigned>(map.tileset_id)
               << '\n'
               << "    size_blocks " << static_cast<unsigned>(map.width_blocks) << ' '
               << static_cast<unsigned>(map.height_blocks) << '\n'
               << "    size_tiles " << width_tiles << ' ' << height_tiles << '\n'
               << "    header_source " << map.header_offset << '\n'
               << "    block_source " << map.blocks_offset << ' '
               << map.blocks.size() << '\n'
               << "    blocks\n";
        for (std::size_t y = 0; y < map.height_blocks; ++y) {
            source << "        row";
            for (std::size_t x = 0; x < map.width_blocks; ++x)
                source << ' ' << static_cast<unsigned>(
                    map.blocks[y * map.width_blocks + x]);
            source << '\n';
        }
        add_text_file(result,
                      "source/world/maps/" +
                          std::to_string(static_cast<unsigned>(map.profile->id)) +
                          "_" + std::string(map.profile->key) + ".sexpr",
                      source.str());
    }
}

void emit_runtime_cache(const ImportedTileset& tileset,
                        const std::vector<ImportedMap>& maps, MapImport& result) {
    std::vector<std::uint8_t> bytes{'P', 'M', 'V', '1'};
    write_u16(bytes, 1);
    bytes.push_back(tileset.id);
    write_u16(bytes, tileset.pixels.size() / 64U);
    write_u32(bytes, tileset.pixels.size());
    bytes.insert(bytes.end(), tileset.pixels.begin(), tileset.pixels.end());

    write_u16(bytes, maps.size());
    for (const ImportedMap& map : maps) {
        const std::string_view key = map.profile->key;
        bytes.push_back(map.profile->id);
        bytes.push_back(map.tileset_id);
        bytes.push_back(map.width_blocks);
        bytes.push_back(map.height_blocks);
        write_u16(bytes, static_cast<std::size_t>(map.width_blocks) * 4U);
        write_u16(bytes, static_cast<std::size_t>(map.height_blocks) * 4U);
        bytes.push_back(static_cast<std::uint8_t>(key.size()));
        bytes.insert(bytes.end(), key.begin(), key.end());
        write_u32(bytes, map.tiles.size());
        bytes.insert(bytes.end(), map.tiles.begin(), map.tiles.end());
    }
    result.files.push_back({"compiled/map_browser.bin", std::move(bytes)});
}

} // namespace

bool decode_map_import(std::span<const std::uint8_t> rom, MapImport& result,
                       std::string& error) {
    result = {};
    if (!verify_pokemon_red_us_rev_0(rom, error)) return false;

    std::vector<ImportedMap> maps;
    maps.reserve(kMapProfiles.size());
    for (const MapProfile& profile : kMapProfiles) {
        ImportedMap map;
        if (!decode_map(rom, profile, map, error)) {
            error = std::string(profile.key) + ": " + error;
            return false;
        }
        maps.push_back(std::move(map));
    }

    ImportedTileset tileset;
    if (!decode_tileset(rom, maps, tileset, error) ||
        !expand_maps(tileset, maps, error))
        return false;

    emit_readable_source(tileset, maps, result);
    emit_runtime_cache(tileset, maps, result);
    result.maps = maps.size();
    result.tilesets = 1;
    for (const ImportedMap& map : maps) result.expanded_tiles += map.tiles.size();
    return true;
}

} // namespace pokered::import
