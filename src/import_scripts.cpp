#include "import_scripts.hpp"
#include "import_text.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pokered::import {
namespace {

constexpr std::size_t kBankSize = 0x4000;
constexpr std::size_t kMapHeaderPointersOffset = 0x001AE;
constexpr std::size_t kMapHeaderBanksOffset = 0x0C23D;
constexpr std::size_t kMapSlotCount = 0xF8;
constexpr std::size_t kExpectedDecodedMapCount = 226;
constexpr std::size_t kExpectedUnusedMapCount = 22;
constexpr std::size_t kFixedHeaderSize = 10;
constexpr std::size_t kConnectionSize = 11;
constexpr std::size_t kMaximumMapDimension = 128;
constexpr std::size_t kMaximumWarps = 32;
constexpr std::size_t kMaximumBackgroundEvents = 32;
constexpr std::size_t kMaximumActors = 16;

struct InteractionOwner {
    std::uint8_t index{};
    std::uint8_t x{};
    std::uint8_t y{};
    std::uint8_t text_id{};
};

struct MapProgramInventory {
    std::uint8_t map_id{};
    std::uint8_t bank{};
    std::uint8_t tileset{};
    std::uint8_t width{};
    std::uint8_t height{};
    std::uint16_t header_pointer{};
    std::uint16_t text_pointer{};
    std::uint16_t script_pointer{};
    std::uint16_t objects_pointer{};
    std::size_t header_offset{};
    std::size_t text_offset{};
    std::size_t script_offset{};
    std::size_t objects_offset{};
    std::vector<InteractionOwner> backgrounds;
    std::vector<InteractionOwner> actors;
    std::vector<std::size_t> text_entry_offsets;
    std::vector<DecodedTextProgram> owned_entries;
    std::string unresolved_reason;
    std::size_t alias_of{kMapSlotCount};
    bool decoded{};
};

bool has_range(std::span<const std::uint8_t> rom, std::size_t offset, std::size_t size) {
    return offset <= rom.size() && size <= rom.size() - offset;
}

std::uint16_t read_u16(std::span<const std::uint8_t> rom, std::size_t offset) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(rom[offset]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(rom[offset + 1U]) << 8U));
}

bool bank_pointer_to_offset(std::span<const std::uint8_t> rom, std::uint8_t bank,
                            std::uint16_t pointer, std::size_t& result) {
    if (bank == 0) {
        if (pointer >= 0x4000) return false;
        result = pointer;
    } else {
        if (pointer < 0x4000 || pointer >= 0x8000) return false;
        result = static_cast<std::size_t>(bank) * kBankSize +
                 static_cast<std::size_t>(pointer - 0x4000U);
    }
    return result < rom.size();
}

bool visible_pointer_to_offset(std::span<const std::uint8_t> rom, std::uint8_t bank,
                               std::uint16_t pointer, std::size_t& result) {
    if (pointer < 0x4000) {
        result = pointer;
        return result < rom.size();
    }
    return bank_pointer_to_offset(rom, bank, pointer, result);
}

