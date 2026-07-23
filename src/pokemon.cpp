#include "pokemon.hpp"

#include "battle_rules.hpp"
#include "rules.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace pokered {
namespace {

std::array<std::uint8_t, 5> base_stats(const SpeciesRule& species) {
    return {
        species.base_hp,    species.base_attack,  species.base_defense,
        species.base_speed, species.base_special,
    };
}

void assign_stats(PokemonState& pokemon, const StatFormulaResult& calculated) {
    pokemon.stats = {
        .hp = calculated.stats[0],
        .attack = calculated.stats[1],
        .defense = calculated.stats[2],
        .speed = calculated.stats[3],
        .special = calculated.stats[4],
    };
}

bool knows_move(const PokemonState& pokemon, std::uint8_t move_id) {
    return std::ranges::any_of(
        pokemon.moves, [move_id](const PokemonMoveState& move) { return move.move_id == move_id; });
}

bool fill_empty_move(const RuleCatalog& rules, PokemonState& pokemon, std::uint8_t move_id) {
    if (move_id == 0U || knows_move(pokemon, move_id)) return true;
    const MoveRule* move = find_move(rules, move_id);
    if (move == nullptr) return false;
    const auto empty = std::ranges::find_if(
        pokemon.moves, [](const PokemonMoveState& value) { return value.move_id == 0U; });
    if (empty == pokemon.moves.end()) return false;
    *empty = {
        .move_id = move_id,
        .pp = move->pp,
        .maximum_pp = move->pp,
    };
    return true;
}

bool append_initial_move(const RuleCatalog& rules, PokemonState& pokemon, std::uint8_t move_id) {
    if (move_id == 0U || knows_move(pokemon, move_id)) return true;
    if (fill_empty_move(rules, pokemon, move_id)) return true;
    const MoveRule* move = find_move(rules, move_id);
    if (move == nullptr) return false;
    std::move(pokemon.moves.begin() + 1, pokemon.moves.end(), pokemon.moves.begin());
    pokemon.moves.back() = {
        .move_id = move_id,
        .pp = move->pp,
        .maximum_pp = move->pp,
    };
    return true;
}

} // namespace

bool recalculate_pokemon_stats(const RuleCatalog& rules, const StatFormulaProgram& stat_formula,
                               PokemonState& pokemon, std::string& error) {
    const SpeciesRule* species = find_species(rules, pokemon.species_dex);
    if (species == nullptr || pokemon.level == 0U || pokemon.level > 100U) {
        error = "owned Pokemon has an invalid species or level";
        return false;
    }
    StatFormulaResult calculated;
    if (!execute_stat_formula(stat_formula,
                              {
                                  .base_stats = base_stats(*species),
                                  .dvs = pokemon.dvs,
                                  .stat_experience = pokemon.stat_experience,
                                  .level = pokemon.level,
                              },
                              calculated, error)) {
        return false;
    }
    assign_stats(pokemon, calculated);
    error.clear();
    return true;
}

bool build_pokemon(const RuleCatalog& rules, const StatFormulaProgram& stat_formula,
                   std::uint8_t species_dex, std::uint8_t level,
                   const std::array<std::uint8_t, 4>& dvs, std::uint16_t trainer_id,
                   std::string original_trainer, PokemonState& result, std::string& error) {
    const SpeciesRule* species = find_species(rules, species_dex);
    if (species == nullptr || level == 0U || level > 100U ||
        std::ranges::any_of(dvs, [](std::uint8_t dv) { return dv > 15U; })) {
        error = "Pokemon creation received an invalid species, level, or DV";
        return false;
    }

    PokemonState built;
    built.species_dex = species_dex;
    built.level = level;
    built.experience = experience_for_level(rules, species->growth_curve_id, level);
    built.dvs = dvs;
    built.trainer_id = trainer_id;
    built.original_trainer = std::move(original_trainer);
    built.nickname = species->name;

    // Starting moves and ordered level-up records are content. The engine only
    // applies the four-slot acquisition policy while constructing an instance.
    for (std::uint8_t move_id : species->starting_move_ids)
        if (!append_initial_move(rules, built, move_id)) {
            error = "Pokemon creation references an invalid starting move";
            return false;
        }
    for (const LearnsetRule& learned : rules.learnsets)
        if (learned.species_dex == species_dex && learned.level <= level &&
            !append_initial_move(rules, built, learned.move_id)) {
            error = "Pokemon creation references an invalid learned move";
            return false;
        }

    if (!recalculate_pokemon_stats(rules, stat_formula, built, error)) return false;
    built.current_hp = built.stats.hp;
    result = std::move(built);
    error.clear();
    return true;
}

