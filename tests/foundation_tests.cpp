#include "animations.hpp"
#include "battle.hpp"
#include "battle_animation_lab.hpp"
#include "battle_controller.hpp"
#include "battle_rules.hpp"
#include "battle_view.hpp"
#include "boot.hpp"
#include "catalog.hpp"
#include "clocks.hpp"
#include "content_index.hpp"
#include "encounters.hpp"
#include "interactions.hpp"
#include "maps.hpp"
#include "overlays.hpp"
#include "predicates.hpp"
#include "pokemon.hpp"
#include "rules.hpp"
#include "settings.hpp"
#include "sexpr.hpp"
#include "state.hpp"
#include "symbols.hpp"
#include "trainers.hpp"

#include <algorithm>
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
    constexpr std::string_view source = "animation original_title\n"
                                        "    set_offset logo 0 -36 native_canvas\n"
                                        "    set_palette logo darkened\n"
                                        "    show logo\n"
                                        "    set_form logo minimized\n"
                                        "    set_squish logo 3\n"
                                        "    set_wave_phase logo 7\n"
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
    check(state,
          target != nullptr && target->palette == pokered::content::AnimationPalette::darkened,
          "animation palette applies to a persistent target");
    check(state,
          target != nullptr && target->form == pokered::content::AnimationForm::minimized &&
              target->squish_half_steps == 3 && target->wave_phase == 7,
          "animation picture and wave transforms apply to persistent targets");
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
    const std::filesystem::path source_root = std::filesystem::path(POKERED_MODERN_SOURCE_DIR) /
                                              "tests" / "fixtures" / "battle_animations";
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

void test_battle_ui(TestState& state) {
    // Load a synthetic readable fixture, then compose semantic slots from parsed data.
    const std::filesystem::path source_root =
        std::filesystem::path(POKERED_MODERN_SOURCE_DIR) / "tests" / "fixtures" / "battle_ui";
    pokered::Diagnostics diagnostics;
    pokered::BattleUiState ui;
    check(state, pokered::load_battle_ui_source(source_root, ui, diagnostics),
          "battle UI source loads");
    pokered::BattleTileMap tiles;
    std::string error;
    check(state, pokered::compose_battle_ui(ui, tiles, error), "standard battle UI composes");
    check(state, tiles[12U * 20U + 8U] == 0x79, "standard command box uses right-side geometry");
    check(state, tiles[14U * 20U + 9U] == 0xED, "standard command selection draws cursor");
    check(state, tiles[9U * 20U + 10U] == 0x71, "player HP label uses battle HUD tile");

    // Move selection owns a separate joined type/PP and move-list layout.
    pokered::next_battle_ui_mode(ui);
    check(state, pokered::compose_battle_ui(ui, tiles, error), "move battle UI composes");
    check(state, tiles[8U * 20U] == 0x79 && tiles[12U * 20U + 10U] == 0x7E,
          "move type box joins the move list at the canonical border");
    check(state, tiles[13U * 20U + 5U] == 0xED, "move selection draws its own cursor");

    // Safari supplies another command descriptor and deliberately hides player HUD.
    pokered::next_battle_ui_mode(ui);
    check(state, pokered::compose_battle_ui(ui, tiles, error), "Safari battle UI composes");
    check(state, tiles[12U * 20U] == 0x79, "Safari command box spans the full width");
    check(state, tiles[9U * 20U + 18U] == 0, "Safari layout omits the player HUD");
}

void test_host_settings_and_clocks(TestState& state) {
    // Host settings round-trip without involving SDL or campaign state.
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "pokered_modern_settings_test.cfg";
    std::error_code filesystem_error;
    std::filesystem::remove(path, filesystem_error);
    pokered::PresentationSettings expected;
    expected.vsync = false;
    expected.motion_interpolation = false;
    expected.show_fps = false;
    expected.render_rate_limit = 240;
    expected.control_profile = 1;
    expected.fast_forward_enabled = true;
    expected.fast_forward_toggle = true;
    expected.fast_forward_multiplier = 8;
    std::string error;
    check(state, pokered::save_settings(path, expected, error), "host settings save");
    pokered::PresentationSettings loaded;
    check(state, pokered::load_settings(path, loaded, error), "host settings load");
    check(state, loaded == expected, "host settings round-trip");
    std::filesystem::remove(path, filesystem_error);

    // Fast-forward affects how often game steps are requested by the host; it
    // never scales the independent wall-clock domains.
    pokered::GameClocks clocks;
    pokered::advance_unscaled_clocks(clocks, 0.5);
    pokered::advance_game_clock(clocks, 1.0 / 60.0);
    check(state,
          clocks.real_time == 0.25 && clocks.presentation_time == 0.25 &&
              clocks.audio_time == 0.25 && clocks.music_time == 0.25,
          "host clocks advance together with bounded unscaled time");
    check(state, clocks.game_steps == 1 && clocks.game_time == 1.0 / 60.0,
          "game clock advances only on deterministic steps");
}

