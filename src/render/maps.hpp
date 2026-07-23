#pragma once

#include "../maps.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

namespace pokered::render {

struct TilesetRenderResources {
    SDL_Texture* texture{};
    int columns{};
    int rows{};
};

struct TerrainChunk {
    std::size_t texture_page{};
    int source_x{};
    int source_y{};
    std::int32_t origin_x_tiles{};
    std::int32_t origin_y_tiles{};
    int width_pixels{};
    int height_pixels{};
};

struct AnimatedWorldTile {
    std::size_t texture_page{};
    std::size_t tileset_index{};
    int target_x{};
    int target_y{};
    std::uint8_t tile{};
};

struct WorldRenderResources {
    std::vector<TilesetRenderResources> tilesets;
    std::vector<SDL_Texture*> terrain_pages;
    std::vector<TerrainChunk> terrain_chunks;
    std::vector<AnimatedWorldTile> animated_tiles;
    std::uint64_t animation_signature{};
    bool animation_cache_valid{};
    int world_width_pixels{};
    int world_height_pixels{};
};

bool upload_world_textures(SDL_Renderer* renderer, const WorldState& world,
                           WorldRenderResources& resources);
void destroy_world_textures(WorldRenderResources& resources);
bool draw_world(SDL_Renderer* renderer, int output_width, int output_height,
                const WorldState& world, WorldRenderResources& resources);

} // namespace pokered::render
