#pragma once

#include "content/catalog.hpp"
#include "runtime/state.hpp"

struct SDL_Renderer;
struct SDL_Texture;

namespace pokered::presentation {

struct ViewLayout {
    float x{};
    float y{};
    float width{};
    float height{};
    float scale{};
};

ViewLayout layout_view(int output_width, int output_height);
bool draw_game_view(SDL_Renderer* renderer, SDL_Texture* target, int output_width,
                    int output_height, const GameState& game,
                    const content::CatalogSummary& catalog);

} // namespace pokered::presentation
