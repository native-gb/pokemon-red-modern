#pragma once

#include <filesystem>
#include <string>

namespace pokered {

struct PresentationSettings {
    bool vsync{true};
    bool motion_interpolation{true};
    bool show_fps{true};
    int render_rate_limit{144};
    int control_profile{};
    bool fast_forward_enabled{true};
    bool fast_forward_toggle{};
    bool automatic_camera_framing{true};
    int fast_forward_multiplier{3};

    bool operator==(const PresentationSettings&) const = default;
};

bool load_settings(const std::filesystem::path& path, PresentationSettings& settings,
                   std::string& error);
bool save_settings(const std::filesystem::path& path, const PresentationSettings& settings,
                   std::string& error);

} // namespace pokered
