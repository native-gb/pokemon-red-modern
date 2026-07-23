#include "battle_view.hpp"

#include "battle.hpp"
#include "battle_animation_lab.hpp"
#include "battle_rules.hpp"
#include "battle_ui.hpp"
#include "rules.hpp"

#include <array>
#include <cstddef>
#include <string>

namespace pokered {
namespace {

Symbol status_symbol(MajorStatus status) {
    switch (status) {
    case MajorStatus::none:
        return Symbol{"none"};
    case MajorStatus::sleep:
        return Symbol{"sleep"};
    case MajorStatus::poison:
        return Symbol{"poison"};
    case MajorStatus::burn:
        return Symbol{"burn"};
    case MajorStatus::freeze:
        return Symbol{"freeze"};
    case MajorStatus::paralysis:
        return Symbol{"paralysis"};
    }
    return Symbol{"none"};
}

BattleHudState hud(const SpeciesRule& species,
                   const PokemonState& pokemon) {
    return {
        .name = pokemon.nickname.empty() ? species.name : pokemon.nickname,
        .level = pokemon.level,
        .current_hp = pokemon.current_hp,
        .maximum_hp = pokemon.stats.hp,
        .status = status_symbol(pokemon.status),
        .visible = true,
    };
}

bool fill_moves(const RuleCatalog& rules,
                const BattleRuleCatalog& battle_rules,
                const PokemonState& pokemon,
                std::array<BattleMoveOption, 4>& result,
                std::string& error) {
    for (std::size_t index = 0; index < pokemon.moves.size(); ++index) {
        const PokemonMoveState& owned = pokemon.moves[index];
        if (owned.move_id == 0U) {
            result[index] = {
                .name = {},
                .type = {},
                .current_pp = 0U,
                .maximum_pp = 0U,
                .enabled = false,
            };
            continue;
        }
        const MoveRule* move = find_move(rules, owned.move_id);
        const TypeRule* type =
            move == nullptr ? nullptr : find_type(rules, move->type_id);
        if (move == nullptr || type == nullptr) {
            error = "battle view references an absent move or type";
            return false;
        }
        result[index] = {
            .name = move->name,
            .type = type->name,
            .current_pp = owned.pp,
            .maximum_pp = owned.maximum_pp,
            .enabled =
                owned.pp != 0U &&
                find_move_effect_program(battle_rules, move->effect_id) !=
                    nullptr,
        };
    }
    return true;
}

} // namespace

bool sync_battle_view(const RuleCatalog& rules,
                      const BattleRuleCatalog& battle_rules,
                      const PartyState& player_party,
                      const BattleState& battle,
                      BattleAnimationLab& view, std::string& error) {
    if (!rules.loaded || !battle_rules.loaded || !view.loaded ||
        !battle.active ||
        battle.player.active_index >= player_party.members.size() ||
        battle.enemy.active_index >= battle.enemy_party.members.size()) {
        error = "battle view cannot bind incomplete battle state";
        return false;
    }
    const PokemonState& player =
        player_party.members[battle.player.active_index];
    const PokemonState& enemy =
        battle.enemy_party.members[battle.enemy.active_index];
    const SpeciesRule* player_species =
        find_species(rules, player.species_dex);
    const SpeciesRule* enemy_species =
        find_species(rules, enemy.species_dex);
    if (player_species == nullptr || enemy_species == nullptr ||
        player.species_dex > view.imported_assets.pokemon.size() ||
        enemy.species_dex > view.imported_assets.pokemon.size()) {
        error = "battle view cannot resolve imported Pokemon pictures";
        return false;
    }

    view.distinct_battlers = true;
    view.player_species =
        static_cast<std::size_t>(player.species_dex - 1U);
    view.enemy_species =
        static_cast<std::size_t>(enemy.species_dex - 1U);
    view.ui.player = hud(*player_species, player);
    view.ui.enemy = hud(*enemy_species, enemy);
    if (!fill_moves(rules, battle_rules, player, view.ui.moves, error))
        return false;
    if (view.ui.selected_move >= view.ui.moves.size() ||
        !view.ui.moves[view.ui.selected_move].enabled) {
        view.ui.selected_move = 0U;
    }
    if (!compose_battle_ui(view.ui, view.ui_tile_map, error))
        return false;
    error.clear();
    return true;
}

} // namespace pokered
