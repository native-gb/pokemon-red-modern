#include "render/pokedex.hpp"

#include "render/frame.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace pokered::render {
namespace {

constexpr std::array<std::uint8_t, 20> kDivider{
    0x68U, 0x69U, 0x6BU, 0x69U, 0x6BU,
    0x69U, 0x6BU, 0x69U, 0x6BU, 0x6BU,
    0x6BU, 0x6BU, 0x69U, 0x6BU, 0x69U,
    0x6BU, 0x69U, 0x6BU, 0x69U, 0x6AU,
};

constexpr std::array<std::array<std::uint8_t, 3>, 4>
    kColors{{
        {250U, 238U, 246U},
        {229U, 178U, 166U},
        {138U, 116U, 154U},
        {39U, 25U, 39U},
    }};

std::uint8_t ascii_tile(char value) {
    if (value >= 'A' && value <= 'Z')
        return static_cast<std::uint8_t>(
            0x80 + value - 'A');
    if (value >= 'a' && value <= 'z')
        return static_cast<std::uint8_t>(
            0xA0 + value - 'a');
    if (value >= '0' && value <= '9')
        return static_cast<std::uint8_t>(
            0xF6 + value - '0');
    switch (value) {
    case ' ': return 0x7FU;
    case '(': return 0x9AU;
    case ')': return 0x9BU;
    case ':': return 0x9CU;
    case ';': return 0x9DU;
    case '[': return 0x9EU;
    case ']': return 0x9FU;
    case '\'': return 0xE0U;
    case '-': return 0xE3U;
    case '?': return 0xE6U;
    case '!': return 0xE7U;
    case '.': return 0xE8U;
    case '/': return 0xF3U;
    case ',': return 0xF4U;
    default: return 0xE6U;
    }
}

std::uint8_t next_tile(
    std::string_view text, std::size_t& cursor) {
    const std::string_view remaining =
        text.substr(cursor);
    if (remaining.starts_with("é")) {
        cursor += std::string_view{"é"}.size();
        return 0xBAU;
    }
    if (remaining.starts_with("♂")) {
        cursor += std::string_view{"♂"}.size();
        return 0xEFU;
    }
    if (remaining.starts_with("♀")) {
        cursor += std::string_view{"♀"}.size();
        return 0xF5U;
    }
    return ascii_tile(text[cursor++]);
}

bool draw_tile(
    SDL_Renderer* renderer, const ViewLayout& view,
    const BootRenderResources& resources,
    std::uint8_t tile, int x, int y) {
    SDL_Texture* texture = resources.ui_tiles;
    SDL_FRect source{
        static_cast<float>(tile % 16U) * 8.0F,
        static_cast<float>(tile / 16U) * 8.0F,
        8.0F, 8.0F,
    };
    if (tile >= 0x60U && tile <= 0x71U) {
        texture = resources.pokedex_tiles;
        source = {
            static_cast<float>(tile - 0x60U) * 8.0F,
            0.0F, 8.0F, 8.0F,
        };
    }
    if (texture == nullptr) return false;
    const SDL_FRect destination{
        view.x + static_cast<float>(x * 8) * view.scale,
        view.y + static_cast<float>(y * 8) * view.scale,
        8.0F * view.scale, 8.0F * view.scale,
    };
    return SDL_RenderTexture(
        renderer, texture, &source, &destination);
}

bool draw_text(
    SDL_Renderer* renderer, const ViewLayout& view,
    const BootRenderResources& resources,
    int x, int y, std::string_view text) {
    int column = 0;
    for (std::size_t cursor = 0U;
         cursor < text.size();) {
        if (!draw_tile(
                renderer, view, resources,
                next_tile(text, cursor),
                x + column, y))
            return false;
        ++column;
    }
    return true;
}

std::string decimal(
    std::uint16_t value, int width,
    bool leading_zero = false) {
    std::ostringstream output;
    output << std::setw(width)
           << std::setfill(leading_zero ? '0' : ' ')
           << value;
    return output.str();
}

bool draw_border(
    SDL_Renderer* renderer, const ViewLayout& view,
    const BootRenderResources& resources) {
    for (int x = 0; x < 20; ++x) {
        if (!draw_tile(
                renderer, view, resources, 0x64U,
                x, 0) ||
            !draw_tile(
                renderer, view, resources, 0x6FU,
                x, 17))
            return false;
    }
    for (int y = 1; y < 17; ++y) {
        if (!draw_tile(
                renderer, view, resources, 0x66U,
                0, y) ||
            !draw_tile(
                renderer, view, resources, 0x67U,
                19, y))
            return false;
    }
    constexpr std::array corners{
        std::array{0, 0, 0x63},
        std::array{19, 0, 0x65},
        std::array{0, 17, 0x6C},
        std::array{19, 17, 0x6E},
    };
    for (const auto& corner : corners)
        if (!draw_tile(
                renderer, view, resources,
                static_cast<std::uint8_t>(corner[2]),
                corner[0], corner[1]))
            return false;
    for (std::size_t x = 0U; x < kDivider.size(); ++x)
        if (!draw_tile(
                renderer, view, resources, kDivider[x],
                static_cast<int>(x), 9))
            return false;
    return true;
}

void draw_front_picture(
    SDL_Renderer* renderer, const ViewLayout& view,
    const ImportedPokemonVisual& visual) {
    const ImportedBattlePicture& picture = visual.front;
    const std::size_t width =
        static_cast<std::size_t>(picture.width_tiles) * 8U;
    const std::size_t height =
        static_cast<std::size_t>(picture.height_tiles) * 8U;
    if (picture.pixels.size() != width * height ||
        width > 56U || height > 56U)
        return;
    const float left =
        8.0F + static_cast<float>(56U - width) * 0.5F;
    const float top =
        8.0F + static_cast<float>(56U - height);
    for (std::size_t y = 0U; y < height; ++y) {
        for (std::size_t x = 0U; x < width; ++x) {
            const std::uint8_t shade =
                picture.pixels[y * width + x] & 0x03U;
            if (shade == 0U) continue;
            const auto& color = kColors[shade];
            (void)SDL_SetRenderDrawColor(
                renderer, color[0], color[1], color[2],
                255U);
            const SDL_FRect pixel{
                view.x +
                    (left + static_cast<float>(x)) *
                        view.scale,
                view.y +
                    (top + static_cast<float>(y)) *
                        view.scale,
                view.scale, view.scale,
            };
            (void)SDL_RenderFillRect(renderer, &pixel);
        }
    }
}

} // namespace

