#pragma once

#include "../maps.hpp"
#include "boot.hpp"
#include "battle_animation_lab.hpp"
#include "catalog.hpp"
#include "render/boot.hpp"
#include "render/maps.hpp"
#include "state.hpp"

struct SDL_Renderer;
struct SDL_Texture;

namespace pokered::render {

struct ViewLayout {
    float x{};
    float y{};
    float width{};
    float height{};
    float scale{};
};

ViewLayout layout_view(int output_width, int output_height);
bool render_frame(SDL_Renderer* renderer, SDL_Texture* target, int output_width, int output_height,
                  const GameState& game, const content::CatalogSummary& catalog,
                  const BootContent& boot_content, const BootState& boot,
                  const BootRenderResources& boot_resources,
                  const BattleAnimationLab& lab, const WorldState& maps,
                  WorldRenderResources& world_resources);

} // namespace pokered::render
