#include "battle_controller.hpp"

#include "battle.hpp"
#include "battle_animation_lab.hpp"
#include "battle_rules.hpp"
#include "battle_ui.hpp"
#include "battle_view.hpp"
#include "campaign_programs.hpp"
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
#include <utility>
#include <vector>

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

bool begin_event_animations(const RuleCatalog& rules,
                            const BattleState& battle,
                            BattleAnimationLab& view,
                            std::string& error) {
    std::vector<GameplayBattleAnimation> animations;
    for (const BattleEvent& event : battle.events) {
        if (event.kind != BattleEventKind::used_move) continue;
        const MoveRule* move = find_move(rules, event.move_id);
        if (move == nullptr || move->animation_id == 0U) continue;
        animations.push_back({
            .animation_id = move->animation_id,
            .enemy_turn = !event.player_actor,
        });
    }
    return begin_gameplay_battle_animations(
        view, std::move(animations), error);
}

bool show_battle_message(
    BattleAnimationLab& view, std::string first, std::string second,
    bool finish_after, bool return_to_command,
    std::string& error) {
    view.ui.message_lines = {
        std::move(first),
        std::move(second),
    };
    view.ui.mode = BattleUiMode::message;
    view.finish_after_message = finish_after;
    view.return_to_command_after_message = return_to_command;
    return recompose(view, error);
}

bool has_capture_destination(
    const CampaignProgramCatalog& programs,
    const CampaignState& campaign) {
    if (campaign.party.members.size() < programs.party_capacity)
        return true;
    return campaign.storage.current_box <
               campaign.storage.boxes.size() &&
           campaign.storage.boxes[campaign.storage.current_box].size() <
               campaign.storage.box_capacity;
}

