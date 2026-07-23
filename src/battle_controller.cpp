#include "battle_controller.hpp"

#include "battle.hpp"
#include "battle_animation_lab.hpp"
#include "battle_rules.hpp"
#include "battle_ui.hpp"
#include "battle_view.hpp"
#include "encounters.hpp"
#include "interactions.hpp"
#include "maps.hpp"
#include "pokemon.hpp"
#include "rules.hpp"
#include "state.hpp"
#include "trainers.hpp"

#include <algorithm>
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

const WorldActorSpawn* find_actor_spawn(
    const WorldState& world, std::uint8_t map_id,
    std::uint8_t actor_index) {
    const auto map = std::ranges::find_if(
        world.maps, [map_id](const WorldMap& candidate) {
            return candidate.id == map_id;
        });
    if (map == world.maps.end()) return nullptr;
    const auto actor = std::ranges::find_if(
        map->actors, [actor_index](const WorldActorSpawn& candidate) {
            return candidate.index == actor_index;
        });
    return actor == map->actors.end() ? nullptr : &*actor;
}

bool begin_actor_battle(
    const TrainerCatalog& trainers, WorldState& world,
    const RuleCatalog& rules,
    const BattleRuleCatalog& battle_rules,
    CampaignState& campaign, BattleAnimationLab& view,
    const TrainerInteractionRule& interaction,
    const WorldActorSpawn& actor, std::string& error) {
    ActorOpponentBinding binding;
    if (!resolve_actor_opponent(trainers, actor.parameter_a,
                                actor.parameter_b, binding, error))
        return false;

    bool started = false;
    if (binding.kind == ActorOpponentKind::trainer) {
        const TrainerPartyRule* party = find_trainer_party(
            trainers, binding.trainer_class_id,
            binding.trainer_party_index);
        started =
            party != nullptr &&
            begin_trainer_battle(
                rules, battle_rules, campaign.party, *party,
                world.random_state, campaign.battle, error);
    } else {
        const WildEncounterResult encounter{
            .occurred = true,
            .level = binding.level,
            .species_dex = binding.species_dex,
        };
        started = begin_wild_battle(
            rules, battle_rules, campaign.party, encounter,
            world.random_state, campaign.battle, error);
    }
    if (!started) {
        if (error.empty())
            error = "world actor has no executable opponent party";
        return false;
    }

    view.ui.mode = BattleUiMode::command;
    prepare_battle_view(view);
    if (!sync_battle_view(rules, battle_rules, campaign.party,
                          campaign.battle, view, error)) {
        campaign.battle = {};
        return false;
    }
    campaign.battle_owner = {
        .map_id = world.opponent_request.map_id,
        .actor_index = world.opponent_request.actor_index,
        .defeated_flag = interaction.defeated_flag,
        .active = true,
    };
    world.opponent_request = {};
    world.dialogue = {};
    return true;
}

} // namespace

bool begin_world_wild_battle(
    const EncounterCatalog& encounters, WorldState& world,
    const RuleCatalog& rules, const BattleRuleCatalog& battle_rules,
    CampaignState& campaign, BattleAnimationLab& view, bool& began,
    std::string& error) {
    began = false;
    if (!world.player_completed_step ||
        world.trainer_approach.active ||
        world.opponent_request.pending ||
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

bool service_world_actor_battle(
    const InteractionCatalog& interactions,
    const TrainerCatalog& trainers, WorldState& world,
    const RuleCatalog& rules,
    const BattleRuleCatalog& battle_rules,
    CampaignState& campaign, BattleAnimationLab& view, bool& began,
    std::string& error) {
    began = false;

    // A fresh activation selects imported before/after behavior and queues an
    // undefeated opponent until its dialogue pages have been acknowledged.
    if (world.last_actor_activation.occurred) {
        const std::uint8_t map_id =
            world.last_actor_activation.map_id;
        const std::uint8_t actor_index =
            world.last_actor_activation.actor_index;
        const TrainerInteractionRule* interaction =
            find_trainer_interaction(interactions, map_id, actor_index);
        const WorldActorSpawn* actor =
            find_actor_spawn(world, map_id, actor_index);
        if (interaction != nullptr && actor != nullptr &&
            actor->kind == WorldActorKind::trainer_or_pokemon) {
            if (campaign_flag(campaign,
                              interaction->defeated_flag)) {
                world.opponent_request = {};
                open_world_dialogue(world, campaign,
                                    interaction->after_pages);
            } else if (party_has_usable_pokemon(campaign.party)) {
                world.opponent_request = {
                    .map_id = map_id,
                    .actor_index = actor_index,
                    .pending = true,
                };
                open_world_dialogue(world, campaign,
                                    interaction->before_pages);
            }
        }
    }

    if (!world.opponent_request.pending || world.dialogue.open) {
        error.clear();
        return true;
    }
    const TrainerInteractionRule* interaction =
        find_trainer_interaction(
            interactions, world.opponent_request.map_id,
            world.opponent_request.actor_index);
    const WorldActorSpawn* actor = find_actor_spawn(
        world, world.opponent_request.map_id,
        world.opponent_request.actor_index);
    if (interaction == nullptr || actor == nullptr) {
        world.opponent_request = {};
        error = "pending world opponent lost its imported owner";
        return false;
    }
    if (!begin_actor_battle(trainers, world, rules, battle_rules,
                            campaign, view, *interaction, *actor,
                            error))
        return false;
    began = true;
    error.clear();
    return true;
}

void finish_world_actor_battle(
    const InteractionCatalog& interactions, WorldState& world,
    CampaignState& campaign) {
    if (!campaign.battle_owner.active) return;
    const TrainerInteractionRule* interaction =
        find_trainer_interaction(
            interactions, campaign.battle_owner.map_id,
            campaign.battle_owner.actor_index);
    if (campaign.battle.outcome ==
        BattleOutcome::player_victory) {
        set_campaign_flag(campaign,
                          campaign.battle_owner.defeated_flag, true);
        if (interaction != nullptr)
            open_world_dialogue(world, campaign,
                                interaction->end_pages);
    }
    campaign.battle_owner = {};
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
