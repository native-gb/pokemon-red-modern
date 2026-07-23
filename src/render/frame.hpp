#pragma once

#include "catalog.hpp"
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
                  const GameState& game, const content::CatalogSummary& catalog);

} // namespace pokered::render
