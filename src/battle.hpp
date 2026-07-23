#pragma once

#include "pokemon.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace pokered {

struct BattleRuleCatalog;
struct RuleCatalog;
struct WildEncounterResult;

enum class BattleKind : std::uint8_t {
    wild,
    trainer,
};

enum class BattlePhase : std::uint8_t {
    choose_action,
    resolving,
    finished,
};

enum class BattleOutcome : std::uint8_t {
    ongoing,
    player_victory,
    player_defeat,
    escaped,
};

enum class BattleEventKind : std::uint8_t {
    used_move,
    missed,
    critical_hit,
    dealt_damage,
    fainted,
    gained_experience,
    sent_out,
};

struct BattleEvent {
    BattleEventKind kind{BattleEventKind::used_move};
    bool player_actor{};
    std::uint8_t move_id{};
    std::uint16_t value{};
    std::string text;
};

struct BattleSideState {
    std::size_t active_index{};
    // Attack, Defense, Speed, Special, Accuracy, and Evasion.
    std::array<std::uint8_t, 6> stat_stages{};
    bool focused{};
    bool accuracy_bypassed{};
};

struct BattleState {
    BattleKind kind{BattleKind::wild};
    BattlePhase phase{BattlePhase::finished};
    BattleOutcome outcome{BattleOutcome::ongoing};
    PartyState enemy_party;
    BattleSideState player;
    BattleSideState enemy;
    std::vector<BattleEvent> events;
    std::uint64_t turn{};
    std::uint32_t random_state{};
    bool active{};
};

struct BattleTurnInput {
    std::size_t player_move_slot{};
    std::size_t enemy_move_slot{};
};

bool begin_battle(const RuleCatalog& rules,
                  const BattleRuleCatalog& battle_rules,
                  const PartyState& player_party, PartyState enemy_party,
                  BattleKind kind, std::uint32_t random_seed,
                  BattleState& result, std::string& error);
bool begin_wild_battle(const RuleCatalog& rules,
                       const BattleRuleCatalog& battle_rules,
                       const PartyState& player_party,
                       const WildEncounterResult& encounter,
                       std::uint32_t random_seed, BattleState& result,
                       std::string& error);
std::optional<std::size_t> first_executable_move_slot(
    const RuleCatalog& rules, const BattleRuleCatalog& battle_rules,
    const PokemonState& pokemon);
bool execute_battle_turn(const RuleCatalog& rules,
                         const BattleRuleCatalog& battle_rules,
                         std::uint16_t player_trainer_id,
                         PartyState& player_party, BattleState& battle,
                         const BattleTurnInput& input, std::string& error);

} // namespace pokered
