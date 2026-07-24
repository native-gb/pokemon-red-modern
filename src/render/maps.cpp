#include "render/maps.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

namespace pokered::render {
namespace {

constexpr int kTileSize = 8;
constexpr int kAtlasColumns = 16;
constexpr int kChunkSizeTiles = 32;
constexpr int kChunkSizePixels = kChunkSizeTiles * kTileSize;
constexpr int kTerrainPageSize = 2048;
constexpr int kChunksPerPageRow = kTerrainPageSize / kChunkSizePixels;
constexpr int kChunksPerPage = kChunksPerPageRow * kChunksPerPageRow;
constexpr int kActorSize = 16;
constexpr int kActorAtlasColumns = 16;

struct TileBatch {
    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;
};

struct ChunkKey {
    std::uint16_t component{};
    std::int32_t x{};
    std::int32_t y{};
    std::size_t tileset_index{};

    bool operator<(const ChunkKey& other) const {
        return std::tie(component, y, x, tileset_index) <
               std::tie(other.component, other.y, other.x, other.tileset_index);
    }
};

struct ChunkTiles {
    std::array<std::uint8_t, static_cast<std::size_t>(kChunkSizeTiles* kChunkSizeTiles)> tiles{};
    std::array<bool, static_cast<std::size_t>(kChunkSizeTiles* kChunkSizeTiles)> occupied{};
    int min_x{kChunkSizeTiles};
    int min_y{kChunkSizeTiles};
    int max_x{-1};
    int max_y{-1};
};

std::array<std::uint8_t, 4> map_color(std::uint8_t shade) {
    constexpr std::array<std::array<std::uint8_t, 4>, 4> colors{{
        {224, 248, 208, 255},
        {136, 192, 112, 255},
        {52, 104, 86, 255},
        {8, 24, 32, 255},
    }};
    return colors[shade & 0x03U];
}

SDL_Texture* upload_tileset(SDL_Renderer* renderer, const MapTileset& tileset,
                            TilesetRenderResources& resources) {
    const std::size_t animation_frames = tileset.animation_pixels.size() / 64U;
    const std::size_t total_tiles = static_cast<std::size_t>(tileset.tile_count) + animation_frames;
    if (total_tiles == 0 || total_tiles > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return nullptr;

    resources.columns = kAtlasColumns;
    resources.rows = static_cast<int>((total_tiles + static_cast<std::size_t>(kAtlasColumns) - 1U) /
                                      static_cast<std::size_t>(kAtlasColumns));
    const int pixel_width = resources.columns * kTileSize;
    const int pixel_height = resources.rows * kTileSize;
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(pixel_width) * static_cast<std::size_t>(pixel_height) * 4U, 0);

    const auto copy_tile = [&](std::size_t atlas_tile, const std::vector<std::uint8_t>& source,
                               std::size_t source_tile) {
        const std::size_t tile_x = atlas_tile % static_cast<std::size_t>(resources.columns);
        const std::size_t tile_y = atlas_tile / static_cast<std::size_t>(resources.columns);
        for (std::size_t y = 0; y < 8; ++y) {
            for (std::size_t x = 0; x < 8; ++x) {
                const auto color = map_color(source[source_tile * 64U + y * 8U + x]);
                const std::size_t destination =
                    ((tile_y * 8U + y) * static_cast<std::size_t>(pixel_width) + tile_x * 8U + x) *
                    4U;
                std::copy(color.begin(), color.end(),
                          pixels.begin() + static_cast<std::ptrdiff_t>(destination));
            }
        }
    };
    for (std::size_t tile = 0; tile < tileset.tile_count; ++tile)
        copy_tile(tile, tileset.pixels, tile);
    for (std::size_t frame = 0; frame < animation_frames; ++frame)
        copy_tile(static_cast<std::size_t>(tileset.tile_count) + frame, tileset.animation_pixels,
                  frame);

    SDL_Surface* surface = SDL_CreateSurfaceFrom(pixel_width, pixel_height, SDL_PIXELFORMAT_RGBA32,
                                                 pixels.data(), pixel_width * 4);
    if (surface == nullptr) return nullptr;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture != nullptr) (void)SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    return texture;
}

SDL_Texture* upload_actor_atlas(SDL_Renderer* renderer, const WorldState& world, int& columns) {
    if (world.sprites.empty()) return nullptr;
    const std::size_t frame_count = world.sprites.size() * 16U;
    columns = kActorAtlasColumns;
    const int rows = static_cast<int>((frame_count + static_cast<std::size_t>(columns) - 1U) /
                                      static_cast<std::size_t>(columns));
    const int width = columns * kActorSize;
    const int height = rows * kActorSize;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height) * 4U, 0);
    for (const WorldSprite& sprite : world.sprites) {
        if (sprite.id == 0 ||
            sprite.pixels.size() != 16U * 16U * 16U)
            return nullptr;
        for (std::size_t facing = 0; facing < 4U; ++facing) {
            for (std::size_t phase = 0U; phase < 4U; ++phase) {
                const std::size_t source_frame =
                    facing * 4U + phase;
                const std::size_t frame =
                    (static_cast<std::size_t>(sprite.id) - 1U) *
                        16U +
                    source_frame;
                const std::size_t frame_x =
                    frame % static_cast<std::size_t>(columns);
                const std::size_t frame_y =
                    frame / static_cast<std::size_t>(columns);
                for (std::size_t y = 0; y < 16U; ++y) {
                    for (std::size_t x = 0; x < 16U; ++x) {
                        const std::uint8_t shade =
                            sprite.pixels[source_frame * 256U +
                                          y * 16U + x];
                        if (shade == 0) continue;
                        const auto color = map_color(shade);
                        const std::size_t target =
                            ((frame_y * 16U + y) *
                                 static_cast<std::size_t>(width) +
                             frame_x * 16U + x) *
                            4U;
                        std::copy(
                            color.begin(), color.end(),
                            pixels.begin() +
                                static_cast<std::ptrdiff_t>(
                                    target));
                    }
                }
            }
        }
    }
    SDL_Surface* surface =
        SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, pixels.data(), width * 4);
    if (surface == nullptr) return nullptr;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (texture != nullptr) {
        (void)SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        (void)SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
    return texture;
}

