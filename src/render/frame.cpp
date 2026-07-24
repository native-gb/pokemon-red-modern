#include "render/frame.hpp"
#include "render/dialogue.hpp"
#include "render/naming.hpp"

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

void draw_mon_pixel(SDL_Renderer* renderer, const ViewLayout& view,
                    content::AnimationPalette palette, std::uint8_t pixel, float x, float y) {
    constexpr std::array<std::array<std::uint8_t, 3>, 4> colors{{
        {246, 238, 230},
        {190, 172, 176},
        {116, 100, 124},
        {54, 47, 58},
    }};
    const auto& color = colors[pixel & 0x03U];
    set_draw_color(renderer, palette, color[0], color[1], color[2]);
    fill_native_rect(renderer, view, x, y, 1.0F, 1.0F);
}

void draw_substitute_mon(SDL_Renderer* renderer, const ViewLayout& view,
                         const AnimationTarget& target, bool player,
                         content::AnimationPalette palette, const ImportedAnimationAssets& assets) {
    if (assets.substitute_mon_tiles.size() < 8U * 16U) return;
    const std::size_t first_tile = player ? 4U : 0U;
    for (std::size_t tile = 0; tile < 4; ++tile) {
        const std::size_t tile_begin = (first_tile + tile) * 16U;
        const float tile_x =
            target.x + target.offset_x - 8.0F + static_cast<float>(tile % 2U) * 8.0F;
        const float tile_y =
            target.y + target.offset_y - 8.0F + static_cast<float>(tile / 2U) * 8.0F;
        for (std::uint8_t row = 0; row < 8; ++row) {
            const std::uint8_t low =
                assets.substitute_mon_tiles[tile_begin + static_cast<std::size_t>(row) * 2U];
            const std::uint8_t high =
                assets.substitute_mon_tiles[tile_begin + static_cast<std::size_t>(row) * 2U + 1U];
            for (std::uint8_t column = 0; column < 8; ++column) {
                const std::uint8_t bit = static_cast<std::uint8_t>(7U - column);
                const std::uint8_t pixel =
                    static_cast<std::uint8_t>(((high >> bit) & 1U) << 1U | ((low >> bit) & 1U));
                if (pixel == 0) continue;
                draw_mon_pixel(renderer, view, palette, pixel, tile_x + static_cast<float>(column),
                               tile_y + static_cast<float>(row));
            }
        }
    }
}

void draw_minimized_mon(SDL_Renderer* renderer, const ViewLayout& view,
                        const AnimationTarget& target, content::AnimationPalette palette,
                        const ImportedAnimationAssets& assets) {
    const float left = target.x + target.offset_x - 4.0F;
    const float top = target.y + target.offset_y + 8.0F;
    for (std::size_t row = 0; row < assets.minimized_mon_rows.size(); ++row) {
        const std::uint8_t pixels = assets.minimized_mon_rows[row];
        for (std::uint8_t column = 0; column < 8; ++column) {
            const std::uint8_t bit = static_cast<std::uint8_t>(7U - column);
            if (((pixels >> bit) & 1U) == 0) continue;
            draw_mon_pixel(renderer, view, palette, 3, left + static_cast<float>(column),
                           top + static_cast<float>(row));
        }
    }
}

