#include "symbols.hpp"

#include <functional>

namespace pokered {
namespace {

bool valid_segment(std::string_view text) {
    if (text.empty()) return false;
    if ((text.front() < 'a' || text.front() > 'z') && text.front() != '_') return false;

    for (const char character : text) {
        const bool lower = character >= 'a' && character <= 'z';
        const bool number = character >= '0' && character <= '9';
        if (!lower && !number && character != '_') return false;
    }
    return true;
}

} // namespace

bool Symbol::empty() const {
    return text.empty();
}

std::size_t SymbolHash::operator()(const Symbol& symbol) const noexcept {
    return std::hash<std::string>{}(symbol.text);
}

bool valid_symbol(std::string_view text) {
    // Validate every dotted namespace component under the same snake_case rule.
    while (!text.empty()) {
        const std::size_t dot = text.find('.');
        const std::string_view segment = text.substr(0, dot);
        if (!valid_segment(segment)) return false;
        if (dot == std::string_view::npos) return true;
        text.remove_prefix(dot + 1);
    }
    return false;
}

bool read_symbol(std::string_view text, const SourceSpan& source, Symbol& result,
                 Diagnostics& diagnostics) {
    if (!valid_symbol(text)) {
        add_error(diagnostics, source, "invalid_symbol",
                  "expected a snake_case symbol, got '" + std::string(text) + "'");
        return false;
    }
    result.text.assign(text);
    return true;
}

} // namespace pokered
