#include "animations.hpp"
#include "battle_animation_lab.hpp"
#include "catalog.hpp"
#include "content_index.hpp"
#include "overlays.hpp"
#include "predicates.hpp"
#include "sexpr.hpp"
#include "symbols.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace {

struct TestState {
    int failures{};
};

void check(TestState& state, bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++state.failures;
}

pokered::Symbol symbol(std::string text) {
    return {.text = std::move(text)};
}

void test_symbols(TestState& state) {
    // Accept source symbols used by content and reject other spelling conventions.
    check(state, pokered::valid_symbol("oak_leads_player"), "snake_case symbol accepted");
    check(state, pokered::valid_symbol("species.bulbasaur"), "dotted symbol accepted");
    check(state, !pokered::valid_symbol("oak-leads-player"), "kebab symbol rejected");
    check(state, !pokered::valid_symbol("OakLeadsPlayer"), "uppercase symbol rejected");
    check(state, !pokered::valid_symbol("9lives"), "numeric prefix rejected");
}

void test_sexpr(TestState& state) {
    constexpr std::string_view source = "effect counter\n"
                                        "    when\n"
                                        "        equal\n"
                                        "            last_physical_damage target user\n"
                                        "            0\n"
                                        "        fail\n"
                                        "        return\n"
                                        "    deal_fixed_damage target\n"
                                        "        multiply\n"
                                        "            last_physical_damage target user\n"
                                        "            2\n";

    // Parse one nested battle effect and verify its stable canonical form.
    pokered::Diagnostics diagnostics;
    pokered::sexpr::Document document;
    check(state, pokered::sexpr::parse("counter.sexpr", source, document, diagnostics),
          "nested S-expression parses");
    check(state, diagnostics.ok(), "valid S-expression has no errors");
    check(state, document.forms.size() == 1, "one top-level form parsed");
    for (const pokered::Diagnostic& diagnostic : diagnostics.entries)
        std::fprintf(stderr, "%s\n", pokered::format_diagnostic(diagnostic).c_str());
    if (document.forms.empty()) return;
    check(state, document.forms.front().children.size() == 2,
          "effect contains two top-level instructions");
    const std::string printed = pokered::sexpr::canonical(document);
    check(state, printed.find("(last_physical_damage target user)") != std::string::npos,
          "canonical output contains nested query");

    // Reject spelling and indentation that would produce ambiguous generated source.
    pokered::sexpr::Document invalid;
    pokered::Diagnostics invalid_diagnostics;
    check(
        state,
        !pokered::sexpr::parse("invalid.sexpr", "script bad-name\n", invalid, invalid_diagnostics),
        "kebab-case form argument is rejected");
    pokered::Diagnostics tab_diagnostics;
    check(state,
          !pokered::sexpr::parse("tabs.sexpr", "script test\n\tmove oak up\n", invalid,
                                 tab_diagnostics),
          "tab indentation is rejected");
    pokered::Diagnostics dedent_diagnostics;
    check(state,
          !pokered::sexpr::parse("dedent.sexpr", "root\n    child\n        deep\n      invalid\n",
                                 invalid, dedent_diagnostics),
          "unknown dedent level is rejected");
}

bool parse_package(std::string_view name, std::string_view text,
                   pokered::content::PackageSource& package) {
    pokered::Diagnostics diagnostics;
    pokered::sexpr::Document document;
    return pokered::sexpr::parse(name, text, document, diagnostics) &&
           pokered::content::read_package(document, package, diagnostics);
}

