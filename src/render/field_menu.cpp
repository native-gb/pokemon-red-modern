#include "render/field_menu.hpp"

#include "campaign_programs.hpp"
#include "maps.hpp"
#include "rules.hpp"
#include "state.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <string_view>

namespace pokered::render {
namespace {

constexpr ImU32 panel_color = IM_COL32(246, 238, 242, 250);
constexpr ImU32 border_color = IM_COL32(42, 29, 42, 255);
constexpr ImU32 text_color = IM_COL32(35, 25, 35, 255);
constexpr ImU32 selected_color = IM_COL32(230, 207, 218, 255);

void panel(ImDrawList* draw, ImVec2 minimum, ImVec2 maximum) {
    draw->AddRectFilled(minimum, maximum, panel_color, 8.0F);
    draw->AddRect(minimum, maximum, border_color, 8.0F, 0, 4.0F);
}

void text(ImDrawList* draw, float size, ImVec2 position,
          std::string_view value) {
    draw->AddText(
        ImGui::GetFont(), size, position, text_color,
        value.data(), value.data() + value.size());
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

std::string item_name(const CampaignProgramCatalog& programs,
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

void draw_root(ImDrawList* draw, const FieldMenuState& menu,
               ImVec2 minimum, ImVec2 maximum) {
    panel(draw, minimum, maximum);
    constexpr std::array<std::string_view, 4> entries{
        "PARTY", "BAG", "TRAINER", "CLOSE"};
    for (std::size_t index = 0U; index < entries.size(); ++index) {
        const float y =
            minimum.y + 25.0F + static_cast<float>(index) * 47.0F;
        if (index == menu.selected)
            draw->AddRectFilled(
                {minimum.x + 14.0F, y - 7.0F},
                {maximum.x - 14.0F, y + 31.0F},
                selected_color, 5.0F);
        text(draw, 27.0F, {minimum.x + 34.0F, y}, entries[index]);
    }
}

void draw_party(ImDrawList* draw, const CampaignState& campaign,
                const RuleCatalog& rules, ImVec2 minimum,
                ImVec2 maximum) {
    panel(draw, minimum, maximum);
    text(draw, 30.0F, {minimum.x + 28.0F, minimum.y + 22.0F},
         "PARTY");
    if (campaign.party.members.empty()) {
        text(draw, 26.0F, {minimum.x + 28.0F, minimum.y + 82.0F},
             "No POKéMON.");
        return;
    }
    for (std::size_t index = 0U;
         index < campaign.party.members.size(); ++index) {
        const PokemonState& pokemon = campaign.party.members[index];
        const SpeciesRule* species =
            find_species(rules, pokemon.species_dex);
        const std::string name =
            pokemon.nickname.empty() && species != nullptr
                ? species->name
                : pokemon.nickname;
        const float y =
            minimum.y + 72.0F + static_cast<float>(index) * 68.0F;
        text(draw, 25.0F, {minimum.x + 30.0F, y}, name);
        char details[96]{};
        std::snprintf(
            details, sizeof(details), "Lv%u   HP %u/%u   %s",
            static_cast<unsigned>(pokemon.level),
            static_cast<unsigned>(pokemon.current_hp),
            static_cast<unsigned>(pokemon.stats.hp),
            status_name(pokemon.status).data());
        text(draw, 21.0F, {minimum.x + 300.0F, y + 3.0F}, details);
    }
}

void draw_bag(ImDrawList* draw, const CampaignState& campaign,
              const CampaignProgramCatalog& programs, ImVec2 minimum,
              ImVec2 maximum) {
    panel(draw, minimum, maximum);
    text(draw, 30.0F, {minimum.x + 28.0F, minimum.y + 22.0F},
         "BAG");
    if (campaign.inventory.stacks.empty()) {
        text(draw, 26.0F, {minimum.x + 28.0F, minimum.y + 82.0F},
             "The BAG is empty.");
        return;
    }
    const std::size_t visible =
        std::min<std::size_t>(campaign.inventory.stacks.size(), 12U);
    for (std::size_t index = 0U; index < visible; ++index) {
        const InventoryStack& stack =
            campaign.inventory.stacks[index];
        const float y =
            minimum.y + 72.0F + static_cast<float>(index) * 39.0F;
        text(draw, 24.0F, {minimum.x + 30.0F, y},
             item_name(programs, stack.item_id));
        const std::string quantity =
            "×" + std::to_string(stack.quantity);
        text(draw, 23.0F, {maximum.x - 110.0F, y}, quantity);
    }
}

void draw_trainer(ImDrawList* draw, const CampaignState& campaign,
                  ImVec2 minimum, ImVec2 maximum) {
    panel(draw, minimum, maximum);
    text(draw, 31.0F, {minimum.x + 30.0F, minimum.y + 28.0F},
         campaign.player_name);
    text(draw, 25.0F, {minimum.x + 30.0F, minimum.y + 100.0F},
         "TRAINER ID");
    text(draw, 25.0F, {minimum.x + 330.0F, minimum.y + 100.0F},
         std::to_string(campaign.trainer_id));
    text(draw, 25.0F, {minimum.x + 30.0F, minimum.y + 155.0F},
         "MONEY");
    text(draw, 25.0F, {minimum.x + 330.0F, minimum.y + 155.0F},
         "¥" + std::to_string(campaign.money));
    text(draw, 25.0F, {minimum.x + 30.0F, minimum.y + 210.0F},
         "PLAY TIME");
    const std::uint64_t seconds = campaign.play_steps / 60U;
    const std::string duration =
        std::to_string(seconds / 3600U) + ":" +
        (seconds / 60U % 60U < 10U ? "0" : "") +
        std::to_string(seconds / 60U % 60U);
    text(draw, 25.0F, {minimum.x + 330.0F, minimum.y + 210.0F},
         duration);
}

} // namespace

void draw_field_menu_overlay(
    const WorldState& world, const CampaignState& campaign,
    const CampaignProgramCatalog& programs,
    const RuleCatalog& rules) {
    if (!world.menu.open) return;
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    if (world.menu.page == FieldMenuPage::root) {
        const ImVec2 maximum{
            io.DisplaySize.x - 32.0F, io.DisplaySize.y - 32.0F};
        const ImVec2 minimum{maximum.x - 270.0F, maximum.y - 230.0F};
        draw_root(draw, world.menu, minimum, maximum);
        return;
    }
    const float width = std::min(io.DisplaySize.x - 64.0F, 900.0F);
    const float height = std::min(io.DisplaySize.y - 64.0F, 540.0F);
    const ImVec2 minimum{
        (io.DisplaySize.x - width) * 0.5F,
        (io.DisplaySize.y - height) * 0.5F};
    const ImVec2 maximum{minimum.x + width, minimum.y + height};
    if (world.menu.page == FieldMenuPage::party)
        draw_party(draw, campaign, rules, minimum, maximum);
    else if (world.menu.page == FieldMenuPage::bag)
        draw_bag(draw, campaign, programs, minimum, maximum);
    else
        draw_trainer(draw, campaign, minimum, maximum);
}

} // namespace pokered::render
