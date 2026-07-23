#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace pokered {

enum class Mode {
    no_campaign,
    title,
    overworld,
    battle,
};

struct GameState {
    Mode mode{Mode::no_campaign};
    std::uint64_t step{};
    bool paused{};
};

// Campaign-owned state is separate from host presentation settings and from
// transient world/render state. Imported programs address flags and variables
// through typed numeric IDs; vectors grow only as validated programs require.
struct CampaignState {
    std::string player_name;
    std::string rival_name;
    std::array<std::uint8_t, 3> options{};
    std::vector<std::uint8_t> flags;
    std::vector<std::uint16_t> variables;
    std::uint64_t play_steps{};
    bool input_locked{};
    bool initialized{};
};

bool begin_new_campaign(CampaignState& campaign, std::string player_name,
                        std::string rival_name,
                        const std::array<std::uint8_t, 3>& options,
                        std::string& error);
bool campaign_flag(const CampaignState& campaign, std::uint16_t id);
void set_campaign_flag(CampaignState& campaign, std::uint16_t id, bool value);
std::uint16_t campaign_variable(const CampaignState& campaign, std::uint16_t id);
void set_campaign_variable(CampaignState& campaign, std::uint16_t id,
                           std::uint16_t value);
void step_game(GameState& game);
void step_campaign(CampaignState& campaign);
const char* label(Mode mode);

} // namespace pokered
