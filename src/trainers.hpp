#pragma once

#include <array>
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
    std::uint8_t actor_trainer_offset{};
    std::array<std::uint8_t, 190> dex_by_internal{};
    std::vector<TrainerClassRule> classes;
    bool loaded{};
};

enum class ActorOpponentKind : std::uint8_t {
    static_pokemon,
    trainer,
};

struct ActorOpponentBinding {
    ActorOpponentKind kind{ActorOpponentKind::static_pokemon};
    std::uint8_t trainer_class_id{};
    std::uint16_t trainer_party_index{};
    std::uint8_t species_dex{};
    std::uint8_t level{};
};

bool load_trainers(const std::filesystem::path& path,
                   TrainerCatalog& result, std::string& error);
const TrainerClassRule* find_trainer_class(
    const TrainerCatalog& trainers, std::uint8_t class_id);
const TrainerPartyRule* find_trainer_party(
    const TrainerCatalog& trainers, std::uint8_t class_id,
    std::uint16_t party_index);
bool resolve_actor_opponent(const TrainerCatalog& trainers,
                            std::uint8_t parameter_a,
                            std::uint8_t parameter_b,
                            ActorOpponentBinding& result,
                            std::string& error);

} // namespace pokered
