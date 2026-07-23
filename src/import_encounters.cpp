#include "import_encounters.hpp"

#include "encounters.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace pokered::import {
namespace {

constexpr std::size_t map_count = 248U;
constexpr std::size_t pointer_offset = 0x0CEEBU;
constexpr std::size_t data_begin = 0x0D0DDU;
constexpr std::size_t data_end = 0x0D5C7U;
constexpr std::size_t probability_offset = 0x13918U;
constexpr std::size_t pokedex_order_offset = 0x41024U;
constexpr std::size_t internal_species_count = 190U;

bool has_range(std::span<const std::uint8_t> rom, std::size_t offset,
               std::size_t size) {
    return offset <= rom.size() && size <= rom.size() - offset;
}

std::uint16_t read_u16(std::span<const std::uint8_t> rom,
                       std::size_t offset) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(rom[offset]) |
        static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(rom[offset + 1U]) << 8U));
}

bool pointer_to_offset(std::uint16_t pointer, std::size_t& result) {
    if (pointer < 0x4000U || pointer >= 0x8000U) return false;
    result = 3U * 0x4000U + pointer - 0x4000U;
    return true;
}

bool decode_table(std::span<const std::uint8_t> rom,
                  const std::array<std::uint8_t, internal_species_count>&
                      dex_by_internal,
                  std::size_t& cursor, EncounterTable& result,
                  std::string& error) {
    if (cursor >= data_end || cursor >= rom.size()) {
        error = "wild encounter table crosses its verified source range";
        return false;
    }
    result.rate = rom[cursor++];
    if (result.rate == 0U) return true;
    result.slots.reserve(10U);
    for (std::size_t index = 0; index < 10U; ++index) {
        if (!has_range(rom, cursor, 2U) || cursor + 2U > data_end) {
            error = "wild encounter slots cross their verified source range";
            return false;
        }
        const std::uint8_t level = rom[cursor++];
        const std::uint8_t internal_species = rom[cursor++];
        if (level == 0U || level > 100U || internal_species == 0U ||
            internal_species > internal_species_count ||
            dex_by_internal[internal_species - 1U] == 0U) {
            error = "wild encounter slot has an invalid level or species";
            return false;
        }
        result.slots.push_back(
            {level, dex_by_internal[internal_species - 1U]});
    }
    return true;
}

void write_string_file(EncounterImport& result, std::string path,
                       const std::string& text) {
    GeneratedFile file;
    file.relative_path = std::move(path);
    file.bytes.assign(text.begin(), text.end());
    result.files.push_back(std::move(file));
}

void emit_table(std::ostringstream& source, std::string_view name,
                const EncounterTable& table) {
    source << "    table " << name << '\n'
           << "        rate " << static_cast<unsigned>(table.rate) << '\n';
    for (std::size_t index = 0; index < table.slots.size(); ++index) {
        const EncounterSlot& slot = table.slots[index];
        source << "        slot " << index << " species_"
               << static_cast<unsigned>(slot.species_dex) << " level "
               << static_cast<unsigned>(slot.level) << '\n';
    }
}

void emit_source(const EncounterCatalog& encounters,
                 EncounterImport& result) {
    std::ostringstream probabilities;
    probabilities
        << "; Inclusive cartridge thresholds cover all 256 slot rolls.\n"
        << "encounter_probability_table gen_1_original_slots\n";
    for (const EncounterProbability& probability :
         encounters.probabilities)
        probabilities << "    slot "
                      << static_cast<unsigned>(probability.slot)
                      << " through "
                      << static_cast<unsigned>(
                             probability.inclusive_threshold)
                      << '\n';
    write_string_file(
        result, "source/encounters/probabilities.sexpr",
        probabilities.str());

    std::ostringstream maps;
    maps << "; All 248 map slots retain explicit land/water rates and pools.\n"
         << "encounter_terrain_policy pokemon_red_original\n"
         << "    water_tile "
         << static_cast<unsigned>(encounters.water_tile) << '\n'
         << "    first_indoor_map "
         << static_cast<unsigned>(encounters.first_indoor_map_id) << '\n'
         << "    excluded_indoor_tileset "
         << static_cast<unsigned>(
                encounters.excluded_indoor_tileset_id)
         << "\n\n";
    for (const MapEncounterTables& map : encounters.maps) {
        maps << "map_encounters map_"
             << static_cast<unsigned>(map.map_id) << '\n';
        emit_table(maps, "land", map.land);
        emit_table(maps, "water", map.water);
        maps << '\n';
    }
    write_string_file(result, "source/encounters/maps.sexpr", maps.str());

    std::ostringstream report;
    report << "Pokemon Red wild encounter import\n"
           << "map_slots " << result.map_slots << '\n'
           << "active_land_tables " << result.active_land_tables << '\n'
           << "active_water_tables " << result.active_water_tables << '\n'
           << "encounter_slots " << result.slots << '\n'
           << "probability_slots " << encounters.probabilities.size()
           << '\n'
           << "unresolved_entries 0\n";
    write_string_file(
        result, "reports/encounter_import_summary.txt", report.str());
}

