#include "import_battle_animations.hpp"
#include "import_battle_animations_io.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

bool read_file(const std::filesystem::path& path, std::vector<std::uint8_t>& result) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::vector<char> bytes;
    bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    result.reserve(bytes.size());
    for (const char byte : bytes)
        result.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(byte)));
    return input.good() || input.eof();
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " ROM OUTPUT_DIRECTORY\n";
        return 2;
    }
    const std::filesystem::path rom_path = argv[1];
    const std::filesystem::path output_root = argv[2];
    std::vector<std::uint8_t> rom;
    if (!read_file(rom_path, rom)) {
        std::cerr << "could not read ROM: " << rom_path << '\n';
        return 1;
    }

    pokered::import::BattleAnimationImport imported;
    std::string error;
    if (!pokered::import::decode_battle_animation_import(rom, imported, error) ||
        !pokered::import::write_battle_animation_import(imported, output_root, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    std::cout << "Imported " << imported.animation_programs << " battle animations from "
              << rom_path << '\n';
    std::cout << "Readable scripts: " << output_root / "source" / "animations" / "battle_moves"
              << '\n';
    std::cout << "Frame assets: " << output_root / "compiled" / "battle_animation_frames.bin"
              << '\n';
    std::cout << "Procedural profile: "
              << output_root / "compiled" / "battle_animation_procedural.bin" << '\n';
    return 0;
}
