#pragma once

#include "battle_animation_lab.hpp"
#include "catalog.hpp"
#include "../maps.hpp"
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
                  const BattleAnimationLab& lab, const MapBrowser& maps,
                  const MapRenderResources& map_resources);

} // namespace pokered::render