bool draw_pokedex_presentation(
    SDL_Renderer* renderer, const ViewLayout& view,
    const WorldState& world, const RuleCatalog& rules,
    const BootRenderResources& boot,
    const ImportedAnimationAssets& assets) {
    if (!world.pokemon_presentation.active)
        return true;
    const std::uint16_t dex =
        world.pokemon_presentation.species_dex;
    if (dex == 0U || dex > assets.pokemon.size() ||
        dex > 0xFFU)
        return false;
    const SpeciesRule* species =
        find_species(
            rules, static_cast<std::uint8_t>(dex));
    const PokedexEntryRule* entry =
        find_pokedex_entry(
            rules, static_cast<std::uint8_t>(dex));
    if (species == nullptr || entry == nullptr)
        return false;

    const SDL_FRect screen{
        view.x, view.y, view.width, view.height,
    };
    (void)SDL_SetRenderDrawColor(
        renderer, kColors[0][0], kColors[0][1],
        kColors[0][2], 255U);
    if (!SDL_RenderFillRect(renderer, &screen) ||
        !draw_border(renderer, view, boot))
        return false;

    draw_front_picture(
        renderer, view, assets.pokemon[dex - 1U]);
    if (!draw_text(
            renderer, view, boot, 9, 2,
            species->name) ||
        !draw_text(
            renderer, view, boot, 9, 4,
            entry->classification) ||
        !draw_tile(
            renderer, view, boot, 0x74U, 2, 8) ||
        !draw_tile(
            renderer, view, boot, 0xF2U, 3, 8) ||
        !draw_text(
            renderer, view, boot, 4, 8,
            decimal(entry->dex_number, 3, true)) ||
        !draw_text(
            renderer, view, boot, 9, 6, "HT") ||
        !draw_text(
            renderer, view, boot, 12, 6,
            decimal(entry->height_feet, 2)) ||
        !draw_tile(
            renderer, view, boot, 0x60U, 14, 6) ||
        !draw_text(
            renderer, view, boot, 15, 6,
            decimal(entry->height_inches, 2, true)) ||
        !draw_tile(
            renderer, view, boot, 0x61U, 17, 6) ||
        !draw_text(
            renderer, view, boot, 9, 8, "WT"))
        return false;

    const std::uint16_t pounds =
        entry->weight_tenths_pounds / 10U;
    const std::uint16_t tenths =
        entry->weight_tenths_pounds % 10U;
    if (!draw_text(
            renderer, view, boot, 11, 8,
            decimal(pounds, 4) + "." +
                decimal(tenths, 1, true)) ||
        !draw_text(
            renderer, view, boot, 17, 8, "lb"))
        return false;

    const std::size_t first =
        std::min<std::size_t>(
            world.pokemon_presentation.page, 1U) *
        3U;
    for (std::size_t row = 0U; row < 3U; ++row)
        if (!draw_text(
                renderer, view, boot, 1,
                11 + static_cast<int>(row) * 2,
                entry->description_lines[first + row]))
            return false;
    return draw_tile(
        renderer, view, boot, 0xEEU, 18, 16);
}

} // namespace pokered::render
