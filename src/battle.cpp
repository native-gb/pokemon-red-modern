#include "battle.hpp"

#include "battle_rules.hpp"
#include "encounters.hpp"
#include "rules.hpp"
#include "trainers.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace pokered {
namespace {

struct SelectedMove {
    PokemonMoveState* state{};
    const MoveRule* rule{};
    const MoveEffectProgram* effect{};
};

struct ActionResult {
    bool target_fainted{};
};

std::optional<std::size_t> first_usable(const PartyState& party) {
    const auto found = std::ranges::find_if(
        party.members,
        [](const PokemonState& pokemon) { return pokemon.current_hp != 0U; });
    if (found == party.members.end()) return std::nullopt;
    return static_cast<std::size_t>(
        std::distance(party.members.begin(), found));
}

bool valid_party(const RuleCatalog& rules, const PartyState& party) {
    if (party.members.empty()) return false;
    return std::ranges::all_of(
        party.members, [&](const PokemonState& pokemon) {
            if (find_species(rules, pokemon.species_dex) == nullptr ||
                pokemon.level == 0U || pokemon.stats.hp == 0U ||
                pokemon.current_hp > pokemon.stats.hp) {
                return false;
            }
            return std::ranges::all_of(
                pokemon.moves, [&](const PokemonMoveState& move) {
                    return move.move_id == 0U ||
                           (find_move(rules, move.move_id) != nullptr &&
                            move.pp <= move.maximum_pp);
                });
        });
}

void reset_stages(BattleSideState& side, std::uint8_t neutral_stage) {
    side.stat_stages.fill(neutral_stage);
    side.focused = false;
    side.accuracy_bypassed = false;
}

std::uint8_t next_random_byte(std::uint32_t& state) {
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    return static_cast<std::uint8_t>(state >> 24U);
}

std::array<std::uint8_t, 512> preview_random(std::uint32_t state) {
    std::array<std::uint8_t, 512> bytes{};
    for (std::uint8_t& byte : bytes) byte = next_random_byte(state);
    return bytes;
}

void consume_random(std::uint32_t& state, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index)
        (void)next_random_byte(state);
}

std::uint16_t stage_stat(const AccuracyFormulaProgram& accuracy,
                         std::uint16_t stat, std::uint8_t stage) {
    const AccuracyStageRatio& ratio = accuracy.stage_ratios[stage - 1U];
    const std::uint64_t scaled =
        static_cast<std::uint64_t>(stat) * ratio.numerator /
        ratio.denominator;
    return static_cast<std::uint16_t>(
        std::clamp<std::uint64_t>(scaled, 1U, 999U));
}

bool select_move(const RuleCatalog& rules,
                 const BattleRuleCatalog& battle_rules,
                 PokemonState& pokemon, std::size_t slot,
                 SelectedMove& result, std::string& error) {
    if (slot >= pokemon.moves.size()) {
        error = "battle action selected an invalid move slot";
        return false;
    }
    PokemonMoveState& state = pokemon.moves[slot];
    const MoveRule* rule = find_move(rules, state.move_id);
    if (rule == nullptr || state.pp == 0U) {
        error = "battle action selected an absent or exhausted move";
        return false;
    }
    const MoveEffectProgram* effect =
        find_move_effect_program(battle_rules, rule->effect_id);
    if (effect == nullptr) {
        error = "selected move effect has no imported executable program";
        return false;
    }
    result = {
        .state = &state,
        .rule = rule,
        .effect = effect,
    };
    error.clear();
    return true;
}

