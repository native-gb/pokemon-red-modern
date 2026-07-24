#include "rules.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <set>
#include <string>
#include <utility>

namespace pokered {
namespace {

bool read_u8(std::istream& input, std::uint8_t& result) {
    char byte = 0;
    if (!input.get(byte)) return false;
    result = static_cast<std::uint8_t>(static_cast<unsigned char>(byte));
    return true;
}

bool read_u16(std::istream& input, std::uint16_t& result) {
    std::uint8_t low = 0;
    std::uint8_t high = 0;
    if (!read_u8(input, low) || !read_u8(input, high)) return false;
    result = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(low) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(high) << 8U));
    return true;
}

bool read_u32(std::istream& input, std::uint32_t& result) {
    std::uint8_t byte_0 = 0;
    std::uint8_t byte_1 = 0;
    std::uint8_t byte_2 = 0;
    std::uint8_t byte_3 = 0;
    if (!read_u8(input, byte_0) || !read_u8(input, byte_1) || !read_u8(input, byte_2) ||
        !read_u8(input, byte_3))
        return false;
    result = static_cast<std::uint32_t>(byte_0) | (static_cast<std::uint32_t>(byte_1) << 8U) |
             (static_cast<std::uint32_t>(byte_2) << 16U) |
             (static_cast<std::uint32_t>(byte_3) << 24U);
    return true;
}

bool read_string(std::istream& input, std::string& result) {
    std::uint8_t size = 0;
    if (!read_u8(input, size) || size == 0U) return false;
    result.resize(size);
    return static_cast<bool>(
        input.read(result.data(), static_cast<std::streamsize>(result.size())));
}

bool read_types(std::istream& input, RuleCatalog& rules) {
    std::uint16_t count = 0;
    if (!read_u16(input, count) || count != 27U) return false;
    rules.types.reserve(count);
    std::set<std::string> keys;
    for (std::uint16_t index = 0; index < count; ++index) {
        TypeRule type;
        std::uint8_t damage_class = 0;
        std::uint8_t unused = 0;
        if (!read_u8(input, type.id) || type.id != index || !read_string(input, type.key) ||
            !keys.insert(type.key).second || !read_string(input, type.name) ||
            !read_u8(input, damage_class) || damage_class > 1U || !read_u8(input, unused) ||
            unused > 1U)
            return false;
        type.damage_class = static_cast<MoveDamageClass>(damage_class);
        type.unused = unused != 0U;
        rules.types.push_back(std::move(type));
    }
    return true;
}

bool read_type_interactions(std::istream& input, RuleCatalog& rules) {
    std::uint16_t count = 0;
    if (!read_u16(input, count) || count != 82U) return false;
    rules.type_interactions.reserve(count);
    std::set<std::uint16_t> pairs;
    for (std::uint16_t index = 0; index < count; ++index) {
        TypeInteractionRule interaction;
        if (!read_u8(input, interaction.attacking_type) ||
            !read_u8(input, interaction.defending_type) ||
            !read_u8(input, interaction.multiplier_tenths) ||
            interaction.attacking_type >= rules.types.size() ||
            interaction.defending_type >= rules.types.size() ||
            (interaction.multiplier_tenths != 0U && interaction.multiplier_tenths != 5U &&
             interaction.multiplier_tenths != 20U))
            return false;
        const std::uint16_t pair =
            static_cast<std::uint16_t>(static_cast<std::uint16_t>(interaction.attacking_type)
                                       << 8U) |
            interaction.defending_type;
        if (!pairs.insert(pair).second) return false;
        rules.type_interactions.push_back(interaction);
    }
    return true;
}

bool read_moves(std::istream& input, RuleCatalog& rules) {
    std::uint16_t count = 0;
    if (!read_u16(input, count) || count != 165U) return false;
    rules.moves.reserve(count);
    std::set<std::string> keys;
    for (std::uint16_t index = 0; index < count; ++index) {
        MoveRule move;
        std::uint8_t damage_class = 0;
        if (!read_u8(input, move.id) || move.id != index + 1U || !read_string(input, move.key) ||
            !keys.insert(move.key).second || !read_string(input, move.name) ||
            !read_u8(input, move.animation_id) || !read_u8(input, move.effect_id) ||
            !read_u8(input, move.power) || !read_u8(input, move.type_id) ||
            move.type_id >= rules.types.size() || !read_u8(input, move.accuracy_raw) ||
            !read_u8(input, move.pp) || move.pp == 0U || move.pp > 40U ||
            !read_u8(input, damage_class) || damage_class > 1U)
            return false;
        move.damage_class = static_cast<MoveDamageClass>(damage_class);
        rules.moves.push_back(std::move(move));
    }
    return true;
}

