#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace pokered {

struct BattleRuleCatalog;
struct ExperienceFormulaProgram;
struct RuleCatalog;
struct StatFormulaProgram;

enum class MajorStatus : std::uint8_t {
    none,
    sleep,
    poison,
    burn,
    freeze,
    paralysis,
};

struct PokemonStats {
    std::uint16_t hp{};
    std::uint16_t attack{};
    std::uint16_t defense{};
    std::uint16_t speed{};
    std::uint16_t special{};
};

struct PokemonMoveState {
    std::uint8_t move_id{};
    std::uint8_t pp{};
    std::uint8_t maximum_pp{};
    std::uint8_t pp_ups{};
};

struct PokemonState {
    std::uint8_t species_dex{};
    std::uint8_t level{};
    std::uint32_t experience{};
    // Attack, Defense, Speed, and Special determinant values.
    std::array<std::uint8_t, 4> dvs{};
    std::array<std::uint16_t, 5> stat_experience{};
    std::array<PokemonMoveState, 4> moves{};
    PokemonStats stats;
    std::uint16_t current_hp{};
    MajorStatus status{MajorStatus::none};
    std::uint8_t sleep_turns{};
    std::uint16_t trainer_id{};
    std::string original_trainer;
    std::string nickname;
};

struct PartyState {
    std::vector<PokemonState> members;
};

struct ExperienceAwardResult {
    std::uint32_t experience_gained{};
    std::uint8_t old_level{};
    std::uint8_t new_level{};
    std::vector<std::uint8_t> learned_moves;
    std::vector<std::uint8_t> pending_moves;
    std::uint8_t pending_evolution_species{};
};

bool build_pokemon(const RuleCatalog& rules, const StatFormulaProgram& stat_formula,
                   std::uint8_t species_dex, std::uint8_t level,
                   const std::array<std::uint8_t, 4>& dvs, std::uint16_t trainer_id,
                   std::string original_trainer, PokemonState& result, std::string& error);
bool recalculate_pokemon_stats(const RuleCatalog& rules, const StatFormulaProgram& stat_formula,
                               PokemonState& pokemon, std::string& error);
bool award_pokemon_experience(const RuleCatalog& rules,
                              const ExperienceFormulaProgram& experience_formula,
                              const StatFormulaProgram& stat_formula, const PokemonState& defeated,
                              bool trainer_battle, std::uint16_t player_trainer_id,
                              std::uint8_t participant_divisor, PokemonState& recipient,
                              ExperienceAwardResult& result, std::string& error,
                              std::uint8_t base_value_divisor = 1U);
bool party_has_usable_pokemon(const PartyState& party);
void heal_party(PartyState& party);

} // namespace pokered
