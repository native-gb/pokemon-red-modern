#include "diagnostics.hpp"

#include <algorithm>

namespace pokered {

bool Diagnostics::ok() const {
    return error_count() == 0;
}

std::size_t Diagnostics::error_count() const {
    return static_cast<std::size_t>(
        std::count_if(entries.begin(), entries.end(), [](const Diagnostic& entry) {
            return entry.severity == DiagnosticSeverity::error;
        }));
}

void add_error(Diagnostics& diagnostics, SourceSpan source, std::string code, std::string message) {
    diagnostics.entries.push_back({
        .severity = DiagnosticSeverity::error,
        .code = std::move(code),
        .message = std::move(message),
        .source = std::move(source),
    });
}

void add_warning(Diagnostics& diagnostics, SourceSpan source, std::string code,
                 std::string message) {
    diagnostics.entries.push_back({
        .severity = DiagnosticSeverity::warning,
        .code = std::move(code),
        .message = std::move(message),
        .source = std::move(source),
    });
}

std::string format_diagnostic(const Diagnostic& diagnostic) {
    const char* severity = diagnostic.severity == DiagnosticSeverity::error ? "error" : "warning";
    return diagnostic.source.file + ":" + std::to_string(diagnostic.source.line) + ":" +
           std::to_string(diagnostic.source.column) + ": " + severity + " [" + diagnostic.code +
           "]: " + diagnostic.message;
}

} // namespace pokered
