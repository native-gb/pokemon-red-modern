#include "render/field_menu.hpp"

#include "campaign_programs.hpp"
#include "maps.hpp"
#include "render/boot.hpp"
#include "rules.hpp"
#include "state.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
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
               const PixelGrid& grid, std::string_view value,
               int column, int row, int columns) {
    int x = 0;
    for (std::size_t cursor = 0U;
         cursor < value.size() && x < columns;) {
        if (!draw_tile(
                renderer, resources, grid,
                next_tile(value, cursor),
                column + x, row))
            return false;
        ++x;
    }
    return true;
}

std::string_view status_name(MajorStatus status) {
    switch (status) {
    case MajorStatus::none:
        return "OK";
    case MajorStatus::sleep:
        return "SLP";
    case MajorStatus::poison:
        return "PSN";
    case MajorStatus::burn:
        return "BRN";
    case MajorStatus::freeze:
        return "FRZ";
    case MajorStatus::paralysis:
        return "PAR";
    }
    return "?";
}

std::string item_name(
    const CampaignProgramCatalog& programs,
    std::uint16_t item_id) {
    const auto found = std::ranges::find_if(
        programs.item_names,
        [item_id](const CampaignItemName& item) {
            return item.item_id == item_id;
        });
    return found == programs.item_names.end()
               ? "ITEM " + std::to_string(item_id)
               : found->name;
}

PixelGrid centered_grid(
    int output_width, int output_height,
    int columns, int rows, int scale) {
    return {
        .left =
            (output_width - columns * scale * 8) / 2,
        .top =
            (output_height - rows * scale * 8) / 2,
        .scale = scale,
    };
}

bool draw_root(
    SDL_Renderer* renderer,
    const BootRenderResources& resources,
    const FieldMenuState& menu, int output_width,
    int output_height, int scale) {
    constexpr int columns = 14;
    constexpr int rows = 8;
    const PixelGrid grid{
        .left = output_width - columns * scale * 8 - 24,
        .top = output_height - rows * scale * 8 - 24,
        .scale = scale,
    };
    if (!draw_box(
            renderer, resources, grid, columns, rows))
        return false;
    constexpr std::array<std::string_view, 6> entries{
        "PARTY", "BAG", "TRAINER", "SAVE", "QUIT", "CLOSE"};
    for (std::size_t index = 0U; index < entries.size(); ++index) {
        const int row = 1 + static_cast<int>(index);
        if (index == menu.selected &&
            !draw_tile(
                renderer, resources, grid, 0xEDU, 1, row))
            return false;
        if (!draw_text(
                renderer, resources, grid, entries[index],
                2, row, columns - 3))
            return false;
    }
    return true;
}

bool draw_quit_confirmation(
    SDL_Renderer* renderer,
    const BootRenderResources& resources,
    const FieldMenuState& menu, int output_width,
    int output_height, int scale) {
    constexpr int columns = 18;
    constexpr int rows = 5;
    const PixelGrid grid = centered_grid(
        output_width, output_height, columns, rows, scale);
    if (!draw_box(
            renderer, resources, grid, columns, rows) ||
        !draw_text(
            renderer, resources, grid,
            "QUIT TO TITLE?", 1, 1, columns - 2))
        return false;
    constexpr std::array<std::string_view, 2> entries{"YES", "NO"};
    for (std::size_t index = 0U; index < entries.size(); ++index) {
        const int row = 2 + static_cast<int>(index);
        if (index == menu.selected &&
            !draw_tile(
                renderer, resources, grid, 0xEDU, 2, row))
            return false;
        if (!draw_text(
                renderer, resources, grid, entries[index],
                3, row, columns - 4))
            return false;
    }
    return true;
}

bool draw_party(
    SDL_Renderer* renderer,
    const BootRenderResources& resources,
    const CampaignState& campaign,
    const RuleCatalog& rules, const PixelGrid& grid,
    int columns, int rows) {
    if (!draw_box(
            renderer, resources, grid, columns, rows) ||
        !draw_text(
            renderer, resources, grid, "PARTY",
            2, 1, columns - 4))
        return false;
    if (campaign.party.members.empty())
        return draw_text(
            renderer, resources, grid, "No POKéMON.",
            2, 3, columns - 4);
    for (std::size_t index = 0U;
         index < campaign.party.members.size(); ++index) {
        const PokemonState& pokemon =
            campaign.party.members[index];
        const SpeciesRule* species =
            find_species(rules, pokemon.species_dex);
        const std::string name =
            pokemon.nickname.empty() && species != nullptr
                ? species->name
                : pokemon.nickname;
        const int row = 3 + static_cast<int>(index) * 2;
        if (row + 1 >= rows) break;
        char details[96]{};
        std::snprintf(
            details, sizeof(details), "L%u HP %u/%u %s",
            static_cast<unsigned>(pokemon.level),
            static_cast<unsigned>(pokemon.current_hp),
            static_cast<unsigned>(pokemon.stats.hp),
            status_name(pokemon.status).data());
        if (!draw_text(
                renderer, resources, grid, name,
                2, row, columns - 4) ||
            !draw_text(
                renderer, resources, grid, details,
                4, row + 1, columns - 6))
            return false;
    }
    return true;
}