bool store_captured_pokemon(
    const CampaignProgramCatalog& programs,
    CampaignState& campaign, PokemonState pokemon) {
    pokemon.trainer_id = campaign.trainer_id;
    pokemon.original_trainer = campaign.player_name;
    if (campaign.party.members.size() < programs.party_capacity) {
        campaign.party.members.push_back(std::move(pokemon));
        return true;
    }
    if (campaign.storage.current_box >=
            campaign.storage.boxes.size() ||
        campaign.storage.boxes[campaign.storage.current_box].size() >=
            campaign.storage.box_capacity)
        return false;
    campaign.storage.boxes[campaign.storage.current_box].push_back(
        std::move(pokemon));
    return true;
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
    begin_battle_presentation(
        view, binding.kind == ActorOpponentKind::trainer);
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
    const EncounterCatalog& encounters,
    const CampaignProgramCatalog& programs, WorldState& world,
    const RuleCatalog& rules, const BattleRuleCatalog& battle_rules,
    CampaignState& campaign, BattleAnimationLab& view, bool& began,
    std::string& error) {
    began = false;
    if (!world.player_completed_step ||
        world.trainer_approach.active ||
        world.opponent_request.pending ||
        campaign_suppresses_wild_encounters(
            programs, campaign, world) ||
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
    begin_battle_presentation(view, false);
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

bool begin_campaign_trainer_battle(
    const TrainerCatalog& trainers, WorldState& world,
    const RuleCatalog& rules,
    const BattleRuleCatalog& battle_rules,
    CampaignState& campaign, BattleAnimationLab& view, bool& began,
    std::string& error) {
    began = false;
    if (!campaign.trainer_battle_request.pending) {
        error.clear();
        return true;
    }
    if (campaign.battle.active) {
        error = "campaign trainer battle overlaps an active battle";
        return false;
    }
    const TrainerPartyRule* party = find_trainer_party(
        trainers, campaign.trainer_battle_request.trainer_class_id,
        campaign.trainer_battle_request.trainer_party_index);
    if (party == nullptr) {
        error =
            "campaign trainer battle references an unavailable party";
        return false;
    }
    if (!begin_trainer_battle(
            rules, battle_rules, campaign.party, *party,
            world.random_state, campaign.battle, error))
        return false;

    view.ui.mode = BattleUiMode::command;
    prepare_battle_view(view);
    if (!sync_battle_view(rules, battle_rules, campaign.party,
                          campaign.battle, view, error)) {
        campaign.battle = {};
        return false;
    }
    begin_battle_presentation(view, true);
    campaign.trainer_battle_request = {};
    campaign.battle_owner = {};
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
                    const CampaignProgramCatalog& programs,
                    CampaignState& campaign, BattleAnimationLab& view,
                    const BattleControlInput& input,
                    BattleControlResult& result, std::string& error) {
    result = {};
    if (!campaign.battle.active) {
        error = "battle controls require an active owned battle";
        return false;
    }
    if (!battle_accepts_input(view)) {
        error.clear();
        return true;
    }
    if (view.gameplay_animation_active) {
        error.clear();
        return true;
    }
    if (view.ui.mode == BattleUiMode::message) {
        if (!input.confirm) {
            error.clear();
            return true;
        }
        if (view.finish_after_message) {
            campaign.battle.active = false;
            view.distinct_battlers = false;
            view.finish_after_message = false;
            view.return_to_command_after_message = false;
            result.finished = true;
            error.clear();
            return true;
        }
        if (view.return_to_command_after_message) {
            view.return_to_command_after_message = false;
            view.ui.mode = BattleUiMode::command;
            return sync_battle_view(
                rules, battle_rules, campaign.party,
                campaign.battle, view, error);
        }
        error.clear();
        return true;
    }
    if (input.left || input.right || input.up || input.down) {
        move_battle_ui_selection(
            view.ui, input.left ? -1 : input.right ? 1 : 0,
            input.up ? -1 : input.down ? 1 : 0);
        if (!recompose(view, error)) return false;
    }
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
        if (commands.selected >= commands.slots.size()) {
            error.clear();
            return true;
        }
        const std::string& action =
            commands.slots[commands.selected].on_select.text;
        if (action == "battle_choose_move") {
            view.ui.mode = BattleUiMode::moves;
            return recompose(view, error);
        }
        if (action == "battle_attempt_escape") {
            if (campaign.battle.kind == BattleKind::trainer)
                return show_battle_message(
                    view, "No! There's no", "running from a trainer!",
                    false, true, error);
            campaign.battle.outcome = BattleOutcome::escaped;
            campaign.battle.phase = BattlePhase::finished;
            return show_battle_message(
                view, "Got away safely!", {}, true, false, error);
        }
        if (action == "battle_choose_item") {
            if (campaign.battle.kind != BattleKind::wild)
                return show_battle_message(
                    view, "The Trainer blocked", "the BALL!",
                    false, true, error);
            const CaptureFormulaProgram* capture =
                find_capture_formula(
                    battle_rules,
                    battle_rules.original_capture_formula);
            if (capture == nullptr) {
                error =
                    "battle item command lost its imported capture formula";
                return false;
            }
            std::optional<std::size_t> selected_profile;
            for (std::size_t profile = 0U;
                 profile < capture->ball_profiles.size(); ++profile) {
                if (inventory_item_quantity(
                        campaign.inventory,
                        capture->ball_profiles[profile].item_id) != 0U) {
                    selected_profile = profile;
                    break;
                }
            }
            if (!selected_profile.has_value())
                return show_battle_message(
                    view, "There is no usable", "BALL in the BAG.",
                    false, true, error);
            if (!has_capture_destination(programs, campaign))
                return show_battle_message(
                    view, "The POKéMON storage", "system is full.",
                    false, true, error);

            const CaptureBallProfile& ball =
                capture->ball_profiles[*selected_profile];
            BattleCaptureAttempt attempt;
            if (!attempt_battle_capture(
                    rules, battle_rules, *selected_profile,
                    campaign.battle, attempt, error))
                return false;
            if (!take_inventory_item(
                    campaign.inventory, ball.item_id, 1U)) {
                error =
                    "capture ball disappeared before transaction commit";
                return false;
            }
            const SpeciesRule* species =
                find_species(rules, attempt.species_dex);
            const std::string name =
                species == nullptr ? "POKéMON" : species->name;
            if (attempt.caught) {
                PokemonState caught =
                    campaign.battle.enemy_party.members[
                        campaign.battle.enemy.active_index];
                if (!store_captured_pokemon(
                        programs, campaign, std::move(caught))) {
                    error =
                        "capture destination changed during transaction";
                    return false;
                }
                campaign.battle.outcome = BattleOutcome::captured;
                campaign.battle.phase = BattlePhase::finished;
                return show_battle_message(
                    view, "All right!", name + " was caught!",
                    true, false, error);
            }

            const PokemonState& enemy =
                campaign.battle.enemy_party.members[
                    campaign.battle.enemy.active_index];
            const std::optional<std::size_t> enemy_move =
                first_executable_move_slot(
                    rules, battle_rules, enemy);
            if (enemy_move.has_value() &&
                !execute_enemy_battle_action(
                    rules, battle_rules, campaign.trainer_id,
                    campaign.party, campaign.battle, *enemy_move,
                    error))
                return false;
            if (!campaign.battle.active) {
                view.distinct_battlers = false;
                result.finished = true;
                error.clear();
                return true;
            }
            view.ui.mode = BattleUiMode::command;
            if (!sync_battle_view(
                    rules, battle_rules, campaign.party,
                    campaign.battle, view, error))
                return false;
            const std::string first =
                attempt.shakes == 0U
                    ? "Oh no! The POKéMON"
                    : "Aww! It appeared";
            const std::string second =
                attempt.shakes == 0U
                    ? "broke free!"
                    : "to be caught!";
            return show_battle_message(
                view, first, second, false, true, error);
        }
        if (action == "battle_choose_party") {
            std::optional<std::size_t> next;
            for (std::size_t offset = 1U;
                 offset < campaign.party.members.size(); ++offset) {
                const std::size_t candidate =
                    (campaign.battle.player.active_index + offset) %
                    campaign.party.members.size();
                if (campaign.party.members[candidate].current_hp != 0U) {
                    next = candidate;
                    break;
                }
            }
            if (!next.has_value())
                return show_battle_message(
                    view, "There is no other", "POKéMON ready.",
                    false, true, error);
            if (!switch_player_battler(
                    battle_rules, campaign.party, *next,
                    campaign.battle, error))
                return false;
            const PokemonState& enemy =
                campaign.battle.enemy_party.members[
                    campaign.battle.enemy.active_index];
            const std::optional<std::size_t> enemy_move =
                first_executable_move_slot(
                    rules, battle_rules, enemy);
            if (enemy_move.has_value() &&
                !execute_enemy_battle_action(
                    rules, battle_rules, campaign.trainer_id,
                    campaign.party, campaign.battle, *enemy_move,
                    error))
                return false;
            if (!campaign.battle.active) {
                view.distinct_battlers = false;
                result.finished = true;
                error.clear();
                return true;
            }
            view.ui.mode = BattleUiMode::command;
            if (!sync_battle_view(
                    rules, battle_rules, campaign.party,
                    campaign.battle, view, error))
                return false;
            const PokemonState& selected =
                campaign.party.members[*next];
            const SpeciesRule* species =
                find_species(rules, selected.species_dex);
            const std::string name =
                selected.nickname.empty() && species != nullptr
                    ? species->name
                    : selected.nickname;
            return show_battle_message(
                view, "Go!", name + "!", false, true, error);
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
    if (!sync_battle_view(
            rules, battle_rules, campaign.party,
            campaign.battle, view, error))
        return false;
    return begin_event_animations(
        rules, campaign.battle, view, error);
}

} // namespace pokered
