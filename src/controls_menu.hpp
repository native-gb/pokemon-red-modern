#pragma once

#include "settings.hpp"

#include <string>

class GubsyRuntime;

namespace pokered {

void sync_controls_navigation(bool visible, bool& navigation_enabled);
void draw_controls_panel(GubsyRuntime& runtime, PresentationSettings& settings,
                         std::string& status);

} // namespace pokered
