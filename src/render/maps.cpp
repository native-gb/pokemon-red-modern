#include "render/maps.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

} // namespace

bool upload_map_textures(SDL_Renderer* renderer, const MapBrowser& browser,
                         MapRenderResources& resources) {
    destroy_map_textures(resources);
    if (renderer == nullptr || !browser.loaded) return false;
    resources.textures.reserve(browser.maps.size());
    for (const WorldMap& map : browser.maps) {
        const MapTileset* tileset = find_tileset(browser, map.tileset_id);
        if (tileset == nullptr) {
            destroy_map_textures(resources);
            return false;
        }
        SDL_Texture* texture = upload_one_map(renderer, map, *tileset);
        if (texture == nullptr) {
            destroy_map_textures(resources);
            return false;
        }
        resources.textures.push_back(texture);
    }
    return true;
}

void destroy_map_textures(MapRenderResources& resources) {
    for (SDL_Texture* texture : resources.textures) SDL_DestroyTexture(texture);
    resources.textures.clear();
}

bool draw_map_browser(SDL_Renderer* renderer, int output_width, int output_height,
                      const MapBrowser& browser,
                      const MapRenderResources& resources) {
    const WorldMap* map = current_map(browser);
    if (renderer == nullptr || map == nullptr ||
        browser.current >= resources.textures.size())
        return false;

    SDL_Texture* texture = resources.textures[browser.current];
    if (texture == nullptr) return false;
    const float pixel_width = static_cast<float>(map->width_tiles) * 8.0F;
    const float pixel_height = static_cast<float>(map->height_tiles) * 8.0F;
    const float available_width =
        std::max(static_cast<float>(output_width) - 64.0F, 1.0F);
    const float available_height =
        std::max(static_cast<float>(output_height) - 96.0F, 1.0F);
    float scale = std::min(available_width / pixel_width,
                           available_height / pixel_height);
    if (scale >= 1.0F) scale = std::max(1.0F, std::floor(scale));
    const float width = pixel_width * scale;
    const float height = pixel_height * scale;
    const SDL_FRect destination{
        .x = (static_cast<float>(output_width) - width) * 0.5F,
        .y = (static_cast<float>(output_height) - height) * 0.5F + 12.0F,
        .w = width,
        .h = height,
    };
    const SDL_FRect shadow{
        destination.x - 8.0F,
        destination.y - 8.0F,
        destination.w + 16.0F,
        destination.h + 16.0F,
    };
    (void)SDL_SetRenderDrawColor(renderer, 5, 6, 9, 255);
    (void)SDL_RenderFillRect(renderer, &shadow);
    return SDL_RenderTexture(renderer, texture, nullptr, &destination);
}

} // namespace pokered::render