void emit_cache(const EncounterCatalog& encounters,
                EncounterImport& result) {
    std::vector<std::uint8_t> bytes{'P', 'E', 'C', '1'};
    bytes.push_back(encounters.water_tile);
    bytes.push_back(encounters.first_indoor_map_id);
    bytes.push_back(encounters.excluded_indoor_tileset_id);
    bytes.push_back(
        static_cast<std::uint8_t>(encounters.probabilities.size()));
    for (const EncounterProbability& probability :
         encounters.probabilities) {
        bytes.push_back(probability.inclusive_threshold);
        bytes.push_back(probability.slot);
    }
    bytes.push_back(static_cast<std::uint8_t>(encounters.maps.size()));
    for (const MapEncounterTables& map : encounters.maps) {
        bytes.push_back(map.map_id);
        for (const EncounterTable* table : {&map.land, &map.water}) {
            bytes.push_back(table->rate);
            bytes.push_back(
                static_cast<std::uint8_t>(table->slots.size()));
            for (const EncounterSlot& slot : table->slots) {
                bytes.push_back(slot.level);
                bytes.push_back(slot.species_dex);
            }
        }
    }
    result.files.push_back({"compiled/encounters.bin", std::move(bytes)});
}

} // namespace

bool decode_encounter_import(std::span<const std::uint8_t> rom,
                             EncounterImport& result,
                             std::string& error) {
    result = {};
    if (!verify_pokemon_red_us_rev_0(rom, error)) return false;
    if (!has_range(rom, pointer_offset, map_count * 2U + 2U) ||
        rom[pointer_offset + map_count * 2U] != 0xFFU ||
        rom[pointer_offset + map_count * 2U + 1U] != 0xFFU ||
        !has_range(rom, pokedex_order_offset, internal_species_count) ||
        !has_range(rom, probability_offset, 20U)) {
        error = "wild encounter source tables exceed their verified ranges";
        return false;
    }

    std::array<std::uint8_t, internal_species_count> dex_by_internal{};
    for (std::size_t index = 0; index < internal_species_count; ++index) {
        const std::uint8_t dex = rom[pokedex_order_offset + index];
        if (dex > 151U) {
            error = "wild encounter species identity table is invalid";
            return false;
        }
        dex_by_internal[index] = dex;
    }

    EncounterCatalog encounters;
    if (rom[0x138ABU] != 0x3EU || rom[0x138ADU] != 0xB9U ||
        rom[0x138B6U] != 0xFEU || rom[0x138BDU] != 0xFEU ||
        rom[0x138DEU] != 0xFEU ||
        rom[0x138ACU] != rom[0x138DFU]) {
        error = "wild encounter terrain policy is not the verified routine";
        return false;
    }
    encounters.water_tile = rom[0x138ACU];
    encounters.first_indoor_map_id = rom[0x138B7U];
    encounters.excluded_indoor_tileset_id = rom[0x138BEU];

    encounters.probabilities.reserve(10U);
    for (std::size_t index = 0; index < 10U; ++index) {
        const std::uint8_t threshold =
            rom[probability_offset + index * 2U];
        const std::uint8_t slot_offset =
            rom[probability_offset + index * 2U + 1U];
        if (slot_offset != index * 2U ||
            (index != 0U &&
             threshold <=
                 encounters.probabilities.back().inclusive_threshold)) {
            error = "wild encounter probability table is invalid";
            return false;
        }
        encounters.probabilities.push_back(
            {threshold, static_cast<std::uint8_t>(index)});
    }
    if (encounters.probabilities.back().inclusive_threshold != 0xFFU) {
        error = "wild encounter probabilities do not cover every roll";
        return false;
    }

    encounters.maps.reserve(map_count);
    for (std::size_t map_id = 0; map_id < map_count; ++map_id) {
        std::size_t cursor = 0;
        if (!pointer_to_offset(
                read_u16(rom, pointer_offset + map_id * 2U), cursor) ||
            cursor < data_begin || cursor >= data_end) {
            error = "wild encounter pointer is outside its verified bank";
            return false;
        }
        MapEncounterTables map;
        map.map_id = static_cast<std::uint8_t>(map_id);
        if (!decode_table(rom, dex_by_internal, cursor, map.land, error) ||
            !decode_table(rom, dex_by_internal, cursor, map.water, error)) {
            return false;
        }
        if (map.land.rate != 0U) ++result.active_land_tables;
        if (map.water.rate != 0U) ++result.active_water_tables;
        result.slots += map.land.slots.size() + map.water.slots.size();
        encounters.maps.push_back(std::move(map));
    }
    result.map_slots = encounters.maps.size();
    emit_source(encounters, result);
    emit_cache(encounters, result);
    error.clear();
    return true;
}

} // namespace pokered::import
