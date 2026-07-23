#include "battle_animation_lab.hpp"

#include "source_loader.hpp"

#include <array>
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

} // namespace

bool load_battle_animation_lab(const std::filesystem::path& source_root, BattleAnimationLab& result,
                               Diagnostics& diagnostics) {
    std::vector<SourceDocument> sources;
    if (!load_source_directory(source_root, sources, diagnostics)) return false;

    // Compile every top-level animation and reject unrelated forms in this narrow source tree.
    BattleAnimationLab loaded;
    loaded.source_root = source_root;
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

} // namespace pokered
