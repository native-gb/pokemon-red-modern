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

enum class CaptureFormulaOpcode : std::uint8_t {
    sample_first_roll,
    guaranteed_capture,
    apply_status_reduction,
    calculate_capture_value,
    compare_primary,
    sample_second_roll,
    calculate_shake_value,
    select_shakes,
};

struct CaptureFormulaInstruction {
    CaptureFormulaOpcode opcode{CaptureFormulaOpcode::sample_first_roll};
    std::array<std::uint16_t, 4> operands{};
};

struct CaptureBallProfile {
    std::string key;
    std::uint8_t rejection_ceiling{};
    std::uint8_t hp_divisor{};
    std::uint8_t shake_divisor{};
    bool guaranteed{};
};

struct CaptureStatusProfile {
    std::string key;
    std::uint8_t first_roll_reduction{};
    std::uint8_t shake_bonus{};
};

struct CaptureFormulaProgram {
    std::string key;
    std::vector<CaptureBallProfile> ball_profiles;
    std::vector<CaptureStatusProfile> status_profiles;
    std::vector<CaptureFormulaInstruction> instructions;
};

enum class ExperienceFormulaOpcode : std::uint8_t {
    divide_reward_data,
    calculate_stat_experience,
    calculate_base_experience,
    boost_if_traded,
    boost_if_trainer,
};

struct ExperienceFormulaInstruction {
    ExperienceFormulaOpcode opcode{
        ExperienceFormulaOpcode::divide_reward_data};
    std::array<std::uint16_t, 4> operands{};
};

struct ExperienceFormulaProgram {
    std::string key;
    std::vector<ExperienceFormulaInstruction> instructions;
};

enum class StatFormulaOpcode : std::uint8_t {
    derive_hp_dv,
    calculate_effort_bonus,
    combine_base_and_dv,
    scale_by_level,
    add_hp_bonus,
    add_other_bonus,
    cap_stats,
};

struct StatFormulaInstruction {
    StatFormulaOpcode opcode{StatFormulaOpcode::derive_hp_dv};
    std::array<std::uint16_t, 4> operands{};
};

struct StatFormulaProgram {
    std::string key;
    std::vector<StatFormulaInstruction> instructions;
};

struct AccuracyStageRatio {
    std::uint16_t numerator{};
    std::uint16_t denominator{};
};

enum class AccuracyFormulaOpcode : std::uint8_t {
    guarantee_if_bypassed,
    reflect_evasion,
    apply_accuracy_stage,
    apply_evasion_stage,
    cap_chance,
    sample_random,
    compare_less,
};

struct AccuracyFormulaInstruction {
    AccuracyFormulaOpcode opcode{
        AccuracyFormulaOpcode::guarantee_if_bypassed};
    std::array<std::uint16_t, 4> operands{};
};

struct AccuracyFormulaProgram {
    std::string key;
    std::vector<AccuracyStageRatio> stage_ratios;
    std::uint8_t neutral_stage{};
    std::vector<AccuracyFormulaInstruction> instructions;
};

enum class MoveEffectOpcode : std::uint8_t {
    check_accuracy,
    calculate_critical,
    calculate_damage,
    deal_damage,
    enemy_random_gate,
    modify_stage,
};

struct MoveEffectInstruction {
    MoveEffectOpcode opcode{MoveEffectOpcode::check_accuracy};
    std::array<std::uint16_t, 4> operands{};
};

struct MoveEffectProgram {
    std::string key;
    std::vector<std::uint8_t> source_effect_ids;
    std::vector<MoveEffectInstruction> instructions;
};

