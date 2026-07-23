#include "import_rules_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pokered::import {
namespace {

constexpr std::size_t kMovesOffset = 0x38000;
constexpr std::size_t kMoveSize = 6;
constexpr std::size_t kMoveCount = 165;
constexpr std::size_t kBaseStatsOffset = 0x383DE;
constexpr std::size_t kBaseStatsSize = 28;
constexpr std::size_t kMainSpeciesCount = 150;
constexpr std::size_t kMewBaseStatsOffset = 0x0425B;
constexpr std::size_t kInternalSpeciesCount = 190;
constexpr std::size_t kTypeMatchupsOffset = 0x3E474;
constexpr std::size_t kTypeMatchupsEnd = 0x3E56B;
constexpr std::size_t kEvolutionDataOffset = 0x3B1D8;
constexpr std::size_t kEvolutionDataEnd = 0x3B9EC;

void write_u16(std::vector<std::uint8_t>& bytes, std::size_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void write_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void write_string(std::vector<std::uint8_t>& bytes, std::string_view value) {
    bytes.push_back(static_cast<std::uint8_t>(value.size()));
    bytes.insert(bytes.end(), value.begin(), value.end());
}

void add_file(RuleImport& result, std::string path, std::string text) {
    GeneratedFile file;
    file.relative_path = std::move(path);
    file.bytes.assign(text.begin(), text.end());
    result.files.push_back(std::move(file));
}

std::string source_range(std::size_t offset, std::size_t size) {
    return std::to_string(offset) + " " + std::to_string(size);
}

void emit_source(const std::vector<TypeRule>& types,
                 const std::vector<TypeInteractionRule>& interactions,
                 const std::vector<MoveRule>& moves, const std::vector<SpeciesRule>& species,
                 const std::vector<LearnsetRule>& learnsets,
                 const std::vector<EvolutionRule>& evolutions,
                 const std::vector<GrowthCurveRule>& growth_curves,
                 const std::vector<MachineRule>& machines, RuleImport& result) {
    std::ostringstream type_source;
    type_source << "; ROM-derived type slots. Unused slots remain addressable.\n";
    for (const TypeRule& type : types) {
        type_source << "\ntype " << type.key << '\n'
                    << "    rom_id " << static_cast<unsigned>(type.id) << '\n'
                    << "    name \"" << type.name << "\"\n"
                    << "    damage_class "
                    << (type.damage_class == MoveDamageClass::physical ? "physical" : "special")
                    << '\n'
                    << "    unused " << (type.unused ? "true" : "false") << '\n';
    }
    add_file(result, "source/rules/types.sexpr", type_source.str());

    std::ostringstream matchup_source;
    matchup_source << "; Sparse type chart; omitted pairs have multiplier 1/1.\n";
    for (const TypeInteractionRule& interaction : interactions) {
        const unsigned numerator = interaction.multiplier_tenths == 5U    ? 1U
                                   : interaction.multiplier_tenths == 20U ? 2U
                                                                          : 0U;
        const unsigned denominator = interaction.multiplier_tenths == 5U ? 2U : 1U;
        matchup_source << "\ntype_interaction " << types[interaction.attacking_type].key << ' '
                       << types[interaction.defending_type].key << '\n'
                       << "    multiplier " << numerator << ' ' << denominator << '\n';
    }
    add_file(result, "source/rules/type_interactions.sexpr", matchup_source.str());

    std::ostringstream move_source;
    move_source << "; ROM-derived move records. Effect IDs bind to separately lifted programs.\n";
    for (const MoveRule& move : moves) {
        move_source << "\nmove " << move.key << '\n'
                    << "    rom_id " << static_cast<unsigned>(move.id) << '\n'
                    << "    name \"" << move.name << "\"\n"
                    << "    type " << types[move.type_id].key << '\n'
                    << "    damage_class "
                    << (move.damage_class == MoveDamageClass::physical ? "physical" : "special")
                    << '\n'
                    << "    power " << static_cast<unsigned>(move.power) << '\n'
                    << "    accuracy_raw " << static_cast<unsigned>(move.accuracy_raw) << " 255\n"
                    << "    pp " << static_cast<unsigned>(move.pp) << '\n'
                    << "    effect effect_" << std::setfill('0') << std::setw(2) << std::hex
                    << static_cast<unsigned>(move.effect_id) << std::dec << '\n'
                    << "    animation battle_animation_" << static_cast<unsigned>(move.animation_id)
                    << '\n';
    }
    add_file(result, "source/rules/moves.sexpr", move_source.str());

    std::ostringstream species_source;
    species_source << "; Dex-ordered species joined through all 190 internal slots.\n";
    for (const SpeciesRule& record : species) {
        species_source << "\nspecies " << record.key << '\n'
                       << "    dex_number " << static_cast<unsigned>(record.dex_number) << '\n'
                       << "    internal_id " << static_cast<unsigned>(record.internal_id) << '\n'
                       << "    name \"" << record.name << "\"\n"
                       << "    base_stats " << static_cast<unsigned>(record.base_hp) << ' '
                       << static_cast<unsigned>(record.base_attack) << ' '
                       << static_cast<unsigned>(record.base_defense) << ' '
                       << static_cast<unsigned>(record.base_speed) << ' '
                       << static_cast<unsigned>(record.base_special) << '\n'
                       << "    types " << types[record.type_ids[0]].key << ' '
                       << types[record.type_ids[1]].key << '\n'
                       << "    catch_rate " << static_cast<unsigned>(record.catch_rate) << '\n'
                       << "    experience_yield " << static_cast<unsigned>(record.experience_yield)
                       << '\n'
                       << "    growth_curve " << growth_curves[record.growth_curve_id].key << '\n'
                       << "    starting_moves";
        for (const std::uint8_t move_id : record.starting_move_ids) {
            if (move_id != 0U) species_source << ' ' << moves[move_id - 1U].key;
        }
        species_source << "\n    machine_compatibility";
        for (std::size_t machine = 0; machine < machines.size(); ++machine) {
            const bool compatible = (record.machine_compatibility[machine / 8U] &
                                     static_cast<std::uint8_t>(1U << (machine % 8U))) != 0U;
            if (compatible)
                species_source << ' ' << (machines[machine].hidden_machine ? "hm_" : "tm_")
                               << static_cast<unsigned>(machines[machine].number);
        }
        species_source << '\n';
    }
    add_file(result, "source/rules/species.sexpr", species_source.str());

    std::ostringstream learnset_source;
    learnset_source << "; Ordered level-up move records.\n";
    for (const LearnsetRule& learn : learnsets) {
        learnset_source << "\nlearnset_entry " << species[learn.species_dex - 1U].key << '_'
                        << moves[learn.move_id - 1U].key << "_level_"
                        << static_cast<unsigned>(learn.level) << '\n'
                        << "    species " << species[learn.species_dex - 1U].key << '\n'
                        << "    level " << static_cast<unsigned>(learn.level) << '\n'
                        << "    move " << moves[learn.move_id - 1U].key << '\n'
                        << "    order " << learn.order << '\n';
    }
    add_file(result, "source/rules/learnsets.sexpr", learnset_source.str());

    std::ostringstream evolution_source;
    evolution_source << "; Level, item, and trade evolution records.\n";
    for (const EvolutionRule& evolution : evolutions) {
        const char* method = evolution.method == EvolutionMethod::level  ? "level"
                             : evolution.method == EvolutionMethod::item ? "item"
                                                                         : "trade";
        evolution_source << "\nevolution " << species[evolution.species_dex - 1U].key << "_to_"
                         << species[evolution.target_species_dex - 1U].key << '\n'
                         << "    from " << species[evolution.species_dex - 1U].key << '\n'
                         << "    to " << species[evolution.target_species_dex - 1U].key << '\n'
                         << "    method " << method << '\n'
                         << "    parameter " << static_cast<unsigned>(evolution.parameter) << '\n'
                         << "    minimum_level " << static_cast<unsigned>(evolution.minimum_level)
                         << '\n';
    }
    add_file(result, "source/rules/evolutions.sexpr", evolution_source.str());

    std::ostringstream growth_source;
    growth_source << "; Semantic growth routines materialized as level-indexed data.\n";
    for (const GrowthCurveRule& curve : growth_curves) {
        growth_source << "\ngrowth_curve " << curve.key << '\n'
                      << "    rom_id " << static_cast<unsigned>(curve.id) << '\n';
        for (std::size_t level = 1; level <= curve.experience_by_level.size(); ++level)
            growth_source << "    level " << level << " total_experience "
                          << curve.experience_by_level[level - 1U] << '\n';
    }
    add_file(result, "source/rules/growth_curves.sexpr", growth_source.str());

    std::ostringstream machine_source;
    machine_source << "; Machine-to-move table used by species compatibility bits.\n";
    for (const MachineRule& machine : machines) {
        machine_source << "\nmachine " << (machine.hidden_machine ? "hm_" : "tm_")
                       << static_cast<unsigned>(machine.number) << '\n'
                       << "    index " << static_cast<unsigned>(machine.index) << '\n'
                       << "    move " << moves[machine.move_id - 1U].key << '\n'
                       << "    reusable " << (machine.hidden_machine ? "true" : "false") << '\n';
    }
    add_file(result, "source/rules/machines.sexpr", machine_source.str());

    std::ostringstream report;
    report << "Pokemon Red immutable rule import\n"
           << "types " << types.size() << '\n'
           << "type_interactions " << interactions.size() << '\n'
           << "moves " << moves.size() << '\n'
           << "species " << species.size() << '\n'
           << "internal_species_slots " << kInternalSpeciesCount << '\n'
           << "learnset_entries " << learnsets.size() << '\n'
           << "evolutions " << evolutions.size() << '\n'
           << "growth_curves " << growth_curves.size() << '\n'
           << "machines " << machines.size() << '\n'
           << "moves_source " << source_range(kMovesOffset, kMoveCount * kMoveSize) << '\n'
           << "base_stats_source "
           << source_range(kBaseStatsOffset, kMainSpeciesCount * kBaseStatsSize) << '\n'
           << "mew_base_stats_source " << source_range(kMewBaseStatsOffset, kBaseStatsSize) << '\n'
           << "type_chart_source "
           << source_range(kTypeMatchupsOffset, kTypeMatchupsEnd - kTypeMatchupsOffset) << '\n'
           << "progression_source "
           << source_range(kEvolutionDataOffset, kEvolutionDataEnd - kEvolutionDataOffset) << '\n'
           << "semantic_move_effect_programs 0\n"
           << "coverage_note immutable identities, stats, relationships, and lookup tables are "
              "complete; battle formula/effect programs remain separately accountable\n";
    add_file(result, "reports/rule_import_summary.txt", report.str());
}

void emit_cache(const std::vector<TypeRule>& types,
                const std::vector<TypeInteractionRule>& interactions,
                const std::vector<MoveRule>& moves, const std::vector<SpeciesRule>& species,
                const std::vector<LearnsetRule>& learnsets,
                const std::vector<EvolutionRule>& evolutions,
                const std::vector<GrowthCurveRule>& growth_curves,
                const std::vector<MachineRule>& machines, RuleImport& result) {
    std::vector<std::uint8_t> bytes{'P', 'R', 'L', '1'};
    write_u16(bytes, types.size());
    for (const TypeRule& type : types) {
        bytes.push_back(type.id);
        write_string(bytes, type.key);
        write_string(bytes, type.name);
        bytes.push_back(static_cast<std::uint8_t>(type.damage_class));
        bytes.push_back(type.unused ? 1U : 0U);
    }
    write_u16(bytes, interactions.size());
    for (const TypeInteractionRule& interaction : interactions) {
        bytes.push_back(interaction.attacking_type);
        bytes.push_back(interaction.defending_type);
        bytes.push_back(interaction.multiplier_tenths);
    }
    write_u16(bytes, moves.size());
    for (const MoveRule& move : moves) {
        bytes.push_back(move.id);
        write_string(bytes, move.key);
        write_string(bytes, move.name);
        bytes.push_back(move.animation_id);
        bytes.push_back(move.effect_id);
        bytes.push_back(move.power);
        bytes.push_back(move.type_id);
        bytes.push_back(move.accuracy_raw);
        bytes.push_back(move.pp);
        bytes.push_back(static_cast<std::uint8_t>(move.damage_class));
    }
    write_u16(bytes, species.size());
    for (const SpeciesRule& record : species) {
        bytes.push_back(record.dex_number);
        bytes.push_back(record.internal_id);
        write_string(bytes, record.key);
        write_string(bytes, record.name);
        bytes.push_back(record.base_hp);
        bytes.push_back(record.base_attack);
        bytes.push_back(record.base_defense);
        bytes.push_back(record.base_speed);
        bytes.push_back(record.base_special);
        bytes.insert(bytes.end(), record.type_ids.begin(), record.type_ids.end());
        bytes.push_back(record.catch_rate);
        bytes.push_back(record.experience_yield);
        bytes.insert(bytes.end(), record.starting_move_ids.begin(), record.starting_move_ids.end());
        bytes.push_back(record.growth_curve_id);
        bytes.insert(bytes.end(), record.machine_compatibility.begin(),
                     record.machine_compatibility.end());
    }
    write_u16(bytes, learnsets.size());
    for (const LearnsetRule& learn : learnsets) {
        bytes.push_back(learn.species_dex);
        bytes.push_back(learn.level);
        bytes.push_back(learn.move_id);
        write_u16(bytes, learn.order);
    }
    write_u16(bytes, evolutions.size());
    for (const EvolutionRule& evolution : evolutions) {
        bytes.push_back(evolution.species_dex);
        bytes.push_back(evolution.target_species_dex);
        bytes.push_back(static_cast<std::uint8_t>(evolution.method));
        bytes.push_back(evolution.parameter);
        bytes.push_back(evolution.minimum_level);
    }
    write_u16(bytes, growth_curves.size());
    for (const GrowthCurveRule& curve : growth_curves) {
        bytes.push_back(curve.id);
        write_string(bytes, curve.key);
        for (const std::uint32_t experience : curve.experience_by_level)
            write_u32(bytes, experience);
    }
    write_u16(bytes, machines.size());
    for (const MachineRule& machine : machines) {
        bytes.push_back(machine.index);
        bytes.push_back(machine.number);
        bytes.push_back(machine.move_id);
        bytes.push_back(machine.hidden_machine ? 1U : 0U);
    }
    result.files.push_back({"compiled/pokemon_rules.bin", std::move(bytes)});
}

} // namespace

void emit_rule_import(const std::vector<TypeRule>& types,
                      const std::vector<TypeInteractionRule>& interactions,
                      const std::vector<MoveRule>& moves, const std::vector<SpeciesRule>& species,
                      const std::vector<LearnsetRule>& learnsets,
                      const std::vector<EvolutionRule>& evolutions,
                      const std::vector<GrowthCurveRule>& growth_curves,
                      const std::vector<MachineRule>& machines, RuleImport& result) {
    emit_source(types, interactions, moves, species, learnsets, evolutions, growth_curves, machines,
                result);
    emit_cache(types, interactions, moves, species, learnsets, evolutions, growth_curves, machines,
               result);
}

} // namespace pokered::import