std::size_t tileset_index(const WorldState& world, std::uint8_t id) {
    for (std::size_t index = 0; index < world.tilesets.size(); ++index) {
        if (world.tilesets[index].id == id) return index;
    }
    return world.tilesets.size();
}

std::int32_t floor_divide(std::int32_t value, std::int32_t divisor) {
    const std::int32_t quotient = value / divisor;
    const std::int32_t remainder = value % divisor;
    return remainder < 0 ? quotient - 1 : quotient;
}

bool is_animated_tile(const MapTileset& tileset, std::uint8_t tile) {
    return (tileset.animation_mode != 0 && tile == 0x14) ||
           (tileset.animation_mode == 2 && tile == 0x03);
}

std::size_t rendered_tile(const MapTileset& tileset, std::uint8_t tile,
                          std::uint64_t animation_tick);
std::uint64_t world_animation_signature(const WorldState& world);

void copy_tile_pixels(std::vector<std::uint8_t>& destination, int destination_width,
                      int destination_x, int destination_y, const MapTileset& tileset,
                      std::size_t tile) {
    const bool animation_frame = tile >= static_cast<std::size_t>(tileset.tile_count);
    const std::vector<std::uint8_t>& source_pixels =
        animation_frame ? tileset.animation_pixels : tileset.pixels;
    const std::size_t source_tile =
        animation_frame ? tile - static_cast<std::size_t>(tileset.tile_count) : tile;
    for (int y = 0; y < kTileSize; ++y) {
        for (int x = 0; x < kTileSize; ++x) {
            const std::size_t source =
                source_tile * 64U + static_cast<std::size_t>(y * kTileSize + x);
            const auto color = map_color(source_pixels[source]);
            const std::size_t target =
                static_cast<std::size_t>((destination_y + y) * destination_width + destination_x +
                                         x) *
                4U;
            std::copy(color.begin(), color.end(),
                      destination.begin() + static_cast<std::ptrdiff_t>(target));
        }
    }
}

