#include "import_rules.hpp"

#include "import_rules_internal.hpp"
#include "rules.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pokered::import {
namespace {

constexpr std::size_t kMovesOffset = 0x38000;
constexpr std::size_t kMoveSize = 6;
constexpr std::size_t kMoveCount = 165;
constexpr std::size_t kMoveNamesOffset = 0xB0000;
constexpr std::size_t kMoveNamesEnd = 0xB060F;

constexpr std::size_t kBaseStatsOffset = 0x383DE;
constexpr std::size_t kBaseStatsSize = 28;
constexpr std::size_t kMainSpeciesCount = 150;
constexpr std::size_t kMewBaseStatsOffset = 0x0425B;
constexpr std::size_t kSpeciesCount = 151;
constexpr std::size_t kMonsterNamesOffset = 0x1C21E;
constexpr std::size_t kMonsterNameSize = 10;
constexpr std::size_t kInternalSpeciesCount = 190;
constexpr std::size_t kPokedexOrderOffset = 0x41024;

constexpr std::size_t kTypeNamesOffset = 0x27DAE;
constexpr std::size_t kTypeNamesEnd = 0x27E4A;
constexpr std::size_t kTypeCount = 27;
constexpr std::size_t kSpecialTypeStart = 20;
constexpr std::size_t kTypeMatchupsOffset = 0x3E474;
constexpr std::size_t kTypeMatchupsEnd = 0x3E56B;
constexpr std::size_t kTypeMatchupCount = 82;

constexpr std::size_t kEvolutionPointerTableOffset = 0x3B05C;
constexpr std::size_t kEvolutionDataOffset = 0x3B1D8;
constexpr std::size_t kEvolutionDataEnd = 0x3B9EC;
constexpr std::size_t kGrowthCurveCount = 6;

constexpr std::size_t kMachineMovesOffset = 0x13773;
constexpr std::size_t kMachineCount = 55;

struct InternalSpecies {
    std::uint8_t id{};
    std::uint8_t dex{};
    std::string name;
};

bool has_range(std::span<const std::uint8_t> rom, std::size_t offset, std::size_t size) {
    return offset <= rom.size() && size <= rom.size() - offset;
}

std::uint16_t read_u16(std::span<const std::uint8_t> rom, std::size_t offset) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(rom[offset]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(rom[offset + 1U]) << 8U));
}

bool bank_pointer_to_offset(std::uint8_t bank, std::uint16_t pointer, std::size_t& offset) {
    if (pointer < 0x4000U || pointer >= 0x8000U) return false;
    offset = static_cast<std::size_t>(bank) * 0x4000U + static_cast<std::size_t>(pointer - 0x4000U);
    return true;
}

bool append_glyph(std::uint8_t value, std::string& text) {
    if (value >= 0x80U && value <= 0x99U) {
        text.push_back(static_cast<char>('A' + value - 0x80U));
        return true;
    }
    if (value >= 0xA0U && value <= 0xB9U) {
        text.push_back(static_cast<char>('a' + value - 0xA0U));
        return true;
    }
    if (value >= 0xF6U) {
        text.push_back(static_cast<char>('0' + value - 0xF6U));
        return true;
    }
    switch (value) {
    case 0x7F:
        text.push_back(' ');
        return true;
    case 0xBA:
        text += "é";
        return true;
    case 0xE0:
        text.push_back('\'');
        return true;
    case 0xE3:
        text.push_back('-');
        return true;
    case 0xE4:
        text += "'r";
        return true;
    case 0xE5:
        text += "'m";
        return true;
    case 0xE6:
        text.push_back('?');
        return true;
    case 0xE7:
        text.push_back('!');
        return true;
    case 0xE8:
        text.push_back('.');
        return true;
    case 0xEF:
        text += "♂";
        return true;
    case 0xF2:
        text.push_back('.');
        return true;
    case 0xF4:
        text.push_back(',');
        return true;
    case 0xF5:
        text += "♀";
        return true;
    default:
        return false;
    }
}

