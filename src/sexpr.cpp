#include "sexpr.hpp"

#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace pokered::sexpr {
namespace {

struct ParsedLine {
    std::size_t indentation{};
    std::size_t line{};
    std::vector<Atom> atoms;
};

struct OpenForm {
    std::size_t indentation{};
    Form* form{};
};

bool parse_integer(std::string_view token, std::int64_t& result) {
    if (token.size() > 2U && token[0] == '0' &&
        (token[1] == 'x' || token[1] == 'X')) {
        std::uint64_t unsigned_result = 0U;
        const char* begin = token.data() + 2;
        const char* end = token.data() + token.size();
        const auto parsed =
            std::from_chars(begin, end, unsigned_result, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != end ||
            unsigned_result >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max()))
            return false;
        result = static_cast<std::int64_t>(unsigned_result);
        return true;
    }
    const char* begin = token.data();
    const char* end = token.data() + token.size();
    const auto parsed = std::from_chars(begin, end, result);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool parse_decimal(std::string_view token, double& result) {
    if (token.find('.') == std::string_view::npos) return false;
    const char* begin = token.data();
    const char* end = token.data() + token.size();
    const auto parsed = std::from_chars(begin, end, result);
    return parsed.ec == std::errc{} && parsed.ptr == end && std::isfinite(result);
}

char escaped_character(char character, bool& valid) {
    valid = true;
    switch (character) {
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case 't':
        return '\t';
    case '\\':
        return '\\';
    case '"':
        return '"';
    default:
        valid = false;
        return character;
    }
}

bool read_string(std::string_view line, std::size_t& cursor, SourceSpan source, Atom& result,
                 Diagnostics& diagnostics) {
    // Decode the small escape set while retaining the opening quote location.
    std::string value;
    ++cursor;
    while (cursor < line.size() && line[cursor] != '"') {
        if (line[cursor] != '\\') {
            value.push_back(line[cursor++]);
            continue;
        }
        if (cursor + 1 >= line.size()) {
            add_error(diagnostics, std::move(source), "unterminated_escape",
                      "string ends after an escape character");
            return false;
        }
        bool valid = false;
        const char decoded = escaped_character(line[cursor + 1], valid);
        if (!valid) {
            add_error(diagnostics, std::move(source), "invalid_escape",
                      "string contains an unsupported escape sequence");
            return false;
        }
        value.push_back(decoded);
        cursor += 2;
    }
    if (cursor >= line.size()) {
        add_error(diagnostics, std::move(source), "unterminated_string",
                  "string is missing its closing quote");
        return false;
    }

    ++cursor;
    result.kind = AtomKind::string;
    result.source = std::move(source);
    result.string = std::move(value);
    return true;
}

bool read_unquoted(std::string_view token, SourceSpan source, Atom& result,
                   Diagnostics& diagnostics) {
    // Classify literal tokens before falling back to a validated symbol.
    if (token == "true" || token == "false") {
        result.kind = AtomKind::boolean;
        result.source = std::move(source);
        result.boolean = token == "true";
        return true;
    }
    if (parse_integer(token, result.integer)) {
        result.kind = AtomKind::integer;
        result.source = std::move(source);
        return true;
    }
    if (parse_decimal(token, result.decimal)) {
        result.kind = AtomKind::decimal;
        result.source = std::move(source);
        return true;
    }

    result.kind = AtomKind::symbol;
    result.source = source;
    return read_symbol(token, source, result.symbol, diagnostics);
}

bool tokenize_line(std::string_view source_name, std::string_view line, std::size_t line_number,
                   ParsedLine& result, Diagnostics& diagnostics) {
    if (line.find('\t') != std::string_view::npos) {
        add_error(diagnostics, {std::string(source_name), line_number, 1}, "tab_indentation",
                  "tabs are not permitted in indented S-expression source");
        return false;
    }

    // Count leading spaces and skip empty or comment-only lines.
    std::size_t cursor = 0;
    while (cursor < line.size() && line[cursor] == ' ')
        ++cursor;
    result.indentation = cursor;
    result.line = line_number;
    if (cursor >= line.size() || line[cursor] == ';') return true;

    // Read atoms until the line ends or an unquoted comment begins.
    while (cursor < line.size()) {
        while (cursor < line.size() && line[cursor] == ' ')
            ++cursor;
        if (cursor >= line.size() || line[cursor] == ';') break;

        const std::size_t column = cursor + 1;
        Atom atom;
        if (line[cursor] == '"') {
            if (!read_string(line, cursor, {std::string(source_name), line_number, column}, atom,
                             diagnostics)) {
                return false;
            }
        } else {
            const std::size_t begin = cursor;
            while (cursor < line.size() && line[cursor] != ' ' && line[cursor] != ';')
                ++cursor;
            const std::string_view token = line.substr(begin, cursor - begin);
            if (!read_unquoted(token, {std::string(source_name), line_number, column}, atom,
                               diagnostics)) {
                return false;
            }
        }
        result.atoms.push_back(std::move(atom));
    }
    return true;
}

bool append_line(const ParsedLine& line, Document& document, std::vector<OpenForm>& open,
                 Diagnostics& diagnostics) {
    if (line.atoms.empty()) return true;
    if (line.atoms.front().kind != AtomKind::symbol && line.atoms.size() != 1) {
        add_error(diagnostics, line.atoms.front().source, "invalid_literal_expression",
                  "a literal expression cannot have inline arguments");
        return false;
    }

    // Close deeper forms and require every dedent to return to an existing level.
    bool closed_deeper_form = false;
    while (!open.empty() && open.back().indentation > line.indentation) {
        closed_deeper_form = true;
        open.pop_back();
    }
    if (closed_deeper_form &&
        (open.empty() ? line.indentation != 0 : open.back().indentation != line.indentation)) {
        add_error(diagnostics, line.atoms.front().source, "invalid_dedent",
                  "dedentation does not match an earlier indentation level");
        return false;
    }
    if (!open.empty() && open.back().indentation == line.indentation) {
        open.pop_back();
    }
    if (open.empty() && line.indentation != 0) {
        add_error(diagnostics, line.atoms.front().source, "unexpected_indentation",
                  "top-level forms must start in column one");
        return false;
    }
    if (!open.empty() && open.back().form->head.kind != AtomKind::symbol) {
        add_error(diagnostics, line.atoms.front().source, "literal_has_children",
                  "a literal expression cannot contain child expressions");
        return false;
    }

    // Append the new list to its parent, then keep its address for child lines.
    Form form;
    form.source = line.atoms.front().source;
    form.head = line.atoms.front();
    form.arguments.assign(line.atoms.begin() + 1, line.atoms.end());
    Form* inserted = nullptr;
    if (open.empty()) {
        document.forms.push_back(std::move(form));
        inserted = &document.forms.back();
    } else {
        open.back().form->children.push_back(std::move(form));
        inserted = &open.back().form->children.back();
    }
    open.push_back({line.indentation, inserted});
    return true;
}

std::string escaped(std::string_view value) {
    std::string result;
    result.push_back('"');
    for (const char character : value) {
        switch (character) {
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\\"";
            break;
        default:
            result.push_back(character);
            break;
        }
    }
    result.push_back('"');
    return result;
}

std::string print_atom(const Atom& atom) {
    switch (atom.kind) {
    case AtomKind::symbol:
        return atom.symbol.text;
    case AtomKind::integer:
        return std::to_string(atom.integer);
    case AtomKind::decimal: {
        std::ostringstream stream;
        stream << std::setprecision(std::numeric_limits<double>::max_digits10) << atom.decimal;
        return stream.str();
    }
    case AtomKind::string:
        return escaped(atom.string);
    case AtomKind::boolean:
        return atom.boolean ? "true" : "false";
    }
    return {};
}

void print_form(const Form& form, std::size_t depth, std::string& result) {
    result.append(depth * 2, ' ');
    if (form.head.kind != AtomKind::symbol) {
        result += print_atom(form.head);
        return;
    }
    result.push_back('(');
    result += form.head.symbol.text;
    for (const Atom& atom : form.arguments) {
        result.push_back(' ');
        result += print_atom(atom);
    }
    for (const Form& child : form.children) {
        result.push_back('\n');
        print_form(child, depth + 1, result);
    }
    result.push_back(')');
}

} // namespace

