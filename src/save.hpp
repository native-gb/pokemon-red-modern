#pragma once

#include <filesystem>
#include <string>

namespace pokered {

struct CampaignState;
struct InteractionCatalog;
struct WorldState;

bool save_game_exists(const std::filesystem::path& path);
bool save_game(const std::filesystem::path& path,
               const CampaignState& campaign,
               const WorldState& world, std::string& error);
bool load_game(const std::filesystem::path& path,
               CampaignState& campaign, WorldState& world,
               const InteractionCatalog& interactions,
               std::string& error);

} // namespace pokered