bool decode_fixed_name(std::span<const std::uint8_t> rom, std::size_t offset, std::size_t size,
                       std::string& result, std::string& error) {
    if (!has_range(rom, offset, size)) {
        error = "fixed Pokemon text extends outside the verified ROM";
        return false;
    }
    result.clear();
    for (std::size_t index = 0; index < size; ++index) {
        const std::uint8_t value = rom[offset + index];
        if (value == 0x50U) return !result.empty();
        if (!append_glyph(value, result)) {
            error = "fixed Pokemon text contains an unsupported glyph";
            return false;
        }
    }
    while (!result.empty() && result.back() == ' ')
        result.pop_back();
    if (result.empty()) error = "fixed Pokemon text is empty";
    return !result.empty();
}

bool decode_terminated_name(std::span<const std::uint8_t> rom, std::size_t& cursor,
                            std::size_t limit, std::string& result, std::string& error) {
    result.clear();
    while (cursor < limit && cursor < rom.size()) {
        const std::uint8_t value = rom[cursor++];
        if (value == 0x50U) {
            if (result.empty()) error = "terminated Pokemon text is empty";
            return !result.empty();
        }
        if (!append_glyph(value, result)) {
            error = "terminated Pokemon text contains an unsupported glyph";
            return false;
        }
    }
    error = "terminated Pokemon text is missing its terminator";
    return false;
}

std::string snake_key(std::string_view name) {
    std::string key;
    bool separator = false;
    for (std::size_t index = 0; index < name.size(); ++index) {
        const unsigned char byte = static_cast<unsigned char>(name[index]);
        if (byte < 0x80U && std::isalnum(byte) != 0) {
            if (separator && !key.empty()) key.push_back('_');
            key.push_back(static_cast<char>(std::tolower(byte)));
            separator = false;
        } else if (name.substr(index, 3) == "♂") {
            if (!key.empty() && key.back() != '_') key.push_back('_');
            key += "male";
            index += 2U;
            separator = true;
        } else if (name.substr(index, 3) == "♀") {
            if (!key.empty() && key.back() != '_') key.push_back('_');
            key += "female";
            index += 2U;
            separator = true;
        } else {
            separator = true;
        }
    }
    if (key.empty()) key = "unnamed";
    return key;
}

std::string unique_key(std::string_view name, std::uint32_t id, std::set<std::string>& used) {
    std::string key = snake_key(name);
    if (used.insert(key).second) return key;
    key += "_slot_" + std::to_string(id);
    used.insert(key);
    return key;
}

bool decode_types(std::span<const std::uint8_t> rom, std::vector<TypeRule>& types,
                  std::string& error) {
    if (!has_range(rom, kTypeNamesOffset, kTypeCount * 2U) || !has_range(rom, kTypeNamesEnd, 0)) {
        error = "type name tables extend outside the verified ROM";
        return false;
    }
    std::set<std::string> keys;
    types.reserve(kTypeCount);
    for (std::size_t index = 0; index < kTypeCount; ++index) {
        std::size_t name_offset = 0;
        if (!bank_pointer_to_offset(9, read_u16(rom, kTypeNamesOffset + index * 2U), name_offset)) {
            error = "type name table contains an invalid pointer";
            return false;
        }
        std::size_t cursor = name_offset;
        std::string name;
        if (!decode_terminated_name(rom, cursor, kTypeNamesEnd, name, error)) return false;
        types.push_back({
            .id = static_cast<std::uint8_t>(index),
            .key = unique_key(name, static_cast<std::uint32_t>(index), keys),
            .name = std::move(name),
            .damage_class =
                index < kSpecialTypeStart ? MoveDamageClass::physical : MoveDamageClass::special,
            .unused = index >= 9U && index < 20U,
        });
    }
    return true;
}

