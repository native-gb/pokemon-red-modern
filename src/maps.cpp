#include "maps.hpp"
#include "interactions.hpp"
#include "state.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <queue>
#include <string>
#include <string_view>
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
    result = static_cast<std::uint32_t>(bytes[0]) | static_cast<std::uint32_t>(bytes[1]) << 8U |
             static_cast<std::uint32_t>(bytes[2]) << 16U |
             static_cast<std::uint32_t>(bytes[3]) << 24U;
    return true;
}

bool read_i32(std::istream& input, std::int32_t& result) {
    std::uint32_t bits = 0;
    if (!read_u32(input, bits)) return false;
    result = static_cast<std::int32_t>(bits);
    return true;
}

bool read_bytes(std::istream& input, std::size_t count, std::vector<std::uint8_t>& result) {
    result.resize(count);
    return input
        .read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size()))
        .good();
}

bool read_tilesets(std::istream& input, WorldState& world, std::string& error) {
    std::uint16_t count = 0;
    if (!read_u16(input, count) || count == 0 || count > 24) {
        error = "world map cache has an invalid tileset count";
        return false;
    }
    world.tilesets.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        MapTileset tileset;
        std::uint32_t pixel_count = 0;
        std::uint32_t animation_pixel_count = 0;
        std::uint8_t passable_count = 0;
        if (!read_u8(input, tileset.id) || !read_u16(input, tileset.tile_count) ||
            !read_u8(input, tileset.grass_tile) ||
            !read_u8(input, tileset.animation_mode) || tileset.animation_mode > 2 ||
            !read_u8(input, passable_count) || passable_count == 0 ||
            !read_bytes(input, passable_count, tileset.passable_tiles) ||
            !read_u32(input, pixel_count) || tileset.tile_count == 0 ||
            pixel_count != static_cast<std::uint32_t>(tileset.tile_count) * 64U ||
            !read_bytes(input, pixel_count, tileset.pixels) ||
            !read_u32(input, animation_pixel_count) ||
            animation_pixel_count != (tileset.animation_mode == 0   ? 0U
                                      : tileset.animation_mode == 1 ? 8U * 64U
                                                                    : 11U * 64U) ||
            !read_bytes(input, animation_pixel_count, tileset.animation_pixels)) {
            error = "world map cache has an invalid tileset record";
            return false;
        }
        const auto duplicate =
            std::find_if(world.tilesets.begin(), world.tilesets.end(),
                         [&](const MapTileset& value) { return value.id == tileset.id; });
        if (duplicate != world.tilesets.end()) {
            error = "world map cache repeats a tileset ID";
            return false;
        }
        world.tilesets.push_back(std::move(tileset));
    }
    return true;
}

bool read_sprites(std::istream& input, WorldState& world, std::string& error) {
    std::uint16_t count = 0;
    if (!read_u16(input, count) || count == 0 || count > 256) {
        error = "world map cache has an invalid overworld sprite count";
        return false;
    }
    world.sprites.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        WorldSprite sprite;
        std::uint8_t key_size = 0;
        std::uint8_t still = 0;
        std::uint32_t pixel_count = 0;
        if (!read_u8(input, sprite.id) || sprite.id == 0 || !read_u8(input, key_size) ||
            key_size == 0 || key_size > 63) {
            error = "world map cache has an invalid overworld sprite key";
            return false;
        }
        sprite.key.resize(key_size);
        if (!input.read(sprite.key.data(), static_cast<std::streamsize>(sprite.key.size())) ||
            !read_u8(input, still) || still > 1 ||
            !read_u32(input, pixel_count) ||
            pixel_count != 16U * 16U * 16U ||
            !read_bytes(input, pixel_count, sprite.pixels)) {
            error = "world map cache has an invalid overworld sprite record";
            return false;
        }
        sprite.still = still != 0;
        if (find_world_sprite(world, sprite.id) != nullptr) {
            error = "world map cache repeats an overworld sprite ID";
            return false;
        }
        const auto duplicate_key =
            std::find_if(world.sprites.begin(), world.sprites.end(),
                         [&](const WorldSprite& value) { return value.key == sprite.key; });
        if (duplicate_key != world.sprites.end()) {
            error = "world map cache repeats an overworld sprite key";
            return false;
        }
        world.sprites.push_back(std::move(sprite));
    }
    return true;
}

bool read_world_spaces(std::istream& input, WorldState& world, std::string& error) {
    std::uint16_t count = 0;
    if (!read_u16(input, count) || count == 0 || count > 226) {
        error = "world map cache has an invalid world-space count";
        return false;
    }
    world.spaces.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        WorldSpace space;
        std::uint8_t key_size = 0;
        std::uint8_t outdoor = 0;
        if (!read_u16(input, space.id) || space.id != index || !read_u8(input, key_size) ||
            key_size == 0 || key_size > 63) {
            error = "world map cache has an invalid world-space record";
            return false;
        }
        space.key.resize(key_size);
        if (!input.read(space.key.data(), static_cast<std::streamsize>(space.key.size())) ||
            !read_u8(input, outdoor) || outdoor > 1) {
            error = "world map cache has a truncated world-space record";
            return false;
        }
        space.outdoor = outdoor != 0;
        const auto duplicate =
            std::find_if(world.spaces.begin(), world.spaces.end(),
                         [&](const WorldSpace& value) { return value.key == space.key; });
        if (duplicate != world.spaces.end()) {
            error = "world map cache repeats a world-space key";
            return false;
        }
        world.spaces.push_back(std::move(space));
    }
    return true;
}

bool read_ledges(std::istream& input, WorldState& world,
                 std::string& error) {
    std::uint8_t count = 0U;
    if (!read_u8(input, count) || count == 0U || count > 32U) {
        error = "world map cache has an invalid ledge count";
        return false;
    }
    world.ledges.reserve(count);
    for (std::uint8_t index = 0U; index < count; ++index) {
        std::uint8_t direction = 0U;
        WorldLedge ledge;
        if (!read_u8(input, direction) || direction > 3U ||
            !read_u8(input, ledge.standing_tile) ||
            !read_u8(input, ledge.ledge_tile) ||
            !read_u8(input, ledge.required_input_mask)) {
            error = "world map cache has an invalid ledge record";
            return false;
        }
        ledge.direction = static_cast<WorldDirection>(direction);
        world.ledges.push_back(ledge);
    }
    return true;
}

bool read_map(std::istream& input, const WorldState& world, WorldMap& map, std::string& error) {
    std::uint8_t key_size = 0;
    std::uint8_t name_size = 0;
    std::uint8_t camera_framing = 0U;
    std::uint16_t camera_zoom = 0U;
    std::uint32_t tile_count = 0;
    if (!read_u8(input, map.id) || !read_u8(input, map.tileset_id) ||
        !read_u8(input, map.width_blocks) || !read_u8(input, map.height_blocks) ||
        !read_u16(input, map.width_tiles) || !read_u16(input, map.height_tiles) ||
        !read_u8(input, key_size) || key_size == 0 || key_size > 63) {
        error = "world map cache has an invalid map header";
        return false;
    }
    map.key.resize(key_size);
    if (!input.read(map.key.data(), static_cast<std::streamsize>(map.key.size())) ||
        !read_u8(input, name_size) || name_size == 0 || name_size > 63) {
        error = "world map cache has a truncated map record";
        return false;
    }
    map.display_name.resize(name_size);
    if (!input.read(map.display_name.data(),
                    static_cast<std::streamsize>(map.display_name.size())) ||
        !read_i32(input, map.global_x_tiles) || !read_i32(input, map.global_y_tiles) ||
        !read_u16(input, map.world_space) || map.world_space >= world.spaces.size() ||
        !read_u8(input, camera_framing) ||
        camera_framing >
            static_cast<std::uint8_t>(
                WorldCameraFraming::fit_space) ||
        !read_u16(input, camera_zoom) ||
        camera_zoom < 5U || camera_zoom > 6400U ||
        !read_u32(input, tile_count)) {
        error = "world map cache has truncated map placement or tile data";
        return false;
    }
    map.camera_framing =
        static_cast<WorldCameraFraming>(camera_framing);
    map.camera_zoom =
        static_cast<float>(camera_zoom) / 100.0F;
    const std::uint32_t expected = static_cast<std::uint32_t>(map.width_tiles) * map.height_tiles;
    if (map.width_blocks == 0 || map.height_blocks == 0 ||
        map.width_tiles != static_cast<std::uint16_t>(map.width_blocks * 4U) ||
        map.height_tiles != static_cast<std::uint16_t>(map.height_blocks * 4U) ||
        tile_count != expected || find_tileset(world, map.tileset_id) == nullptr ||
        !read_bytes(input, tile_count, map.tiles)) {
        error = "world map cache has invalid map dimensions or tile data";
        return false;
    }
    const MapTileset* tileset = find_tileset(world, map.tileset_id);
    const auto invalid_tile =
        std::find_if(map.tiles.begin(), map.tiles.end(),
                     [&](std::uint8_t tile) { return tile >= tileset->tile_count; });
    if (invalid_tile != map.tiles.end()) {
        error = "world map cache references a tile outside its tileset";
        return false;
    }

    std::uint8_t warp_count = 0;
    if (!read_u8(input, warp_count) || warp_count > 32) {
        error = "world map cache has an invalid warp count";
        return false;
    }
    map.warps.reserve(warp_count);
    for (std::uint8_t index = 0; index < warp_count; ++index) {
        WorldWarp warp;
        if (!read_u8(input, warp.index) || !read_u8(input, warp.x) || !read_u8(input, warp.y) ||
            !read_u8(input, warp.destination_map_id) ||
            !read_u8(input, warp.destination_warp_index) ||
            warp.index != static_cast<std::uint8_t>(index + 1U) ||
            warp.x >= map.width_blocks * 2U || warp.y >= map.height_blocks * 2U) {
            error = "world map cache has an invalid warp record";
            return false;
        }
        map.warps.push_back(warp);
    }

    std::uint8_t actor_count = 0;
    if (!read_u8(input, actor_count) || actor_count > 16) {
        error = "world map cache has an invalid actor count";
        return false;
    }
    map.actors.reserve(actor_count);
    for (std::uint8_t index = 0; index < actor_count; ++index) {
        WorldActorSpawn actor;
        std::uint8_t kind = 0;
        std::uint8_t has_movement_bounds = 0;
        if (!read_u8(input, actor.index) || !read_u8(input, actor.sprite_id) ||
            !read_u8(input, actor.x) || !read_u8(input, actor.y) ||
            !read_u8(input, actor.movement) || !read_u8(input, actor.direction_or_axis) ||
            !read_u8(input, actor.text_id) || !read_u8(input, actor.parameter_a) ||
            !read_u8(input, actor.parameter_b) || !read_u8(input, kind) ||
            !read_u8(input, has_movement_bounds) || has_movement_bounds > 1 ||
            actor.index != static_cast<std::uint8_t>(index + 1U) || kind > 2 ||
            actor.x >= map.width_blocks * 2U || actor.y >= map.height_blocks * 2U ||
            find_world_sprite(world, actor.sprite_id) == nullptr) {
            error = "world map cache has an invalid actor record";
            return false;
        }
        actor.kind = static_cast<WorldActorKind>(kind);
        if (has_movement_bounds != 0) {
            WorldMovementBounds bounds;
            std::uint16_t width = 0;
            std::uint16_t height = 0;
            if (!read_i32(input, bounds.x) || !read_i32(input, bounds.y) ||
                !read_u16(input, width) || !read_u16(input, height) || width == 0 || height == 0) {
                error = "world map cache has invalid actor movement bounds";
                return false;
            }
            bounds.width = width;
            bounds.height = height;
            actor.movement_bounds = bounds;
        }
        const bool should_roam = actor.movement == 0xFEU;
        const std::int32_t expected_x = map.global_x_tiles / 2;
        const std::int32_t expected_y = map.global_y_tiles / 2;
        if (should_roam != actor.movement_bounds.has_value() ||
            (actor.movement_bounds.has_value() &&
             (actor.movement_bounds->x != expected_x || actor.movement_bounds->y != expected_y ||
              actor.movement_bounds->width != map.width_blocks * 2 ||
              actor.movement_bounds->height != map.height_blocks * 2))) {
            error = "world map cache actor movement bounds disagree with its authored map";
            return false;
        }
        map.actors.push_back(actor);
    }
    return true;
}