void draw_imported_battler(SDL_Renderer* renderer, const ViewLayout& view,
                           const AnimationTarget& target, bool player,
                           content::AnimationPalette palette, const ImportedPokemonVisual& visual) {
    // Reproduce Red's two distinct picture layouts from renderer-owned anchors.
    const ImportedBattlePicture& picture = player ? visual.back : visual.front;
    const std::size_t source_width = static_cast<std::size_t>(picture.width_tiles) * 8U;
    const std::size_t source_height = static_cast<std::size_t>(picture.height_tiles) * 8U;
    if (picture.pixels.size() != source_width * source_height) return;

    const float center_x = target.x + target.offset_x;
    const float center_y = target.y + target.offset_y;
    const float width_scale =
        std::max(1.0F - static_cast<float>(target.squish_half_steps) / 8.0F, 0.125F);
    const std::size_t visible_width = player ? 28U : source_width;
    const std::size_t visible_height = player ? 28U : source_height;
    const float pixel_scale = player ? 2.0F : 1.0F;
    const float box_left = center_x - 28.0F;
    const float box_top = center_y - 28.0F;
    const float source_left =
        player ? box_left
               : box_left +
                     static_cast<float>((8U - static_cast<std::size_t>(picture.width_tiles)) / 2U) *
                         8.0F;
    const float source_top =
        player ? box_top
               : box_top +
                     static_cast<float>(7U - static_cast<std::size_t>(picture.height_tiles)) * 8.0F;

    for (std::size_t y = 0; y < visible_height; ++y) {
        for (std::size_t x = 0; x < visible_width; ++x) {
            const std::uint8_t pixel = picture.pixels[y * source_width + x];
            if (pixel == 0) continue;
            constexpr std::array<std::array<std::uint8_t, 3>, 4> colors{{
                {246, 238, 230},
                {190, 172, 176},
                {116, 100, 124},
                {54, 47, 58},
            }};
            const auto& color = colors[pixel & 0x03U];
            set_draw_color(renderer, palette, color[0], color[1], color[2]);
            const float unsquished_x = source_left + static_cast<float>(x) * pixel_scale;
            const float squished_x = center_x + (unsquished_x - center_x) * width_scale;
            fill_native_rect(renderer, view, squished_x,
                             source_top + static_cast<float>(y) * pixel_scale,
                             pixel_scale * width_scale, pixel_scale);
        }
    }
}

void draw_battler(SDL_Renderer* renderer, const ViewLayout& view, const AnimationTarget& target,
                  bool player, content::AnimationPalette screen_palette,
                  const ImportedAnimationAssets& assets, const ImportedPokemonVisual* pokemon) {
    if (!target.visible || target.form == content::AnimationForm::blank) return;
    const float x = target.x + target.offset_x;
    const float y = target.y + target.offset_y;

    // Use deliberately original block creatures while the ROM asset importer is absent.
    const content::AnimationPalette palette =
        target.palette == content::AnimationPalette::normal ? screen_palette : target.palette;
    const bool transformed = target.form == content::AnimationForm::transformed;
    if (player != transformed)
        set_draw_color(renderer, palette, 68, 104, 156);
    else
        set_draw_color(renderer, palette, 176, 104, 72);

    if (target.form == content::AnimationForm::minimized) {
        if (!assets.minimized_mon_rows.empty())
            draw_minimized_mon(renderer, view, target, palette, assets);
        else
            fill_native_rect(renderer, view, x - 3.0F, y + 10.0F, 6.0F, 5.0F);
        return;
    }
    if (target.form == content::AnimationForm::substitute) {
        if (!assets.substitute_mon_tiles.empty())
            draw_substitute_mon(renderer, view, target, player, palette, assets);
        else {
            fill_native_rect(renderer, view, x - 8.0F, y - 2.0F, 16.0F, 16.0F);
            fill_native_rect(renderer, view, x - 5.0F, y - 10.0F, 10.0F, 9.0F);
        }
        return;
    }
    if (pokemon != nullptr) {
        draw_imported_battler(renderer, view, target, player, palette, *pokemon);
        return;
    }

    const float width_scale = 1.0F - static_cast<float>(target.squish_half_steps) / 8.0F;
    const auto squished_rect = [&](float center_x, float top_y, float width, float height) {
        const float scaled_width = width * width_scale;
        fill_native_rect(renderer, view, center_x - scaled_width * 0.5F, top_y, scaled_width,
                         height);
    };
    squished_rect(x - 1.0F, y - 16.0F, 34.0F, 28.0F);
    squished_rect(x, y - 28.0F, 22.0F, 15.0F);
    squished_rect(player ? x - 19.0F : x + 20.0F, y - 10.0F, 12.0F, 10.0F);
    squished_rect(x - 9.5F, y + 9.0F, 9.0F, 11.0F);
    squished_rect(x + 9.5F, y + 9.0F, 9.0F, 11.0F);

    set_draw_color(renderer, palette, 246, 242, 224);
    squished_rect(x + 4.0F, y - 24.0F, 4.0F, 4.0F);
}