bool decode_type_interactions(std::span<const std::uint8_t> rom,
                              std::vector<TypeInteractionRule>& interactions, std::string& error) {
    if (!has_range(rom, kTypeMatchupsOffset, kTypeMatchupsEnd - kTypeMatchupsOffset)) {
        error = "type interaction table extends outside the verified ROM";
        return false;
    }
    std::size_t cursor = kTypeMatchupsOffset;
    while (cursor < kTypeMatchupsEnd && rom[cursor] != 0xFFU) {
        if (cursor + 3U > kTypeMatchupsEnd) {
            error = "type interaction table ends inside a record";
            return false;
        }
        const std::uint8_t attacking = rom[cursor++];
        const std::uint8_t defending = rom[cursor++];
        const std::uint8_t multiplier = rom[cursor++];
        if (attacking >= kTypeCount || defending >= kTypeCount ||
            (multiplier != 0U && multiplier != 5U && multiplier != 20U)) {
            error = "type interaction table contains an invalid record";
            return false;
        }
        interactions.push_back({attacking, defending, multiplier});
    }
    if (cursor + 1U != kTypeMatchupsEnd || rom[cursor] != 0xFFU ||
        interactions.size() != kTypeMatchupCount) {
        error = "type interaction table did not consume exactly 82 records";
        return false;
    }
    return true;
}

bool decode_moves(std::span<const std::uint8_t> rom, const std::vector<TypeRule>& types,
                  std::vector<MoveRule>& moves, std::string& error) {
    if (!has_range(rom, kMovesOffset, kMoveCount * kMoveSize) ||
        !has_range(rom, kMoveNamesOffset, kMoveNamesEnd - kMoveNamesOffset)) {
        error = "move tables extend outside the verified ROM";
        return false;
    }
    std::vector<std::string> names;
    names.reserve(kMoveCount);
    std::size_t name_cursor = kMoveNamesOffset;
    for (std::size_t index = 0; index < kMoveCount; ++index) {
        std::string name;
        if (!decode_terminated_name(rom, name_cursor, kMoveNamesEnd, name, error)) return false;
        names.push_back(std::move(name));
    }
    if (name_cursor != kMoveNamesEnd) {
        error = "move names did not consume their exact source range";
        return false;
    }

    std::set<std::string> keys;
    moves.reserve(kMoveCount);
    for (std::size_t index = 0; index < kMoveCount; ++index) {
        const std::size_t offset = kMovesOffset + index * kMoveSize;
        const std::uint8_t id = static_cast<std::uint8_t>(index + 1U);
        const std::uint8_t type = rom[offset + 3U];
        if (type >= types.size() || rom[offset] != id || rom[offset + 5U] == 0U ||
            rom[offset + 5U] > 40U) {
            error = "move table contains an invalid type, animation, or PP value";
            return false;
        }
        moves.push_back({
            .id = id,
            .key = unique_key(names[index], id, keys),
            .name = std::move(names[index]),
            .animation_id = rom[offset],
            .effect_id = rom[offset + 1U],
            .power = rom[offset + 2U],
            .type_id = type,
            .accuracy_raw = rom[offset + 4U],
            .pp = rom[offset + 5U],
            .damage_class = types[type].damage_class,
        });
    }
    return true;
}

bool decode_internal_species(std::span<const std::uint8_t> rom,
                             std::vector<InternalSpecies>& internal,
                             std::array<std::uint8_t, kSpeciesCount>& internal_by_dex,
                             std::string& error) {
    if (!has_range(rom, kMonsterNamesOffset, kInternalSpeciesCount * kMonsterNameSize) ||
        !has_range(rom, kPokedexOrderOffset, kInternalSpeciesCount)) {
        error = "internal species tables extend outside the verified ROM";
        return false;
    }
    internal.reserve(kInternalSpeciesCount);
    for (std::size_t index = 0; index < kInternalSpeciesCount; ++index) {
        std::string name;
        if (!decode_fixed_name(rom, kMonsterNamesOffset + index * kMonsterNameSize,
                               kMonsterNameSize, name, error))
            return false;
        const std::uint8_t dex = rom[kPokedexOrderOffset + index];
        if (dex > kSpeciesCount) {
            error = "internal species table contains an invalid Pokedex number";
            return false;
        }
        const std::uint8_t id = static_cast<std::uint8_t>(index + 1U);
        internal.push_back({id, dex, std::move(name)});
        if (dex != 0U) {
            if (internal_by_dex[dex - 1U] != 0U) {
                error = "more than one internal species slot resolves a Pokedex species";
                return false;
            }
            internal_by_dex[dex - 1U] = id;
        }
    }
    if (std::ranges::any_of(internal_by_dex, [](std::uint8_t id) { return id == 0U; })) {
        error = "internal species table does not resolve all 151 species";
        return false;
    }
    return true;
}

