#include "battle_animation_lab.hpp"

#include "source_loader.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <utility>

namespace pokered {
namespace {

std::array<AnimationTarget, 3> battle_targets() {
    std::array<AnimationTarget, 3> targets;
    targets[0].name = Symbol{"attacker"};
    targets[0].x = 36.0F;
    targets[0].y = 68.0F;
    targets[0].visible = true;
    targets[1].name = Symbol{"defender"};
    targets[1].x = 124.0F;
    targets[1].y = 28.0F;
    targets[1].visible = true;
    targets[2].name = Symbol{"battle_screen"};
    targets[2].visible = true;
    return targets;
}

bool start_current(BattleAnimationLab& lab, Diagnostics& diagnostics) {
    if (lab.entries.empty() || lab.current >= lab.entries.size()) return false;
    const auto targets = battle_targets();
    lab.finished_ticks = 0;
    return start_animation(lab.entries[lab.current].program, targets, lab.animation, diagnostics);
}

bool read_u16(std::istream& input, std::uint16_t& result) {
    std::array<unsigned char, 2> bytes{};
    if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) return false;
    result = static_cast<std::uint16_t>(bytes[0]) |
             static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8U);
    return true;
}

bool read_u32(std::istream& input, std::uint32_t& result) {
    std::array<unsigned char, 4> bytes{};
    if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) return false;
    result = static_cast<std::uint32_t>(bytes[0]) | static_cast<std::uint32_t>(bytes[1]) << 8U |
             static_cast<std::uint32_t>(bytes[2]) << 16U |
             static_cast<std::uint32_t>(bytes[3]) << 24U;
    return true;
}

bool read_i16(std::istream& input, std::int16_t& result) {
    std::uint16_t raw = 0;
    if (!read_u16(input, raw)) return false;
    result = static_cast<std::int16_t>(raw);
    return true;
}

bool load_imported_assets(const std::filesystem::path& path, ImportedAnimationAssets& result,
                          Diagnostics& diagnostics) {
    if (!std::filesystem::exists(path)) return true;
    std::ifstream input(path, std::ios::binary);
    std::array<char, 4> magic{};
    std::uint16_t tile_count_0 = 0;
    std::uint16_t tile_count_1 = 0;
    std::uint32_t visual_count = 0;
    if (!input.read(magic.data(), magic.size()) || magic != std::array{'P', 'R', 'A', '1'} ||
        !read_u16(input, tile_count_0) || !read_u16(input, tile_count_1) ||
        !read_u32(input, visual_count)) {
        add_error(diagnostics, {path.string(), 1, 1}, "invalid_animation_asset_header",
                  "imported battle-animation asset file has an invalid header");
        return false;
    }
    constexpr std::uint16_t maximum_tiles = 256;
    constexpr std::uint32_t maximum_visuals = 100000;
    if (tile_count_0 > maximum_tiles || tile_count_1 > maximum_tiles ||
        visual_count > maximum_visuals) {
        add_error(diagnostics, {path.string(), 1, 1}, "invalid_animation_asset_counts",
                  "imported battle-animation asset counts exceed supported limits");
        return false;
    }

    ImportedAnimationAssets loaded;
    loaded.tile_set_0.resize(static_cast<std::size_t>(tile_count_0) * 16U);
    loaded.tile_set_1.resize(static_cast<std::size_t>(tile_count_1) * 16U);
    if (!input.read(reinterpret_cast<char*>(loaded.tile_set_0.data()),
                    static_cast<std::streamsize>(loaded.tile_set_0.size())) ||
        !input.read(reinterpret_cast<char*>(loaded.tile_set_1.data()),
                    static_cast<std::streamsize>(loaded.tile_set_1.size()))) {
        add_error(diagnostics, {path.string(), 1, 1}, "truncated_animation_tiles",
                  "imported battle-animation tile data is truncated");
        return false;
    }

    loaded.visuals.reserve(visual_count);
    for (std::uint32_t visual_index = 0; visual_index < visual_count; ++visual_index) {
        std::uint16_t name_size = 0;
        std::uint16_t piece_count = 0;
        if (!read_u16(input, name_size) || !read_u16(input, piece_count) || name_size == 0 ||
            name_size > 255 || piece_count > 40) {
            add_error(diagnostics, {path.string(), 1, 1}, "invalid_animation_visual",
                      "imported battle-animation visual header is invalid");
            return false;
        }
        std::string name(name_size, '\0');
        if (!input.read(name.data(), static_cast<std::streamsize>(name.size()))) {
            add_error(diagnostics, {path.string(), 1, 1}, "truncated_animation_visual_name",
                      "imported battle-animation visual name is truncated");
            return false;
        }
        ImportedAnimationVisual visual;
        if (!read_symbol(name, {path.string(), 1, 1}, visual.name, diagnostics)) return false;
        visual.pieces.reserve(piece_count);
        for (std::uint16_t piece_index = 0; piece_index < piece_count; ++piece_index) {
            ImportedAnimationPiece piece;
            std::array<unsigned char, 3> bytes{};
            if (!read_i16(input, piece.x) || !read_i16(input, piece.y) ||
                !input.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) {
                add_error(diagnostics, {path.string(), 1, 1}, "truncated_animation_piece",
                          "imported battle-animation sprite piece is truncated");
                return false;
            }
            piece.tile_set = bytes[0];
            piece.tile = bytes[1];
            piece.attributes = bytes[2];
            const std::vector<std::uint8_t>& tiles =
                piece.tile_set == 0 ? loaded.tile_set_0 : loaded.tile_set_1;
            if (piece.tile_set > 1 ||
                static_cast<std::size_t>(piece.tile) * 16U + 16U > tiles.size()) {
                add_error(diagnostics, {path.string(), 1, 1}, "invalid_animation_piece",
                          "imported battle-animation sprite piece references an invalid tile");
                return false;
            }
            visual.pieces.push_back(piece);
        }
        loaded.visuals.push_back(std::move(visual));
    }
    result = std::move(loaded);
    return true;
}

