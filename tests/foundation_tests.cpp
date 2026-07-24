#include "animations.hpp"
#include "battle.hpp"
#include "battle_animation_lab.hpp"
#include "battle_controller.hpp"
#include "battle_rules.hpp"
#include "battle_view.hpp"
#include "boot.hpp"
#include "campaign_programs.hpp"
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
#include <fstream>
#include <iterator>
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
    pokered::sexpr::Document hexadecimal;
    pokered::Diagnostics hexadecimal_diagnostics;
    check(state,
          pokered::sexpr::parse("hex.sexpr", "set_flag 0x6ba38\n",
                                hexadecimal,
                                hexadecimal_diagnostics),
          "hexadecimal content IDs parse");

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
        .trainers = {},
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

    // Trainer sight is indexed by world cell and owns the approach before it
    // emits the same activation used by direct conversation.
    pokered::WorldState sight_world;
    sight_world.loaded = true;
    sight_world.tilesets.push_back({
        .id = 0,
        .tile_count = 1,
        .animation_mode = 0,
        .passable_tiles = {0},
        .pixels = std::vector<std::uint8_t>(64U, 0),
        .animation_pixels = {},
    });
    sight_world.spaces = {
        {.id = 0, .key = "sight_fixture", .outdoor = true},
    };
    sight_world.maps.push_back({
        .id = 9,
        .tileset_id = 0,
        .width_blocks = 1,
        .height_blocks = 1,
        .width_tiles = 4,
        .height_tiles = 4,
        .key = "sight_map",
        .display_name = "Sight",
        .global_x_tiles = 0,
        .global_y_tiles = 0,
        .world_space = 0,
        .tiles = std::vector<std::uint8_t>(16U, 0),
        .warps = {},
        .actors =
            {{
                .index = 1,
                .sprite_id = 1,
                .x = 1,
                .y = 0,
                .movement = 0xFF,
                .direction_or_axis = 0xD0,
                .text_id = 1,
                .parameter_a = 201,
                .parameter_b = 1,
                .kind = pokered::WorldActorKind::trainer_or_pokemon,
                .movement_bounds = std::nullopt,
            }},
    });
    pokered::InteractionCatalog sight_interactions;
    sight_interactions.loaded = true;
    sight_interactions.maps.push_back({
        .map_id = 9,
        .decoded = true,
        .backgrounds = {},
        .actors =
            {{.index = 1, .x = 1, .y = 0, .program_id = 1}},
        .trainers =
            {{
                .actor_index = 1,
                .sight_range = 1,
                .defeated_flag = 100,
                .before_pages = {"Fight."},
                .after_pages = {"Done."},
                .end_pages = {"Lost."},
            }},
        .programs =
            {{
                .status = pokered::InteractionProgramStatus::dialogue,
                .pages = {"Fight."},
            }},
    });
    check(state,
          pokered::initialize_world_runtime(
              sight_world, sight_interactions, error),
          "trainer sight fixture initializes its spatial index");
    sight_world.player.x = 0;
    sight_world.player.y = 1;
    sight_world.player.map_index = 0;
    sight_world.player.move_cooldown = 0;
    pokered::step_world(sight_world, sight_interactions, campaign,
                        {.right = true});
    check(state, sight_world.trainer_approach.active,
          "entering an imported sight ray starts trainer approach");
    bool sight_activated = false;
    for (std::size_t tick = 0U; tick < 20U && !sight_activated;
         ++tick) {
        pokered::step_world(sight_world, sight_interactions, campaign,
                            {});
        sight_activated =
            sight_world.last_actor_activation.occurred;
    }
    check(state,
          sight_activated &&
              sight_world.last_actor_activation.map_id == 9U &&
              sight_world.last_actor_activation.actor_index == 1U,
          "completed trainer approach emits its indexed actor activation");
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

    // Every trainer/static-Pokemon map actor must resolve through imported
    // policy. This proves the engine needs no Pokemon Red opponent offset.
    pokered::WorldState world;
    check(state,
          pokered::load_world(path.parent_path() / "world_maps.bin", world,
                              error),
          "complete world cache loads for actor opponents");
    std::size_t opponent_actors = 0U;
    std::size_t trainer_actors = 0U;
    std::size_t static_pokemon_actors = 0U;
    bool every_actor_resolved = world.loaded;
    for (const pokered::WorldMap& map : world.maps) {
        for (const pokered::WorldActorSpawn& actor : map.actors) {
            if (actor.kind !=
                pokered::WorldActorKind::trainer_or_pokemon)
                continue;
            ++opponent_actors;
            pokered::ActorOpponentBinding binding;
            if (!pokered::resolve_actor_opponent(
                    trainers, actor.parameter_a, actor.parameter_b,
                    binding, error)) {
                every_actor_resolved = false;
                continue;
            }
            if (binding.kind ==
                pokered::ActorOpponentKind::trainer)
                ++trainer_actors;
            else
                ++static_pokemon_actors;
        }
    }
    if (!every_actor_resolved || opponent_actors != 346U ||
        trainer_actors != 334U || static_pokemon_actors != 12U)
        std::fprintf(stderr,
                     "actor opponent counts: total=%zu trainers=%zu "
                     "static=%zu last_error=%s\n",
                     opponent_actors, trainer_actors,
                     static_pokemon_actors, error.c_str());
    check(state,
          every_actor_resolved && opponent_actors == 346U &&
              trainer_actors == 334U &&
              static_pokemon_actors == 12U,
          "all map-owned trainer and static-Pokemon actors resolve through "
          "imported semantic bindings");

    pokered::InteractionCatalog interaction_catalog;
    check(state,
          pokered::load_interactions(
              path.parent_path() / "world_interactions.bin",
              interaction_catalog, error),
          "complete world interaction cache loads for trainer ownership");
    std::size_t trainer_interaction_count = 0U;
    for (const pokered::MapInteractions& map :
         interaction_catalog.maps)
        trainer_interaction_count += map.trainers.size();
    const pokered::TrainerInteractionRule* articuno =
        pokered::find_trainer_interaction(interaction_catalog, 162U, 3U);
    check(state,
          trainer_interaction_count == 322U && articuno != nullptr &&
              articuno->sight_range == 0U &&
              articuno->defeated_flag == 443410U &&
              !articuno->before_pages.empty(),
          "all trainer headers load, including the static encounter whose "
          "flag bit differs from its actor index");

    pokered::BattleRuleCatalog battle_rules;
    check(state,
          pokered::load_battle_rules(
              path.parent_path() / "battle_rules.bin", battle_rules,
              error),
          "battle rules load for trainer interaction ownership");
    pokered::BattleAnimationLab battle_view;
    pokered::Diagnostics battle_diagnostics;
    check(state,
          pokered::load_battle_animation_lab(
              path.parent_path().parent_path() / "source" /
                  "animations" / "battle_moves",
              battle_view, battle_diagnostics),
          "battle presentation loads for trainer interaction ownership");
    const pokered::StatFormulaProgram* stat_formula =
        pokered::find_stat_formula(
            battle_rules, battle_rules.original_stat_formula);
    pokered::CampaignState trainer_campaign;
    check(state,
          pokered::begin_new_campaign(trainer_campaign, "RED", "BLUE",
                                      {}, error),
          "trainer interaction fixture creates campaign state");
    pokered::PokemonState starter;
    check(state,
          stat_formula != nullptr &&
              pokered::build_pokemon(
                  rules, *stat_formula, 1U, 12U, {15U, 15U, 15U, 15U},
                  trainer_campaign.trainer_id, "BULBASAUR", starter,
                  error),
          "trainer interaction fixture creates a usable party member");
    trainer_campaign.party.members.push_back(std::move(starter));
    world.last_actor_activation = {
        .map_id = 14U,
        .actor_index = 2U,
        .occurred = true,
    };
    bool trainer_began = false;
    check(state,
          pokered::service_world_actor_battle(
              interaction_catalog, trainers, world, rules, battle_rules,
              trainer_campaign, battle_view, trainer_began, error) &&
              !trainer_began && world.dialogue.open &&
              world.opponent_request.pending,
          "fresh trainer activation presents imported before-battle text");
    world.last_actor_activation = {};
    world.dialogue = {};
    check(state,
          pokered::service_world_actor_battle(
              interaction_catalog, trainers, world, rules, battle_rules,
              trainer_campaign, battle_view, trainer_began, error) &&
              trainer_began && trainer_campaign.battle.active &&
              trainer_campaign.battle.kind ==
                  pokered::BattleKind::trainer &&
              trainer_campaign.battle_owner.active,
          "acknowledged trainer dialogue starts its indexed party battle");
    const std::uint32_t trainer_flag =
        trainer_campaign.battle_owner.defeated_flag;
    trainer_campaign.battle.active = false;
    trainer_campaign.battle.outcome =
        pokered::BattleOutcome::player_victory;
    pokered::finish_world_actor_battle(
        interaction_catalog, world, trainer_campaign);
    check(state,
          pokered::campaign_flag(trainer_campaign, trainer_flag) &&
              world.dialogue.open,
          "trainer victory persists its imported flag and presents end text");
    world.dialogue = {};
    world.last_actor_activation = {
        .map_id = 162U,
        .actor_index = 3U,
        .occurred = true,
    };
    check(state,
          pokered::service_world_actor_battle(
              interaction_catalog, trainers, world, rules, battle_rules,
              trainer_campaign, battle_view, trainer_began, error) &&
              !trainer_began && world.opponent_request.pending &&
              world.dialogue.open,
          "static Pokemon activation presents its imported battle text");
    world.last_actor_activation = {};
    world.dialogue = {};
    check(state,
          pokered::service_world_actor_battle(
              interaction_catalog, trainers, world, rules, battle_rules,
              trainer_campaign, battle_view, trainer_began, error) &&
              trainer_began &&
              trainer_campaign.battle.kind == pokered::BattleKind::wild &&
              trainer_campaign.battle.enemy_party.members.size() == 1U &&
              trainer_campaign.battle.enemy_party.members.front()
                      .species_dex == 144U &&
              trainer_campaign.battle.enemy_party.members.front().level ==
                  50U,
          "static Pokemon actor starts its imported species and level battle");

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

    pokered::BootState typed_name;
    check(state, pokered::begin_boot(content, typed_name, error),
          "typed-name fixture starts a boot owner");
    typed_name.screen = pokered::BootScreen::naming;
    typed_name.oak_stage = pokered::BootOakStage::player_name;
    typed_name.naming_player = true;
    check(state,
          pokered::step_boot(content, {.text = "VEGA"}, typed_name,
                             result, error) &&
              typed_name.naming_value == "VEGA",
          "Oak naming accepts ordinary typed input");
    check(state,
          pokered::step_boot(content, {.submit_pressed = true},
                             typed_name, result, error) &&
              typed_name.player_name == "VEGA" &&
              typed_name.screen == pokered::BootScreen::oak_text,
          "typed Oak name submits through the normal confirmation flow");
}

