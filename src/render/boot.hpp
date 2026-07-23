#pragma once

#include "../boot.hpp"

#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

namespace pokered::render {

struct ViewLayout;

struct BootRenderResources {
    std::vector<SDL_Texture*> images;
    SDL_Texture* ui_tiles{};
};

bool upload_boot_textures(SDL_Renderer* renderer, const BootContent& content,
                          BootRenderResources& resources);
void destroy_boot_textures(BootRenderResources& resources);
bool draw_boot(SDL_Renderer* renderer, const ViewLayout& view,
               const BootContent& content, const BootState& state,
               const BootRenderResources& resources);

} // namespace pokered::render
