#pragma once

#include <string>

namespace pokered {

struct BattleAnimationLab;
struct BattleRuleCatalog;
struct CampaignState;
struct EncounterCatalog;
struct RuleCatalog;
struct WorldState;

struct BattleControlInput {
    bool previous{};
    bool next{};
    bool confirm{};
    bool back{};
};

struct BattleControlResult {
    bool finished{};
};

bool begin_world_wild_battle(
    const EncounterCatalog& encounters, WorldState& world,
    const RuleCatalog& rules, const BattleRuleCatalog& battle_rules,
    CampaignState& campaign, BattleAnimationLab& view, bool& began,
    std::string& error);
bool control_battle(const RuleCatalog& rules,
                    const BattleRuleCatalog& battle_rules,
                    CampaignState& campaign, BattleAnimationLab& view,
                    const BattleControlInput& input,
                    BattleControlResult& result, std::string& error);

} // namespace pokered