void test_overlays(TestState& state) {
    constexpr std::string_view canonical = "package canonical\n"
                                           "    define evolution kadabra_alakazam\n"
                                           "        trigger trade\n"
                                           "        condition\n"
                                           "            always\n";
    constexpr std::string_view enhancement = "package no_trade_evolutions\n"
                                             "    override evolution kadabra_alakazam\n"
                                             "        trigger level_up\n"
                                             "        condition\n"
                                             "            level_at_least 37\n";

    // Read two packages and resolve the enhancement over the canonical record.
    pokered::content::PackageSource base;
    pokered::content::PackageSource patch;
    check(state, parse_package("canonical.sexpr", canonical, base), "canonical package parses");
    check(state, parse_package("enhancement.sexpr", enhancement, patch),
          "enhancement package parses");
    const std::vector packages = {base, patch};
    pokered::Diagnostics diagnostics;
    pokered::content::ResolvedRecords records;
    check(state, pokered::content::resolve_packages(packages, records, diagnostics),
          "package overlay resolves");

    // Verify the winning field and retain both provenance contributions.
    const auto* evolution =
        pokered::content::find_record(records, symbol("evolution"), symbol("kadabra_alakazam"));
    check(state, evolution != nullptr, "resolved evolution exists");
    if (evolution == nullptr) return;
    const auto* trigger = pokered::content::find_field(*evolution, symbol("trigger"));
    check(state, trigger != nullptr, "resolved trigger exists");
    if (trigger == nullptr) return;
    check(state, trigger->forms.size() == 1, "trigger contains one form");
    check(state, trigger->forms.front().arguments.front().symbol.text == "level_up",
          "override supplies level_up");
    check(state, trigger->history.size() == 2, "field retains both contributions");
    check(state, trigger->history.back().package.text == "no_trade_evolutions",
          "winning contribution names enhancement");
}

void test_indexes_and_catalog(TestState& state) {
    // Assign stable symbols to dense typed IDs and recover both directions.
    pokered::content::Index<pokered::content::TypeId, pokered::content::TypeDef> types;
    pokered::Diagnostics diagnostics;
    pokered::content::TypeDef steel_type;
    steel_type.damage_class = pokered::content::DamageClass::physical;
    steel_type.color = {.red = 168, .green = 168, .blue = 192};
    const auto steel = pokered::content::add(types, symbol("steel"), std::move(steel_type),
                                             {"types.sexpr", 1, 1}, diagnostics);
    check(state, steel.has_value(), "dense type ID assigned");
    check(state, steel && steel->value == 0, "first dense ID is zero");
    check(state, pokered::content::find(types, symbol("steel")) == steel,
          "stable symbol resolves to dense ID");
    check(state, pokered::content::get(types, *steel) != nullptr, "dense ID resolves to record");

    // Catch malformed catalog tables before an effective pack is published.
    pokered::content::Catalog catalog;
    pokered::content::MapDef broken_map;
    broken_map.width = 2;
    broken_map.height = 2;
    broken_map.cells = {1, 2};
    catalog.maps.records.push_back(std::move(broken_map));
    catalog.maps.keys.push_back(symbol("broken_map"));
    catalog.maps.ids_by_key.emplace(symbol("broken_map"), 0);
    pokered::Diagnostics catalog_diagnostics;
    check(state, !pokered::content::validate_catalog(catalog, catalog_diagnostics),
          "invalid map cell count is rejected");
}

void test_predicates(TestState& state) {
    constexpr std::string_view source = "predicate can_receive_lapras\n"
                                        "    and\n"
                                        "        party_has_space\n"
                                        "        not\n"
                                        "            species_owned lapras\n";

    // Resolve a species reference while compiling the expression to typed instructions.
    pokered::content::Catalog catalog;
    pokered::Diagnostics catalog_diagnostics;
    const auto lapras =
        pokered::content::add(catalog.species, symbol("lapras"), pokered::content::SpeciesDef{},
                              {"species.sexpr", 1, 1}, catalog_diagnostics);
    pokered::sexpr::Document document;
    pokered::Diagnostics diagnostics;
    check(state, pokered::sexpr::parse("predicate.sexpr", source, document, diagnostics),
          "predicate source parses");
    pokered::content::PredicateProgram program;
    check(state, pokered::compile_predicate(document.forms.front(), catalog, program, diagnostics),
          "predicate compiles");
    check(state, program.instructions.size() == 4, "predicate emits four typed instructions");

    // Evaluate the same program against explicit state without catalog or string lookups.
    const std::array<std::uint8_t, 1> not_owned = {0};
    pokered::PredicateState predicate_state;
    predicate_state.species_owned = not_owned;
    predicate_state.party_capacity = 6;
    bool value = false;
    check(state, lapras.has_value(), "lapras receives a dense ID");
    check(state, pokered::evaluate_predicate(program, predicate_state, value, diagnostics),
          "predicate evaluates");
    check(state, value, "space and unowned species permits gift");

    const std::array<std::uint8_t, 1> owned = {1};
    predicate_state.species_owned = owned;
    check(state, pokered::evaluate_predicate(program, predicate_state, value, diagnostics),
          "predicate reevaluates");
    check(state, !value, "owned species rejects gift");
}