struct BattleRuleCatalog {
    std::filesystem::path source;
    std::vector<DamageFormulaProgram> damage_formulas;
    std::uint16_t original_damage_formula{};
    std::vector<CriticalHitProgram> critical_hit_programs;
    std::uint16_t original_critical_hit_program{};
    std::vector<CaptureFormulaProgram> capture_formulas;
    std::uint16_t original_capture_formula{};
    std::vector<ExperienceFormulaProgram> experience_formulas;
    std::uint16_t original_experience_formula{};
    std::vector<StatFormulaProgram> stat_formulas;
    std::uint16_t original_stat_formula{};
    std::vector<AccuracyFormulaProgram> accuracy_formulas;
    std::uint16_t original_accuracy_formula{};
    std::vector<MoveEffectProgram> move_effect_programs;
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

struct CaptureFormulaInput {
    std::uint16_t ball_profile{};
    std::uint16_t status_profile{};
    std::uint8_t catch_rate{};
    std::uint16_t current_hp{};
    std::uint16_t maximum_hp{};
};

struct CaptureFormulaResult {
    std::uint16_t capture_value{};
    std::uint16_t shake_value{};
    std::size_t random_bytes_consumed{};
    std::uint8_t shakes{};
    bool caught{};
};

struct ExperienceFormulaInput {
    std::uint8_t base_experience{};
    std::array<std::uint8_t, 5> base_stats{};
    std::uint8_t defeated_level{};
    std::uint8_t base_value_divisor{1U};
    std::uint8_t participant_divisor{1U};
    bool traded{};
    bool trainer_battle{};
};

struct ExperienceFormulaResult {
    std::uint32_t experience{};
    std::array<std::uint16_t, 5> stat_experience{};
};

struct StatFormulaInput {
    std::array<std::uint8_t, 5> base_stats{};
    // Attack, Defense, Speed, and Special determinant values.
    std::array<std::uint8_t, 4> dvs{};
    std::array<std::uint16_t, 5> stat_experience{};
    std::uint8_t level{};
};

struct StatFormulaResult {
    // HP, Attack, Defense, Speed, and Special.
    std::array<std::uint16_t, 5> stats{};
    std::uint8_t hp_dv{};
};

struct AccuracyFormulaInput {
    std::uint8_t raw_accuracy{};
    std::uint8_t accuracy_stage{};
    std::uint8_t target_evasion_stage{};
    bool bypassed{};
};

struct AccuracyFormulaResult {
    std::uint16_t chance{};
    std::size_t random_bytes_consumed{};
    bool hit{};
};

bool load_battle_rules(const std::filesystem::path& path,
                       BattleRuleCatalog& result, std::string& error);
const DamageFormulaProgram* find_damage_formula(
    const BattleRuleCatalog& rules, std::uint16_t id);
const CriticalHitProgram* find_critical_hit_program(
    const BattleRuleCatalog& rules, std::uint16_t id);
const CaptureFormulaProgram* find_capture_formula(
    const BattleRuleCatalog& rules, std::uint16_t id);
const ExperienceFormulaProgram* find_experience_formula(
    const BattleRuleCatalog& rules, std::uint16_t id);
const StatFormulaProgram* find_stat_formula(
    const BattleRuleCatalog& rules, std::uint16_t id);
const AccuracyFormulaProgram* find_accuracy_formula(
    const BattleRuleCatalog& rules, std::uint16_t id);
const MoveEffectProgram* find_move_effect_program(
    const BattleRuleCatalog& rules, std::uint8_t source_effect_id);
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
bool execute_capture_formula(const CaptureFormulaProgram& program,
                             const CaptureFormulaInput& input,
                             std::span<const std::uint8_t> random_bytes,
                             CaptureFormulaResult& result,
                             std::string& error);
bool execute_experience_formula(const ExperienceFormulaProgram& program,
                                const ExperienceFormulaInput& input,
                                ExperienceFormulaResult& result,
                                std::string& error);
bool execute_stat_formula(const StatFormulaProgram& program,
                          const StatFormulaInput& input,
                          StatFormulaResult& result,
                          std::string& error);
bool execute_accuracy_formula(
    const AccuracyFormulaProgram& program,
    const AccuracyFormulaInput& input,
    std::span<const std::uint8_t> random_bytes,
    AccuracyFormulaResult& result, std::string& error);

} // namespace pokered
