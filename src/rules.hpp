#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace pokered {

enum class MoveDamageClass : std::uint8_t {
    physical,
    special,
};

enum class EvolutionMethod : std::uint8_t {
    level,
    item,
    trade,
};

struct TypeRule {
    std::uint8_t id{};
    std::string key;
    std::string name;
    MoveDamageClass damage_class{MoveDamageClass::physical};
    bool unused{};
};

struct TypeInteractionRule {
    std::uint8_t attacking_type{};
    std::uint8_t defending_type{};
    std::uint8_t multiplier_tenths{};
};

struct MoveRule {
    std::uint8_t id{};
    std::string key;
    std::string name;
    std::uint8_t animation_id{};
    std::uint8_t effect_id{};
    std::uint8_t power{};
    std::uint8_t type_id{};
    std::uint8_t accuracy_raw{};
    std::uint8_t pp{};
    MoveDamageClass damage_class{MoveDamageClass::physical};
};

struct SpeciesRule {
    std::uint8_t dex_number{};
    std::uint8_t internal_id{};
    std::string key;
    std::string name;
    std::uint8_t base_hp{};
    std::uint8_t base_attack{};
    std::uint8_t base_defense{};
    std::uint8_t base_speed{};
    std::uint8_t base_special{};
    std::array<std::uint8_t, 2> type_ids{};
    std::uint8_t catch_rate{};
    std::uint8_t experience_yield{};
    std::array<std::uint8_t, 4> starting_move_ids{};
    std::uint8_t growth_curve_id{};
    std::array<std::uint8_t, 7> machine_compatibility{};
};

struct LearnsetRule {
    std::uint8_t species_dex{};
    std::uint8_t level{};
    std::uint8_t move_id{};
    std::uint16_t order{};
};

struct EvolutionRule {
    std::uint8_t species_dex{};
    std::uint8_t target_species_dex{};
    EvolutionMethod method{EvolutionMethod::level};
    std::uint8_t parameter{};
    std::uint8_t minimum_level{};
};

struct GrowthCurveRule {
    std::uint8_t id{};
    std::string key;
    std::array<std::uint32_t, 100> experience_by_level{};
};

struct MachineRule {
    std::uint8_t index{};
    std::uint8_t number{};
    std::uint8_t move_id{};
    bool hidden_machine{};
};

struct RuleCatalog {
    std::filesystem::path source;
    std::vector<TypeRule> types;
    std::vector<TypeInteractionRule> type_interactions;
    std::vector<MoveRule> moves;
    std::vector<SpeciesRule> species;
    std::vector<LearnsetRule> learnsets;
    std::vector<EvolutionRule> evolutions;
    std::vector<GrowthCurveRule> growth_curves;
    std::vector<MachineRule> machines;
    bool loaded{};
};

bool load_rules(const std::filesystem::path& path, RuleCatalog& result, std::string& error);
const TypeRule* find_type(const RuleCatalog& rules, std::uint8_t id);
const MoveRule* find_move(const RuleCatalog& rules, std::uint8_t id);
const SpeciesRule* find_species(const RuleCatalog& rules, std::uint8_t dex_number);
std::uint32_t experience_for_level(const RuleCatalog& rules, std::uint8_t growth_curve_id,
                                   std::uint8_t level);

} // namespace pokered
