#pragma once

#include "import_battle_animations.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pokered::import {

struct BattleRuleImport {
    std::vector<GeneratedFile> files;
    std::size_t damage_formulas{};
    std::size_t critical_hit_programs{};
    std::size_t capture_formulas{};
    std::size_t experience_formulas{};
    std::size_t stat_formulas{};
};

// Lift the verified cartridge's battle calculation routines into semantic,
// campaign-owned programs consumed by the generic battle formula executor.
bool decode_battle_rule_import(std::span<const std::uint8_t> rom,
                               BattleRuleImport& result, std::string& error);

} // namespace pokered::import
