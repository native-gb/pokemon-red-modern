#include "battle_rules.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>

namespace pokered {

const CaptureFormulaProgram* find_capture_formula(
    const BattleRuleCatalog& rules, std::uint16_t id) {
    return id < rules.capture_formulas.size() ? &rules.capture_formulas[id]
                                              : nullptr;
}

bool execute_capture_formula(const CaptureFormulaProgram& program,
                             const CaptureFormulaInput& input,
                             std::span<const std::uint8_t> random_bytes,
                             CaptureFormulaResult& result,
                             std::string& error) {
    result = {};
    if (program.instructions.empty() ||
        input.ball_profile >= program.ball_profiles.size() ||
        input.status_profile >= program.status_profiles.size() ||
        input.current_hp == 0U ||
        input.maximum_hp == 0U || input.current_hp > input.maximum_hp) {
        error = "capture formula received invalid program or opponent inputs";
        return false;
    }

    const CaptureBallProfile& ball =
        program.ball_profiles[input.ball_profile];
    const CaptureStatusProfile& status =
        program.status_profiles[input.status_profile];
    std::uint16_t first_roll = 0U;
    bool first_roll_loaded = false;
    bool primary_passed = false;

    // The semantic program owns source ordering and numeric policy. This
    // executor supplies only bounded arithmetic and deterministic RNG access.
    for (const CaptureFormulaInstruction& instruction : program.instructions) {
        const auto& value = instruction.operands;
        switch (instruction.opcode) {
        case CaptureFormulaOpcode::sample_first_roll:
            do {
                if (result.random_bytes_consumed >= random_bytes.size()) {
                    error =
                        "capture formula exhausted its deterministic random stream";
                    return false;
                }
                first_roll = random_bytes[result.random_bytes_consumed++];
            } while (first_roll > ball.rejection_ceiling);
            first_roll_loaded = true;
            break;
        case CaptureFormulaOpcode::guaranteed_capture:
            if (!first_roll_loaded) {
                error = "capture formula tested a guarantee before sampling";
                return false;
            }
            result.caught = ball.guaranteed;
            break;
        case CaptureFormulaOpcode::apply_status_reduction:
            if (result.caught) break;
            if (!first_roll_loaded) {
                error = "capture formula applied status before sampling";
                return false;
            }
            if (first_roll < status.first_roll_reduction) {
                result.caught = true;
                break;
            }
            first_roll = static_cast<std::uint16_t>(
                first_roll - status.first_roll_reduction);
            break;
        case CaptureFormulaOpcode::calculate_capture_value: {
            if (result.caught) break;
            const std::uint64_t quarter_hp = std::max<std::uint64_t>(
                input.current_hp / value[1], value[2]);
            const std::uint64_t capture_value =
                (static_cast<std::uint64_t>(input.maximum_hp) * value[0] /
                 ball.hp_divisor) /
                quarter_hp;
            if (capture_value > std::numeric_limits<std::uint16_t>::max()) {
                error = "capture formula exceeded its result domain";
                return false;
            }
            result.capture_value =
                static_cast<std::uint16_t>(capture_value);
            break;
        }
        case CaptureFormulaOpcode::compare_primary:
            if (result.caught) break;
            primary_passed = first_roll <= input.catch_rate;
            if (primary_passed && result.capture_value > value[0])
                result.caught = true;
            break;
        case CaptureFormulaOpcode::sample_second_roll:
            if (result.caught || !primary_passed) break;
            if (result.random_bytes_consumed >= random_bytes.size()) {
                error =
                    "capture formula exhausted its deterministic random stream";
                return false;
            }
            result.caught =
                std::min<std::uint16_t>(result.capture_value, value[0]) >=
                random_bytes[result.random_bytes_consumed++];
            break;
        case CaptureFormulaOpcode::calculate_shake_value: {
            if (result.caught) break;
            const std::uint64_t capped_capture =
                std::min<std::uint64_t>(result.capture_value, value[1]);
            const std::uint64_t rate_factor =
                static_cast<std::uint64_t>(input.catch_rate) * value[0] /
                ball.shake_divisor;
            const std::uint64_t shake_value =
                capped_capture * rate_factor / value[1] +
                status.shake_bonus;
            if (shake_value > std::numeric_limits<std::uint16_t>::max()) {
                error = "capture shake formula exceeded its result domain";
                return false;
            }
            result.shake_value = static_cast<std::uint16_t>(shake_value);
            break;
        }
        case CaptureFormulaOpcode::select_shakes:
            if (result.caught) break;
            result.shakes =
                result.shake_value < value[0]
                    ? 0U
                    : (result.shake_value < value[1]
                           ? 1U
                           : (result.shake_value < value[2] ? 2U : 3U));
            break;
        }
    }
    error.clear();
    return true;
}

} // namespace pokered
