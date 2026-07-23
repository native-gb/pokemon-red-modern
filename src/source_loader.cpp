#include "source_loader.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace pokered {

bool load_source_directory(const std::filesystem::path& root, std::vector<SourceDocument>& result,
                           Diagnostics& diagnostics) {
    std::error_code error;
    if (!std::filesystem::is_directory(root, error) || error) {
        add_error(diagnostics, {root.string(), 1, 1}, "source_directory_missing",
                  "source directory does not exist");
        return false;
    }

    // Discover only regular S-expression files and stabilize filesystem ordering.
    std::vector<std::filesystem::path> paths;
    for (std::filesystem::recursive_directory_iterator iterator(root, error), end;
         iterator != end && !error; iterator.increment(error)) {
        if (iterator->is_regular_file(error) && iterator->path().extension() == ".sexpr")
            paths.push_back(iterator->path());
    }
    if (error) {
        add_error(diagnostics, {root.string(), 1, 1}, "source_directory_read_failed",
                  "could not enumerate source directory: " + error.message());
        return false;
    }
    std::sort(paths.begin(), paths.end());

    // Parse every file independently so diagnostics retain readable relative paths.
    std::vector<SourceDocument> loaded;
    loaded.reserve(paths.size());
    for (const std::filesystem::path& path : paths) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            add_error(diagnostics, {path.string(), 1, 1}, "source_file_read_failed",
                      "could not open source file");
            continue;
        }
        const std::string text{std::istreambuf_iterator<char>(stream),
                               std::istreambuf_iterator<char>()};
        SourceDocument source;
        source.path = std::filesystem::relative(path, root, error);
        if (error) {
            source.path = path.filename();
            error.clear();
        }
        if (sexpr::parse(source.path.generic_string(), text, source.document, diagnostics))
            loaded.push_back(std::move(source));
    }
    if (!diagnostics.ok()) return false;
    result = std::move(loaded);
    return true;
}

} // namespace pokered
