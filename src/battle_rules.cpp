#include "battle_rules.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <set>
#include <string>
#include <utility>

namespace pokered {
namespace {

bool read_u8(std::istream& input, std::uint8_t& result) {
    char value = 0;
    if (!input.get(value)) return false;
    result = static_cast<std::uint8_t>(static_cast<unsigned char>(value));
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

bool read_string(std::istream& input, std::string& result) {
    std::uint16_t size = 0;
    if (!read_u16(input, size) || size == 0U || size > 256U) return false;
    result.resize(size);
    return static_cast<bool>(
        input.read(result.data(), static_cast<std::streamsize>(result.size())));
}

bool valid_instruction(const DamageFormulaInstruction& instruction) {
    const auto& value = instruction.operands;
    switch (instruction.opcode) {
    case DamageFormulaOpcode::halve_defense_for_effect:
        return value[0] <= 0xFFU && value[1] != 0U && value[2] != 0U;
    case DamageFormulaOpcode::scale_wide_stats:
        return value[0] != 0U && value[1] != 0U && value[2] != 0U;
    case DamageFormulaOpcode::multiply_level_if_critical:
        return value[0] != 0U;
    case DamageFormulaOpcode::calculate_base_damage:
        return value[0] != 0U && value[1] != 0U && value[3] != 0U;
    case DamageFormulaOpcode::cap_and_add:
        return value[2] != 0U && value[0] <= value[2] && value[1] <= value[2];
    case DamageFormulaOpcode::same_type_bonus:
        return value[0] != 0U && value[1] != 0U;
    case DamageFormulaOpcode::type_effectiveness:
        return true;
    case DamageFormulaOpcode::random_factor:
        return value[0] <= value[1] && value[1] <= 0xFFU &&
               value[2] != 0U && value[3] < 8U;
    }
    return false;
}

bool valid_instruction(const CriticalHitInstruction& instruction) {
    const auto& value = instruction.operands;
    switch (instruction.opcode) {
    case CriticalHitOpcode::base_speed_fraction:
        return value[0] != 0U && value[1] != 0U;
    case CriticalHitOpcode::focus_energy_ratio:
    case CriticalHitOpcode::move_ratio:
        return value[0] != 0U && value[1] != 0U && value[2] != 0U &&
               value[3] != 0U;
    case CriticalHitOpcode::rotate_random_left:
        return value[0] < 8U;
    case CriticalHitOpcode::compare_less:
        return true;
    }
    return false;
}

bool valid_instruction(const CaptureFormulaInstruction& instruction) {
    const auto& value = instruction.operands;
    switch (instruction.opcode) {
    case CaptureFormulaOpcode::sample_first_roll:
    case CaptureFormulaOpcode::guaranteed_capture:
    case CaptureFormulaOpcode::apply_status_reduction:
        return true;
    case CaptureFormulaOpcode::calculate_capture_value:
        return value[0] != 0U && value[1] != 0U && value[2] != 0U;
    case CaptureFormulaOpcode::compare_primary:
    case CaptureFormulaOpcode::sample_second_roll:
        return value[0] != 0U;
    case CaptureFormulaOpcode::calculate_shake_value:
        return value[0] != 0U && value[1] != 0U;
    case CaptureFormulaOpcode::select_shakes:
        return value[0] < value[1] && value[1] < value[2];
    }
    return false;
}

std::uint16_t interaction_multiplier(const RuleCatalog& rules,
                                     std::uint8_t attacking,
                                     std::uint8_t defending) {
    const auto found = std::find_if(
        rules.type_interactions.begin(), rules.type_interactions.end(),
        [&](const TypeInteractionRule& interaction) {
            return interaction.attacking_type == attacking &&
                   interaction.defending_type == defending;
        });
    return found == rules.type_interactions.end() ? 10U
                                                  : found->multiplier_tenths;
}

std::uint8_t rotate_right(std::uint8_t value, std::uint16_t count) {
    for (std::uint16_t index = 0; index < count; ++index)
        value = static_cast<std::uint8_t>((value >> 1U) | (value << 7U));
    return value;
}

} // namespace

bool load_battle_rules(const std::filesystem::path& path,
                       BattleRuleCatalog& result, std::string& error) {
    std::ifstream input(path, std::ios::binary);
    std::array<char, 4> magic{};
    if (!input.read(magic.data(), static_cast<std::streamsize>(magic.size())) ||
        magic != std::array{'P', 'B', 'R', '3'}) {
        error = "battle rule cache is missing or has an invalid header";
        return false;
    }

    BattleRuleCatalog loaded;
    loaded.source = path;
    std::uint16_t count = 0;
    if (!read_u16(input, count) || count == 0U || count > 64U ||
        !read_u16(input, loaded.original_damage_formula) ||
        loaded.original_damage_formula >= count) {
        error = "battle rule cache has an invalid formula index";
        return false;
    }
    loaded.damage_formulas.reserve(count);
    std::set<std::string> keys;
    for (std::uint16_t index = 0; index < count; ++index) {
        DamageFormulaProgram program;
        std::uint16_t instruction_count = 0;
        if (!read_string(input, program.key) || !keys.insert(program.key).second ||
            !read_u16(input, instruction_count) || instruction_count == 0U ||
            instruction_count > 128U) {
            error = "battle rule cache has an invalid damage formula";
            return false;
        }
        program.instructions.reserve(instruction_count);
        for (std::uint16_t instruction_index = 0;
             instruction_index < instruction_count; ++instruction_index) {
            DamageFormulaInstruction instruction;
            std::uint8_t opcode = 0;
            if (!read_u8(input, opcode) ||
                opcode > static_cast<std::uint8_t>(
                             DamageFormulaOpcode::random_factor)) {
                error = "battle rule cache has an unknown formula opcode";
                return false;
            }
            instruction.opcode = static_cast<DamageFormulaOpcode>(opcode);
            for (std::uint16_t& operand : instruction.operands)
                if (!read_u16(input, operand)) {
                    error = "battle rule cache has truncated formula operands";
                    return false;
                }
            if (!valid_instruction(instruction)) {
                error = "battle rule cache has invalid formula operands";
                return false;
            }
            program.instructions.push_back(instruction);
        }
        loaded.damage_formulas.push_back(std::move(program));
    }

    std::uint16_t critical_count = 0;
    if (!read_u16(input, critical_count) || critical_count == 0U ||
        critical_count > 64U ||
        !read_u16(input, loaded.original_critical_hit_program) ||
        loaded.original_critical_hit_program >= critical_count) {
        error = "battle rule cache has an invalid critical-hit program index";
        return false;
    }
    loaded.critical_hit_programs.reserve(critical_count);
    keys.clear();
    for (std::uint16_t index = 0; index < critical_count; ++index) {
        CriticalHitProgram program;
        std::uint16_t move_count = 0;
        std::uint16_t instruction_count = 0;
        if (!read_string(input, program.key) || !keys.insert(program.key).second ||
            !read_u16(input, move_count) || move_count > 165U) {
            error = "battle rule cache has an invalid critical-hit program";
            return false;
        }
        std::set<std::uint8_t> moves;
        program.high_critical_moves.reserve(move_count);
        for (std::uint16_t move_index = 0; move_index < move_count;
             ++move_index) {
            std::uint8_t move = 0;
            if (!read_u8(input, move) || move == 0U || move > 165U ||
                !moves.insert(move).second) {
                error = "battle rule cache has invalid high-critical moves";
                return false;
            }
            program.high_critical_moves.push_back(move);
        }
        if (!read_u16(input, instruction_count) || instruction_count == 0U ||
            instruction_count > 64U) {
            error = "battle rule cache has invalid critical-hit instructions";
            return false;
        }
        program.instructions.reserve(instruction_count);
        for (std::uint16_t instruction_index = 0;
             instruction_index < instruction_count; ++instruction_index) {
            CriticalHitInstruction instruction;
            std::uint8_t opcode = 0;
            if (!read_u8(input, opcode) ||
                opcode > static_cast<std::uint8_t>(
                             CriticalHitOpcode::compare_less)) {
                error = "battle rule cache has an unknown critical-hit opcode";
                return false;
            }
            instruction.opcode = static_cast<CriticalHitOpcode>(opcode);
            for (std::uint16_t& operand : instruction.operands)
                if (!read_u16(input, operand)) {
                    error = "battle rule cache has truncated critical-hit operands";
                    return false;
                }
            if (!valid_instruction(instruction)) {
                error = "battle rule cache has invalid critical-hit operands";
                return false;
            }
            program.instructions.push_back(instruction);
        }
        loaded.critical_hit_programs.push_back(std::move(program));
    }

    std::uint16_t capture_count = 0;
    if (!read_u16(input, capture_count) || capture_count == 0U ||
        capture_count > 64U ||
        !read_u16(input, loaded.original_capture_formula) ||
        loaded.original_capture_formula >= capture_count) {
        error = "battle rule cache has an invalid capture formula index";
        return false;
    }
    loaded.capture_formulas.reserve(capture_count);
    keys.clear();
    for (std::uint16_t index = 0; index < capture_count; ++index) {
        CaptureFormulaProgram program;
        std::uint16_t ball_count = 0;
        std::uint16_t status_count = 0;
        std::uint16_t instruction_count = 0;
        if (!read_string(input, program.key) ||
            !keys.insert(program.key).second ||
            !read_u16(input, ball_count) || ball_count == 0U ||
            ball_count > 64U) {
            error = "battle rule cache has an invalid capture formula";
            return false;
        }

        std::set<std::string> profile_keys;
        program.ball_profiles.reserve(ball_count);
        for (std::uint16_t profile_index = 0; profile_index < ball_count;
             ++profile_index) {
            CaptureBallProfile profile;
            std::uint8_t guaranteed = 0;
            if (!read_string(input, profile.key) ||
                !profile_keys.insert(profile.key).second ||
                !read_u8(input, profile.rejection_ceiling) ||
                !read_u8(input, profile.hp_divisor) ||
                !read_u8(input, profile.shake_divisor) ||
                !read_u8(input, guaranteed) || profile.hp_divisor == 0U ||
                profile.shake_divisor == 0U || guaranteed > 1U) {
                error = "battle rule cache has an invalid capture ball profile";
                return false;
            }
            profile.guaranteed = guaranteed != 0U;
            program.ball_profiles.push_back(std::move(profile));
        }

        if (!read_u16(input, status_count) || status_count == 0U ||
            status_count > 64U) {
            error = "battle rule cache has invalid capture status profiles";
            return false;
        }
        profile_keys.clear();
        program.status_profiles.reserve(status_count);
        for (std::uint16_t profile_index = 0; profile_index < status_count;
             ++profile_index) {
            CaptureStatusProfile profile;
            if (!read_string(input, profile.key) ||
                !profile_keys.insert(profile.key).second ||
                !read_u8(input, profile.first_roll_reduction) ||
                !read_u8(input, profile.shake_bonus)) {
                error =
                    "battle rule cache has an invalid capture status profile";
                return false;
            }
            program.status_profiles.push_back(std::move(profile));
        }

        if (!read_u16(input, instruction_count) ||
            instruction_count == 0U || instruction_count > 64U) {
            error = "battle rule cache has invalid capture instructions";
            return false;
        }
        program.instructions.reserve(instruction_count);
        for (std::uint16_t instruction_index = 0;
             instruction_index < instruction_count; ++instruction_index) {
            CaptureFormulaInstruction instruction;
            std::uint8_t opcode = 0;
            if (!read_u8(input, opcode) ||
                opcode > static_cast<std::uint8_t>(
                             CaptureFormulaOpcode::select_shakes)) {
                error = "battle rule cache has an unknown capture opcode";
                return false;
            }
            instruction.opcode = static_cast<CaptureFormulaOpcode>(opcode);
            for (std::uint16_t& operand : instruction.operands)
                if (!read_u16(input, operand)) {
                    error = "battle rule cache has truncated capture operands";
                    return false;
                }
            if (!valid_instruction(instruction)) {
                error = "battle rule cache has invalid capture operands";
                return false;
            }
            program.instructions.push_back(instruction);
        }
        loaded.capture_formulas.push_back(std::move(program));
    }
    if (input.peek() != std::char_traits<char>::eof()) {
        error = "battle rule cache contains trailing data";
        return false;
    }
    loaded.loaded = true;
    result = std::move(loaded);
    error.clear();
    return true;
}

const DamageFormulaProgram* find_damage_formula(
    const BattleRuleCatalog& rules, std::uint16_t id) {
    return id < rules.damage_formulas.size() ? &rules.damage_formulas[id]
                                             : nullptr;
}

const CriticalHitProgram* find_critical_hit_program(
    const BattleRuleCatalog& rules, std::uint16_t id) {
    return id < rules.critical_hit_programs.size()
               ? &rules.critical_hit_programs[id]
               : nullptr;
}

bool execute_damage_formula(const RuleCatalog& pokemon_rules,
                            const DamageFormulaProgram& program,
                            const DamageFormulaInput& input,
                            std::span<const std::uint8_t> random_bytes,
                            DamageFormulaResult& result, std::string& error) {
    result = {};
    if (!pokemon_rules.loaded || program.instructions.empty() ||
        input.level == 0U || find_type(pokemon_rules, input.move_type) == nullptr ||
        find_type(pokemon_rules, input.attacker_types[0]) == nullptr ||
        find_type(pokemon_rules, input.attacker_types[1]) == nullptr ||
        find_type(pokemon_rules, input.defender_types[0]) == nullptr ||
        find_type(pokemon_rules, input.defender_types[1]) == nullptr) {
        error = "damage formula received invalid catalog or battler inputs";
        return false;
    }
    if (input.power == 0U) {
        error.clear();
        return true;
    }

    std::uint64_t level = input.level;
    std::uint64_t attack = input.attack;
    std::uint64_t defense = input.defense;
    std::uint64_t damage = 0U;
    for (const DamageFormulaInstruction& instruction : program.instructions) {
        const auto& value = instruction.operands;
        switch (instruction.opcode) {
        case DamageFormulaOpcode::halve_defense_for_effect:
            if (input.move_effect == value[0])
                defense = std::max<std::uint64_t>(
                    defense / value[1], value[2]);
            break;
        case DamageFormulaOpcode::scale_wide_stats:
            if (attack >= value[0] || defense >= value[0]) {
                attack /= value[1];
                defense /= value[1];
                attack = std::max<std::uint64_t>(attack, value[2]);
            }
            break;
        case DamageFormulaOpcode::multiply_level_if_critical:
            if (input.critical) level *= value[0];
            break;
        case DamageFormulaOpcode::calculate_base_damage:
            if (defense == 0U) {
                error = "damage formula reached a zero defense divisor";
                return false;
            }
            damage = (level * value[0] / value[1]) + value[2];
            damage = damage * input.power * attack / defense;
            damage /= value[3];
            break;
        case DamageFormulaOpcode::cap_and_add:
            damage = std::min<std::uint64_t>(damage, value[0]);
            damage = std::min<std::uint64_t>(damage + value[1], value[2]);
            break;
        case DamageFormulaOpcode::same_type_bonus:
            if (input.move_type == input.attacker_types[0] ||
                input.move_type == input.attacker_types[1])
                damage = damage * value[0] / value[1];
            break;
        case DamageFormulaOpcode::type_effectiveness:
            for (std::size_t slot = 0; slot < input.defender_types.size();
                 ++slot) {
                if (slot != 0U &&
                    input.defender_types[slot] == input.defender_types[0])
                    continue;
                damage =
                    damage * interaction_multiplier(
                                 pokemon_rules, input.move_type,
                                 input.defender_types[slot]) /
                    10U;
            }
            if (damage == 0U) result.immune = true;
            break;
        case DamageFormulaOpcode::random_factor:
            if (damage < 2U) break;
            while (true) {
                if (result.random_bytes_consumed >= random_bytes.size()) {
                    error = "damage formula exhausted its deterministic random stream";
                    return false;
                }
                const std::uint8_t random = rotate_right(
                    random_bytes[result.random_bytes_consumed++], value[3]);
                if (random < value[0] || random > value[1]) continue;
                damage = damage * random / value[2];
                break;
            }
            break;
        }
        if (damage > std::numeric_limits<std::uint16_t>::max()) {
            error = "damage formula exceeded its 16-bit result domain";
            return false;
        }
    }
    result.damage = static_cast<std::uint16_t>(damage);
    error.clear();
    return true;
}

bool execute_critical_hit_program(const CriticalHitProgram& program,
                                  const CriticalHitInput& input,
                                  std::span<const std::uint8_t> random_bytes,
                                  CriticalHitResult& result,
                                  std::string& error) {
    result = {};
    if (program.instructions.empty() || input.move_id == 0U ||
        input.base_speed == 0U) {
        error = "critical-hit program received invalid inputs";
        return false;
    }
    if (input.move_power == 0U) {
        error.clear();
        return true;
    }

    std::uint64_t threshold = input.base_speed;
    std::uint8_t random = 0U;
    bool random_loaded = false;
    const bool high_critical =
        std::find(program.high_critical_moves.begin(),
                  program.high_critical_moves.end(),
                  input.move_id) != program.high_critical_moves.end();
    for (const CriticalHitInstruction& instruction : program.instructions) {
        const auto& value = instruction.operands;
        switch (instruction.opcode) {
        case CriticalHitOpcode::base_speed_fraction:
            threshold = threshold * value[0] / value[1];
            break;
        case CriticalHitOpcode::focus_energy_ratio:
            threshold =
                input.focused
                    ? threshold * value[2] / value[3]
                    : threshold * value[0] / value[1];
            threshold = std::min<std::uint64_t>(threshold, 255U);
            break;
        case CriticalHitOpcode::move_ratio:
            threshold =
                high_critical
                    ? threshold * value[2] / value[3]
                    : threshold * value[0] / value[1];
            threshold = std::min<std::uint64_t>(threshold, 255U);
            break;
        case CriticalHitOpcode::rotate_random_left:
            if (result.random_bytes_consumed >= random_bytes.size()) {
                error =
                    "critical-hit program exhausted its deterministic random stream";
                return false;
            }
            random = random_bytes[result.random_bytes_consumed++];
            for (std::uint16_t rotation = 0; rotation < value[0]; ++rotation)
                random = static_cast<std::uint8_t>((random << 1U) |
                                                   (random >> 7U));
            random_loaded = true;
            break;
        case CriticalHitOpcode::compare_less:
            if (!random_loaded) {
                error = "critical-hit program compared before loading random";
                return false;
            }
            result.critical = random < threshold;
            break;
        }
    }
    result.threshold = static_cast<std::uint16_t>(threshold);
    error.clear();
    return true;
}

} // namespace pokered
