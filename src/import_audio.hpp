#pragma once

#include "import_battle_animations.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pokered::import {

struct AudioImport {
    std::vector<GeneratedFile> files;
    std::size_t banks{};
    std::size_t headers{};
    std::size_t programs{};
    std::size_t commands{};
    std::size_t waves{};
    std::size_t pitches{};
    std::size_t cries{};
    std::size_t map_music{};
    std::size_t scene_music{};
};

bool decode_audio_import(std::span<const std::uint8_t> rom,
                         AudioImport& result, std::string& error);

} // namespace pokered::import
