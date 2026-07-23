#include "maps.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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
        if (!read_u8(input, tileset.id) || !read_u16(input, tileset.tile_count) ||
            !read_u8(input, tileset.animation_mode) || tileset.animation_mode > 2 ||
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
            !read_u8(input, still) || still > 1 || !read_u32(input, pixel_count) ||
            pixel_count != 4U * 16U * 16U || !read_bytes(input, pixel_count, sprite.pixels)) {
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

bool read_map(std::istream& input, const WorldState& world, WorldMap& map, std::string& error) {
    std::uint8_t key_size = 0;
    std::uint8_t name_size = 0;
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
        !read_u16(input, map.world_component) || !read_u32(input, tile_count)) {
        error = "world map cache has truncated map placement or tile data";
        return false;
    }
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
    selected_view_bounds(world, left, top, right, bottom);
    if (world.maps.empty()) return;
    for (const WorldMap& map : world.maps) {
        const float map_left = static_cast<float>(map.global_x_tiles) * 8.0F;
        const float map_top = static_cast<float>(map.global_y_tiles) * 8.0F;
        left = std::min(left, map_left);
        top = std::min(top, map_top);
        right = std::max(right, map_left + static_cast<float>(map.width_tiles) * 8.0F);
        bottom = std::max(bottom, map_top + static_cast<float>(map.height_tiles) * 8.0F);
    }
}

} // namespace

bool load_world(const std::filesystem::path& path, WorldState& result, std::string& error) {
    std::ifstream input(path, std::ios::binary);
    std::array<char, 4> magic{};
    if (!input.read(magic.data(), static_cast<std::streamsize>(magic.size())) ||
        magic != std::array{'P', 'M', 'V', '7'}) {
        error = "world map cache is missing or has an invalid header";
        return false;
    }

    WorldState loaded;
    loaded.source = path;
    if (!read_tilesets(input, loaded, error) || !read_sprites(input, loaded, error)) return false;
    std::uint16_t map_count = 0;
    if (!read_u16(input, map_count) || map_count == 0 || map_count > 248) {
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

void select_next_map(WorldState& world) {
    if (!world.maps.empty()) world.current = (world.current + 1U) % world.maps.size();
    if (world.view == WorldView::selected) reset_world_view(world);
}

void select_previous_map(WorldState& world) {
    if (world.maps.empty()) return;
    world.current = world.current == 0 ? world.maps.size() - 1U : world.current - 1U;
    if (world.view == WorldView::selected) reset_world_view(world);
}

void toggle_world_view(WorldState& world) {
    world.view = world.view == WorldView::selected ? WorldView::world : WorldView::selected;
    reset_world_view(world);
}

void zoom_world_view(WorldState& world, float factor) {
    world.target_zoom = std::clamp(world.target_zoom * factor, 0.05F, 64.0F);
}

void pan_world_view(WorldState& world, float x, float y) {
    const float scale = std::max(world.target_zoom, 0.05F);
    world.target_camera_x += x / scale;
    world.target_camera_y += y / scale;
}

void reset_world_view(WorldState& world) {
    float left = 0.0F;
    float top = 0.0F;
    float right = 0.0F;
    float bottom = 0.0F;
    if (world.view == WorldView::world)
        world_view_bounds(world, left, top, right, bottom);
    else
        selected_view_bounds(world, left, top, right, bottom);
    world.target_camera_x = (left + right) * 0.5F;
    world.target_camera_y = (top + bottom) * 0.5F;
    world.target_zoom = 1.0F;
    if (!world.camera_initialized) {
        world.camera_x = world.target_camera_x;
        world.camera_y = world.target_camera_y;
        world.zoom = world.target_zoom;
        world.camera_initialized = true;
    }
}

void update_world_view(WorldState& world, double elapsed_seconds) {
    const double bounded = std::clamp(elapsed_seconds, 0.0, 0.1);
    const float response = 1.0F - std::exp(static_cast<float>(-12.0 * bounded));
    world.camera_x += (world.target_camera_x - world.camera_x) * response;
    world.camera_y += (world.target_camera_y - world.camera_y) * response;
    const float ratio = world.target_zoom / std::max(world.zoom, 0.0001F);
    world.zoom *= std::exp(std::log(ratio) * response);
}

void step_world_animation(WorldState& world) {
    ++world.animation_tick;
}

const WorldMap* selected_map(const WorldState& world) {
    if (world.maps.empty() || world.current >= world.maps.size()) return nullptr;
    return &world.maps[world.current];
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
