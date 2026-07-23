#include "render/maps.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace pokered::render {
namespace {

std::array<std::uint8_t, 4> map_color(std::uint8_t shade) {
    constexpr std::array<std::array<std::uint8_t, 4>, 4> colors{{
        {224, 248, 208, 255},
        {136, 192, 112, 255},
        {52, 104, 86, 255},
        {8, 24, 32, 255},
    }};
    return colors[shade & 0x03U];
}

SDL_Texture* upload_one_map(SDL_Renderer* renderer, const WorldMap& map,
                            const MapTileset& tileset) {
    const std::size_t pixel_width = static_cast<std::size_t>(map.width_tiles) * 8U;
    const std::size_t pixel_height = static_cast<std::size_t>(map.height_tiles) * 8U;
    std::vector<std::uint8_t> pixels(pixel_width * pixel_height * 4U);

    // Compose the complete map once; steady-state rendering is one GPU texture draw.
    for (std::size_t tile_y = 0; tile_y < map.height_tiles; ++tile_y) {
        for (std::size_t tile_x = 0; tile_x < map.width_tiles; ++tile_x) {
            const std::uint8_t tile =
                map.tiles[tile_y * map.width_tiles + tile_x];
            const std::size_t source = static_cast<std::size_t>(tile) * 64U;
            for (std::size_t y = 0; y < 8; ++y) {
                for (std::size_t x = 0; x < 8; ++x) {
                    const auto color = map_color(tileset.pixels[source + y * 8U + x]);
                    const std::size_t destination =
                        ((tile_y * 8U + y) * pixel_width + tile_x * 8U + x) * 4U;
                    std::copy(color.begin(), color.end(),
                              pixels.begin() + static_cast<std::ptrdiff_t>(destination));
                }
            }
        }
    }

    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        static_cast<int>(pixel_width), static_cast<int>(pixel_height),
        SDL_PIXELFORMAT_RGBA32, pixels.data(), static_cast<int>(pixel_width * 4U));
    if (surface == nullptr) return nullptr;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture != nullptr) (void)SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    return texture;
}

SDL_Texture* upload_animation_tiles(SDL_Renderer* renderer,
                                    const WorldState& world,
                                    WorldRenderResources& resources) {
    constexpr int frame_count = 11;
    constexpr int tile_size = 8;
    resources.animation_atlas_width = frame_count * tile_size;
    resources.animation_atlas_height =
        static_cast<int>(world.tilesets.size()) * tile_size;
    if (resources.animation_atlas_height == 0) return nullptr;
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(resources.animation_atlas_width) *
            static_cast<std::size_t>(resources.animation_atlas_height) * 4U,
        0);
    for (std::size_t row = 0; row < world.tilesets.size(); ++row) {
        const MapTileset& tileset = world.tilesets[row];
        const std::size_t frames = tileset.animation_pixels.size() / 64U;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            for (std::size_t y = 0; y < 8; ++y) {
                for (std::size_t x = 0; x < 8; ++x) {
                    const auto color = map_color(
                        tileset.animation_pixels[frame * 64U + y * 8U + x]);
                    const std::size_t destination =
                        ((row * 8U + y) *
                             static_cast<std::size_t>(
                                 resources.animation_atlas_width) +
                         frame * 8U + x) *
                        4U;
                    std::copy(
                        color.begin(), color.end(),
                        pixels.begin() +
                            static_cast<std::ptrdiff_t>(destination));
                }
            }
        }
    }
    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        resources.animation_atlas_width, resources.animation_atlas_height,
        SDL_PIXELFORMAT_RGBA32, pixels.data(),
        resources.animation_atlas_width * 4);
    if (surface == nullptr) return nullptr;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture != nullptr)
        (void)SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    return texture;
}

