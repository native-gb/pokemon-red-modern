#pragma once

#include "import_battle_animations.hpp"

#include <filesystem>
#include <string>

namespace pokered::import {

bool write_battle_animation_import(const BattleAnimationImport& imported,
                                   const std::filesystem::path& output_root,
                                   std::string& error);

} // namespace pokered::import
