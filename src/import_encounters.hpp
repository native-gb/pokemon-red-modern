#pragma once

#include "import_battle_animations.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pokered::import {

struct EncounterImport {
    std::vector<GeneratedFile> files;
    std::size_t map_slots{};
    std::size_t active_land_tables{};
    std::size_t active_water_tables{};
    std::size_t slots{};
};

bool decode_encounter_import(std::span<const std::uint8_t> rom,
                             EncounterImport& result, std::string& error);

} // namespace pokered::import
