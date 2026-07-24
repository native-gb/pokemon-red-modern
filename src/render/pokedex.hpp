#pragma once

#include "../battle_animation_lab.hpp"
#include "../maps.hpp"
#include "../rules.hpp"
#include "render/boot.hpp"

struct SDL_Renderer;

namespace pokered::render {

struct ViewLayout;

bool draw_pokedex_presentation(
    SDL_Renderer* renderer, const ViewLayout& view,
    const WorldState& world, const RuleCatalog& rules,
    const BootRenderResources& boot,
    const ImportedAnimationAssets& assets);

} // namespace pokered::render
