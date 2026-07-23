#pragma once

#include "import_battle_animations.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pokered::import {

struct MapImport {
    std::vector<GeneratedFile> files;
    std::size_t maps{};
    std::size_t tilesets{};
    std::size_t expanded_tiles{};
};

// Import a small verified outdoor-map slice into readable source and a runtime cache.
bool decode_map_import(std::span<const std::uint8_t> rom, MapImport& result,
                       std::string& error);

} // namespace pokered::import