void test_world_spaces_and_warps(TestState& state) {
    // Two original fixture maps exercise direct entry and LAST_MAP return
    // without depending on cartridge content.
    pokered::WorldState world;
    world.loaded = true;
    world.tilesets.push_back({
        .id = 0,
        .tile_count = 1,
        .animation_mode = 0,
        .passable_tiles = {0},
        .pixels = std::vector<std::uint8_t>(64U, 0),
        .animation_pixels = {},
    });
    world.spaces = {
        {.id = 0, .key = "surface", .outdoor = true},
        {.id = 1, .key = "house", .outdoor = false},
    };
    pokered::WorldMap surface{
        .id = 0,
        .tileset_id = 0,
        .width_blocks = 1,
        .height_blocks = 1,
        .width_tiles = 4,
        .height_tiles = 4,
        .key = "surface_map",
        .display_name = "Surface",
        .global_x_tiles = 0,
        .global_y_tiles = 0,
        .world_space = 0,
        .tiles = std::vector<std::uint8_t>(16U, 0),
        .warps =
            {{.index = 1, .x = 1, .y = 0, .destination_map_id = 37, .destination_warp_index = 0}},
        .actors = {},
    };
    pokered::WorldMap house{
        .id = 37,
        .tileset_id = 0,
        .width_blocks = 1,
        .height_blocks = 1,
        .width_tiles = 4,
        .height_tiles = 4,
        .key = "house_map",
        .display_name = "House",
        .global_x_tiles = 0,
        .global_y_tiles = 0,
        .world_space = 1,
        .tiles = std::vector<std::uint8_t>(16U, 0),
        .warps =
            {{.index = 1, .x = 0, .y = 1, .destination_map_id = 0xFF, .destination_warp_index = 0}},
        .actors = {},
    };
    world.maps = {std::move(surface), std::move(house)};
    pokered::InteractionCatalog interactions;
    interactions.loaded = true;
    interactions.maps.push_back({
        .map_id = 0,
        .decoded = true,
        .backgrounds =
            {{.index = 1, .x = 0, .y = 1, .program_id = 1}},
        .actors = {},
        .programs =
            {{
                .status = pokered::InteractionProgramStatus::dialogue,
                .pages = {"Hello {player_name}. {rival_name} is waiting."},
            }},
    });
    pokered::CampaignState campaign;
    std::string error;
    check(state,
          pokered::begin_new_campaign(campaign, "PLAYER", "RIVAL", {}, error),
          "campaign fixture initializes");
    check(state, pokered::initialize_world_runtime(world, interactions, error),
          "world-space fixture initializes");

    world.player.x = 0;
    world.player.y = 0;
    world.player.move_cooldown = 0;
    pokered::step_world(world, interactions, campaign, {.right = true});
    check(state,
          world.player.map_index == 1 && world.current_space == 1 && world.player.x == 0 &&
              world.player.y == 1,
          "direct warp enters its destination world space");
    check(state,
          world.last_warp.occurred && world.last_warp.source_map_id == 0 &&
              world.last_warp.destination_map_id == 37,
          "direct warp records diagnostics");

    world.player.x = 0;
    world.player.y = 0;
    world.player.move_cooldown = 0;
    pokered::step_world(world, interactions, campaign, {.down = true});
    check(state,
          world.player.map_index == 0 && world.current_space == 0 && world.player.x == 1 &&
              world.player.y == 0,
          "LAST_MAP warp returns through the remembered outdoor endpoint");

    world.player.x = 0;
    world.player.y = 0;
    world.player.facing = pokered::WorldDirection::down;
    world.player.move_cooldown = 0;
    pokered::step_world(world, interactions, campaign, {.activate = true});
    check(state,
          world.dialogue.open && world.dialogue.pages ==
                                     std::vector<std::string>{
                                         "Hello PLAYER. RIVAL is waiting."},
          "world dialogue consumes campaign-owned naming results");
}