bool upload_terrain_chunks(SDL_Renderer* renderer, const WorldState& world,
                           WorldRenderResources& resources) {
    std::map<ChunkKey, ChunkTiles> chunks;
    for (const WorldMap& map : world.maps) {
        const std::size_t set_index = tileset_index(world, map.tileset_id);
        if (set_index >= world.tilesets.size()) return false;
        for (std::uint16_t y = 0; y < map.height_tiles; ++y) {
            for (std::uint16_t x = 0; x < map.width_tiles; ++x) {
                const std::uint8_t tile =
                    map.tiles[static_cast<std::size_t>(y) * map.width_tiles + x];
                const std::int32_t world_x = map.global_x_tiles + static_cast<std::int32_t>(x);
                const std::int32_t world_y = map.global_y_tiles + static_cast<std::int32_t>(y);
                const std::int32_t chunk_x = floor_divide(world_x, kChunkSizeTiles);
                const std::int32_t chunk_y = floor_divide(world_y, kChunkSizeTiles);
                const ChunkKey key{
                    .component = map.world_space,
                    .x = chunk_x,
                    .y = chunk_y,
                    .tileset_index = set_index,
                };
                ChunkTiles& chunk = chunks[key];
                const int local_x = static_cast<int>(world_x - chunk_x * kChunkSizeTiles);
                const int local_y = static_cast<int>(world_y - chunk_y * kChunkSizeTiles);
                const std::size_t local =
                    static_cast<std::size_t>(local_y * kChunkSizeTiles + local_x);
                chunk.tiles[local] = tile;
                chunk.occupied[local] = true;
                chunk.min_x = std::min(chunk.min_x, local_x);
                chunk.min_y = std::min(chunk.min_y, local_y);
                chunk.max_x = std::max(chunk.max_x, local_x);
                chunk.max_y = std::max(chunk.max_y, local_y);
            }
        }
    }

    // Pack chunks into texture pages so SDL can batch the visible blits.
    // Page placement is a cache detail and has no gameplay meaning.
    resources.terrain_chunks.reserve(chunks.size());
    std::vector<std::uint8_t> page_pixels(
        static_cast<std::size_t>(kTerrainPageSize * kTerrainPageSize) * 4U, 0);
    std::size_t page_slot = 0;
    const auto upload_page = [&]() {
        SDL_Surface* surface =
            SDL_CreateSurfaceFrom(kTerrainPageSize, kTerrainPageSize, SDL_PIXELFORMAT_RGBA32,
                                  page_pixels.data(), kTerrainPageSize * 4);
        if (surface == nullptr) return false;
        SDL_Texture* staging = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_DestroySurface(surface);
        if (staging == nullptr) return false;
        (void)SDL_SetTextureBlendMode(staging, SDL_BLENDMODE_NONE);
        SDL_Texture* texture =
            SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                              kTerrainPageSize, kTerrainPageSize);
        if (texture == nullptr) {
            SDL_DestroyTexture(staging);
            return false;
        }
        SDL_Texture* previous = SDL_GetRenderTarget(renderer);
        const bool copied = SDL_SetRenderTarget(renderer, texture) &&
                            SDL_RenderTexture(renderer, staging, nullptr, nullptr);
        const bool restored = SDL_SetRenderTarget(renderer, previous);
        SDL_DestroyTexture(staging);
        if (!copied || !restored) {
            SDL_DestroyTexture(texture);
            return false;
        }
        (void)SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        (void)SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        resources.terrain_pages.push_back(texture);
        std::fill(page_pixels.begin(), page_pixels.end(), 0);
        page_slot = 0;
        return true;
    };

    for (const auto& [key, chunk] : chunks) {
        const int width_tiles = chunk.max_x - chunk.min_x + 1;
        const int height_tiles = chunk.max_y - chunk.min_y + 1;
        const int pixel_width = width_tiles * kTileSize;
        const int pixel_height = height_tiles * kTileSize;
        const int source_x = static_cast<int>(page_slot % kChunksPerPageRow) * kChunkSizePixels;
        const int source_y = static_cast<int>(page_slot / kChunksPerPageRow) * kChunkSizePixels;
        TerrainChunk uploaded{
            .world_space = key.component,
            .texture_page = resources.terrain_pages.size(),
            .source_x = source_x,
            .source_y = source_y,
            .origin_x_tiles = key.x * kChunkSizeTiles + chunk.min_x,
            .origin_y_tiles = key.y * kChunkSizeTiles + chunk.min_y,
            .width_pixels = pixel_width,
            .height_pixels = pixel_height,
        };

        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(pixel_width * pixel_height) * 4U,
                                         0);
        const MapTileset& tileset = world.tilesets[key.tileset_index];
        for (int y = chunk.min_y; y <= chunk.max_y; ++y) {
            for (int x = chunk.min_x; x <= chunk.max_x; ++x) {
                const std::size_t local = static_cast<std::size_t>(y * kChunkSizeTiles + x);
                if (!chunk.occupied[local]) continue;
                copy_tile_pixels(pixels, pixel_width, (x - chunk.min_x) * kTileSize,
                                 (y - chunk.min_y) * kTileSize, tileset, chunk.tiles[local]);
                if (is_animated_tile(tileset, chunk.tiles[local])) {
                    resources.animated_tiles.push_back({
                        .texture_page = resources.terrain_pages.size(),
                        .tileset_index = key.tileset_index,
                        .target_x = source_x + (x - chunk.min_x) * kTileSize,
                        .target_y = source_y + (y - chunk.min_y) * kTileSize,
                        .tile = chunk.tiles[local],
                    });
                }
            }
        }
        for (int y = 0; y < pixel_height; ++y) {
            const std::size_t source = static_cast<std::size_t>(y * pixel_width) * 4U;
            const std::size_t destination =
                static_cast<std::size_t>((source_y + y) * kTerrainPageSize + source_x) * 4U;
            std::copy_n(pixels.begin() + static_cast<std::ptrdiff_t>(source),
                        static_cast<std::size_t>(pixel_width) * 4U,
                        page_pixels.begin() + static_cast<std::ptrdiff_t>(destination));
        }
        resources.terrain_chunks.push_back(std::move(uploaded));
        ++page_slot;
        if (page_slot == kChunksPerPage && !upload_page()) return false;
    }
    if (page_slot != 0 && !upload_page()) return false;
    resources.animation_cache_valid = false;
    return true;
}

