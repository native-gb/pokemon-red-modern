#pragma once

#include "catalog.hpp"
#include "diagnostics.hpp"
#include "sexpr.hpp"

#include <cstdint>
#include <span>

namespace pokered {

enum class BattleResult : std::int64_t {
    none,
    won,
    lost,
    caught,
    escaped,
};

struct PredicateState {
    std::span<const std::uint8_t> flags;
    std::span<const std::int64_t> variables;
    std::span<const std::uint32_t> item_counts;
    std::span<const content::SpeciesId> party;
    std::span<const std::uint8_t> species_seen;
    std::span<const std::uint8_t> species_owned;
    std::span<const std::uint8_t> actors_visible;
    content::MapId current_map;
    std::int64_t player_level{};
    std::int64_t friendship{};
    BattleResult battle_result{BattleResult::none};
    std::uint32_t party_capacity{6};
};

bool compile_predicate(const sexpr::Form& source, const content::Catalog& catalog,
                       content::PredicateProgram& result, Diagnostics& diagnostics);
bool evaluate_predicate(const content::PredicateProgram& program, const PredicateState& state,
                        bool& result, Diagnostics& diagnostics);

} // namespace pokered
