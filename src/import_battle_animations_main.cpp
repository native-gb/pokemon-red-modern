#include "import_battle_animations.hpp"
#include "import_battle_animations_io.hpp"
#include "import_maps.hpp"
#include "import_pictures.hpp"
#include "import_scripts.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
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
    pokered::import::MapImport maps;
    pokered::import::PictureImport pictures;
    pokered::import::ScriptImport scripts;
    std::string error;
    if (!pokered::import::decode_battle_animation_import(rom, imported, error) ||
        !pokered::import::decode_picture_import(rom, pictures, error) ||
        !pokered::import::decode_map_import(rom, maps, error) ||
        !pokered::import::decode_script_import(rom, scripts, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    imported.files.insert(imported.files.end(), std::make_move_iterator(pictures.files.begin()),
                          std::make_move_iterator(pictures.files.end()));
    imported.files.insert(imported.files.end(), std::make_move_iterator(maps.files.begin()),
                          std::make_move_iterator(maps.files.end()));
    imported.files.insert(imported.files.end(), std::make_move_iterator(scripts.files.begin()),
                          std::make_move_iterator(scripts.files.end()));
    const auto manifest =
        std::find_if(imported.files.begin(), imported.files.end(),
                     [](const auto& file) { return file.relative_path == "import_manifest"; });
    if (manifest == imported.files.end()) {
        std::cerr << "import domains produced no manifest\n";
        return 1;
    }
    std::ostringstream domain_manifest;
    domain_manifest << "picture_importer_version 1\n"
                    << "front_pictures " << pictures.front_pictures << '\n'
                    << "back_pictures " << pictures.back_pictures << '\n'
                    << "trainer_pictures " << pictures.trainer_classes << '\n'
                    << "world_importer_version 2\n"
                    << "map_slots 248\n"
                    << "active_maps " << maps.maps << '\n'
                    << "unused_map_slots " << maps.unused_map_slots << '\n'
                    << "world_spaces " << maps.world_spaces << '\n'
                    << "overworld_sprites " << maps.sprites << '\n'
                    << "map_program_importer_version 3\n"
                    << "map_slots " << scripts.map_slots << '\n'
                    << "decoded_map_programs " << scripts.decoded_maps << '\n'
                    << "unused_map_slots " << scripts.unresolved_slots << '\n'
                    << "decoded_map_text_programs " << scripts.decoded_text_programs << '\n'
                    << "decoded_map_interaction_scripts " << scripts.decoded_interaction_scripts
                    << '\n'
                    << "untranslated_map_interaction_scripts "
                    << scripts.untranslated_interaction_scripts << '\n'
                    << "unresolved_map_owned_entries " << scripts.unresolved_owned_entries << '\n';
    const std::string domain_manifest_text = domain_manifest.str();
    manifest->bytes.insert(manifest->bytes.end(), domain_manifest_text.begin(),
                           domain_manifest_text.end());
    if (!pokered::import::write_battle_animation_import(imported, output_root, error)) {
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
    std::cout << "Imported " << pictures.front_pictures << " front pictures, "
              << pictures.back_pictures << " back pictures, and " << pictures.trainer_classes
              << " trainer-class portraits\n";
    std::cout << "Picture cache: " << output_root / "compiled" / "battle_pictures.bin" << '\n';
    std::cout << "Battle UI cache: " << output_root / "compiled" / "battle_ui_tiles.bin" << '\n';
    std::cout << "Imported " << maps.maps << " active maps in " << maps.world_spaces
              << " world spaces through " << maps.tilesets << " tilesets into "
              << maps.expanded_tiles << " expanded tiles, " << maps.sprites
              << " overworld sprites, " << maps.actors << " actor spawns, and " << maps.warps
              << " warps\n";
    std::cout << "World map cache: " << output_root / "compiled" / "world_maps.bin" << '\n';
    std::cout << "Indexed " << scripts.script_entry_points << " map script entry points, "
              << scripts.owned_map_entries << " owned map-table entries, "
              << scripts.background_interactions << " background interactions, and "
              << scripts.actor_interactions << " actor interactions across " << scripts.map_slots
              << " ROM map slots (" << scripts.unresolved_slots << " unresolved slots)\n";
    std::cout << "Script inventory: " << output_root / "reports" / "script_import_summary.txt"
              << '\n';
    std::cout << "Text source: " << output_root / "source" / "text" / "maps" << '\n';
    return 0;
}
