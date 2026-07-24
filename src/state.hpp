#pragma once

#include "battle.hpp"
#include "inventory.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pokered {

enum class Mode {
    no_campaign,
    title,
    overworld,
    battle,
    battle_lab,
};

struct GameState {
    Mode mode{Mode::no_campaign};
    std::uint64_t step{};
    bool paused{};
};

struct CampaignBattleOwner {
    std::uint8_t map_id{};
    std::uint8_t actor_index{};
    std::uint32_t defeated_flag{};
    bool active{};
};

struct CampaignFiberState {
    std::size_t program_index{};
    std::size_t instruction_index{};
    std::uint32_t waiting_ticks{};
    bool waiting_dialogue{};
    bool waiting_motion{};
    bool waiting_choice{};
    bool waiting_naming{};
    bool waiting_battle{};
    std::uint8_t naming_party_index{};
    std::uint8_t last_choice{};
    bool last_item_grant_succeeded{};
    bool last_pokemon_grant_succeeded{};
    bool last_pokemon_sent_to_box{};
    bool active{};
};

struct CampaignTrainerBattleRequest {
    std::uint8_t trainer_class_id{};
    std::uint16_t trainer_party_index{};
    bool pending{};
};

struct PokemonStorageState {
    std::vector<std::vector<PokemonState>> boxes;
    std::uint16_t box_capacity{};
    std::uint8_t current_box{};
};

// Campaign-owned state is separate from host presentation settings and from
// transient world/render state. Imported programs address flags and variables
// through typed numeric IDs; vectors grow only as validated programs require.
struct CampaignState {
    std::string player_name;
    std::string rival_name;
    std::array<std::uint8_t, 3> options{};
    std::uint16_t trainer_id{};
    PartyState party;
    PokemonStorageState storage;
    InventoryState inventory;
    BattleState battle;
    CampaignBattleOwner battle_owner;
    CampaignTrainerBattleRequest trainer_battle_request;
    CampaignFiberState fiber;
    std::vector<std::uint8_t> flags;
    std::vector<std::uint16_t> variables;
    std::uint32_t money{};
    std::uint64_t play_steps{};
    bool imported_initial_state{};
    bool input_locked{};
    bool initialized{};
};

bool begin_new_campaign(CampaignState& campaign, std::string player_name,
                        std::string rival_name,
                        const std::array<std::uint8_t, 3>& options,
                        std::string& error);
bool campaign_flag(const CampaignState& campaign, std::uint32_t id);
void set_campaign_flag(CampaignState& campaign, std::uint32_t id, bool value);
std::uint16_t campaign_variable(const CampaignState& campaign, std::uint16_t id);
void set_campaign_variable(CampaignState& campaign, std::uint16_t id,
                           std::uint16_t value);
void step_game(GameState& game);
void step_campaign(CampaignState& campaign);
const char* label(Mode mode);

} // namespace pokered