bool decode_species(std::span<const std::uint8_t> rom, const std::vector<InternalSpecies>& internal,
                    const std::array<std::uint8_t, kSpeciesCount>& internal_by_dex,
                    const std::vector<TypeRule>& types, const std::vector<MoveRule>& moves,
                    std::vector<SpeciesRule>& species, std::string& error) {
    if (!has_range(rom, kBaseStatsOffset, kMainSpeciesCount * kBaseStatsSize) ||
        !has_range(rom, kMewBaseStatsOffset, kBaseStatsSize)) {
        error = "base-stat tables extend outside the verified ROM";
        return false;
    }
    std::set<std::string> keys;
    species.reserve(kSpeciesCount);
    for (std::size_t index = 0; index < kSpeciesCount; ++index) {
        const std::size_t offset = index < kMainSpeciesCount
                                       ? kBaseStatsOffset + index * kBaseStatsSize
                                       : kMewBaseStatsOffset;
        const std::uint8_t dex = static_cast<std::uint8_t>(index + 1U);
        const std::uint8_t internal_id = internal_by_dex[index];
        const InternalSpecies& identity = internal[internal_id - 1U];
        if (rom[offset] != dex || identity.dex != dex || rom[offset + 6U] >= types.size() ||
            rom[offset + 7U] >= types.size() || rom[offset + 19U] >= kGrowthCurveCount) {
            error = "base-stat record has an invalid identity, type, or growth curve";
            return false;
        }
        SpeciesRule record{
            .dex_number = dex,
            .internal_id = internal_id,
            .key = unique_key(identity.name, dex, keys),
            .name = identity.name,
            .base_hp = rom[offset + 1U],
            .base_attack = rom[offset + 2U],
            .base_defense = rom[offset + 3U],
            .base_speed = rom[offset + 4U],
            .base_special = rom[offset + 5U],
            .type_ids = {rom[offset + 6U], rom[offset + 7U]},
            .catch_rate = rom[offset + 8U],
            .experience_yield = rom[offset + 9U],
            .starting_move_ids = {},
            .growth_curve_id = rom[offset + 19U],
            .machine_compatibility = {},
        };
        for (std::size_t move = 0; move < record.starting_move_ids.size(); ++move) {
            record.starting_move_ids[move] = rom[offset + 15U + move];
            if (record.starting_move_ids[move] > moves.size()) {
                error = "species starting move is outside the move table";
                return false;
            }
        }
        std::copy_n(rom.begin() + static_cast<std::ptrdiff_t>(offset + 20U),
                    record.machine_compatibility.size(), record.machine_compatibility.begin());
        species.push_back(std::move(record));
    }
    return true;
}

bool take_progression_byte(std::span<const std::uint8_t> rom, std::size_t& cursor,
                           std::uint8_t& result, std::string& error) {
    if (cursor >= kEvolutionDataEnd || cursor >= rom.size()) {
        error = "evolution/learnset record crosses its proven ROM boundary";
        return false;
    }
    result = rom[cursor++];
    return true;
}

