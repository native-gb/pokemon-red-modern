#include "presentation/game_view.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace pokered::presentation {

ViewLayout layout_view(int output_width, int output_height) {
    constexpr float native_width = 160.0F;
    constexpr float native_height = 144.0F;
    constexpr float margin = 32.0F;
    const float available_width =
        std::max(static_cast<float>(output_width) - margin * 2.0F, native_width);
    const float available_height =
        std::max(static_cast<float>(output_height) - margin * 2.0F, native_height);
    const float fitted = std::min(available_width / native_width, available_height / native_height);
    const float scale = std::max(std::floor(fitted), 1.0F);
    const float width = native_width * scale;
    const float height = native_height * scale;
    return {
        .x = (static_cast<float>(output_width) - width) * 0.5F,
        .y = (static_cast<float>(output_height) - height) * 0.5F,
        .width = width,
        .height = height,
        .scale = scale,
    };
}

bool draw_game_view(SDL_Renderer* renderer, SDL_Texture* target, int output_width,
                    int output_height, const GameState& game,
                    const content::CatalogSummary& catalog) {
    if (renderer == nullptr || target == nullptr) return false;
    if (!SDL_SetRenderTarget(renderer, target)) return false;

    (void)SDL_SetRenderDrawColor(renderer, 15, 18, 25, 255);
    if (!SDL_RenderClear(renderer)) return false;

    const ViewLayout view = layout_view(output_width, output_height);
    const SDL_FRect shadow{view.x - 8.0F, view.y - 8.0F, view.width + 16.0F, view.height + 16.0F};
    (void)SDL_SetRenderDrawColor(renderer, 5, 6, 9, 255);
    (void)SDL_RenderFillRect(renderer, &shadow);

    const SDL_FRect viewport{view.x, view.y, view.width, view.height};
    const bool content_ready = catalog.state == content::PackState::ready;
    if (content_ready && game.mode != Mode::no_campaign)
        (void)SDL_SetRenderDrawColor(renderer, 216, 232, 192, 255);
    else
        (void)SDL_SetRenderDrawColor(renderer, 223, 229, 211, 255);
    (void)SDL_RenderFillRect(renderer, &viewport);

    const float unit = view.scale;
    const SDL_FRect title_bar{view.x + 12.0F * unit, view.y + 16.0F * unit, 136.0F * unit,
                              10.0F * unit};
    (void)SDL_SetRenderDrawColor(renderer, 39, 55, 49, 255);
    (void)SDL_RenderFillRect(renderer, &title_bar);

    const SDL_FRect status{view.x + 24.0F * unit, view.y + 62.0F * unit, 112.0F * unit,
                           42.0F * unit};
    (void)SDL_SetRenderDrawColor(renderer, 173, 188, 158, 255);
    (void)SDL_RenderFillRect(renderer, &status);

    const SDL_FRect inset{status.x + 2.0F * unit, status.y + 2.0F * unit, status.w - 4.0F * unit,
                          status.h - 4.0F * unit};
    (void)SDL_SetRenderDrawColor(renderer, 238, 242, 231, 255);
    (void)SDL_RenderFillRect(renderer, &inset);
    return true;
}

} // namespace pokered::presentation
