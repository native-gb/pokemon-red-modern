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
        !read_u16(input, map.world_space) || map.world_space >= world.spaces.size() ||
        !read_u32(input, tile_count)) {
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

bool activate_world_warp(WorldState& world) {
    if (world.player.map_index >= world.maps.size()) return false;
    const std::size_t source_index = world.player.map_index;
    const WorldMap& source = world.maps[source_index];
    const WorldWarp* source_warp = warp_at(source, world.player.x, world.player.y);
    if (source_warp == nullptr) return false;

    if (source.world_space < world.spaces.size() && world.spaces[source.world_space].outdoor)
        world.player.last_outdoor_map_index = source_index;
    const std::size_t destination_index =
        source_warp->destination_map_id == 0xFFU
            ? world.player.last_outdoor_map_index
            : find_map_by_id(world, source_warp->destination_map_id);
    if (destination_index >= world.maps.size()) return false;
    const WorldMap& destination = world.maps[destination_index];
    if (source_warp->destination_warp_index >= destination.warps.size()) return false;
    const WorldWarp& destination_warp =
        destination.warps[source_warp->destination_warp_index];

    world.player.map_index = destination_index;
    world.player.x = destination_warp.x;
    world.player.y = destination_warp.y;
    world.player.move_cooldown = 7U;
    world.current = destination_index;
    world.current_space = destination.world_space;
    world.follow_player = true;
    world.dialogue = {};
    world.player.visual_global_x =
        static_cast<float>(destination.global_x_tiles / 2 + world.player.x);
    world.player.visual_global_y =
        static_cast<float>(destination.global_y_tiles / 2 + world.player.y);
    world.camera_x = world.target_camera_x = world.player.visual_global_x * 16.0F + 8.0F;
    world.camera_y = world.target_camera_y = world.player.visual_global_y * 16.0F + 8.0F;
    world.last_warp = {
        .source_map_id = source.id,
        .source_warp_index = source_warp->index,
        .destination_map_id = destination.id,
        .destination_warp_index = source_warp->destination_warp_index,
        .simulation_tick = world.simulation_tick,
        .occurred = true,
    };
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

void open_interaction(WorldState& world, const CampaignState& campaign,
                      const InteractionProgram* program) {
    world.dialogue = {};
    if (program != nullptr && program->status == InteractionProgramStatus::dialogue &&
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
        magic != std::array{'P', 'M', 'V', 'A'}) {
        error = "world map cache is missing or has an invalid header";
        return false;
    }

    WorldState loaded;
    loaded.source = path;
    if (!read_tilesets(input, loaded, error) || !read_sprites(input, loaded, error) ||
        !read_world_spaces(input, loaded, error))
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
                .facing = imported_facing(spawn.direction_or_axis),
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
    world.follow_player = true;
    world.target_zoom = 2.0F;
    world.zoom = 2.0F;
    world.camera_initialized = true;
    world.camera_x = world.target_camera_x = world.player.visual_global_x * 16.0F + 8.0F;
    world.camera_y = world.target_camera_y = world.player.visual_global_y * 16.0F + 8.0F;
    error.clear();
    return true;
}

bool enter_world_at(WorldState& world, std::uint8_t map_id, std::int32_t x,
                    std::int32_t y, std::string& error) {
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
    world.player.map_index = map_index;
    world.player.last_outdoor_map_index = map_index;
    world.player.x = x;
    world.player.y = y;
    world.player.visual_global_x = static_cast<float>(global_x);
    world.player.visual_global_y = static_cast<float>(global_y);
    world.player.facing = WorldDirection::down;
    world.player.move_cooldown = 0U;
    world.player.initialized = true;
    world.current = map_index;
    world.current_space = found->world_space;
    world.view = WorldView::world;
    world.follow_player = true;
    world.camera_x = world.target_camera_x =
        world.player.visual_global_x * 16.0F + 8.0F;
    world.camera_y = world.target_camera_y =
        world.player.visual_global_y * 16.0F + 8.0F;
    world.camera_initialized = true;
    error.clear();
    return true;
}

void step_world(WorldState& world, const InteractionCatalog& interactions,
                const CampaignState& campaign,
                const WorldStepInput& input) {
    world.player_completed_step = false;
    if (!campaign.initialized || !world.player.initialized ||
        world.spatial.size() != world.maps.size())
        return;
    ++world.simulation_tick;

    if (world.dialogue.open) {
        if (input.activate) {
            if (world.dialogue.page + 1U < world.dialogue.pages.size())
                ++world.dialogue.page;
            else
                world.dialogue = {};
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
            WorldMapCellIndex& cells = world.spatial[target_map];
            const std::size_t cell = cell_offset(cells, local_x, local_y);
            const std::int32_t actor_index = cells.actor_by_cell[cell];
            if (actor_index >= 0 &&
                static_cast<std::size_t>(actor_index) < world.actors.size()) {
                WorldActorState& actor = world.actors[static_cast<std::size_t>(actor_index)];
                actor.facing = opposite(world.player.facing);
                const WorldActorSpawn& spawn =
                    world.maps[actor.map_index].actors[actor.spawn_index];
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
            const std::int32_t global_x = map.global_x_tiles / 2 + world.player.x + dx;
            const std::int32_t global_y = map.global_y_tiles / 2 + world.player.y + dy;
            const std::size_t target_map =
                find_map_for_global_cell(world, world.player.map_index, global_x, global_y);
            if (target_map < world.maps.size() && is_passable(world, target_map, global_x, global_y)) {
                const WorldMap& target = world.maps[target_map];
                const std::int32_t local_x = global_x - target.global_x_tiles / 2;
                const std::int32_t local_y = global_y - target.global_y_tiles / 2;
                const WorldMapCellIndex& cells = world.spatial[target_map];
                if (cells.actor_by_cell[cell_offset(cells, local_x, local_y)] < 0) {
                    world.player.map_index = target_map;
                    world.player.x = local_x;
                    world.player.y = local_y;
                    world.current = target_map;
                    world.follow_player = true;
                    world.player.move_cooldown = 7U;
                    world.player_completed_step =
                        !activate_world_warp(world);
                }
            }
        }
    }

    const std::size_t schedule_slot =
        static_cast<std::size_t>(world.simulation_tick % world.roam_schedule.size());
    std::vector<std::size_t> scheduled = std::move(world.roam_schedule[schedule_slot]);
    world.roam_schedule[schedule_slot].clear();
    for (const std::size_t actor_index : scheduled) {
        if (actor_index >= world.actors.size()) continue;
        WorldActorState& actor = world.actors[actor_index];
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

void select_next_map(WorldState& world) {
    if (!world.maps.empty()) world.current = (world.current + 1U) % world.maps.size();
    if (world.current < world.maps.size()) world.current_space = world.maps[world.current].world_space;
    world.follow_player = false;
    reset_world_view(world);
}

void select_previous_map(WorldState& world) {
    if (world.maps.empty()) return;
    world.current = world.current == 0 ? world.maps.size() - 1U : world.current - 1U;
    world.current_space = world.maps[world.current].world_space;
    world.follow_player = false;
    reset_world_view(world);
}

void toggle_world_view(WorldState& world) {
    world.view = world.view == WorldView::selected ? WorldView::world : WorldView::selected;
    world.follow_player = true;
    reset_world_view(world);
}

void zoom_world_view(WorldState& world, float factor) {
    world.target_zoom = std::clamp(world.target_zoom * factor, 0.05F, 64.0F);
}

void pan_world_view(WorldState& world, float x, float y) {
    world.follow_player = false;
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
    const bool player_in_view =
        world.player.initialized && world.player.map_index < world.maps.size() &&
        ((world.view == WorldView::world &&
          world.maps[world.player.map_index].world_space == world.current_space) ||
         (world.view == WorldView::selected && world.player.map_index == world.current));
    world.follow_player = player_in_view;
    if (world.follow_player) {
        world.target_camera_x = world.player.visual_global_x * 16.0F + 8.0F;
        world.target_camera_y = world.player.visual_global_y * 16.0F + 8.0F;
    } else {
        world.target_camera_x = (left + right) * 0.5F;
        world.target_camera_y = (top + bottom) * 0.5F;
    }
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
    if (world.player.initialized) {
        const WorldMap& map = world.maps[world.player.map_index];
        const float target_x =
            static_cast<float>(map.global_x_tiles / 2 + world.player.x);
        const float target_y =
            static_cast<float>(map.global_y_tiles / 2 + world.player.y);
        const float movement_response =
            1.0F - std::exp(static_cast<float>(-20.0 * bounded));
        world.player.visual_global_x +=
            (target_x - world.player.visual_global_x) * movement_response;
        world.player.visual_global_y +=
            (target_y - world.player.visual_global_y) * movement_response;
        if (world.follow_player) {
            world.target_camera_x = world.player.visual_global_x * 16.0F + 8.0F;
            world.target_camera_y = world.player.visual_global_y * 16.0F + 8.0F;
        }
    }
    for (WorldActorState& actor : world.actors) {
        const WorldMap& map = world.maps[actor.map_index];
        const float target_x = static_cast<float>(map.global_x_tiles / 2 + actor.x);
        const float target_y = static_cast<float>(map.global_y_tiles / 2 + actor.y);
        const float movement_response =
            1.0F - std::exp(static_cast<float>(-16.0 * bounded));
        actor.visual_global_x += (target_x - actor.visual_global_x) * movement_response;
        actor.visual_global_y += (target_y - actor.visual_global_y) * movement_response;
    }
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