bool execute_action(const RuleCatalog& rules,
                    const DamageFormulaProgram& damage_formula,
                    const CriticalHitProgram& critical_program,
                    const AccuracyFormulaProgram& accuracy_formula,
                    PokemonState& attacker, PokemonState& defender,
                    BattleSideState& attacker_side,
                    BattleSideState& defender_side,
                    bool player_actor, const SelectedMove& selected,
                    BattleState& battle, ActionResult& result,
                    std::string& error) {
    result = {};
    const SpeciesRule* attacker_species =
        find_species(rules, attacker.species_dex);
    const SpeciesRule* defender_species =
        find_species(rules, defender.species_dex);
    if (attacker_species == nullptr || defender_species == nullptr ||
        selected.state == nullptr || selected.rule == nullptr ||
        selected.effect == nullptr) {
        error = "battle action has invalid battler or move bindings";
        return false;
    }

    --selected.state->pp;
    battle.events.push_back({
        .kind = BattleEventKind::used_move,
        .player_actor = player_actor,
        .move_id = selected.rule->id,
        .text = attacker.nickname + " used " + selected.rule->name,
    });

    bool hit = true;
    bool critical = false;
    bool damage_ready = false;
    bool outcome_reached = false;
    bool effect_applies = true;
    std::uint16_t damage = 0U;
    for (const MoveEffectInstruction& instruction :
         selected.effect->instructions) {
        switch (instruction.opcode) {
        case MoveEffectOpcode::check_accuracy: {
            if (!effect_applies) break;
            const auto random = preview_random(battle.random_state);
            AccuracyFormulaResult accuracy;
            if (!execute_accuracy_formula(
                    accuracy_formula,
                    {
                        .raw_accuracy = selected.rule->accuracy_raw,
                        .accuracy_stage = attacker_side.stat_stages[4],
                        .target_evasion_stage =
                            defender_side.stat_stages[5],
                        .bypassed = attacker_side.accuracy_bypassed,
                    },
                    random, accuracy, error)) {
                return false;
            }
            consume_random(battle.random_state,
                           accuracy.random_bytes_consumed);
            hit = accuracy.hit;
            if (!hit) {
                effect_applies = false;
                outcome_reached = true;
                battle.events.push_back({
                    .kind = BattleEventKind::missed,
                    .player_actor = player_actor,
                    .move_id = selected.rule->id,
                    .text = "The attack missed",
                });
            }
            break;
        }
        case MoveEffectOpcode::calculate_critical: {
            if (!hit || !effect_applies) break;
            const auto random = preview_random(battle.random_state);
            CriticalHitResult critical_result;
            if (!execute_critical_hit_program(
                    critical_program,
                    {
                        .base_speed = attacker_species->base_speed,
                        .move_id = selected.rule->id,
                        .move_power = selected.rule->power,
                        .focused = attacker_side.focused,
                    },
                    random, critical_result, error)) {
                return false;
            }
            consume_random(battle.random_state,
                           critical_result.random_bytes_consumed);
            critical = critical_result.critical;
            if (critical)
                battle.events.push_back({
                    .kind = BattleEventKind::critical_hit,
                    .player_actor = player_actor,
                    .move_id = selected.rule->id,
                    .text = "A critical hit",
                });
            break;
        }
        case MoveEffectOpcode::calculate_damage: {
            if (!hit || !effect_applies) break;
            const bool physical =
                selected.rule->damage_class == MoveDamageClass::physical;
            const std::uint16_t raw_attack =
                physical ? attacker.stats.attack : attacker.stats.special;
            const std::uint16_t raw_defense =
                physical ? defender.stats.defense : defender.stats.special;
            const std::size_t attack_stage = physical ? 0U : 3U;
            const std::size_t defense_stage = physical ? 1U : 3U;
            const std::uint16_t attack =
                critical ? raw_attack
                         : stage_stat(accuracy_formula, raw_attack,
                                      attacker_side.stat_stages[attack_stage]);
            const std::uint16_t defense =
                critical ? raw_defense
                         : stage_stat(accuracy_formula, raw_defense,
                                      defender_side.stat_stages[defense_stage]);
            const auto random = preview_random(battle.random_state);
            DamageFormulaResult calculated;
            if (!execute_damage_formula(
                    rules, damage_formula,
                    {
                        .level = attacker.level,
                        .power = selected.rule->power,
                        .move_type = selected.rule->type_id,
                        .move_effect = selected.rule->effect_id,
                        .attack = attack,
                        .defense = defense,
                        .attacker_types = attacker_species->type_ids,
                        .defender_types = defender_species->type_ids,
                        .critical = critical,
                    },
                    random, calculated, error)) {
                return false;
            }
            consume_random(battle.random_state,
                           calculated.random_bytes_consumed);
            damage = calculated.damage;
            damage_ready = true;
            break;
        }
        case MoveEffectOpcode::deal_damage:
            if (!hit || !effect_applies) {
                outcome_reached = true;
                break;
            }
            if (!damage_ready) {
                error =
                    "move-effect program dealt damage before calculating it";
                return false;
            }
            damage = std::min(damage, defender.current_hp);
            defender.current_hp =
                static_cast<std::uint16_t>(defender.current_hp - damage);
            battle.events.push_back({
                .kind = BattleEventKind::dealt_damage,
                .player_actor = player_actor,
                .move_id = selected.rule->id,
                .value = damage,
                .text = {},
            });
            outcome_reached = true;
            if (defender.current_hp == 0U) {
                battle.events.push_back({
                    .kind = BattleEventKind::fainted,
                    .player_actor = !player_actor,
                    .text = defender.nickname + " fainted",
                });
                result.target_fainted = true;
            }
            break;
        case MoveEffectOpcode::enemy_random_gate:
            if (!player_actor) {
                const std::uint8_t random =
                    next_random_byte(battle.random_state);
                if (random < instruction.operands[0]) {
                    effect_applies = false;
                    outcome_reached = true;
                    battle.events.push_back({
                        .kind = BattleEventKind::failed,
                        .player_actor = player_actor,
                        .move_id = selected.rule->id,
                        .text = "But it failed",
                    });
                }
            }
            break;
        case MoveEffectOpcode::modify_stage: {
            if (!effect_applies) break;
            BattleSideState& side =
                instruction.operands[0] == 0U ? attacker_side
                                              : defender_side;
            const std::size_t stage_index = instruction.operands[1];
            const std::int16_t delta =
                static_cast<std::int16_t>(instruction.operands[2]);
            const int updated =
                static_cast<int>(side.stat_stages[stage_index]) + delta;
            side.stat_stages[stage_index] = static_cast<std::uint8_t>(
                std::clamp(updated, 1, 13));
            battle.events.push_back({
                .kind = BattleEventKind::stat_changed,
                .player_actor =
                    instruction.operands[0] == 0U ? player_actor
                                                  : !player_actor,
                .move_id = selected.rule->id,
                .value = side.stat_stages[stage_index],
                .text = {},
            });
            outcome_reached = true;
            break;
        }
        }
    }
    if (!outcome_reached) {
        error = "move-effect program did not reach an action outcome";
        return false;
    }
    error.clear();
    return true;
}

