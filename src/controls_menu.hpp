#pragma once

#include "settings.hpp"

#include <string>

class GubsyRuntime;

namespace pokered {

void draw_controls_panel(GubsyRuntime& runtime, PresentationSettings& settings,
                         std::string& status);

} // namespace pokered
