#pragma once

#include "maps.hpp"

struct SDL_Renderer;

namespace pokered::render {

struct BootRenderResources;
struct ViewLayout;

bool draw_naming_overlay(SDL_Renderer* renderer, const ViewLayout& view,
                         const WorldState& world,
                         const BootRenderResources& resources);
bool draw_area_banner(SDL_Renderer* renderer, const WorldState& world,
                      const BootRenderResources& resources);

} // namespace pokered::render