void write_u16(std::vector<std::uint8_t>& bytes, std::size_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void write_u32(std::vector<std::uint8_t>& bytes, std::size_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void add_text_file(ScriptImport& result, std::string path, std::string text) {
    GeneratedFile file;
    file.relative_path = std::move(path);
    file.bytes.assign(text.begin(), text.end());
    result.files.push_back(std::move(file));
}

std::string map_slot_key(std::size_t map_id) {
    std::ostringstream key;
    key << "map_" << std::setfill('0') << std::setw(3) << map_id;
    return key.str();
}

std::string source_address(std::uint8_t bank, std::size_t offset) {
    std::ostringstream source;
    source << "bank_" << std::hex << std::setfill('0') << std::setw(2)
           << static_cast<unsigned>(bank) << " 0x" << std::setw(6) << offset;
    return source.str();
}

std::uint8_t source_bank(std::uint8_t mapped_bank, std::size_t offset) {
    return offset < 0x4000 ? 0U : mapped_bank;
}

bool decode_object_owners(std::span<const std::uint8_t> rom, MapProgramInventory& map,
                          std::string& reason) {
    std::size_t cursor = map.objects_offset;
    if (!has_range(rom, cursor, 2)) {
        reason = "object_header_out_of_range";
        return false;
    }
    ++cursor; // border block

    const std::uint8_t warp_count = rom[cursor++];
    const std::size_t warp_bytes = static_cast<std::size_t>(warp_count) * 4U;
    if (warp_count > kMaximumWarps || !has_range(rom, cursor, warp_bytes + 1U)) {
        reason = "invalid_warp_table";
        return false;
    }
    cursor += warp_bytes;

    const std::uint8_t background_count = rom[cursor++];
    const std::size_t background_bytes = static_cast<std::size_t>(background_count) * 3U;
    if (background_count > kMaximumBackgroundEvents ||
        !has_range(rom, cursor, background_bytes + 1U)) {
        reason = "invalid_background_table";
        return false;
    }
    map.backgrounds.reserve(background_count);
    for (std::uint8_t index = 0; index < background_count; ++index) {
        map.backgrounds.push_back({
            .index = static_cast<std::uint8_t>(index + 1U),
            .x = rom[cursor + 1U],
            .y = rom[cursor],
            .text_id = rom[cursor + 2U],
        });
        cursor += 3U;
    }

    const std::uint8_t actor_count = rom[cursor++];
    if (actor_count > kMaximumActors) {
        reason = "invalid_actor_table";
        return false;
    }
    map.actors.reserve(actor_count);
    for (std::uint8_t index = 0; index < actor_count; ++index) {
        if (!has_range(rom, cursor, 6)) {
            reason = "actor_table_out_of_range";
            return false;
        }
        const std::uint8_t stored_y = rom[cursor + 1U];
        const std::uint8_t stored_x = rom[cursor + 2U];
        const std::uint8_t encoded_text = rom[cursor + 5U];
        if (stored_x < 4 || stored_y < 4) {
            reason = "invalid_actor_coordinate";
            return false;
        }
        map.actors.push_back({
            .index = static_cast<std::uint8_t>(index + 1U),
            .x = static_cast<std::uint8_t>(stored_x - 4U),
            .y = static_cast<std::uint8_t>(stored_y - 4U),
            .text_id = static_cast<std::uint8_t>(encoded_text & 0x3FU),
        });
        cursor += 6U;

        const std::uint8_t kind = static_cast<std::uint8_t>(encoded_text & 0xC0U);
        const std::size_t extra_bytes = kind == 0x40U ? 2U : kind == 0x80U ? 1U : 0U;
        if (kind == 0xC0U || !has_range(rom, cursor, extra_bytes)) {
            reason = "invalid_actor_kind";
            return false;
        }
        cursor += extra_bytes;
    }
    return true;
}

bool decode_map_program(std::span<const std::uint8_t> rom, std::size_t map_id,
                        MapProgramInventory& map) {
    map.map_id = static_cast<std::uint8_t>(map_id);
    const std::size_t pointer_record = kMapHeaderPointersOffset + map_id * 2U;
    const std::size_t bank_record = kMapHeaderBanksOffset + map_id;
    if (!has_range(rom, pointer_record, 2) || !has_range(rom, bank_record, 1)) {
        map.unresolved_reason = "map_lookup_out_of_range";
        return false;
    }

    map.bank = rom[bank_record];
    map.header_pointer = read_u16(rom, pointer_record);
    if (!bank_pointer_to_offset(rom, map.bank, map.header_pointer, map.header_offset) ||
        !has_range(rom, map.header_offset, kFixedHeaderSize)) {
        map.unresolved_reason = "map_header_out_of_range";
        return false;
    }

    map.tileset = rom[map.header_offset];
    map.height = rom[map.header_offset + 1U];
    map.width = rom[map.header_offset + 2U];
    map.text_pointer = read_u16(rom, map.header_offset + 5U);
    map.script_pointer = read_u16(rom, map.header_offset + 7U);
    const std::uint8_t connections = rom[map.header_offset + 9U];
    if (map.tileset >= 24 || map.width == 0 || map.height == 0 ||
        map.width > kMaximumMapDimension || map.height > kMaximumMapDimension ||
        (connections & 0xF0U) != 0) {
        map.unresolved_reason = "invalid_fixed_map_header";
        return false;
    }
    if (!bank_pointer_to_offset(rom, map.bank, map.text_pointer, map.text_offset)) {
        map.unresolved_reason = "text_table_out_of_range";
        return false;
    }
    if (!bank_pointer_to_offset(rom, map.bank, map.script_pointer, map.script_offset)) {
        map.unresolved_reason = "script_entry_out_of_range";
        return false;
    }

    const std::size_t connection_count =
        static_cast<std::size_t>(std::popcount(static_cast<unsigned>(connections & 0x0FU)));
    const std::size_t object_pointer_offset =
        map.header_offset + kFixedHeaderSize + connection_count * kConnectionSize;
    if (!has_range(rom, object_pointer_offset, 2)) {
        map.unresolved_reason = "object_pointer_out_of_range";
        return false;
    }
    map.objects_pointer = read_u16(rom, object_pointer_offset);
    if (!bank_pointer_to_offset(rom, map.bank, map.objects_pointer, map.objects_offset)) {
        map.unresolved_reason = "object_table_out_of_range";
        return false;
    }
    if (!decode_object_owners(rom, map, map.unresolved_reason)) return false;

    std::uint8_t maximum_text_id = 0;
    for (const InteractionOwner& owner : map.backgrounds)
        maximum_text_id = std::max(maximum_text_id, owner.text_id);
    for (const InteractionOwner& owner : map.actors)
        maximum_text_id = std::max(maximum_text_id, owner.text_id);
    if (!has_range(rom, map.text_offset, static_cast<std::size_t>(maximum_text_id) * 2U)) {
        map.unresolved_reason = "owned_text_pointer_table_out_of_range";
        return false;
    }
    map.text_entry_offsets.reserve(maximum_text_id);
    for (std::uint8_t text_id = 1; text_id <= maximum_text_id; ++text_id) {
        std::size_t text_offset = 0;
        const std::uint16_t text_pointer =
            read_u16(rom, map.text_offset + static_cast<std::size_t>(text_id - 1U) * 2U);
        if (!visible_pointer_to_offset(rom, map.bank, text_pointer, text_offset)) {
            map.unresolved_reason = "owned_text_entry_out_of_range";
            return false;
        }
        map.text_entry_offsets.push_back(text_offset);
        DecodedTextProgram program;
        decode_text_program(rom, source_bank(map.bank, text_offset), text_offset, program);
        map.owned_entries.push_back(std::move(program));
    }

    map.decoded = true;
    return true;
}

void assign_aliases(std::vector<MapProgramInventory>& maps) {
    std::map<std::pair<std::uint8_t, std::uint16_t>, std::size_t> canonical;
    for (MapProgramInventory& map : maps) {
        if (!map.decoded) continue;
        const auto identity = std::make_pair(map.bank, map.header_pointer);
        const auto [position, inserted] = canonical.emplace(identity, map.map_id);
        if (!inserted) map.alias_of = position->second;
    }
}

std::string owned_entry_key(const MapProgramInventory& map, std::size_t index,
                            std::string_view domain) {
    std::ostringstream key;
    key << map_slot_key(map.map_id) << '_' << domain << '_' << std::setfill('0') << std::setw(2)
        << index + 1U;
    return key.str();
}

void emit_interaction_target(std::ostringstream& source, const MapProgramInventory& map,
                             std::uint8_t text_id) {
    const std::size_t index = static_cast<std::size_t>(text_id - 1U);
    source << " script " << owned_entry_key(map, index, "interaction");
}

void emit_map_source(const MapProgramInventory& map, ScriptImport& result) {
    const std::string key = map_slot_key(map.map_id);
    std::ostringstream source;
    source << "; ROM-derived ownership inventory. This routine is not semantically lifted yet.\n"
           << "map_program_inventory " << key << '\n'
           << "    rom_id " << static_cast<unsigned>(map.map_id) << '\n';
    if (!map.decoded) {
        source << "    status unresolved\n"
               << "    reason " << map.unresolved_reason << '\n'
               << "    lookup_source " << kMapHeaderBanksOffset + map.map_id << ' '
               << kMapHeaderPointersOffset + static_cast<std::size_t>(map.map_id) * 2U << '\n';
    } else {
        source << "    status decoded_untranslated\n"
               << "    header_source " << source_address(map.bank, map.header_offset) << ' '
               << kFixedHeaderSize << '\n'
               << "    load_script_entry " << source_address(map.bank, map.script_offset) << '\n'
               << "    text_table " << source_address(map.bank, map.text_offset) << '\n'
               << "    object_table " << source_address(map.bank, map.objects_offset) << '\n';
        if (map.alias_of != kMapSlotCount)
            source << "    alias_of " << map_slot_key(map.alias_of) << '\n';
        for (std::size_t index = 0; index < map.owned_entries.size(); ++index) {
            source << "    owned_entry entry_" << std::setfill('0') << std::setw(2) << index + 1U
                   << " script " << owned_entry_key(map, index, "interaction") << '\n';
        }
        for (const InteractionOwner& background : map.backgrounds) {
            source << "    background_interaction " << static_cast<unsigned>(background.index)
                   << " at " << static_cast<unsigned>(background.x) << ' '
                   << static_cast<unsigned>(background.y);
            emit_interaction_target(source, map, background.text_id);
            source << '\n';
        }
        for (const InteractionOwner& actor : map.actors) {
            source << "    actor_interaction " << static_cast<unsigned>(actor.index) << " at "
                   << static_cast<unsigned>(actor.x) << ' ' << static_cast<unsigned>(actor.y);
            emit_interaction_target(source, map, actor.text_id);
            source << '\n';
        }
    }
    add_text_file(result, "source/scripts/maps/" + key + ".sexpr", source.str());
}

void emit_owned_entry_sources(const MapProgramInventory& map, ScriptImport& result) {
    if (!map.decoded || map.owned_entries.empty()) return;

    const std::string map_key = map_slot_key(map.map_id);
    std::ostringstream text_source;
    std::ostringstream interaction_source;
    text_source << "; ROM-decoded presentation text directly owned by " << map_key << ".\n";
    interaction_source << "; ROM-decoded gameplay interactions directly owned by " << map_key
                       << ".\n";
    bool has_text = false;
    for (std::size_t index = 0; index < map.owned_entries.size(); ++index) {
        const std::size_t text_offset = map.text_entry_offsets[index];
        const std::uint8_t text_bank = source_bank(map.bank, text_offset);
        const DecodedTextProgram& program = map.owned_entries[index];
        has_text = has_text || !program.interaction;

        if (!program.interaction) {
            text_source << "\ntext " << owned_entry_key(map, index, "text") << '\n'
                        << "    entry_source " << source_address(text_bank, text_offset) << '\n'
                        << "    decoded_bytes " << program.source_bytes << '\n';
        }
        interaction_source << "\nscript " << owned_entry_key(map, index, "interaction") << '\n'
                           << "    entry_source " << source_address(text_bank, text_offset) << '\n'
                           << "    decoded_bytes " << program.source_bytes << '\n';
        if (!program.complete) {
            interaction_source << "    status unresolved\n"
                               << "    reason " << program.unresolved_reason << '\n'
                               << program.operations;
            ++result.unresolved_owned_entries;
        } else if (program.dynamic) {
            interaction_source << "    status interaction_script_untranslated\n"
                               << program.operations;
            ++result.untranslated_interaction_scripts;
        } else if (program.interaction) {
            interaction_source << "    status decoded\n" << program.operations;
            ++result.decoded_interaction_scripts;
        } else {
            text_source << "    status decoded\n" << program.operations;
            interaction_source << "    status decoded\n"
                               << "    show_text " << owned_entry_key(map, index, "text") << '\n';
            ++result.decoded_text_programs;
            ++result.decoded_interaction_scripts;
        }
    }
    if (has_text)
        add_text_file(result, "source/text/maps/" + map_key + ".sexpr", text_source.str());
    add_text_file(result, "source/scripts/interactions/" + map_key + ".sexpr",
                  interaction_source.str());
}

void emit_reports(const std::vector<MapProgramInventory>& maps, ScriptImport& result) {
    std::ostringstream summary;
    summary << "Pokemon Red map-program import inventory\n"
            << "map_slots " << result.map_slots << '\n'
            << "decoded_maps " << result.decoded_maps << '\n'
            << "header_aliases " << result.aliases << '\n'
            << "unresolved_slots " << result.unresolved_slots << '\n'
            << "script_entry_points " << result.script_entry_points << '\n'
            << "owned_map_entries " << result.owned_map_entries << '\n'
            << "decoded_text_programs " << result.decoded_text_programs << '\n'
            << "decoded_interaction_scripts " << result.decoded_interaction_scripts << '\n'
            << "untranslated_interaction_scripts " << result.untranslated_interaction_scripts
            << '\n'
            << "unresolved_owned_entries " << result.unresolved_owned_entries << '\n'
            << "background_interactions " << result.background_interactions << '\n'
            << "actor_interactions " << result.actor_interactions << '\n'
            << "semantic_scripts_translated 0\n"
            << "coverage_note every ROM map slot is classified; decoded routines remain queued "
               "for semantic lifting\n";
    add_text_file(result, "reports/script_import_summary.txt", summary.str());

    std::ostringstream unresolved;
    unresolved << "Map script translation queue\n"
               << "Decoded entries are machine-code entry points, not executable campaign ISA.\n"
               << "Text entries referenced only from inside those routines may be discovered "
                  "during semantic lifting.\n\n";
    for (const MapProgramInventory& map : maps) {
        unresolved << map_slot_key(map.map_id) << ' ';
        if (!map.decoded) {
            unresolved << "slot_unresolved " << map.unresolved_reason;
        } else if (map.alias_of != kMapSlotCount) {
            unresolved << "header_alias " << map_slot_key(map.alias_of);
        } else {
            unresolved << "campaign_script_untranslated "
                       << source_address(map.bank, map.script_offset);
        }
        unresolved << '\n';
    }
    add_text_file(result, "reports/unresolved_scripts.txt", unresolved.str());
}

void emit_compiled_index(const std::vector<MapProgramInventory>& maps, ScriptImport& result) {
    std::vector<std::uint8_t> bytes{'P', 'M', 'S', '1'};
    write_u16(bytes, maps.size());
    for (const MapProgramInventory& map : maps) {
        bytes.push_back(map.map_id);
        bytes.push_back(map.decoded ? 1U : 0U);
        bytes.push_back(map.bank);
        bytes.push_back(map.alias_of == kMapSlotCount ? 0xFFU
                                                      : static_cast<std::uint8_t>(map.alias_of));
        write_u32(bytes, map.header_offset);
        write_u32(bytes, map.script_offset);
        write_u32(bytes, map.text_offset);
        write_u32(bytes, map.objects_offset);
        write_u16(bytes, map.text_entry_offsets.size());
        write_u16(bytes, map.backgrounds.size());
        write_u16(bytes, map.actors.size());
    }
    result.files.push_back({"compiled/map_program_index.bin", std::move(bytes)});
}

} // namespace

bool decode_script_import(std::span<const std::uint8_t> rom, ScriptImport& result,
                          std::string& error) {
    result = {};
    if (!verify_pokemon_red_us_rev_0(rom, error)) return false;

    std::vector<MapProgramInventory> maps(kMapSlotCount);
    for (std::size_t map_id = 0; map_id < maps.size(); ++map_id)
        decode_map_program(rom, map_id, maps[map_id]);
    assign_aliases(maps);

    result.map_slots = maps.size();
    for (const MapProgramInventory& map : maps) {
        emit_map_source(map, result);
        emit_owned_entry_sources(map, result);
        if (!map.decoded) {
            ++result.unresolved_slots;
            continue;
        }
        ++result.decoded_maps;
        ++result.script_entry_points;
        if (map.alias_of != kMapSlotCount) ++result.aliases;
        result.owned_map_entries += map.text_entry_offsets.size();
        result.background_interactions += map.backgrounds.size();
        result.actor_interactions += map.actors.size();
    }
    if (result.decoded_maps != kExpectedDecodedMapCount ||
        result.unresolved_slots != kExpectedUnusedMapCount) {
        error = "map-program inventory does not match the pinned ROM profile";
        result = {};
        return false;
    }
    emit_reports(maps, result);
    emit_compiled_index(maps, result);
    return true;
}

} // namespace pokered::import
