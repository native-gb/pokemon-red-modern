#pragma once

#include <string>

namespace pokered {

struct BattleAnimationLab;
struct BattleRuleCatalog;
struct BattleState;
struct PartyState;
struct RuleCatalog;

bool sync_battle_view(const RuleCatalog& rules,
                      const BattleRuleCatalog& battle_rules,
                      const PartyState& player_party,
                      const BattleState& battle,
                      BattleAnimationLab& view, std::string& error);

} // namespace pokered
