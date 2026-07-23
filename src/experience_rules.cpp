#include "battle_rules.hpp"

#include <cstdint>
#include <limits>
#include <string>

namespace pokered {

const ExperienceFormulaProgram* find_experience_formula(
    const BattleRuleCatalog& rules, std::uint16_t id) {
    return id < rules.experience_formulas.size()
               ? &rules.experience_formulas[id]
               : nullptr;
}

bool execute_experience_formula(const ExperienceFormulaProgram& program,
                                const ExperienceFormulaInput& input,
                                ExperienceFormulaResult& result,
                                std::string& error) {
    result = {};
    if (program.instructions.empty() || input.base_experience == 0U ||
        input.defeated_level == 0U || input.base_value_divisor == 0U ||
        input.participant_divisor == 0U) {
        error = "experience formula received invalid reward inputs";
        return false;
    }

    std::uint32_t divided_experience = input.base_experience;
    std::array<std::uint32_t, 5> divided_stats{};
    for (std::size_t index = 0; index < divided_stats.size(); ++index)
        divided_stats[index] = input.base_stats[index];

    // Division and boost ordering are program-owned because Gen I floors at
    // every boundary. Reassociating these operations changes awarded values.
    for (const ExperienceFormulaInstruction& instruction :
         program.instructions) {
        const auto& value = instruction.operands;
        switch (instruction.opcode) {
        case ExperienceFormulaOpcode::divide_reward_data:
            divided_experience /= input.base_value_divisor;
            divided_experience /= input.participant_divisor;
            for (std::uint32_t& stat : divided_stats) {
                stat /= input.base_value_divisor;
                stat /= input.participant_divisor;
            }
            break;
        case ExperienceFormulaOpcode::calculate_stat_experience:
            for (std::size_t index = 0; index < divided_stats.size(); ++index)
                result.stat_experience[index] =
                    static_cast<std::uint16_t>(divided_stats[index]);
            break;
        case ExperienceFormulaOpcode::calculate_base_experience:
            result.experience =
                divided_experience * input.defeated_level / value[0];
            break;
        case ExperienceFormulaOpcode::boost_if_traded:
            if (input.traded)
                result.experience =
                    result.experience * value[0] / value[1];
            break;
        case ExperienceFormulaOpcode::boost_if_trainer:
            if (input.trainer_battle)
                result.experience =
                    result.experience * value[0] / value[1];
            break;
        }
        if (result.experience >
            std::numeric_limits<std::uint16_t>::max()) {
            error = "experience formula exceeded its 16-bit award domain";
            return false;
        }
    }
    error.clear();
    return true;
}

} // namespace pokered