void find_animated_tiles(const WorldState& world,
                         WorldRenderResources& resources) {
    resources.animated_tiles.resize(world.maps.size());
    for (std::size_t index = 0; index < world.maps.size(); ++index) {
        const WorldMap& map = world.maps[index];
        const MapTileset* tileset = find_tileset(world, map.tileset_id);
        if (tileset == nullptr || tileset->animation_mode == 0) continue;
        std::vector<AnimatedMapTile>& animated =
            resources.animated_tiles[index];
        for (std::uint16_t y = 0; y < map.height_tiles; ++y) {
            for (std::uint16_t x = 0; x < map.width_tiles; ++x) {
                const std::uint8_t tile =
                    map.tiles[static_cast<std::size_t>(y) * map.width_tiles + x];
                if (tile == 0x14) {
                    animated.push_back(
                        {x, y, AnimatedMapTileKind::water});
                } else if (tile == 0x03 &&
                           tileset->animation_mode == 2) {
                    animated.push_back(
                        {x, y, AnimatedMapTileKind::flower});
                }
            }
        }
    }
}

bool find_world_bounds(const WorldState& world,
                       WorldRenderResources& resources) {
    if (world.maps.empty()) return false;
    std::int32_t min_x = world.maps.front().global_x_tiles;
    std::int32_t min_y = world.maps.front().global_y_tiles;
    std::int32_t max_x =
        min_x + static_cast<std::int32_t>(world.maps.front().width_tiles);
    std::int32_t max_y =
        min_y + static_cast<std::int32_t>(world.maps.front().height_tiles);
    for (const WorldMap& map : world.maps) {
        min_x = std::min(min_x, map.global_x_tiles);
        min_y = std::min(min_y, map.global_y_tiles);
        max_x = std::max(
            max_x,
            map.global_x_tiles + static_cast<std::int32_t>(map.width_tiles));
        max_y = std::max(
            max_y,
            map.global_y_tiles + static_cast<std::int32_t>(map.height_tiles));
    }
    const std::int32_t width_tiles = max_x - min_x;
    const std::int32_t height_tiles = max_y - min_y;
    if (width_tiles <= 0 || height_tiles <= 0 ||
        width_tiles > std::numeric_limits<int>::max() / 8 ||
        height_tiles > std::numeric_limits<int>::max() / 8)
        return false;

    resources.world_min_x_pixels = min_x * 8;
    resources.world_min_y_pixels = min_y * 8;
    resources.world_width_pixels = static_cast<int>(width_tiles) * 8;
    resources.world_height_pixels = static_cast<int>(height_tiles) * 8;
    return true;
}

std::size_t tileset_row(const WorldState& world, std::uint8_t id) {
    for (std::size_t index = 0; index < world.tilesets.size(); ++index) {
        if (world.tilesets[index].id == id) return index;
    }
    return world.tilesets.size();
}

std::size_t water_frame(std::uint64_t tick, std::uint8_t animation_mode) {
    std::uint64_t updates = 0;
    if (animation_mode == 1) {
        updates = tick / 20U;
    } else if (tick >= 20U) {
        updates = 1U + (tick - 20U) / 21U;
    }
    return updates == 0 ? 7U
                        : static_cast<std::size_t>((updates - 1U) % 8U);
}

std::size_t flower_frame(std::uint64_t tick) {
    const std::uint64_t updates =
        tick < 21U ? 0U : 1U + (tick - 21U) / 21U;
    const std::uint64_t phase = updates & 3U;
    return phase < 2U ? 8U : phase == 2U ? 9U : 10U;
}

