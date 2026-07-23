#pragma once

#include "diagnostics.hpp"
#include "symbols.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pokered::sexpr {

enum class AtomKind {
    symbol,
    integer,
    decimal,
    string,
    boolean,
};

struct Atom {
    AtomKind kind{AtomKind::symbol};
    SourceSpan source;
    Symbol symbol;
    std::string string;
    std::int64_t integer{};
    double decimal{};
    bool boolean{};
};

struct Form {
    Atom head;
    std::vector<Atom> arguments;
    std::vector<Form> children;
    SourceSpan source;
};

struct Document {
    std::string source_name;
    std::vector<Form> forms;
};

bool parse(std::string_view source_name, std::string_view text, Document& result,
           Diagnostics& diagnostics);
std::string canonical(const Document& document);
std::string canonical(const Form& form);

const Atom* argument(const Form& form, std::size_t index);
bool is_head(const Form& form, std::string_view symbol);

} // namespace pokered::sexpr
