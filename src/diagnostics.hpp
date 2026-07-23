#pragma once

#include "source.hpp"

#include <string>
#include <vector>

namespace pokered {

enum class DiagnosticSeverity {
    warning,
    error,
};

struct Diagnostic {
    DiagnosticSeverity severity{DiagnosticSeverity::error};
    std::string code;
    std::string message;
    SourceSpan source;
};

struct Diagnostics {
    std::vector<Diagnostic> entries;

    bool ok() const;
    std::size_t error_count() const;
};

void add_error(Diagnostics& diagnostics, SourceSpan source, std::string code, std::string message);
void add_warning(Diagnostics& diagnostics, SourceSpan source, std::string code,
                 std::string message);
std::string format_diagnostic(const Diagnostic& diagnostic);

} // namespace pokered