std::size_t water_frame(std::uint64_t tick, std::uint8_t animation_mode) {
    std::uint64_t updates = 0;
    if (animation_mode == 1) {
        updates = tick / 20U;
    } else if (tick >= 20U) {
        updates = 1U + (tick - 20U) / 21U;
    }
    return updates == 0 ? 7U : static_cast<std::size_t>((updates - 1U) % 8U);
}

std::size_t flower_frame(std::uint64_t tick) {
    const std::uint64_t updates = tick < 21U ? 0U : 1U + (tick - 21U) / 21U;
    const std::uint64_t phase = updates & 3U;
    return phase < 2U ? 8U : phase == 2U ? 9U : 10U;
}

std::size_t rendered_tile(const MapTileset& tileset, std::uint8_t tile,
                          std::uint64_t animation_tick) {
    if (tileset.animation_mode != 0 && tile == 0x14) {
        return static_cast<std::size_t>(tileset.tile_count) +
               water_frame(animation_tick, tileset.animation_mode);
    }
    if (tileset.animation_mode == 2 && tile == 0x03) {
        return static_cast<std::size_t>(tileset.tile_count) + flower_frame(animation_tick);
    }
    return tile;
}

std::uint64_t world_animation_signature(const WorldState& world) {
    std::uint64_t signature = 1469598103934665603ULL;
    for (const MapTileset& tileset : world.tilesets) {
        if (tileset.animation_mode == 0) continue;
        signature ^= rendered_tile(tileset, 0x14, world.animation_tick);
        signature *= 1099511628211ULL;
        if (tileset.animation_mode == 2) {
            signature ^= rendered_tile(tileset, 0x03, world.animation_tick);
            signature *= 1099511628211ULL;
        }
    }
    return signature;
}

