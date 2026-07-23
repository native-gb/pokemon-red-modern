#include "render/dialogue.hpp"

#include <imgui.h>

#include <algorithm>

namespace pokered::render {

void draw_dialogue_overlay(const WorldState& world) {
    if (!world.dialogue.open || world.dialogue.page >= world.dialogue.pages.size()) return;

    ImGuiIO& io = ImGui::GetIO();
    const float width = std::min(io.DisplaySize.x - 48.0F, 920.0F);
    const float height = 156.0F;
    const ImVec2 minimum{(io.DisplaySize.x - width) * 0.5F, io.DisplaySize.y - height - 28.0F};
    const ImVec2 maximum{minimum.x + width, minimum.y + height};
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(minimum, maximum, IM_COL32(246, 238, 242, 248), 7.0F);
    draw->AddRect(minimum, maximum, IM_COL32(42, 29, 42, 255), 7.0F, 0, 4.0F);

    const std::string& page = world.dialogue.pages[world.dialogue.page];
    const ImVec2 text_position{minimum.x + 24.0F, minimum.y + 21.0F};
    draw->AddText(ImGui::GetFont(), 27.0F, text_position, IM_COL32(35, 25, 35, 255), page.data(),
                  page.data() + page.size(), width - 48.0F);

    const float arrow_x = maximum.x - 30.0F;
    const float arrow_y = maximum.y - 23.0F;
    draw->AddTriangleFilled({arrow_x - 8.0F, arrow_y - 5.0F},
                            {arrow_x + 8.0F, arrow_y - 5.0F}, {arrow_x, arrow_y + 5.0F},
                            IM_COL32(35, 25, 35, 255));
}

} // namespace pokered::render
