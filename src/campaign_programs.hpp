#pragma once

#include "maps.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pokered {

struct CampaignState;
enum class CampaignOpcode : std::uint8_t {
    lock_input,
    set_flag,
    show_actor,
    hide_actor,
    say,
    face_actor,
    face_player,
    move_actor_to_player,
    align_pair_x,
    parallel_path,
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
    std::uint8_t trigger_map_id{};
    std::uint8_t trigger_y{};
    std::uint32_t absent_flag{};
    std::vector<CampaignActorRef> initially_hidden;
    std::vector<CampaignInstruction> instructions;
};

struct CampaignProgramCatalog {
    std::filesystem::path source;
    std::vector<CampaignProgram> programs;
    bool loaded{};
};

bool load_campaign_programs(const std::filesystem::path& path, CampaignProgramCatalog& result,
                            std::string& error);
bool initialize_campaign_program_runtime(const CampaignProgramCatalog& programs, WorldState& world,
                                         std::string& error);
bool service_campaign_programs(const CampaignProgramCatalog& programs, WorldState& world,
                               CampaignState& campaign, std::string& error);

} // namespace pokered