void set_imported_pixel_color(SDL_Renderer* renderer, std::uint8_t pixel, std::uint8_t attributes,
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
                          const AnimationEffect& effect, const ImportedAnimationAssets& assets,
                          content::AnimationPalette screen_palette,
                          bool enemy_turn) {
    const ImportedAnimationVisual* visual = find_imported_animation_visual(assets, effect.visual);
    if (visual == nullptr) return false;
    // Lower OAM indexes win sprite overlap priority, so draw them last.
    for (auto piece_iterator = visual->pieces.rbegin(); piece_iterator != visual->pieces.rend();
         ++piece_iterator) {
        const ImportedAnimationPiece& piece = *piece_iterator;
        const std::vector<std::uint8_t>& tiles =
            piece.tile_set == 0 ? assets.tile_set_0 : assets.tile_set_1;
        const std::size_t tile_begin = static_cast<std::size_t>(piece.tile) * 16U;
        for (std::uint8_t output_y = 0; output_y < 8; ++output_y) {
            const std::uint8_t source_y = (piece.attributes & 0x40U) == 0
                                              ? output_y
                                              : static_cast<std::uint8_t>(7U - output_y);
            const std::uint8_t low = tiles[tile_begin + static_cast<std::size_t>(source_y) * 2U];
            const std::uint8_t high =
                tiles[tile_begin + static_cast<std::size_t>(source_y) * 2U + 1U];
            for (std::uint8_t output_x = 0; output_x < 8; ++output_x) {
                const std::uint8_t source_x = (piece.attributes & 0x20U) == 0
                                                  ? output_x
                                                  : static_cast<std::uint8_t>(7U - output_x);
                const std::uint8_t bit = static_cast<std::uint8_t>(7U - source_x);
                const std::uint8_t pixel =
                    static_cast<std::uint8_t>(((high >> bit) & 1U) << 1U | ((low >> bit) & 1U));
                if (pixel == 0) continue;
                set_imported_pixel_color(renderer, pixel, piece.attributes, screen_palette);
                float x =
                    effect.x + static_cast<float>(piece.x) +
                    static_cast<float>(output_x);
                float y =
                    effect.y + static_cast<float>(piece.y) +
                    static_cast<float>(output_y);
                if (enemy_turn) {
                    x = 159.0F - x;
                    // Gen I OAM Y includes a 16-pixel hardware bias. Mirroring
                    // normalized canvas pixels therefore pivots around 111,
                    // not the visible 0..95 viewport used by our first pass.
                    y = 111.0F - y;
                }
                fill_native_rect(renderer, view, x, y, 1.0F, 1.0F);
            }
        }
    }
    return true;
}

void draw_effect(SDL_Renderer* renderer, const ViewLayout& view, const AnimationEffect& effect,
                 const ImportedAnimationAssets& imported_assets,
                 content::AnimationPalette screen_palette,
                 bool enemy_turn) {
    if (!effect.visible) return;
    if (draw_imported_effect(
            renderer, view, effect, imported_assets,
            screen_palette, enemy_turn))
        return;
    const float effect_x =
        enemy_turn ? 160.0F - effect.x : effect.x;
    const float effect_y =
        enemy_turn ? 112.0F - effect.y : effect.y;
    const float x = view.x + effect_x * view.scale;
    const float y = view.y + effect_y * view.scale;
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
        fill_native_rect(renderer, view, effect_x - 1.0F,
                         effect_y - 6.0F, 3.0F, 13.0F);
        fill_native_rect(renderer, view, effect_x - 6.0F,
                         effect_y - 1.0F, 13.0F, 3.0F);
        return;
    }

    if (visual == "water_drop")
        (void)SDL_SetRenderDrawColor(renderer, 64, 148, 212, 255);
    else if (visual == "ember_burst")
        (void)SDL_SetRenderDrawColor(renderer, 244, 112, 48, 255);
    else
        (void)SDL_SetRenderDrawColor(renderer, 224, 76, 48, 255);
    const float size = visual == "ember_burst" ? 14.0F : 7.0F;
    fill_native_rect(renderer, view, effect_x - size * 0.5F,
                     effect_y - size * 0.5F, size, size);
}

