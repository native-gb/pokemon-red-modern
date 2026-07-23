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
    std::uint8_t animation_mode{};
    std::vector<std::uint8_t> pixels;
    std::vector<std::uint8_t> animation_pixels;
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

enum class WorldView {
    selected,
    world,
};

struct WorldState {
    std::filesystem::path source;
    std::vector<MapTileset> tilesets;
    std::vector<WorldMap> maps;
    std::size_t current{};
    WorldView view{WorldView::world};
    float zoom{1.0F};
    float target_zoom{1.0F};
    float camera_x{};
    float camera_y{};
    float target_camera_x{};
    float target_camera_y{};
    std::uint64_t animation_tick{};
    bool camera_initialized{};
    bool loaded{};
};

bool load_world(const std::filesystem::path& path, WorldState& result,
                std::string& error);
void select_next_map(WorldState& world);
void select_previous_map(WorldState& world);
void toggle_world_view(WorldState& world);
void zoom_world_view(WorldState& world, float factor);
void pan_world_view(WorldState& world, float x, float y);
void reset_world_view(WorldState& world);
void update_world_view(WorldState& world, double elapsed_seconds);
void step_world_animation(WorldState& world);
const WorldMap* selected_map(const WorldState& world);
const MapTileset* find_tileset(const WorldState& world, std::uint8_t id);
std::string_view selected_map_name(const WorldState& world);
std::string_view label(WorldView view);

} // namespace pokered
