#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pokered {

struct TrainerPartyMember {
    std::uint8_t level{};
    std::uint8_t species_dex{};

    bool operator==(const TrainerPartyMember&) const = default;
};

struct TrainerPartyRule {
    std::vector<TrainerPartyMember> members;
};

struct TrainerClassRule {
    std::uint8_t id{};
    std::string name;
    std::uint32_t base_reward{};
    std::uint8_t ai_uses{};
    std::vector<TrainerPartyRule> parties;
};

struct TrainerCatalog {
    std::filesystem::path source;
    std::vector<TrainerClassRule> classes;
    bool loaded{};
};

bool load_trainers(const std::filesystem::path& path,
                   TrainerCatalog& result, std::string& error);
const TrainerClassRule* find_trainer_class(
    const TrainerCatalog& trainers, std::uint8_t class_id);
const TrainerPartyRule* find_trainer_party(
    const TrainerCatalog& trainers, std::uint8_t class_id,
    std::uint16_t party_index);

} // namespace pokered