void append_tile(TileBatch& batch, const TilesetRenderResources& resources, std::size_t tile,
                 float left, float top, float size) {
    if (batch.vertices.size() > static_cast<std::size_t>(std::numeric_limits<int>::max() - 4))
        return;
    const int first = static_cast<int>(batch.vertices.size());
    const float right = left + size;
    const float bottom = top + size;
    const std::size_t column = tile % static_cast<std::size_t>(resources.columns);
    const std::size_t row = tile / static_cast<std::size_t>(resources.columns);
    const float atlas_width = static_cast<float>(resources.columns * kTileSize);
    const float atlas_height = static_cast<float>(resources.rows * kTileSize);
    const float u0 = static_cast<float>(column * static_cast<std::size_t>(kTileSize)) / atlas_width;
    const float u1 = static_cast<float>(column * static_cast<std::size_t>(kTileSize) +
                                        static_cast<std::size_t>(kTileSize)) /
                     atlas_width;
    const float v0 = static_cast<float>(row * static_cast<std::size_t>(kTileSize)) / atlas_height;
    const float v1 = static_cast<float>(row * static_cast<std::size_t>(kTileSize) +
                                        static_cast<std::size_t>(kTileSize)) /
                     atlas_height;
    constexpr SDL_FColor color{1.0F, 1.0F, 1.0F, 1.0F};
    batch.vertices.insert(batch.vertices.end(), {
                                                    {{left, top}, color, {u0, v0}},
                                                    {{right, top}, color, {u1, v0}},
                                                    {{right, bottom}, color, {u1, v1}},
                                                    {{left, bottom}, color, {u0, v1}},
                                                });
    batch.indices.insert(batch.indices.end(), {
                                                  first,
                                                  first + 1,
                                                  first + 2,
                                                  first,
                                                  first + 2,
                                                  first + 3,
                                              });
}

void append_visible_map(const WorldState& world, const WorldRenderResources& resources,
                        const WorldMap& map, int output_width, int output_height,
                        float view_center_x, float view_center_y, float scale,
                        std::vector<TileBatch>& batches) {
    const std::size_t set_index = tileset_index(world, map.tileset_id);
    if (set_index >= world.tilesets.size() || set_index >= resources.tilesets.size()) return;
    const MapTileset& tileset = world.tilesets[set_index];
    const float tile_size = static_cast<float>(kTileSize) * scale;
    const float map_left =
        view_center_x +
        (static_cast<float>(map.global_x_tiles * kTileSize) - world.camera_x) * scale;
    const float map_top =
        view_center_y +
        (static_cast<float>(map.global_y_tiles * kTileSize) - world.camera_y) * scale;
    const float map_right = map_left + static_cast<float>(map.width_tiles) * tile_size;
    const float map_bottom = map_top + static_cast<float>(map.height_tiles) * tile_size;
    if (map_left >= static_cast<float>(output_width) ||
        map_top >= static_cast<float>(output_height) || map_right <= 0.0F || map_bottom <= 0.0F)
        return;

    const auto visible_begin = [](float origin, float tile_extent) {
        if (origin >= 0.0F) return 0;
        return std::max(static_cast<int>(std::floor(-origin / tile_extent)), 0);
    };
    const auto visible_end = [](float origin, float tile_extent, int limit, int output_extent) {
        return std::clamp(
            static_cast<int>(std::ceil((static_cast<float>(output_extent) - origin) / tile_extent)),
            0, limit);
    };
    const int first_x = visible_begin(map_left, tile_size);
    const int first_y = visible_begin(map_top, tile_size);
    const int end_x =
        visible_end(map_left, tile_size, static_cast<int>(map.width_tiles), output_width);
    const int end_y =
        visible_end(map_top, tile_size, static_cast<int>(map.height_tiles), output_height);
    TileBatch& batch = batches[set_index];
    const std::size_t visible_tiles = static_cast<std::size_t>(std::max(end_x - first_x, 0)) *
                                      static_cast<std::size_t>(std::max(end_y - first_y, 0));
    batch.vertices.reserve(batch.vertices.size() + visible_tiles * 4U);
    batch.indices.reserve(batch.indices.size() + visible_tiles * 6U);
    for (int y = first_y; y < end_y; ++y) {
        for (int x = first_x; x < end_x; ++x) {
            const std::uint8_t tile = map.tiles[static_cast<std::size_t>(y) * map.width_tiles +
                                                static_cast<std::size_t>(x)];
            append_tile(batch, resources.tilesets[set_index],
                        rendered_tile(tileset, tile, world.animation_tick),
                        map_left + static_cast<float>(x) * tile_size,
                        map_top + static_cast<float>(y) * tile_size, tile_size);
        }
    }
}

