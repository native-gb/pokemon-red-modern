#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace pokered {

struct MapTileset {
    std::uint8_t id{};
    std::uint16_t tile_count{};
    std::vector<std::uint8_t> pixels;
};

struct WorldMap {
    std::uint8_t id{};
    std::uint8_t tileset_id{};
    std::uint8_t width_blocks{};
    std::uint8_t height_blocks{};
    std::uint16_t width_tiles{};
    std::uint16_t height_tiles{};
    std::string key;
    std::vector<std::uint8_t> tiles;
};

struct MapBrowser {
    std::filesystem::path source;
    std::vector<MapTileset> tilesets;
    std::vector<WorldMap> maps;
    std::size_t current{};
    bool loaded{};
};

bool load_map_browser(const std::filesystem::path& path, MapBrowser& result,
                      std::string& error);
void next_map(MapBrowser& browser);
void previous_map(MapBrowser& browser);
const WorldMap* current_map(const MapBrowser& browser);
const MapTileset* find_tileset(const MapBrowser& browser, std::uint8_t id);
std::string_view current_map_name(const MapBrowser& browser);

} // namespace pokered