void test_animations(TestState& state) {
    constexpr std::string_view source =
        "animation original_title\n"
        "    set_offset logo 0 -36 native_canvas\n"
        "    set_palette logo darkened\n"
        "    show logo\n"
        "    parallel\n"
        "        tween_offset logo 0 0 4 ease_out native_canvas\n"
        "        sequence\n"
        "            wait 2\n"
        "            play_sound title_rise\n"
        "    wait 1\n"
        "    show version_label\n"
        "    signal title_ready\n";

    // Resolve the sound cue while the title view retains ownership of persistent targets.
    pokered::content::Catalog catalog;
    pokered::Diagnostics diagnostics;
    pokered::content::add(catalog.sounds, symbol("title_rise"), pokered::content::SoundProgram{},
                          {"sound.sexpr", 1, 1}, diagnostics);

    // Compile sequence and parallel forms into one stable event timeline.
    pokered::sexpr::Document document;
    check(state, pokered::sexpr::parse("animation.sexpr", source, document, diagnostics),
          "animation source parses");
    pokered::content::AnimationProgram program;
    check(state, pokered::compile_animation(document.forms.front(), catalog, program, diagnostics),
          "animation compiles");
    check(state, program.duration == 5, "parallel duration contributes to sequence");

    // Step the executor and observe movement, sound, visibility, and signal output.
    std::vector<pokered::AnimationTarget> targets(2);
    targets[0].name = symbol("logo");
    targets[0].x = 40.0F;
    targets[0].y = 16.0F;
    targets[1].name = symbol("version_label");
    pokered::AnimationState animation;
    check(state, pokered::start_animation(program, targets, animation, diagnostics),
          "animation starts");
    pokered::step_animation(animation);
    const pokered::AnimationTarget* target =
        pokered::find_animation_target(animation, symbol("logo"));
    check(state, target != nullptr && target->visible, "title logo becomes visible");
    check(state, target != nullptr && target->y == 16.0F && target->offset_y == -36.0F,
          "title logo starts above its renderer-owned anchor");
    check(state, target != nullptr &&
                     target->palette == pokered::content::AnimationPalette::darkened,
          "animation palette applies to a persistent target");
    pokered::step_animation(animation);
    pokered::step_animation(animation);
    check(state,
          animation.cues.size() == 1 &&
              animation.cues.front().kind == pokered::AnimationCueKind::sound,
          "parallel sound cue fires on schedule");
    while (!animation.finished)
        pokered::step_animation(animation);
    target = pokered::find_animation_target(animation, symbol("logo"));
    check(state, target != nullptr && target->y == 16.0F && target->offset_y == 0.0F,
          "title logo tween reaches its renderer-owned anchor");
    const pokered::AnimationTarget* version =
        pokered::find_animation_target(animation, symbol("version_label"));
    check(state, version != nullptr && version->visible, "version label appears after tween");
    check(state,
          animation.cues.size() == 1 &&
              animation.cues.front().kind == pokered::AnimationCueKind::signal &&
              animation.cues.front().signal.text == "title_ready",
          "title_ready signal ends timeline");
}

void test_battle_animation_lab(TestState& state) {
    // Load the same readable source tree used by the visual development view.
    const std::filesystem::path source_root =
        std::filesystem::path(POKERED_MODERN_SOURCE_DIR) / "data" / "dev" / "battle_animations";
    pokered::Diagnostics diagnostics;
    pokered::BattleAnimationLab lab;
    check(state, pokered::load_battle_animation_lab(source_root, lab, diagnostics),
          "battle animation lab source loads");
    check(state, lab.entries.size() == 6, "all original lab animations compile");

    // Exercise temporary-effect ownership independently of SDL rendering.
    lab.auto_advance = false;
    for (std::uint32_t tick = 0; tick < 40 && !lab.animation.finished; ++tick)
        pokered::step_battle_animation_lab(lab);
    check(state, lab.animation.finished, "first lab animation reaches completion");
    pokered::next_battle_animation_lab(lab);
    pokered::step_battle_animation_lab(lab);
    check(state, !lab.animation.effects.empty(), "scratch spawns temporary visual effects");
}

} // namespace

int main() {
    TestState state;
    test_symbols(state);
    test_sexpr(state);
    test_overlays(state);
    test_indexes_and_catalog(state);
    test_predicates(state);
    test_animations(state);
    test_battle_animation_lab(state);
    if (state.failures == 0) std::puts("foundation tests passed");
    return state.failures == 0 ? 0 : 1;
}