bool decode_progression(std::span<const std::uint8_t> rom,
                        const std::vector<InternalSpecies>& internal,
                        std::vector<LearnsetRule>& learnsets,
                        std::vector<EvolutionRule>& evolutions, std::string& error) {
    if (!has_range(rom, kEvolutionPointerTableOffset, kInternalSpeciesCount * 2U) ||
        !has_range(rom, kEvolutionDataOffset, kEvolutionDataEnd - kEvolutionDataOffset)) {
        error = "evolution and learnset tables extend outside the verified ROM";
        return false;
    }
    std::array<std::uint16_t, kSpeciesCount> orders{};
    for (std::size_t index = 0; index < internal.size(); ++index) {
        std::size_t cursor = 0;
        if (!bank_pointer_to_offset(14, read_u16(rom, kEvolutionPointerTableOffset + index * 2U),
                                    cursor) ||
            cursor < kEvolutionDataOffset || cursor >= kEvolutionDataEnd) {
            error = "evolution pointer table contains an out-of-range pointer";
            return false;
        }
        const InternalSpecies& source = internal[index];
        while (true) {
            std::uint8_t method = 0;
            if (!take_progression_byte(rom, cursor, method, error)) return false;
            if (method == 0U) break;
            if (method < 1U || method > 3U) {
                error = "evolution table contains an unknown method";
                return false;
            }
            std::uint8_t parameter = 0;
            std::uint8_t minimum_level = 0;
            std::uint8_t target_internal = 0;
            if (method == 2U) {
                if (!take_progression_byte(rom, cursor, parameter, error) ||
                    !take_progression_byte(rom, cursor, minimum_level, error) ||
                    !take_progression_byte(rom, cursor, target_internal, error))
                    return false;
            } else {
                if (!take_progression_byte(rom, cursor, minimum_level, error) ||
                    !take_progression_byte(rom, cursor, target_internal, error))
                    return false;
                parameter = method == 1U ? minimum_level : 0U;
            }
            if (minimum_level == 0U || minimum_level > 100U || target_internal == 0U ||
                target_internal > internal.size()) {
                error = "evolution table contains an invalid level or target";
                return false;
            }
            const std::uint8_t target_dex = internal[target_internal - 1U].dex;
            if (source.dex != 0U) {
                if (target_dex == 0U) {
                    error = "canonical species evolution targets a missing internal slot";
                    return false;
                }
                evolutions.push_back({
                    .species_dex = source.dex,
                    .target_species_dex = target_dex,
                    .method = static_cast<EvolutionMethod>(method - 1U),
                    .parameter = parameter,
                    .minimum_level = minimum_level,
                });
            }
        }

        std::uint8_t previous_level = 0;
        while (true) {
            std::uint8_t level = 0;
            if (!take_progression_byte(rom, cursor, level, error)) return false;
            if (level == 0U) break;
            std::uint8_t move = 0;
            if (!take_progression_byte(rom, cursor, move, error)) return false;
            if (level > 100U || level < previous_level || move == 0U || move > kMoveCount) {
                error = "learnset table contains an invalid level, order, or move";
                return false;
            }
            if (source.dex != 0U) {
                learnsets.push_back({
                    .species_dex = source.dex,
                    .level = level,
                    .move_id = move,
                    .order = orders[source.dex - 1U]++,
                });
            }
            previous_level = level;
        }
    }
    return true;
}

std::uint32_t growth_experience(std::uint8_t curve, std::uint8_t level) {
    const std::int64_t n = level;
    const std::int64_t cube = n * n * n;
    const std::int64_t square = n * n;
    std::int64_t value = 0;
    switch (curve) {
    case 0:
        value = cube;
        break;
    case 1:
        value = 3 * cube / 4 + 10 * square - 30;
        break;
    case 2:
        value = 3 * cube / 4 + 20 * square - 70;
        break;
    case 3:
        value = 6 * cube / 5 - 15 * square + 100 * n - 140;
        break;
    case 4:
        value = 4 * cube / 5;
        break;
    case 5:
        value = 5 * cube / 4;
        break;
    default:
        return 0;
    }
    return static_cast<std::uint32_t>(std::max<std::int64_t>(value, 0));
}

