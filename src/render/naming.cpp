#include "render/naming.hpp"

#include "render/boot.hpp"
#include "render/frame.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace pokered::render {
namespace {

SDL_FRect native_rect(const ViewLayout& view, float x, float y,
                      float width, float height) {
    return {
        view.x + x * view.scale,
        view.y + y * view.scale,
        width * view.scale,
        height * view.scale,
    };
}

std::uint8_t ascii_tile(char value) {
    if (value >= 'A' && value <= 'Z')
        return static_cast<std::uint8_t>(0x80 + value - 'A');
    if (value >= 'a' && value <= 'z')
        return static_cast<std::uint8_t>(0xA0 + value - 'a');
    if (value >= '0' && value <= '9')
        return static_cast<std::uint8_t>(0xF6 + value - '0');
    switch (value) {
    case ' ': return 0x7F;
    case '(': return 0x9A;
    case ')': return 0x9B;
    case ':': return 0x9C;
    case ';': return 0x9D;
    case '[': return 0x9E;
    case ']': return 0x9F;
    case '\'': return 0xE0;
    case '-': return 0xE3;
    case '?': return 0xE6;
    case '!': return 0xE7;
    case '.': return 0xE8;
    case '/': return 0xF3;
    case ',': return 0xF4;
    default: return 0xE6;
    }
}

std::uint8_t next_tile(std::string_view text, std::size_t& cursor) {
    const auto consume = [&](std::string_view token, std::uint8_t tile) {
        cursor += token.size();
        return tile;
    };
    const std::string_view remaining = text.substr(cursor);
    if (remaining.starts_with("é")) return consume("é", 0xBA);
    if (remaining.starts_with("♂")) return consume("♂", 0xEF);
    if (remaining.starts_with("♀")) return consume("♀", 0xF5);
    if (remaining.starts_with("×")) return consume("×", 0xF1);
    const char value = text[cursor++];
    return ascii_tile(value);
}

bool draw_tile(SDL_Renderer* renderer, const ViewLayout& view,
               const BootRenderResources& resources, std::uint8_t tile,
               float x, float y) {
    if (resources.ui_tiles == nullptr) return false;
    const SDL_FRect source{
        static_cast<float>(tile % 16U) * 8.0F,
        static_cast<float>(tile / 16U) * 8.0F,
        8.0F, 8.0F,
    };
    const SDL_FRect destination = native_rect(view, x, y, 8.0F, 8.0F);
    return SDL_RenderTexture(renderer, resources.ui_tiles, &source,
                             &destination);
}

bool draw_text(SDL_Renderer* renderer, const ViewLayout& view,
               const BootRenderResources& resources, std::string_view text,
               std::size_t left, std::size_t top, std::size_t columns,
               std::size_t rows = 1U) {
    std::size_t x = 0U;
    std::size_t y = 0U;
    for (std::size_t cursor = 0U; cursor < text.size() && y < rows;) {
        if (text[cursor] == '\n') {
            ++cursor;
            x = 0U;
            ++y;
            continue;
        }
        if (!draw_tile(renderer, view, resources, next_tile(text, cursor),
                       static_cast<float>((left + x) * 8U),
                       static_cast<float>((top + y) * 8U)))
            return false;
        if (++x >= columns) {
            x = 0U;
            ++y;
        }
    }
    return true;
}

bool draw_box(SDL_Renderer* renderer, const ViewLayout& view,
              const BootRenderResources& resources, std::size_t left,
              std::size_t top, std::size_t width, std::size_t height) {
    for (std::size_t y = 0U; y < height; ++y) {
        for (std::size_t x = 0U; x < width; ++x) {
            std::uint8_t tile = 0x7F;
            if (y == 0U)
                tile = x == 0U ? 0x79 : (x + 1U == width ? 0x7B : 0x7A);
            else if (y + 1U == height)
                tile = x == 0U ? 0x7D : (x + 1U == width ? 0x7E : 0x7A);
            else if (x == 0U || x + 1U == width)
                tile = 0x7C;
            if (!draw_tile(renderer, view, resources, tile,
                           static_cast<float>((left + x) * 8U),
                           static_cast<float>((top + y) * 8U)))
                return false;
        }
    }
    return true;
}

} // namespace