bool load_procedural_assets(const std::filesystem::path& path, ImportedAnimationAssets& result,
                            Diagnostics& diagnostics) {
    if (!std::filesystem::exists(path)) return true;
    std::ifstream input(path, std::ios::binary);
    std::array<char, 4> magic{};
    std::uint16_t wave_count = 0;
    std::uint16_t minimized_row_count = 0;
    std::uint16_t substitute_tile_count = 0;
    std::uint16_t palette_count = 0;
    if (!input.read(magic.data(), magic.size()) || magic != std::array{'P', 'R', 'P', '1'} ||
        !read_u16(input, wave_count) || !read_u16(input, minimized_row_count) ||
        !read_u16(input, substitute_tile_count) || !read_u16(input, palette_count) ||
        wave_count > 256 || minimized_row_count > 8 || substitute_tile_count > 16 ||
        palette_count > 32) {
        add_error(diagnostics, {path.string(), 1, 1}, "invalid_animation_procedural_header",
                  "imported procedural animation cache has an invalid header");
        return false;
    }

    result.wave_offsets.resize(wave_count);
    result.minimized_mon_rows.resize(minimized_row_count);
    result.substitute_mon_tiles.resize(static_cast<std::size_t>(substitute_tile_count) * 16U);
    result.long_flash_dmg_palettes.resize(palette_count);
    result.long_flash_sgb_palettes.resize(palette_count);
    if (!input.read(reinterpret_cast<char*>(result.wave_offsets.data()),
                    static_cast<std::streamsize>(result.wave_offsets.size())) ||
        !input.read(reinterpret_cast<char*>(result.minimized_mon_rows.data()),
                    static_cast<std::streamsize>(result.minimized_mon_rows.size())) ||
        !input.read(reinterpret_cast<char*>(result.substitute_mon_tiles.data()),
                    static_cast<std::streamsize>(result.substitute_mon_tiles.size())) ||
        !input.read(reinterpret_cast<char*>(result.long_flash_dmg_palettes.data()),
                    static_cast<std::streamsize>(result.long_flash_dmg_palettes.size())) ||
        !input.read(reinterpret_cast<char*>(result.long_flash_sgb_palettes.data()),
                    static_cast<std::streamsize>(result.long_flash_sgb_palettes.size()))) {
        add_error(diagnostics, {path.string(), 1, 1}, "truncated_animation_procedural_data",
                  "imported procedural animation cache is truncated");
        return false;
    }
    return true;
}

bool read_picture(std::istream& input, ImportedBattlePicture& result) {
    std::array<unsigned char, 2> dimensions{};
    std::uint32_t pixel_count = 0;
    if (!input.read(reinterpret_cast<char*>(dimensions.data()), dimensions.size()) ||
        !read_u32(input, result.rom_offset) || !read_u32(input, result.compressed_size) ||
        !read_u32(input, pixel_count))
        return false;
    result.width_tiles = dimensions[0];
    result.height_tiles = dimensions[1];
    const std::size_t width = static_cast<std::size_t>(result.width_tiles) * 8U;
    const std::size_t height = static_cast<std::size_t>(result.height_tiles) * 8U;
    if (result.width_tiles == 0 || result.height_tiles == 0 || width * height != pixel_count)
        return false;
    result.pixels.resize(pixel_count);
    return input
        .read(reinterpret_cast<char*>(result.pixels.data()),
              static_cast<std::streamsize>(result.pixels.size()))
        .good();
}

