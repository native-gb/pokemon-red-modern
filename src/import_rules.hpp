#pragma once

#include "import_battle_animations.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pokered::import {

struct RuleImport {
    std::vector<GeneratedFile> files;
    std::size_t types{};
    std::size_t type_interactions{};
    std::size_t species{};
    std::size_t internal_species_slots{};
    std::size_t moves{};
    std::size_t learnset_entries{};
    std::size_t evolutions{};
    std::size_t growth_curves{};
    std::size_t machines{};
};

// Decode immutable Pokemon and battle-rule tables into readable source and a
// runtime-only compiled cache. Formula and effect programs remain separate
// domains even when they reference these records.
bool decode_rule_import(std::span<const std::uint8_t> rom, RuleImport& result, std::string& error);

} // namespace pokered::import
