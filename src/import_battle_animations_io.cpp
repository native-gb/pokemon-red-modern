#include "import_battle_animations_io.hpp"

#include <chrono>
#include <fstream>
#include <system_error>

namespace pokered::import {
namespace {

bool write_file(const std::filesystem::path& path,
                const std::vector<std::uint8_t>& bytes, std::string& error) {
    std::error_code filesystem_error;
    std::filesystem::create_directories(path.parent_path(), filesystem_error);
    if (filesystem_error) {
        error = "could not create import directory " + path.parent_path().string() +
                ": " + filesystem_error.message();
        return false;
    }
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        error = "could not open generated file " + path.string();
        return false;
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (output) return true;
    error = "could not write generated file " + path.string();
    return false;
}

} // namespace

bool write_battle_animation_import(const BattleAnimationImport& imported,
                                   const std::filesystem::path& output_root,
                                   std::string& error) {
    error.clear();
    if (output_root.empty() || output_root == output_root.root_path()) {
        error = "refusing to replace an empty or filesystem-root import path";
        return false;
    }
    const std::filesystem::path destination =
        std::filesystem::absolute(output_root).lexically_normal();

    std::error_code filesystem_error;
    std::filesystem::create_directories(destination.parent_path(), filesystem_error);
    if (filesystem_error) {
        error = "could not create import parent directory: " +
                filesystem_error.message();
        return false;
    }
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path temporary =
        destination.parent_path() /
        ("." + destination.filename().string() + ".temporary." +
         std::to_string(nonce));
    const std::filesystem::path backup =
        destination.parent_path() /
        ("." + destination.filename().string() + ".backup." +
         std::to_string(nonce));

    for (const GeneratedFile& file : imported.files) {
        const std::filesystem::path relative(file.relative_path);
        if (relative.empty() || relative.is_absolute() ||
            relative.lexically_normal().string().starts_with("..")) {
            error = "importer produced an unsafe relative path: " +
                    file.relative_path;
            std::filesystem::remove_all(temporary, filesystem_error);
            return false;
        }
        if (!write_file(temporary / relative, file.bytes, error)) {
            std::filesystem::remove_all(temporary, filesystem_error);
            return false;
        }
    }

    const bool had_previous = std::filesystem::exists(destination);
    if (had_previous) {
        std::filesystem::rename(destination, backup, filesystem_error);
        if (filesystem_error) {
            error = "could not preserve previous import: " +
                    filesystem_error.message();
            std::filesystem::remove_all(temporary, filesystem_error);
            return false;
        }
    }
    std::filesystem::rename(temporary, destination, filesystem_error);
    if (filesystem_error) {
        error = "could not install generated import: " +
                filesystem_error.message();
        if (had_previous) {
            std::error_code restore_error;
            std::filesystem::rename(backup, destination, restore_error);
            if (restore_error)
                error += "; previous import remains at " + backup.string();
        }
        std::filesystem::remove_all(temporary, filesystem_error);
        return false;
    }
    if (had_previous) std::filesystem::remove_all(backup, filesystem_error);
    return true;
}

} // namespace pokered::import
