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

struct MapInteractions {
    std::uint8_t map_id{};
    bool decoded{};
    std::vector<InteractionOwner> backgrounds;
    std::vector<InteractionOwner> actors;
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

} // namespace pokered
