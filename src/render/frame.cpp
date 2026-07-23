#include "render/frame.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

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

std::array<std::uint8_t, 3> palette_color(content::AnimationPalette palette, std::uint8_t red,
                                          std::uint8_t green, std::uint8_t blue) {
    if (palette == content::AnimationPalette::white) return {255, 255, 255};
    if (palette == content::AnimationPalette::inverted)
        return {static_cast<std::uint8_t>(255U - red), static_cast<std::uint8_t>(255U - green),
                static_cast<std::uint8_t>(255U - blue)};
    const float amount = palette == content::AnimationPalette::light      ? 0.55F
                         : palette == content::AnimationPalette::dark     ? 0.35F
                         : palette == content::AnimationPalette::darkened ? 0.55F
                                                                          : 1.0F;
    if (palette == content::AnimationPalette::light) {
        return {
            static_cast<std::uint8_t>(static_cast<float>(red) +
                                      (255.0F - static_cast<float>(red)) * amount),
            static_cast<std::uint8_t>(static_cast<float>(green) +
                                      (255.0F - static_cast<float>(green)) * amount),
            static_cast<std::uint8_t>(static_cast<float>(blue) +
                                      (255.0F - static_cast<float>(blue)) * amount),
        };
    }
    return {
        static_cast<std::uint8_t>(static_cast<float>(red) * amount),
        static_cast<std::uint8_t>(static_cast<float>(green) * amount),
        static_cast<std::uint8_t>(static_cast<float>(blue) * amount),
    };
}

void set_draw_color(SDL_Renderer* renderer, content::AnimationPalette palette, std::uint8_t red,
                    std::uint8_t green, std::uint8_t blue) {
    const auto color = palette_color(palette, red, green, blue);
    (void)SDL_SetRenderDrawColor(renderer, color[0], color[1], color[2], 255);
}

void draw_battler(SDL_Renderer* renderer, const ViewLayout& view, const AnimationTarget& target,
                  bool player, content::AnimationPalette screen_palette) {
    if (!target.visible) return;
    const float x = target.x + target.offset_x;
    const float y = target.y + target.offset_y;

    // Use deliberately original block creatures while the ROM asset importer is absent.
    const content::AnimationPalette palette =
        target.palette == content::AnimationPalette::normal ? screen_palette : target.palette;
    if (player)
        set_draw_color(renderer, palette, 68, 104, 156);
    else
        set_draw_color(renderer, palette, 176, 104, 72);
    fill_native_rect(renderer, view, x - 18.0F, y - 16.0F, 34.0F, 28.0F);
    fill_native_rect(renderer, view, x - 11.0F, y - 28.0F, 22.0F, 15.0F);
    fill_native_rect(renderer, view, player ? x - 25.0F : x + 14.0F, y - 10.0F, 12.0F,
                     10.0F);
    fill_native_rect(renderer, view, x - 14.0F, y + 9.0F, 9.0F, 11.0F);
    fill_native_rect(renderer, view, x + 5.0F, y + 9.0F, 9.0F, 11.0F);

    set_draw_color(renderer, palette, 246, 242, 224);
    fill_native_rect(renderer, view, x + 2.0F, y - 24.0F, 4.0F, 4.0F);
}

void set_imported_pixel_color(SDL_Renderer* renderer, std::uint8_t pixel,
                              std::uint8_t attributes,
                              content::AnimationPalette screen_palette) {
    const std::uint8_t palette = (attributes & 0x10U) == 0 ? 0xE4U : 0x6CU;
    const std::uint8_t shade = static_cast<std::uint8_t>((palette >> (pixel * 2U)) & 0x03U);
    constexpr std::array<std::array<std::uint8_t, 3>, 4> colors{{
        {246, 238, 230},
        {190, 172, 176},
        {116, 100, 124},
        {54, 47, 58},
    }};
    const auto& color = colors[shade];
    set_draw_color(renderer, screen_palette, color[0], color[1], color[2]);
}

