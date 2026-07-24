#include "settings.hpp"

#include <charconv>
#include <fstream>
#include <string>
#include <string_view>

namespace pokered {
namespace {

constexpr int kCurrentSchema = 2;

bool parse_bool(std::string_view text, bool& value) {
    if (text == "true") {
        value = true;
        return true;
    }
    if (text == "false") {
        value = false;
        return true;
    }
    return false;
}

bool parse_int(std::string_view text, int& value) {
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool valid_render_rate(int value) {
    return value == 60 || value == 120 || value == 144 || value == 165 || value == 240;
}

bool parse_setting(PresentationSettings& settings, std::string_view key,
                   std::string_view value) {
    if (key == "vsync") return parse_bool(value, settings.vsync);
    if (key == "motion_interpolation")
        return parse_bool(value, settings.motion_interpolation);
    if (key == "show_fps") return parse_bool(value, settings.show_fps);
    if (key == "fast_forward_enabled")
        return parse_bool(value, settings.fast_forward_enabled);
    if (key == "fast_forward_toggle")
        return parse_bool(value, settings.fast_forward_toggle);
    if (key == "automatic_camera_framing")
        return parse_bool(value,
                          settings.automatic_camera_framing);
    if (key == "render_rate_limit") {
        int rate = 0;
        if (!parse_int(value, rate) || !valid_render_rate(rate)) return false;
        settings.render_rate_limit = rate;
        return true;
    }
    if (key == "control_profile") {
        int profile = 0;
        if (!parse_int(value, profile) || profile < 0 || profile > 1) return false;
        settings.control_profile = profile;
        return true;
    }
    if (key == "fast_forward_multiplier") {
        int multiplier = 0;
        if (!parse_int(value, multiplier) || multiplier < 2 || multiplier > 8) return false;
        settings.fast_forward_multiplier = multiplier;
        return true;
    }
    return false;
}

void write_bool(std::ofstream& output, std::string_view key, bool value) {
    output << key << '=' << (value ? "true" : "false") << '\n';
}

} // namespace

bool load_settings(const std::filesystem::path& path, PresentationSettings& settings,
                   std::string& error) {
    error.clear();
    std::ifstream input(path);
    if (!input) return !std::filesystem::exists(path);

    PresentationSettings loaded;
    int schema = 0;
    int line_number = 0;
    std::string line;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty() || line.front() == '#') continue;
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) {
            error = "invalid setting on line " + std::to_string(line_number);
            return false;
        }
        const std::string_view key = std::string_view(line).substr(0, separator);
        const std::string_view value = std::string_view(line).substr(separator + 1U);
        if (key == "schema") {
            if (!parse_int(value, schema) || schema < 0 || schema > kCurrentSchema) {
                error = "unsupported settings schema on line " + std::to_string(line_number);
                return false;
            }
        } else if (!parse_setting(loaded, key, value)) {
            error = "invalid setting on line " + std::to_string(line_number);
            return false;
        }
    }
    settings = loaded;
    return true;
}

bool save_settings(const std::filesystem::path& path, const PresentationSettings& settings,
                   std::string& error) {
    error.clear();
    std::error_code filesystem_error;
    std::filesystem::create_directories(path.parent_path(), filesystem_error);
    if (filesystem_error) {
        error = filesystem_error.message();
        return false;
    }

    const std::filesystem::path temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) {
        error = "could not create settings file";
        return false;
    }
    output << "schema=" << kCurrentSchema << '\n';
    write_bool(output, "vsync", settings.vsync);
    write_bool(output, "motion_interpolation", settings.motion_interpolation);
    write_bool(output, "show_fps", settings.show_fps);
    output << "render_rate_limit=" << settings.render_rate_limit << '\n'
           << "control_profile=" << settings.control_profile << '\n';
    write_bool(output, "fast_forward_enabled", settings.fast_forward_enabled);
    write_bool(output, "fast_forward_toggle", settings.fast_forward_toggle);
    write_bool(output, "automatic_camera_framing",
               settings.automatic_camera_framing);
    output << "fast_forward_multiplier=" << settings.fast_forward_multiplier << '\n';
    output.close();
    if (!output) {
        error = "could not finish settings file";
        return false;
    }
    std::filesystem::rename(temporary, path, filesystem_error);
    if (filesystem_error) {
        error = filesystem_error.message();
        std::filesystem::remove(temporary, filesystem_error);
        return false;
    }
    return true;
}

} // namespace pokered
