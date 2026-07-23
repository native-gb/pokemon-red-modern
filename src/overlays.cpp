#include "overlays.hpp"

#include <algorithm>
#include <utility>

namespace pokered::content {
namespace {

const Symbol* symbol_argument(const sexpr::Form& form, std::size_t index,
                              Diagnostics& diagnostics) {
    const sexpr::Atom* atom = sexpr::argument(form, index);
    if (atom == nullptr || atom->kind != sexpr::AtomKind::symbol) {
        add_error(diagnostics, form.source, "expected_symbol_argument",
                  "form '" + form.head.symbol.text + "' requires a symbol argument");
        return nullptr;
    }
    return &atom->symbol;
}

PatchField* find_patch_field(std::vector<PatchField>& fields, const Symbol& name) {
    const auto found = std::find_if(fields.begin(), fields.end(), [&name](const PatchField& field) {
        return field.name == name;
    });
    return found == fields.end() ? nullptr : &*found;
}

ResolvedField* find_resolved_field(std::vector<ResolvedField>& fields, const Symbol& name) {
    const auto found =
        std::find_if(fields.begin(), fields.end(),
                     [&name](const ResolvedField& field) { return field.name == name; });
    return found == fields.end() ? nullptr : &*found;
}

std::string record_lookup_key(const Symbol& domain, const Symbol& key) {
    return domain.text + '\x1f' + key.text;
}

bool read_record_patch(const sexpr::Form& form, PatchOperation operation, RecordPatch& result,
                       Diagnostics& diagnostics) {
    const Symbol* domain = symbol_argument(form, 0, diagnostics);
    const Symbol* key = symbol_argument(form, 1, diagnostics);
    if (domain == nullptr || key == nullptr) return false;
    if (form.arguments.size() != 2) {
        add_error(diagnostics, form.source, "invalid_record_header",
                  "define and override forms require exactly a domain and key");
        return false;
    }

    // Group repeated child forms under one field so ordered members replace together.
    RecordPatch patch{
        .operation = operation,
        .domain = *domain,
        .key = *key,
        .fields = {},
        .source = form.source,
    };
    for (const sexpr::Form& child : form.children) {
        PatchField* field = find_patch_field(patch.fields, child.head.symbol);
        if (field == nullptr) {
            patch.fields.push_back({
                .name = child.head.symbol,
                .forms = {},
                .source = child.source,
            });
            field = &patch.fields.back();
        }
        field->forms.push_back(child);
    }
    result = std::move(patch);
    return true;
}

void define_record(const PackageSource& package, const RecordPatch& patch, ResolvedRecords& records,
                   Diagnostics& diagnostics) {
    const std::string lookup = record_lookup_key(patch.domain, patch.key);
    if (records.indexes_by_key.contains(lookup)) {
        add_error(diagnostics, patch.source, "duplicate_record_definition",
                  "record '" + patch.domain.text + "." + patch.key.text + "' is already defined");
        return;
    }

    // Preserve every defining field and its first package contribution.
    ResolvedRecord record{
        .domain = patch.domain,
        .key = patch.key,
        .defined_by = package.id,
        .source = patch.source,
        .fields = {},
    };
    for (const PatchField& source : patch.fields) {
        record.fields.push_back({
            .name = source.name,
            .forms = source.forms,
            .history = {{package.id, source.source}},
        });
    }
    records.indexes_by_key.emplace(lookup, records.records.size());
    records.records.push_back(std::move(record));
}

void override_record(const PackageSource& package, const RecordPatch& patch,
                     ResolvedRecords& records, Diagnostics& diagnostics) {
    const std::string lookup = record_lookup_key(patch.domain, patch.key);
    const auto found = records.indexes_by_key.find(lookup);
    if (found == records.indexes_by_key.end()) {
        add_error(diagnostics, patch.source, "missing_override_target",
                  "record '" + patch.domain.text + "." + patch.key.text + "' does not exist");
        return;
    }

    // Replace named fields while retaining the complete contribution history.
    ResolvedRecord& record = records.records[found->second];
    for (const PatchField& source : patch.fields) {
        ResolvedField* field = find_resolved_field(record.fields, source.name);
        if (field == nullptr) {
            add_error(diagnostics, source.source, "missing_override_field",
                      "record '" + patch.domain.text + "." + patch.key.text +
                          "' has no field named '" + source.name.text + "'");
            continue;
        }
        field->forms = source.forms;
        field->history.push_back({package.id, source.source});
    }
}

} // namespace

bool read_package(const sexpr::Document& document, PackageSource& result,
                  Diagnostics& diagnostics) {
    if (document.forms.size() != 1 || !sexpr::is_head(document.forms.front(), "package")) {
        add_error(diagnostics, {document.source_name, 1, 1}, "expected_package",
                  "a package file must contain one top-level package form");
        return false;
    }

    const sexpr::Form& root = document.forms.front();
    const Symbol* package_id = symbol_argument(root, 0, diagnostics);
    if (package_id == nullptr || root.arguments.size() != 1) {
        add_error(diagnostics, root.source, "invalid_package_header",
                  "package form requires exactly one package ID");
        return false;
    }

    // Separate package metadata from ordered record operations.
    PackageSource package{
        .id = *package_id,
        .metadata = {},
        .records = {},
        .source = root.source,
    };
    for (const sexpr::Form& child : root.children) {
        if (sexpr::is_head(child, "define") || sexpr::is_head(child, "override")) {
            RecordPatch patch;
            const PatchOperation operation = sexpr::is_head(child, "define")
                                                 ? PatchOperation::define
                                                 : PatchOperation::override_record;
            if (read_record_patch(child, operation, patch, diagnostics))
                package.records.push_back(std::move(patch));
        } else {
            package.metadata.push_back(child);
        }
    }
    if (!diagnostics.ok()) return false;
    result = std::move(package);
    return true;
}

bool resolve_packages(std::span<const PackageSource> packages, ResolvedRecords& result,
                      Diagnostics& diagnostics) {
    ResolvedRecords records;

    // Apply packages and their record operations in caller-supplied deterministic order.
    for (const PackageSource& package : packages) {
        for (const RecordPatch& patch : package.records) {
            if (patch.operation == PatchOperation::define)
                define_record(package, patch, records, diagnostics);
            else
                override_record(package, patch, records, diagnostics);
        }
    }
    if (!diagnostics.ok()) return false;
    result = std::move(records);
    return true;
}

const ResolvedRecord* find_record(const ResolvedRecords& records, const Symbol& domain,
                                  const Symbol& key) {
    const auto found = records.indexes_by_key.find(record_lookup_key(domain, key));
    return found == records.indexes_by_key.end() ? nullptr : &records.records[found->second];
}

const ResolvedField* find_field(const ResolvedRecord& record, const Symbol& name) {
    const auto found =
        std::find_if(record.fields.begin(), record.fields.end(),
                     [&name](const ResolvedField& field) { return field.name == name; });
    return found == record.fields.end() ? nullptr : &*found;
}

} // namespace pokered::content