bool draw_battle_ui(SDL_Renderer* renderer, const ViewLayout& view, const BattleAnimationLab& lab,
                    content::AnimationPalette palette) {
    if (lab.imported_assets.battle_ui_tiles.size() != 256U * 64U) return false;
    for (std::size_t map_index = 0; map_index < lab.ui_tile_map.size(); ++map_index) {
        const std::uint8_t tile = lab.ui_tile_map[map_index];
        if (tile == 0) continue;
        const std::size_t tile_x = map_index % 20U;
        const std::size_t tile_y = map_index / 20U;
        const std::size_t pixel_begin = static_cast<std::size_t>(tile) * 64U;
        for (std::size_t y = 0; y < 8; ++y) {
            for (std::size_t x = 0; x < 8; ++x) {
                draw_mon_pixel(renderer, view, palette,
                               lab.imported_assets.battle_ui_tiles[pixel_begin + y * 8U + x],
                               static_cast<float>(tile_x * 8U + x),
                               static_cast<float>(tile_y * 8U + y));
            }
        }
    }
    return true;
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

    // Apply imported screen motion to the native composition, while the fixed
    // viewport clips the shifted result.
    ViewLayout scene_view = view;
    if (battle_screen != nullptr) {
        scene_view.x += battle_screen->offset_x * view.scale;
        scene_view.y += battle_screen->offset_y * view.scale;
        if (battle_screen->wave_phase >= 0 && !lab.imported_assets.wave_offsets.empty()) {
            const std::size_t phase = static_cast<std::size_t>(battle_screen->wave_phase) %
                                      lab.imported_assets.wave_offsets.size();
            scene_view.x -=
                static_cast<float>(lab.imported_assets.wave_offsets[phase]) * view.scale;
        }
    }
    const SDL_Rect clip{
        static_cast<int>(std::lround(view.x)),
        static_cast<int>(std::lround(view.y)),
        static_cast<int>(std::lround(view.width)),
        static_cast<int>(std::lround(view.height)),
    };
    (void)SDL_SetRenderClipRect(renderer, &clip);

    // Draw a fixed Pokémon battle composition; animation state supplies only overrides.
    const AnimationTarget* attacker_source =
        find_animation_target(lab.animation, Symbol{"attacker"});
    const AnimationTarget* defender_source =
        find_animation_target(lab.animation, Symbol{"defender"});
    AnimationTarget attacker_storage =
        attacker_source != nullptr ? *attacker_source : AnimationTarget{};
    AnimationTarget defender_storage =
        defender_source != nullptr ? *defender_source : AnimationTarget{};
    const BattlePresentationPhase phase =
        lab.presentation.phase;
    if (phase == BattlePresentationPhase::opening_wipe) {
        attacker_storage.visible = false;
        defender_storage.visible = false;
    } else if (phase ==
               BattlePresentationPhase::opponent_arrival) {
        attacker_storage.visible = false;
        const float progress = std::clamp(
            static_cast<float>(lab.presentation.tick) / 10.0F,
            0.0F, 1.0F);
        defender_storage.offset_x +=
            (1.0F - progress) * 48.0F;
    } else if (phase ==
               BattlePresentationPhase::player_deployment) {
        const float progress = std::clamp(
            static_cast<float>(lab.presentation.tick) / 14.0F,
            0.0F, 1.0F);
        attacker_storage.offset_x -=
            (1.0F - progress) * 52.0F;
    }
    const AnimationTarget* attacker =
        attacker_source != nullptr ? &attacker_storage : nullptr;
    const AnimationTarget* defender =
        defender_source != nullptr ? &defender_storage : nullptr;
    const ImportedPokemonVisual* player_pokemon =
        battle_view_player_species(lab);
    const ImportedPokemonVisual* enemy_pokemon =
        battle_view_enemy_species(lab);
    const bool show_player = lab.ui.mode == BattleUiMode::safari
                                 ? lab.ui.definition.safari_commands.show_player
                                 : lab.ui.definition.standard_commands.show_player;
    if (attacker != nullptr && show_player &&
        !lab.player_battler_hidden)
        draw_battler(renderer, scene_view, *attacker, true, screen_palette, lab.imported_assets,
                     player_pokemon);
    if (defender != nullptr && !lab.enemy_battler_hidden)
        draw_battler(renderer, scene_view, *defender, false, screen_palette, lab.imported_assets,
                     enemy_pokemon);

    // Draw the battle interface before transient effects so attacks can cross its region.
    const bool show_interface =
        phase == BattlePresentationPhase::inactive ||
        phase == BattlePresentationPhase::active ||
        phase == BattlePresentationPhase::closing_wipe;
    if (show_interface &&
        !draw_battle_ui(renderer, scene_view, lab, screen_palette)) {
        set_draw_color(renderer, screen_palette, 54, 47, 58);
        fill_native_rect(renderer, scene_view, 0.0F, 96.0F, 160.0F, 48.0F);
        set_draw_color(renderer, screen_palette, 250, 247, 238);
        fill_native_rect(renderer, scene_view, 2.0F, 98.0F, 156.0F, 44.0F);
    }
    for (const AnimationEffect& effect : lab.animation.effects)
        draw_effect(renderer, scene_view, effect, lab.imported_assets,
                    screen_palette, lab.gameplay_enemy_turn);

    if (phase == BattlePresentationPhase::player_deployment) {
        const float progress = std::clamp(
            static_cast<float>(lab.presentation.tick) / 14.0F,
            0.0F, 1.0F);
        const float ball_x = std::lerp(8.0F, 36.0F, progress);
        const float ball_y =
            std::lerp(82.0F, 66.0F, progress) -
            20.0F * 4.0F * progress * (1.0F - progress);
        set_draw_color(renderer, screen_palette, 54, 47, 58);
        fill_native_rect(renderer, scene_view, ball_x, ball_y,
                         4.0F, 4.0F);
        set_draw_color(renderer, screen_palette, 246, 238, 230);
        fill_native_rect(renderer, scene_view, ball_x + 1.0F,
                         ball_y + 1.0F, 2.0F, 1.0F);
    }
    if (phase == BattlePresentationPhase::opening_wipe ||
        phase == BattlePresentationPhase::closing_wipe) {
        const float progress = std::clamp(
            static_cast<float>(lab.presentation.tick) / 12.0F,
            0.0F, 1.0F);
        const float amount =
            phase == BattlePresentationPhase::opening_wipe
                ? 1.0F - progress
                : progress;
        set_draw_color(renderer, screen_palette, 54, 47, 58);
        if (lab.presentation.trainer_battle) {
            for (int band = 0; band < 10; ++band) {
                const float height = 72.0F * amount;
                fill_native_rect(
                    renderer, scene_view,
                    static_cast<float>(band * 16),
                    band % 2 == 0 ? 0.0F : 144.0F - height,
                    16.0F, height);
            }
        } else {
            for (int band = 0; band < 8; ++band) {
                const float width = 80.0F * amount;
                fill_native_rect(
                    renderer, scene_view,
                    band % 2 == 0 ? 0.0F : 160.0F - width,
                    static_cast<float>(band * 18), width, 18.0F);
            }
        }
    }
    (void)SDL_SetRenderClipRect(renderer, nullptr);
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
                  const BootContent& boot_content, const BootState& boot,
                  const BootRenderResources& boot_resources,
                  const BattleAnimationLab& lab, const WorldState& maps,
                  WorldRenderResources& world_resources) {
    if (renderer == nullptr || target == nullptr) return false;
    if (!SDL_SetRenderTarget(renderer, target)) return false;

    (void)SDL_SetRenderDrawColor(renderer, 15, 18, 25, 255);
    if (!SDL_RenderClear(renderer)) return false;

    const ViewLayout view = layout_view(output_width, output_height);

    if (game.mode == Mode::overworld && maps.loaded) {
        if (!draw_world(renderer, output_width, output_height, maps,
                        world_resources))
            return false;
        return draw_area_banner(renderer, maps, boot_resources) &&
               draw_naming_overlay(
                   renderer, view, maps, boot_resources) &&
               draw_dialogue_overlay(
                   renderer, output_width, output_height, maps,
                   boot_resources);
    }

    const SDL_FRect shadow{view.x - 8.0F, view.y - 8.0F, view.width + 16.0F, view.height + 16.0F};
    (void)SDL_SetRenderDrawColor(renderer, 5, 6, 9, 255);
    (void)SDL_RenderFillRect(renderer, &shadow);

    if (game.mode == Mode::title && boot_content.loaded && boot.active)
        return draw_boot(renderer, view, boot_content, boot, boot_resources);

    if ((game.mode == Mode::battle || game.mode == Mode::battle_lab) &&
        lab.loaded) {
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
