#pragma once

#include "../maps.hpp"

#include <cstdint>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

namespace pokered::render {

enum class AnimatedMapTileKind : std::uint8_t {
    water,
    flower,
};

struct AnimatedMapTile {
    std::uint16_t x{};
    std::uint16_t y{};
    AnimatedMapTileKind kind{AnimatedMapTileKind::water};
};

struct WorldRenderResources {
    std::vector<SDL_Texture*> textures;
    SDL_Texture* animation_tiles{};
    int animation_atlas_width{};
    int animation_atlas_height{};
    std::vector<std::vector<AnimatedMapTile>> animated_tiles;
    std::int32_t world_min_x_pixels{};
    std::int32_t world_min_y_pixels{};
    int world_width_pixels{};
    int world_height_pixels{};
};

bool upload_world_textures(SDL_Renderer* renderer, const WorldState& world,
                           WorldRenderResources& resources);
void destroy_world_textures(WorldRenderResources& resources);
bool draw_world(SDL_Renderer* renderer, int output_width, int output_height,
                const WorldState& world,
                const WorldRenderResources& resources);

} // namespace pokered::render
