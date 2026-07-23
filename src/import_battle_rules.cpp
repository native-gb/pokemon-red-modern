#include "import_battle_rules.hpp"

#include "battle_rules.hpp"

#include <array>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pokered::import {
namespace {

void write_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void write_string(std::vector<std::uint8_t>& bytes, std::string_view value) {
    write_u16(bytes, static_cast<std::uint16_t>(value.size()));
    bytes.insert(bytes.end(), value.begin(), value.end());
}

void add_text_file(BattleRuleImport& result, std::string path,
                   const std::string& text) {
    GeneratedFile file;
    file.relative_path = std::move(path);
    file.bytes.assign(text.begin(), text.end());
    result.files.push_back(std::move(file));
}

DamageFormulaInstruction instruction(
    DamageFormulaOpcode opcode, std::uint16_t a = 0U, std::uint16_t b = 0U,
    std::uint16_t c = 0U, std::uint16_t d = 0U) {
    return {.opcode = opcode, .operands = {a, b, c, d}};
}

CriticalHitInstruction critical_instruction(
    CriticalHitOpcode opcode, std::uint16_t a = 0U, std::uint16_t b = 0U,
    std::uint16_t c = 0U, std::uint16_t d = 0U) {
    return {.opcode = opcode, .operands = {a, b, c, d}};
}

DamageFormulaProgram lift_original_damage_formula() {
    DamageFormulaProgram program;
    program.key = "gen_1_original_damage";
    program.instructions = {
        instruction(DamageFormulaOpcode::halve_defense_for_effect, 0x07U, 2U,
                    1U),
        instruction(DamageFormulaOpcode::scale_wide_stats, 256U, 4U, 1U),
        instruction(DamageFormulaOpcode::multiply_level_if_critical, 2U),
        instruction(DamageFormulaOpcode::calculate_base_damage, 2U, 5U, 2U,
                    50U),
        instruction(DamageFormulaOpcode::cap_and_add, 997U, 2U, 999U),
        instruction(DamageFormulaOpcode::same_type_bonus, 3U, 2U),
        instruction(DamageFormulaOpcode::type_effectiveness),
        instruction(DamageFormulaOpcode::random_factor, 217U, 255U, 255U, 1U),
    };
    return program;
}

bool lift_original_critical_hit_program(
    std::span<const std::uint8_t> rom, CriticalHitProgram& program,
    std::string& error) {
    constexpr std::size_t table_offset = 0x03E08EU;
    constexpr std::size_t move_count = 4U;
    if (table_offset + move_count >= rom.size() ||
        rom[table_offset + move_count] != 0xFFU) {
        error = "high-critical move table is outside its verified source range";
        return false;
    }
    program.key = "gen_1_original_critical_hits";
    program.high_critical_moves.assign(
        rom.begin() + static_cast<std::ptrdiff_t>(table_offset),
        rom.begin() + static_cast<std::ptrdiff_t>(table_offset + move_count));
    program.instructions = {
        critical_instruction(CriticalHitOpcode::base_speed_fraction, 1U, 2U),
        critical_instruction(CriticalHitOpcode::focus_energy_ratio, 2U, 1U,
                             1U, 2U),
        critical_instruction(CriticalHitOpcode::move_ratio, 1U, 2U, 4U, 1U),
        critical_instruction(CriticalHitOpcode::rotate_random_left, 3U),
        critical_instruction(CriticalHitOpcode::compare_less),
    };
    return true;
}

void emit_instruction(std::ostringstream& source,
                      const DamageFormulaInstruction& instruction) {
    const auto& value = instruction.operands;
    switch (instruction.opcode) {
    case DamageFormulaOpcode::halve_defense_for_effect:
        source << "    halve_defense_for_effect\n"
               << "        effect_id " << value[0] << '\n'
               << "        divisor " << value[1] << '\n'
               << "        minimum " << value[2] << '\n';
        break;
    case DamageFormulaOpcode::scale_wide_stats:
        source << "    scale_wide_stats\n"
               << "        threshold " << value[0] << '\n'
               << "        divisor " << value[1] << '\n'
               << "        minimum_attack " << value[2] << '\n';
        break;
    case DamageFormulaOpcode::multiply_level_if_critical:
        source << "    multiply_level_if_critical " << value[0] << '\n';
        break;
    case DamageFormulaOpcode::calculate_base_damage:
        source << "    calculate_base_damage\n"
               << "        level_numerator " << value[0] << '\n'
               << "        level_denominator " << value[1] << '\n'
               << "        level_add " << value[2] << '\n'
               << "        final_divisor " << value[3] << '\n';
        break;
    case DamageFormulaOpcode::cap_and_add:
        source << "    cap_and_add\n"
               << "        pre_add_cap " << value[0] << '\n'
               << "        add " << value[1] << '\n'
               << "        result_cap " << value[2] << '\n';
        break;
    case DamageFormulaOpcode::same_type_bonus:
        source << "    same_type_bonus " << value[0] << ' ' << value[1]
               << '\n';
        break;
    case DamageFormulaOpcode::type_effectiveness:
        source << "    type_effectiveness\n";
        break;
    case DamageFormulaOpcode::random_factor:
        source << "    random_factor\n"
               << "        minimum " << value[0] << '\n'
               << "        maximum " << value[1] << '\n'
               << "        divisor " << value[2] << '\n'
               << "        rotate_right " << value[3] << '\n';
        break;
    }
}

void emit_source(const DamageFormulaProgram& program,
                 const CriticalHitProgram& critical,
                 BattleRuleImport& result) {
    std::ostringstream source;
    source << "; Semantic lift from the verified cartridge's damage routines.\n"
           << "; Stats 0x03ddcf..0x03df65; base 0x03df65..0x03e023;\n"
           << "; type 0x03e3a5..0x03e449; random 0x03e687.\n\n"
           << "damage_formula " << program.key << '\n';
    for (const DamageFormulaInstruction& value : program.instructions)
        emit_instruction(source, value);
    add_text_file(result, "source/battle_effects/damage.sexpr",
                  source.str());

    std::ostringstream critical_source;
    critical_source
        << "; Semantic lift from 0x03e023..0x03e093, including the\n"
        << "; cartridge's Focus Energy ratio and high-critical move table.\n\n"
        << "critical_hit_program " << critical.key << '\n'
        << "    high_critical_moves";
    for (const std::uint8_t move : critical.high_critical_moves)
        critical_source << ' ' << static_cast<unsigned>(move);
    critical_source
        << "\n    base_speed_fraction 1 2\n"
        << "    focus_energy_ratio\n"
        << "        ordinary 2 1\n"
        << "        focused 1 2\n"
        << "    move_ratio\n"
        << "        ordinary 1 2\n"
        << "        high_critical 4 1\n"
        << "    rotate_random_left 3\n"
        << "    compare_less\n";
    add_text_file(result, "source/battle_effects/critical_hits.sexpr",
                  critical_source.str());

    std::ostringstream report;
    report << "Pokemon Red semantic battle-rule import\n"
           << "damage_formula_programs 1\n"
           << "damage_formula_instructions " << program.instructions.size()
           << '\n'
           << "capture_formula_programs 0\n"
           << "critical_hit_programs 1\n"
           << "move_effect_programs 0\n"
           << "status_programs 0\n"
           << "coverage_note ordinary damage is executable; remaining battle "
              "program domains stay explicitly incomplete\n";
    add_text_file(result, "reports/battle_rule_import_summary.txt",
                  report.str());
}

void emit_cache(const DamageFormulaProgram& program,
                const CriticalHitProgram& critical,
                BattleRuleImport& result) {
    std::vector<std::uint8_t> bytes{'P', 'B', 'R', '2'};
    write_u16(bytes, 1U);
    write_u16(bytes, 0U);
    write_string(bytes, program.key);
    write_u16(bytes,
              static_cast<std::uint16_t>(program.instructions.size()));
    for (const DamageFormulaInstruction& value : program.instructions) {
        bytes.push_back(static_cast<std::uint8_t>(value.opcode));
        for (const std::uint16_t operand : value.operands)
            write_u16(bytes, operand);
    }

    write_u16(bytes, 1U);
    write_u16(bytes, 0U);
    write_string(bytes, critical.key);
    write_u16(bytes,
              static_cast<std::uint16_t>(critical.high_critical_moves.size()));
    bytes.insert(bytes.end(), critical.high_critical_moves.begin(),
                 critical.high_critical_moves.end());
    write_u16(bytes,
              static_cast<std::uint16_t>(critical.instructions.size()));
    for (const CriticalHitInstruction& value : critical.instructions) {
        bytes.push_back(static_cast<std::uint8_t>(value.opcode));
        for (const std::uint16_t operand : value.operands)
            write_u16(bytes, operand);
    }
    result.files.push_back(
        {"compiled/battle_rules.bin", std::move(bytes)});
}

} // namespace

bool decode_battle_rule_import(std::span<const std::uint8_t> rom,
                               BattleRuleImport& result, std::string& error) {
    result = {};
    if (!verify_pokemon_red_us_rev_0(rom, error)) return false;
    constexpr std::array<std::size_t, 4> required_offsets{
        0x03DDCFU, 0x03DF65U, 0x03E3A5U, 0x03E687U};
    if (rom.size() <= required_offsets.back()) {
        error = "battle formula source ranges exceed the verified cartridge";
        return false;
    }

    const DamageFormulaProgram program = lift_original_damage_formula();
    CriticalHitProgram critical;
    if (!lift_original_critical_hit_program(rom, critical, error)) return false;
    emit_source(program, critical, result);
    emit_cache(program, critical, result);
    result.damage_formulas = 1U;
    result.critical_hit_programs = 1U;
    error.clear();
    return true;
}

} // namespace pokered::import