bool read_species(std::istream& input, RuleCatalog& rules) {
    std::uint16_t count = 0;
    if (!read_u16(input, count) || count != 151U) return false;
    rules.species.reserve(count);
    std::set<std::string> keys;
    std::set<std::uint8_t> internal_ids;
    for (std::uint16_t index = 0; index < count; ++index) {
        SpeciesRule species;
        if (!read_u8(input, species.dex_number) || species.dex_number != index + 1U ||
            !read_u8(input, species.internal_id) || species.internal_id == 0U ||
            !internal_ids.insert(species.internal_id).second || !read_string(input, species.key) ||
            !keys.insert(species.key).second || !read_string(input, species.name) ||
            !read_u8(input, species.base_hp) || !read_u8(input, species.base_attack) ||
            !read_u8(input, species.base_defense) || !read_u8(input, species.base_speed) ||
            !read_u8(input, species.base_special) || !read_u8(input, species.type_ids[0]) ||
            species.type_ids[0] >= rules.types.size() || !read_u8(input, species.type_ids[1]) ||
            species.type_ids[1] >= rules.types.size() || !read_u8(input, species.catch_rate) ||
            !read_u8(input, species.experience_yield))
            return false;
        for (std::uint8_t& move : species.starting_move_ids) {
            if (!read_u8(input, move) || move > rules.moves.size()) return false;
        }
        if (!read_u8(input, species.growth_curve_id) || species.growth_curve_id >= 6U) return false;
        for (std::uint8_t& byte : species.machine_compatibility) {
            if (!read_u8(input, byte)) return false;
        }
        rules.species.push_back(std::move(species));
    }
    return true;
}

bool read_pokedex_entries(
    std::istream& input, RuleCatalog& rules) {
    std::uint16_t count = 0U;
    if (!read_u16(input, count) || count != 151U)
        return false;
    rules.pokedex_entries.reserve(count);
    for (std::uint16_t index = 0U;
         index < count; ++index) {
        PokedexEntryRule entry;
        if (!read_u8(input, entry.dex_number) ||
            entry.dex_number != index + 1U ||
            !read_string(input, entry.classification) ||
            !read_u8(input, entry.height_feet) ||
            !read_u8(input, entry.height_inches) ||
            entry.height_inches > 11U ||
            !read_u16(input, entry.weight_tenths_pounds) ||
            entry.weight_tenths_pounds == 0U)
            return false;
        for (std::string& line :
             entry.description_lines)
            if (!read_string(input, line))
                return false;
        rules.pokedex_entries.push_back(
            std::move(entry));
    }
    return true;
}

bool read_learnsets(std::istream& input, RuleCatalog& rules) {
    std::uint16_t count = 0;
    if (!read_u16(input, count) || count > 4096U) return false;
    rules.learnsets.reserve(count);
    std::array<std::uint16_t, 151> next_order{};
    for (std::uint16_t index = 0; index < count; ++index) {
        LearnsetRule learn;
        if (!read_u8(input, learn.species_dex) || learn.species_dex == 0U ||
            learn.species_dex > rules.species.size() || !read_u8(input, learn.level) ||
            learn.level == 0U || learn.level > 100U || !read_u8(input, learn.move_id) ||
            learn.move_id == 0U || learn.move_id > rules.moves.size() ||
            !read_u16(input, learn.order) || learn.order != next_order[learn.species_dex - 1U]++)
            return false;
        rules.learnsets.push_back(learn);
    }
    return true;
}

bool read_evolutions(std::istream& input, RuleCatalog& rules) {
    std::uint16_t count = 0;
    if (!read_u16(input, count) || count > 1024U) return false;
    rules.evolutions.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        EvolutionRule evolution;
        std::uint8_t method = 0;
        if (!read_u8(input, evolution.species_dex) || evolution.species_dex == 0U ||
            evolution.species_dex > rules.species.size() ||
            !read_u8(input, evolution.target_species_dex) || evolution.target_species_dex == 0U ||
            evolution.target_species_dex > rules.species.size() || !read_u8(input, method) ||
            method > 2U || !read_u8(input, evolution.parameter) ||
            !read_u8(input, evolution.minimum_level) || evolution.minimum_level == 0U ||
            evolution.minimum_level > 100U)
            return false;
        evolution.method = static_cast<EvolutionMethod>(method);
        rules.evolutions.push_back(evolution);
    }
    return true;
}

