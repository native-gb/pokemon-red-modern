#include "battle_rules.hpp"

#include <algorithm>
#include <cstdint>

namespace pokered {

const AccuracyFormulaProgram* find_accuracy_formula(
    const BattleRuleCatalog& rules, std::uint16_t id) {
    return id < rules.accuracy_formulas.size()
               ? &rules.accuracy_formulas[id]
               : nullptr;
}

bool execute_accuracy_formula(
    const AccuracyFormulaProgram& program,
    const AccuracyFormulaInput& input,
    std::span<const std::uint8_t> random_bytes,
    AccuracyFormulaResult& result, std::string& error) {
    result = {};
    const std::size_t stage_count = program.stage_ratios.size();
    if (stage_count == 0U || program.instructions.empty() ||
        input.accuracy_stage == 0U ||
        static_cast<std::size_t>(input.accuracy_stage) > stage_count ||
        input.target_evasion_stage == 0U ||
        static_cast<std::size_t>(input.target_evasion_stage) > stage_count) {
        error = "accuracy formula received invalid stage inputs";
        return false;
    }

    std::uint64_t chance = input.raw_accuracy;
    std::uint16_t reflected_evasion = input.target_evasion_stage;
    std::uint8_t random = 0U;
    bool random_loaded = false;
    bool compared = false;

    // Execute the imported formula in order. Content supplies both the ratio
    // table and the reflection/cap constants; the engine only performs the
    // generic integer operations.
    for (const AccuracyFormulaInstruction& instruction :
         program.instructions) {
        const auto& value = instruction.operands;
        switch (instruction.opcode) {
        case AccuracyFormulaOpcode::guarantee_if_bypassed:
            if (input.bypassed) {
                result.chance = value[0] == 0U ? 255U : value[0];
                result.hit = true;
                error.clear();
                return true;
            }
            break;
        case AccuracyFormulaOpcode::reflect_evasion:
            if (input.target_evasion_stage >= value[0]) {
                error = "accuracy formula reflected an invalid evasion stage";
                return false;
            }
            reflected_evasion = static_cast<std::uint16_t>(
                value[0] - input.target_evasion_stage);
            if (reflected_evasion == 0U ||
                reflected_evasion > stage_count) {
                error = "accuracy formula reflection left the stage table";
                return false;
            }
            break;
        case AccuracyFormulaOpcode::apply_accuracy_stage: {
            const AccuracyStageRatio& ratio =
                program.stage_ratios[input.accuracy_stage - 1U];
            chance = chance * ratio.numerator / ratio.denominator;
            chance = std::max<std::uint64_t>(chance, 1U);
            break;
        }
        case AccuracyFormulaOpcode::apply_evasion_stage: {
            const AccuracyStageRatio& ratio =
                program.stage_ratios[reflected_evasion - 1U];
            chance = chance * ratio.numerator / ratio.denominator;
            chance = std::max<std::uint64_t>(chance, 1U);
            break;
        }
        case AccuracyFormulaOpcode::cap_chance:
            chance = std::min<std::uint64_t>(chance, value[0]);
            break;
        case AccuracyFormulaOpcode::sample_random:
            if (result.random_bytes_consumed >= random_bytes.size()) {
                error =
                    "accuracy formula exhausted its deterministic random stream";
                return false;
            }
            random = random_bytes[result.random_bytes_consumed++];
            random_loaded = true;
            break;
        case AccuracyFormulaOpcode::compare_less:
            if (!random_loaded) {
                error = "accuracy formula compared before loading random";
                return false;
            }
            result.hit = random < chance;
            compared = true;
            break;
        }
    }
    if (!compared || chance > 255U) {
        error = "accuracy formula did not produce a valid hit decision";
        return false;
    }
    result.chance = static_cast<std::uint16_t>(chance);
    error.clear();
    return true;
}

} // namespace pokered