bool read_name(std::istream& input, std::string& result) {
    std::uint16_t size = 0;
    if (!read_u16(input, size) || size == 0 || size > 64) return false;
    result.resize(size);
    return input.read(result.data(), static_cast<std::streamsize>(result.size())).good();
}

bool load_battle_pictures(const std::filesystem::path& path, ImportedAnimationAssets& result,
                          Diagnostics& diagnostics) {
    if (!std::filesystem::exists(path)) return true;
    // Load normalized pixels only; cartridge decompression never enters the hot loop.
    std::ifstream input(path, std::ios::binary);
    std::array<char, 4> magic{};
    std::uint16_t species_count = 0;
    std::uint16_t trainer_count = 0;
    if (!input.read(magic.data(), magic.size()) || magic != std::array{'P', 'G', 'P', '1'} ||
        !read_u16(input, species_count) || !read_u16(input, trainer_count) ||
        species_count != 151 || trainer_count != 47) {
        add_error(diagnostics, {path.string(), 1, 1}, "invalid_battle_picture_header",
                  "imported battle-picture cache has an invalid header or record count");
        return false;
    }

    result.pokemon.reserve(species_count);
    for (std::uint16_t index = 0; index < species_count; ++index) {
        ImportedPokemonVisual visual;
        if (!read_name(input, visual.name) || !read_picture(input, visual.front) ||
            !read_picture(input, visual.back) || visual.front.width_tiles < 5 ||
            visual.front.width_tiles > 7 || visual.front.width_tiles != visual.front.height_tiles ||
            visual.back.width_tiles != 4 || visual.back.height_tiles != 4) {
            add_error(diagnostics, {path.string(), 1, 1}, "invalid_pokemon_picture",
                      "imported Pokemon picture record is invalid or truncated");
            return false;
        }
        result.pokemon.push_back(std::move(visual));
    }
    result.trainers.reserve(trainer_count);
    for (std::uint16_t index = 0; index < trainer_count; ++index) {
        ImportedTrainerVisual visual;
        if (!read_name(input, visual.name) || !read_picture(input, visual.portrait) ||
            visual.portrait.width_tiles != 7 || visual.portrait.height_tiles != 7) {
            add_error(diagnostics, {path.string(), 1, 1}, "invalid_trainer_picture",
                      "imported trainer portrait record is invalid or truncated");
            return false;
        }
        result.trainers.push_back(std::move(visual));
    }
    return true;
}

bool load_battle_ui_tiles(const std::filesystem::path& path, ImportedAnimationAssets& result,
                          Diagnostics& diagnostics) {
    if (!std::filesystem::exists(path)) return true;
    std::ifstream input(path, std::ios::binary);
    std::array<char, 4> magic{};
    std::uint16_t tile_count = 0;
    if (!input.read(magic.data(), magic.size()) || magic != std::array{'P', 'U', 'I', '1'} ||
        !read_u16(input, tile_count) || tile_count != 256) {
        add_error(diagnostics, {path.string(), 1, 1}, "invalid_battle_ui_header",
                  "imported battle UI cache has an invalid header or tile count");
        return false;
    }
    result.battle_ui_tiles.resize(static_cast<std::size_t>(tile_count) * 64U);
    if (!input.read(reinterpret_cast<char*>(result.battle_ui_tiles.data()),
                    static_cast<std::streamsize>(result.battle_ui_tiles.size()))) {
        add_error(diagnostics, {path.string(), 1, 1}, "truncated_battle_ui_tiles",
                  "imported battle UI tile pixels are truncated");
        return false;
    }
    return true;
}

std::filesystem::path battle_ui_source_root(const std::filesystem::path& animation_root) {
    const std::filesystem::path imported =
        animation_root.parent_path().parent_path() / "ui";
    if (std::filesystem::exists(imported)) return imported;
    return animation_root.parent_path() / "battle_ui";
}

} // namespace

