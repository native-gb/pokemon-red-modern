#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pokered {

struct WorldState;

enum class EncounterEnvironment : std::uint8_t {
    land,
    water,
};

struct EncounterSlot {
    std::uint8_t level{};
    std::uint8_t species_dex{};
};

struct EncounterTable {
    std::uint8_t rate{};
    std::vector<EncounterSlot> slots;
};

struct MapEncounterTables {
    std::uint8_t map_id{};
    EncounterTable land;
    EncounterTable water;
};

struct EncounterProbability {
    std::uint8_t inclusive_threshold{};
    std::uint8_t slot{};
};

struct EncounterCatalog {
    std::filesystem::path source;
    std::uint8_t water_tile{};
    std::uint8_t first_indoor_map_id{};
    std::uint8_t excluded_indoor_tileset_id{};
    std::vector<EncounterProbability> probabilities;
    std::vector<MapEncounterTables> maps;
    bool loaded{};
};

enum class WildEncounterSuppression : std::uint8_t {
    none,
    no_completed_step,
    invalid_terrain,
    rate_roll,
    repel_level,
};

struct WildEncounterResult {
    bool occurred{};
    WildEncounterSuppression suppression{
        WildEncounterSuppression::none};
    EncounterEnvironment environment{EncounterEnvironment::land};
    std::uint8_t encounter_rate{};
    std::uint8_t chance_roll{};
    std::uint8_t slot_roll{};
    std::uint8_t slot_index{};
    std::uint8_t level{};
    std::uint8_t species_dex{};
};

bool load_encounters(const std::filesystem::path& path,
                     EncounterCatalog& result, std::string& error);
const MapEncounterTables* find_map_encounters(
    const EncounterCatalog& encounters, std::uint8_t map_id);
bool resolve_world_wild_encounter(
    const EncounterCatalog& encounters, const WorldState& world,
    std::uint8_t chance_roll, std::uint8_t slot_roll,
    bool repel_active, std::uint8_t leading_level,
    WildEncounterResult& result, std::string& error);

} // namespace pokered
