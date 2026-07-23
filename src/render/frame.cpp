#include "render/frame.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace pokered::render {
namespace {

SDL_FRect native_rect(const ViewLayout& view, float x, float y, float width, float height) {
    return {
        .x = view.x + x * view.scale,
        .y = view.y + y * view.scale,
        .w = width * view.scale,
        .h = height * view.scale,
    };
}

void fill_native_rect(SDL_Renderer* renderer, const ViewLayout& view, float x, float y, float width,
                      float height) {
    const SDL_FRect rectangle = native_rect(view, x, y, width, height);
    (void)SDL_RenderFillRect(renderer, &rectangle);
}

void draw_battler(SDL_Renderer* renderer, const ViewLayout& view, const AnimationTarget& target,
                  bool player) {
    if (!target.visible) return;
    const float x = target.x;
    const float y = target.y;

    // Use deliberately original block creatures while the ROM asset importer is absent.
    if (player)
        (void)SDL_SetRenderDrawColor(renderer, 68, 104, 156, 255);
    else
        (void)SDL_SetRenderDrawColor(renderer, 176, 104, 72, 255);
    fill_native_rect(renderer, view, x - 12.0F, y - 12.0F, 22.0F, 18.0F);
    fill_native_rect(renderer, view, x - 7.0F, y - 20.0F, 14.0F, 10.0F);
    fill_native_rect(renderer, view, player ? x - 17.0F : x + 8.0F, y - 7.0F, 8.0F, 6.0F);
    fill_native_rect(renderer, view, x - 9.0F, y + 5.0F, 6.0F, 6.0F);
    fill_native_rect(renderer, view, x + 3.0F, y + 5.0F, 6.0F, 6.0F);

    (void)SDL_SetRenderDrawColor(renderer, 246, 242, 224, 255);
    fill_native_rect(renderer, view, x + 1.0F, y - 17.0F, 3.0F, 3.0F);
}

void draw_effect(SDL_Renderer* renderer, const ViewLayout& view, const AnimationEffect& effect) {
    if (!effect.visible) return;
    const float x = view.x + effect.x * view.scale;
    const float y = view.y + effect.y * view.scale;
    const float unit = view.scale;
    const std::string& visual = effect.visual.text;

    // Procedural placeholders make timing and coordinates visible before imported sprites exist.
    if (visual == "slash") {
        (void)SDL_SetRenderDrawColor(renderer, 62, 48, 70, 255);
        for (float offset = -1.0F; offset <= 1.0F; offset += 1.0F)
            (void)SDL_RenderLine(renderer, x - 6.0F * unit + offset, y - 7.0F * unit,
                                 x + 6.0F * unit + offset, y + 7.0F * unit);
        return;
    }
    if (visual == "lightning") {
        (void)SDL_SetRenderDrawColor(renderer, 244, 204, 52, 255);
        (void)SDL_RenderLine(renderer, x - 5.0F * unit, y - 10.0F * unit, x + 2.0F * unit, y);
        (void)SDL_RenderLine(renderer, x + 2.0F * unit, y, x - 2.0F * unit, y + 2.0F * unit);
        (void)SDL_RenderLine(renderer, x - 2.0F * unit, y + 2.0F * unit, x + 5.0F * unit,
                             y + 11.0F * unit);
        return;
    }
    if (visual == "healing_star") {
        (void)SDL_SetRenderDrawColor(renderer, 88, 196, 116, 255);
        fill_native_rect(renderer, view, effect.x - 1.0F, effect.y - 6.0F, 3.0F, 13.0F);
        fill_native_rect(renderer, view, effect.x - 6.0F, effect.y - 1.0F, 13.0F, 3.0F);
        return;
    }

    if (visual == "water_drop")
        (void)SDL_SetRenderDrawColor(renderer, 64, 148, 212, 255);
    else if (visual == "ember_burst")
        (void)SDL_SetRenderDrawColor(renderer, 244, 112, 48, 255);
    else
        (void)SDL_SetRenderDrawColor(renderer, 224, 76, 48, 255);
    const float size = visual == "ember_burst" ? 14.0F : 7.0F;
    fill_native_rect(renderer, view, effect.x - size * 0.5F, effect.y - size * 0.5F, size, size);
}

void draw_battle_lab(SDL_Renderer* renderer, const ViewLayout& view,
                     const BattleAnimationLab& lab) {
    const SDL_FRect viewport{view.x, view.y, view.width, view.height};
    (void)SDL_SetRenderDrawColor(renderer, 246, 238, 230, 255);
    (void)SDL_RenderFillRect(renderer, &viewport);

    // Draw a fixed Pokémon battle composition; animation state supplies only overrides.
    (void)SDL_SetRenderDrawColor(renderer, 190, 181, 167, 255);
    fill_native_rect(renderer, view, 18.0F, 101.0F, 58.0F, 4.0F);
    fill_native_rect(renderer, view, 88.0F, 54.0F, 54.0F, 4.0F);
    const AnimationTarget* attacker = find_animation_target(lab.animation, Symbol{"attacker"});
    const AnimationTarget* defender = find_animation_target(lab.animation, Symbol{"defender"});
    if (attacker != nullptr) draw_battler(renderer, view, *attacker, true);
    if (defender != nullptr) draw_battler(renderer, view, *defender, false);
    for (const AnimationEffect& effect : lab.animation.effects)
        draw_effect(renderer, view, effect);

    // Reserve the lower portion as the familiar battle message area.
    (void)SDL_SetRenderDrawColor(renderer, 54, 47, 58, 255);
    fill_native_rect(renderer, view, 4.0F, 112.0F, 152.0F, 28.0F);
    (void)SDL_SetRenderDrawColor(renderer, 250, 247, 238, 255);
    fill_native_rect(renderer, view, 6.0F, 114.0F, 148.0F, 24.0F);
}

} // namespace

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

bool render_frame(SDL_Renderer* renderer, SDL_Texture* target, int output_width, int output_height,
                  const GameState& game, const content::CatalogSummary& catalog,
                  const BattleAnimationLab& lab) {
    if (renderer == nullptr || target == nullptr) return false;
    if (!SDL_SetRenderTarget(renderer, target)) return false;

    (void)SDL_SetRenderDrawColor(renderer, 15, 18, 25, 255);
    if (!SDL_RenderClear(renderer)) return false;

    const ViewLayout view = layout_view(output_width, output_height);
    const SDL_FRect shadow{view.x - 8.0F, view.y - 8.0F, view.width + 16.0F, view.height + 16.0F};
    (void)SDL_SetRenderDrawColor(renderer, 5, 6, 9, 255);
    (void)SDL_RenderFillRect(renderer, &shadow);

    if (lab.loaded) {
        draw_battle_lab(renderer, view, lab);
        return true;
    }

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

} // namespace pokered::render
