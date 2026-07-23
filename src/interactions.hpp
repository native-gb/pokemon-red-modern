#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pokered {

enum class InteractionProgramStatus : std::uint8_t {
    unresolved,
    dialogue,
    decoded_native,
    untranslated_native,
};

struct InteractionProgram {
    InteractionProgramStatus status{InteractionProgramStatus::unresolved};
    std::vector<std::string> pages;
};

struct InteractionOwner {
    std::uint8_t index{};
    std::uint8_t x{};
    std::uint8_t y{};
    std::uint8_t program_id{};
};

struct TrainerInteractionRule {
    std::uint8_t actor_index{};
    std::uint8_t sight_range{};
    std::uint32_t defeated_flag{};
    std::vector<std::string> before_pages;
    std::vector<std::string> after_pages;
    std::vector<std::string> end_pages;
};

struct MapInteractions {
    std::uint8_t map_id{};
    bool decoded{};
    std::vector<InteractionOwner> backgrounds;
    std::vector<InteractionOwner> actors;
    std::vector<TrainerInteractionRule> trainers;
    std::vector<InteractionProgram> programs;
};

struct InteractionCatalog {
    std::filesystem::path source;
    std::vector<MapInteractions> maps;
    bool loaded{};
};

bool load_interactions(const std::filesystem::path& path, InteractionCatalog& result,
                       std::string& error);
const MapInteractions* find_map_interactions(const InteractionCatalog& catalog,
                                             std::uint8_t map_id);
const InteractionProgram* find_interaction(const InteractionCatalog& catalog,
                                           std::uint8_t map_id, std::uint8_t program_id);
const TrainerInteractionRule* find_trainer_interaction(
    const InteractionCatalog& catalog, std::uint8_t map_id,
    std::uint8_t actor_index);

} // namespace pokered