bool draw_animated_map_tiles(SDL_Renderer* renderer,
                             const WorldState& world,
                             const WorldRenderResources& resources,
                             std::size_t map_index,
                             const SDL_FRect& map_destination,
                             float scale) {
    if (resources.animation_tiles == nullptr ||
        map_index >= world.maps.size() ||
        map_index >= resources.animated_tiles.size())
        return false;
    const WorldMap& map = world.maps[map_index];
    const MapTileset* tileset = find_tileset(world, map.tileset_id);
    const std::size_t row = tileset_row(world, map.tileset_id);
    if (tileset == nullptr || row >= world.tilesets.size()) return false;
    const std::vector<AnimatedMapTile>& animated =
        resources.animated_tiles[map_index];
    if (animated.empty()) return true;

    std::vector<SDL_Vertex> vertices;
    vertices.reserve(animated.size() * 6U);
    const SDL_FColor color{1.0F, 1.0F, 1.0F, 1.0F};
    const float atlas_width =
        static_cast<float>(resources.animation_atlas_width);
    const float atlas_height =
        static_cast<float>(resources.animation_atlas_height);
    for (const AnimatedMapTile& tile : animated) {
        const std::size_t frame =
            tile.kind == AnimatedMapTileKind::water
                ? water_frame(world.animation_tick,
                              tileset->animation_mode)
                : flower_frame(world.animation_tick);
        const float left =
            map_destination.x + static_cast<float>(tile.x) * 8.0F * scale;
        const float top =
            map_destination.y + static_cast<float>(tile.y) * 8.0F * scale;
        const float right = left + 8.0F * scale;
        const float bottom = top + 8.0F * scale;
        const float u0 = static_cast<float>(frame * 8U) / atlas_width;
        const float u1 = static_cast<float>(frame * 8U + 8U) / atlas_width;
        const float v0 = static_cast<float>(row * 8U) / atlas_height;
        const float v1 = static_cast<float>(row * 8U + 8U) / atlas_height;
        vertices.insert(vertices.end(), {
            {{left, top}, color, {u0, v0}},
            {{right, top}, color, {u1, v0}},
            {{right, bottom}, color, {u1, v1}},
            {{left, top}, color, {u0, v0}},
            {{right, bottom}, color, {u1, v1}},
            {{left, bottom}, color, {u0, v1}},
        });
    }
    if (vertices.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return false;
    return SDL_RenderGeometry(
        renderer, resources.animation_tiles, vertices.data(),
        static_cast<int>(vertices.size()), nullptr, 0);
}

} // namespace

bool upload_world_textures(SDL_Renderer* renderer, const WorldState& world,
                           WorldRenderResources& resources) {
    destroy_world_textures(resources);
    if (renderer == nullptr || !world.loaded) return false;
    resources.textures.reserve(world.maps.size());
    for (const WorldMap& map : world.maps) {
        const MapTileset* tileset = find_tileset(world, map.tileset_id);
        if (tileset == nullptr) {
            destroy_world_textures(resources);
            return false;
        }
        SDL_Texture* texture = upload_one_map(renderer, map, *tileset);
        if (texture == nullptr) {
            destroy_world_textures(resources);
            return false;
        }
        resources.textures.push_back(texture);
    }
    resources.animation_tiles =
        upload_animation_tiles(renderer, world, resources);
    if (resources.animation_tiles == nullptr) {
        destroy_world_textures(resources);
        return false;
    }
    find_animated_tiles(world, resources);
    if (!find_world_bounds(world, resources)) {
        destroy_world_textures(resources);
        return false;
    }
    return true;
}

void destroy_world_textures(WorldRenderResources& resources) {
    for (SDL_Texture* texture : resources.textures) SDL_DestroyTexture(texture);
    resources.textures.clear();
    SDL_DestroyTexture(resources.animation_tiles);
    resources.animation_tiles = nullptr;
    resources.animation_atlas_width = 0;
    resources.animation_atlas_height = 0;
    resources.animated_tiles.clear();
    resources.world_min_x_pixels = 0;
    resources.world_min_y_pixels = 0;
    resources.world_width_pixels = 0;
    resources.world_height_pixels = 0;
}

