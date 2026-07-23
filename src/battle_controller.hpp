#pragma once

#include <string>

namespace pokered {

struct BattleAnimationLab;
struct BattleRuleCatalog;
struct CampaignState;
struct EncounterCatalog;
struct InteractionCatalog;
struct RuleCatalog;
struct TrainerCatalog;
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
bool service_world_actor_battle(
    const InteractionCatalog& interactions,
    const TrainerCatalog& trainers, WorldState& world,
    const RuleCatalog& rules,
    const BattleRuleCatalog& battle_rules,
    CampaignState& campaign, BattleAnimationLab& view, bool& began,
    std::string& error);
bool begin_campaign_trainer_battle(
    const TrainerCatalog& trainers, WorldState& world,
    const RuleCatalog& rules,
    const BattleRuleCatalog& battle_rules,
    CampaignState& campaign, BattleAnimationLab& view, bool& began,
    std::string& error);
void finish_world_actor_battle(
    const InteractionCatalog& interactions, WorldState& world,
    CampaignState& campaign);
bool control_battle(const RuleCatalog& rules,
                    const BattleRuleCatalog& battle_rules,
                    CampaignState& campaign, BattleAnimationLab& view,
                    const BattleControlInput& input,
                    BattleControlResult& result, std::string& error);

} // namespace pokered
