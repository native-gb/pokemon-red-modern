#include "battle_controller.hpp"

#include "battle.hpp"
#include "battle_animation_lab.hpp"
#include "battle_rules.hpp"
#include "battle_ui.hpp"
#include "battle_view.hpp"
#include "encounters.hpp"
#include "maps.hpp"
#include "pokemon.hpp"
#include "rules.hpp"
#include "state.hpp"

#include <cstddef>
#include <optional>

namespace pokered {
namespace {

std::uint8_t leading_level(const PartyState& party) {
    for (const PokemonState& pokemon : party.members)
        if (pokemon.current_hp != 0U) return pokemon.level;
    return 0U;
}

bool recompose(BattleAnimationLab& view, std::string& error) {
    return compose_battle_ui(view.ui, view.ui_tile_map, error);
}

} // namespace

bool begin_world_wild_battle(
    const EncounterCatalog& encounters, WorldState& world,
    const RuleCatalog& rules, const BattleRuleCatalog& battle_rules,
    CampaignState& campaign, BattleAnimationLab& view, bool& began,
    std::string& error) {
    began = false;
    if (!world.player_completed_step ||
        !party_has_usable_pokemon(campaign.party)) {
        error.clear();
        return true;
    }

    WildEncounterResult encounter;
    const std::uint8_t chance_roll = next_world_random_byte(world);
    const std::uint8_t slot_roll = next_world_random_byte(world);
    if (!resolve_world_wild_encounter(
            encounters, world, chance_roll, slot_roll, false,
            leading_level(campaign.party), encounter, error)) {
        return false;
    }
    if (!encounter.occurred) {
        error.clear();
        return true;
    }
    if (!begin_wild_battle(rules, battle_rules, campaign.party, encounter,
                           world.random_state, campaign.battle, error)) {
        return false;
    }
    view.ui.mode = BattleUiMode::command;
    prepare_battle_view(view);
    if (!sync_battle_view(rules, battle_rules, campaign.party,
                          campaign.battle, view, error)) {
        campaign.battle = {};
        return false;
    }
    began = true;
    error.clear();
    return true;
}

bool control_battle(const RuleCatalog& rules,
                    const BattleRuleCatalog& battle_rules,
                    CampaignState& campaign, BattleAnimationLab& view,
                    const BattleControlInput& input,
                    BattleControlResult& result, std::string& error) {
    result = {};
    if (!campaign.battle.active) {
        error = "battle controls require an active owned battle";
        return false;
    }
    if (input.previous) previous_battle_ui_menu_selection(view);
    if (input.next) next_battle_ui_menu_selection(view);
    if (input.back && view.ui.mode == BattleUiMode::moves) {
        view.ui.mode = BattleUiMode::command;
        return recompose(view, error);
    }
    if (!input.confirm) {
        error.clear();
        return true;
    }

    if (view.ui.mode == BattleUiMode::command) {
        const BattleCommandMenu& commands =
            view.ui.definition.standard_commands;
        if (commands.selected < commands.slots.size() &&
            commands.slots[commands.selected].on_select.text ==
                "battle_choose_move") {
            view.ui.mode = BattleUiMode::moves;
            return recompose(view, error);
        }
        error.clear();
        return true;
    }
    if (view.ui.mode != BattleUiMode::moves ||
        campaign.battle.enemy.active_index >=
            campaign.battle.enemy_party.members.size()) {
        error.clear();
        return true;
    }

    const PokemonState& enemy = campaign.battle.enemy_party.members[
        campaign.battle.enemy.active_index];
    const std::optional<std::size_t> enemy_move =
        first_executable_move_slot(rules, battle_rules, enemy);
    const std::size_t player_move = view.ui.selected_move;
    if (!enemy_move.has_value() || player_move >= view.ui.moves.size() ||
        !view.ui.moves[player_move].enabled) {
        error.clear();
        return true;
    }
    if (!execute_battle_turn(
            rules, battle_rules, campaign.trainer_id, campaign.party,
            campaign.battle,
            {
                .player_move_slot = player_move,
                .enemy_move_slot = *enemy_move,
            },
            error)) {
        return false;
    }
    if (!campaign.battle.active) {
        view.distinct_battlers = false;
        result.finished = true;
        error.clear();
        return true;
    }

    view.ui.mode = BattleUiMode::command;
    return sync_battle_view(rules, battle_rules, campaign.party,
                            campaign.battle, view, error);
}

} // namespace pokered
