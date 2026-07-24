#pragma once

#include "../maps.hpp"

struct SDL_Renderer;

namespace pokered::render {

struct BootRenderResources;

bool draw_dialogue_overlay(
    SDL_Renderer* renderer, int output_width, int output_height,
    const WorldState& world,
    const BootRenderResources& resources);

} // namespace pokered::render