bool draw_batches(SDL_Renderer* renderer, const WorldRenderResources& resources,
                  const std::vector<TileBatch>& batches) {
    for (std::size_t index = 0; index < batches.size(); ++index) {
        const TileBatch& batch = batches[index];
        if (batch.vertices.empty()) continue;
        if (index >= resources.tilesets.size() || resources.tilesets[index].texture == nullptr ||
            batch.vertices.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
            batch.indices.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            return false;
        if (!SDL_RenderGeometry(renderer, resources.tilesets[index].texture, batch.vertices.data(),
                                static_cast<int>(batch.vertices.size()), batch.indices.data(),
                                static_cast<int>(batch.indices.size())))
            return false;
    }
    return true;
}

bool draw_visible_chunks(SDL_Renderer* renderer, int output_width, int output_height,
                         const WorldState& world, const WorldRenderResources& resources,
                         float view_center_x, float view_center_y, float scale) {
    for (const TerrainChunk& chunk : resources.terrain_chunks) {
        if (chunk.texture_page >= resources.terrain_pages.size()) return false;
        if (chunk.world_space != world.current_space) continue;
        const SDL_FRect source{
            .x = static_cast<float>(chunk.source_x),
            .y = static_cast<float>(chunk.source_y),
            .w = static_cast<float>(chunk.width_pixels),
            .h = static_cast<float>(chunk.height_pixels),
        };
        SDL_FRect destination{
            .x = view_center_x +
                 (static_cast<float>(chunk.origin_x_tiles * kTileSize) - world.camera_x) * scale,
            .y = view_center_y +
                 (static_cast<float>(chunk.origin_y_tiles * kTileSize) - world.camera_y) * scale,
            .w = static_cast<float>(chunk.width_pixels) * scale,
            .h = static_cast<float>(chunk.height_pixels) * scale,
        };
        if (destination.x >= static_cast<float>(output_width) ||
            destination.y >= static_cast<float>(output_height) ||
            destination.x + destination.w <= 0.0F || destination.y + destination.h <= 0.0F)
            continue;
        if (!SDL_RenderTexture(renderer, resources.terrain_pages[chunk.texture_page], &source,
                               &destination))
            return false;
    }
    return true;
}

std::size_t actor_facing(std::uint8_t direction_or_axis) {
    if (direction_or_axis == 0xD1U) return 1U;
    if (direction_or_axis == 0xD2U) return 2U;
    if (direction_or_axis == 0xD3U) return 3U;
    return 0U;
}

std::size_t actor_facing(WorldDirection direction) {
    if (direction == WorldDirection::up) return 1U;
    if (direction == WorldDirection::left) return 2U;
    if (direction == WorldDirection::right) return 3U;
    return 0U;
}

bool draw_world_actors(SDL_Renderer* renderer, int output_width, int output_height,
                       const WorldState& world, const WorldRenderResources& resources,
                       const WorldProjection& projection) {
    if (resources.actor_atlas == nullptr || resources.actor_atlas_columns <= 0) return false;
    const WorldMap* selected = selected_map(world);
    const auto draw_actor = [&](std::uint8_t sprite_id,
                                WorldDirection facing,
                                std::uint8_t animation_phase,
                                float global_x, float global_y) {
        const std::size_t frame =
            (static_cast<std::size_t>(sprite_id) - 1U) * 16U +
            actor_facing(facing) * 4U +
            std::min<std::uint8_t>(animation_phase, 3U);
        const SDL_FRect source{
            .x = static_cast<float>(
                frame % static_cast<std::size_t>(resources.actor_atlas_columns) * 16U),
            .y = static_cast<float>(
                frame / static_cast<std::size_t>(resources.actor_atlas_columns) * 16U),
            .w = 16.0F,
            .h = 16.0F,
        };
        const SDL_FRect destination{
            .x = projection.center_x + (global_x * 16.0F - world.camera_x) * projection.scale,
            .y = projection.center_y + (global_y * 16.0F - world.camera_y) * projection.scale,
            .w = 16.0F * projection.scale,
            .h = 16.0F * projection.scale,
        };
        if (destination.x >= static_cast<float>(output_width) ||
            destination.y >= static_cast<float>(output_height) ||
            destination.x + destination.w <= 0.0F || destination.y + destination.h <= 0.0F)
            return true;
        return SDL_RenderTexture(renderer, resources.actor_atlas, &source, &destination);
    };

    if (world.actors.empty()) {
        for (const WorldMap& map : world.maps) {
            if ((world.view == WorldView::selected && &map != selected) ||
                (world.view == WorldView::world && map.world_space != world.current_space))
                continue;
            for (const WorldActorSpawn& actor : map.actors) {
                const float global_x =
                    static_cast<float>(map.global_x_tiles / 2 + static_cast<int>(actor.x));
                const float global_y =
                    static_cast<float>(map.global_y_tiles / 2 + static_cast<int>(actor.y));
                if (!draw_actor(
                        actor.sprite_id,
                        static_cast<WorldDirection>(
                            actor_facing(actor.direction_or_axis)),
                        0U, global_x, global_y))
                    return false;
            }
        }
    } else {
        for (const WorldActorState& actor : world.actors) {
            if (!actor.visible || actor.map_index >= world.maps.size())
                continue;
            const WorldMap& actor_map = world.maps[actor.map_index];
            if ((world.view == WorldView::selected && &actor_map != selected) ||
                (world.view == WorldView::world &&
                 actor_map.world_space != world.current_space))
                continue;
            const WorldActorSpawn& spawn =
                world.maps[actor.map_index].actors[actor.spawn_index];
            if (!draw_actor(
                    spawn.sprite_id, actor.facing,
                    actor.animation_phase, actor.visual_global_x,
                    actor.visual_global_y))
                return false;
        }
    }
    if (world.player.initialized && world.player.map_index < world.maps.size() &&
        ((world.view == WorldView::world &&
          world.maps[world.player.map_index].world_space == world.current_space) ||
         (world.player.map_index < world.maps.size() &&
          &world.maps[world.player.map_index] == selected)) &&
        !draw_actor(
            1U, world.player.facing, world.player.animation_phase,
            world.player.visual_global_x,
            world.player.visual_global_y +
                world.player.visual_offset_y_pixels / 16.0F))
        return false;
    return true;
}

bool update_animated_pages(SDL_Renderer* renderer, const WorldState& world,
                           WorldRenderResources& resources) {
    const std::uint64_t signature = world_animation_signature(world);
    if (resources.animation_cache_valid && resources.animation_signature == signature) return true;

    SDL_Texture* previous = SDL_GetRenderTarget(renderer);
    std::size_t current_page = resources.terrain_pages.size();
    bool updated = true;
    for (const AnimatedWorldTile& tile : resources.animated_tiles) {
        if (tile.texture_page >= resources.terrain_pages.size() ||
            tile.tileset_index >= world.tilesets.size() ||
            tile.tileset_index >= resources.tilesets.size()) {
            updated = false;
            break;
        }
        if (tile.texture_page != current_page) {
            current_page = tile.texture_page;
            if (!SDL_SetRenderTarget(renderer, resources.terrain_pages[current_page])) {
                updated = false;
                break;
            }
        }
        const std::size_t rendered =
            rendered_tile(world.tilesets[tile.tileset_index], tile.tile, world.animation_tick);
        const TilesetRenderResources& tileset = resources.tilesets[tile.tileset_index];
        const SDL_FRect source{
            .x = static_cast<float>(rendered % static_cast<std::size_t>(tileset.columns)) *
                 kTileSize,
            .y = static_cast<float>(rendered / static_cast<std::size_t>(tileset.columns)) *
                 kTileSize,
            .w = static_cast<float>(kTileSize),
            .h = static_cast<float>(kTileSize),
        };
        const SDL_FRect destination{
            .x = static_cast<float>(tile.target_x),
            .y = static_cast<float>(tile.target_y),
            .w = static_cast<float>(kTileSize),
            .h = static_cast<float>(kTileSize),
        };
        if (!SDL_RenderTexture(renderer, tileset.texture, &source, &destination)) {
            updated = false;
            break;
        }
    }
    const bool restored = SDL_SetRenderTarget(renderer, previous);
    if (!updated || !restored) return false;
    resources.animation_signature = signature;
    resources.animation_cache_valid = true;
    return true;
}

} // namespace

