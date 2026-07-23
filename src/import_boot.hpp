#pragma once

#include "import_battle_animations.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pokered::import {

struct BootImport {
    std::vector<GeneratedFile> files;
    std::size_t images{};
    std::size_t title_species{};
    std::size_t text_programs{};
};

// Import the cartridge-owned startup presentation into readable source and a
// compact cache. Runtime code never sees the ROM offsets used by this decoder.
bool decode_boot_import(std::span<const std::uint8_t> rom, BootImport& result,
                        std::string& error);

} // namespace pokered::import
