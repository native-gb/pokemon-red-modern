#pragma once

#include "animations.hpp"
#include "catalog.hpp"
#include "diagnostics.hpp"

#include <cstddef>
#include <filesystem>
#include <string_view>
#include <vector>

namespace pokered {

struct BattleAnimationLabEntry {
    Symbol name;
    content::AnimationProgram program;
};

struct BattleAnimationLab {
    std::filesystem::path source_root;
    content::Catalog catalog;
    std::vector<BattleAnimationLabEntry> entries;
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

} // namespace pokered