void test_local_encounter_cache(TestState& state) {
    // When a local cartridge import is available, exercise the complete join
    // from a rendered world cell through its imported encounter table.
    const std::filesystem::path root =
        std::filesystem::path(POKERED_MODERN_SOURCE_DIR) / "data" /
        "runtime" / "imports" / "pokemon_red_us_rev_0" / "compiled";
    const std::filesystem::path encounter_path = root / "encounters.bin";
    const std::filesystem::path world_path = root / "world_maps.bin";
    if (!std::filesystem::exists(encounter_path) ||
        !std::filesystem::exists(world_path)) {
        std::puts("encounter cache test skipped: no local imported campaign");
        return;
    }

    pokered::EncounterCatalog encounters;
    pokered::WorldState world;
    std::string error;
    check(state, pokered::load_encounters(encounter_path, encounters, error),
          "local encounter cache loads");
    check(state, pokered::load_world(world_path, world, error),
          "local world cache loads for encounters");
    if (!encounters.loaded || !world.loaded) return;
    check(state,
          encounters.maps.size() == 248U &&
              encounters.probabilities.size() == 10U &&
              encounters.probabilities.front().inclusive_threshold == 50U &&
              encounters.probabilities.back().inclusive_threshold == 255U,
          "encounter cache covers every map and probability roll");

    const auto map_it = std::find_if(
        world.maps.begin(), world.maps.end(),
        [](const pokered::WorldMap& map) { return map.id == 12U; });
    check(state, map_it != world.maps.end(), "Route 1 map is imported");
    if (map_it == world.maps.end()) return;
    const pokered::MapTileset* tileset =
        pokered::find_tileset(world, map_it->tileset_id);
    check(state, tileset != nullptr && tileset->grass_tile != 0xFFU,
          "Route 1 tileset supplies its imported grass tile");
    if (tileset == nullptr) return;

    bool found_grass = false;
    std::int32_t grass_x = 0;
    std::int32_t grass_y = 0;
    for (std::int32_t y = 0; y < map_it->height_blocks * 2 && !found_grass; ++y) {
        for (std::int32_t x = 0; x < map_it->width_blocks * 2; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * 2U + 1U) *
                    map_it->width_tiles +
                static_cast<std::size_t>(x) * 2U + 1U;
            if (offset < map_it->tiles.size() &&
                map_it->tiles[offset] == tileset->grass_tile) {
                grass_x = x;
                grass_y = y;
                found_grass = true;
                break;
            }
        }
    }
    check(state, found_grass, "Route 1 contains an imported grass cell");
    if (!found_grass) return;

    world.player.map_index =
        static_cast<std::size_t>(map_it - world.maps.begin());
    world.player.x = grass_x;
    world.player.y = grass_y;
    world.player_completed_step = true;
    pokered::WildEncounterResult result;
    check(state,
          pokered::resolve_world_wild_encounter(
              encounters, world, 0U, 0U, false, 0U, result, error),
          "Route 1 encounter resolves");
    check(state,
          result.occurred && result.environment ==
                                 pokered::EncounterEnvironment::land &&
              result.encounter_rate == 25U && result.slot_index == 0U &&
              result.species_dex == 16U && result.level == 3U,
          "Route 1 first slot joins terrain, rate, species, and level");
    const pokered::WildEncounterResult route_one_encounter = result;

    check(state,
          pokered::resolve_world_wild_encounter(
              encounters, world, 25U, 0U, false, 0U, result, error) &&
              !result.occurred &&
              result.suppression ==
                  pokered::WildEncounterSuppression::rate_roll,
          "encounter rate uses the imported strict threshold");
    check(state,
          pokered::resolve_world_wild_encounter(
              encounters, world, 0U, 0U, true, 4U, result, error) &&
              !result.occurred &&
              result.suppression ==
                  pokered::WildEncounterSuppression::repel_level,
          "repel suppresses an encounter below the leading level");

    pokered::RuleCatalog rules;
    pokered::BattleRuleCatalog battle_rules;
    check(state, pokered::load_rules(root / "pokemon_rules.bin", rules, error),
          "Pokemon rules load for wild battle");
    check(state,
          pokered::load_battle_rules(root / "battle_rules.bin",
                                     battle_rules, error),
          "battle rules load for wild battle");
    const pokered::StatFormulaProgram* stats = pokered::find_stat_formula(
        battle_rules, battle_rules.original_stat_formula);
    if (!rules.loaded || !battle_rules.loaded || stats == nullptr) return;
    pokered::PokemonState starter;
    check(state,
          pokered::build_pokemon(
              rules, *stats, 1U, 5U, {15U, 14U, 13U, 12U}, 7U,
              "PLAYER", starter, error),
          "starter fixture builds from imported rules");
    pokered::PartyState party;
    party.members.push_back(std::move(starter));
    pokered::BattleState battle;
    check(state,
          pokered::begin_wild_battle(
              rules, battle_rules, party, route_one_encounter,
              0x12345678U, battle, error),
          "resolved world encounter begins an owned wild battle");
    check(state,
          battle.active && battle.kind == pokered::BattleKind::wild &&
              battle.enemy_party.members.size() == 1U &&
              battle.enemy_party.members.front().species_dex == 16U &&
              battle.enemy_party.members.front().level == 3U &&
              battle.enemy_party.members.front().current_hp ==
                  battle.enemy_party.members.front().stats.hp,
          "wild battle materializes imported species, level, moves, and stats");

    pokered::BattleAnimationLab battle_view;
    pokered::Diagnostics diagnostics;
    const std::filesystem::path animation_source =
        root.parent_path() / "source" / "animations" / "battle_moves";
    check(state,
          pokered::load_battle_animation_lab(
              animation_source, battle_view, diagnostics),
          "imported battle presentation loads for owned battle");
    if (!battle_view.loaded || !battle.active) return;
    pokered::CampaignState campaign;
    campaign.initialized = true;
    campaign.trainer_id = 7U;
    campaign.party = std::move(party);
    campaign.battle = std::move(battle);
    battle_view.ui.mode = pokered::BattleUiMode::command;
    pokered::prepare_battle_view(battle_view);
    check(state,
          pokered::sync_battle_view(
              rules, battle_rules, campaign.party, campaign.battle,
              battle_view, error),
          "owned battle binds imported pictures and UI");
    check(state,
          battle_view.distinct_battlers &&
              battle_view.player_species == 0U &&
              battle_view.enemy_species == 15U &&
              battle_view.ui.player.name == "BULBASAUR" &&
              battle_view.ui.enemy.name == "PIDGEY" &&
              battle_view.ui.enemy.level == 3U,
          "battle view reflects campaign-owned battlers");

    const auto fight = std::find_if(
        battle_view.ui.definition.standard_commands.slots.begin(),
        battle_view.ui.definition.standard_commands.slots.end(),
        [](const pokered::BattleCommandSlot& command) {
            return command.on_select.text == "battle_choose_move";
        });
    check(state,
          fight != battle_view.ui.definition.standard_commands.slots.end(),
          "imported command menu exposes move selection");
    if (fight ==
        battle_view.ui.definition.standard_commands.slots.end())
        return;
    battle_view.ui.definition.standard_commands.selected =
        static_cast<std::size_t>(fight -
                                 battle_view.ui.definition.standard_commands
                                     .slots.begin());
    pokered::BattleControlResult control_result;
    check(state,
          pokered::control_battle(
              rules, battle_rules, campaign, battle_view,
              {.confirm = true}, control_result, error) &&
              battle_view.ui.mode == pokered::BattleUiMode::moves,
          "battle command enters the imported move menu");
    check(state,
          pokered::control_battle(
              rules, battle_rules, campaign, battle_view,
              {.confirm = true}, control_result, error) &&
              campaign.battle.turn == 1U,
          "battle control resolves an ordinary owned turn");
}

