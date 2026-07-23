#pragma once

#include "animations.hpp"
#include "catalog.hpp"
#include "diagnostics.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

namespace pokered {

struct BattleAnimationLabEntry {
    Symbol name;
    content::AnimationProgram program;
};

struct ImportedAnimationPiece {
    std::int16_t x{};
    std::int16_t y{};
    std::uint8_t tile_set{};
    std::uint8_t tile{};
    std::uint8_t attributes{};
};

struct ImportedAnimationVisual {
    Symbol name;
    std::vector<ImportedAnimationPiece> pieces;
};

struct ImportedAnimationAssets {
    std::vector<std::uint8_t> tile_set_0;
    std::vector<std::uint8_t> tile_set_1;
    std::vector<std::int8_t> wave_offsets;
    std::vector<std::uint8_t> minimized_mon_rows;
    std::vector<std::uint8_t> substitute_mon_tiles;
    std::vector<std::uint8_t> long_flash_dmg_palettes;
    std::vector<std::uint8_t> long_flash_sgb_palettes;
    std::vector<ImportedAnimationVisual> visuals;
};

struct BattleAnimationLab {
    std::filesystem::path source_root;
    content::Catalog catalog;
    std::vector<BattleAnimationLabEntry> entries;
    ImportedAnimationAssets imported_assets;
    AnimationState animation;
    std::size_t current{};
    std::uint32_t finished_ticks{};
    bool auto_advance{true};
    bool loaded{};
};

bool load_battle_animation_lab(const std::filesystem::path& source_root, BattleAnimationLab& result,
                               Diagnostics& diagnostics);
bool reload_battle_animation_lab(BattleAnimationLab& lab, Diagnostics& diagnostics);
void step_battle_animation_lab(BattleAnimationLab& lab);
void restart_battle_animation_lab(BattleAnimationLab& lab);
void next_battle_animation_lab(BattleAnimationLab& lab);
void previous_battle_animation_lab(BattleAnimationLab& lab);
std::string_view battle_animation_lab_name(const BattleAnimationLab& lab);
const ImportedAnimationVisual* find_imported_animation_visual(const ImportedAnimationAssets& assets,
                                                              const Symbol& name);

} // namespace pokered
