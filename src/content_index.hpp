#pragma once

#include "diagnostics.hpp"
#include "symbols.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace pokered::content {

template <class IdType, class Record> struct Index {
    std::vector<Record> records;
    std::vector<Symbol> keys;
    std::unordered_map<Symbol, std::uint32_t, SymbolHash> ids_by_key;
};

template <class IdType, class Record>
std::optional<IdType> add(Index<IdType, Record>& index, Symbol key, Record record,
                          const SourceSpan& source, Diagnostics& diagnostics) {
    // Reject duplicate stable keys before assigning the next dense runtime ID.
    if (index.ids_by_key.contains(key)) {
        add_error(diagnostics, source, "duplicate_content_key",
                  "content key '" + key.text + "' is already defined");
        return std::nullopt;
    }
    if (index.records.size() >= static_cast<std::size_t>(IdType::invalid_value)) {
        add_error(diagnostics, source, "content_index_overflow",
                  "content index cannot assign another dense ID");
        return std::nullopt;
    }

    // Keep records and keys in the same dense order used by runtime handles.
    const auto value = static_cast<std::uint32_t>(index.records.size());
    index.records.push_back(std::move(record));
    index.keys.push_back(key);
    index.ids_by_key.emplace(std::move(key), value);
    return IdType{value};
}

template <class IdType, class Record>
const Record* get(const Index<IdType, Record>& index, IdType id) {
    if (!id.valid() || id.value >= index.records.size()) return nullptr;
    return &index.records[id.value];
}

template <class IdType, class Record> Record* get(Index<IdType, Record>& index, IdType id) {
    if (!id.valid() || id.value >= index.records.size()) return nullptr;
    return &index.records[id.value];
}

template <class IdType, class Record>
std::optional<IdType> find(const Index<IdType, Record>& index, const Symbol& key) {
    const auto found = index.ids_by_key.find(key);
    if (found == index.ids_by_key.end()) return std::nullopt;
    return IdType{found->second};
}

template <class IdType, class Record>
const Symbol* key_for(const Index<IdType, Record>& index, IdType id) {
    if (!id.valid() || id.value >= index.keys.size()) return nullptr;
    return &index.keys[id.value];
}

} // namespace pokered::content
