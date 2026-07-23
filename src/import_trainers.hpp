#pragma once

#include "import_battle_animations.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pokered::import {

struct TrainerImport {
    std::vector<GeneratedFile> files;
    std::size_t classes{};
    std::size_t parties{};
    std::size_t members{};
};

bool decode_trainer_import(std::span<const std::uint8_t> rom,
                           TrainerImport& result, std::string& error);

} // namespace pokered::import