bool settle_faint(const RuleCatalog& rules,
                  const ExperienceFormulaProgram& experience_formula,
                  const StatFormulaProgram& stat_formula,
                  std::uint8_t neutral_stage,
                  std::uint16_t player_trainer_id,
                  bool player_target, PartyState& player_party,
                  BattleState& battle, std::string& error) {
    if (player_target) {
        const std::optional<std::size_t> next = first_usable(player_party);
        if (!next.has_value()) {
            battle.outcome = BattleOutcome::player_defeat;
            battle.phase = BattlePhase::finished;
            error.clear();
            return true;
        }
        battle.player.active_index = *next;
        reset_stages(battle.player, neutral_stage);
        battle.events.push_back({
            .kind = BattleEventKind::sent_out,
            .player_actor = true,
            .text = player_party.members[*next].nickname,
        });
        battle.phase = BattlePhase::choose_action;
        error.clear();
        return true;
    }

    PokemonState& recipient =
        player_party.members[battle.player.active_index];
    const PokemonState& defeated =
        battle.enemy_party.members[battle.enemy.active_index];
    ExperienceAwardResult award;
    if (recipient.current_hp != 0U &&
        !award_pokemon_experience(
            rules, experience_formula, stat_formula, defeated,
            battle.kind == BattleKind::trainer, player_trainer_id, 1U,
            recipient, award, error)) {
        return false;
    }
    if (recipient.current_hp != 0U)
        battle.events.push_back({
            .kind = BattleEventKind::gained_experience,
            .player_actor = true,
            .value = static_cast<std::uint16_t>(
                std::min<std::uint32_t>(
                    award.experience_gained,
                    std::numeric_limits<std::uint16_t>::max())),
            .text = recipient.nickname,
        });

    const std::optional<std::size_t> next =
        first_usable(battle.enemy_party);
    if (!next.has_value()) {
        battle.outcome = BattleOutcome::player_victory;
        battle.phase = BattlePhase::finished;
        error.clear();
        return true;
    }
    battle.enemy.active_index = *next;
    reset_stages(battle.enemy, neutral_stage);
    battle.events.push_back({
        .kind = BattleEventKind::sent_out,
        .player_actor = false,
        .text = battle.enemy_party.members[*next].nickname,
    });
    battle.phase = BattlePhase::choose_action;
    error.clear();
    return true;
}

} // namespace

