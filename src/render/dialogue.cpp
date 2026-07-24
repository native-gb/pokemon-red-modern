#include "render/dialogue.hpp"

#include "render/boot.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace pokered::render {
namespace {

struct PixelGrid {
    int left{};
    int top{};
    int scale{1};
};

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

std::uint8_t next_tile(std::string_view text,
                       std::size_t& cursor) {
    const auto consume =
        [&](std::string_view token, std::uint8_t tile) {
            cursor += token.size();
            return tile;
        };
    const std::string_view remaining = text.substr(cursor);
    if (remaining.starts_with("é"))
        return consume("é", 0xBA);
    if (remaining.starts_with("♂"))
        return consume("♂", 0xEF);
    if (remaining.starts_with("♀"))
        return consume("♀", 0xF5);
    if (remaining.starts_with("×"))
        return consume("×", 0xF1);
    return ascii_tile(text[cursor++]);
}

bool draw_tile(SDL_Renderer* renderer,
               const BootRenderResources& resources,
               const PixelGrid& grid, std::uint8_t tile,
               int column, int row) {
    if (resources.ui_tiles == nullptr) return false;
    const SDL_FRect source{
        static_cast<float>(tile % 16U) * 8.0F,
        static_cast<float>(tile / 16U) * 8.0F,
        8.0F, 8.0F,
    };
    const float tile_size =
        static_cast<float>(grid.scale * 8);
    const SDL_FRect destination{
        static_cast<float>(
            grid.left + column * grid.scale * 8),
        static_cast<float>(
            grid.top + row * grid.scale * 8),
        tile_size, tile_size,
    };
    return SDL_RenderTexture(
        renderer, resources.ui_tiles, &source, &destination);
}

bool draw_box(SDL_Renderer* renderer,
              const BootRenderResources& resources,
              const PixelGrid& grid, int columns, int rows) {
    const SDL_FRect background{
        static_cast<float>(grid.left),
        static_cast<float>(grid.top),
        static_cast<float>(columns * grid.scale * 8),
        static_cast<float>(rows * grid.scale * 8),
    };
    (void)SDL_SetRenderDrawColor(renderer, 246, 238, 242, 255);
    if (!SDL_RenderFillRect(renderer, &background)) return false;
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            std::uint8_t tile = 0x7FU;
            if (row == 0)
                tile = column == 0
                           ? 0x79U
                           : (column + 1 == columns
                                  ? 0x7BU
                                  : 0x7AU);
            else if (row + 1 == rows)
                tile = column == 0
                           ? 0x7DU
                           : (column + 1 == columns
                                  ? 0x7EU
                                  : 0x7AU);
            else if (column == 0 || column + 1 == columns)
                tile = 0x7CU;
            if (!draw_tile(
                    renderer, resources, grid, tile,
                    column, row))
                return false;
        }
    }
    return true;
}

bool draw_text(SDL_Renderer* renderer,
               const BootRenderResources& resources,
               const PixelGrid& grid, std::string_view text,
               int left, int top, int columns, int rows) {
    int column = 0;
    int row = 0;
    for (std::size_t cursor = 0U;
         cursor < text.size() && row < rows;) {
        if (text[cursor] == '\n') {
            ++cursor;
            column = 0;
            ++row;
            continue;
        }
        if (!draw_tile(
                renderer, resources, grid,
                next_tile(text, cursor), left + column,
                top + row))
            return false;
        if (++column >= columns) {
            column = 0;
            ++row;
        }
    }
    return true;
}

std::size_t longest_line(std::string_view text) {
    std::size_t longest = 0U;
    std::size_t current = 0U;
    for (char value : text) {
        if (value == '\n') {
            longest = std::max(longest, current);
            current = 0U;
        } else {
            ++current;
        }
    }
    return std::max(longest, current);
}

std::size_t line_count(std::string_view text) {
    return 1U + static_cast<std::size_t>(
                    std::ranges::count(text, '\n'));
}

} // namespace

bool draw_dialogue_overlay(
    SDL_Renderer* renderer, int output_width, int output_height,
    const WorldState& world,
    const BootRenderResources& resources) {
    if (!world.dialogue.open ||
        world.dialogue.page >= world.dialogue.pages.size())
        return true;

    const std::string_view page =
        world.dialogue.pages[world.dialogue.page];
    const int desired_columns = std::clamp(
        static_cast<int>(longest_line(page)) + 2, 20, 28);
    const int rows = std::clamp(
        static_cast<int>(line_count(page)) + 2, 4, 7);
    const int width_scale =
        std::max(1, (output_width - 32) /
                        (desired_columns * 8));
    const int height_scale =
        std::max(1, (output_height - 32) / (rows * 8));
    const int scale =
        std::clamp(std::min(width_scale, height_scale), 1, 3);
    const int available_columns =
        std::max(20, (output_width - 32) / (scale * 8));
    const int columns =
        std::min(desired_columns, available_columns);
    const int width = columns * scale * 8;
    const int height = rows * scale * 8;
    const PixelGrid dialogue{
        .left = std::min(
            24, std::max(8, output_width - width - 8)),
        .top = output_height - height - 24,
        .scale = scale,
    };
    if (!draw_box(
            renderer, resources, dialogue, columns, rows) ||
        !draw_text(
            renderer, resources, dialogue,
            page,
            1, 1, columns - 2, rows - 2) ||
        !draw_tile(
            renderer, resources, dialogue, 0xEEU,
            columns - 2, rows - 2))
        return false;

    if (!world.choice.open || world.choice.options.empty())
        return true;
    std::size_t longest = 0U;
    for (const std::string& option : world.choice.options)
        longest = std::max(longest, option.size());
    const int choice_columns = std::max(
        8, static_cast<int>(longest) + 4);
    const int choice_rows =
        static_cast<int>(world.choice.options.size()) + 2;
    const int choice_width = choice_columns * scale * 8;
    const int choice_height = choice_rows * scale * 8;
    const PixelGrid choice{
        .left = dialogue.left + width - choice_width,
        .top = dialogue.top - choice_height - scale * 4,
        .scale = scale,
    };
    if (!draw_box(
            renderer, resources, choice, choice_columns,
            choice_rows))
        return false;
    for (std::size_t index = 0U;
         index < world.choice.options.size(); ++index) {
        if (index == world.choice.selected &&
            !draw_tile(
                renderer, resources, choice, 0xEDU, 1,
                1 + static_cast<int>(index)))
            return false;
        if (!draw_text(
                renderer, resources, choice,
                world.choice.options[index], 2,
                1 + static_cast<int>(index),
                choice_columns - 3, 1))
            return false;
    }
    return true;
}

} // namespace pokered::render