bool read_growth_curves(std::istream& input, RuleCatalog& rules) {
    std::uint16_t count = 0;
    if (!read_u16(input, count) || count != 6U) return false;
    rules.growth_curves.reserve(count);
    std::set<std::string> keys;
    for (std::uint16_t index = 0; index < count; ++index) {
        GrowthCurveRule curve;
        if (!read_u8(input, curve.id) || curve.id != index || !read_string(input, curve.key) ||
            !keys.insert(curve.key).second)
            return false;
        std::uint32_t previous = 0;
        for (std::uint32_t& experience : curve.experience_by_level) {
            if (!read_u32(input, experience) || experience < previous) return false;
            previous = experience;
        }
        rules.growth_curves.push_back(std::move(curve));
    }
    return true;
}

bool read_machines(std::istream& input, RuleCatalog& rules) {
    std::uint16_t count = 0;
    if (!read_u16(input, count) || count != 55U) return false;
    rules.machines.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        MachineRule machine;
        std::uint8_t hidden = 0;
        if (!read_u8(input, machine.index) || machine.index != index ||
            !read_u8(input, machine.number) || machine.number == 0U ||
            !read_u8(input, machine.move_id) || machine.move_id == 0U ||
            machine.move_id > rules.moves.size() || !read_u8(input, hidden) || hidden > 1U)
            return false;
        machine.hidden_machine = hidden != 0U;
        const std::uint8_t expected =
            static_cast<std::uint8_t>(machine.hidden_machine ? index - 49U : index + 1U);
        if (machine.number != expected || machine.hidden_machine != (index >= 50U)) return false;
        rules.machines.push_back(machine);
    }
    return true;
}

} // namespace

bool load_rules(const std::filesystem::path& path, RuleCatalog& result, std::string& error) {
    error.clear();
    std::ifstream input(path, std::ios::binary);
    std::array<char, 4> magic{};
    if (!input.read(magic.data(), static_cast<std::streamsize>(magic.size())) ||
        magic != std::array{'P', 'R', 'L', '2'}) {
        error = "Pokemon rule cache is missing or has an invalid header";
        return false;
    }

    RuleCatalog loaded;
    loaded.source = path;
    if (!read_types(input, loaded))
        error = "Pokemon rule cache has invalid types";
    else if (!read_type_interactions(input, loaded))
        error = "Pokemon rule cache has invalid type interactions";
    else if (!read_moves(input, loaded))
        error = "Pokemon rule cache has invalid moves";
    else if (!read_species(input, loaded))
        error = "Pokemon rule cache has invalid species";
    else if (!read_pokedex_entries(input, loaded))
        error = "Pokemon rule cache has invalid Pokedex entries";
    else if (!read_learnsets(input, loaded))
        error = "Pokemon rule cache has invalid learnsets";
    else if (!read_evolutions(input, loaded))
        error = "Pokemon rule cache has invalid evolutions";
    else if (!read_growth_curves(input, loaded))
        error = "Pokemon rule cache has invalid growth curves";
    else if (!read_machines(input, loaded))
        error = "Pokemon rule cache has invalid machines";
    if (!error.empty()) return false;
    if (input.peek() != std::char_traits<char>::eof()) {
        error = "Pokemon rule cache contains trailing data";
        return false;
    }
    loaded.loaded = true;
    result = std::move(loaded);
    error.clear();
    return true;
}

const TypeRule* find_type(const RuleCatalog& rules, std::uint8_t id) {
    if (id >= rules.types.size() || rules.types[id].id != id) return nullptr;
    return &rules.types[id];
}

const MoveRule* find_move(const RuleCatalog& rules, std::uint8_t id) {
    if (id == 0U || id > rules.moves.size() || rules.moves[id - 1U].id != id) return nullptr;
    return &rules.moves[id - 1U];
}

const SpeciesRule* find_species(const RuleCatalog& rules, std::uint8_t dex_number) {
    if (dex_number == 0U || dex_number > rules.species.size() ||
        rules.species[dex_number - 1U].dex_number != dex_number)
        return nullptr;
    return &rules.species[dex_number - 1U];
}

