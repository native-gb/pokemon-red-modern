#include "render/naming.hpp"

#include <imgui.h>

#include <algorithm>

namespace pokered::render {

void draw_naming_overlay(const WorldState& world) {
    const NamingState& naming = world.naming;
    if (!naming.open || !valid_naming_profile(naming.profile)) return;

    ImGuiIO& io = ImGui::GetIO();
    const float width = std::min(io.DisplaySize.x - 48.0F, 860.0F);
    const float height = std::min(io.DisplaySize.y - 48.0F, 630.0F);
    const ImVec2 minimum{(io.DisplaySize.x - width) * 0.5F,
                         (io.DisplaySize.y - height) * 0.5F};
    const ImVec2 maximum{minimum.x + width, minimum.y + height};
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(minimum, maximum, IM_COL32(246, 238, 242, 255),
                        9.0F);
    draw->AddRect(minimum, maximum, IM_COL32(42, 29, 42, 255), 9.0F,
                  0, 5.0F);

    draw->AddText(ImGui::GetFont(), 29.0F,
                  {minimum.x + 32.0F, minimum.y + 25.0F},
                  IM_COL32(35, 25, 35, 255), naming.heading.data(),
                  naming.heading.data() + naming.heading.size());
    draw->AddText(ImGui::GetFont(), 31.0F,
                  {minimum.x + 405.0F, minimum.y + 34.0F},
                  IM_COL32(35, 25, 35, 255), naming.value.data(),
                  naming.value.data() + naming.value.size());

    const float underscore_y = minimum.y + 78.0F;
    for (std::size_t index = 0U;
         index < naming.profile.maximum_length; ++index) {
        const float x =
            minimum.x + 405.0F + static_cast<float>(index) * 35.0F;
        draw->AddLine({x, underscore_y}, {x + 25.0F, underscore_y},
                      IM_COL32(72, 55, 72, 255), 2.0F);
    }

    const float grid_left = minimum.x + 72.0F;
    const float grid_top = minimum.y + 145.0F;
    const float cell_width = (width - 144.0F) /
                             static_cast<float>(kNamingColumns);
    constexpr float cell_height = 65.0F;
    for (std::uint8_t row = 0U; row < kNamingRows; ++row) {
        for (std::uint8_t column = 0U; column < kNamingColumns;
             ++column) {
            const float x =
                grid_left + static_cast<float>(column) * cell_width;
            const float y =
                grid_top + static_cast<float>(row) * cell_height;
            const bool selected =
                naming.row == row && naming.column == column;
            if (selected)
                draw->AddRectFilled(
                    {x - 9.0F, y - 8.0F},
                    {x + cell_width - 13.0F, y + 39.0F},
                    IM_COL32(230, 207, 218, 255), 6.0F);
            const std::string& cell = naming_cell(naming, row, column);
            draw->AddText(
                ImGui::GetFont(), 28.0F, {x, y},
                IM_COL32(35, 25, 35, 255), cell.data(),
                cell.data() + cell.size());
        }
    }

    const std::string& case_action =
        naming.lowercase ? naming.profile.uppercase_action
                         : naming.profile.lowercase_action;
    const ImVec2 action_minimum{
        grid_left - 12.0F,
        grid_top + static_cast<float>(kNamingRows) * cell_height - 5.0F};
    const ImVec2 action_maximum{action_minimum.x + 250.0F,
                                action_minimum.y + 48.0F};
    if (naming.row == kNamingRows)
        draw->AddRectFilled(action_minimum, action_maximum,
                            IM_COL32(230, 207, 218, 255), 6.0F);
    draw->AddText(ImGui::GetFont(), 25.0F,
                  {action_minimum.x + 12.0F, action_minimum.y + 9.0F},
                  IM_COL32(35, 25, 35, 255), case_action.data(),
                  case_action.data() + case_action.size());
}

} // namespace pokered::render
