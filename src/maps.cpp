#include "maps.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace pokered {
namespace {

bool read_u8(std::istream& input, std::uint8_t& result) {
    char byte = 0;
    if (!input.get(byte)) return false;
    result = static_cast<std::uint8_t>(static_cast<unsigned char>(byte));
    return true;
}

bool read_u16(std::istream& input, std::uint16_t& result) {
    std::array<std::uint8_t, 2> bytes{};
    char first = 0;
    char second = 0;
    if (!input.get(first) || !input.get(second)) return false;
    bytes[0] = static_cast<std::uint8_t>(static_cast<unsigned char>(first));
    bytes[1] = static_cast<std::uint8_t>(static_cast<unsigned char>(second));
    result = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[0]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8U));
    return true;
}

bool read_u32(std::istream& input, std::uint32_t& result) {
    std::array<std::uint8_t, 4> bytes{};
    for (std::uint8_t& byte : bytes) {
        char value = 0;
        if (!input.get(value)) return false;
        byte = static_cast<std::uint8_t>(static_cast<unsigned char>(value));
    }
    result = static_cast<std::uint32_t>(bytes[0]) |
             static_cast<std::uint32_t>(bytes[1]) << 8U |
             static_cast<std::uint32_t>(bytes[2]) << 16U |
             static_cast<std::uint32_t>(bytes[3]) << 24U;
    return true;
}

bool read_bytes(std::istream& input, std::size_t count,
                std::vector<std::uint8_t>& result) {
    result.resize(count);
    return input.read(reinterpret_cast<char*>(result.data()),
                      static_cast<std::streamsize>(result.size()))
        .good();
}

bool read_tilesets(std::istream& input, MapBrowser& browser, std::string& error) {
    std::uint16_t count = 0;
    if (!read_u16(input, count) || count == 0 || count > 24) {
        error = "map browser cache has an invalid tileset count";
        return false;
    }
    browser.tilesets.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        MapTileset tileset;
        std::uint32_t pixel_count = 0;
        if (!read_u8(input, tileset.id) || !read_u16(input, tileset.tile_count) ||
            !read_u32(input, pixel_count) || tileset.tile_count == 0 ||
            pixel_count != static_cast<std::uint32_t>(tileset.tile_count) * 64U ||
            !read_bytes(input, pixel_count, tileset.pixels)) {
            error = "map browser cache has an invalid tileset record";
            return false;
        }
        const auto duplicate =
            std::find_if(browser.tilesets.begin(), browser.tilesets.end(),
                         [&](const MapTileset& value) { return value.id == tileset.id; });
        if (duplicate != browser.tilesets.end()) {
            error = "map browser cache repeats a tileset ID";
            return false;
        }
        browser.tilesets.push_back(std::move(tileset));
    }
    return true;
}

bool read_map(std::istream& input, const MapBrowser& browser, WorldMap& map,
              std::string& error) {
    std::uint8_t key_size = 0;
    std::uint32_t tile_count = 0;
    if (!read_u8(input, map.id) || !read_u8(input, map.tileset_id) ||
        !read_u8(input, map.width_blocks) || !read_u8(input, map.height_blocks) ||
        !read_u16(input, map.width_tiles) || !read_u16(input, map.height_tiles) ||
        !read_u8(input, key_size) || key_size == 0 || key_size > 63) {
        error = "map browser cache has an invalid map header";
        return false;
    }
    map.key.resize(key_size);
    if (!input.read(map.key.data(), static_cast<std::streamsize>(map.key.size())) ||
        !read_u32(input, tile_count)) {
        error = "map browser cache has a truncated map record";
        return false;
    }
    const std::uint32_t expected =
        static_cast<std::uint32_t>(map.width_tiles) * map.height_tiles;
    if (map.width_blocks == 0 || map.height_blocks == 0 ||
        map.width_tiles != static_cast<std::uint16_t>(map.width_blocks * 4U) ||
        map.height_tiles != static_cast<std::uint16_t>(map.height_blocks * 4U) ||
        tile_count != expected || find_tileset(browser, map.tileset_id) == nullptr ||
        !read_bytes(input, tile_count, map.tiles)) {
        error = "map browser cache has invalid map dimensions or tile data";
        return false;
    }
    const MapTileset* tileset = find_tileset(browser, map.tileset_id);
    const auto invalid_tile =
        std::find_if(map.tiles.begin(), map.tiles.end(),
                     [&](std::uint8_t tile) { return tile >= tileset->tile_count; });
    if (invalid_tile != map.tiles.end()) {
        error = "map browser cache references a tile outside its tileset";
        return false;
    }
    return true;
}

} // namespace

bool load_map_browser(const std::filesystem::path& path, MapBrowser& result,
                      std::string& error) {
    std::ifstream input(path, std::ios::binary);
    std::array<char, 4> magic{};
    if (!input.read(magic.data(), static_cast<std::streamsize>(magic.size())) ||
        magic != std::array{'P', 'M', 'V', '1'}) {
        error = "map browser cache is missing or has an invalid header";
        return false;
    }

    MapBrowser loaded;
    loaded.source = path;
    if (!read_tilesets(input, loaded, error)) return false;
    std::uint16_t map_count = 0;
    if (!read_u16(input, map_count) || map_count == 0 || map_count > 248) {
        error = "map browser cache has an invalid map count";
        return false;
    }
    loaded.maps.reserve(map_count);
    for (std::uint16_t index = 0; index < map_count; ++index) {
        WorldMap map;
        if (!read_map(input, loaded, map, error)) return false;
        const auto duplicate =
            std::find_if(loaded.maps.begin(), loaded.maps.end(),
                         [&](const WorldMap& value) { return value.id == map.id; });
        if (duplicate != loaded.maps.end()) {
            error = "map browser cache repeats a map ID";
            return false;
        }
        loaded.maps.push_back(std::move(map));
    }
    if (input.peek() != std::char_traits<char>::eof()) {
        error = "map browser cache contains trailing data";
        return false;
    }
    loaded.loaded = true;
    result = std::move(loaded);
    error.clear();
    return true;
}

void next_map(MapBrowser& browser) {
    if (!browser.maps.empty()) browser.current = (browser.current + 1U) % browser.maps.size();
}

void previous_map(MapBrowser& browser) {
    if (browser.maps.empty()) return;
    browser.current = browser.current == 0 ? browser.maps.size() - 1U : browser.current - 1U;
}

const WorldMap* current_map(const MapBrowser& browser) {
    if (browser.maps.empty() || browser.current >= browser.maps.size()) return nullptr;
    return &browser.maps[browser.current];
}

const MapTileset* find_tileset(const MapBrowser& browser, std::uint8_t id) {
    const auto found =
        std::find_if(browser.tilesets.begin(), browser.tilesets.end(),
                     [&](const MapTileset& tileset) { return tileset.id == id; });
    return found == browser.tilesets.end() ? nullptr : &*found;
}

std::string_view current_map_name(const MapBrowser& browser) {
    const WorldMap* map = current_map(browser);
    return map == nullptr ? std::string_view("none") : std::string_view(map->key);
}

} // namespace pokered
