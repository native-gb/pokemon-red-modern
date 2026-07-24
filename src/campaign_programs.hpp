#pragma once

#include "maps.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pokered {

struct BattleRuleCatalog;
struct CampaignState;
struct RuleCatalog;

enum class CampaignTriggerKind : std::uint8_t {
    player_y,
    actor_activation,
    map_entry,
    player_rectangle,
    map_presence,
    cell_activation,
};

enum class CampaignOpcode : std::uint8_t {
    lock_input,
    set_flag,
    clear_flag,
    show_actor,
    hide_actor,
    say,
    face_actor,
    face_player,
    move_actor_to_player,
    align_pair_x,
    parallel_path,
    ask_yes_no,
    end_if_choice_no,
    set_variable,
    give_pokemon,
    try_give_pokemon,
    jump_if_pokemon_grant_failed,
    say_if_pokemon_sent_to_box,
    jump_if_money_below,
    take_money,
    nickname_last_party_member_if_yes,
    player_path,
    give_item,
    try_give_item,
    take_item,
    place_actor,
    place_actor_scripted,
    place_actor_at_player_x,
    actor_path,
    jump_if_player_y,
    jump_if_item_grant_failed,
    jump,
    wait_ticks,
    actor_path_by_player_x,
    actor_path_by_player_y,
    actor_path_by_player_facing,
    start_trainer_battle,
    jump_if_choice_no,
    say_if_player_won,
    say_if_player_lost,
    jump_if_player_won,
    end_if_player_lost,
    heal_party,
    escort_player_to,
    unlock_input,
    end,
};

struct CampaignActorRef {
    std::uint8_t map_id{};
    std::uint8_t actor_index{};
};

struct CampaignInstruction {
    CampaignOpcode opcode{CampaignOpcode::end};
    std::uint8_t a{};
    std::uint8_t b{};
    std::uint32_t value{};
    std::vector<std::string> pages;
    std::vector<WorldPathCommand> actor_path;
    std::vector<WorldPathCommand> player_path;
};

struct CampaignProgram {
    std::string key;
    CampaignTriggerKind trigger_kind{CampaignTriggerKind::player_y};
    std::uint8_t trigger_map_id{};
    std::uint8_t trigger_x{};
    std::uint8_t trigger_y{};
    std::uint8_t trigger_width{};
    std::uint8_t trigger_height{};
    std::uint32_t required_flag{0xFFFFFFFFU};
    std::uint32_t absent_flag{0xFFFFFFFFU};
    std::uint16_t required_variable{0xFFFFU};
    std::uint16_t required_variable_value{};
    std::uint16_t required_item_id{};
    std::uint16_t required_item_quantity{};
    std::vector<CampaignActorRef> initially_hidden;
    std::vector<CampaignInstruction> instructions;
};

struct CampaignItemName {
    std::uint16_t item_id{};
    std::string name;
};

struct CampaignEncounterSuppressionZone {
    std::uint8_t map_id{};
    std::uint8_t x{};
    std::uint8_t y{};
    std::uint8_t width{};
    std::uint8_t height{};
    std::uint32_t required_flag{0xFFFFFFFFU};
};

struct CampaignProgramCatalog {
    std::filesystem::path source;
    NamingProfile naming;
    std::string nickname_heading;
    std::uint16_t inventory_stack_capacity{};
    std::uint32_t starting_money{};
    std::uint8_t party_capacity{};
    std::uint8_t storage_box_count{};
    std::uint16_t storage_box_capacity{};
    std::vector<CampaignItemName> item_names;
    std::vector<std::string> found_item_pages;
    std::vector<std::string> no_item_room_pages;
    std::vector<CampaignEncounterSuppressionZone>
        encounter_suppression_zones;
    std::vector<CampaignProgram> programs;
    bool loaded{};
};

bool load_campaign_programs(const std::filesystem::path& path, CampaignProgramCatalog& result,
                            std::string& error);
bool initialize_campaign_program_runtime(const CampaignProgramCatalog& programs, WorldState& world,
                                         std::string& error);
bool campaign_suppresses_wild_encounters(
    const CampaignProgramCatalog& programs,
    const CampaignState& campaign, const WorldState& world);
bool service_campaign_programs(const CampaignProgramCatalog& programs, const RuleCatalog& rules,
                               const BattleRuleCatalog& battle_rules, WorldState& world,
                               CampaignState& campaign, std::string& error);

} // namespace pokered
