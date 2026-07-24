#include "render/dialogue.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace pokered::render {

void draw_dialogue_overlay(const WorldState& world) {
    if (!world.dialogue.open || world.dialogue.page >= world.dialogue.pages.size()) return;

    ImGuiIO& io = ImGui::GetIO();
    const float width =
        std::floor(std::min(io.DisplaySize.x - 48.0F, 920.0F));
    const float height = 156.0F;
    const ImVec2 minimum{
        std::floor((io.DisplaySize.x - width) * 0.5F),
        std::floor(io.DisplaySize.y - height - 28.0F)};
    const ImVec2 maximum{minimum.x + width, minimum.y + height};
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(minimum, maximum,
                        IM_COL32(246, 238, 242, 248));
    draw->AddRect(minimum, maximum,
                  IM_COL32(42, 29, 42, 255), 0.0F, 0, 4.0F);

    const std::string& page = world.dialogue.pages[world.dialogue.page];
    const ImVec2 text_position{minimum.x + 24.0F, minimum.y + 21.0F};
    draw->AddText(ImGui::GetFont(), 27.0F, text_position, IM_COL32(35, 25, 35, 255), page.data(),
                  page.data() + page.size(), width - 48.0F);

    const float arrow_x = maximum.x - 30.0F;
    const float arrow_y = maximum.y - 23.0F;
    draw->AddTriangleFilled({arrow_x - 8.0F, arrow_y - 5.0F},
                            {arrow_x + 8.0F, arrow_y - 5.0F}, {arrow_x, arrow_y + 5.0F},
                            IM_COL32(35, 25, 35, 255));

    if (!world.choice.open || world.choice.options.empty()) return;
    const float choice_width = 180.0F;
    const float choice_height =
        28.0F + static_cast<float>(world.choice.options.size()) * 34.0F;
    const ImVec2 choice_minimum{
        maximum.x - choice_width - 18.0F,
        minimum.y - choice_height - 12.0F};
    const ImVec2 choice_maximum{
        choice_minimum.x + choice_width,
        choice_minimum.y + choice_height};
    draw->AddRectFilled(choice_minimum, choice_maximum,
                        IM_COL32(246, 238, 242, 248));
    draw->AddRect(choice_minimum, choice_maximum,
                  IM_COL32(42, 29, 42, 255), 0.0F, 0, 4.0F);
    for (std::size_t index = 0U; index < world.choice.options.size();
         ++index) {
        const float y =
            choice_minimum.y + 15.0F + static_cast<float>(index) * 34.0F;
        if (index == world.choice.selected)
            draw->AddTriangleFilled(
                {choice_minimum.x + 26.0F, y + 5.0F},
                {choice_minimum.x + 16.0F, y},
                {choice_minimum.x + 16.0F, y + 10.0F},
                IM_COL32(35, 25, 35, 255));
        const std::string& option = world.choice.options[index];
        draw->AddText(ImGui::GetFont(), 25.0F,
                      {choice_minimum.x + 38.0F, y - 7.0F},
                      IM_COL32(35, 25, 35, 255), option.data(),
                      option.data() + option.size());
    }
}

} // namespace pokered::render
