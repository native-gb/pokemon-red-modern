#pragma once

#include "../maps.hpp"

#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

namespace pokered::render {

struct MapRenderResources {
    std::vector<SDL_Texture*> textures;
};

bool upload_map_textures(SDL_Renderer* renderer, const MapBrowser& browser,
                         MapRenderResources& resources);
void destroy_map_textures(MapRenderResources& resources);
bool draw_map_browser(SDL_Renderer* renderer, int output_width, int output_height,
                      const MapBrowser& browser, const MapRenderResources& resources);

} // namespace pokered::render