bool upload_world_textures(SDL_Renderer* renderer, const WorldState& world,
                           WorldRenderResources& resources) {
    destroy_world_textures(resources);
    if (renderer == nullptr || !world.loaded) return false;
    resources.tilesets.resize(world.tilesets.size());
    for (std::size_t index = 0; index < world.tilesets.size(); ++index) {
        TilesetRenderResources& uploaded = resources.tilesets[index];
        uploaded.texture = upload_tileset(renderer, world.tilesets[index], uploaded);
        if (uploaded.texture == nullptr) {
            destroy_world_textures(resources);
            return false;
        }
    }
    resources.actor_atlas = upload_actor_atlas(renderer, world, resources.actor_atlas_columns);
    if (resources.actor_atlas == nullptr) {
        destroy_world_textures(resources);
        return false;
    }
    if (!upload_terrain_chunks(renderer, world, resources)) {
        destroy_world_textures(resources);
        return false;
    }
    return true;
}

void destroy_world_textures(WorldRenderResources& resources) {
    for (TilesetRenderResources& tileset : resources.tilesets)
        SDL_DestroyTexture(tileset.texture);
    for (SDL_Texture* page : resources.terrain_pages)
        SDL_DestroyTexture(page);
    SDL_DestroyTexture(resources.actor_atlas);
    resources.tilesets.clear();
    resources.actor_atlas = nullptr;
    resources.actor_atlas_columns = 0;
    resources.terrain_pages.clear();
    resources.terrain_chunks.clear();
    resources.animated_tiles.clear();
    resources.animation_signature = 0;
    resources.animation_cache_valid = false;
}