void test_local_pallet_campaign_program(TestState& state) {
    // Exercise one imported campaign fiber end to end: trigger Oak, advance both
    // dialogue sections, consume the ROM movement streams, and cross the
    // ordinary Pallet-to-lab warp.
    const std::filesystem::path root =
        std::filesystem::path(POKERED_MODERN_SOURCE_DIR) / "data" /
        "runtime" / "imports" / "pokemon_red_us_rev_0" / "compiled";
    const std::filesystem::path program_path =
        root / "campaign_programs.bin";
    if (!std::filesystem::exists(program_path)) {
        std::puts(
            "campaign program test skipped: no local imported campaign");
        return;
    }

    pokered::WorldState world;
    pokered::InteractionCatalog interactions;
    pokered::CampaignProgramCatalog programs;
    pokered::RuleCatalog rules;
    pokered::BattleRuleCatalog battle_rules;
    pokered::TrainerCatalog trainers;
    pokered::BattleAnimationLab battle_view;
    pokered::CampaignState campaign;
    std::string error;
    check(state, pokered::load_world(root / "world_maps.bin", world, error),
          "campaign fixture loads imported world");
    check(state,
          pokered::load_interactions(root / "world_interactions.bin",
                                     interactions, error),
          "campaign fixture loads imported interactions");
    check(state,
          pokered::find_trainer_interaction(
              interactions, 51U, 2U) != nullptr &&
              pokered::find_trainer_interaction(
                  interactions, 51U, 3U) != nullptr &&
              pokered::find_trainer_interaction(
                  interactions, 51U, 4U) != nullptr,
          "Viridian Forest's three trainer actors retain imported battle interactions");
    check(state,
          pokered::load_campaign_programs(program_path, programs, error),
          "campaign fixture loads imported programs");
    check(state,
          programs.naming.maximum_length == 10U &&
              programs.inventory_stack_capacity == 20U &&
              programs.starting_money == 3000U &&
              programs.party_capacity == 6U &&
              programs.storage_box_count == 12U &&
              programs.storage_box_capacity == 20U &&
              programs.programs.size() == 44U &&
              programs.encounter_suppression_zones.size() == 1U &&
              programs.item_names.size() == 138U &&
              programs.item_names.front().item_id == 1U &&
              programs.item_names.front().name == "MASTER BALL" &&
              programs.item_names.back().item_id == 0xFAU &&
              programs.item_names.back().name == "TM50" &&
              !programs.found_item_pages.empty() &&
              programs.found_item_pages.front().find(
                  "\n{item_name}") !=
                  std::string::npos &&
              !programs.no_item_room_pages.empty() &&
              programs.naming.uppercase.front() == "A" &&
              programs.naming.lowercase.front() == "a" &&
              programs.naming.uppercase.back() == "END" &&
              programs.nickname_heading.find("{name_buffer}") !=
                  std::string::npos,
          "campaign fixture loads the ROM naming profile");
    pokered::NamingState controller_name;
    pokered::begin_naming(programs.naming, "NICKNAME?",
                          controller_name);
    pokered::step_naming({.confirm = true}, controller_name);
    pokered::step_naming({.toggle_case = true}, controller_name);
    pokered::step_naming({.confirm = true}, controller_name);
    pokered::step_naming({.submit = true}, controller_name);
    check(state,
          controller_name.decided && !controller_name.open &&
              controller_name.value == "Aa",
          "imported naming profile supports controller-only editing");
    check(state,
          pokered::load_rules(root / "pokemon_rules.bin", rules, error),
          "campaign fixture loads imported Pokemon rules");
    check(state,
          pokered::load_battle_rules(root / "battle_rules.bin",
                                     battle_rules, error),
          "campaign fixture loads imported battle rules");
    check(state,
          pokered::load_trainers(root / "trainers.bin", trainers, error),
          "campaign fixture loads imported trainers");
    pokered::Diagnostics battle_diagnostics;
    check(state,
          pokered::load_battle_animation_lab(
              root.parent_path() / "source" / "animations" /
                  "battle_moves",
              battle_view, battle_diagnostics),
          "campaign fixture loads battle presentation");
    constexpr std::array<std::string_view, 19> source_names{
        "pallet_oak_interception.sexpr",
        "oaks_lab_choose_charmander.sexpr",
        "oaks_lab_choose_squirtle.sexpr",
        "oaks_lab_choose_bulbasaur.sexpr",
        "oaks_lab_first_rival_after_charmander.sexpr",
        "oaks_lab_first_rival_after_squirtle.sexpr",
        "oaks_lab_first_rival_after_bulbasaur.sexpr",
        "viridian_mart_oaks_parcel.sexpr",
        "oaks_lab_deliver_parcel_and_get_pokedex.sexpr",
        "route_22_first_rival_after_charmander.sexpr",
        "route_22_first_rival_after_squirtle.sexpr",
        "route_22_first_rival_after_bulbasaur.sexpr",
        "oaks_lab_pokeballs.sexpr",
        "blues_house_daisy_town_map.sexpr",
        "pallet_reward_updates.sexpr",
        "route_1_potion.sexpr",
        "pewter_city.sexpr",
        "mt_moon_fossils.sexpr",
        "mt_moon_magikarp_sale.sexpr",
    };
    for (const std::string_view source_name : source_names) {
        const std::filesystem::path source_path =
            root.parent_path() / "source" / "scripts" / "campaign" /
            source_name;
        std::ifstream source_input(source_path);
        const std::string source{
            std::istreambuf_iterator<char>(source_input),
            std::istreambuf_iterator<char>()};
        pokered::sexpr::Document source_document;
        pokered::Diagnostics source_diagnostics;
        check(state, source_input.good() || source_input.eof(),
              "campaign fixture reads generated source");
        check(state,
              pokered::sexpr::parse(source_path.string(), source,
                                    source_document,
                                    source_diagnostics),
              "generated campaign source reparses");
    }
    const std::filesystem::path naming_source_path =
        root.parent_path() / "source" / "menus" / "naming.sexpr";
    std::ifstream naming_source_input(naming_source_path);
    const std::string naming_source{
        std::istreambuf_iterator<char>(naming_source_input),
        std::istreambuf_iterator<char>()};
    pokered::sexpr::Document naming_document;
    pokered::Diagnostics naming_diagnostics;
    check(state,
          (naming_source_input.good() || naming_source_input.eof()) &&
              pokered::sexpr::parse(
                  naming_source_path.string(), naming_source,
                  naming_document, naming_diagnostics),
          "generated naming profile reparses");
    const std::filesystem::path visibility_source_path =
        root.parent_path() / "source" / "world" /
        "initial_actor_visibility.sexpr";
    std::ifstream visibility_source_input(visibility_source_path);
    const std::string visibility_source{
        std::istreambuf_iterator<char>(visibility_source_input),
        std::istreambuf_iterator<char>()};
    pokered::sexpr::Document visibility_document;
    pokered::Diagnostics visibility_diagnostics;
    check(state,
          (visibility_source_input.good() ||
           visibility_source_input.eof()) &&
              pokered::sexpr::parse(
                  visibility_source_path.string(),
                  visibility_source, visibility_document,
                  visibility_diagnostics),
          "generated initial actor visibility reparses");
    check(state,
          pokered::begin_new_campaign(campaign, "RED", "BLUE", {}, error),
          "campaign fixture owns New Game state");
    if (!world.loaded || !interactions.loaded || !programs.loaded ||
        !rules.loaded || !battle_rules.loaded || !trainers.loaded ||
        !battle_view.loaded || !campaign.initialized)
        return;
    check(state,
          pokered::initialize_world_runtime(world, interactions, error),
          "campaign fixture initializes world indexes");
    check(state,
          pokered::initialize_campaign_program_runtime(programs, world,
                                                       error),
          "campaign fixture applies initial actor visibility");
    const auto actor_visible =
        [&](std::uint8_t map_id, std::uint8_t actor_index) {
            for (const pokered::WorldActorState& actor :
                 world.actors) {
                const pokered::WorldMap& map =
                    world.maps[actor.map_index];
                const pokered::WorldActorSpawn& spawn =
                    map.actors[actor.spawn_index];
                if (map.id == map_id &&
                    spawn.index == actor_index)
                    return actor.visible;
            }
            return false;
        };
    check(state,
          !actor_visible(0U, 1U) &&
              actor_visible(1U, 5U) &&
              !actor_visible(1U, 7U) &&
              !actor_visible(33U, 1U) &&
              !actor_visible(33U, 2U),
          "cartridge toggle table initializes campaign actor visibility");
    check(state, pokered::enter_world_at(world, 0U, 10, 1, error),
          "campaign fixture enters Pallet north exit");

    constexpr std::uint32_t followed_oak_flag = 0x6BA38U;
    constexpr std::uint32_t oak_appeared_flag = 0x6BA5FU;
    constexpr std::uint32_t oak_asked_to_choose_flag = 0x6BA59U;
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign, error),
          "Pallet trigger starts its campaign fiber");
    check(state,
          campaign.input_locked &&
              pokered::campaign_flag(campaign, oak_appeared_flag) &&
              world.dialogue.open &&
              world.dialogue.pages.front().find("Hey! Wait!") !=
                  std::string::npos,
          "Pallet trigger locks input, records Oak, and opens ROM dialogue");

    for (std::size_t guard = 0U;
         guard < 2000U && campaign.fiber.active; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world, campaign, error),
              "Pallet campaign fiber advances");
        if (!error.empty()) {
            std::fprintf(stderr, "Pallet campaign error: %s\n",
                         error.c_str());
            break;
        }
    }
    check(state, error.empty(), "Pallet campaign fiber reports no error");
    check(state,
          !campaign.fiber.active && !campaign.input_locked &&
              pokered::campaign_flag(campaign, followed_oak_flag) &&
              pokered::campaign_flag(campaign,
                                     oak_asked_to_choose_flag),
          "Pallet campaign fiber completes and records progression");
    check(state,
          world.player.map_index < world.maps.size() &&
              world.maps[world.player.map_index].id == 40U &&
              world.player.x == 5 && world.player.y == 3,
          "ROM movement streams walk the player through the lab warp to the starters");

    // Exercise both answers on the imported Charmander-ball program. Declining
    // must leave all campaign state untouched; accepting must construct the
    // level-five party member and execute Blue's ROM-derived selection route.
    world.player.facing = pokered::WorldDirection::right;
    pokered::step_world(world, interactions, campaign, {.activate = true});
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign, error),
          "starter actor activation begins its campaign program");
    check(state,
          world.choice.open && world.choice.options.size() == 2U &&
              world.dialogue.open &&
              world.dialogue.pages.front().find("CHARMANDER") !=
                  std::string::npos,
          "starter program presents imported dialogue and a choice");
    pokered::step_world(world, interactions, campaign, {.down = true});
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign, error),
          "starter choice accepts navigation");
    pokered::step_world(world, interactions, campaign, {.activate = true});
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign, error),
          "declined starter choice exits its campaign program");
    check(state,
          !campaign.fiber.active && !campaign.input_locked &&
              campaign.party.members.empty() &&
              !pokered::campaign_flag(campaign, 0x6BA5AU),
          "declining a starter has no progression side effects");

    pokered::step_world(world, interactions, campaign, {.activate = true});
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign, error),
          "starter actor can be activated again after declining");
    pokered::step_world(world, interactions, campaign, {.activate = true});
    for (std::size_t guard = 0U;
         guard < 1000U && campaign.fiber.active; ++guard) {
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world, campaign, error),
              "accepted starter program advances");
        if (world.naming.open)
            pokered::step_world(
                world, interactions, campaign,
                {.submit = true, .text = "EMBER"});
        else
            pokered::step_world(
                world, interactions, campaign,
                {.activate = world.dialogue.open ||
                             world.choice.open});
        if (!error.empty()) break;
    }
    check(state, error.empty(), "starter campaign program reports no error");
    check(state,
          !campaign.fiber.active && !campaign.input_locked &&
              campaign.party.members.size() == 1U &&
              campaign.party.members.front().species_dex == 4U &&
              campaign.party.members.front().level == 5U &&
              campaign.party.members.front().nickname == "EMBER" &&
              pokered::campaign_variable(campaign, 0U) == 4U &&
              pokered::campaign_variable(campaign, 1U) == 7U &&
              pokered::campaign_flag(campaign, 0x6BA5AU),
          "accepted starter creates imported player/rival state");

    check(state,
          !actor_visible(40U, 2U) &&
              !actor_visible(40U, 3U) &&
              actor_visible(40U, 4U),
          "player and rival starter balls are removed from the lab");

    // Cross the imported y=6 challenge line, let Blue approach, hand the
    // decoded RIVAL1 party to the real battle owner, and then resume the same
    // campaign fiber after the deterministic battle finishes.
    check(state, pokered::enter_world_at(world, 40U, 5, 6, error),
          "campaign fixture reaches the lab exit line");
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign, error),
          "lab exit starts the imported rival challenge");
    for (std::size_t guard = 0U;
         guard < 1000U &&
         !campaign.trainer_battle_request.pending; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world, campaign, error),
              "lab rival challenge advances to battle");
        if (!error.empty()) break;
    }
    check(state,
          campaign.trainer_battle_request.pending &&
              campaign.trainer_battle_request.trainer_class_id == 25U &&
              campaign.trainer_battle_request.trainer_party_index == 0U,
          "rival starter selects the imported first RIVAL1 party");

    bool battle_began = false;
    check(state,
          pokered::begin_campaign_trainer_battle(
              trainers, world, rules, battle_rules, campaign, battle_view,
              battle_began, error),
          "campaign request begins the owned trainer battle");
    check(state,
          battle_began && campaign.battle.active &&
              campaign.battle.enemy_party.members.size() == 1U &&
              campaign.battle.enemy_party.members.front().species_dex ==
                  7U &&
              campaign.battle.enemy_party.members.front().level == 5U,
          "first rival battle materializes Blue's imported Squirtle");

    for (std::size_t guard = 0U;
         guard < 100U && campaign.battle.active; ++guard) {
        const std::optional<std::size_t> player_move =
            pokered::first_executable_move_slot(
                rules, battle_rules, campaign.party.members.front());
        const std::optional<std::size_t> enemy_move =
            pokered::first_executable_move_slot(
                rules, battle_rules,
                campaign.battle.enemy_party.members.front());
        check(state, player_move.has_value() && enemy_move.has_value(),
              "first rival battlers have executable moves");
        if (!player_move.has_value() || !enemy_move.has_value()) break;
        check(state,
              pokered::execute_battle_turn(
                  rules, battle_rules, campaign.trainer_id,
                  campaign.party, campaign.battle,
                  {
                      .player_move_slot = *player_move,
                      .enemy_move_slot = *enemy_move,
                  },
                  error),
              "first rival battle executes a turn");
        if (!error.empty()) break;
    }
    check(state,
          !campaign.battle.active &&
              campaign.battle.outcome != pokered::BattleOutcome::ongoing,
          "first rival battle reaches a real outcome");

    for (std::size_t guard = 0U;
         guard < 1000U && campaign.fiber.active; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world, campaign, error),
              "post-battle lab campaign advances");
        if (!error.empty()) break;
    }
    check(state, error.empty(),
          "first rival campaign sequence reports no error");
    check(state,
          !campaign.fiber.active && !campaign.input_locked &&
              pokered::campaign_flag(campaign, 0x6BA5BU) &&
              campaign.party.members.front().current_hp ==
                  campaign.party.members.front().stats.hp &&
              !actor_visible(40U, 1U),
          "lab rival outcome heals the party and completes Blue's exit");

    // Enter the Viridian Mart through its real city warp. The imported
    // map-entry fiber owns the clerk interruption, simulated player path,
    // dialogue, parcel grant, and progression flag.
    check(state, pokered::enter_world_at(world, 1U, 29, 20, error),
          "campaign fixture reaches the Viridian Mart entrance");
    pokered::step_world(world, interactions, campaign, {.up = true});
    check(state,
          world.player.map_index < world.maps.size() &&
              world.maps[world.player.map_index].id == 42U &&
              world.last_warp.occurred &&
              world.last_warp.destination_map_id == 42U,
          "Viridian City entrance performs the imported mart warp");
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign, error),
          "Viridian Mart entry starts the parcel fiber");
    check(state,
          campaign.input_locked && world.dialogue.open &&
              world.dialogue.pages.front().find(
                  "You came from") != std::string::npos,
          "mart clerk interrupts with imported Pallet dialogue");

    for (std::size_t guard = 0U;
         guard < 1000U && campaign.fiber.active; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world, campaign, error),
              "Viridian Mart parcel fiber advances");
        if (!error.empty()) break;
    }
    check(state, error.empty(),
          "Viridian Mart parcel sequence reports no error");
    check(state,
          !campaign.fiber.active && !campaign.input_locked &&
              pokered::campaign_flag(campaign, 0x6BA71U) &&
              pokered::inventory_item_quantity(
                  campaign.inventory, 0x46U) == 1U,
          "parcel sequence grants the ROM-derived item and progression flag");
    check(state,
          world.player.map_index < world.maps.size() &&
              world.maps[world.player.map_index].id == 42U &&
              world.player.x == 2 && world.player.y == 5,
          "mart simulated joypad path ends beside the clerk");

    // Return through Pallet's ordinary lab warp, walk up the central aisle,
    // and activate Oak from below. The imported request program removes the
    // parcel, brings Blue back through the position-dependent path, delivers
    // the Pokedex, and opens the first Route 22 rival gate.
    check(state, pokered::enter_world_at(world, 0U, 12, 12, error),
          "campaign fixture reaches the Pallet lab entrance");
    pokered::step_world(world, interactions, campaign, {.up = true});
    check(state,
          world.player.map_index < world.maps.size() &&
              world.maps[world.player.map_index].id == 40U &&
              world.player.x == 5 && world.player.y == 11,
          "Pallet entrance performs the imported Oak's Lab warp");
    for (std::size_t guard = 0U;
         guard < 200U && world.player.y > 3; ++guard) {
        pokered::step_world(world, interactions, campaign,
                            {.up = true});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "campaign fixture walks the lab aisle");
        if (!error.empty()) break;
    }
    check(state,
          error.empty() && world.player.x == 5 &&
              world.player.y == 3 &&
              world.player.facing ==
                  pokered::WorldDirection::up,
          "campaign fixture reaches Oak from below");
    pokered::step_world(world, interactions, campaign,
                        {.activate = true});
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error),
          "activating Oak starts the imported parcel return");
    check(state,
          campaign.input_locked && world.dialogue.open &&
              world.dialogue.pages.front().find(
                  "OAK: Oh, RED!") != std::string::npos,
          "Oak return begins with imported delivery dialogue");

    for (std::size_t guard = 0U;
         guard < 3000U && campaign.fiber.active; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Oak request and Pokedex fiber advances");
        if (!error.empty()) break;
    }
    check(state, error.empty(),
          "Oak request and Pokedex sequence reports no error");
    check(state,
          !campaign.fiber.active && !campaign.input_locked &&
              pokered::inventory_item_quantity(
                  campaign.inventory, 0x46U) == 0U &&
              pokered::campaign_flag(campaign, 0x6BA5DU) &&
              pokered::campaign_flag(campaign, 0x6BA70U),
          "Oak consumes the parcel and grants Pokedex progression");
    check(state,
          pokered::campaign_flag(campaign, 0x6BF58U) &&
              !pokered::campaign_flag(campaign, 0x6BF59U) &&
              pokered::campaign_flag(campaign, 0x6BF5FU),
          "Oak request configures the imported first Route 22 rival flags");
    check(state,
          !actor_visible(40U, 1U) &&
              !actor_visible(40U, 6U) &&
              !actor_visible(40U, 7U) &&
              !actor_visible(1U, 5U) &&
              actor_visible(1U, 7U) &&
              actor_visible(33U, 1U) &&
              !actor_visible(33U, 2U),
          "Pokedex, old-man, and Route 22 actor toggles match the cartridge");

    // Enter the exact two-cell Route 22 sight trigger. The imported program
    // chooses Blue's approach, party, dialogue, exit, and progression from
    // the player coordinate and selected rival starter.
    check(state,
          pokered::enter_world_at(world, 33U, 29, 4, error),
          "campaign fixture reaches the first Route 22 rival trigger");
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error),
          "Route 22 rectangle starts the imported rival program");
    for (std::size_t guard = 0U;
         guard < 1000U &&
         !campaign.trainer_battle_request.pending; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Route 22 rival challenge advances to battle");
        if (!error.empty()) break;
    }
    check(state,
          error.empty() &&
              campaign.trainer_battle_request.pending &&
              campaign.trainer_battle_request.trainer_class_id ==
                  25U &&
              campaign.trainer_battle_request.trainer_party_index ==
                  3U,
          "rival starter selects the imported Route 22 RIVAL1 party");
    battle_began = false;
    check(state,
          pokered::begin_campaign_trainer_battle(
              trainers, world, rules, battle_rules, campaign,
              battle_view, battle_began, error),
          "Route 22 campaign request starts its trainer battle");
    check(state,
          battle_began && campaign.battle.active &&
              campaign.battle.enemy_party.members.size() == 2U &&
              campaign.battle.enemy_party.members[0].species_dex ==
                  16U &&
              campaign.battle.enemy_party.members[0].level == 9U &&
              campaign.battle.enemy_party.members[1].species_dex ==
                  7U &&
              campaign.battle.enemy_party.members[1].level == 8U,
          "Route 22 battle materializes Blue's imported two-Pokemon party");

    // Battle mechanics are exercised independently above. Return a victory
    // result here to isolate the campaign continuation and both path owners.
    campaign.battle.active = false;
    campaign.battle.outcome =
        pokered::BattleOutcome::player_victory;
    for (std::size_t guard = 0U;
         guard < 1000U && campaign.fiber.active; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Route 22 post-battle campaign advances");
        if (!error.empty()) {
            std::fprintf(stderr, "Route 22 campaign error: %s\n",
                         error.c_str());
            for (const pokered::WorldActorState& actor :
                 world.actors) {
                const pokered::WorldMap& map =
                    world.maps[actor.map_index];
                if (map.id == 33U)
                    std::fprintf(
                        stderr,
                        "Route 22 actor %u at %d,%d visible=%d\n",
                        static_cast<unsigned>(
                            map.actors[actor.spawn_index].index),
                        actor.x, actor.y,
                        actor.visible ? 1 : 0);
            }
            break;
        }
    }
    check(state,
          error.empty() && !campaign.fiber.active &&
              !campaign.input_locked &&
              pokered::campaign_flag(campaign, 0x6BF5DU) &&
              !pokered::campaign_flag(campaign, 0x6BF58U) &&
              !pokered::campaign_flag(campaign, 0x6BF5FU) &&
              !actor_visible(33U, 1U),
          "Route 22 victory records the beat flag and completes Blue's imported exit");

    // Return to Oak after the Route 22 victory. His source check-and-set event
    // and item tuple grant the imported five Poke Balls.
    check(state,
          pokered::enter_world_at(world, 40U, 5, 3, error),
          "campaign fixture returns below Oak");
    world.player.facing = pokered::WorldDirection::up;
    pokered::step_world(
        world, interactions, campaign, {.activate = true});
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error),
          "Oak's post-Route-22 reward program starts");
    for (std::size_t guard = 0U;
         guard < 1000U && campaign.fiber.active; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Oak's Poke Ball reward advances");
        if (!error.empty()) break;
    }
    check(state,
          error.empty() &&
              pokered::campaign_flag(campaign, 0x6BA5CU) &&
              pokered::inventory_item_quantity(
                  campaign.inventory, 4U) == 5U,
          "Oak grants the imported five-Poke-Ball stack and event");

    // Fill every remaining imported bag slot before asking Daisy for the Town
    // Map. Her failed grant must retain the map actor and progression, then a
    // second activation after space is freed must complete the source branch.
    for (std::uint16_t item_id = 100U;
         item_id < 119U; ++item_id)
        check(state,
              pokered::give_inventory_item(
                  campaign.inventory, item_id, 1U),
              "campaign fixture fills one imported bag slot");
    check(state,
          campaign.inventory.stacks.size() ==
              campaign.inventory.stack_capacity,
          "campaign fixture reaches imported bag capacity");
    check(state,
          pokered::enter_world_at(world, 39U, 2, 4, error),
          "campaign fixture enters Blue's House below Daisy");
    world.player.facing = pokered::WorldDirection::up;
    pokered::step_world(
        world, interactions, campaign, {.activate = true});
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error),
          "Daisy full-bag program starts");
    for (std::size_t guard = 0U;
         guard < 1000U && campaign.fiber.active; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Daisy full-bag branch advances");
        if (!error.empty()) break;
    }
    check(state,
          error.empty() &&
              !pokered::campaign_flag(campaign, 0x6BA50U) &&
              pokered::inventory_item_quantity(
                  campaign.inventory, 5U) == 0U &&
              actor_visible(39U, 3U),
          "Daisy's imported full-bag branch retains the Town Map");

    for (std::uint16_t item_id = 100U;
         item_id < 119U; ++item_id)
        check(state,
              pokered::take_inventory_item(
                  campaign.inventory, item_id, 1U),
              "campaign fixture frees one imported bag slot");
    pokered::step_world(
        world, interactions, campaign, {.activate = true});
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error),
          "Daisy Town Map program starts again");
    for (std::size_t guard = 0U;
         guard < 1000U && campaign.fiber.active; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Daisy Town Map gift advances");
        if (!error.empty()) break;
    }
    check(state,
          error.empty() &&
              pokered::campaign_flag(campaign, 0x6BA50U) &&
              pokered::inventory_item_quantity(
                  campaign.inventory, 5U) == 1U &&
              !actor_visible(39U, 3U),
          "Daisy grants the imported Town Map and hides its world actor");

    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error) &&
              pokered::campaign_flag(campaign, 0x6BA51U),
          "Blue's House map presence records its imported entry flag");
    check(state,
          pokered::enter_world_at(world, 0U, 12, 12, error),
          "campaign fixture returns to Pallet after its rewards");
    for (std::size_t update = 0U; update < 4U; ++update)
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Pallet reward state update advances");
    check(state,
          error.empty() &&
              pokered::campaign_flag(campaign, 0x6BA3EU) &&
              pokered::campaign_flag(campaign, 0x6BA52U) &&
              pokered::campaign_flag(campaign, 0x6BA5EU) &&
              !actor_visible(39U, 1U) &&
              actor_visible(39U, 2U),
          "Pallet updates both reward flags and swaps Daisy to her walking actor");

    check(state,
          pokered::enter_world_at(world, 12U, 5, 25, error),
          "campaign fixture reaches the Route 1 sample clerk");
    world.player.facing = pokered::WorldDirection::up;
    pokered::step_world(
        world, interactions, campaign, {.activate = true});
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error),
          "Route 1 potion sample program starts");
    for (std::size_t guard = 0U;
         guard < 1000U && campaign.fiber.active; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Route 1 potion sample advances");
        if (!error.empty()) break;
    }
    check(state,
          error.empty() &&
              pokered::campaign_flag(campaign, 0x6BDF8U) &&
              pokered::inventory_item_quantity(
                  campaign.inventory, 20U) == 1U,
          "Route 1 clerk grants the ROM-derived Potion sample");

    // Every ordinary loose Poké Ball actor uses the same runtime transaction:
    // its imported actor parameter supplies the item ID, and imported shared
    // text supplies success/full-bag presentation.
    check(state,
          pokered::enter_world_at(world, 51U, 25, 12, error),
          "campaign fixture reaches Viridian Forest loose items");
    for (std::uint16_t item_id = 100U; item_id < 117U; ++item_id)
        check(state,
              pokered::give_inventory_item(
                  campaign.inventory, item_id, 1U),
              "campaign fixture fills one bag stack");
    world.last_actor_activation = {
        .map_id = 51U,
        .actor_index = 5U,
        .occurred = true,
    };
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error),
          "full bag services a generic loose item");
    check(state,
          error.empty() &&
              pokered::inventory_item_quantity(
                  campaign.inventory, 11U) == 0U &&
              actor_visible(51U, 5U) && world.dialogue.open &&
              world.dialogue.pages.front().find("No more room") !=
                  std::string::npos,
          "full bag leaves the imported Antidote actor available");
    world.dialogue = {};
    for (std::uint16_t item_id = 100U; item_id < 117U; ++item_id)
        check(state,
              pokered::take_inventory_item(
                  campaign.inventory, item_id, 1U),
              "campaign fixture frees one bag stack");

    world.last_actor_activation = {
        .map_id = 51U,
        .actor_index = 5U,
        .occurred = true,
    };
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error),
          "generic Antidote pickup succeeds after freeing room");
    check(state,
          error.empty() &&
              pokered::inventory_item_quantity(
                  campaign.inventory, 11U) == 1U &&
              !actor_visible(51U, 5U) && world.dialogue.open &&
              world.dialogue.pages.front().find("ANTIDOTE") !=
                  std::string::npos,
          "generic pickup grants, names, and hides the imported actor");
    world.dialogue = {};

    world.last_actor_activation = {
        .map_id = 51U,
        .actor_index = 6U,
        .occurred = true,
    };
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error) &&
              pokered::inventory_item_quantity(
                  campaign.inventory, 20U) == 2U &&
              !actor_visible(51U, 6U),
          "generic Forest Potion pickup stacks and hides");
    world.dialogue = {};
    world.last_actor_activation = {
        .map_id = 51U,
        .actor_index = 7U,
        .occurred = true,
    };
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error) &&
              pokered::inventory_item_quantity(
                  campaign.inventory, 4U) == 6U &&
              !actor_visible(51U, 7U),
          "generic Forest Poké Ball pickup stacks and hides");
    world.dialogue = {};

    bool route_3_trainers_complete = true;
    for (std::uint8_t actor = 2U; actor <= 9U; ++actor)
        route_3_trainers_complete =
            route_3_trainers_complete &&
            pokered::find_trainer_interaction(
                interactions, 14U, actor) != nullptr;
    bool mt_moon_trainers_complete = true;
    for (std::uint8_t actor = 1U; actor <= 7U; ++actor)
        mt_moon_trainers_complete =
            mt_moon_trainers_complete &&
            pokered::find_trainer_interaction(
                interactions, 59U, actor) != nullptr;
    for (std::uint8_t actor = 2U; actor <= 5U; ++actor)
        mt_moon_trainers_complete =
            mt_moon_trainers_complete &&
            pokered::find_trainer_interaction(
                interactions, 61U, actor) != nullptr;
    check(state,
          route_3_trainers_complete && mt_moon_trainers_complete,
          "Route 3 and Mt. Moon ordinary trainers retain imported battle bindings");

    // The B2F Super Nerd is not an ordinary trainer interaction: his victory
    // gates both fossil transactions. Exercise the forced coordinate, imported
    // party, Dome grant, actor movement, other-fossil removal, and final text.
    check(state,
          pokered::enter_world_at(world, 61U, 13, 8, error),
          "campaign fixture reaches the Mt. Moon fossil gate");
    check(state,
          !pokered::campaign_suppresses_wild_encounters(
              programs, campaign, world),
          "Mt. Moon fossil area permits encounters before the Super Nerd victory");
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error) &&
              world.dialogue.open &&
              world.dialogue.pages.front().find("Hey, stop") !=
                  std::string::npos,
          "Mt. Moon fossil gate starts the imported Super Nerd program");
    for (std::size_t guard = 0U;
         guard < 1000U &&
         !campaign.trainer_battle_request.pending; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Mt. Moon Super Nerd challenge advances");
        if (!error.empty()) break;
    }
    check(state,
          campaign.trainer_battle_request.pending &&
              campaign.trainer_battle_request.trainer_class_id == 8U &&
              campaign.trainer_battle_request.trainer_party_index == 1U,
          "Mt. Moon fossil gate selects imported Super Nerd party 1");
    bool fossil_battle_began = false;
    check(state,
          pokered::begin_campaign_trainer_battle(
              trainers, world, rules, battle_rules, campaign,
              battle_view, fossil_battle_began, error),
          "Mt. Moon Super Nerd request begins its battle");
    check(state,
          fossil_battle_began &&
              campaign.battle.enemy_party.members.size() == 3U &&
              campaign.battle.enemy_party.members[0].species_dex == 88U &&
              campaign.battle.enemy_party.members[1].species_dex == 100U &&
              campaign.battle.enemy_party.members[2].species_dex == 109U,
          "Mt. Moon Super Nerd materializes imported Grimer Voltorb Koffing party");
    campaign.battle.active = false;
    campaign.battle.outcome =
        pokered::BattleOutcome::player_victory;
    for (std::size_t guard = 0U;
         guard < 1000U && campaign.fiber.active; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Mt. Moon Super Nerd victory advances");
        if (!error.empty()) break;
    }
    check(state,
          error.empty() &&
              pokered::campaign_flag(campaign, 0x6BFB1U) &&
              pokered::campaign_suppresses_wild_encounters(
                  programs, campaign, world),
          "Mt. Moon Super Nerd victory opens the fossil choice and suppresses area encounters");

    world.last_actor_activation = {
        .map_id = 61U,
        .actor_index = 6U,
        .occurred = true,
    };
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error) &&
              world.choice.open &&
              world.dialogue.pages.front().find("DOME") !=
                  std::string::npos,
          "Dome Fossil actor opens its imported confirmation");
    pokered::step_world(
        world, interactions, campaign, {.activate = true});
    for (std::size_t guard = 0U;
         guard < 1000U && campaign.fiber.active; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Dome Fossil transaction advances");
        if (!error.empty()) break;
    }
    check(state,
          error.empty() &&
              pokered::campaign_flag(campaign, 0x6BFB6U) &&
              pokered::inventory_item_quantity(
                  campaign.inventory, 41U) == 1U &&
              !actor_visible(61U, 6U) &&
              !actor_visible(61U, 7U),
          "Dome transaction grants its imported item and removes both fossil actors");
    world.dialogue = {};
    world.last_actor_activation = {
        .map_id = 61U,
        .actor_index = 1U,
        .occurred = true,
    };
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error) &&
              world.dialogue.open &&
              world.dialogue.pages.front().find("CINNABAR") !=
                  std::string::npos,
          "Super Nerd switches to imported post-fossil laboratory text");
    while (campaign.fiber.active) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Super Nerd post-fossil response completes");
        if (!error.empty()) break;
    }
    world.dialogue = {};

    // Pewter's east exit is blocked until Brock is defeated. The imported
    // trigger cell and gym warp become a semantic escort destination; the
    // generic world owner supplies collision-aware smooth paths.
    check(state,
          pokered::enter_world_at(world, 2U, 35, 17, error),
          "campaign fixture reaches Pewter's east gate");
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error) &&
              world.dialogue.open &&
              world.dialogue.pages.front().find("Follow me") !=
                  std::string::npos,
          "Pewter east gate starts the imported gym escort");
    for (std::size_t guard = 0U;
         guard < 4000U && campaign.fiber.active; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Pewter gym escort advances");
        if (!error.empty()) {
            std::fprintf(stderr, "Pewter escort error: %s\n",
                         error.c_str());
            break;
        }
    }
    check(state,
          error.empty() && !campaign.fiber.active &&
              !campaign.input_locked &&
              world.maps[world.player.map_index].id == 2U &&
              world.player.x == 16 && world.player.y == 18 &&
              actor_visible(2U, 5U),
          "Pewter escort reaches the imported gym warp and restores its guide");

    // The Pewter guide's ROM branch is a small but useful conditional-fiber
    // check: NO selects the "free service" response, then rejoins the common
    // party-order advice.
    check(state,
          pokered::enter_world_at(world, 54U, 7, 11, error),
          "campaign fixture enters Pewter Gym below its guide");
    world.player.facing = pokered::WorldDirection::up;
    pokered::step_world(
        world, interactions, campaign, {.activate = true});
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error) &&
              world.choice.open,
          "Pewter guide opens its imported yes/no advice");
    pokered::step_world(
        world, interactions, campaign, {.down = true});
    pokered::step_world(
        world, interactions, campaign, {.activate = true});
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error) &&
              world.dialogue.open &&
              world.dialogue.pages.front().find("free") !=
                  std::string::npos,
          "Pewter guide NO branch selects the imported free-service response");
    for (std::size_t guard = 0U;
         guard < 1000U && campaign.fiber.active; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Pewter guide advice advances");
        if (!error.empty()) break;
    }
    check(state,
          error.empty() && !campaign.input_locked,
          "Pewter guide rejoins and completes its imported advice");

    // Fill every imported stack slot so Brock's first reward pass exercises
    // the cartridge's retryable TM branch after a real battle handoff.
    for (std::uint16_t item_id = 100U; item_id < 115U; ++item_id)
        check(state,
              pokered::give_inventory_item(
                  campaign.inventory, item_id, 1U),
              "campaign fixture fills one Brock reward bag slot");
    check(state,
          pokered::enter_world_at(world, 54U, 4, 2, error),
          "campaign fixture reaches Brock");
    world.player.facing = pokered::WorldDirection::up;
    pokered::step_world(
        world, interactions, campaign, {.activate = true});
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error),
          "Brock activation starts its imported campaign fiber");
    for (std::size_t guard = 0U;
         guard < 1000U &&
         !campaign.trainer_battle_request.pending; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Brock challenge advances to battle");
        if (!error.empty()) break;
    }
    check(state,
          error.empty() &&
              campaign.trainer_battle_request.pending &&
              campaign.trainer_battle_request.trainer_class_id ==
                  34U &&
              campaign.trainer_battle_request.trainer_party_index ==
                  0U,
          "Brock fiber selects imported trainer class 34 party 0");
    battle_began = false;
    check(state,
          pokered::begin_campaign_trainer_battle(
              trainers, world, rules, battle_rules, campaign,
              battle_view, battle_began, error),
          "Brock request starts its owned trainer battle");
    check(state,
          battle_began && campaign.battle.active &&
              campaign.battle.enemy_party.members.size() == 2U &&
              campaign.battle.enemy_party.members[0].species_dex ==
                  74U &&
              campaign.battle.enemy_party.members[0].level == 12U &&
              campaign.battle.enemy_party.members[1].species_dex ==
                  95U &&
              campaign.battle.enemy_party.members[1].level == 14U,
          "Brock battle materializes imported Geodude and Onix");
    campaign.battle.active = false;
    campaign.battle.outcome =
        pokered::BattleOutcome::player_victory;
    for (std::size_t guard = 0U;
         guard < 1000U && campaign.fiber.active; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Brock victory and reward flow advances");
        if (!error.empty()) break;
    }
    check(state,
          error.empty() &&
              pokered::campaign_flag(campaign, 0x6BAAFU) &&
              !pokered::campaign_flag(campaign, 0x6BAAEU) &&
              pokered::campaign_flag(campaign, 0x69AB0U) &&
              pokered::campaign_flag(campaign, 0x6B950U) &&
              pokered::campaign_flag(campaign, 0x6BAAAU) &&
              !pokered::campaign_flag(campaign, 0x6BF58U) &&
              !pokered::campaign_flag(campaign, 0x6BF5FU) &&
              pokered::inventory_item_quantity(
                  campaign.inventory, 0xEAU) == 0U &&
              !actor_visible(2U, 5U),
          "Brock full-bag victory grants badge state, preserves TM retry, and applies map state");

    for (std::uint16_t item_id = 100U; item_id < 115U; ++item_id)
        check(state,
              pokered::take_inventory_item(
                  campaign.inventory, item_id, 1U),
              "campaign fixture frees one Brock reward bag slot");
    world.dialogue = {};
    world.last_actor_activation = {
        .map_id = 54U,
        .actor_index = 1U,
        .occurred = true,
    };
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error),
          "Brock TM retry starts after freeing room");
    for (std::size_t guard = 0U;
         guard < 1000U && campaign.fiber.active; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Brock TM retry advances");
        if (!error.empty()) break;
    }
    check(state,
          error.empty() &&
              pokered::campaign_flag(campaign, 0x6BAAEU) &&
              pokered::inventory_item_quantity(
                  campaign.inventory, 0xEAU) == 1U,
          "Brock retry grants imported TM34 and records its event");

    // The imported Mt. Moon sale exercises campaign economy and the original
    // party/current-box storage rule. A full box must leave both money and
    // event untouched; room in the same box accepts Magikarp and charges once.
    check(state,
          campaign.imported_initial_state &&
              campaign.money == 3000U &&
              campaign.storage.boxes.size() == 12U &&
              campaign.storage.box_capacity == 20U,
          "campaign initializes ROM-derived money and Pokemon storage");
    const std::size_t original_party_size =
        campaign.party.members.size();
    check(state, original_party_size != 0U,
          "Magikarp sale fixture owns a starter");
    while (campaign.party.members.size() <
           programs.party_capacity)
        campaign.party.members.push_back(
            campaign.party.members.front());
    std::vector<pokered::PokemonState>& current_box =
        campaign.storage.boxes[
            campaign.storage.current_box];
    current_box.assign(
        programs.storage_box_capacity,
        campaign.party.members.front());

    world.dialogue = {};
    world.choice = {};
    world.last_actor_activation = {
        .map_id = 68U,
        .actor_index = 4U,
        .occurred = true,
    };
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error) &&
              world.choice.open &&
              world.dialogue.open &&
              world.dialogue.pages.front().find(
                  "Have I got a deal") != std::string::npos,
          "Mt. Moon salesman opens the imported offer");
    for (std::size_t guard = 0U;
         guard < 1000U && campaign.fiber.active; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate =
                 world.dialogue.open ||
                 world.choice.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "full-box Magikarp sale advances");
        if (!error.empty()) break;
    }
    check(state,
          error.empty() &&
              campaign.money == 3000U &&
              !pokered::campaign_flag(
                  campaign, 0x6BE37U) &&
              current_box.size() == 20U,
          "full current box refuses Magikarp without charging");

    current_box.clear();
    world.dialogue = {};
    world.choice = {};
    world.last_actor_activation = {
        .map_id = 68U,
        .actor_index = 4U,
        .occurred = true,
    };
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error),
          "Mt. Moon salesman permits a retry after storage failure");
    for (std::size_t guard = 0U;
         guard < 1000U && campaign.fiber.active; ++guard) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate =
                 world.dialogue.open ||
                 world.choice.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "successful Magikarp sale advances");
        if (!error.empty()) break;
    }
    check(state,
          error.empty() &&
              campaign.money == 2500U &&
              pokered::campaign_flag(
                  campaign, 0x6BE37U) &&
              current_box.size() == 1U &&
              current_box.front().species_dex == 129U &&
              current_box.front().level == 5U,
          "sale sends imported level-5 Magikarp to the current box and charges 500");
    campaign.party.members.resize(original_party_size);

    world.dialogue = {};
    world.last_actor_activation = {
        .map_id = 68U,
        .actor_index = 4U,
        .occurred = true,
    };
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error) &&
              world.dialogue.open &&
              world.dialogue.pages.front().find(
                  "don't") != std::string::npos,
          "repeat salesman interaction uses imported no-refunds branch");
    while (campaign.fiber.active) {
        pokered::step_world(
            world, interactions, campaign,
            {.activate = world.dialogue.open});
        check(state,
              pokered::service_campaign_programs(
                  programs, rules, battle_rules, world,
                  campaign, error),
              "Magikarp no-refunds branch completes");
        if (!error.empty()) break;
    }

    world.dialogue = {};
    world.last_actor_activation = {
        .map_id = 54U,
        .actor_index = 1U,
        .occurred = true,
    };
    check(state,
          pokered::service_campaign_programs(
              programs, rules, battle_rules, world, campaign,
              error) &&
              world.dialogue.open &&
              world.dialogue.pages.front().find(
                  "kinds of trainers") != std::string::npos,
          "Brock switches to imported post-reward advice");
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
    test_local_pallet_campaign_program(state);
    if (state.failures == 0) std::puts("foundation tests passed");
    return state.failures == 0 ? 0 : 1;
}
