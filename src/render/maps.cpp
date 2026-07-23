#include "render/maps.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
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

bool compose_world_atlas(SDL_Renderer* renderer, const MapBrowser& browser,
                         MapRenderResources& resources) {
    if (browser.maps.empty() ||
        resources.textures.size() != browser.maps.size())
        return false;

    std::int32_t min_x = browser.maps.front().global_x_tiles;
    std::int32_t min_y = browser.maps.front().global_y_tiles;
    std::int32_t max_x =
        min_x + static_cast<std::int32_t>(browser.maps.front().width_tiles);
    std::int32_t max_y =
        min_y + static_cast<std::int32_t>(browser.maps.front().height_tiles);
    for (const WorldMap& map : browser.maps) {
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

    resources.atlas_min_x_tiles = min_x;
    resources.atlas_min_y_tiles = min_y;
    resources.atlas_width = static_cast<int>(width_tiles) * 8;
    resources.atlas_height = static_cast<int>(height_tiles) * 8;
    resources.world_atlas = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
        resources.atlas_width, resources.atlas_height);
    if (resources.world_atlas == nullptr) return false;
    (void)SDL_SetTextureScaleMode(resources.world_atlas, SDL_SCALEMODE_NEAREST);
    (void)SDL_SetTextureBlendMode(resources.world_atlas, SDL_BLENDMODE_BLEND);

    SDL_Texture* previous_target = SDL_GetRenderTarget(renderer);
    if (!SDL_SetRenderTarget(renderer, resources.world_atlas)) return false;
    (void)SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    if (!SDL_RenderClear(renderer)) {
        (void)SDL_SetRenderTarget(renderer, previous_target);
        return false;
    }
    for (std::size_t index = 0; index < browser.maps.size(); ++index) {
        const WorldMap& map = browser.maps[index];
        const SDL_FRect destination{
            .x = static_cast<float>(
                     (map.global_x_tiles - resources.atlas_min_x_tiles) * 8),
            .y = static_cast<float>(
                     (map.global_y_tiles - resources.atlas_min_y_tiles) * 8),
            .w = static_cast<float>(map.width_tiles) * 8.0F,
            .h = static_cast<float>(map.height_tiles) * 8.0F,
        };
        if (!SDL_RenderTexture(renderer, resources.textures[index], nullptr,
                               &destination)) {
            (void)SDL_SetRenderTarget(renderer, previous_target);
            return false;
        }
    }
    return SDL_SetRenderTarget(renderer, previous_target);
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
    if (!compose_world_atlas(renderer, browser, resources)) {
        destroy_map_textures(resources);
        return false;
    }
    return true;
}

void destroy_map_textures(MapRenderResources& resources) {
    for (SDL_Texture* texture : resources.textures) SDL_DestroyTexture(texture);
    resources.textures.clear();
    SDL_DestroyTexture(resources.world_atlas);
    resources.world_atlas = nullptr;
    resources.atlas_min_x_tiles = 0;
    resources.atlas_min_y_tiles = 0;
    resources.atlas_width = 0;
    resources.atlas_height = 0;
}

bool draw_map_browser(SDL_Renderer* renderer, int output_width, int output_height,
                      const MapBrowser& browser,
                      const MapRenderResources& resources) {
    const WorldMap* map = current_map(browser);
    if (renderer == nullptr || map == nullptr)
        return false;

    const bool world = browser.view == MapView::world;
    if ((!world && browser.current >= resources.textures.size()) ||
        (world && resources.world_atlas == nullptr))
        return false;
    SDL_Texture* texture =
        world ? resources.world_atlas : resources.textures[browser.current];
    if (texture == nullptr) return false;
    const float pixel_width =
        world ? static_cast<float>(resources.atlas_width)
              : static_cast<float>(map->width_tiles) * 8.0F;
    const float pixel_height =
        world ? static_cast<float>(resources.atlas_height)
              : static_cast<float>(map->height_tiles) * 8.0F;
    const float available_width =
        std::max(static_cast<float>(output_width) - 64.0F, 1.0F);
    const float available_height =
        std::max(static_cast<float>(output_height) - 96.0F, 1.0F);
    float fit_scale = std::min(available_width / pixel_width,
                               available_height / pixel_height);
    if (!world && fit_scale >= 1.0F)
        fit_scale = std::max(1.0F, std::floor(fit_scale));
    const float scale = fit_scale * browser.zoom;
    const float width = pixel_width * scale;
    const float height = pixel_height * scale;
    const SDL_FRect destination{
        .x = (static_cast<float>(output_width) - width) * 0.5F + browser.pan_x,
        .y = (static_cast<float>(output_height) - height) * 0.5F + 12.0F +
             browser.pan_y,
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
    if (!SDL_RenderTexture(renderer, texture, nullptr, &destination))
        return false;

    if (world) {
        const float map_x = static_cast<float>(
            (map->global_x_tiles - resources.atlas_min_x_tiles) * 8);
        const float map_y = static_cast<float>(
            (map->global_y_tiles - resources.atlas_min_y_tiles) * 8);
        const SDL_FRect selection{
            .x = destination.x + map_x * scale,
            .y = destination.y + map_y * scale,
            .w = static_cast<float>(map->width_tiles) * 8.0F * scale,
            .h = static_cast<float>(map->height_tiles) * 8.0F * scale,
        };
        (void)SDL_SetRenderDrawColor(renderer, 246, 218, 82, 255);
        (void)SDL_RenderRect(renderer, &selection);
    }
    return true;
}

} // namespace pokered::render
