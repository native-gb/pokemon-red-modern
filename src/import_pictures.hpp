#pragma once

#include "import_battle_animations.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pokered::import {

struct PictureImport {
    std::vector<GeneratedFile> files;
    std::size_t species{};
    std::size_t front_pictures{};
    std::size_t back_pictures{};
    std::size_t trainer_classes{};
    std::size_t battle_ui_tiles{};
};

// Import decoded, GPU-upload-ready battle pixels and readable ROM bindings.
bool decode_picture_import(std::span<const std::uint8_t> rom, PictureImport& result,
                           std::string& error);

} // namespace pokered::import
