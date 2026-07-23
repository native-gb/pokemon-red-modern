#include "state.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace pokered {

bool begin_new_campaign(CampaignState& campaign, std::string player_name,
                        std::string rival_name,
                        const std::array<std::uint8_t, 3>& options,
                        std::string& error) {
    if (player_name.empty() || rival_name.empty() ||
        std::ranges::any_of(options,
                            [](std::uint8_t value) { return value > 2U; })) {
        error = "new campaign has invalid naming or option results";
        return false;
    }
    CampaignState started;
    started.player_name = std::move(player_name);
    started.rival_name = std::move(rival_name);
    started.options = options;
    std::uint32_t identity = 2166136261U;
    for (char byte : started.player_name)
        identity =
            (identity ^ static_cast<std::uint8_t>(byte)) * 16777619U;
    for (char byte : started.rival_name)
        identity =
            (identity ^ static_cast<std::uint8_t>(byte)) * 16777619U;
    for (std::uint8_t byte : options)
        identity = (identity ^ byte) * 16777619U;
    started.trainer_id = static_cast<std::uint16_t>(
        identity ^ (identity >> 16U));
    started.initialized = true;
    campaign = std::move(started);
    error.clear();
    return true;
}

bool campaign_flag(const CampaignState& campaign, std::uint32_t id) {
    const std::size_t index = static_cast<std::size_t>(id);
    return index < campaign.flags.size() &&
           campaign.flags[index] != 0U;
}

void set_campaign_flag(CampaignState& campaign, std::uint32_t id, bool value) {
    const std::size_t index = static_cast<std::size_t>(id);
    if (index >= campaign.flags.size())
        campaign.flags.resize(index + 1U);
    campaign.flags[index] = value ? 1U : 0U;
}

std::uint16_t campaign_variable(const CampaignState& campaign, std::uint16_t id) {
    return id < campaign.variables.size() ? campaign.variables[id] : 0U;
}

void set_campaign_variable(CampaignState& campaign, std::uint16_t id,
                           std::uint16_t value) {
    if (id >= campaign.variables.size())
        campaign.variables.resize(static_cast<std::size_t>(id) + 1U);
    campaign.variables[id] = value;
}

void step_game(GameState& game) {
    if (!game.paused) ++game.step;
}

void step_campaign(CampaignState& campaign) {
    if (campaign.initialized) ++campaign.play_steps;
}

const char* label(Mode mode) {
    switch (mode) {
    case Mode::no_campaign:
        return "No campaign";
    case Mode::title:
        return "Title";
    case Mode::overworld:
        return "Overworld";
    case Mode::battle:
        return "Battle";
    case Mode::battle_lab:
        return "Battle lab";
    }
    return "Unknown";
}

} // namespace pokered
