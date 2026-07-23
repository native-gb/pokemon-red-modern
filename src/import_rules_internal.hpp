#pragma once

#include "import_rules.hpp"
#include "rules.hpp"

#include <vector>

namespace pokered::import {

void emit_rule_import(const std::vector<TypeRule>& types,
                      const std::vector<TypeInteractionRule>& interactions,
                      const std::vector<MoveRule>& moves, const std::vector<SpeciesRule>& species,
                      const std::vector<LearnsetRule>& learnsets,
                      const std::vector<EvolutionRule>& evolutions,
                      const std::vector<GrowthCurveRule>& growth_curves,
                      const std::vector<MachineRule>& machines, RuleImport& result);

} // namespace pokered::import