bool draw_bag(
    SDL_Renderer* renderer,
    const BootRenderResources& resources,
    const CampaignState& campaign,
    const CampaignProgramCatalog& programs,
    const PixelGrid& grid, int columns, int rows) {
    if (!draw_box(
            renderer, resources, grid, columns, rows) ||
        !draw_text(
            renderer, resources, grid, "BAG",
            2, 1, columns - 4))
        return false;
    if (campaign.inventory.stacks.empty())
        return draw_text(
            renderer, resources, grid,
            "The BAG is empty.", 2, 3, columns - 4);
    const std::size_t visible = std::min<std::size_t>(
        campaign.inventory.stacks.size(),
        static_cast<std::size_t>(rows - 4));
    for (std::size_t index = 0U; index < visible; ++index) {
        const InventoryStack& stack =
            campaign.inventory.stacks[index];
        const int row = 3 + static_cast<int>(index);
        const std::string quantity =
            "x" + std::to_string(stack.quantity);
        if (!draw_text(
                renderer, resources, grid,
                item_name(programs, stack.item_id),
                2, row, columns - 10) ||
            !draw_text(
                renderer, resources, grid, quantity,
                columns - 7, row, 5))
            return false;
    }
    return true;
}

bool draw_trainer(
    SDL_Renderer* renderer,
    const BootRenderResources& resources,
    const CampaignState& campaign,
    const PixelGrid& grid, int columns, int rows) {
    if (!draw_box(
            renderer, resources, grid, columns, rows) ||
        !draw_text(
            renderer, resources, grid,
            campaign.player_name, 2, 1, columns - 4))
        return false;
    const std::uint64_t seconds = campaign.play_steps / 60U;
    const std::string duration =
        std::to_string(seconds / 3600U) + ":" +
        (seconds / 60U % 60U < 10U ? "0" : "") +
        std::to_string(seconds / 60U % 60U);
    return draw_text(
               renderer, resources, grid, "TRAINER ID",
               2, 4, columns - 4) &&
           draw_text(
               renderer, resources, grid,
               std::to_string(campaign.trainer_id),
               columns / 2, 4, columns / 2 - 2) &&
           draw_text(
               renderer, resources, grid, "MONEY",
               2, 7, columns - 4) &&
           draw_text(
               renderer, resources, grid,
               std::to_string(campaign.money),
               columns / 2, 7, columns / 2 - 2) &&
           draw_text(
               renderer, resources, grid, "PLAY TIME",
               2, 10, columns - 4) &&
           draw_text(
               renderer, resources, grid, duration,
               columns / 2, 10, columns / 2 - 2);
}

} // namespace

bool draw_field_menu_overlay(
    SDL_Renderer* renderer, int output_width, int output_height,
    const BootRenderResources& resources,
    const WorldState& world, const CampaignState& campaign,
    const CampaignProgramCatalog& programs,
    const RuleCatalog& rules) {
    if (!world.menu.open) return true;
    const int scale = std::clamp(
        std::min(output_width / 320, output_height / 240),
        1, 3);
    if (world.menu.page == FieldMenuPage::root)
        return draw_root(
            renderer, resources, world.menu,
            output_width, output_height, scale);
    if (world.menu.page == FieldMenuPage::confirm_quit)
        return draw_quit_confirmation(
            renderer, resources, world.menu,
            output_width, output_height, scale);

    const int columns = std::clamp(
        (output_width - 48) / (scale * 8), 24, 38);
    const int rows = std::clamp(
        (output_height - 48) / (scale * 8), 14, 20);
    const PixelGrid grid = centered_grid(
        output_width, output_height, columns, rows, scale);
    if (world.menu.page == FieldMenuPage::party)
        return draw_party(
            renderer, resources, campaign, rules,
            grid, columns, rows);
    if (world.menu.page == FieldMenuPage::bag)
        return draw_bag(
            renderer, resources, campaign, programs,
            grid, columns, rows);
    return draw_trainer(
        renderer, resources, campaign,
        grid, columns, rows);
}

} // namespace pokered::render