bool load_battle_animation_lab(const std::filesystem::path& source_root, BattleAnimationLab& result,
                               Diagnostics& diagnostics) {
    std::vector<SourceDocument> sources;
    if (!load_source_directory(source_root, sources, diagnostics)) return false;

    // Compile every top-level animation and reject unrelated forms in this narrow source tree.
    BattleAnimationLab loaded;
    loaded.source_root = source_root;
    const std::filesystem::path imported_assets =
        source_root.parent_path().parent_path().parent_path() / "compiled" /
        "battle_animation_frames.bin";
    if (!load_imported_assets(imported_assets, loaded.imported_assets, diagnostics)) return false;
    const std::filesystem::path procedural_assets =
        imported_assets.parent_path() / "battle_animation_procedural.bin";
    if (!load_procedural_assets(procedural_assets, loaded.imported_assets, diagnostics))
        return false;
    const std::filesystem::path battle_pictures =
        imported_assets.parent_path() / "battle_pictures.bin";
    if (!load_battle_pictures(battle_pictures, loaded.imported_assets, diagnostics)) return false;
    const std::filesystem::path battle_ui_tiles =
        imported_assets.parent_path() / "battle_ui_tiles.bin";
    if (!load_battle_ui_tiles(battle_ui_tiles, loaded.imported_assets, diagnostics)) return false;
    for (const SourceDocument& source : sources) {
        for (const sexpr::Form& form : source.document.forms) {
            if (!sexpr::is_head(form, "animation") || form.arguments.size() != 1 ||
                form.arguments.front().kind != sexpr::AtomKind::symbol) {
                add_error(diagnostics, form.source, "unexpected_battle_lab_form",
                          "battle animation lab files may contain only named animations");
                continue;
            }
            content::AnimationProgram program;
            if (!compile_animation(form, loaded.catalog, program, diagnostics)) continue;
            loaded.entries.push_back({
                .name = form.arguments.front().symbol,
                .program = std::move(program),
            });
        }
    }
    if (!diagnostics.ok()) return false;
    if (loaded.entries.empty()) {
        add_error(diagnostics, {source_root.string(), 1, 1}, "battle_lab_empty",
                  "battle animation lab has no animation programs");
        return false;
    }

    // Load the imported battle layout, or the small authored fixture used without a cartridge.
    const std::filesystem::path ui_root = battle_ui_source_root(source_root);
    if (!load_battle_ui_source(ui_root, loaded.ui, diagnostics)) return false;

    // Start the first program with the fixed semantic targets exposed by the battle view.
    const std::string_view initial_species = loaded.imported_assets.pokemon.empty()
                                                 ? std::string_view("POKEMON")
                                                 : loaded.imported_assets.pokemon.front().name;
    set_battle_ui_species(loaded.ui, initial_species);
    std::string ui_error;
    if (!compose_battle_ui(loaded.ui, loaded.ui_tile_map, ui_error)) {
        add_error(diagnostics, {source_root.string(), 1, 1}, "invalid_battle_ui_state", ui_error);
        return false;
    }
    loaded.loaded = true;
    if (!start_current(loaded, diagnostics)) return false;
    result = std::move(loaded);
    return true;
}

bool reload_battle_animation_lab(BattleAnimationLab& lab, Diagnostics& diagnostics) {
    BattleAnimationLab reloaded;
    if (!load_battle_animation_lab(lab.source_root, reloaded, diagnostics)) return false;
    reloaded.auto_advance = lab.auto_advance;
    if (!reloaded.entries.empty()) reloaded.current = lab.current % reloaded.entries.size();
    if (!reloaded.imported_assets.pokemon.empty())
        reloaded.current_species = lab.current_species % reloaded.imported_assets.pokemon.size();
    reloaded.ui = lab.ui;
    set_battle_ui_species(reloaded.ui, battle_animation_lab_species_name(reloaded));
    std::string ui_error;
    if (!compose_battle_ui(reloaded.ui, reloaded.ui_tile_map, ui_error)) {
        add_error(diagnostics, {lab.source_root.string(), 1, 1}, "invalid_battle_ui_state",
                  ui_error);
        return false;
    }
    if (!start_current(reloaded, diagnostics)) return false;
    lab = std::move(reloaded);
    return true;
}

void step_battle_animation_lab(BattleAnimationLab& lab) {
    if (!lab.loaded || lab.entries.empty()) return;
    if (!lab.animation.finished) {
        step_animation(lab.animation);
        return;
    }
    if (!lab.auto_advance || ++lab.finished_ticks < 45) return;
    next_battle_animation_lab(lab);
}

void restart_battle_animation_lab(BattleAnimationLab& lab) {
    if (!lab.loaded) return;
    Diagnostics diagnostics;
    (void)start_current(lab, diagnostics);
}

