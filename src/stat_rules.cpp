#include "battle_rules.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace pokered {
namespace {

std::uint32_t ceiling_square_root(std::uint16_t value, std::uint16_t maximum) {
    std::uint32_t root = 0U;
    while (root < maximum && root * root < value)
        ++root;
    return root;
}

} // namespace

const StatFormulaProgram* find_stat_formula(const BattleRuleCatalog& rules, std::uint16_t id) {
    return id < rules.stat_formulas.size() ? &rules.stat_formulas[id] : nullptr;
}

bool execute_stat_formula(const StatFormulaProgram& program, const StatFormulaInput& input,
                          StatFormulaResult& result, std::string& error) {
    result = {};
    if (program.instructions.empty() || input.level == 0U || input.level > 100U) {
        error = "stat formula received invalid program or level";
        return false;
    }
    for (std::uint8_t dv : input.dvs)
        if (dv > 15U) {
            error = "stat formula received a determinant value above 15";
            return false;
        }

    std::array<std::uint32_t, 5> dvs{0U, input.dvs[0], input.dvs[1], input.dvs[2], input.dvs[3]};
    std::array<std::uint32_t, 5> effort{};
    std::array<std::uint32_t, 5> values{};
    bool hp_dv_ready = false;
    bool effort_ready = false;
    bool values_ready = false;

    // The imported operation order owns all numeric policy. Intermediate
    // arrays make invalid or reordered programs fail instead of silently
    // calculating a plausible but different generation's stats.
    for (const StatFormulaInstruction& instruction : program.instructions) {
        const auto& operand = instruction.operands;
        switch (instruction.opcode) {
        case StatFormulaOpcode::derive_hp_dv:
            dvs[0] = 0U;
            for (std::size_t index = 0; index < input.dvs.size(); ++index)
                dvs[0] += (input.dvs[index] & 1U) * operand[index];
            result.hp_dv = static_cast<std::uint8_t>(dvs[0]);
            hp_dv_ready = true;
            break;
        case StatFormulaOpcode::calculate_effort_bonus:
            if (operand[0] == 0U || operand[1] == 0U) {
                error = "stat formula has an invalid effort operation";
                return false;
            }
            for (std::size_t index = 0; index < effort.size(); ++index)
                effort[index] =
                    ceiling_square_root(input.stat_experience[index], operand[1]) / operand[0];
            effort_ready = true;
            break;
        case StatFormulaOpcode::combine_base_and_dv:
            if (!hp_dv_ready || !effort_ready || operand[0] == 0U) {
                error = "stat formula combined values before its inputs";
                return false;
            }
            for (std::size_t index = 0; index < values.size(); ++index)
                values[index] = (input.base_stats[index] + dvs[index]) * operand[0] + effort[index];
            values_ready = true;
            break;
        case StatFormulaOpcode::scale_by_level:
            if (!values_ready || operand[0] == 0U) {
                error = "stat formula scaled values before combining them";
                return false;
            }
            for (std::uint32_t& value : values)
                value = value * input.level / operand[0];
            break;
        case StatFormulaOpcode::add_hp_bonus:
            if (!values_ready) {
                error = "stat formula added HP before calculating it";
                return false;
            }
            values[0] += static_cast<std::uint32_t>(input.level) * operand[0] + operand[1];
            break;
        case StatFormulaOpcode::add_other_bonus:
            if (!values_ready) {
                error = "stat formula added a stat bonus before calculating";
                return false;
            }
            for (std::size_t index = 1; index < values.size(); ++index)
                values[index] += operand[0];
            break;
        case StatFormulaOpcode::cap_stats:
            if (!values_ready || operand[0] == 0U) {
                error = "stat formula capped values before calculating them";
                return false;
            }
            for (std::uint32_t& value : values)
                if (value > operand[0]) value = operand[0];
            break;
        }
    }

    if (!values_ready) {
        error = "stat formula did not calculate any stats";
        return false;
    }
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (values[index] > std::numeric_limits<std::uint16_t>::max()) {
            error = "stat formula exceeded its 16-bit result domain";
            return false;
        }
        result.stats[index] = static_cast<std::uint16_t>(values[index]);
    }
    error.clear();
    return true;
}

} // namespace pokered
