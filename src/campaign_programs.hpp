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
    nickname_last_party_member_if_yes,
    player_path,
    give_item,
    try_give_item,
    take_item,
    place_actor,
    actor_path,
    jump_if_player_y,
    jump_if_item_grant_failed,
    jump,
    wait_ticks,
    actor_path_by_player_x,
    actor_path_by_player_y,
    start_trainer_battle,
    say_if_player_won,
    say_if_player_lost,
    end_if_player_lost,
    heal_party,
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

struct CampaignProgramCatalog {
    std::filesystem::path source;
    NamingProfile naming;
    std::string nickname_heading;
    std::uint16_t inventory_stack_capacity{};
    std::vector<CampaignProgram> programs;
    bool loaded{};
};

bool load_campaign_programs(const std::filesystem::path& path, CampaignProgramCatalog& result,
                            std::string& error);
bool initialize_campaign_program_runtime(const CampaignProgramCatalog& programs, WorldState& world,
                                         std::string& error);
bool service_campaign_programs(const CampaignProgramCatalog& programs, const RuleCatalog& rules,
                               const BattleRuleCatalog& battle_rules, WorldState& world,
                               CampaignState& campaign, std::string& error);

} // namespace pokered