bool draw_world(SDL_Renderer* renderer, int output_width, int output_height,
                const WorldState& world,
                const WorldRenderResources& resources) {
    const WorldMap* map = selected_map(world);
    if (renderer == nullptr || map == nullptr)
        return false;

    const bool show_connected_world = world.view == WorldView::world;
    if (resources.textures.size() != world.maps.size() ||
        world.current >= resources.textures.size())
        return false;
    const float pixel_width =
        show_connected_world
            ? static_cast<float>(resources.world_width_pixels)
            : static_cast<float>(map->width_tiles) * 8.0F;
    const float pixel_height =
        show_connected_world
            ? static_cast<float>(resources.world_height_pixels)
            : static_cast<float>(map->height_tiles) * 8.0F;
    const float available_width =
        std::max(static_cast<float>(output_width) - 64.0F, 1.0F);
    const float available_height =
        std::max(static_cast<float>(output_height) - 96.0F, 1.0F);
    float fit_scale = std::min(available_width / pixel_width,
                               available_height / pixel_height);
    if (!show_connected_world && fit_scale >= 1.0F)
        fit_scale = std::max(1.0F, std::floor(fit_scale));
    const float scale = fit_scale * world.zoom;
    const float view_center_x = static_cast<float>(output_width) * 0.5F;
    const float view_center_y = static_cast<float>(output_height) * 0.5F + 12.0F;
    const float world_origin_x =
        show_connected_world
            ? static_cast<float>(resources.world_min_x_pixels)
            : static_cast<float>(map->global_x_tiles) * 8.0F;
    const float world_origin_y =
        show_connected_world
            ? static_cast<float>(resources.world_min_y_pixels)
            : static_cast<float>(map->global_y_tiles) * 8.0F;
    const SDL_FRect world_rectangle{
        .x = view_center_x + (world_origin_x - world.camera_x) * scale,
        .y = view_center_y + (world_origin_y - world.camera_y) * scale,
        .w = pixel_width * scale,
        .h = pixel_height * scale,
    };
    const SDL_FRect shadow{
        world_rectangle.x - 8.0F,
        world_rectangle.y - 8.0F,
        world_rectangle.w + 16.0F,
        world_rectangle.h + 16.0F,
    };
    (void)SDL_SetRenderDrawColor(renderer, 5, 6, 9, 255);
    (void)SDL_RenderFillRect(renderer, &shadow);

    if (show_connected_world) {
        std::vector<std::pair<std::size_t, SDL_FRect>> visible_chunks;
        visible_chunks.reserve(world.maps.size());
        for (std::size_t index = 0; index < world.maps.size(); ++index) {
            const WorldMap& chunk = world.maps[index];
            SDL_Texture* texture = resources.textures[index];
            if (texture == nullptr) return false;
            const float chunk_x =
                static_cast<float>(chunk.global_x_tiles) * 8.0F;
            const float chunk_y =
                static_cast<float>(chunk.global_y_tiles) * 8.0F;
            const SDL_FRect destination{
                .x = view_center_x + (chunk_x - world.camera_x) * scale,
                .y = view_center_y + (chunk_y - world.camera_y) * scale,
                .w = static_cast<float>(chunk.width_tiles) * 8.0F * scale,
                .h = static_cast<float>(chunk.height_tiles) * 8.0F * scale,
            };
            const bool visible =
                destination.x < static_cast<float>(output_width) &&
                destination.y < static_cast<float>(output_height) &&
                destination.x + destination.w > 0.0F &&
                destination.y + destination.h > 0.0F;
            if (visible &&
                !SDL_RenderTexture(renderer, texture, nullptr, &destination))
                return false;
            if (visible) visible_chunks.emplace_back(index, destination);
        }
        for (const auto& [index, destination] : visible_chunks) {
            if (!draw_animated_map_tiles(renderer, world, resources, index,
                                         destination, scale))
                return false;
        }
    } else {
        SDL_Texture* texture = resources.textures[world.current];
        if (texture == nullptr ||
            !SDL_RenderTexture(renderer, texture, nullptr, &world_rectangle))
            return false;
        if (!draw_animated_map_tiles(renderer, world, resources,
                                     world.current, world_rectangle, scale))
            return false;
    }
    return true;
}

} // namespace pokered::render