void next_battle_animation_lab(BattleAnimationLab& lab) {
    if (!lab.loaded || lab.entries.empty()) return;
    lab.current = (lab.current + 1) % lab.entries.size();
    restart_battle_animation_lab(lab);
}

void previous_battle_animation_lab(BattleAnimationLab& lab) {
    if (!lab.loaded || lab.entries.empty()) return;
    lab.current = lab.current == 0 ? lab.entries.size() - 1 : lab.current - 1;
    restart_battle_animation_lab(lab);
}

void next_battle_species(BattleAnimationLab& lab) {
    if (lab.imported_assets.pokemon.empty()) return;
    lab.current_species = (lab.current_species + 1U) % lab.imported_assets.pokemon.size();
    set_battle_ui_species(lab.ui, battle_animation_lab_species_name(lab));
    std::string error;
    (void)compose_battle_ui(lab.ui, lab.ui_tile_map, error);
    restart_battle_animation_lab(lab);
}

void previous_battle_species(BattleAnimationLab& lab) {
    if (lab.imported_assets.pokemon.empty()) return;
    lab.current_species = lab.current_species == 0 ? lab.imported_assets.pokemon.size() - 1U
                                                   : lab.current_species - 1U;
    set_battle_ui_species(lab.ui, battle_animation_lab_species_name(lab));
    std::string error;
    (void)compose_battle_ui(lab.ui, lab.ui_tile_map, error);
    restart_battle_animation_lab(lab);
}

void cycle_battle_ui_mode(BattleAnimationLab& lab) {
    next_battle_ui_mode(lab.ui);
    std::string error;
    (void)compose_battle_ui(lab.ui, lab.ui_tile_map, error);
}

void next_battle_ui_menu_selection(BattleAnimationLab& lab) {
    next_battle_ui_selection(lab.ui);
    std::string error;
    (void)compose_battle_ui(lab.ui, lab.ui_tile_map, error);
}

void previous_battle_ui_menu_selection(BattleAnimationLab& lab) {
    previous_battle_ui_selection(lab.ui);
    std::string error;
    (void)compose_battle_ui(lab.ui, lab.ui_tile_map, error);
}

void cycle_battle_ui_status(BattleAnimationLab& lab) {
    next_battle_ui_status(lab.ui);
    std::string error;
    (void)compose_battle_ui(lab.ui, lab.ui_tile_map, error);
}

void prepare_battle_view(BattleAnimationLab& lab) {
    if (!lab.loaded) return;
    lab.animation = {};
    const auto targets = battle_targets();
    lab.animation.targets.assign(targets.begin(), targets.end());
    lab.animation.finished = true;
}

std::string_view battle_animation_lab_name(const BattleAnimationLab& lab) {
    if (!lab.loaded || lab.current >= lab.entries.size()) return "unavailable";
    return lab.entries[lab.current].name.text;
}

std::string_view battle_animation_lab_species_name(const BattleAnimationLab& lab) {
    const ImportedPokemonVisual* visual = battle_animation_lab_species(lab);
    return visual == nullptr ? "placeholder" : std::string_view(visual->name);
}

const ImportedPokemonVisual* battle_animation_lab_species(const BattleAnimationLab& lab) {
    if (lab.current_species >= lab.imported_assets.pokemon.size()) return nullptr;
    return &lab.imported_assets.pokemon[lab.current_species];
}

const ImportedPokemonVisual* battle_view_player_species(
    const BattleAnimationLab& lab) {
    if (!lab.distinct_battlers)
        return battle_animation_lab_species(lab);
    if (lab.player_species >= lab.imported_assets.pokemon.size())
        return nullptr;
    return &lab.imported_assets.pokemon[lab.player_species];
}

const ImportedPokemonVisual* battle_view_enemy_species(
    const BattleAnimationLab& lab) {
    if (!lab.distinct_battlers)
        return battle_animation_lab_species(lab);
    if (lab.enemy_species >= lab.imported_assets.pokemon.size())
        return nullptr;
    return &lab.imported_assets.pokemon[lab.enemy_species];
}

const ImportedAnimationVisual* find_imported_animation_visual(const ImportedAnimationAssets& assets,
                                                              const Symbol& name) {
    const auto found = std::find_if(
        assets.visuals.begin(), assets.visuals.end(),
        [&name](const ImportedAnimationVisual& visual) { return visual.name == name; });
    return found == assets.visuals.end() ? nullptr : &*found;
}

} // namespace pokered
