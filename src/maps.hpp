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
    std::string display_name;
    std::int32_t global_x_tiles{};
    std::int32_t global_y_tiles{};
    std::uint16_t world_component{};
    std::vector<std::uint8_t> tiles;
};

enum class MapView {
    selected,
    world,
};

struct MapBrowser {
    std::filesystem::path source;
    std::vector<MapTileset> tilesets;
    std::vector<WorldMap> maps;
    std::size_t current{};
    MapView view{MapView::world};
    float zoom{1.0F};
    float pan_x{};
    float pan_y{};
    bool loaded{};
};

bool load_map_browser(const std::filesystem::path& path, MapBrowser& result,
                      std::string& error);
void next_map(MapBrowser& browser);
void previous_map(MapBrowser& browser);
void toggle_map_view(MapBrowser& browser);
void zoom_map_view(MapBrowser& browser, float factor);
void pan_map_view(MapBrowser& browser, float x, float y);
void reset_map_view(MapBrowser& browser);
const WorldMap* current_map(const MapBrowser& browser);
const MapTileset* find_tileset(const MapBrowser& browser, std::uint8_t id);
std::string_view current_map_name(const MapBrowser& browser);
std::string_view label(MapView view);

} // namespace pokered
