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

struct ImportedBattlePicture {
    std::uint8_t width_tiles{};
    std::uint8_t height_tiles{};
    std::uint32_t rom_offset{};
    std::uint32_t compressed_size{};
    std::vector<std::uint8_t> pixels;
};

struct ImportedPokemonVisual {
    std::string name;
    ImportedBattlePicture front;
    ImportedBattlePicture back;
};

struct ImportedTrainerVisual {
    std::string name;
    ImportedBattlePicture portrait;
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
    std::vector<ImportedPokemonVisual> pokemon;
    std::vector<ImportedTrainerVisual> trainers;
};

struct BattleAnimationLab {
    std::filesystem::path source_root;
    content::Catalog catalog;
    std::vector<BattleAnimationLabEntry> entries;
    ImportedAnimationAssets imported_assets;
    AnimationState animation;
    std::size_t current{};
    std::size_t current_species{};
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
void next_battle_species(BattleAnimationLab& lab);
void previous_battle_species(BattleAnimationLab& lab);
std::string_view battle_animation_lab_name(const BattleAnimationLab& lab);
std::string_view battle_animation_lab_species_name(const BattleAnimationLab& lab);
const ImportedPokemonVisual* battle_animation_lab_species(const BattleAnimationLab& lab);
const ImportedAnimationVisual* find_imported_animation_visual(const ImportedAnimationAssets& assets,
                                                              const Symbol& name);

} // namespace pokered