const PokedexEntryRule* find_pokedex_entry(
    const RuleCatalog& rules, std::uint8_t dex_number) {
    if (dex_number == 0U ||
        dex_number > rules.pokedex_entries.size() ||
        rules.pokedex_entries[dex_number - 1U].dex_number !=
            dex_number)
        return nullptr;
    return &rules.pokedex_entries[dex_number - 1U];
}

std::uint16_t type_multiplier_tenths(
    const RuleCatalog& rules, std::uint8_t attacking_type,
    const std::array<std::uint8_t, 2>& defending_types) {
    if (find_type(rules, attacking_type) == nullptr ||
        find_type(rules, defending_types[0]) == nullptr ||
        find_type(rules, defending_types[1]) == nullptr)
        return 0U;
    std::uint16_t result = 10U;
    for (std::size_t slot = 0; slot < defending_types.size(); ++slot) {
        if (slot != 0U && defending_types[slot] == defending_types[0]) continue;
        const auto found = std::find_if(
            rules.type_interactions.begin(), rules.type_interactions.end(),
            [&](const TypeInteractionRule& interaction) {
                return interaction.attacking_type == attacking_type &&
                       interaction.defending_type == defending_types[slot];
            });
        const std::uint16_t multiplier =
            found == rules.type_interactions.end() ? 10U
                                                   : found->multiplier_tenths;
        result = static_cast<std::uint16_t>((result * multiplier) / 10U);
    }
    return result;
}

bool species_can_learn_machine(const RuleCatalog& rules,
                               std::uint8_t species_dex,
                               std::uint8_t machine_index) {
    const SpeciesRule* species = find_species(rules, species_dex);
    if (species == nullptr || machine_index >= rules.machines.size()) return false;
    const MachineRule& machine = rules.machines[machine_index];
    if (machine.index != machine_index) return false;
    const std::size_t byte = static_cast<std::size_t>(machine.index) / 8U;
    const std::uint8_t mask =
        static_cast<std::uint8_t>(1U << (machine.index % 8U));
    return byte < species->machine_compatibility.size() &&
           (species->machine_compatibility[byte] & mask) != 0U;
}

std::vector<std::uint8_t> moves_learned_at_level(const RuleCatalog& rules,
                                                 std::uint8_t species_dex,
                                                 std::uint8_t level) {
    std::vector<std::uint8_t> result;
    if (find_species(rules, species_dex) == nullptr || level == 0U ||
        level > 100U)
        return result;
    for (const LearnsetRule& entry : rules.learnsets) {
        if (entry.species_dex == species_dex && entry.level == level)
            result.push_back(entry.move_id);
    }
    return result;
}

const EvolutionRule* eligible_evolution(
    const RuleCatalog& rules, std::uint8_t species_dex, std::uint8_t level,
    std::optional<std::uint8_t> item, bool traded) {
    const auto found = std::find_if(
        rules.evolutions.begin(), rules.evolutions.end(),
        [&](const EvolutionRule& evolution) {
            if (evolution.species_dex != species_dex ||
                level < evolution.minimum_level)
                return false;
            switch (evolution.method) {
            case EvolutionMethod::level:
                return !item.has_value() && !traded &&
                       level >= evolution.parameter;
            case EvolutionMethod::item:
                return item.has_value() && *item == evolution.parameter;
            case EvolutionMethod::trade:
                return traded;
            }
            return false;
        });
    return found == rules.evolutions.end() ? nullptr : &*found;
}

std::uint32_t experience_for_level(const RuleCatalog& rules, std::uint8_t growth_curve_id,
                                   std::uint8_t level) {
    if (growth_curve_id >= rules.growth_curves.size() || level == 0U || level > 100U) return 0;
    return rules.growth_curves[growth_curve_id].experience_by_level[level - 1U];
}

std::uint8_t level_for_experience(const RuleCatalog& rules,
                                  std::uint8_t growth_curve_id,
                                  std::uint32_t experience) {
    if (growth_curve_id >= rules.growth_curves.size()) return 0U;
    const auto& levels = rules.growth_curves[growth_curve_id].experience_by_level;
    const auto upper = std::upper_bound(levels.begin(), levels.end(), experience);
    const std::size_t count = static_cast<std::size_t>(upper - levels.begin());
    return static_cast<std::uint8_t>(std::clamp<std::size_t>(count, 1U, 100U));
}

} // namespace pokered