void test_local_rule_cache(TestState& state) {
    // A locally imported cartridge cache is optional in clean source builds.
    // When present, pin the exhaustive immutable-domain joins used by gameplay.
    const std::filesystem::path path = std::filesystem::path(POKERED_MODERN_SOURCE_DIR) / "data" /
                                       "runtime" / "imports" / "pokemon_red_us_rev_0" / "compiled" /
                                       "pokemon_rules.bin";
    if (!std::filesystem::exists(path)) {
        std::puts("rule cache test skipped: no local imported campaign");
        return;
    }
    pokered::RuleCatalog rules;
    std::string error;
    check(state, pokered::load_rules(path, rules, error), "local Pokemon rule cache loads");
    if (!rules.loaded) return;
    check(state, rules.types.size() == 27 && rules.type_interactions.size() == 82,
          "complete type slots and interaction chart load");
    check(state, rules.species.size() == 151 && rules.moves.size() == 165,
          "complete species and move tables load");
    check(state,
          rules.learnsets.size() == 728 && rules.evolutions.size() == 72 &&
              rules.growth_curves.size() == 6 && rules.machines.size() == 55,
          "complete progression and machine tables load");

    pokered::TrainerCatalog trainers;
    check(state,
          pokered::load_trainers(path.parent_path() / "trainers.bin",
                                 trainers, error),
          "complete trainer cache loads");
    const pokered::TrainerClassRule* youngster =
        pokered::find_trainer_class(trainers, 1U);
    const pokered::TrainerPartyRule* youngster_party =
        pokered::find_trainer_party(trainers, 1U, 0U);
    std::size_t trainer_party_count = 0U;
    std::size_t trainer_member_count = 0U;
    for (const pokered::TrainerClassRule& trainer : trainers.classes) {
        trainer_party_count += trainer.parties.size();
        for (const pokered::TrainerPartyRule& party : trainer.parties)
            trainer_member_count += party.members.size();
    }
    check(state,
          trainers.classes.size() == 47U &&
              trainer_party_count == 391U &&
              trainer_member_count == 994U && youngster != nullptr &&
              youngster->name == "YOUNGSTER" &&
              youngster_party != nullptr &&
              youngster_party->members ==
                  std::vector<pokered::TrainerPartyMember>{
                      {11U, 19U}, {11U, 23U}},
          "trainer classes join rewards, AI profiles, and indexed parties");

    const pokered::SpeciesRule* bulbasaur = pokered::find_species(rules, 1);
    check(state,
          bulbasaur != nullptr && bulbasaur->key == "bulbasaur" && bulbasaur->internal_id == 153 &&
              bulbasaur->base_hp == 45 && bulbasaur->type_ids == std::array<std::uint8_t, 2>{22, 3},
          "Bulbasaur joins internal identity, stats, and types");
    check(state,
          bulbasaur != nullptr &&
              pokered::experience_for_level(rules, bulbasaur->growth_curve_id, 5) == 135,
          "materialized growth curve resolves level-five experience");
    check(state,
          bulbasaur != nullptr &&
              pokered::level_for_experience(
                  rules, bulbasaur->growth_curve_id, 135) == 5,
          "materialized growth curve resolves experience back to level");
    check(state,
          bulbasaur != nullptr &&
              pokered::type_multiplier_tenths(
                  rules, 20U, bulbasaur->type_ids) == 20U,
          "type executor combines Fire versus Grass and Poison");

    const std::vector<std::uint8_t> level_seven =
        pokered::moves_learned_at_level(rules, 1U, 7U);
    check(state, level_seven == std::vector<std::uint8_t>{73U},
          "learnset executor resolves Bulbasaur's level-seven move");
    check(state,
          pokered::species_can_learn_machine(rules, 1U, 2U) &&
              !pokered::species_can_learn_machine(rules, 1U, 23U),
          "machine executor reads species compatibility bits");

    const pokered::EvolutionRule* level_evolution =
        pokered::eligible_evolution(rules, 1U, 16U, std::nullopt, false);
    const pokered::EvolutionRule* item_evolution =
        pokered::eligible_evolution(rules, 25U, 1U, 33U, false);
    const pokered::EvolutionRule* trade_evolution =
        pokered::eligible_evolution(rules, 64U, 1U, std::nullopt, true);
    check(state,
          level_evolution != nullptr &&
              level_evolution->target_species_dex == 2U &&
              item_evolution != nullptr &&
              item_evolution->target_species_dex == 26U &&
              trade_evolution != nullptr &&
              trade_evolution->target_species_dex == 65U,
          "evolution executor resolves level, item, and trade methods");

    const std::filesystem::path battle_path =
        path.parent_path() / "battle_rules.bin";
    pokered::BattleRuleCatalog battle_rules;
    check(state,
          pokered::load_battle_rules(battle_path, battle_rules, error),
          "semantic battle rule cache loads");
    const pokered::DamageFormulaProgram* damage =
        pokered::find_damage_formula(
            battle_rules, battle_rules.original_damage_formula);
    check(state,
          damage != nullptr && damage->key == "gen_1_original_damage" &&
              damage->instructions.size() == 8U,
          "original damage formula resolves by imported ruleset binding");
    if (damage != nullptr) {
        constexpr std::array<std::uint8_t, 2> random{0x00U, 0xFFU};
        pokered::DamageFormulaResult damage_result;
        check(state,
              pokered::execute_damage_formula(
                  rules, *damage,
                  {
                      .level = 5U,
                      .power = 40U,
                      .move_type = 20U,
                      .move_effect = 0U,
                      .attack = 50U,
                      .defense = 50U,
                      .attacker_types = {20U, 20U},
                      .defender_types = {22U, 3U},
                      .critical = false,
                  },
                  random, damage_result, error) &&
                  damage_result.damage == 14U &&
                  damage_result.random_bytes_consumed == 2U &&
                  !damage_result.immune,
              "imported ordinary damage program executes STAB, types, and "
              "rejection-sampled variance");
    }

    const pokered::CriticalHitProgram* critical =
        pokered::find_critical_hit_program(
            battle_rules, battle_rules.original_critical_hit_program);
    check(state,
          critical != nullptr &&
              critical->key == "gen_1_original_critical_hits" &&
              critical->high_critical_moves ==
                  std::vector<std::uint8_t>{2U, 75U, 152U, 163U} &&
              critical->instructions.size() == 5U,
          "original critical-hit program and move table resolve by imported "
          "ruleset binding");
    if (critical != nullptr) {
        constexpr std::array<std::uint8_t, 1> low_roll{0x00U};
        pokered::CriticalHitResult ordinary;
        check(state,
              pokered::execute_critical_hit_program(
                  *critical,
                  {
                      .base_speed = 100U,
                      .move_id = 33U,
                      .move_power = 35U,
                      .focused = false,
                  },
                  low_roll, ordinary, error) &&
                  ordinary.threshold == 50U &&
                  ordinary.random_bytes_consumed == 1U &&
                  ordinary.critical,
              "imported critical-hit program executes the ordinary rate");

        pokered::CriticalHitResult focused;
        check(state,
              pokered::execute_critical_hit_program(
                  *critical,
                  {
                      .base_speed = 100U,
                      .move_id = 33U,
                      .move_power = 35U,
                      .focused = true,
                  },
                  low_roll, focused, error) &&
                  focused.threshold == 12U && focused.critical,
              "original compatibility profile preserves the Focus Energy "
              "critical-rate defect");

        constexpr std::array<std::uint8_t, 1> high_roll{0xFFU};
        pokered::CriticalHitResult high_rate;
        check(state,
              pokered::execute_critical_hit_program(
                  *critical,
                  {
                      .base_speed = 100U,
                      .move_id = 2U,
                      .move_power = 50U,
                      .focused = false,
                  },
                  high_roll, high_rate, error) &&
                  high_rate.threshold == 255U && !high_rate.critical,
              "high-critical move rate saturates at the cartridge threshold "
              "and keeps strict comparison");
    }

    const pokered::CaptureFormulaProgram* capture =
        pokered::find_capture_formula(
            battle_rules, battle_rules.original_capture_formula);
    check(state,
          capture != nullptr &&
              capture->key == "gen_1_original_capture" &&
              capture->ball_profiles.size() == 5U &&
              capture->status_profiles.size() == 3U &&
              capture->instructions.size() == 8U,
          "original capture formula and modifier profiles resolve by "
          "imported ruleset binding");
    if (capture != nullptr) {
        constexpr std::array<std::uint8_t, 1> master_roll{0xFFU};
        pokered::CaptureFormulaResult master;
        check(state,
              pokered::execute_capture_formula(
                  *capture,
                  {
                      .ball_profile = 0U,
                      .status_profile = 0U,
                      .catch_rate = 3U,
                      .current_hp = 45U,
                      .maximum_hp = 45U,
                  },
                  master_roll, master, error) &&
                  master.caught && master.random_bytes_consumed == 1U,
              "Master Ball profile preserves the cartridge's initial random "
              "call before guaranteed capture");

        constexpr std::array<std::uint8_t, 2> ordinary_rolls{40U, 100U};
        pokered::CaptureFormulaResult ordinary_capture;
        check(state,
              pokered::execute_capture_formula(
                  *capture,
                  {
                      .ball_profile = 3U,
                      .status_profile = 0U,
                      .catch_rate = 45U,
                      .current_hp = 39U,
                      .maximum_hp = 45U,
                  },
                  ordinary_rolls, ordinary_capture, error) &&
                  ordinary_capture.caught &&
                  ordinary_capture.capture_value == 106U &&
                  ordinary_capture.random_bytes_consumed == 2U,
              "ordinary capture executes HP arithmetic and the inclusive "
              "second-roll comparison");

        constexpr std::array<std::uint8_t, 2> rejected_rolls{200U, 150U};
        pokered::CaptureFormulaResult rejected_capture;
        check(state,
              pokered::execute_capture_formula(
                  *capture,
                  {
                      .ball_profile = 1U,
                      .status_profile = 0U,
                      .catch_rate = 45U,
                      .current_hp = 45U,
                      .maximum_hp = 45U,
                  },
                  rejected_rolls, rejected_capture, error) &&
                  !rejected_capture.caught &&
                  rejected_capture.capture_value == 86U &&
                  rejected_capture.shake_value == 10U &&
                  rejected_capture.shakes == 1U &&
                  rejected_capture.random_bytes_consumed == 2U,
              "Ultra Ball profile rejection-samples and calculates the exact "
              "failure shake tier");

        constexpr std::array<std::uint8_t, 1> status_roll{24U};
        pokered::CaptureFormulaResult status_capture;
        check(state,
              pokered::execute_capture_formula(
                  *capture,
                  {
                      .ball_profile = 3U,
                      .status_profile = 2U,
                      .catch_rate = 1U,
                      .current_hp = 45U,
                      .maximum_hp = 45U,
                  },
                  status_roll, status_capture, error) &&
                  status_capture.caught &&
                  status_capture.random_bytes_consumed == 1U,
              "freeze and sleep profile captures immediately when status "
              "subtraction underflows");
    }

    const pokered::ExperienceFormulaProgram* experience =
        pokered::find_experience_formula(
            battle_rules, battle_rules.original_experience_formula);
    check(state,
          experience != nullptr &&
              experience->key == "gen_1_original_experience" &&
              experience->instructions.size() == 5U,
          "original experience formula resolves by imported ruleset binding");
    if (experience != nullptr) {
        const pokered::SpeciesRule* defeated_species =
            pokered::find_species(rules, 7U);
        pokered::ExperienceFormulaResult boosted;
        check(state,
              defeated_species != nullptr &&
                  pokered::execute_experience_formula(
                      *experience,
                      {
                          .base_experience =
                              defeated_species->experience_yield,
                          .base_stats = {
                              defeated_species->base_hp,
                              defeated_species->base_attack,
                              defeated_species->base_defense,
                              defeated_species->base_speed,
                              defeated_species->base_special,
                          },
                          .defeated_level = 5U,
                          .base_value_divisor = 1U,
                          .participant_divisor = 1U,
                          .traded = true,
                          .trainer_battle = true,
                      },
                      boosted, error) &&
                  boosted.experience == 105U &&
                  boosted.stat_experience ==
                      std::array<std::uint16_t, 5>{
                          44U, 48U, 65U, 43U, 50U},
              "experience executor awards stat experience and applies "
              "traded then trainer boosts");

        pokered::ExperienceFormulaResult divided;
        check(state,
              defeated_species != nullptr &&
                  pokered::execute_experience_formula(
                      *experience,
                      {
                          .base_experience =
                              defeated_species->experience_yield,
                          .base_stats = {
                              defeated_species->base_hp,
                              defeated_species->base_attack,
                              defeated_species->base_defense,
                              defeated_species->base_speed,
                              defeated_species->base_special,
                          },
                          .defeated_level = 5U,
                          .base_value_divisor = 2U,
                          .participant_divisor = 2U,
                          .traded = false,
                          .trainer_battle = false,
                      },
                      divided, error) &&
                  divided.experience == 11U &&
                  divided.stat_experience ==
                      std::array<std::uint16_t, 5>{
                          11U, 12U, 16U, 10U, 12U},
              "experience executor preserves sequential Exp. All and "
              "participant division floors");
    }

    const pokered::StatFormulaProgram* stats =
        pokered::find_stat_formula(
            battle_rules, battle_rules.original_stat_formula);
    check(state,
          stats != nullptr && stats->key == "gen_1_original_stats" &&
              stats->instructions.size() == 7U,
          "original owned-Pokemon stat formula resolves by imported binding");
    if (stats != nullptr && youngster_party != nullptr) {
        pokered::PokemonState player_fixture;
        pokered::PartyState trainer_player;
        pokered::BattleState trainer_battle;
        check(state,
              pokered::build_pokemon(
                  rules, *stats, 1U, 12U,
                  {15U, 15U, 15U, 15U}, 7U, "PLAYER",
                  player_fixture, error),
              "trainer battle player fixture builds");
        trainer_player.members.push_back(std::move(player_fixture));
        check(state,
              pokered::begin_trainer_battle(
                  rules, battle_rules, trainer_player, *youngster_party,
                  0x2468ACE1U, trainer_battle, error) &&
                  trainer_battle.active &&
                  trainer_battle.kind == pokered::BattleKind::trainer &&
                  trainer_battle.enemy_party.members.size() == 2U &&
                  trainer_battle.enemy_party.members[0].species_dex == 19U &&
                  trainer_battle.enemy_party.members[0].level == 11U &&
                  trainer_battle.enemy_party.members[1].species_dex == 23U,
              "indexed trainer party materializes an owned trainer battle");
    }
    if (stats != nullptr) {
        const pokered::SpeciesRule* squirtle_species =
            pokered::find_species(rules, 7U);
        pokered::StatFormulaResult level_five;
        check(state,
              squirtle_species != nullptr &&
                  pokered::execute_stat_formula(
                      *stats,
                      {
                          .base_stats = {
                              squirtle_species->base_hp,
                              squirtle_species->base_attack,
                              squirtle_species->base_defense,
                              squirtle_species->base_speed,
                              squirtle_species->base_special,
                          },
                          .dvs = {15U, 15U, 15U, 15U},
                          .stat_experience = {},
                          .level = 5U,
                      },
                      level_five, error) &&
                  level_five.hp_dv == 15U &&
                  level_five.stats ==
                      std::array<std::uint16_t, 5>{
                          20U, 11U, 13U, 10U, 11U},
              "stat executor derives HP DV and all five level-five stats");

        pokered::StatFormulaResult effort_rounding;
        check(state,
              squirtle_species != nullptr &&
                  pokered::execute_stat_formula(
                      *stats,
                      {
                          .base_stats = {
                              squirtle_species->base_hp,
                              squirtle_species->base_attack,
                              squirtle_species->base_defense,
                              squirtle_species->base_speed,
                              squirtle_species->base_special,
                          },
                          .dvs = {},
                          .stat_experience = {
                              10U, 10U, 10U, 10U, 10U},
                          .level = 100U,
                      },
                      effort_rounding, error) &&
                  effort_rounding.stats ==
                      std::array<std::uint16_t, 5>{
                          199U, 102U, 136U, 92U, 106U},
              "stat executor preserves the cartridge's ceiling-square-root "
              "effort rounding");

        pokered::PokemonState owned_squirtle;
        check(state,
              pokered::build_pokemon(
                  rules, *stats, 7U, 5U,
                  {15U, 15U, 15U, 15U}, 0x1234U, "RED",
                  owned_squirtle, error) &&
                  owned_squirtle.nickname == "SQUIRTLE" &&
                  owned_squirtle.current_hp == 20U &&
                  owned_squirtle.moves[0].move_id == 33U &&
                  owned_squirtle.moves[1].move_id == 39U &&
                  owned_squirtle.moves[2].move_id == 0U,
              "owned Pokemon construction consumes imported species, moves, "
              "growth, and stat rules");

        pokered::PokemonState recipient;
        pokered::ExperienceAwardResult progress;
        check(state,
              experience != nullptr &&
                  pokered::build_pokemon(
                      rules, *stats, 1U, 6U,
                      {15U, 15U, 15U, 15U}, 0x1234U, "RED",
                      recipient, error) &&
                  pokered::award_pokemon_experience(
                      rules, *experience, *stats, owned_squirtle, true,
                      0x1234U, 1U, recipient, progress, error) &&
                  progress.experience_gained == 70U &&
                  progress.old_level == 6U && progress.new_level == 7U &&
                  progress.learned_moves ==
                      std::vector<std::uint8_t>{73U} &&
                  progress.pending_moves.empty() &&
                  recipient.level == 7U &&
                  recipient.stat_experience ==
                      std::array<std::uint16_t, 5>{
                          44U, 48U, 65U, 43U, 50U},
              "owned Pokemon progression applies imported experience, "
              "learnset, and stat programs end to end");
    }

    const pokered::AccuracyFormulaProgram* accuracy =
        pokered::find_accuracy_formula(
            battle_rules, battle_rules.original_accuracy_formula);
    check(state,
          accuracy != nullptr &&
              accuracy->key == "gen_1_original_accuracy" &&
              accuracy->neutral_stage == 7U &&
              accuracy->stage_ratios.size() == 13U &&
              accuracy->instructions.size() == 7U,
          "original accuracy formula and stage ratios resolve by imported "
          "ruleset binding");
    if (accuracy != nullptr) {
        constexpr std::array<std::uint8_t, 1> last_hit_roll{254U};
        pokered::AccuracyFormulaResult last_hit;
        check(state,
              pokered::execute_accuracy_formula(
                  *accuracy,
                  {
                      .raw_accuracy = 255U,
                      .accuracy_stage = 7U,
                      .target_evasion_stage = 7U,
                      .bypassed = false,
                  },
                  last_hit_roll, last_hit, error) &&
                  last_hit.chance == 255U && last_hit.hit &&
                  last_hit.random_bytes_consumed == 1U,
              "neutral maximum accuracy hits through random value 254");

        constexpr std::array<std::uint8_t, 1> miss_roll{255U};
        pokered::AccuracyFormulaResult one_in_256_miss;
        check(state,
              pokered::execute_accuracy_formula(
                  *accuracy,
                  {
                      .raw_accuracy = 255U,
                      .accuracy_stage = 7U,
                      .target_evasion_stage = 7U,
                      .bypassed = false,
                  },
                  miss_roll, one_in_256_miss, error) &&
                  one_in_256_miss.chance == 255U &&
                  !one_in_256_miss.hit,
              "strict accuracy comparison preserves the cartridge's "
              "one-in-256 maximum-accuracy miss");

        constexpr std::array<std::uint8_t, 1> scaled_roll{11U};
        pokered::AccuracyFormulaResult scaled;
        check(state,
              pokered::execute_accuracy_formula(
                  *accuracy,
                  {
                      .raw_accuracy = 200U,
                      .accuracy_stage = 1U,
                      .target_evasion_stage = 13U,
                      .bypassed = false,
                  },
                  scaled_roll, scaled, error) &&
                  scaled.chance == 12U && scaled.hit,
              "accuracy executor applies the two cartridge ratios "
              "sequentially with integer floors");

        pokered::AccuracyFormulaResult bypassed;
        check(state,
              pokered::execute_accuracy_formula(
                  *accuracy,
                  {
                      .raw_accuracy = 1U,
                      .accuracy_stage = 1U,
                      .target_evasion_stage = 13U,
                      .bypassed = true,
                  },
                  {}, bypassed, error) &&
                  bypassed.chance == 255U && bypassed.hit &&
                  bypassed.random_bytes_consumed == 0U,
              "content-requested accuracy bypass guarantees a hit without "
              "consuming battle random state");
    }

    const pokered::MoveEffectProgram* ordinary_effect =
        pokered::find_move_effect_program(battle_rules, 0U);
    check(state,
          ordinary_effect != nullptr &&
              ordinary_effect->key == "ordinary_damage" &&
              ordinary_effect->source_effect_ids ==
                  std::vector<std::uint8_t>{0U} &&
              ordinary_effect->instructions.size() == 4U,
          "ordinary damage moves resolve through an imported source-effect "
          "binding");
    const pokered::MoveEffectProgram* lower_defense =
        pokered::find_move_effect_program(battle_rules, 0x13U);
    check(state,
          battle_rules.move_effect_programs.size() == 25U &&
              lower_defense != nullptr &&
              lower_defense->key == "lower_defense_1" &&
              lower_defense->instructions.size() == 3U,
          "all direct stat-stage effects resolve through imported programs");
    if (stats != nullptr && experience != nullptr &&
        ordinary_effect != nullptr) {
        pokered::PokemonState player_mon;
        pokered::PokemonState enemy_mon;
        check(state,
              pokered::build_pokemon(
                  rules, *stats, 7U, 5U,
                  {15U, 15U, 15U, 15U}, 0x1234U, "RED",
                  player_mon, error) &&
                  pokered::build_pokemon(
                      rules, *stats, 1U, 5U,
                      {15U, 15U, 15U, 15U}, 0x2222U, "BLUE",
                      enemy_mon, error),
              "battle owner fixtures build from imported Pokemon content");

        pokered::PartyState player_party{{player_mon}};
        pokered::PartyState enemy_party{{enemy_mon}};
        pokered::BattleState exchange;
        const std::uint16_t player_hp = player_mon.current_hp;
        const std::uint16_t enemy_hp = enemy_mon.current_hp;
        check(state,
              pokered::begin_battle(
                  rules, battle_rules, player_party, enemy_party,
                  pokered::BattleKind::wild, 0x12345678U, exchange,
                  error) &&
                  pokered::execute_battle_turn(
                      rules, battle_rules, 0x1234U, player_party,
                      exchange, {0U, 0U}, error) &&
                  exchange.turn == 1U &&
                  exchange.phase == pokered::BattlePhase::choose_action &&
                  exchange.outcome == pokered::BattleOutcome::ongoing &&
                  player_party.members[0].moves[0].pp ==
                      player_mon.moves[0].pp - 1U &&
                  exchange.enemy_party.members[0].moves[0].pp ==
                      enemy_mon.moves[0].pp - 1U &&
                  player_party.members[0].current_hp < player_hp &&
                  exchange.enemy_party.members[0].current_hp < enemy_hp,
              "battle owner resolves a complete speed-ordered ordinary move "
              "exchange through imported effect and formula programs");

        const pokered::PokemonState before_stat_move =
            player_party.members[0];
        check(state,
              pokered::execute_battle_turn(
                  rules, battle_rules, 0x1234U, player_party, exchange,
                  {1U, 0U}, error) &&
                  player_party.members[0].moves[1].pp ==
                      before_stat_move.moves[1].pp - 1U &&
                  exchange.enemy.stat_stages[1] == 6U,
              "imported Tail Whip program consumes PP and lowers enemy "
              "defense one stage");

        pokered::PartyState gate_player{{player_mon}};
        pokered::BattleState enemy_gate;
        check(state,
              pokered::begin_battle(
                  rules, battle_rules, gate_player,
                  pokered::PartyState{{enemy_mon}},
                  pokered::BattleKind::wild, 1U, enemy_gate, error) &&
                  pokered::execute_battle_turn(
                      rules, battle_rules, 0x1234U, gate_player,
                      enemy_gate, {0U, 1U}, error) &&
                  enemy_gate.player.stat_stages[0] == 7U &&
                  std::ranges::any_of(
                      enemy_gate.events,
                      [](const pokered::BattleEvent& event) {
                          return event.kind ==
                                     pokered::BattleEventKind::failed &&
                                 !event.player_actor;
                      }),
              "enemy stat-down program applies its imported random failure "
              "gate before accuracy");

        pokered::PartyState victory_party{{player_mon}};
        enemy_mon.current_hp = 1U;
        pokered::BattleState victory;
        const std::uint32_t old_experience =
            victory_party.members[0].experience;
        check(state,
              pokered::begin_battle(
                  rules, battle_rules, victory_party,
                  pokered::PartyState{{enemy_mon}},
                  pokered::BattleKind::wild, 0x3456789AU, victory,
                  error),
              "battle owner starts a deterministic victory fixture");
        victory.player.accuracy_bypassed = true;
        check(state,
              pokered::execute_battle_turn(
                  rules, battle_rules, 0x1234U, victory_party, victory,
                  {0U, 0U}, error) &&
                  !victory.active &&
                  victory.phase == pokered::BattlePhase::finished &&
                  victory.outcome ==
                      pokered::BattleOutcome::player_victory &&
                  victory.enemy_party.members[0].current_hp == 0U &&
                  victory_party.members[0].experience > old_experience &&
                  std::ranges::any_of(
                      victory.events,
                      [](const pokered::BattleEvent& event) {
                          return event.kind ==
                                 pokered::BattleEventKind::gained_experience;
                      }),
              "ordinary battle victory faints the enemy, awards imported "
              "experience, and reaches a finished outcome");
    }
}