bool begin_battle(const RuleCatalog& rules,
                  const BattleRuleCatalog& battle_rules,
                  const PartyState& player_party, PartyState enemy_party,
                  BattleKind kind, std::uint32_t random_seed,
                  BattleState& result, std::string& error) {
    const AccuracyFormulaProgram* accuracy = find_accuracy_formula(
        battle_rules, battle_rules.original_accuracy_formula);
    const DamageFormulaProgram* damage = find_damage_formula(
        battle_rules, battle_rules.original_damage_formula);
    const CriticalHitProgram* critical = find_critical_hit_program(
        battle_rules, battle_rules.original_critical_hit_program);
    const ExperienceFormulaProgram* experience = find_experience_formula(
        battle_rules, battle_rules.original_experience_formula);
    const StatFormulaProgram* stats = find_stat_formula(
        battle_rules, battle_rules.original_stat_formula);
    const std::optional<std::size_t> player_active =
        first_usable(player_party);
    const std::optional<std::size_t> enemy_active =
        first_usable(enemy_party);
    if (!rules.loaded || !battle_rules.loaded ||
        !valid_party(rules, player_party) ||
        !valid_party(rules, enemy_party) || !player_active.has_value() ||
        !enemy_active.has_value() || accuracy == nullptr || damage == nullptr ||
        critical == nullptr || experience == nullptr || stats == nullptr ||
        accuracy->neutral_stage == 0U) {
        error = "battle cannot begin with incomplete rules or parties";
        return false;
    }

    BattleState started;
    started.kind = kind;
    started.phase = BattlePhase::choose_action;
    started.outcome = BattleOutcome::ongoing;
    started.enemy_party = std::move(enemy_party);
    started.player.active_index = *player_active;
    started.enemy.active_index = *enemy_active;
    reset_stages(started.player, accuracy->neutral_stage);
    reset_stages(started.enemy, accuracy->neutral_stage);
    started.random_state = random_seed == 0U ? 1U : random_seed;
    started.active = true;
    result = std::move(started);
    error.clear();
    return true;
}

bool begin_wild_battle(const RuleCatalog& rules,
                       const BattleRuleCatalog& battle_rules,
                       const PartyState& player_party,
                       const WildEncounterResult& encounter,
                       std::uint32_t random_seed, BattleState& result,
                       std::string& error) {
    const StatFormulaProgram* stats = find_stat_formula(
        battle_rules, battle_rules.original_stat_formula);
    if (!encounter.occurred || encounter.species_dex == 0U ||
        encounter.level == 0U || stats == nullptr) {
        error = "wild battle cannot begin from an incomplete encounter";
        return false;
    }

    // The host seed materializes per-instance DVs before the battle begins.
    // Species, level, initial moves, and stat behavior remain imported content.
    std::uint32_t state = random_seed == 0U ? 1U : random_seed;
    const std::uint8_t first = next_random_byte(state);
    const std::uint8_t second = next_random_byte(state);
    const std::array<std::uint8_t, 4> dvs{
        static_cast<std::uint8_t>(first >> 4U),
        static_cast<std::uint8_t>(first & 0x0FU),
        static_cast<std::uint8_t>(second >> 4U),
        static_cast<std::uint8_t>(second & 0x0FU),
    };
    PokemonState wild;
    if (!build_pokemon(rules, *stats, encounter.species_dex,
                       encounter.level, dvs, 0U, {}, wild, error)) {
        return false;
    }
    PartyState enemy;
    enemy.members.push_back(std::move(wild));
    return begin_battle(rules, battle_rules, player_party, std::move(enemy),
                        BattleKind::wild, state, result, error);
}