bool award_pokemon_experience(const RuleCatalog& rules,
                              const ExperienceFormulaProgram& experience_formula,
                              const StatFormulaProgram& stat_formula, const PokemonState& defeated,
                              bool trainer_battle, std::uint16_t player_trainer_id,
                              std::uint8_t participant_divisor, PokemonState& recipient,
                              ExperienceAwardResult& result, std::string& error,
                              std::uint8_t base_value_divisor) {
    result = {};
    const SpeciesRule* defeated_species = find_species(rules, defeated.species_dex);
    const SpeciesRule* recipient_species = find_species(rules, recipient.species_dex);
    if (defeated_species == nullptr || recipient_species == nullptr || defeated.level == 0U ||
        recipient.level == 0U || participant_divisor == 0U || base_value_divisor == 0U) {
        error = "experience award references invalid owned Pokemon";
        return false;
    }

    ExperienceFormulaResult award;
    if (!execute_experience_formula(experience_formula,
                                    {
                                        .base_experience = defeated_species->experience_yield,
                                        .base_stats = base_stats(*defeated_species),
                                        .defeated_level = defeated.level,
                                        .base_value_divisor = base_value_divisor,
                                        .participant_divisor = participant_divisor,
                                        .traded = recipient.trainer_id != player_trainer_id,
                                        .trainer_battle = trainer_battle,
                                    },
                                    award, error)) {
        return false;
    }

    result.old_level = recipient.level;
    result.experience_gained = award.experience;
    for (std::size_t index = 0; index < recipient.stat_experience.size(); ++index) {
        const std::uint32_t updated = static_cast<std::uint32_t>(recipient.stat_experience[index]) +
                                      award.stat_experience[index];
        recipient.stat_experience[index] = static_cast<std::uint16_t>(
            std::min<std::uint32_t>(updated, std::numeric_limits<std::uint16_t>::max()));
    }
    const std::uint32_t maximum_experience =
        experience_for_level(rules, recipient_species->growth_curve_id, 100U);
    recipient.experience = std::min(maximum_experience, recipient.experience + award.experience);
    result.new_level =
        level_for_experience(rules, recipient_species->growth_curve_id, recipient.experience);
    if (result.new_level <= result.old_level) {
        error.clear();
        return true;
    }

    const std::uint16_t old_maximum_hp = recipient.stats.hp;
    recipient.level = result.new_level;
    if (!recalculate_pokemon_stats(rules, stat_formula, recipient, error)) {
        return false;
    }
    const std::uint32_t hp_increase =
        recipient.stats.hp > old_maximum_hp ? recipient.stats.hp - old_maximum_hp : 0U;
    recipient.current_hp = static_cast<std::uint16_t>(
        std::min<std::uint32_t>(recipient.stats.hp, recipient.current_hp + hp_increase));

    for (std::uint8_t move_id :
         moves_learned_at_level(rules, recipient.species_dex, result.new_level)) {
        if (knows_move(recipient, move_id)) continue;
        if (fill_empty_move(rules, recipient, move_id))
            result.learned_moves.push_back(move_id);
        else
            result.pending_moves.push_back(move_id);
    }
    const EvolutionRule* evolution =
        eligible_evolution(rules, recipient.species_dex, result.new_level, std::nullopt, false);
    if (evolution != nullptr) result.pending_evolution_species = evolution->target_species_dex;
    error.clear();
    return true;
}

bool party_has_usable_pokemon(const PartyState& party) {
    return std::ranges::any_of(
        party.members, [](const PokemonState& pokemon) { return pokemon.current_hp != 0U; });
}

void heal_party(PartyState& party) {
    for (PokemonState& pokemon : party.members) {
        pokemon.current_hp = pokemon.stats.hp;
        pokemon.status = MajorStatus::none;
        pokemon.sleep_turns = 0U;
        for (PokemonMoveState& move : pokemon.moves)
            move.pp = move.maximum_pp;
    }
}

} // namespace pokered