void test_local_boot_cache(TestState& state) {
    // The generated cache is optional in a clean public checkout. When a local
    // ROM has been imported, drive the actual title-to-New Game owner headlessly.
    const std::filesystem::path path =
        std::filesystem::path(POKERED_MODERN_SOURCE_DIR) / "data" / "runtime" /
        "imports" / "pokemon_red_us_rev_0" / "compiled" / "boot_content.bin";
    if (!std::filesystem::exists(path)) {
        std::puts("boot cache test skipped: no local imported campaign");
        return;
    }
    pokered::BootContent content;
    std::string error;
    check(state, pokered::load_boot_content(path, content, error),
          "local boot cache loads");
    if (!content.loaded) return;
    check(state,
          content.images.size() == 27U && content.title.species.size() == 16U &&
              content.ui_tiles.size() == 256U * 64U,
          "boot cache retains complete title graphics and UI tiles");
    check(state,
          content.oak.player_names ==
                  std::array<std::string, 4>{"NEW NAME", "RED", "ASH", "JACK"} &&
              content.oak.rival_names ==
                  std::array<std::string, 4>{"NEW NAME", "BLUE", "GARY", "JOHN"},
          "boot cache retains cartridge default names");

    pokered::BootState boot;
    check(state, pokered::begin_boot(content, boot, error),
          "normal boot owner starts at title");
    pokered::BootStepResult result;
    for (std::size_t guard = 0U;
         guard < 1000U && boot.title_startup_frames != 0U; ++guard)
        check(state, pokered::step_boot(content, {}, boot, result, error),
              "title startup advances");
    check(state,
          pokered::step_boot(content, {.confirm_pressed = true}, boot, result,
                             error) &&
              boot.screen == pokered::BootScreen::main_menu,
          "title input opens imported main menu");
    while (boot.delay_frames != 0U)
        check(state, pokered::step_boot(content, {}, boot, result, error),
              "main-menu input guard advances");
    check(state,
          pokered::step_boot(content, {.confirm_pressed = true}, boot, result,
                             error) &&
              boot.screen == pokered::BootScreen::oak_text,
          "New Game enters imported Oak introduction");

    // Drain imported pages and select the first concrete defaults. The finite
    // guard proves this path emits a semantic campaign handoff.
    for (std::size_t guard = 0U;
         guard < 2000U && !result.new_game_requested; ++guard) {
        pokered::BootInput input;
        if (boot.screen == pokered::BootScreen::oak_text &&
            !boot.picture_sliding) {
            input.confirm_pressed = true;
        } else if (boot.screen == pokered::BootScreen::name_menu &&
                   !boot.picture_sliding) {
            if (boot.name_selection == 0U)
                input.down_pressed = true;
            else
                input.confirm_pressed = true;
        }
        check(state, pokered::step_boot(content, input, boot, result, error),
              "Oak New Game path advances");
        if (!error.empty()) break;
    }
    check(state, result.new_game_requested,
          "Oak sequence emits New Game handoff");
    check(state, boot.player_name == "RED" && boot.rival_name == "BLUE",
          "New Game handoff retains selected names");
    check(state,
          content.new_game_map_id == 0x26U && content.new_game_x == 3U &&
              content.new_game_y == 6U,
          "New Game placement comes from imported campaign content");
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
    test_battle_ui(state);
    test_host_settings_and_clocks(state);
    test_world_spaces_and_warps(state);
    test_local_encounter_cache(state);
    test_local_rule_cache(state);
    test_local_boot_cache(state);
    if (state.failures == 0) std::puts("foundation tests passed");
    return state.failures == 0 ? 0 : 1;
}