void selected_view_bounds(const WorldState& world, float& left, float& top, float& right,
                          float& bottom) {
    const WorldMap* map = selected_map(world);
    if (map == nullptr) {
        left = top = right = bottom = 0.0F;
        return;
    }
    left = static_cast<float>(map->global_x_tiles) * 8.0F;
    top = static_cast<float>(map->global_y_tiles) * 8.0F;
    right = left + static_cast<float>(map->width_tiles) * 8.0F;
    bottom = top + static_cast<float>(map->height_tiles) * 8.0F;
}

void world_view_bounds(const WorldState& world, float& left, float& top, float& right,
                       float& bottom) {
    const auto first =
        std::find_if(world.maps.begin(), world.maps.end(), [&](const WorldMap& map) {
            return map.world_space == world.current_space;
        });
    if (first == world.maps.end()) {
        left = top = right = bottom = 0.0F;
        return;
    }
    left = static_cast<float>(first->global_x_tiles) * 8.0F;
    top = static_cast<float>(first->global_y_tiles) * 8.0F;
    right = left + static_cast<float>(first->width_tiles) * 8.0F;
    bottom = top + static_cast<float>(first->height_tiles) * 8.0F;
    for (const WorldMap& map : world.maps) {
        if (map.world_space != world.current_space) continue;
        const float map_left = static_cast<float>(map.global_x_tiles) * 8.0F;
        const float map_top = static_cast<float>(map.global_y_tiles) * 8.0F;
        left = std::min(left, map_left);
        top = std::min(top, map_top);
        right = std::max(right, map_left + static_cast<float>(map.width_tiles) * 8.0F);
        bottom = std::max(bottom, map_top + static_cast<float>(map.height_tiles) * 8.0F);
    }
}

std::size_t cell_offset(const WorldMapCellIndex& index, std::int32_t x, std::int32_t y) {
    return static_cast<std::size_t>(y) * index.width + static_cast<std::size_t>(x);
}

bool inside(const WorldMapCellIndex& index, std::int32_t x, std::int32_t y) {
    return x >= 0 && y >= 0 && x < index.width && y < index.height;
}

WorldDirection imported_facing(std::uint8_t direction) {
    if (direction == 0xD1U) return WorldDirection::up;
    if (direction == 0xD2U) return WorldDirection::left;
    if (direction == 0xD3U) return WorldDirection::right;
    return WorldDirection::down;
}

WorldDirection opposite(WorldDirection direction) {
    if (direction == WorldDirection::up) return WorldDirection::down;
    if (direction == WorldDirection::down) return WorldDirection::up;
    if (direction == WorldDirection::left) return WorldDirection::right;
    return WorldDirection::left;
}

void direction_delta(WorldDirection direction, std::int32_t& x, std::int32_t& y) {
    x = direction == WorldDirection::left ? -1 : direction == WorldDirection::right ? 1 : 0;
    y = direction == WorldDirection::up ? -1 : direction == WorldDirection::down ? 1 : 0;
}

bool map_contains_global_cell(const WorldMap& map, std::int32_t x, std::int32_t y) {
    const std::int32_t left = map.global_x_tiles / 2;
    const std::int32_t top = map.global_y_tiles / 2;
    return x >= left && y >= top && x < left + map.width_blocks * 2 &&
           y < top + map.height_blocks * 2;
}

std::size_t find_map_for_global_cell(const WorldState& world, std::size_t preferred,
                                     std::int32_t x, std::int32_t y) {
    if (preferred < world.maps.size() &&
        world.maps[preferred].world_space == world.current_space &&
        map_contains_global_cell(world.maps[preferred], x, y))
        return preferred;
    for (std::size_t index = 0; index < world.maps.size(); ++index) {
        if (world.maps[index].world_space == world.current_space &&
            map_contains_global_cell(world.maps[index], x, y))
            return index;
    }
    return world.maps.size();
}

std::size_t find_map_by_id(const WorldState& world, std::uint8_t id) {
    const auto found = std::find_if(world.maps.begin(), world.maps.end(),
                                    [id](const WorldMap& map) { return map.id == id; });
    return found == world.maps.end()
               ? world.maps.size()
               : static_cast<std::size_t>(found - world.maps.begin());
}

const WorldWarp* warp_at(const WorldMap& map, std::int32_t x, std::int32_t y) {
    const auto found = std::find_if(map.warps.begin(), map.warps.end(),
                                    [x, y](const WorldWarp& warp) {
                                        return warp.x == x && warp.y == y;
                                    });
    return found == map.warps.end() ? nullptr : &*found;
}

void show_area_banner(WorldState& world, std::size_t map_index);

bool activate_world_warp(WorldState& world) {
    if (world.player.map_index >= world.maps.size()) return false;
    const std::size_t source_index = world.player.map_index;
    const WorldMap& source = world.maps[source_index];
    const WorldWarp* source_warp = warp_at(source, world.player.x, world.player.y);
    if (source_warp == nullptr) return false;

    const std::size_t destination_index =
        source_warp->destination_map_id == 0xFFU
            ? world.player.last_outdoor_map_index
            : find_map_by_id(world, source_warp->destination_map_id);
    if (destination_index >= world.maps.size()) return false;
    const WorldMap& destination = world.maps[destination_index];
    if (source_warp->destination_map_id != 0xFFU &&
        source.world_space < world.spaces.size() &&
        world.spaces[source.world_space].outdoor &&
        source.world_space != destination.world_space)
        world.player.last_outdoor_map_index = source_index;
    if (source_warp->destination_warp_index >= destination.warps.size()) return false;
    const WorldWarp& destination_warp =
        destination.warps[source_warp->destination_warp_index];

    world.player.map_index = destination_index;
    world.player.x = destination_warp.x;
    world.player.y = destination_warp.y;
    world.player.move_cooldown = 15U;
    world.current = destination_index;
    world.current_space = destination.world_space;
    world.camera_region_dirty = true;
    world.dialogue = {};
    world.player.visual_global_x =
        static_cast<float>(destination.global_x_tiles / 2 + world.player.x);
    world.player.visual_global_y =
        static_cast<float>(destination.global_y_tiles / 2 + world.player.y);
    world.player.movement_queue.clear();
    world.player.moving = false;
    world.player.ledge_hop = false;
    world.player.warp_pending = false;
    world.player.visual_offset_y_pixels = 0.0F;
    world.player.animation_phase = 0U;
    world.camera_initialized = false;
    world.last_warp = {
        .source_map_id = source.id,
        .source_warp_index = source_warp->index,
        .destination_map_id = destination.id,
        .destination_warp_index = source_warp->destination_warp_index,
        .simulation_tick = world.simulation_tick,
        .occurred = true,
    };
    show_area_banner(world, destination_index);
    return true;
}

bool is_passable(const WorldState& world, std::size_t map_index, std::int32_t global_x,
                 std::int32_t global_y) {
    if (map_index >= world.maps.size()) return false;
    const WorldMap& map = world.maps[map_index];
    const std::int32_t local_x = global_x - map.global_x_tiles / 2;
    const std::int32_t local_y = global_y - map.global_y_tiles / 2;
    if (local_x < 0 || local_y < 0 || local_x >= map.width_blocks * 2 ||
        local_y >= map.height_blocks * 2)
        return false;
    const std::size_t tile_x = static_cast<std::size_t>(local_x) * 2U;
    const std::size_t tile_y = static_cast<std::size_t>(local_y) * 2U + 1U;
    const std::size_t offset = tile_y * map.width_tiles + tile_x;
    if (offset >= map.tiles.size()) return false;
    const MapTileset* tileset = find_tileset(world, map.tileset_id);
    return tileset != nullptr &&
           std::find(tileset->passable_tiles.begin(), tileset->passable_tiles.end(),
                     map.tiles[offset]) != tileset->passable_tiles.end();
}

std::optional<std::uint8_t> collision_tile(
    const WorldState& world, std::size_t map_index,
    std::int32_t global_x, std::int32_t global_y) {
    if (map_index >= world.maps.size()) return std::nullopt;
    const WorldMap& map = world.maps[map_index];
    const std::int32_t local_x =
        global_x - map.global_x_tiles / 2;
    const std::int32_t local_y =
        global_y - map.global_y_tiles / 2;
    if (local_x < 0 || local_y < 0 ||
        local_x >= map.width_blocks * 2 ||
        local_y >= map.height_blocks * 2)
        return std::nullopt;
    const std::size_t tile_x =
        static_cast<std::size_t>(local_x) * 2U;
    const std::size_t tile_y =
        static_cast<std::size_t>(local_y) * 2U + 1U;
    const std::size_t offset =
        tile_y * map.width_tiles + tile_x;
    if (offset >= map.tiles.size()) return std::nullopt;
    return map.tiles[offset];
}

void queue_player_segment(WorldState& world, float global_x,
                          float global_y, bool ledge_hop = false) {
    world.player.movement_queue.push_back({
        .target_x = global_x,
        .target_y = global_y,
        .ledge_hop = ledge_hop,
    });
}

void show_area_banner(WorldState& world, std::size_t map_index) {
    if (map_index >= world.maps.size()) return;
    world.area_banner = {
        .text = world.maps[map_index].display_name,
        .elapsed = 0.0F,
        .active = true,
    };
}

