#pragma once

#include "battle_animation_lab.hpp"
#include "catalog.hpp"
#include "clocks.hpp"
#include "maps.hpp"
#include "render/maps.hpp"
#include "state.hpp"
#include "window.hpp"

#include <string>

namespace pokered {

enum class ToolLayout {
    closed,
    player,
    developer,
};

struct ToolState {
    ToolLayout layout{ToolLayout::closed};
    bool arrange{};
    bool controller_navigation{};
    std::string control_status;
};

void apply_tool_shortcuts(ToolState& tools, const WindowInput& input);
void draw_tools(ToolState& tools, GubsyRuntime& runtime, GameState& game,
                const content::CatalogSummary& catalog, BattleAnimationLab& lab, WorldState& maps,
                PresentationSettings& presentation, const GameClocks& clocks,
                const char* renderer_name);

} // namespace pokered
