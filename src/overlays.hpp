#pragma once

#include "diagnostics.hpp"
#include "sexpr.hpp"
#include "symbols.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace pokered::content {

enum class PatchOperation {
    define,
    override_record,
};

struct FieldContribution {
    Symbol package;
    SourceSpan source;
};

struct PatchField {
    Symbol name;
    std::vector<sexpr::Form> forms;
    SourceSpan source;
};

struct RecordPatch {
    PatchOperation operation{PatchOperation::define};
    Symbol domain;
    Symbol key;
    std::vector<PatchField> fields;
    SourceSpan source;
};

struct PackageSource {
    Symbol id;
    std::vector<sexpr::Form> metadata;
    std::vector<RecordPatch> records;
    SourceSpan source;
};

struct ResolvedField {
    Symbol name;
    std::vector<sexpr::Form> forms;
    std::vector<FieldContribution> history;
};

struct ResolvedRecord {
    Symbol domain;
    Symbol key;
    Symbol defined_by;
    SourceSpan source;
    std::vector<ResolvedField> fields;
};

struct ResolvedRecords {
    std::vector<ResolvedRecord> records;
    std::unordered_map<std::string, std::size_t> indexes_by_key;
};

bool read_package(const sexpr::Document& document, PackageSource& result, Diagnostics& diagnostics);
bool resolve_packages(std::span<const PackageSource> packages, ResolvedRecords& result,
                      Diagnostics& diagnostics);

const ResolvedRecord* find_record(const ResolvedRecords& records, const Symbol& domain,
                                  const Symbol& key);
const ResolvedField* find_field(const ResolvedRecord& record, const Symbol& name);

} // namespace pokered::content
