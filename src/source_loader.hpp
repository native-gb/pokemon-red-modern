#pragma once

#include "diagnostics.hpp"
#include "sexpr.hpp"

#include <filesystem>
#include <vector>

namespace pokered {

struct SourceDocument {
    std::filesystem::path path;
    sexpr::Document document;
};

bool load_source_directory(const std::filesystem::path& root, std::vector<SourceDocument>& result,
                           Diagnostics& diagnostics);

} // namespace pokered