const WorldLedge* matching_ledge(
    const WorldState& world, std::size_t map_index,
    std::int32_t current_x, std::int32_t current_y,
    std::int32_t next_x, std::int32_t next_y,
    WorldDirection direction) {
    if (map_index >= world.maps.size() ||
        world.maps[map_index].tileset_id != 0U)
        return nullptr;
    const auto standing =
        collision_tile(world, map_index, current_x, current_y);
    const auto ledge =
        collision_tile(world, map_index, next_x, next_y);
    if (!standing.has_value() || !ledge.has_value())
        return nullptr;
    const auto found = std::ranges::find_if(
        world.ledges, [&](const WorldLedge& candidate) {
            return candidate.direction == direction &&
                   candidate.standing_tile == *standing &&
                   candidate.ledge_tile == *ledge;
        });
    return found == world.ledges.end() ? nullptr : &*found;
}

std::string substitute_campaign_text(std::string text,
                                     const CampaignState& campaign) {
    const auto replace_all = [&](std::string_view token, std::string_view replacement) {
        std::size_t position = 0;
        while ((position = text.find(token, position)) != std::string::npos) {
            text.replace(position, token.size(), replacement);
            position += replacement.size();
        }
    };
    replace_all("{player_name}", campaign.player_name);
    replace_all("{rival_name}", campaign.rival_name);
    return text;
}

void open_interaction(WorldState& world, CampaignState& campaign,
                      const InteractionProgram* program) {
    world.dialogue = {};
    if (program != nullptr &&
        program->builtin ==
            InteractionBuiltin::pokecenter_nurse &&
        program->pages.size() == 5U) {
        world.service = {
            .kind = WorldServiceKind::pokecenter_nurse,
            .pages = program->pages,
            .active = true,
        };
        world.dialogue.pages.push_back(program->pages[0]);
        world.dialogue.pages.push_back(program->pages[1]);
        campaign.used_pokemon_center = true;
        world.dialogue.open = true;
        world.choice = {};
    } else if (program != nullptr && program->status == InteractionProgramStatus::dialogue &&
        !program->pages.empty()) {
        for (const std::string& page : program->pages)
            world.dialogue.pages.push_back(
                substitute_campaign_text(page, campaign));
    } else if (program != nullptr &&
               program->status == InteractionProgramStatus::decoded_native) {
        world.dialogue.pages.push_back("[Decoded interaction executor pending]");
    } else {
        world.dialogue.pages.push_back("[Interaction needs semantic translation]");
    }
    world.dialogue.open = true;
}

std::uint32_t next_random(WorldState& world) {
    world.random_state = world.random_state * 1664525U + 1013904223U;
    return world.random_state;
}

} // namespace

std::uint8_t next_world_random_byte(WorldState& world) {
    return static_cast<std::uint8_t>(next_random(world) >> 24U);
}

bool load_world(const std::filesystem::path& path, WorldState& result, std::string& error) {
    std::ifstream input(path, std::ios::binary);
    std::array<char, 4> magic{};
    if (!input.read(magic.data(), static_cast<std::streamsize>(magic.size())) ||
        magic != std::array{'P', 'M', 'V', 'C'}) {
        error = "world map cache is missing or has an invalid header";
        return false;
    }

    WorldState loaded;
    loaded.source = path;
    if (!read_tilesets(input, loaded, error) || !read_sprites(input, loaded, error) ||
        !read_world_spaces(input, loaded, error) ||
        !read_ledges(input, loaded, error))
        return false;
    std::uint16_t map_count = 0;
    if (!read_u16(input, map_count) || map_count != 226) {
        error = "world map cache has an invalid map count";
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
            error = "world map cache repeats a map ID";
            return false;
        }
        loaded.maps.push_back(std::move(map));
    }
    if (input.peek() != std::char_traits<char>::eof()) {
        error = "world map cache contains trailing data";
        return false;
    }
    loaded.loaded = true;
    result = std::move(loaded);
    reset_world_view(result);
    error.clear();
    return true;
}

