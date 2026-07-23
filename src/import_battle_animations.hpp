#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pokered::import {

// Portable generated output. Hosts decide how and where these files persist.
struct GeneratedFile {
    std::string relative_path;
    std::vector<std::uint8_t> bytes;
};

struct BattleAnimationImport {
    std::vector<GeneratedFile> files;
    std::size_t animation_programs{};
    std::size_t subanimations{};
    std::size_t frame_blocks{};
    std::size_t base_coordinates{};
    std::size_t visual_frames{};
    std::vector<std::string> unresolved_effects;
};

// All cartridge-derived import domains share one pinned source revision.
bool verify_pokemon_red_us_rev_0(std::span<const std::uint8_t> rom, std::string& error);

// Decode verified ROM bytes without filesystem or process access.
bool decode_battle_animation_import(std::span<const std::uint8_t> rom,
                                    BattleAnimationImport& result, std::string& error);

} // namespace pokered::import
