#pragma once

#include "import_battle_animations.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pokered::import {

struct CampaignProgramImport {
    std::vector<GeneratedFile> files;
    std::size_t programs{};
};

// Lift verified campaign control flow into readable source and a compact
// runtime cache. This importer owns Pokemon Red addresses; the engine does not.
bool decode_campaign_program_import(std::span<const std::uint8_t> rom,
                                    CampaignProgramImport& result, std::string& error);

} // namespace pokered::import