bool draw_imported_effect(SDL_Renderer* renderer, const ViewLayout& view,
                          const AnimationEffect& effect,
                          const ImportedAnimationAssets& assets,
                          content::AnimationPalette screen_palette) {
    const ImportedAnimationVisual* visual =
        find_imported_animation_visual(assets, effect.visual);
    if (visual == nullptr) return false;
    // Lower OAM indexes win sprite overlap priority, so draw them last.
    for (auto piece_iterator = visual->pieces.rbegin();
         piece_iterator != visual->pieces.rend(); ++piece_iterator) {
        const ImportedAnimationPiece& piece = *piece_iterator;
        const std::vector<std::uint8_t>& tiles =
            piece.tile_set == 0 ? assets.tile_set_0 : assets.tile_set_1;
        const std::size_t tile_begin = static_cast<std::size_t>(piece.tile) * 16U;
        for (std::uint8_t output_y = 0; output_y < 8; ++output_y) {
            const std::uint8_t source_y =
                (piece.attributes & 0x40U) == 0 ? output_y
                                               : static_cast<std::uint8_t>(7U - output_y);
            const std::uint8_t low = tiles[tile_begin + static_cast<std::size_t>(source_y) * 2U];
            const std::uint8_t high =
                tiles[tile_begin + static_cast<std::size_t>(source_y) * 2U + 1U];
            for (std::uint8_t output_x = 0; output_x < 8; ++output_x) {
                const std::uint8_t source_x =
                    (piece.attributes & 0x20U) == 0 ? output_x
                                                   : static_cast<std::uint8_t>(7U - output_x);
                const std::uint8_t bit = static_cast<std::uint8_t>(7U - source_x);
                const std::uint8_t pixel = static_cast<std::uint8_t>(
                    ((high >> bit) & 1U) << 1U | ((low >> bit) & 1U));
                if (pixel == 0) continue;
                set_imported_pixel_color(renderer, pixel, piece.attributes, screen_palette);
                fill_native_rect(renderer, view,
                                 effect.x + static_cast<float>(piece.x) +
                                     static_cast<float>(output_x),
                                 effect.y + static_cast<float>(piece.y) +
                                     static_cast<float>(output_y),
                                 1.0F, 1.0F);
            }
        }
    }
    return true;
}

void draw_effect(SDL_Renderer* renderer, const ViewLayout& view, const AnimationEffect& effect,
                 const ImportedAnimationAssets& imported_assets,
                 content::AnimationPalette screen_palette) {
    if (!effect.visible) return;
    if (draw_imported_effect(renderer, view, effect, imported_assets, screen_palette)) return;
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
    const AnimationTarget* battle_screen =
        find_animation_target(lab.animation, Symbol{"battle_screen"});
    const content::AnimationPalette screen_palette =
        battle_screen == nullptr ? content::AnimationPalette::normal : battle_screen->palette;
    const SDL_FRect viewport{view.x, view.y, view.width, view.height};
    set_draw_color(renderer, screen_palette, 246, 238, 230);
    (void)SDL_RenderFillRect(renderer, &viewport);

    // Draw a fixed Pokémon battle composition; animation state supplies only overrides.
    set_draw_color(renderer, screen_palette, 190, 181, 167);
    fill_native_rect(renderer, view, 8.0F, 94.0F, 56.0F, 2.0F);
    fill_native_rect(renderer, view, 96.0F, 54.0F, 56.0F, 2.0F);
    const AnimationTarget* attacker = find_animation_target(lab.animation, Symbol{"attacker"});
    const AnimationTarget* defender = find_animation_target(lab.animation, Symbol{"defender"});
    if (attacker != nullptr) draw_battler(renderer, view, *attacker, true, screen_palette);
    if (defender != nullptr) draw_battler(renderer, view, *defender, false, screen_palette);
    for (const AnimationEffect& effect : lab.animation.effects)
        draw_effect(renderer, view, effect, lab.imported_assets, screen_palette);

    // Gen 1 reserves the lower six tile rows for battle text and action menus.
    set_draw_color(renderer, screen_palette, 54, 47, 58);
    fill_native_rect(renderer, view, 0.0F, 96.0F, 160.0F, 48.0F);
    set_draw_color(renderer, screen_palette, 250, 247, 238);
    fill_native_rect(renderer, view, 2.0F, 98.0F, 156.0F, 44.0F);
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