bool begin_trainer_battle(const RuleCatalog& rules,
                          const BattleRuleCatalog& battle_rules,
                          const PartyState& player_party,
                          const TrainerPartyRule& trainer_party,
                          std::uint32_t random_seed, BattleState& result,
                          std::string& error) {
    const StatFormulaProgram* stats = find_stat_formula(
        battle_rules, battle_rules.original_stat_formula);
    if (trainer_party.members.empty() ||
        trainer_party.members.size() > 6U || stats == nullptr) {
        error = "trainer battle cannot begin from an incomplete party";
        return false;
    }
    std::uint32_t state = random_seed == 0U ? 1U : random_seed;
    PartyState enemy;
    enemy.members.reserve(trainer_party.members.size());
    for (const TrainerPartyMember& member : trainer_party.members) {
        const std::uint8_t first = next_random_byte(state);
        const std::uint8_t second = next_random_byte(state);
        PokemonState pokemon;
        if (!build_pokemon(
                rules, *stats, member.species_dex, member.level,
                {
                    static_cast<std::uint8_t>(first >> 4U),
                    static_cast<std::uint8_t>(first & 0x0FU),
                    static_cast<std::uint8_t>(second >> 4U),
                    static_cast<std::uint8_t>(second & 0x0FU),
                },
                0U, {}, pokemon, error)) {
            return false;
        }
        enemy.members.push_back(std::move(pokemon));
    }
    return begin_battle(rules, battle_rules, player_party,
                        std::move(enemy), BattleKind::trainer, state,
                        result, error);
}

std::optional<std::size_t> first_executable_move_slot(
    const RuleCatalog& rules, const BattleRuleCatalog& battle_rules,
    const PokemonState& pokemon) {
    for (std::size_t index = 0; index < pokemon.moves.size(); ++index) {
        const PokemonMoveState& owned = pokemon.moves[index];
        const MoveRule* move = find_move(rules, owned.move_id);
        if (owned.pp != 0U && move != nullptr &&
            find_move_effect_program(battle_rules, move->effect_id) !=
                nullptr) {
            return index;
        }
    }
    return std::nullopt;
}