bool parse(std::string_view source_name, std::string_view text, Document& result,
           Diagnostics& diagnostics) {
    Document document;
    document.source_name.assign(source_name);
    std::vector<OpenForm> open;

    // Split without allocating a second copy of the source text.
    std::size_t line_number = 1;
    std::size_t begin = 0;
    while (begin <= text.size()) {
        const std::size_t newline = text.find('\n', begin);
        const std::size_t end = newline == std::string_view::npos ? text.size() : newline;
        std::string_view line = text.substr(begin, end - begin);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

        ParsedLine parsed;
        if (!tokenize_line(source_name, line, line_number, parsed, diagnostics) ||
            !append_line(parsed, document, open, diagnostics)) {
            return false;
        }
        if (newline == std::string_view::npos) break;
        begin = newline + 1;
        ++line_number;
    }

    result = std::move(document);
    return diagnostics.ok();
}

std::string canonical(const Document& document) {
    std::string result;
    for (std::size_t index = 0; index < document.forms.size(); ++index) {
        if (index != 0) result.push_back('\n');
        print_form(document.forms[index], 0, result);
    }
    return result;
}

std::string canonical(const Form& form) {
    std::string result;
    print_form(form, 0, result);
    return result;
}

const Atom* argument(const Form& form, std::size_t index) {
    if (index >= form.arguments.size()) return nullptr;
    return &form.arguments[index];
}

bool is_head(const Form& form, std::string_view symbol) {
    return form.head.kind == AtomKind::symbol && form.head.symbol.text == symbol;
}

} // namespace pokered::sexpr