WorldProjection world_projection(int output_width, int output_height, const WorldState& world) {
    const WorldMap* map = selected_map(world);
    if (map == nullptr) return {};
    return {
        .center_x = static_cast<float>(output_width) * 0.5F,
        .center_y = static_cast<float>(output_height) * 0.5F + 12.0F,
        // Zoom is an absolute output-pixels-per-imported-pixel scale. It does
        // not inherit the radically different fit scale of Kanto versus an
        // interior complex.
        .scale = world.zoom,
    };
}

bool draw_world(SDL_Renderer* renderer, int output_width, int output_height,
                const WorldState& world, WorldRenderResources& resources) {
    const WorldMap* map = selected_map(world);
    if (renderer == nullptr || map == nullptr || resources.tilesets.size() != world.tilesets.size())
        return false;

    const bool show_connected_world = world.view == WorldView::world;
    const WorldProjection projection = world_projection(output_width, output_height, world);
    if (projection.scale <= 0.0F) return false;

    if (show_connected_world) {
        return update_animated_pages(renderer, world, resources) &&
               draw_visible_chunks(renderer, output_width, output_height, world, resources,
                                   projection.center_x, projection.center_y, projection.scale) &&
               draw_world_actors(renderer, output_width, output_height, world, resources,
                                 projection);
    }
    std::vector<TileBatch> batches(world.tilesets.size());
    append_visible_map(world, resources, *map, output_width, output_height, projection.center_x,
                       projection.center_y, projection.scale, batches);
    return draw_batches(renderer, resources, batches) &&
           draw_world_actors(renderer, output_width, output_height, world, resources, projection);
}

} // namespace pokered::render
