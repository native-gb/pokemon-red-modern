#include "import_battle_rules.hpp"

#include "battle_rules.hpp"

#include <algorithm>
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

CaptureFormulaInstruction capture_instruction(
    CaptureFormulaOpcode opcode, std::uint16_t a = 0U, std::uint16_t b = 0U,
    std::uint16_t c = 0U, std::uint16_t d = 0U) {
    return {.opcode = opcode, .operands = {a, b, c, d}};
}

ExperienceFormulaInstruction experience_instruction(
    ExperienceFormulaOpcode opcode, std::uint16_t a = 0U,
    std::uint16_t b = 0U, std::uint16_t c = 0U,
    std::uint16_t d = 0U) {
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

bool read_immediate(std::span<const std::uint8_t> rom,
                    std::size_t instruction_offset, std::uint8_t opcode,
                    std::uint8_t& result, std::string& error) {
    if (instruction_offset + 1U >= rom.size() ||
        rom[instruction_offset] != opcode) {
        error = "battle rule immediate is absent from its verified source";
        return false;
    }
    result = rom[instruction_offset + 1U];
    return true;
}

bool lift_original_experience_formula(
    std::span<const std::uint8_t> rom, ExperienceFormulaProgram& program,
    std::string& error) {
    std::uint8_t experience_divisor = 0;
    if (!read_immediate(rom, 0x0552B3U, 0x3EU, experience_divisor, error))
        return false;

    // BoostExp copies the low word, shifts it right once, then adds that half
    // back into the original reward. Validate the source sequence so the
    // semantic 3/2 ratio is tied to the user's verified cartridge.
    constexpr std::array<std::uint8_t, 18> boost_sequence{
        0xF0U, 0x97U, 0x47U, 0xF0U, 0x98U, 0x4FU,
        0xCBU, 0x38U, 0xCBU, 0x19U, 0x81U, 0xE0U,
        0x98U, 0xF0U, 0x97U, 0x88U, 0xE0U, 0x97U};
    constexpr std::size_t boost_offset = 0x05549FU;
    if (boost_offset + boost_sequence.size() > rom.size() ||
        !std::equal(boost_sequence.begin(), boost_sequence.end(),
                    rom.begin() +
                        static_cast<std::ptrdiff_t>(boost_offset))) {
        error = "experience boost sequence is not the verified routine";
        return false;
    }

    program.key = "gen_1_original_experience";
    program.instructions = {
        experience_instruction(
            ExperienceFormulaOpcode::divide_reward_data),
        experience_instruction(
            ExperienceFormulaOpcode::calculate_stat_experience),
        experience_instruction(
            ExperienceFormulaOpcode::calculate_base_experience,
            experience_divisor),
        experience_instruction(ExperienceFormulaOpcode::boost_if_traded,
                               3U, 2U),
        experience_instruction(ExperienceFormulaOpcode::boost_if_trainer,
                               3U, 2U),
    };
    return true;
}

bool lift_original_capture_formula(
    std::span<const std::uint8_t> rom, CaptureFormulaProgram& program,
    std::string& error) {
    std::uint8_t great_ceiling = 0;
    std::uint8_t ultra_ceiling = 0;
    std::uint8_t default_hp_divisor = 0;
    std::uint8_t great_hp_divisor = 0;
    std::uint8_t minor_reduction = 0;
    std::uint8_t major_reduction = 0;
    std::uint8_t catch_rate_scale = 0;
    std::uint8_t default_shake_divisor = 0;
    std::uint8_t great_shake_divisor = 0;
    std::uint8_t ultra_shake_divisor = 0;
    std::uint8_t capture_scale = 0;
    std::uint8_t minor_shake_bonus = 0;
    std::uint8_t major_shake_bonus = 0;
    std::uint8_t first_shake_threshold = 0;
    std::uint8_t second_shake_threshold = 0;
    std::uint8_t third_shake_threshold = 0;
    if (!read_immediate(rom, 0x0D70BU, 0x3EU, great_ceiling, error) ||
        !read_immediate(rom, 0x0D715U, 0x3EU, ultra_ceiling, error) ||
        !read_immediate(rom, 0x0D747U, 0x3EU, default_hp_divisor, error) ||
        !read_immediate(rom, 0x0D74BU, 0x3EU, great_hp_divisor, error) ||
        !read_immediate(rom, 0x0D722U, 0x0EU, minor_reduction, error) ||
        !read_immediate(rom, 0x0D726U, 0x0EU, major_reduction, error) ||
        !read_immediate(rom, 0x0D79CU, 0x3EU, catch_rate_scale, error) ||
        !read_immediate(rom, 0x0D7A6U, 0x06U,
                        default_shake_divisor, error) ||
        !read_immediate(rom, 0x0D7ACU, 0x06U,
                        great_shake_divisor, error) ||
        !read_immediate(rom, 0x0D7B2U, 0x06U,
                        ultra_shake_divisor, error) ||
        !read_immediate(rom, 0x0D7CFU, 0x3EU, capture_scale, error) ||
        !read_immediate(rom, 0x0D7E0U, 0x06U,
                        minor_shake_bonus, error) ||
        !read_immediate(rom, 0x0D7E4U, 0x06U,
                        major_shake_bonus, error) ||
        !read_immediate(rom, 0x0D7EDU, 0xFEU,
                        first_shake_threshold, error) ||
        !read_immediate(rom, 0x0D7F3U, 0xFEU,
                        second_shake_threshold, error) ||
        !read_immediate(rom, 0x0D7F9U, 0xFEU,
                        third_shake_threshold, error)) {
        return false;
    }
    constexpr std::array<std::uint8_t, 8> quarter_hp_shifts{
        0xCBU, 0x38U, 0xCBU, 0x1FU, 0xCBU, 0x38U, 0xCBU, 0x1FU};
    if (!std::equal(quarter_hp_shifts.begin(), quarter_hp_shifts.end(),
                    rom.begin() + 0x0D75AU)) {
        error = "capture HP quartering sequence is not the verified routine";
        return false;
    }

    program.key = "gen_1_original_capture";
    program.ball_profiles = {
        {"master_ball", default_shake_divisor, default_hp_divisor,
         default_shake_divisor, true},
        {"ultra_ball", ultra_ceiling, default_hp_divisor,
         ultra_shake_divisor, false},
        {"great_ball", great_ceiling, great_hp_divisor,
         great_shake_divisor, false},
        {"poke_ball", default_shake_divisor, default_hp_divisor,
         default_shake_divisor, false},
        {"safari_ball", ultra_ceiling, default_hp_divisor,
         ultra_shake_divisor, false},
    };
    program.status_profiles = {
        {"none", 0U, 0U},
        {"burn_paralysis_poison", minor_reduction, minor_shake_bonus},
        {"freeze_sleep", major_reduction, major_shake_bonus},
    };
    constexpr std::uint16_t quarter_hp_divisor =
        1U << (quarter_hp_shifts.size() / 4U);
    program.instructions = {
        capture_instruction(CaptureFormulaOpcode::sample_first_roll),
        capture_instruction(CaptureFormulaOpcode::guaranteed_capture),
        capture_instruction(CaptureFormulaOpcode::apply_status_reduction),
        capture_instruction(CaptureFormulaOpcode::calculate_capture_value,
                            capture_scale, quarter_hp_divisor, 1U),
        capture_instruction(CaptureFormulaOpcode::compare_primary,
                            capture_scale),
        capture_instruction(CaptureFormulaOpcode::sample_second_roll,
                            capture_scale),
        capture_instruction(CaptureFormulaOpcode::calculate_shake_value,
                            catch_rate_scale, capture_scale),
        capture_instruction(CaptureFormulaOpcode::select_shakes,
                            first_shake_threshold, second_shake_threshold,
                            third_shake_threshold),
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
                 const CaptureFormulaProgram& capture,
                 const ExperienceFormulaProgram& experience,
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

    std::ostringstream capture_source;
    capture_source
        << "; Semantic lift of the verified cartridge capture calculation at\n"
        << "; 0x0d6fa..0x0d801. Item bindings reference these profiles.\n\n"
        << "capture_formula " << capture.key << '\n';
    for (const CaptureBallProfile& profile : capture.ball_profiles) {
        capture_source
            << "    ball_profile " << profile.key << '\n'
            << "        rejection_ceiling "
            << static_cast<unsigned>(profile.rejection_ceiling) << '\n'
            << "        hp_divisor "
            << static_cast<unsigned>(profile.hp_divisor) << '\n'
            << "        shake_divisor "
            << static_cast<unsigned>(profile.shake_divisor) << '\n'
            << "        guaranteed "
            << (profile.guaranteed ? "true" : "false") << '\n';
    }
    for (const CaptureStatusProfile& profile : capture.status_profiles) {
        capture_source
            << "    status_profile " << profile.key << '\n'
            << "        first_roll_reduction "
            << static_cast<unsigned>(profile.first_roll_reduction) << '\n'
            << "        shake_bonus "
            << static_cast<unsigned>(profile.shake_bonus) << '\n';
    }
    for (const CaptureFormulaInstruction& instruction :
         capture.instructions) {
        const auto& value = instruction.operands;
        switch (instruction.opcode) {
        case CaptureFormulaOpcode::sample_first_roll:
            capture_source << "    sample_first_roll\n";
            break;
        case CaptureFormulaOpcode::guaranteed_capture:
            capture_source << "    guaranteed_capture\n";
            break;
        case CaptureFormulaOpcode::apply_status_reduction:
            capture_source << "    apply_status_reduction\n";
            break;
        case CaptureFormulaOpcode::calculate_capture_value:
            capture_source
                << "    calculate_capture_value\n"
                << "        maximum_hp_scale " << value[0] << '\n'
                << "        current_hp_divisor " << value[1] << '\n'
                << "        minimum_current_hp_part " << value[2] << '\n';
            break;
        case CaptureFormulaOpcode::compare_primary:
            capture_source << "    compare_primary " << value[0] << '\n';
            break;
        case CaptureFormulaOpcode::sample_second_roll:
            capture_source << "    sample_second_roll " << value[0] << '\n';
            break;
        case CaptureFormulaOpcode::calculate_shake_value:
            capture_source
                << "    calculate_shake_value\n"
                << "        catch_rate_scale " << value[0] << '\n'
                << "        capture_value_divisor " << value[1] << '\n';
            break;
        case CaptureFormulaOpcode::select_shakes:
            capture_source
                << "    select_shakes " << value[0] << ' ' << value[1]
                << ' ' << value[2] << '\n';
            break;
        }
    }
    add_text_file(result, "source/battle_effects/capture.sexpr",
                  capture_source.str());

    std::ostringstream experience_source;
    experience_source
        << "; Semantic lift of the verified cartridge experience award at\n"
        << "; 0x05524f..0x0554b1. Party scheduling and Exp. All recipient\n"
        << "; selection remain responsibilities of the battle owner.\n\n"
        << "experience_formula " << experience.key << '\n';
    for (const ExperienceFormulaInstruction& instruction :
         experience.instructions) {
        const auto& value = instruction.operands;
        switch (instruction.opcode) {
        case ExperienceFormulaOpcode::divide_reward_data:
            experience_source << "    divide_reward_data\n";
            break;
        case ExperienceFormulaOpcode::calculate_stat_experience:
            experience_source << "    calculate_stat_experience\n";
            break;
        case ExperienceFormulaOpcode::calculate_base_experience:
            experience_source << "    calculate_base_experience\n"
                              << "        divisor " << value[0] << '\n';
            break;
        case ExperienceFormulaOpcode::boost_if_traded:
            experience_source << "    boost_if_traded "
                              << value[0] << ' ' << value[1] << '\n';
            break;
        case ExperienceFormulaOpcode::boost_if_trainer:
            experience_source << "    boost_if_trainer "
                              << value[0] << ' ' << value[1] << '\n';
            break;
        }
    }
    add_text_file(result, "source/battle_effects/experience.sexpr",
                  experience_source.str());

    std::ostringstream report;
    report << "Pokemon Red semantic battle-rule import\n"
           << "damage_formula_programs 1\n"
           << "damage_formula_instructions " << program.instructions.size()
           << '\n'
           << "capture_formula_programs 1\n"
           << "critical_hit_programs 1\n"
           << "experience_formula_programs 1\n"
           << "move_effect_programs 0\n"
           << "status_programs 0\n"
           << "coverage_note damage, critical-hit, capture, and experience "
              "calculations are executable; remaining battle program "
              "domains stay explicitly incomplete\n";
    add_text_file(result, "reports/battle_rule_import_summary.txt",
                  report.str());
}

void emit_cache(const DamageFormulaProgram& program,
                const CriticalHitProgram& critical,
                const CaptureFormulaProgram& capture,
                const ExperienceFormulaProgram& experience,
                BattleRuleImport& result) {
    std::vector<std::uint8_t> bytes{'P', 'B', 'R', '4'};
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

    write_u16(bytes, 1U);
    write_u16(bytes, 0U);
    write_string(bytes, capture.key);
    write_u16(bytes,
              static_cast<std::uint16_t>(capture.ball_profiles.size()));
    for (const CaptureBallProfile& profile : capture.ball_profiles) {
        write_string(bytes, profile.key);
        bytes.push_back(profile.rejection_ceiling);
        bytes.push_back(profile.hp_divisor);
        bytes.push_back(profile.shake_divisor);
        bytes.push_back(profile.guaranteed ? 1U : 0U);
    }
    write_u16(bytes,
              static_cast<std::uint16_t>(capture.status_profiles.size()));
    for (const CaptureStatusProfile& profile : capture.status_profiles) {
        write_string(bytes, profile.key);
        bytes.push_back(profile.first_roll_reduction);
        bytes.push_back(profile.shake_bonus);
    }
    write_u16(bytes,
              static_cast<std::uint16_t>(capture.instructions.size()));
    for (const CaptureFormulaInstruction& value : capture.instructions) {
        bytes.push_back(static_cast<std::uint8_t>(value.opcode));
        for (const std::uint16_t operand : value.operands)
            write_u16(bytes, operand);
    }

    write_u16(bytes, 1U);
    write_u16(bytes, 0U);
    write_string(bytes, experience.key);
    write_u16(bytes,
              static_cast<std::uint16_t>(experience.instructions.size()));
    for (const ExperienceFormulaInstruction& value :
         experience.instructions) {
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
    CaptureFormulaProgram capture;
    if (!lift_original_capture_formula(rom, capture, error)) return false;
    ExperienceFormulaProgram experience;
    if (!lift_original_experience_formula(rom, experience, error))
        return false;
    emit_source(program, critical, capture, experience, result);
    emit_cache(program, critical, capture, experience, result);
    result.damage_formulas = 1U;
    result.critical_hit_programs = 1U;
    result.capture_formulas = 1U;
    result.experience_formulas = 1U;
    error.clear();
    return true;
}

} // namespace pokered::import
