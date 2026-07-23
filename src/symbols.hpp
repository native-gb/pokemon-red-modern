#pragma once

#include "diagnostics.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace pokered {

struct Symbol {
    std::string text;

    bool empty() const;
    bool operator==(const Symbol&) const = default;
};

struct SymbolHash {
    std::size_t operator()(const Symbol& symbol) const noexcept;
};

bool valid_symbol(std::string_view text);
bool read_symbol(std::string_view text, const SourceSpan& source, Symbol& result,
                 Diagnostics& diagnostics);

} // namespace pokered