std::vector<GrowthCurveRule> build_growth_curves() {
    constexpr std::array<std::string_view, kGrowthCurveCount> keys{
        "medium_fast", "slightly_fast", "slightly_slow", "medium_slow", "fast", "slow",
    };
    std::vector<GrowthCurveRule> result;
    result.reserve(keys.size());
    for (std::size_t index = 0; index < keys.size(); ++index) {
        GrowthCurveRule curve;
        curve.id = static_cast<std::uint8_t>(index);
        curve.key = keys[index];
        for (std::size_t level = 1; level <= curve.experience_by_level.size(); ++level) {
            curve.experience_by_level[level - 1U] =
                growth_experience(curve.id, static_cast<std::uint8_t>(level));
        }
        result.push_back(std::move(curve));
    }
    return result;
}

bool decode_machines(std::span<const std::uint8_t> rom, std::vector<MachineRule>& machines,
                     std::string& error) {
    if (!has_range(rom, kMachineMovesOffset, kMachineCount)) {
        error = "machine move table extends outside the verified ROM";
        return false;
    }
    machines.reserve(kMachineCount);
    for (std::size_t index = 0; index < kMachineCount; ++index) {
        const std::uint8_t move = rom[kMachineMovesOffset + index];
        if (move == 0U || move > kMoveCount) {
            error = "machine move table contains an invalid move";
            return false;
        }
        const bool hidden = index >= 50U;
        machines.push_back({
            .index = static_cast<std::uint8_t>(index),
            .number = static_cast<std::uint8_t>(hidden ? index - 49U : index + 1U),
            .move_id = move,
            .hidden_machine = hidden,
        });
    }
    return true;
}

} // namespace

bool decode_rule_import(std::span<const std::uint8_t> rom, RuleImport& result, std::string& error) {
    result = {};
    if (!verify_pokemon_red_us_rev_0(rom, error)) return false;

    std::vector<TypeRule> types;
    std::vector<TypeInteractionRule> interactions;
    std::vector<MoveRule> moves;
    std::vector<InternalSpecies> internal;
    std::array<std::uint8_t, kSpeciesCount> internal_by_dex{};
    std::vector<SpeciesRule> species;
    std::vector<LearnsetRule> learnsets;
    std::vector<EvolutionRule> evolutions;
    std::vector<GrowthCurveRule> growth_curves = build_growth_curves();
    std::vector<MachineRule> machines;

    if (!decode_types(rom, types, error) || !decode_type_interactions(rom, interactions, error) ||
        !decode_moves(rom, types, moves, error) ||
        !decode_internal_species(rom, internal, internal_by_dex, error) ||
        !decode_species(rom, internal, internal_by_dex, types, moves, species, error) ||
        !decode_progression(rom, internal, learnsets, evolutions, error) ||
        !decode_machines(rom, machines, error))
        return false;
    if (learnsets.size() != 728U || evolutions.size() != 72U) {
        error = "progression import did not produce the complete 728 learnsets and 72 evolutions";
        return false;
    }
    std::ranges::sort(learnsets, [](const LearnsetRule& left, const LearnsetRule& right) {
        if (left.species_dex != right.species_dex) return left.species_dex < right.species_dex;
        return left.order < right.order;
    });
    std::ranges::sort(evolutions, [](const EvolutionRule& left, const EvolutionRule& right) {
        if (left.species_dex != right.species_dex) return left.species_dex < right.species_dex;
        return left.target_species_dex < right.target_species_dex;
    });

    emit_rule_import(types, interactions, moves, species, learnsets, evolutions, growth_curves,
                     machines, result);
    result.types = types.size();
    result.type_interactions = interactions.size();
    result.species = species.size();
    result.internal_species_slots = internal.size();
    result.moves = moves.size();
    result.learnset_entries = learnsets.size();
    result.evolutions = evolutions.size();
    result.growth_curves = growth_curves.size();
    result.machines = machines.size();
    error.clear();
    return true;
}

} // namespace pokered::import
