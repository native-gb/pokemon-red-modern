#pragma once

#include "catalog.hpp"
#include "state.hpp"
#include "window.hpp"

namespace pokered {

enum class ToolLayout {
    closed,
    player,
    developer,
};

struct ToolState {
    ToolLayout layout{ToolLayout::closed};
    bool arrange{};
};

void apply_tool_shortcuts(ToolState& tools, const WindowInput& input);
void draw_tools(ToolState& tools, GameState& game, const content::CatalogSummary& catalog,
                const char* renderer_name);

} // namespace pokered