bool initialize_world_runtime(WorldState& world, const InteractionCatalog& interactions,
                              std::string& error) {
    if (!world.loaded || !interactions.loaded || world.maps.empty()) {
        error = "world runtime requires loaded maps and interactions";
        return false;
    }

    world.actors.clear();
    world.spatial.clear();
    world.roam_schedule.assign(256U, {});
    world.spatial.reserve(world.maps.size());
    for (const WorldMap& map : world.maps) {
        WorldMapCellIndex index;
        index.map_id = map.id;
        index.width = static_cast<std::uint16_t>(map.width_blocks * 2U);
        index.height = static_cast<std::uint16_t>(map.height_blocks * 2U);
        const std::size_t cells = static_cast<std::size_t>(index.width) * index.height;
        index.actor_by_cell.assign(cells, -1);
        index.background_program_by_cell.assign(cells, 0U);
        index.trainer_sight_actors_by_cell.assign(cells, {});
        if (const MapInteractions* bindings = find_map_interactions(interactions, map.id);
            bindings != nullptr) {
            for (const InteractionOwner& owner : bindings->backgrounds) {
                if (inside(index, owner.x, owner.y))
                    index.background_program_by_cell[cell_offset(index, owner.x, owner.y)] =
                        owner.program_id;
            }
        }
        world.spatial.push_back(std::move(index));
    }

    for (std::size_t map_index = 0; map_index < world.maps.size(); ++map_index) {
        const WorldMap& map = world.maps[map_index];
        WorldMapCellIndex& cells = world.spatial[map_index];
        for (std::size_t spawn_index = 0; spawn_index < map.actors.size(); ++spawn_index) {
            const WorldActorSpawn& spawn = map.actors[spawn_index];
            WorldActorState actor{
                .map_index = map_index,
                .spawn_index = spawn_index,
                .x = spawn.x,
                .y = spawn.y,
                .visual_global_x =
                    static_cast<float>(map.global_x_tiles / 2 + static_cast<int>(spawn.x)),
                .visual_global_y =
                    static_cast<float>(map.global_y_tiles / 2 + static_cast<int>(spawn.y)),
                .movement_from_x =
                    static_cast<float>(map.global_x_tiles / 2 + static_cast<int>(spawn.x)),
                .movement_from_y =
                    static_cast<float>(map.global_y_tiles / 2 + static_cast<int>(spawn.y)),
                .movement_to_x =
                    static_cast<float>(map.global_x_tiles / 2 + static_cast<int>(spawn.x)),
                .movement_to_y =
                    static_cast<float>(map.global_y_tiles / 2 + static_cast<int>(spawn.y)),
                .facing = imported_facing(spawn.direction_or_axis),
                .visible = true,
            };
            const std::size_t actor_index = world.actors.size();
            if (inside(cells, actor.x, actor.y))
                cells.actor_by_cell[cell_offset(cells, actor.x, actor.y)] =
                    static_cast<std::int32_t>(actor_index);
            world.actors.push_back(actor);
            if (spawn.movement == 0xFEU) {
                const std::size_t slot = (30U + actor_index * 17U) % world.roam_schedule.size();
                world.roam_schedule[slot].push_back(actor_index);
            }
        }
    }

    // Materialize map-local sight rays once. Runtime checks only the player's
    // current cell, while defeated-state predicates remain campaign-owned.
    for (std::size_t actor_index = 0U;
         actor_index < world.actors.size(); ++actor_index) {
        const WorldActorState& actor = world.actors[actor_index];
        const WorldMap& map = world.maps[actor.map_index];
        const WorldActorSpawn& spawn =
            map.actors[actor.spawn_index];
        const TrainerInteractionRule* trainer =
            find_trainer_interaction(interactions, map.id, spawn.index);
        if (trainer == nullptr || trainer->sight_range == 0U) continue;
        std::int32_t dx = 0;
        std::int32_t dy = 0;
        direction_delta(actor.facing, dx, dy);
        WorldMapCellIndex& cells = world.spatial[actor.map_index];
        for (std::uint8_t distance = 1U;
             distance <= trainer->sight_range; ++distance) {
            const std::int32_t x =
                actor.x + dx * static_cast<std::int32_t>(distance);
            const std::int32_t y =
                actor.y + dy * static_cast<std::int32_t>(distance);
            const std::int32_t global_x =
                map.global_x_tiles / 2 + x;
            const std::int32_t global_y =
                map.global_y_tiles / 2 + y;
            if (!inside(cells, x, y) ||
                !is_passable(world, actor.map_index, global_x,
                             global_y))
                break;
            cells.trainer_sight_actors_by_cell[
                cell_offset(cells, x, y)]
                .push_back(actor_index);
        }
    }

    world.player = {};
    world.player.map_index = 0;
    world.player.last_outdoor_map_index = 0;
    world.player.x = 5;
    world.player.y = 6;
    const WorldMap& start = world.maps.front();
    if (!is_passable(world, 0, start.global_x_tiles / 2 + world.player.x,
                     start.global_y_tiles / 2 + world.player.y)) {
        bool found = false;
        for (std::int32_t y = 0; y < start.height_blocks * 2 && !found; ++y) {
            for (std::int32_t x = 0; x < start.width_blocks * 2; ++x) {
                const WorldMapCellIndex& cells = world.spatial.front();
                if (is_passable(world, 0, start.global_x_tiles / 2 + x,
                                start.global_y_tiles / 2 + y) &&
                    cells.actor_by_cell[cell_offset(cells, x, y)] < 0) {
                    world.player.x = x;
                    world.player.y = y;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            error = "starting map has no unoccupied passable player cell";
            return false;
        }
    }
    world.player.visual_global_x =
        static_cast<float>(start.global_x_tiles / 2 + world.player.x);
    world.player.visual_global_y =
        static_cast<float>(start.global_y_tiles / 2 + world.player.y);
    world.player.initialized = true;
    world.current = 0;
    world.current_space = start.world_space;
    world.view = WorldView::world;
    world.follow_player_x = true;
    world.follow_player_y = true;
    world.target_zoom = 2.0F;
    world.zoom = 2.0F;
    world.camera_initialized = false;
    world.camera_x = world.target_camera_x = world.player.visual_global_x * 16.0F + 8.0F;
    world.camera_y = world.target_camera_y = world.player.visual_global_y * 16.0F + 8.0F;
    world.camera_region_dirty = true;
    error.clear();
    return true;
}

bool rebuild_world_actor_spatial(
    WorldState& world, const InteractionCatalog& interactions,
    std::string& error) {
    if (!world.loaded || !interactions.loaded ||
        world.spatial.size() != world.maps.size()) {
        error =
            "world actor spatial rebuild requires an initialized runtime";
        return false;
    }
    for (WorldMapCellIndex& cells : world.spatial) {
        std::ranges::fill(cells.actor_by_cell, -1);
        for (auto& sight : cells.trainer_sight_actors_by_cell)
            sight.clear();
    }
    for (std::size_t actor_index = 0U;
         actor_index < world.actors.size(); ++actor_index) {
        const WorldActorState& actor = world.actors[actor_index];
        if (actor.map_index >= world.maps.size() ||
            actor.map_index >= world.spatial.size()) {
            error = "saved actor references an unavailable map";
            return false;
        }
        const WorldMap& map = world.maps[actor.map_index];
        if (actor.spawn_index >= map.actors.size()) {
            error = "saved actor references an unavailable spawn";
            return false;
        }
        WorldMapCellIndex& cells = world.spatial[actor.map_index];
        if (!inside(cells, actor.x, actor.y)) {
            error = "saved actor is outside its authored map";
            return false;
        }
        const std::size_t offset =
            cell_offset(cells, actor.x, actor.y);
        if (actor.visible) {
            if (cells.actor_by_cell[offset] >= 0) {
                error = "saved actors occupy the same world cell";
                return false;
            }
            cells.actor_by_cell[offset] =
                static_cast<std::int32_t>(actor_index);
        }

        const WorldActorSpawn& spawn =
            map.actors[actor.spawn_index];
        const TrainerInteractionRule* trainer =
            find_trainer_interaction(
                interactions, map.id, spawn.index);
        if (!actor.visible || trainer == nullptr ||
            trainer->sight_range == 0U)
            continue;
        std::int32_t dx = 0;
        std::int32_t dy = 0;
        direction_delta(actor.facing, dx, dy);
        for (std::uint8_t distance = 1U;
             distance <= trainer->sight_range; ++distance) {
            const std::int32_t x =
                actor.x +
                dx * static_cast<std::int32_t>(distance);
            const std::int32_t y =
                actor.y +
                dy * static_cast<std::int32_t>(distance);
            const std::int32_t global_x =
                map.global_x_tiles / 2 + x;
            const std::int32_t global_y =
                map.global_y_tiles / 2 + y;
            if (!inside(cells, x, y) ||
                !is_passable(world, actor.map_index, global_x,
                             global_y))
                break;
            cells.trainer_sight_actors_by_cell[
                cell_offset(cells, x, y)]
                .push_back(actor_index);
        }
    }
    error.clear();
    return true;
}

bool enter_world_at(WorldState& world, std::uint8_t map_id, std::int32_t x,
                    std::int32_t y, std::string& error,
                    std::optional<std::uint8_t> previous_map_id) {
    const auto found = std::find_if(world.maps.begin(), world.maps.end(),
                                    [map_id](const WorldMap& map) {
                                        return map.id == map_id;
                                    });
    if (!world.loaded || found == world.maps.end()) {
        error = "campaign start references an unavailable map";
        return false;
    }
    const std::size_t map_index =
        static_cast<std::size_t>(found - world.maps.begin());
    if (x < 0 || y < 0 || x >= found->width_tiles / 2 ||
        y >= found->height_tiles / 2) {
        error = "campaign start lies outside its imported map";
        return false;
    }
    const std::int32_t global_x = found->global_x_tiles / 2 + x;
    const std::int32_t global_y = found->global_y_tiles / 2 + y;
    if (!is_passable(world, map_index, global_x, global_y)) {
        error = "campaign start lies on an impassable world cell";
        return false;
    }
    std::size_t previous_map_index = map_index;
    if (previous_map_id.has_value()) {
        previous_map_index = find_map_by_id(world, *previous_map_id);
        if (previous_map_index >= world.maps.size() ||
            world.maps[previous_map_index].world_space >= world.spaces.size() ||
            !world.spaces[world.maps[previous_map_index].world_space].outdoor) {
            error = "campaign start references an invalid previous outdoor map";
            return false;
        }
    }
    world.player.map_index = map_index;
    world.player.last_outdoor_map_index = previous_map_index;
    world.player.x = x;
    world.player.y = y;
    world.player.visual_global_x = static_cast<float>(global_x);
    world.player.visual_global_y = static_cast<float>(global_y);
    world.player.movement_from_x =
        world.player.movement_to_x =
            world.player.visual_global_x;
    world.player.movement_from_y =
        world.player.movement_to_y =
            world.player.visual_global_y;
    world.player.moving = false;
    world.player.movement_queue.clear();
    world.player.ledge_hop = false;
    world.player.warp_pending = false;
    world.player.visual_offset_y_pixels = 0.0F;
    world.player.animation_phase = 0U;
    world.player.facing = WorldDirection::down;
    world.player.move_cooldown = 0U;
    world.player.initialized = true;
    world.current = map_index;
    world.current_space = found->world_space;
    world.view = WorldView::world;
    world.follow_player_x = true;
    world.follow_player_y = true;
    world.camera_region_dirty = true;
    world.camera_x = world.target_camera_x =
        world.player.visual_global_x * 16.0F + 8.0F;
    world.camera_y = world.target_camera_y =
        world.player.visual_global_y * 16.0F + 8.0F;
    world.camera_initialized = false;
    show_area_banner(world, map_index);
    error.clear();
    return true;
}

void step_world(WorldState& world, const InteractionCatalog& interactions,
                CampaignState& campaign,
                const WorldStepInput& input) {
    world.player_completed_step = false;
    world.last_actor_activation = {};
    world.last_cell_activation = {};
    if (!campaign.initialized || !world.player.initialized ||
        world.spatial.size() != world.maps.size())
        return;
    ++world.simulation_tick;

    if (world.menu.open) {
        step_field_menu(
            world.menu,
            {
                .up = input.up,
                .down = input.down,
                .confirm = input.activate,
                .back = input.back,
                .start = input.start,
            });
        return;
    }

    // Imported modal services resume only after their generic choice owner has
    // committed a result. No map or actor identity is special-cased here.
    if (world.service.active && world.choice.decided) {
        if (world.service.kind ==
                WorldServiceKind::pokecenter_nurse &&
            world.service.pages.size() == 5U) {
            const bool heal = world.choice.selected == 0U;
            world.choice = {};
            world.dialogue = {};
            if (heal) {
                heal_party(campaign.party);
                const WorldMap& map =
                    world.maps[world.player.map_index];
                campaign.last_healing_map_id = map.id;
                campaign.last_healing_x =
                    static_cast<std::uint8_t>(world.player.x);
                campaign.last_healing_y =
                    static_cast<std::uint8_t>(world.player.y);
                campaign.has_healing_checkpoint = true;
                world.dialogue.pages.push_back(
                    world.service.pages[2]);
                world.dialogue.pages.push_back(
                    world.service.pages[3]);
            }
            world.dialogue.pages.push_back(
                world.service.pages[4]);
            world.dialogue.open = true;
        }
        world.service = {};
        return;
    }
    if (world.service.active && !world.dialogue.open &&
        !world.choice.open) {
        world.choice = {
            .options = {"HEAL", "CANCEL"},
            .selected = 0U,
            .input_cooldown = 0U,
            .open = true,
            .decided = false,
        };
        return;
    }

    if (world.naming.open) {
        step_naming(
            {
                .left = input.left,
                .right = input.right,
                .up = input.up,
                .down = input.down,
                .confirm = input.activate,
                .erase = input.erase,
                .submit = input.submit,
                .toggle_case = input.toggle_case,
                .text = input.text != nullptr ? input.text : "",
            },
            world.naming);
        return;
    }

    if (world.choice.open) {
        if (world.choice.input_cooldown > 0U) {
            --world.choice.input_cooldown;
        } else if (!world.choice.options.empty() &&
                   (input.left || input.up || input.right || input.down)) {
            const bool previous = input.left || input.up;
            if (previous) {
                world.choice.selected =
                    world.choice.selected == 0U
                        ? world.choice.options.size() - 1U
                        : world.choice.selected - 1U;
            } else {
                world.choice.selected =
                    (world.choice.selected + 1U) %
                    world.choice.options.size();
            }
            world.choice.input_cooldown = 8U;
        }
        if (input.activate && !world.choice.options.empty()) {
            world.choice.open = false;
            world.choice.decided = true;
            world.dialogue = {};
        }
        return;
    }

    if (world.dialogue.open) {
        if (input.activate) {
            if (world.dialogue.page + 1U < world.dialogue.pages.size())
                ++world.dialogue.page;
            else
                world.dialogue = {};
        }
        return;
    }

    if (input.start && !campaign.input_locked) {
        open_field_menu(world.menu);
        return;
    }

    // Trainer approach owns movement until the imported sight owner reaches
    // the adjacent cell. Actor visual positions interpolate independently.
    if (world.trainer_approach.active) {
        WorldTrainerApproach& approach = world.trainer_approach;
        if (approach.step_cooldown > 0U) {
            --approach.step_cooldown;
            return;
        }
        if (approach.actor_runtime_index >= world.actors.size()) {
            approach = {};
            return;
        }
        WorldActorState& actor =
            world.actors[approach.actor_runtime_index];
        const WorldMap& map = world.maps[actor.map_index];
        const WorldActorSpawn& spawn =
            map.actors[actor.spawn_index];
        if (approach.steps_remaining == 0U) {
            world.last_actor_activation = {
                .map_id = map.id,
                .actor_index = spawn.index,
                .occurred = true,
            };
            approach = {};
            return;
        }
        std::int32_t dx = 0;
        std::int32_t dy = 0;
        direction_delta(approach.direction, dx, dy);
        WorldMapCellIndex& cells = world.spatial[actor.map_index];
        const std::int32_t target_x = actor.x + dx;
        const std::int32_t target_y = actor.y + dy;
        if (!inside(cells, target_x, target_y) ||
            cells.actor_by_cell[
                cell_offset(cells, target_x, target_y)] >= 0 ||
            (world.player.map_index == actor.map_index &&
             world.player.x == target_x &&
             world.player.y == target_y)) {
            approach = {};
            return;
        }
        cells.actor_by_cell[cell_offset(cells, actor.x, actor.y)] = -1;
        actor.x = target_x;
        actor.y = target_y;
        actor.facing = approach.direction;
        cells.actor_by_cell[cell_offset(cells, actor.x, actor.y)] =
            static_cast<std::int32_t>(
                approach.actor_runtime_index);
        --approach.steps_remaining;
        approach.step_cooldown = 15U;
        return;
    }

    // Scripted paths may end on a warp while campaign input remains locked.
    // The warp is part of the motion contract and must finish before the
    // owning script is allowed to advance.
    if (campaign.input_locked) {
        if (world.player.move_cooldown > 0U) {
            --world.player.move_cooldown;
        } else if (world.player.warp_pending) {
            (void)activate_world_warp(world);
        }
        return;
    }

    if (input.activate) {
        std::int32_t dx = 0;
        std::int32_t dy = 0;
        direction_delta(world.player.facing, dx, dy);
        const WorldMap& map = world.maps[world.player.map_index];
        const std::int32_t global_x = map.global_x_tiles / 2 + world.player.x + dx;
        const std::int32_t global_y = map.global_y_tiles / 2 + world.player.y + dy;
        const std::size_t target_map =
            find_map_for_global_cell(world, world.player.map_index, global_x, global_y);
        if (target_map < world.maps.size()) {
            const WorldMap& target = world.maps[target_map];
            const std::int32_t local_x = global_x - target.global_x_tiles / 2;
            const std::int32_t local_y = global_y - target.global_y_tiles / 2;
            world.last_cell_activation = {
                .map_id = target.id,
                .x = static_cast<std::uint8_t>(local_x),
                .y = static_cast<std::uint8_t>(local_y),
                .facing = world.player.facing,
                .occurred = true,
            };
            WorldMapCellIndex& cells = world.spatial[target_map];
            const std::size_t cell = cell_offset(cells, local_x, local_y);
            const std::int32_t actor_index = cells.actor_by_cell[cell];
            if (actor_index >= 0 &&
                static_cast<std::size_t>(actor_index) < world.actors.size()) {
                WorldActorState& actor = world.actors[static_cast<std::size_t>(actor_index)];
                actor.facing = opposite(world.player.facing);
                const WorldActorSpawn& spawn =
                    world.maps[actor.map_index].actors[actor.spawn_index];
                world.last_actor_activation = {
                    .map_id = target.id,
                    .actor_index = spawn.index,
                    .occurred = true,
                };
                open_interaction(
                    world, campaign,
                    find_interaction(interactions, target.id, spawn.text_id));
            } else if (cells.background_program_by_cell[cell] != 0U) {
                open_interaction(world, campaign,
                                 find_interaction(interactions, target.id,
                                                  cells.background_program_by_cell[cell]));
            }
        }
    }

    if (world.player.move_cooldown > 0U) {
        --world.player.move_cooldown;
    } else if (world.player.warp_pending) {
        // The entry step owns a complete movement interval. Transfer only
        // after the player has visibly arrived on the warp cell.
        (void)activate_world_warp(world);
    } else {
        std::optional<WorldDirection> direction;
        if (input.left)
            direction = WorldDirection::left;
        else if (input.right)
            direction = WorldDirection::right;
        else if (input.up)
            direction = WorldDirection::up;
        else if (input.down)
            direction = WorldDirection::down;
        if (direction.has_value()) {
            world.player.facing = *direction;
            std::int32_t dx = 0;
            std::int32_t dy = 0;
            direction_delta(*direction, dx, dy);
            const WorldMap& map = world.maps[world.player.map_index];
            const std::int32_t current_global_x =
                map.global_x_tiles / 2 + world.player.x;
            const std::int32_t current_global_y =
                map.global_y_tiles / 2 + world.player.y;
            std::int32_t global_x = current_global_x + dx;
            std::int32_t global_y = current_global_y + dy;
            const std::size_t target_map =
                find_map_for_global_cell(world, world.player.map_index, global_x, global_y);
            bool ledge_hop = false;
            std::size_t resolved_map = target_map;
            if (matching_ledge(
                    world, world.player.map_index,
                    current_global_x, current_global_y,
                    global_x, global_y, *direction) != nullptr) {
                global_x += dx;
                global_y += dy;
                resolved_map = find_map_for_global_cell(
                    world, world.player.map_index,
                    global_x, global_y);
                ledge_hop = true;
            }
            if (resolved_map < world.maps.size() &&
                is_passable(world, resolved_map, global_x, global_y)) {
                const WorldMap& target = world.maps[resolved_map];
                const std::int32_t local_x = global_x - target.global_x_tiles / 2;
                const std::int32_t local_y = global_y - target.global_y_tiles / 2;
                const WorldMapCellIndex& cells = world.spatial[resolved_map];
                if (cells.actor_by_cell[cell_offset(cells, local_x, local_y)] < 0) {
                    const std::size_t previous_map =
                        world.player.map_index;
                    world.player.map_index = resolved_map;
                    world.player.x = local_x;
                    world.player.y = local_y;
                    world.current = resolved_map;
                    if (previous_map != resolved_map) {
                        world.camera_region_dirty = true;
                        show_area_banner(world, resolved_map);
                    }
                    world.player.move_cooldown = 15U;
                    queue_player_segment(
                        world, static_cast<float>(global_x),
                        static_cast<float>(global_y), ledge_hop);
                    world.player.warp_pending =
                        warp_at(target, local_x, local_y) != nullptr;
                    world.player_completed_step =
                        !world.player.warp_pending;
                }
            }
        }
    }

    // A completed player step performs one indexed trainer-sight lookup.
    if (world.player_completed_step &&
        !world.opponent_request.pending) {
        const WorldMapCellIndex& cells =
            world.spatial[world.player.map_index];
        const std::size_t player_cell =
            cell_offset(cells, world.player.x, world.player.y);
        for (const std::size_t actor_index :
             cells.trainer_sight_actors_by_cell[player_cell]) {
            if (actor_index >= world.actors.size()) continue;
            const WorldActorState& actor = world.actors[actor_index];
            const WorldMap& map = world.maps[actor.map_index];
            const WorldActorSpawn& spawn =
                map.actors[actor.spawn_index];
            const TrainerInteractionRule* trainer =
                find_trainer_interaction(interactions, map.id,
                                         spawn.index);
            if (trainer == nullptr ||
                campaign_flag(campaign, trainer->defeated_flag))
                continue;
            const std::int32_t distance =
                std::abs(actor.x - world.player.x) +
                std::abs(actor.y - world.player.y);
            if (distance <= 0 || distance > 255) continue;
            world.trainer_approach = {
                .actor_runtime_index = actor_index,
                .steps_remaining = static_cast<std::uint8_t>(
                    distance - 1),
                .step_cooldown = 15U,
                .direction = actor.facing,
                .active = true,
            };
            break;
        }
    }

    const std::size_t schedule_slot =
        static_cast<std::size_t>(world.simulation_tick % world.roam_schedule.size());
    std::vector<std::size_t> scheduled = std::move(world.roam_schedule[schedule_slot]);
    world.roam_schedule[schedule_slot].clear();
    for (const std::size_t actor_index : scheduled) {
        if (actor_index >= world.actors.size()) continue;
        WorldActorState& actor = world.actors[actor_index];
        if (!actor.visible) {
            world.roam_schedule[(schedule_slot + 60U) %
                                world.roam_schedule.size()]
                .push_back(actor_index);
            continue;
        }
        const WorldMap& map = world.maps[actor.map_index];
        if (map.world_space != world.current_space) {
            world.roam_schedule[(schedule_slot + 60U) % world.roam_schedule.size()].push_back(
                actor_index);
            continue;
        }
        const WorldActorSpawn& spawn = map.actors[actor.spawn_index];
        const std::uint32_t random = next_random(world);
        std::uint32_t choice = random % 4U;
        if (spawn.direction_or_axis == 1U) choice = random % 2U;
        if (spawn.direction_or_axis == 2U) choice = 2U + random % 2U;
        const WorldDirection direction = static_cast<WorldDirection>(choice);
        actor.facing = direction;
        std::int32_t dx = 0;
        std::int32_t dy = 0;
        direction_delta(direction, dx, dy);
        const std::int32_t global_x = map.global_x_tiles / 2 + actor.x + dx;
        const std::int32_t global_y = map.global_y_tiles / 2 + actor.y + dy;
        WorldMapCellIndex& cells = world.spatial[actor.map_index];
        const std::int32_t target_x = actor.x + dx;
        const std::int32_t target_y = actor.y + dy;
        const bool player_blocks =
            world.player.map_index == actor.map_index && world.player.x == target_x &&
            world.player.y == target_y;
        if (inside(cells, target_x, target_y) && !player_blocks &&
            actor_can_roam_to(spawn, global_x, global_y) &&
            is_passable(world, actor.map_index, global_x, global_y) &&
            cells.actor_by_cell[cell_offset(cells, target_x, target_y)] < 0) {
            cells.actor_by_cell[cell_offset(cells, actor.x, actor.y)] = -1;
            actor.x = target_x;
            actor.y = target_y;
            cells.actor_by_cell[cell_offset(cells, actor.x, actor.y)] =
                static_cast<std::int32_t>(actor_index);
        }
        const std::size_t delay = 75U + static_cast<std::size_t>(next_random(world) % 106U);
        world.roam_schedule[(schedule_slot + delay) % world.roam_schedule.size()].push_back(
            actor_index);
    }
}

void open_world_dialogue(WorldState& world,
                         const CampaignState& campaign,
                         const std::vector<std::string>& pages) {
    world.dialogue = {};
    for (const std::string& page : pages)
        world.dialogue.pages.push_back(
            substitute_campaign_text(page, campaign));
    world.dialogue.open = !world.dialogue.pages.empty();
}

bool set_world_actor_visible(WorldState& world, std::uint8_t map_id, std::uint8_t actor_index,
                             bool visible, std::string& error) {
    for (std::size_t runtime_index = 0U; runtime_index < world.actors.size(); ++runtime_index) {
        WorldActorState& actor = world.actors[runtime_index];
        const WorldMap& map = world.maps[actor.map_index];
        const WorldActorSpawn& spawn = map.actors[actor.spawn_index];
        if (map.id != map_id || spawn.index != actor_index) continue;
        WorldMapCellIndex& cells = world.spatial[actor.map_index];
        const std::size_t cell = cell_offset(cells, actor.x, actor.y);
        if (visible && !actor.visible) {
            if (cells.actor_by_cell[cell] >= 0) {
                error = "visible campaign actor would overlap another actor";
                return false;
            }
            cells.actor_by_cell[cell] = static_cast<std::int32_t>(runtime_index);
        } else if (!visible && actor.visible &&
                   cells.actor_by_cell[cell] == static_cast<std::int32_t>(runtime_index)) {
            cells.actor_by_cell[cell] = -1;
        }
        actor.visible = visible;
        error.clear();
        return true;
    }
    error = "campaign program references an unavailable actor";
    return false;
}

bool face_world_actor(WorldState& world, std::uint8_t map_id, std::uint8_t actor_index,
                      WorldDirection direction, std::string& error) {
    for (WorldActorState& actor : world.actors) {
        const WorldMap& map = world.maps[actor.map_index];
        const WorldActorSpawn& spawn = map.actors[actor.spawn_index];
        if (map.id == map_id && spawn.index == actor_index) {
            actor.facing = direction;
            error.clear();
            return true;
        }
    }
    error = "campaign program cannot face an unavailable actor";
    return false;
}

namespace {

bool find_runtime_actor(const WorldState& world, std::uint8_t map_id, std::uint8_t actor_index,
                        std::size_t& result) {
    for (std::size_t index = 0U; index < world.actors.size(); ++index) {
        const WorldActorState& actor = world.actors[index];
        const WorldMap& map = world.maps[actor.map_index];
        const WorldActorSpawn& spawn = map.actors[actor.spawn_index];
        if (map.id == map_id && spawn.index == actor_index) {
            result = index;
            return true;
        }
    }
    return false;
}

void append_axis_path(std::int32_t current, std::int32_t target, WorldPathCommand negative,
                      WorldPathCommand positive, std::vector<WorldPathCommand>& path) {
    const WorldPathCommand command = current > target ? negative : positive;
    const std::int32_t count = std::abs(current - target);
    for (std::int32_t step = 0; step < count; ++step)
        path.push_back(command);
}

bool find_local_path(const WorldState& world, std::size_t map_index,
                     std::int32_t start_x, std::int32_t start_y,
                     std::int32_t target_x, std::int32_t target_y,
                     std::size_t permitted_actor,
                     std::vector<WorldPathCommand>& result) {
    if (map_index >= world.maps.size() || map_index >= world.spatial.size())
        return false;
    const WorldMap& map = world.maps[map_index];
    const WorldMapCellIndex& cells = world.spatial[map_index];
    if (!inside(cells, start_x, start_y) ||
        !inside(cells, target_x, target_y))
        return false;

    const std::size_t count =
        static_cast<std::size_t>(cells.width) *
        static_cast<std::size_t>(cells.height);
    const std::size_t start = cell_offset(cells, start_x, start_y);
    const std::size_t target = cell_offset(cells, target_x, target_y);
    std::vector<std::int32_t> previous(count, -1);
    std::vector<WorldPathCommand> entered_with(
        count, WorldPathCommand::wait);
    std::queue<std::size_t> open;
    previous[start] = static_cast<std::int32_t>(start);
    open.push(start);

    constexpr std::array<std::int32_t, 4> dx{0, 0, -1, 1};
    constexpr std::array<std::int32_t, 4> dy{1, -1, 0, 0};
    constexpr std::array<WorldPathCommand, 4> commands{
        WorldPathCommand::down, WorldPathCommand::up,
        WorldPathCommand::left, WorldPathCommand::right};
    while (!open.empty() && previous[target] < 0) {
        const std::size_t current = open.front();
        open.pop();
        const std::int32_t x =
            static_cast<std::int32_t>(
                current % static_cast<std::size_t>(cells.width));
        const std::int32_t y =
            static_cast<std::int32_t>(
                current / static_cast<std::size_t>(cells.width));
        for (std::size_t direction = 0U;
             direction < commands.size(); ++direction) {
            const std::int32_t next_x = x + dx[direction];
            const std::int32_t next_y = y + dy[direction];
            if (!inside(cells, next_x, next_y))
                continue;
            const std::size_t next =
                cell_offset(cells, next_x, next_y);
            if (previous[next] >= 0)
                continue;
            const std::int32_t global_x =
                map.global_x_tiles / 2 + next_x;
            const std::int32_t global_y =
                map.global_y_tiles / 2 + next_y;
            const std::int32_t occupant =
                cells.actor_by_cell[next];
            if (!is_passable(world, map_index, global_x, global_y) ||
                (occupant >= 0 &&
                 static_cast<std::size_t>(occupant) !=
                     permitted_actor))
                continue;
            previous[next] = static_cast<std::int32_t>(current);
            entered_with[next] = commands[direction];
            open.push(next);
        }
    }
    if (previous[target] < 0)
        return false;

    result.clear();
    for (std::size_t current = target; current != start;
         current = static_cast<std::size_t>(previous[current]))
        result.push_back(entered_with[current]);
    std::reverse(result.begin(), result.end());
    return true;
}

bool apply_actor_path_command(WorldState& world, std::size_t runtime_index,
                              WorldPathCommand command,
                              bool may_overlap_player,
                              bool ignores_terrain,
                              std::string& error) {
    if (runtime_index >= world.actors.size()) {
        error = "campaign actor path lost its owner";
        return false;
    }
    WorldActorState& actor = world.actors[runtime_index];
    if (command == WorldPathCommand::wait) return true;
    if (command == WorldPathCommand::face_down) {
        actor.facing = WorldDirection::down;
        return true;
    }
    const WorldDirection facing = command == WorldPathCommand::up      ? WorldDirection::up
                                  : command == WorldPathCommand::left  ? WorldDirection::left
                                  : command == WorldPathCommand::right ? WorldDirection::right
                                                                       : WorldDirection::down;
    std::int32_t dx = 0;
    std::int32_t dy = 0;
    direction_delta(facing, dx, dy);
    WorldMapCellIndex& cells = world.spatial[actor.map_index];
    const std::int32_t target_x = actor.x + dx;
    const std::int32_t target_y = actor.y + dy;
    const WorldMap& map = world.maps[actor.map_index];
    const std::int32_t global_x = map.global_x_tiles / 2 + target_x;
    const std::int32_t global_y = map.global_y_tiles / 2 + target_y;
    const bool target_inside = inside(cells, target_x, target_y);
    const std::int32_t occupant =
        target_inside
            ? cells.actor_by_cell[
                  cell_offset(cells, target_x, target_y)]
            : -1;
    if (!target_inside ||
        (!ignores_terrain &&
         !is_passable(world, actor.map_index, global_x, global_y)) ||
        occupant >= 0 ||
        (!may_overlap_player &&
         world.player.map_index == actor.map_index &&
         world.player.x == target_x && world.player.y == target_y)) {
        error = "campaign actor path is blocked on map " +
                std::to_string(map.id) + " at " +
                std::to_string(target_x) + ',' +
                std::to_string(target_y) + " by actor " +
                std::to_string(occupant) + " (terrain " +
                (is_passable(
                     world, actor.map_index, global_x, global_y)
                     ? "passable"
                     : "blocked") +
                ", player_overlap " +
                (world.player.map_index == actor.map_index &&
                         world.player.x == target_x &&
                         world.player.y == target_y
                     ? "yes"
                     : "no") +
                ", overlap_allowed " +
                (may_overlap_player ? "yes" : "no") + ')';
        return false;
    }
    if (actor.visible) cells.actor_by_cell[cell_offset(cells, actor.x, actor.y)] = -1;
    actor.x = target_x;
    actor.y = target_y;
    actor.facing = facing;
    if (actor.visible)
        cells.actor_by_cell[cell_offset(cells, actor.x, actor.y)] =
            static_cast<std::int32_t>(runtime_index);
    return true;
}

bool apply_player_path_command(WorldState& world, WorldPathCommand command,
                               std::size_t permitted_actor, std::string& error) {
    if (command == WorldPathCommand::wait) return true;
    if (command == WorldPathCommand::face_down) {
        world.player.facing = WorldDirection::down;
        return true;
    }
    const WorldDirection facing = command == WorldPathCommand::up      ? WorldDirection::up
                                  : command == WorldPathCommand::left  ? WorldDirection::left
                                  : command == WorldPathCommand::right ? WorldDirection::right
                                                                       : WorldDirection::down;
    std::int32_t dx = 0;
    std::int32_t dy = 0;
    direction_delta(facing, dx, dy);
    const WorldMap& map = world.maps[world.player.map_index];
    const std::int32_t global_x = map.global_x_tiles / 2 + world.player.x + dx;
    const std::int32_t global_y = map.global_y_tiles / 2 + world.player.y + dy;
    const std::size_t target_map =
        find_map_for_global_cell(world, world.player.map_index, global_x, global_y);
    if (target_map >= world.maps.size() || !is_passable(world, target_map, global_x, global_y)) {
        error =
            "campaign player path is blocked from map " +
            std::to_string(map.id) + " at " +
            std::to_string(world.player.x) + ',' +
            std::to_string(world.player.y) +
            " toward global cell " +
            std::to_string(global_x) + ',' +
            std::to_string(global_y);
        return false;
    }
    const WorldMap& target = world.maps[target_map];
    const std::int32_t local_x = global_x - target.global_x_tiles / 2;
    const std::int32_t local_y = global_y - target.global_y_tiles / 2;
    const WorldMapCellIndex& cells = world.spatial[target_map];
    const std::int32_t occupant = cells.actor_by_cell[cell_offset(cells, local_x, local_y)];
    if (occupant >= 0 && static_cast<std::size_t>(occupant) != permitted_actor) {
        error = "campaign player path is blocked by actor " + std::to_string(occupant) +
                " on map " + std::to_string(target.id) + " at " + std::to_string(local_x) + ',' +
                std::to_string(local_y);
        return false;
    }
    const std::size_t previous_map = world.player.map_index;
    world.player.map_index = target_map;
    world.player.x = local_x;
    world.player.y = local_y;
    world.player.facing = facing;
    world.current = target_map;
    world.current_space = target.world_space;
    if (previous_map != target_map) {
        world.camera_region_dirty = true;
        show_area_banner(world, target_map);
    }
    queue_player_segment(
        world, static_cast<float>(global_x),
        static_cast<float>(global_y));
    world.player.move_cooldown = 15U;
    world.player.warp_pending =
        warp_at(target, local_x, local_y) != nullptr;
    world.player_completed_step = !world.player.warp_pending;
    return true;
}

} // namespace

bool place_world_actor(WorldState& world, std::uint8_t map_id,
                       std::uint8_t actor_index, std::int32_t x,
                       std::int32_t y, std::string& error,
                       bool ignores_terrain) {
    std::size_t runtime_index = 0U;
    if (!find_runtime_actor(world, map_id, actor_index, runtime_index)) {
        error = "campaign actor placement has an unavailable owner";
        return false;
    }
    WorldActorState& actor = world.actors[runtime_index];
    WorldMapCellIndex& cells = world.spatial[actor.map_index];
    const WorldMap& map = world.maps[actor.map_index];
    const std::int32_t global_x = map.global_x_tiles / 2 + x;
    const std::int32_t global_y = map.global_y_tiles / 2 + y;
    if (!inside(cells, x, y) ||
        (!ignores_terrain &&
         !is_passable(world, actor.map_index, global_x, global_y))) {
        error = "campaign actor placement is outside passable terrain";
        return false;
    }
    const std::int32_t occupant =
        cells.actor_by_cell[cell_offset(cells, x, y)];
    if ((occupant >= 0 &&
         static_cast<std::size_t>(occupant) != runtime_index) ||
        (world.player.map_index == actor.map_index &&
         world.player.x == x && world.player.y == y)) {
        error = "campaign actor placement is occupied";
        return false;
    }
    if (actor.visible)
        cells.actor_by_cell[cell_offset(cells, actor.x, actor.y)] = -1;
    actor.x = x;
    actor.y = y;
    actor.visual_global_x = static_cast<float>(global_x);
    actor.visual_global_y = static_cast<float>(global_y);
    actor.movement_from_x = actor.movement_to_x =
        actor.visual_global_x;
    actor.movement_from_y = actor.movement_to_y =
        actor.visual_global_y;
    actor.movement_elapsed = 0.0F;
    actor.animation_phase = 0U;
    actor.moving = false;
    if (actor.visible)
        cells.actor_by_cell[cell_offset(cells, x, y)] =
            static_cast<std::int32_t>(runtime_index);
    error.clear();
    return true;
}

bool start_world_actor_to_player_motion(WorldState& world, std::uint8_t map_id,
                                        std::uint8_t actor_index, std::int8_t target_y_offset,
                                        std::string& error) {
    std::size_t runtime_index = 0U;
    if (!find_runtime_actor(world, map_id, actor_index, runtime_index) ||
        world.player.map_index != world.actors[runtime_index].map_index) {
        error = "actor-to-player path has an unavailable owner";
        return false;
    }
    const WorldActorState& actor = world.actors[runtime_index];
    std::vector<WorldPathCommand> path;
    append_axis_path(actor.x, world.player.x, WorldPathCommand::left, WorldPathCommand::right,
                     path);
    append_axis_path(actor.y, world.player.y + static_cast<std::int32_t>(target_y_offset),
                     WorldPathCommand::up, WorldPathCommand::down, path);
    world.script_motion = {
        .actor_runtime_index = runtime_index,
        .actor_path = std::move(path),
        .player_path = {},
        .actor_cursor = 0U,
        .player_cursor = 0U,
        .step_cooldown = 0U,
        .hide_actor_at_end = false,
        .active = true,
    };
    error.clear();
    return true;
}

bool start_world_pair_alignment(WorldState& world, std::uint8_t map_id, std::uint8_t actor_index,
                                std::uint8_t target_x, std::string& error) {
    std::size_t runtime_index = 0U;
    if (!find_runtime_actor(world, map_id, actor_index, runtime_index) ||
        world.player.map_index != world.actors[runtime_index].map_index) {
        error = "pair alignment has an unavailable actor";
        return false;
    }
    const std::int32_t count = std::abs(world.player.x - static_cast<std::int32_t>(target_x));
    if (count == 0) {
        world.script_motion = {};
        error.clear();
        return true;
    }
    const WorldPathCommand command =
        world.player.x > target_x ? WorldPathCommand::left : WorldPathCommand::right;
    std::vector<WorldPathCommand> actor_path(static_cast<std::size_t>(count), command);
    actor_path.insert(actor_path.end(), static_cast<std::size_t>(count), WorldPathCommand::wait);
    std::vector<WorldPathCommand> player_path(static_cast<std::size_t>(count),
                                              WorldPathCommand::wait);
    player_path.insert(player_path.end(), static_cast<std::size_t>(count), command);
    return start_world_parallel_motion(world, map_id, actor_index, actor_path, player_path, false,
                                       error);
}

bool start_world_escort_motion(
    WorldState& world, std::uint8_t map_id, std::uint8_t actor_index,
    std::int32_t player_target_x, std::int32_t player_target_y,
    WorldDirection actor_target_side, std::string& error) {
    std::size_t runtime_index = 0U;
    if (!find_runtime_actor(world, map_id, actor_index, runtime_index) ||
        world.player.map_index != world.actors[runtime_index].map_index) {
        error = "escort path has an unavailable actor";
        return false;
    }
    std::int32_t actor_target_x = player_target_x;
    std::int32_t actor_target_y = player_target_y;
    std::int32_t dx = 0;
    std::int32_t dy = 0;
    direction_delta(actor_target_side, dx, dy);
    actor_target_x += dx;
    actor_target_y += dy;

    const WorldActorState& actor = world.actors[runtime_index];
    std::vector<WorldPathCommand> actor_path;
    std::vector<WorldPathCommand> player_path;
    if (!find_local_path(
            world, actor.map_index, actor.x, actor.y,
            actor_target_x, actor_target_y, runtime_index,
            actor_path) ||
        !find_local_path(
            world, world.player.map_index, world.player.x,
            world.player.y, player_target_x, player_target_y,
            runtime_index, player_path)) {
        error = "escort destination has no passable local path";
        return false;
    }
    const std::size_t step_count =
        std::max(actor_path.size(), player_path.size());
    actor_path.resize(step_count, WorldPathCommand::wait);
    player_path.resize(step_count, WorldPathCommand::wait);
    if (step_count == 0U) {
        world.script_motion = {};
        error.clear();
        return true;
    }
    return start_world_parallel_motion(
        world, map_id, actor_index, actor_path, player_path,
        false, error, true, false);
}

bool start_world_parallel_motion(WorldState& world, std::uint8_t map_id, std::uint8_t actor_index,
                                 const std::vector<WorldPathCommand>& actor_path,
                                 const std::vector<WorldPathCommand>& player_path,
                                 bool hide_actor_at_end, std::string& error,
                                 bool actor_may_overlap_player,
                                 bool actor_ignores_terrain) {
    std::size_t runtime_index = 0U;
    if (!find_runtime_actor(world, map_id, actor_index, runtime_index) || actor_path.empty() ||
        player_path.empty()) {
        error = "parallel path has incomplete actor or command data";
        return false;
    }
    world.script_motion = {
        .actor_runtime_index = runtime_index,
        .actor_path = actor_path,
        .player_path = player_path,
        .actor_cursor = 0U,
        .player_cursor = 0U,
        .step_cooldown = 0U,
        .hide_actor_at_end = hide_actor_at_end,
        .actor_may_overlap_player = actor_may_overlap_player,
        .actor_ignores_terrain = actor_ignores_terrain,
        .active = true,
    };
    error.clear();
    return true;
}

bool start_world_player_motion(
    WorldState& world,
    const std::vector<WorldPathCommand>& player_path,
    std::string& error) {
    if (player_path.empty()) {
        error = "player path has no command data";
        return false;
    }
    world.script_motion = {
        .actor_runtime_index = world.actors.size(),
        .actor_path = {},
        .player_path = player_path,
        .actor_cursor = 0U,
        .player_cursor = 0U,
        .step_cooldown = 0U,
        .hide_actor_at_end = false,
        .actor_may_overlap_player = false,
        .actor_ignores_terrain = false,
        .active = true,
    };
    error.clear();
    return true;
}

bool step_world_script_motion(WorldState& world, std::string& error) {
    WorldScriptMotion& motion = world.script_motion;
    if (!motion.active) {
        error.clear();
        return true;
    }
    if (motion.step_cooldown > 0U) {
        --motion.step_cooldown;
        error.clear();
        return true;
    }
    if (motion.actor_cursor < motion.actor_path.size() &&
        !apply_actor_path_command(world, motion.actor_runtime_index,
                                  motion.actor_path[motion.actor_cursor++],
                                  motion.actor_may_overlap_player,
                                  motion.actor_ignores_terrain, error))
        return false;
    if (motion.player_cursor < motion.player_path.size() &&
        !apply_player_path_command(world, motion.player_path[motion.player_cursor++],
                                   motion.actor_runtime_index, error))
        return false;
    if (motion.actor_cursor == motion.actor_path.size() &&
        motion.player_cursor == motion.player_path.size()) {
        const bool hide = motion.hide_actor_at_end;
        const std::size_t runtime_index = motion.actor_runtime_index;
        motion = {};
        if (hide && runtime_index < world.actors.size()) {
            const WorldActorState& actor = world.actors[runtime_index];
            const WorldMap& map = world.maps[actor.map_index];
            const WorldActorSpawn& spawn = map.actors[actor.spawn_index];
            return set_world_actor_visible(world, map.id, spawn.index, false, error);
        }
        error.clear();
        return true;
    }
    motion.step_cooldown = 15U;
    error.clear();
    return true;
}

void select_next_map(WorldState& world) {
    if (!world.maps.empty()) world.current = (world.current + 1U) % world.maps.size();
    if (world.current < world.maps.size()) world.current_space = world.maps[world.current].world_space;
    world.follow_player_x = false;
    world.follow_player_y = false;
    reset_world_view(world);
}

void select_previous_map(WorldState& world) {
    if (world.maps.empty()) return;
    world.current = world.current == 0 ? world.maps.size() - 1U : world.current - 1U;
    world.current_space = world.maps[world.current].world_space;
    world.follow_player_x = false;
    world.follow_player_y = false;
    reset_world_view(world);
}

void toggle_world_view(WorldState& world) {
    world.view = world.view == WorldView::selected ? WorldView::world : WorldView::selected;
    world.follow_player_x = true;
    world.follow_player_y = true;
    reset_world_view(world);
}

void zoom_world_view(WorldState& world, float factor) {
    world.target_zoom = std::clamp(world.target_zoom * factor, 0.05F, 64.0F);
    world.manual_camera_override = true;
    world.manual_pan_override = false;
}

void pan_world_view(WorldState& world, float x, float y) {
    world.follow_player_x = false;
    world.follow_player_y = false;
    world.manual_camera_override = true;
    world.manual_pan_override = true;
    const float scale = std::max(world.target_zoom, 0.05F);
    world.target_camera_x += x / scale;
    world.target_camera_y += y / scale;
}

void reset_world_view(WorldState& world) {
    const bool player_in_view =
        world.player.initialized &&
        world.player.map_index < world.maps.size() &&
        ((world.view == WorldView::world &&
          world.maps[world.player.map_index].world_space ==
              world.current_space) ||
         (world.view == WorldView::selected &&
          world.player.map_index == world.current));
    world.follow_player_x = player_in_view;
    world.follow_player_y = player_in_view;
    if (player_in_view) {
        world.target_camera_x = world.player.visual_global_x * 16.0F + 8.0F;
        world.target_camera_y = world.player.visual_global_y * 16.0F + 8.0F;
    } else if (world.view == WorldView::world) {
        float left = 0.0F;
        float top = 0.0F;
        float right = 0.0F;
        float bottom = 0.0F;
        world_view_bounds(world, left, top, right, bottom);
        world.target_camera_x = (left + right) * 0.5F;
        world.target_camera_y = (top + bottom) * 0.5F;
    } else {
        float left = 0.0F;
        float top = 0.0F;
        float right = 0.0F;
        float bottom = 0.0F;
        selected_view_bounds(world, left, top, right, bottom);
        world.target_camera_x = (left + right) * 0.5F;
        world.target_camera_y = (top + bottom) * 0.5F;
    }
    world.manual_camera_override = false;
    world.manual_pan_override = false;
    world.camera_region_dirty = true;
}

void update_world_camera_region(WorldState& world, int output_width,
                                int output_height) {
    if (!world.player.initialized ||
        world.player.map_index >= world.maps.size())
        return;
    const std::size_t map_index = world.player.map_index;
    const bool entered_region =
        world.camera_region_map_index != map_index;
    const bool apply_entry_profile =
        world.camera_region_dirty || entered_region;
    if (entered_region) world.manual_pan_override = false;
    world.camera_region_map_index = map_index;
    world.camera_region_dirty = false;

    const WorldMap& map = world.maps[map_index];
    float left = static_cast<float>(map.global_x_tiles) * 8.0F;
    float top = static_cast<float>(map.global_y_tiles) * 8.0F;
    float right =
        left + static_cast<float>(map.width_tiles) * 8.0F;
    float bottom =
        top + static_cast<float>(map.height_tiles) * 8.0F;
    if (map.camera_framing == WorldCameraFraming::fit_space)
        world_view_bounds(world, left, top, right, bottom);

    const float available_width =
        std::max(static_cast<float>(output_width) - 64.0F, 1.0F);
    const float available_height =
        std::max(static_cast<float>(output_height) - 96.0F, 1.0F);
    const float width = std::max(right - left, 1.0F);
    const float height = std::max(bottom - top, 1.0F);
    if (apply_entry_profile &&
        world.automatic_camera_framing) {
        float requested_zoom = map.camera_zoom;
        switch (map.camera_framing) {
        case WorldCameraFraming::fixed_zoom:
            break;
        case WorldCameraFraming::fit_map:
        case WorldCameraFraming::fit_space:
            requested_zoom =
                std::min(available_width / width,
                         available_height / height);
            break;
        case WorldCameraFraming::fit_width:
            requested_zoom = available_width / width;
            break;
        case WorldCameraFraming::fit_height:
            requested_zoom = available_height / height;
            break;
        }
        if (requested_zoom < 1.0F || requested_zoom > 4.0F)
            requested_zoom = map.camera_zoom;
        world.target_zoom =
            std::clamp(requested_zoom, 0.05F, 64.0F);
        world.manual_camera_override = false;
    }

    const float center_x = (left + right) * 0.5F;
    const float center_y = (top + bottom) * 0.5F;
    const bool area_fits =
        width * world.target_zoom <= available_width &&
        height * world.target_zoom <= available_height;
    if (!world.manual_pan_override) {
        world.follow_player_x = !area_fits;
        world.follow_player_y = !area_fits;
        world.target_camera_x =
            area_fits
                ? center_x
                : world.player.visual_global_x * 16.0F + 8.0F;
        world.target_camera_y =
            area_fits
                ? center_y
                : world.player.visual_global_y * 16.0F + 8.0F;
    }
    if (!world.camera_initialized) {
        world.camera_x = world.target_camera_x;
        world.camera_y = world.target_camera_y;
        world.zoom = world.target_zoom;
        world.camera_initialized = true;
    }
}

void update_world_view(WorldState& world, double elapsed_seconds) {
    const double bounded = std::clamp(elapsed_seconds, 0.0, 0.5);
    const float response = 1.0F - std::exp(static_cast<float>(-12.0 * bounded));
    if (world.player.initialized) {
        const WorldMap& map = world.maps[world.player.map_index];
        const float target_x =
            static_cast<float>(map.global_x_tiles / 2 + world.player.x);
        const float target_y =
            static_cast<float>(map.global_y_tiles / 2 + world.player.y);
        constexpr float duration = 16.0F / 60.0F;
        float remaining = static_cast<float>(bounded);
        while (remaining > 0.0F) {
            if (!world.player.moving) {
                if (world.player.movement_queue.empty())
                    break;
                const WorldMovementSegment& segment =
                    world.player.movement_queue.front();
                world.player.movement_from_x =
                    world.player.visual_global_x;
                world.player.movement_from_y =
                    world.player.visual_global_y;
                world.player.movement_to_x = segment.target_x;
                world.player.movement_to_y = segment.target_y;
                world.player.movement_elapsed = 0.0F;
                world.player.ledge_hop = segment.ledge_hop;
                world.player.moving = true;
            }
            const float consumed = std::min(
                remaining,
                duration - world.player.movement_elapsed);
            world.player.movement_elapsed += consumed;
            remaining -= consumed;
            const float amount = std::clamp(
                world.player.movement_elapsed / duration, 0.0F,
                1.0F);
            world.player.visual_global_x =
                std::lerp(world.player.movement_from_x,
                          world.player.movement_to_x, amount);
            world.player.visual_global_y =
                std::lerp(world.player.movement_from_y,
                          world.player.movement_to_y, amount);
            world.player.visual_offset_y_pixels =
                world.player.ledge_hop
                    ? -32.0F * amount * (1.0F - amount)
                    : 0.0F;
            // Red shows one foot frame per cell and alternates feet between
            // cells: idle -> foot -> idle, then the other foot next cell.
            world.player.animation_phase =
                amount >= 0.25F && amount < 0.75F
                    ? static_cast<std::uint8_t>(
                          world.player.alternate_foot ? 3U : 1U)
                    : 0U;
            if (amount < 1.0F) break;
            world.player.visual_global_x =
                world.player.movement_to_x;
            world.player.visual_global_y =
                world.player.movement_to_y;
            world.player.visual_offset_y_pixels = 0.0F;
            world.player.moving = false;
            world.player.ledge_hop = false;
            world.player.alternate_foot =
                !world.player.alternate_foot;
            world.player.movement_queue.pop_front();
        }
        // Direct state restoration/teleportation is never synthesized as an
        // arbitrary diagonal walk. Explicit movement producers enqueue every
        // walk or hop segment.
        if (!world.player.moving &&
            world.player.movement_queue.empty() &&
            (world.player.visual_global_x != target_x ||
             world.player.visual_global_y != target_y)) {
            world.player.visual_global_x = target_x;
            world.player.visual_global_y = target_y;
            world.player.visual_offset_y_pixels = 0.0F;
        }
        if (world.follow_player_x)
            world.target_camera_x = world.player.visual_global_x * 16.0F + 8.0F;
        if (world.follow_player_y)
            world.target_camera_y = world.player.visual_global_y * 16.0F + 8.0F;
    }
    for (WorldActorState& actor : world.actors) {
        const WorldMap& map = world.maps[actor.map_index];
        const float target_x = static_cast<float>(map.global_x_tiles / 2 + actor.x);
        const float target_y = static_cast<float>(map.global_y_tiles / 2 + actor.y);
        if (!actor.moving &&
            (actor.visual_global_x != target_x ||
             actor.visual_global_y != target_y)) {
            actor.movement_from_x = actor.visual_global_x;
            actor.movement_from_y = actor.visual_global_y;
            actor.movement_to_x = target_x;
            actor.movement_to_y = target_y;
            actor.movement_elapsed = 0.0F;
            actor.moving = true;
        } else if (actor.moving &&
                   (actor.movement_to_x != target_x ||
                    actor.movement_to_y != target_y)) {
            actor.visual_global_x = actor.movement_to_x;
            actor.visual_global_y = actor.movement_to_y;
            actor.movement_from_x = actor.visual_global_x;
            actor.movement_from_y = actor.visual_global_y;
            actor.movement_to_x = target_x;
            actor.movement_to_y = target_y;
            actor.movement_elapsed = 0.0F;
        }
        if (!actor.moving) continue;
        constexpr float duration = 16.0F / 60.0F;
        actor.movement_elapsed += static_cast<float>(bounded);
        const float amount = std::clamp(
            actor.movement_elapsed / duration, 0.0F, 1.0F);
        actor.visual_global_x =
            std::lerp(actor.movement_from_x, actor.movement_to_x,
                      amount);
        actor.visual_global_y =
            std::lerp(actor.movement_from_y, actor.movement_to_y,
                      amount);
        actor.animation_phase =
            amount >= 1.0F
                ? 0U
                : static_cast<std::uint8_t>(
                      std::min(3, static_cast<int>(amount * 4.0F)));
        if (amount >= 1.0F) actor.moving = false;
    }
    world.camera_x += (world.target_camera_x - world.camera_x) * response;
    world.camera_y += (world.target_camera_y - world.camera_y) * response;
    const float ratio = world.target_zoom / std::max(world.zoom, 0.0001F);
    world.zoom *= std::exp(std::log(ratio) * response);
}

void update_world_presentation(WorldState& world,
                               double elapsed_seconds) {
    if (!world.area_banner.active) return;
    constexpr float duration = 2.4F;
    world.area_banner.elapsed += static_cast<float>(
        std::clamp(elapsed_seconds, 0.0, 0.1));
    if (world.area_banner.elapsed >= duration)
        world.area_banner = {};
}

void step_world_animation(WorldState& world) {
    ++world.animation_tick;
}

const WorldMap* selected_map(const WorldState& world) {
    if (world.maps.empty() || world.current >= world.maps.size()) return nullptr;
    return &world.maps[world.current];
}

const WorldSpace* current_world_space(const WorldState& world) {
    if (world.current_space >= world.spaces.size()) return nullptr;
    return &world.spaces[world.current_space];
}

const MapTileset* find_tileset(const WorldState& world, std::uint8_t id) {
    const auto found = std::find_if(world.tilesets.begin(), world.tilesets.end(),
                                    [&](const MapTileset& tileset) { return tileset.id == id; });
    return found == world.tilesets.end() ? nullptr : &*found;
}

const WorldSprite* find_world_sprite(const WorldState& world, std::uint8_t id) {
    const auto found = std::find_if(world.sprites.begin(), world.sprites.end(),
                                    [id](const WorldSprite& sprite) { return sprite.id == id; });
    return found == world.sprites.end() ? nullptr : &*found;
}

const WorldMapCellIndex* find_spatial_index(const WorldState& world, std::uint8_t map_id) {
    const auto found = std::find_if(
        world.spatial.begin(), world.spatial.end(),
        [map_id](const WorldMapCellIndex& index) { return index.map_id == map_id; });
    return found == world.spatial.end() ? nullptr : &*found;
}

bool actor_can_roam_to(const WorldActorSpawn& actor, std::int32_t global_x, std::int32_t global_y) {
    if (actor.movement != 0xFEU || !actor.movement_bounds.has_value()) return false;
    const WorldMovementBounds& bounds = *actor.movement_bounds;
    return global_x >= bounds.x && global_y >= bounds.y && global_x < bounds.x + bounds.width &&
           global_y < bounds.y + bounds.height;
}

std::string_view selected_map_name(const WorldState& world) {
    const WorldMap* map = selected_map(world);
    return map == nullptr ? std::string_view("none") : std::string_view(map->display_name);
}

std::string_view label(WorldView view) {
    return view == WorldView::world ? "Connected world" : "Selected map";
}

std::string_view label(WorldActorKind kind) {
    switch (kind) {
    case WorldActorKind::npc:
        return "npc";
    case WorldActorKind::trainer_or_pokemon:
        return "trainer_or_pokemon";
    case WorldActorKind::item:
        return "item";
    }
    return "unknown";
}

} // namespace pokered
