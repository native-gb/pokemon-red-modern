#pragma once

#include "import_battle_animations.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pokered::import {

struct ScriptImport {
    std::vector<GeneratedFile> files;
    std::size_t map_slots{};
    std::size_t decoded_maps{};
    std::size_t aliases{};
    std::size_t unresolved_slots{};
    std::size_t script_entry_points{};
    std::size_t owned_map_entries{};
    std::size_t decoded_text_programs{};
    std::size_t decoded_interaction_scripts{};
    std::size_t untranslated_interaction_scripts{};
    std::size_t unresolved_owned_entries{};
    std::size_t background_interactions{};
    std::size_t actor_interactions{};
};

// Inventory every map-owned program and interaction before semantic lifting.
bool decode_script_import(std::span<const std::uint8_t> rom, ScriptImport& result,
                          std::string& error);

} // namespace pokered::import