bool execute_battle_turn(const RuleCatalog& rules,
                         const BattleRuleCatalog& battle_rules,
                         std::uint16_t player_trainer_id,
                         PartyState& player_party, BattleState& battle,
                         const BattleTurnInput& input, std::string& error) {
    if (!battle.active || battle.phase != BattlePhase::choose_action ||
        battle.outcome != BattleOutcome::ongoing ||
        battle.player.active_index >= player_party.members.size() ||
        battle.enemy.active_index >= battle.enemy_party.members.size()) {
        error = "battle is not ready to resolve an action turn";
        return false;
    }

    const DamageFormulaProgram* damage = find_damage_formula(
        battle_rules, battle_rules.original_damage_formula);
    const CriticalHitProgram* critical = find_critical_hit_program(
        battle_rules, battle_rules.original_critical_hit_program);
    const AccuracyFormulaProgram* accuracy = find_accuracy_formula(
        battle_rules, battle_rules.original_accuracy_formula);
    const ExperienceFormulaProgram* experience = find_experience_formula(
        battle_rules, battle_rules.original_experience_formula);
    const StatFormulaProgram* stats = find_stat_formula(
        battle_rules, battle_rules.original_stat_formula);
    if (damage == nullptr || critical == nullptr || accuracy == nullptr ||
        experience == nullptr || stats == nullptr) {
        error = "battle ruleset has incomplete original formula bindings";
        return false;
    }

    // Resolve into copies so an unsupported imported effect or executor error
    // cannot leave a half-consumed PP/HP transaction in campaign state.
    PartyState player_work = player_party;
    BattleState battle_work = battle;
    battle_work.events.clear();
    battle_work.phase = BattlePhase::resolving;
    PokemonState& player_pokemon =
        player_work.members[battle_work.player.active_index];
    PokemonState& enemy_pokemon =
        battle_work.enemy_party.members[battle_work.enemy.active_index];
    SelectedMove player_move;
    SelectedMove enemy_move;
    if (!select_move(rules, battle_rules, player_pokemon,
                     input.player_move_slot, player_move, error) ||
        !select_move(rules, battle_rules, enemy_pokemon,
                     input.enemy_move_slot, enemy_move, error)) {
        return false;
    }

    const std::uint16_t player_speed =
        stage_stat(*accuracy, player_pokemon.stats.speed,
                   battle_work.player.stat_stages[2]);
    const std::uint16_t enemy_speed =
        stage_stat(*accuracy, enemy_pokemon.stats.speed,
                   battle_work.enemy.stat_stages[2]);
    bool player_first = player_speed > enemy_speed;
    if (player_speed == enemy_speed)
        player_first =
            (next_random_byte(battle_work.random_state) & 1U) == 0U;

    for (std::size_t action_index = 0; action_index < 2U;
         ++action_index) {
        const bool player_actor =
            action_index == 0U ? player_first : !player_first;
        PokemonState& attacker =
            player_actor ? player_pokemon : enemy_pokemon;
        PokemonState& defender =
            player_actor ? enemy_pokemon : player_pokemon;
        BattleSideState& attacker_side =
            player_actor ? battle_work.player : battle_work.enemy;
        BattleSideState& defender_side =
            player_actor ? battle_work.enemy : battle_work.player;
        const SelectedMove& selected =
            player_actor ? player_move : enemy_move;
        if (attacker.current_hp == 0U || defender.current_hp == 0U) break;

        ActionResult action;
        if (!execute_action(
                rules, *damage, *critical, *accuracy, attacker, defender,
                attacker_side, defender_side, player_actor, selected,
                battle_work, action, error)) {
            return false;
        }
        if (action.target_fainted) {
            if (!settle_faint(
                    rules, *experience, *stats, accuracy->neutral_stage,
                    player_trainer_id,
                    !player_actor, player_work, battle_work, error)) {
                return false;
            }
            break;
        }
    }
    if (battle_work.active &&
        battle_work.outcome == BattleOutcome::ongoing)
        battle_work.phase = BattlePhase::choose_action;
    ++battle_work.turn;
    player_party = std::move(player_work);
    battle = std::move(battle_work);
    error.clear();
    return true;
}

bool execute_enemy_battle_action(
    const RuleCatalog& rules,
    const BattleRuleCatalog& battle_rules,
    std::uint16_t player_trainer_id,
    PartyState& player_party, BattleState& battle,
    std::size_t enemy_move_slot, std::string& error) {
    if (!battle.active ||
        battle.phase != BattlePhase::choose_action ||
        battle.outcome != BattleOutcome::ongoing ||
        battle.player.active_index >= player_party.members.size() ||
        battle.enemy.active_index >= battle.enemy_party.members.size()) {
        error = "battle is not ready for an enemy-only action";
        return false;
    }
    const DamageFormulaProgram* damage = find_damage_formula(
        battle_rules, battle_rules.original_damage_formula);
    const CriticalHitProgram* critical = find_critical_hit_program(
        battle_rules, battle_rules.original_critical_hit_program);
    const AccuracyFormulaProgram* accuracy = find_accuracy_formula(
        battle_rules, battle_rules.original_accuracy_formula);
    const ExperienceFormulaProgram* experience =
        find_experience_formula(
            battle_rules,
            battle_rules.original_experience_formula);
    const StatFormulaProgram* stats = find_stat_formula(
        battle_rules, battle_rules.original_stat_formula);
    if (damage == nullptr || critical == nullptr ||
        accuracy == nullptr || experience == nullptr ||
        stats == nullptr) {
        error =
            "enemy-only action has incomplete original formula bindings";
        return false;
    }

    PartyState player_work = player_party;
    BattleState battle_work = battle;
    battle_work.events.clear();
    battle_work.phase = BattlePhase::resolving;
    PokemonState& player_pokemon =
        player_work.members[battle_work.player.active_index];
    PokemonState& enemy_pokemon =
        battle_work.enemy_party.members[battle_work.enemy.active_index];
    SelectedMove enemy_move;
    if (!select_move(
            rules, battle_rules, enemy_pokemon,
            enemy_move_slot, enemy_move, error))
        return false;

    ActionResult action;
    if (!execute_action(
            rules, *damage, *critical, *accuracy, enemy_pokemon,
            player_pokemon, battle_work.enemy, battle_work.player,
            false, enemy_move, battle_work, action, error))
        return false;
    if (action.target_fainted &&
        !settle_faint(
            rules, *experience, *stats, accuracy->neutral_stage,
            player_trainer_id, true, player_work, battle_work,
            error))
        return false;
    if (battle_work.active &&
        battle_work.outcome == BattleOutcome::ongoing)
        battle_work.phase = BattlePhase::choose_action;
    ++battle_work.turn;
    player_party = std::move(player_work);
    battle = std::move(battle_work);
    error.clear();
    return true;
}

