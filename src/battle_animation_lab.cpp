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
    targets[0].x = 42.0F;
    targets[0].y = 92.0F;
    targets[0].visible = true;
    targets[1].name = Symbol{"defender"};
    targets[1].x = 116.0F;
    targets[1].y = 42.0F;
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
    result = static_cast<std::uint32_t>(bytes[0]) |
             static_cast<std::uint32_t>(bytes[1]) << 8U |
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

    // Start the first program with the fixed semantic targets exposed by the battle view.
    loaded.loaded = true;
    if (!start_current(loaded, diagnostics)) return false;
    result = std::move(loaded);
    return true;
}

bool reload_battle_animation_lab(BattleAnimationLab& lab, Diagnostics& diagnostics) {
    BattleAnimationLab reloaded;
    if (!load_battle_animation_lab(lab.source_root, reloaded, diagnostics)) return false;
    reloaded.auto_advance = lab.auto_advance;
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

std::string_view battle_animation_lab_name(const BattleAnimationLab& lab) {
    if (!lab.loaded || lab.current >= lab.entries.size()) return "unavailable";
    return lab.entries[lab.current].name.text;
}

const ImportedAnimationVisual* find_imported_animation_visual(
    const ImportedAnimationAssets& assets, const Symbol& name) {
    const auto found =
        std::find_if(assets.visuals.begin(), assets.visuals.end(),
                     [&name](const ImportedAnimationVisual& visual) { return visual.name == name; });
    return found == assets.visuals.end() ? nullptr : &*found;
}

} // namespace pokered
