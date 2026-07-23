#include "catalog.hpp"

#include <unordered_set>

namespace pokered::content {
namespace {

template <class IdType, class Record>
void validate_index_shape(const Index<IdType, Record>& index, std::string_view name,
                          Diagnostics& diagnostics) {
    if (index.records.size() != index.keys.size() ||
        index.records.size() != index.ids_by_key.size()) {
        add_error(diagnostics, {"<catalog>", 1, 1}, "invalid_index_shape",
                  std::string(name) + " index has mismatched record, key, or lookup counts");
    }
}

} // namespace

const char* label(PackState state) {
    switch (state) {
    case PackState::absent:
        return "Not imported";
    case PackState::importing:
        return "Importing";
    case PackState::partial:
        return "Partial development pack";
    case PackState::ready:
        return "Ready";
    case PackState::incompatible:
        return "Incompatible";
    }
    return "Unknown";
}

CatalogSummary summarize(const Catalog& catalog, PackState state) {
    return {
        .state = state,
        .campaign = catalog.manifest.campaign.text,
        .source = catalog.manifest.source_sha1,
        .maps = catalog.maps.records.size(),
        .scripts = catalog.scripts.records.size(),
        .species = catalog.species.records.size(),
        .moves = catalog.moves.records.size(),
        .items = catalog.items.records.size(),
    };
}

bool validate_catalog(const Catalog& catalog, Diagnostics& diagnostics) {
    // Check the parallel dense containers before validating domain references.
    validate_index_shape(catalog.texts, "text", diagnostics);
    validate_index_shape(catalog.maps, "map", diagnostics);
    validate_index_shape(catalog.scripts, "script", diagnostics);
    validate_index_shape(catalog.types, "type", diagnostics);
    validate_index_shape(catalog.species, "species", diagnostics);
    validate_index_shape(catalog.moves, "move", diagnostics);
    validate_index_shape(catalog.items, "item", diagnostics);
    validate_index_shape(catalog.animations, "animation", diagnostics);

    // Require exact nonzero rational values for every explicit type pair.
    std::unordered_set<std::uint64_t> type_pairs;
    for (const TypeInteractionDef& interaction : catalog.type_interactions.records) {
        if (interaction.multiplier.denominator == 0) {
            add_error(diagnostics, {"<catalog>", 1, 1}, "invalid_type_multiplier",
                      "type interaction denominator cannot be zero");
        }
        const std::uint64_t pair =
            (static_cast<std::uint64_t>(interaction.attacking.value) << 32U) |
            interaction.defending.value;
        if (!type_pairs.insert(pair).second) {
            add_error(diagnostics, {"<catalog>", 1, 1}, "duplicate_type_interaction",
                      "type interaction pair is defined more than once");
        }
    }

    // Reject map buffers whose dimensions cannot describe their compiled cells.
    for (const MapDef& map : catalog.maps.records) {
        if (map.width < 0 || map.height < 0) {
            add_error(diagnostics, {"<catalog>", 1, 1}, "invalid_map_size",
                      "map dimensions cannot be negative");
            continue;
        }
        const auto width = static_cast<std::size_t>(map.width);
        const auto height = static_cast<std::size_t>(map.height);
        if (width * height != map.cells.size() || map.cells.size() != map.collision.size()) {
            add_error(diagnostics, {"<catalog>", 1, 1}, "invalid_map_cells",
                      "map cell and collision counts must match its dimensions");
        }
    }
    return diagnostics.ok();
}

} // namespace pokered::content
