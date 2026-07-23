#include "encounters.hpp"

#include "maps.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <set>
#include <utility>

namespace pokered {
namespace {

bool read_u8(std::istream& input, std::uint8_t& result) {
    char value = 0;
    if (!input.get(value)) return false;
    result =
        static_cast<std::uint8_t>(static_cast<unsigned char>(value));
    return true;
}

bool read_table(std::istream& input, EncounterTable& table,
                std::string& error) {
    std::uint8_t count = 0;
    if (!read_u8(input, table.rate) || !read_u8(input, count) ||
        (table.rate == 0U) != (count == 0U) ||
        (count != 0U && count != 10U)) {
        error = "encounter cache has an invalid table header";
        return false;
    }
    table.slots.reserve(count);
    for (std::uint8_t index = 0; index < count; ++index) {
        EncounterSlot slot;
        if (!read_u8(input, slot.level) ||
            !read_u8(input, slot.species_dex) || slot.level == 0U ||
            slot.level > 100U || slot.species_dex == 0U ||
            slot.species_dex > 151U) {
            error = "encounter cache has an invalid slot";
            return false;
        }
        table.slots.push_back(slot);
    }
    return true;
}

std::uint8_t slot_for_roll(const EncounterCatalog& encounters,
                           std::uint8_t roll) {
    for (const EncounterProbability& probability :
         encounters.probabilities)
        if (roll <= probability.inclusive_threshold)
            return probability.slot;
    return 0xFFU;
}

} // namespace

bool load_encounters(const std::filesystem::path& path,
                     EncounterCatalog& result, std::string& error) {
    std::ifstream input(path, std::ios::binary);
    std::array<char, 4> magic{};
    if (!input.read(magic.data(),
                    static_cast<std::streamsize>(magic.size())) ||
        magic != std::array{'P', 'E', 'C', '1'}) {
        error = "encounter cache is missing or has an invalid header";
        return false;
    }

    EncounterCatalog loaded;
    loaded.source = path;
    std::uint8_t probability_count = 0;
    if (!read_u8(input, loaded.water_tile) ||
        !read_u8(input, loaded.first_indoor_map_id) ||
        !read_u8(input, loaded.excluded_indoor_tileset_id) ||
        !read_u8(input, probability_count) ||
        probability_count != 10U) {
        error = "encounter cache has invalid terrain or probability metadata";
        return false;
    }
    loaded.probabilities.reserve(probability_count);
    for (std::uint8_t index = 0; index < probability_count; ++index) {
        EncounterProbability probability;
        if (!read_u8(input, probability.inclusive_threshold) ||
            !read_u8(input, probability.slot) ||
            probability.slot != index ||
            (index != 0U &&
             probability.inclusive_threshold <=
                 loaded.probabilities.back().inclusive_threshold)) {
            error = "encounter cache has invalid cumulative probabilities";
            return false;
        }
        loaded.probabilities.push_back(probability);
    }
    if (loaded.probabilities.back().inclusive_threshold != 0xFFU) {
        error = "encounter cache probabilities do not cover every byte";
        return false;
    }

    std::uint8_t map_count = 0;
    if (!read_u8(input, map_count) || map_count != 248U) {
        error = "encounter cache does not account for all map slots";
        return false;
    }
    loaded.maps.reserve(map_count);
    std::set<std::uint8_t> map_ids;
    for (std::uint16_t index = 0; index < map_count; ++index) {
        MapEncounterTables map;
        if (!read_u8(input, map.map_id) ||
            map.map_id != static_cast<std::uint8_t>(index) ||
            !map_ids.insert(map.map_id).second ||
            !read_table(input, map.land, error) ||
            !read_table(input, map.water, error)) {
            if (error.empty())
                error = "encounter cache has an invalid map table";
            return false;
        }
        loaded.maps.push_back(std::move(map));
    }
    if (input.peek() != std::char_traits<char>::eof()) {
        error = "encounter cache contains trailing data";
        return false;
    }
    loaded.loaded = true;
    result = std::move(loaded);
    error.clear();
    return true;
}

const MapEncounterTables* find_map_encounters(
    const EncounterCatalog& encounters, std::uint8_t map_id) {
    return map_id < encounters.maps.size() &&
                   encounters.maps[map_id].map_id == map_id
               ? &encounters.maps[map_id]
               : nullptr;
}

bool resolve_world_wild_encounter(
    const EncounterCatalog& encounters, const WorldState& world,
    std::uint8_t chance_roll, std::uint8_t slot_roll,
    bool repel_active, std::uint8_t leading_level,
    WildEncounterResult& result, std::string& error) {
    result = {
        .chance_roll = chance_roll,
        .slot_roll = slot_roll,
    };
    if (!encounters.loaded || !world.loaded ||
        world.player.map_index >= world.maps.size()) {
        error = "wild encounter resolver received incomplete world content";
        return false;
    }
    if (!world.player_completed_step) {
        result.suppression =
            WildEncounterSuppression::no_completed_step;
        error.clear();
        return true;
    }

    const WorldMap& map = world.maps[world.player.map_index];
    const MapTileset* tileset = find_tileset(world, map.tileset_id);
    const MapEncounterTables* tables =
        find_map_encounters(encounters, map.id);
    if (tileset == nullptr || tables == nullptr ||
        world.player.x < 0 || world.player.y < 0) {
        error = "wild encounter resolver cannot resolve its map records";
        return false;
    }
    const std::size_t tile_x =
        static_cast<std::size_t>(world.player.x) * 2U;
    const std::size_t tile_y =
        static_cast<std::size_t>(world.player.y) * 2U + 1U;
    const std::size_t left_offset = tile_y * map.width_tiles + tile_x;
    const std::size_t right_offset = left_offset + 1U;
    if (right_offset >= map.tiles.size()) {
        error = "wild encounter resolver starts outside map tile data";
        return false;
    }
    const std::uint8_t bottom_left = map.tiles[left_offset];
    const std::uint8_t bottom_right = map.tiles[right_offset];

    // Terrain constants and indoor policy are imported campaign content.
    // Runtime only applies the normalized rule to the current cell.
    if (bottom_right == tileset->grass_tile) {
        result.environment = EncounterEnvironment::land;
        result.encounter_rate = tables->land.rate;
    } else if (bottom_right == encounters.water_tile) {
        result.environment = EncounterEnvironment::water;
        result.encounter_rate = tables->water.rate;
    } else if (map.id >= encounters.first_indoor_map_id &&
               map.tileset_id !=
                   encounters.excluded_indoor_tileset_id) {
        result.environment = EncounterEnvironment::land;
        result.encounter_rate = tables->land.rate;
    }
    if (result.encounter_rate == 0U) {
        result.suppression = WildEncounterSuppression::invalid_terrain;
        error.clear();
        return true;
    }
    if (chance_roll >= result.encounter_rate) {
        result.suppression = WildEncounterSuppression::rate_roll;
        error.clear();
        return true;
    }

    const EncounterTable& selected =
        bottom_left == encounters.water_tile ? tables->water : tables->land;
    result.environment =
        bottom_left == encounters.water_tile ? EncounterEnvironment::water
                                             : EncounterEnvironment::land;
    result.slot_index = slot_for_roll(encounters, slot_roll);
    if (result.slot_index >= selected.slots.size()) {
        error = "wild encounter roll selected an absent content slot";
        return false;
    }
    const EncounterSlot& slot = selected.slots[result.slot_index];
    result.level = slot.level;
    result.species_dex = slot.species_dex;
    if (repel_active && slot.level < leading_level) {
        result.suppression = WildEncounterSuppression::repel_level;
        error.clear();
        return true;
    }
    result.occurred = true;
    error.clear();
    return true;
}

} // namespace pokered