bool switch_player_battler(
    const BattleRuleCatalog& battle_rules,
    const PartyState& player_party, std::size_t party_index,
    BattleState& battle, std::string& error) {
    const AccuracyFormulaProgram* accuracy = find_accuracy_formula(
        battle_rules, battle_rules.original_accuracy_formula);
    if (!battle.active ||
        battle.phase != BattlePhase::choose_action ||
        battle.outcome != BattleOutcome::ongoing ||
        party_index >= player_party.members.size() ||
        party_index == battle.player.active_index ||
        player_party.members[party_index].current_hp == 0U ||
        accuracy == nullptr) {
        error = "battle cannot switch to the requested party member";
        return false;
    }
    battle.player.active_index = party_index;
    reset_stages(battle.player, accuracy->neutral_stage);
    battle.events.clear();
    battle.events.push_back({
        .kind = BattleEventKind::sent_out,
        .player_actor = true,
        .text = player_party.members[party_index].nickname,
    });
    error.clear();
    return true;
}

bool attempt_battle_capture(
    const RuleCatalog& rules,
    const BattleRuleCatalog& battle_rules,
    std::size_t ball_profile, BattleState& battle,
    BattleCaptureAttempt& result, std::string& error) {
    result = {};
    if (!battle.active ||
        battle.phase != BattlePhase::choose_action ||
        battle.outcome != BattleOutcome::ongoing ||
        battle.kind != BattleKind::wild ||
        battle.enemy.active_index >=
            battle.enemy_party.members.size()) {
        error = "capture requires an active wild battle";
        return false;
    }
    const CaptureFormulaProgram* capture = find_capture_formula(
        battle_rules, battle_rules.original_capture_formula);
    const PokemonState& target =
        battle.enemy_party.members[battle.enemy.active_index];
    const SpeciesRule* species =
        find_species(rules, target.species_dex);
    if (capture == nullptr || species == nullptr ||
        ball_profile >= capture->ball_profiles.size()) {
        error = "capture lost its imported formula, species, or ball profile";
        return false;
    }
    std::size_t status_profile = 0U;
    if (target.status == MajorStatus::burn ||
        target.status == MajorStatus::paralysis ||
        target.status == MajorStatus::poison)
        status_profile = 1U;
    else if (target.status == MajorStatus::freeze ||
             target.status == MajorStatus::sleep)
        status_profile = 2U;
    if (status_profile >= capture->status_profiles.size()) {
        error = "capture status has no imported modifier profile";
        return false;
    }

    const std::array<std::uint8_t, 512> random =
        preview_random(battle.random_state);
    CaptureFormulaResult formula;
    if (!execute_capture_formula(
            *capture,
            {
                .ball_profile = static_cast<std::uint16_t>(
                    ball_profile),
                .status_profile = static_cast<std::uint16_t>(
                    status_profile),
                .catch_rate = species->catch_rate,
                .current_hp = target.current_hp,
                .maximum_hp = target.stats.hp,
            },
            random, formula, error))
        return false;
    consume_random(
        battle.random_state, formula.random_bytes_consumed);
    result = {
        .species_dex = target.species_dex,
        .shakes = formula.shakes,
        .caught = formula.caught,
    };
    error.clear();
    return true;
}

} // namespace pokered