bool draw_naming_overlay(SDL_Renderer* renderer, const ViewLayout& view,
                         const WorldState& world,
                         const BootRenderResources& resources) {
    const NamingState& naming = world.naming;
    if (!naming.open || !valid_naming_profile(naming.profile))
        return true;

    const SDL_FRect background{view.x, view.y, view.width, view.height};
    (void)SDL_SetRenderDrawColor(renderer, 246, 238, 242, 255);
    if (!SDL_RenderFillRect(renderer, &background)) return false;

    // Everything lands on the native 8-pixel grid before the integer view
    // scale is applied. This keeps letters, name slots, and cursor crisp.
    if (!draw_text(renderer, view, resources, naming.heading, 0U, 1U,
                   10U, 2U) ||
        !draw_text(renderer, view, resources, naming.value, 10U, 2U,
                   naming.profile.maximum_length) ||
        !draw_box(renderer, view, resources, 0U, 4U, 20U, 11U))
        return false;

    for (std::size_t index = 0U;
         index < naming.profile.maximum_length; ++index) {
        if (!draw_text(renderer, view, resources, "-",
                       10U + index, 3U, 1U))
            return false;
    }
    for (std::uint8_t row = 0U; row < kNamingRows; ++row) {
        const std::uint8_t columns =
            row + 1U == kNamingRows
                ? static_cast<std::uint8_t>(kNamingColumns - 1U)
                : static_cast<std::uint8_t>(kNamingColumns);
        for (std::uint8_t column = 0U; column < columns; ++column) {
            if (!draw_text(renderer, view, resources,
                           naming_cell(naming, row, column),
                           2U + static_cast<std::size_t>(column) * 2U,
                           5U + static_cast<std::size_t>(row) * 2U, 2U))
                return false;
        }
    }

    const std::string_view case_action =
        naming.lowercase ? naming.profile.uppercase_action
                         : naming.profile.lowercase_action;
    if (!draw_text(renderer, view, resources, case_action,
                   2U, 16U, 12U) ||
        !draw_text(renderer, view, resources, "END", 16U, 16U, 3U))
        return false;

    const std::size_t cursor_x =
        naming.row == kNamingRows
            ? (naming.column == 0U ? 1U : 15U)
            : 1U + static_cast<std::size_t>(naming.column) * 2U;
    const std::size_t cursor_y =
        naming.row == kNamingRows
            ? 16U
            : 5U + static_cast<std::size_t>(naming.row) * 2U;
    return draw_tile(renderer, view, resources, 0xED,
                     static_cast<float>(cursor_x * 8U),
                     static_cast<float>(cursor_y * 8U));
}

bool draw_area_banner(SDL_Renderer* renderer, const WorldState& world,
                      const BootRenderResources& resources) {
    if (!world.area_banner.active ||
        world.area_banner.text.empty())
        return true;
    constexpr float slide_seconds = 0.20F;
    constexpr float hold_seconds = 2.0F;
    constexpr float total_seconds =
        slide_seconds * 2.0F + hold_seconds;
    float amount = 1.0F;
    if (world.area_banner.elapsed < slide_seconds)
        amount = world.area_banner.elapsed / slide_seconds;
    else if (world.area_banner.elapsed >
             slide_seconds + hold_seconds)
        amount = (total_seconds - world.area_banner.elapsed) /
                 slide_seconds;
    amount = std::clamp(amount, 0.0F, 1.0F);
    const std::size_t columns = std::clamp<std::size_t>(
        world.area_banner.text.size() + 2U, 6U, 28U);
    const ViewLayout banner{
        .x = 16.0F,
        .y = 42.0F - (1.0F - amount) * 32.0F,
        .width = static_cast<float>(columns * 16U),
        .height = 48.0F,
        .scale = 2.0F,
    };
    return draw_box(renderer, banner, resources, 0U, 0U,
                    columns, 3U) &&
           draw_text(renderer, banner, resources,
                     world.area_banner.text, 1U, 1U,
                     columns - 2U);
}

} // namespace pokered::render
