#pragma once

#include "rules.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace pokered {

enum class DamageFormulaOpcode : std::uint8_t {
    halve_defense_for_effect,
    scale_wide_stats,
    multiply_level_if_critical,
    calculate_base_damage,
    cap_and_add,
    same_type_bonus,
    type_effectiveness,
    random_factor,
};

struct DamageFormulaInstruction {
    DamageFormulaOpcode opcode{DamageFormulaOpcode::calculate_base_damage};
    std::array<std::uint16_t, 4> operands{};
};

struct DamageFormulaProgram {
    std::string key;
    std::vector<DamageFormulaInstruction> instructions;
};

enum class CriticalHitOpcode : std::uint8_t {
    base_speed_fraction,
    focus_energy_ratio,
    move_ratio,
    rotate_random_left,
    compare_less,
};

struct CriticalHitInstruction {
    CriticalHitOpcode opcode{CriticalHitOpcode::base_speed_fraction};
    std::array<std::uint16_t, 4> operands{};
};

struct CriticalHitProgram {
    std::string key;
    std::vector<std::uint8_t> high_critical_moves;
    std::vector<CriticalHitInstruction> instructions;
};

struct BattleRuleCatalog {
    std::filesystem::path source;
    std::vector<DamageFormulaProgram> damage_formulas;
    std::uint16_t original_damage_formula{};
    std::vector<CriticalHitProgram> critical_hit_programs;
    std::uint16_t original_critical_hit_program{};
    bool loaded{};
};

struct DamageFormulaInput {
    std::uint8_t level{};
    std::uint8_t power{};
    std::uint8_t move_type{};
    std::uint8_t move_effect{};
    std::uint16_t attack{};
    std::uint16_t defense{};
    std::array<std::uint8_t, 2> attacker_types{};
    std::array<std::uint8_t, 2> defender_types{};
    bool critical{};
};

struct DamageFormulaResult {
    std::uint16_t damage{};
    std::size_t random_bytes_consumed{};
    bool immune{};
};

struct CriticalHitInput {
    std::uint8_t base_speed{};
    std::uint8_t move_id{};
    std::uint8_t move_power{};
    bool focused{};
};

struct CriticalHitResult {
    std::uint16_t threshold{};
    std::size_t random_bytes_consumed{};
    bool critical{};
};

bool load_battle_rules(const std::filesystem::path& path,
                       BattleRuleCatalog& result, std::string& error);
const DamageFormulaProgram* find_damage_formula(
    const BattleRuleCatalog& rules, std::uint16_t id);
const CriticalHitProgram* find_critical_hit_program(
    const BattleRuleCatalog& rules, std::uint16_t id);
bool execute_damage_formula(const RuleCatalog& pokemon_rules,
                            const DamageFormulaProgram& program,
                            const DamageFormulaInput& input,
                            std::span<const std::uint8_t> random_bytes,
                            DamageFormulaResult& result, std::string& error);
bool execute_critical_hit_program(const CriticalHitProgram& program,
                                  const CriticalHitInput& input,
                                  std::span<const std::uint8_t> random_bytes,
                                  CriticalHitResult& result,
                                  std::string& error);

} // namespace pokered
