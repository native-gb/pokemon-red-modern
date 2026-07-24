#include "audio.hpp"
#include "battle_animation_lab.hpp"
#include "battle_controller.hpp"
#include "battle_rules.hpp"
#include "boot.hpp"
#include "campaign_programs.hpp"
#include "catalog.hpp"
#include "clocks.hpp"
#include "controls.hpp"
#include "dev_input.hpp"
#include "encounters.hpp"
#include "interactions.hpp"
#include "maps.hpp"
#include "render/boot.hpp"
#include "render/field_menu.hpp"
#include "render/frame.hpp"
#include "render/maps.hpp"
#include "render/naming.hpp"
#include "rules.hpp"
#include "save.hpp"
#include "settings.hpp"
#include "src/imgui_layer.hpp"
#include "state.hpp"
#include "tools.hpp"
#include "trainers.hpp"
#include "window.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace {

struct LaunchOptions {
    bool help{};
    bool render_smoke{};
    bool developer_tools{};
    std::filesystem::path input_socket;
};

bool parse_options(int argc, char** argv, LaunchOptions& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument == "--help" || argument == "-h")
            options.help = true;
        else if (argument == "--render-smoke")
            options.render_smoke = true;
        else if (argument == "--tools")
            options.developer_tools = true;
        else if (argument == "--input-socket" && i + 1 < argc)
            options.input_socket = argv[++i];
        else
            return false;
    }
    return true;
}

void print_usage(const char* executable) {
    std::printf(
        "Usage: %s [--render-smoke] [--tools] [--input-socket PATH]\n",
        executable);
}

std::optional<std::uint8_t> imported_sound_id(
    const pokered::AnimationCue& cue) {
    constexpr std::string_view prefix = "imported_sound_";
    if (cue.kind != pokered::AnimationCueKind::signal ||
        !std::string_view(cue.signal.text).starts_with(prefix))
        return std::nullopt;
    const std::string_view number =
        std::string_view(cue.signal.text).substr(prefix.size());
    unsigned value = 0U;
    const auto parsed =
        std::from_chars(number.data(), number.data() + number.size(), value);
    if (parsed.ec != std::errc{} ||
        parsed.ptr != number.data() + number.size() ||
        value > 0xFFU)
        return std::nullopt;
    return static_cast<std::uint8_t>(value);
}

void play_battle_animation_cues(
    pokered::AudioSystem& audio,
    const pokered::AnimationState& animation) {
    for (const pokered::AnimationCue& cue : animation.cues) {
        const std::optional<std::uint8_t> sound =
            imported_sound_id(cue);
        if (!sound.has_value()) continue;
        std::string ignored;
        (void)audio.play_sound(
            audio.preferred_audio_bank(), *sound, ignored);
    }
}

void play_active_battle_cry(
    pokered::AudioSystem& audio, const pokered::RuleCatalog& rules,
    const pokered::CampaignState& campaign, bool enemy) {
    if (!campaign.battle.active) return;
    const pokered::PartyState& party =
        enemy ? campaign.battle.enemy_party : campaign.party;
    const std::size_t active_index =
        enemy ? campaign.battle.enemy.active_index
              : campaign.battle.player.active_index;
    if (active_index >= party.members.size()) return;
    const pokered::SpeciesRule* species =
        pokered::find_species(
            rules, party.members[active_index].species_dex);
    if (species == nullptr) return;
    std::string ignored;
    (void)audio.play_cry(species->internal_id, ignored);
}

} // namespace

int main(int argc, char** argv) {
    LaunchOptions options;
    if (!parse_options(argc, argv, options)) {
        print_usage(argv[0]);
        return 2;
    }
    if (options.help) {
        print_usage(argv[0]);
        return 0;
    }

    const std::filesystem::path data_root =
        std::filesystem::path(POKERED_MODERN_SOURCE_DIR) / "data" / "runtime";
    std::error_code directory_error;
    std::filesystem::create_directories(data_root, directory_error);
    if (directory_error) {
        std::fprintf(stderr, "could not create runtime data directory: %s\n",
                     directory_error.message().c_str());
        return 1;
    }

    // Host choices load before platform initialization because the selected
    // control profile must be active for the first input frame.
    const std::filesystem::path settings_path = data_root / "settings.cfg";
    const std::filesystem::path save_path =
        data_root / "saves" / "pokemon_red_modern.sexpr";
    pokered::PresentationSettings presentation;
    std::string settings_error;
    bool settings_writable = pokered::load_settings(settings_path, presentation, settings_error);
    if (!settings_writable)
        std::fprintf(stderr, "could not load settings: %s\n", settings_error.c_str());
    pokered::PresentationSettings saved_presentation = presentation;

    pokered::content::CatalogSummary catalog;
    pokered::GameState game;
    pokered::CampaignState campaign;
    pokered::BootContent boot_content;
    pokered::BootState boot;
    std::string boot_error;
    const std::filesystem::path boot_cache =
        data_root / "imports" / "pokemon_red_us_rev_0" / "compiled" / "boot_content.bin";
    if (!pokered::load_boot_content(boot_cache, boot_content, boot_error))
        std::fprintf(stderr, "%s\n", boot_error.c_str());

    pokered::BattleAnimationLab animation_lab;
    pokered::Diagnostics animation_diagnostics;
    const std::filesystem::path generated_animation_root =
        data_root / "imports" / "pokemon_red_us_rev_0" / "source" / "animations" / "battle_moves";
    if (!pokered::load_battle_animation_lab(generated_animation_root, animation_lab,
                                            animation_diagnostics)) {
        for (const pokered::Diagnostic& diagnostic : animation_diagnostics.entries)
            std::fprintf(stderr, "%s\n", pokered::format_diagnostic(diagnostic).c_str());
    } else {
        std::printf("Loaded %zu battle animation lab programs from %s\n",
                    animation_lab.entries.size(), generated_animation_root.c_str());
    }

    pokered::WorldState world;
    pokered::EncounterCatalog encounters;
    pokered::TrainerCatalog trainers;
    pokered::RuleCatalog rules;
    pokered::BattleRuleCatalog battle_rules;
    pokered::InteractionCatalog interactions;
    pokered::CampaignProgramCatalog campaign_programs;
    std::string map_error;
    const std::filesystem::path map_cache =
        data_root / "imports" / "pokemon_red_us_rev_0" / "compiled" / "world_maps.bin";
    if (!pokered::load_world(map_cache, world, map_error))
        std::fprintf(stderr, "%s\n", map_error.c_str());
    const std::filesystem::path interaction_cache =
        data_root / "imports" / "pokemon_red_us_rev_0" / "compiled" / "world_interactions.bin";
    std::string interaction_error;
    if (!pokered::load_interactions(interaction_cache, interactions, interaction_error))
        std::fprintf(stderr, "%s\n", interaction_error.c_str());
    const std::filesystem::path campaign_program_cache =
        data_root / "imports" / "pokemon_red_us_rev_0" / "compiled" /
        "campaign_programs.bin";
    std::string campaign_program_error;
    if (!pokered::load_campaign_programs(
            campaign_program_cache, campaign_programs,
            campaign_program_error))
        std::fprintf(stderr, "%s\n", campaign_program_error.c_str());
    const std::filesystem::path rule_cache =
        data_root / "imports" / "pokemon_red_us_rev_0" / "compiled" / "pokemon_rules.bin";
    std::string rule_error;
    if (!pokered::load_rules(rule_cache, rules, rule_error))
        std::fprintf(stderr, "%s\n", rule_error.c_str());
    const std::filesystem::path battle_rule_cache =
        data_root / "imports" / "pokemon_red_us_rev_0" / "compiled" /
        "battle_rules.bin";
    std::string battle_rule_error;
    if (!pokered::load_battle_rules(battle_rule_cache, battle_rules,
                                    battle_rule_error))
        std::fprintf(stderr, "%s\n", battle_rule_error.c_str());
    const std::filesystem::path encounter_cache =
        data_root / "imports" / "pokemon_red_us_rev_0" / "compiled" /
        "encounters.bin";
    std::string encounter_error;
    if (!pokered::load_encounters(encounter_cache, encounters,
                                  encounter_error))
        std::fprintf(stderr, "%s\n", encounter_error.c_str());
    const std::filesystem::path trainer_cache =
        data_root / "imports" / "pokemon_red_us_rev_0" / "compiled" /
        "trainers.bin";
    std::string trainer_error;
    if (!pokered::load_trainers(trainer_cache, trainers, trainer_error))
        std::fprintf(stderr, "%s\n", trainer_error.c_str());
    if (world.loaded && interactions.loaded &&
        !pokered::initialize_world_runtime(world, interactions, interaction_error))
        std::fprintf(stderr, "%s\n", interaction_error.c_str());
    if (world.loaded && campaign_programs.loaded &&
        !pokered::initialize_campaign_program_runtime(
            campaign_programs, world, campaign_program_error))
        std::fprintf(stderr, "%s\n", campaign_program_error.c_str());
    if (world.loaded && interactions.loaded && rules.loaded &&
        encounters.loaded && trainers.loaded &&
        battle_rules.loaded && boot_content.loaded &&
        campaign_programs.loaded) {
        catalog.state = pokered::content::PackState::partial;
        catalog.campaign = "Pokemon Red";
        catalog.source = "Compiled local campaign pack";
        catalog.maps = world.maps.size();
        catalog.scripts = interactions.maps.size();
        catalog.species = rules.species.size();
        catalog.moves = rules.moves.size();
    }

    pokered::HostWindow window;
    if (!pokered::initialize_window(window, data_root, presentation.control_profile)) return 1;
    pokered::AudioSystem audio;
    const std::filesystem::path audio_cache =
        data_root / "imports" / "pokemon_red_us_rev_0" / "compiled" /
        "audio_content.bin";
    std::string audio_error;
    if (!audio.initialize(audio_cache, audio_error))
        std::fprintf(stderr, "audio unavailable: %s\n",
                     audio_error.c_str());
    pokered::DevInputSocket dev_input;
    if (!options.input_socket.empty()) {
        std::string input_error;
        if (!dev_input.open(options.input_socket, input_error)) {
            std::fprintf(stderr, "%s\n", input_error.c_str());
            pokered::shutdown_window(window);
            return 1;
        }
        std::printf("Developer input socket: %s\n",
                    options.input_socket.c_str());
    }
    pokered::render::BootRenderResources boot_resources;
    if (boot_content.loaded &&
        !pokered::render::upload_boot_textures(window.frame.renderer, boot_content,
                                               boot_resources)) {
        std::fprintf(stderr, "could not upload imported boot textures: %s\n", SDL_GetError());
        boot_content.loaded = false;
    }
    pokered::render::WorldRenderResources world_resources;
    if (world.loaded) {
        if (!pokered::render::upload_world_textures(window.frame.renderer, world,
                                                    world_resources)) {
            std::fprintf(stderr, "could not upload imported map textures: %s\n", SDL_GetError());
            world.loaded = false;
        } else {
            std::printf("World renderer: %zu chunks, %zu texture pages, %zu animated tiles\n",
                        world_resources.terrain_chunks.size(), world_resources.terrain_pages.size(),
                        world_resources.animated_tiles.size());
        }
    }
    if (boot_content.loaded) {
        if (!pokered::begin_boot(boot_content, boot, boot_error)) {
            std::fprintf(stderr, "%s\n", boot_error.c_str());
            pokered::render::destroy_boot_textures(boot_resources);
            pokered::render::destroy_world_textures(world_resources);
            pokered::shutdown_window(window);
            return 1;
        }
        boot.continue_available =
            pokered::save_game_exists(save_path);
        game.mode = pokered::Mode::title;
    } else if (world.loaded) {
        game.mode = pokered::Mode::overworld;
    }

    pokered::ToolState tools{
        .layout =
            options.developer_tools ? pokered::ToolLayout::developer : pokered::ToolLayout::closed,
        .arrange = options.developer_tools,
        .control_status = {},
    };
    const char* renderer_name = SDL_GetRendererName(window.frame.renderer);
    std::printf("SDL renderer: %s\n", renderer_name != nullptr ? renderer_name : "unknown");
    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();
    double accumulator = 0.0;
    double audio_accumulator = 0.0;
    constexpr double step_seconds = 1.0 / 60.0;
    constexpr double audio_step_seconds = 1.0 / 59.7275;
    int rendered_frames = 0;
    bool running = true;
    bool render_failed = false;
    bool pending_world_activation = false;
    bool pending_world_erase = false;
    bool pending_world_submit = false;
    bool pending_world_toggle_case = false;
    bool pending_world_start = false;
    bool pending_world_back = false;
    std::string pending_world_text;
    std::string pending_boot_text;
    pokered::ControlButtons previous_controls;
    pokered::GameClocks clocks;
    bool fast_forward_toggle_active = false;
    bool tools_chord_down = false;
    pokered::BootInput pending_boot_input;
    std::uint64_t last_audio_warp_tick =
        std::numeric_limits<std::uint64_t>::max();
    std::uint64_t last_audio_ledge_hop =
        world.ledge_hop_count;

    while (running) {
        const std::uint64_t frame_started = SDL_GetTicksNS();
        if (presentation.vsync != window.vsync &&
            !pokered::apply_window_vsync(window, presentation.vsync)) {
            std::fprintf(stderr, "could not change VSync: %s\n", SDL_GetError());
            presentation.vsync = window.vsync;
        }
        const auto now = Clock::now();
        const double elapsed = std::chrono::duration<double>(now - previous).count();
        previous = now;
        const double bounded_elapsed = std::clamp(elapsed, 0.0, 0.1);
        const float frame_seconds = static_cast<float>(bounded_elapsed);
        pokered::advance_unscaled_clocks(clocks, elapsed);

        const bool naming_input_active =
            (game.mode == pokered::Mode::title &&
             boot.screen == pokered::BootScreen::naming) ||
            (game.mode == pokered::Mode::overworld &&
             world.naming.open);
        if (!pokered::set_window_text_input(window, false))
            std::fprintf(stderr, "could not change text input: %s\n",
                         SDL_GetError());
        const pokered::WindowInput input = pokered::poll_window_events(window);
        const pokered::DevInputFrame injected = dev_input.poll();
        if (input.quit || injected.quit) break;
        pokered::update_window(window, bounded_elapsed);
        pokered::WindowInput host_input = input;
        host_input.toggle_player_tools |= injected.toggle_player_tools;
        host_input.toggle_developer_tools |=
            injected.toggle_developer_tools;
        host_input.toggle_world_annotations |=
            injected.toggle_world_annotations;
        host_input.erase_text |= injected.erase_text;
        host_input.submit_text |= injected.submit_text;
        host_input.text += injected.text;
        const bool tools_were_open =
            tools.layout != pokered::ToolLayout::closed;
        pokered::apply_tool_shortcuts(tools, host_input);

        // Semantic controls are shared by keyboard and every assigned
        // controller. Menu and Start+Select provide controller-only tool access.
        pokered::ControlButtons controls = pokered::read_controls(window.runtime);
        controls.left |= input.move_player_left;
        controls.right |= input.move_player_right;
        controls.up |= input.move_player_up;
        controls.down |= input.move_player_down;
        controls.confirm |= input.confirm_pressed;
        controls.back |= input.back_pressed;
        controls.start |= input.start_pressed;
        controls.select |= input.select_pressed;
        controls.left |= injected.controls.left;
        controls.right |= injected.controls.right;
        controls.up |= injected.controls.up;
        controls.down |= injected.controls.down;
        controls.confirm |= injected.controls.confirm;
        controls.back |= injected.controls.back;
        controls.start |= injected.controls.start;
        controls.select |= injected.controls.select;
        controls.menu |= injected.controls.menu;
        controls.quit |= injected.controls.quit;
        controls.fast_forward |= injected.controls.fast_forward;
        if (naming_input_active) {
            controls.left =
                input.naming_left ||
                (controls.left && !input.keyboard_wasd_left) ||
                injected.controls.left;
            controls.right =
                input.naming_right ||
                (controls.right && !input.keyboard_wasd_right) ||
                injected.controls.right;
            controls.up =
                input.naming_up ||
                (controls.up && !input.keyboard_wasd_up) ||
                injected.controls.up;
            controls.down =
                input.naming_down ||
                (controls.down && !input.keyboard_wasd_down) ||
                injected.controls.down;
            controls.select = false;
            host_input.text.clear();
        }
        const bool menu_pressed = controls.menu && !previous_controls.menu;
        const bool tools_chord = controls.start && controls.select;
        const bool tools_chord_pressed = tools_chord && !tools_chord_down;
        if (menu_pressed || tools_chord_pressed) {
            tools.layout = tools.layout == pokered::ToolLayout::player
                               ? pokered::ToolLayout::closed
                               : pokered::ToolLayout::player;
            tools.arrange = tools.layout != pokered::ToolLayout::closed;
        }
        tools_chord_down = tools_chord;
        const bool tools_own_input =
            tools_were_open ||
            tools.layout != pokered::ToolLayout::closed ||
            menu_pressed || tools_chord_pressed;
        const pokered::ControlButtons sampled_controls = controls;
        if (!tools_own_input && controls.quit) break;
        if (tools_own_input) controls = {};
        const bool confirm_pressed = controls.confirm && !previous_controls.confirm;
        const bool back_pressed =
            controls.back && !previous_controls.back;
        const bool direction_pressed =
            (controls.left && !previous_controls.left) ||
            (controls.right && !previous_controls.right) ||
            (controls.up && !previous_controls.up) ||
            (controls.down && !previous_controls.down);
        const bool start_pressed =
            controls.start && !previous_controls.start;
        const bool world_ui_active =
            world.dialogue.open || world.choice.open ||
            world.naming.open || world.menu.open ||
            world.service.active;
        const bool button_ui_active =
            game.mode == pokered::Mode::title ||
            game.mode == pokered::Mode::battle ||
            (game.mode == pokered::Mode::overworld &&
             world_ui_active);
        if (!tools_own_input && audio.available()) {
            if (game.mode == pokered::Mode::overworld &&
                start_pressed && !world_ui_active)
                audio.play_menu_open();
            else if (button_ui_active &&
                     (confirm_pressed || back_pressed ||
                      direction_pressed || start_pressed))
                audio.play_menu_press();
        }
        if (!tools_own_input &&
            game.mode == pokered::Mode::overworld) {
            pending_world_activation |= confirm_pressed;
            pending_world_erase |=
                host_input.erase_text ||
                back_pressed;
            pending_world_submit |=
                (!naming_input_active && host_input.submit_text) ||
                start_pressed;
            pending_world_toggle_case |=
                !naming_input_active &&
                controls.select && !previous_controls.select;
            pending_world_start |=
                start_pressed;
            pending_world_back |=
                back_pressed;
            if (!naming_input_active)
                pending_world_text += host_input.text;
        }
        if (!tools_own_input && game.mode == pokered::Mode::title) {
            pending_boot_input.up_pressed |= controls.up && !previous_controls.up;
            pending_boot_input.down_pressed |= controls.down && !previous_controls.down;
            pending_boot_input.left_pressed |= controls.left && !previous_controls.left;
            pending_boot_input.right_pressed |= controls.right && !previous_controls.right;
            pending_boot_input.confirm_pressed |= confirm_pressed;
            pending_boot_input.cancel_pressed |=
                back_pressed;
            pending_boot_input.start_pressed |=
                start_pressed;
            pending_boot_input.select_pressed |=
                controls.select && !previous_controls.select;
            pending_boot_input.erase_pressed |= host_input.erase_text;
            pending_boot_input.submit_pressed |= host_input.submit_text;
            if (!naming_input_active)
                pending_boot_text += host_input.text;
            pending_boot_input.random =
                static_cast<std::uint8_t>((game.step * 73U + 41U) & 0xFFU);
        }

        // Fast-forward scales deterministic game work only. Real,
        // presentation, audio, and future music clocks remain wall-clock based.
        const bool fast_forward_pressed = controls.fast_forward && !previous_controls.fast_forward;
        if (!presentation.fast_forward_enabled) {
            fast_forward_toggle_active = false;
        } else if (presentation.fast_forward_toggle && !tools_own_input && fast_forward_pressed) {
            fast_forward_toggle_active = !fast_forward_toggle_active;
        }
        const bool fast_forward =
            presentation.fast_forward_enabled && !tools_own_input &&
            (presentation.fast_forward_toggle ? fast_forward_toggle_active : controls.fast_forward);
        clocks.fast_forward = fast_forward;

        if (!tools_own_input &&
            host_input.toggle_lab_view && world.loaded && animation_lab.loaded &&
            (game.mode == pokered::Mode::overworld ||
             game.mode == pokered::Mode::battle_lab))
            game.mode =
                game.mode == pokered::Mode::battle_lab
                    ? pokered::Mode::overworld
                    : pokered::Mode::battle_lab;
        if (!tools_own_input && host_input.previous_animation) {
            if (game.mode == pokered::Mode::overworld)
                pokered::select_previous_map(world);
            else
                pokered::previous_battle_animation_lab(animation_lab);
        }
        if (!tools_own_input && host_input.next_animation) {
            if (game.mode == pokered::Mode::overworld)
                pokered::select_next_map(world);
            else
                pokered::next_battle_animation_lab(animation_lab);
        }
        if (!tools_own_input &&
            game.mode == pokered::Mode::overworld) {
            if (host_input.toggle_world_view) pokered::toggle_world_view(world);
            if (host_input.toggle_world_annotations)
                world.show_annotations = !world.show_annotations;
            constexpr float zoom_speed = 2.0F;
            if (input.zoom_world_in)
                pokered::zoom_world_view(world, std::exp(zoom_speed * frame_seconds));
            if (input.zoom_world_out)
                pokered::zoom_world_view(world, std::exp(-zoom_speed * frame_seconds));
            if (input.zoom_world_steps != 0.0F)
                pokered::zoom_world_view(
                    world, std::pow(1.18F, input.zoom_world_steps));
            if (std::fabs(controls.camera_zoom) > 0.01F)
                pokered::zoom_world_view(
                    world,
                    std::exp(
                        -zoom_speed * controls.camera_zoom *
                        frame_seconds));
            const bool select_pressed =
                controls.select && !previous_controls.select;
            if (input.reset_world_view ||
                (!tools_chord && select_pressed))
                pokered::reset_world_view(world);
            const float pan_speed = world.view == pokered::WorldView::world ? 3000.0F : 180.0F;
            const float pan_step = pan_speed * frame_seconds;
            if (input.pan_world_left) pokered::pan_world_view(world, -pan_step, 0.0F);
            if (input.pan_world_right) pokered::pan_world_view(world, pan_step, 0.0F);
            if (input.pan_world_up) pokered::pan_world_view(world, 0.0F, -pan_step);
            if (input.pan_world_down) pokered::pan_world_view(world, 0.0F, pan_step);
        }
        if (!tools_own_input &&
            game.mode == pokered::Mode::battle_lab && input.previous_species)
            pokered::previous_battle_species(animation_lab);
        if (!tools_own_input &&
            game.mode == pokered::Mode::battle_lab && input.next_species)
            pokered::next_battle_species(animation_lab);
        if (!tools_own_input &&
            game.mode == pokered::Mode::battle_lab &&
            input.cycle_battle_ui)
            pokered::cycle_battle_ui_mode(animation_lab);
        if (!tools_own_input &&
            game.mode == pokered::Mode::battle_lab &&
            input.previous_battle_ui_selection)
            pokered::previous_battle_ui_menu_selection(animation_lab);
        if (!tools_own_input &&
            game.mode == pokered::Mode::battle_lab &&
            input.next_battle_ui_selection)
            pokered::next_battle_ui_menu_selection(animation_lab);
        if (!tools_own_input &&
            game.mode == pokered::Mode::battle_lab && input.cycle_battle_status)
            pokered::cycle_battle_ui_status(animation_lab);
        if (!tools_own_input && input.restart_animation)
            pokered::restart_battle_animation_lab(animation_lab);
        if (!tools_own_input && input.reload_animation_sources) {
            pokered::Diagnostics reload_diagnostics;
            if (!pokered::reload_battle_animation_lab(animation_lab, reload_diagnostics)) {
                for (const pokered::Diagnostic& diagnostic : reload_diagnostics.entries)
                    std::fprintf(stderr, "%s\n", pokered::format_diagnostic(diagnostic).c_str());
            }
        }
        if (!tools_own_input &&
            input.toggle_animation_auto_advance)
            animation_lab.auto_advance = !animation_lab.auto_advance;

        // Real battle controls dispatch through the owned battle controller.
        // The developer animation lab remains an independent game mode.
        if (!tools_own_input && game.mode == pokered::Mode::battle &&
            campaign.battle.active) {
            pokered::BattleControlResult battle_result;
            std::string control_error;
            if (!pokered::control_battle(
                    rules, battle_rules, campaign_programs, campaign,
                    animation_lab,
                    {
                        .left =
                            controls.left && !previous_controls.left,
                        .right =
                            controls.right && !previous_controls.right,
                        .up = controls.up && !previous_controls.up,
                        .down =
                            controls.down && !previous_controls.down,
                        .confirm = confirm_pressed,
                        .back = controls.back && !previous_controls.back,
                    },
                    battle_result, control_error)) {
                std::fprintf(stderr, "%s\n", control_error.c_str());
            }
            if (battle_result.finished) {
                pokered::finish_world_actor_battle(
                    interactions, world, campaign);
                pokered::begin_battle_exit_presentation(
                    animation_lab);
            }
        }

        const double game_elapsed =
            bounded_elapsed * (fast_forward ? presentation.fast_forward_multiplier : 1);
        accumulator = std::min(accumulator + game_elapsed, 1.0);
        while (accumulator >= step_seconds) {
            if (!game.paused) {
                pokered::step_game(game);
                pokered::step_campaign(campaign);
                if (game.mode == pokered::Mode::overworld)
                    pokered::step_world_animation(world);
                pokered::advance_game_clock(clocks, step_seconds);
            }
            if (!game.paused && !tools_own_input &&
                game.mode == pokered::Mode::title) {
                pokered::BootStepResult boot_result;
                pending_boot_input.text =
                    pending_boot_text.empty()
                        ? nullptr
                        : pending_boot_text.c_str();
                if (!pokered::step_boot(boot_content, pending_boot_input, boot,
                                        boot_result, boot_error)) {
                    std::fprintf(stderr, "%s\n", boot_error.c_str());
                    render_failed = true;
                    running = false;
                    break;
                }
                pending_boot_input = {};
                pending_boot_text.clear();
                if (boot_result.new_game_requested) {
                    if (!pokered::begin_new_campaign(
                            campaign, boot.player_name, boot.rival_name,
                            boot.option_values, boot_error) ||
                        !pokered::enter_world_at(
                            world, boot_content.new_game_map_id,
                            boot_content.new_game_x, boot_content.new_game_y,
                            boot_error,
                            boot_content.new_game_previous_map_id)) {
                        std::fprintf(stderr, "%s\n", boot_error.c_str());
                        render_failed = true;
                        running = false;
                        break;
                    }
                    game.mode = pokered::Mode::overworld;
                } else if (boot_result.continue_requested) {
                    if (!pokered::load_game(
                            save_path, campaign, world,
                            interactions,
                            boot_error)) {
                        std::fprintf(
                            stderr, "could not load save: %s\n",
                            boot_error.c_str());
                        boot.continue_available = false;
                        boot.menu_selection = 0U;
                    } else {
                        game.mode = pokered::Mode::overworld;
                    }
                }
            }
            if (!game.paused && !tools_own_input && game.mode == pokered::Mode::overworld) {
                pokered::step_world(world, interactions, campaign,
                                    {
                                        .left = controls.left,
                                        .right = controls.right,
                                        .up = controls.up,
                                        .down = controls.down,
                                        .activate = pending_world_activation,
                                        .erase = pending_world_erase,
                                        .submit = pending_world_submit,
                                        .toggle_case =
                                            pending_world_toggle_case,
                                        .start = pending_world_start,
                                        .back = pending_world_back,
                                        .text =
                                            pending_world_text.empty()
                                                ? nullptr
                                                : pending_world_text.c_str(),
                                    });
                if (world.menu.save_requested) {
                    world.menu.save_requested = false;
                    std::string save_error;
                    if (pokered::save_game(
                            save_path, campaign, world,
                            save_error)) {
                        pokered::open_world_dialogue(
                            world, campaign, {"Game saved."});
                    } else {
                        std::fprintf(
                            stderr, "could not save game: %s\n",
                            save_error.c_str());
                        pokered::open_world_dialogue(
                            world, campaign, {"Save failed."});
                    }
                }
                pending_world_activation = false;
                pending_world_erase = false;
                pending_world_submit = false;
                pending_world_toggle_case = false;
                pending_world_start = false;
                pending_world_back = false;
                pending_world_text.clear();
                std::string campaign_step_error;
                if (!pokered::service_campaign_programs(
                        campaign_programs, rules, battle_rules, world, campaign,
                        campaign_step_error)) {
                    std::fprintf(stderr, "%s\n",
                                 campaign_step_error.c_str());
                }
                bool campaign_battle_began = false;
                std::string campaign_battle_error;
                if (trainers.loaded &&
                    !pokered::begin_campaign_trainer_battle(
                        trainers, world, rules, battle_rules, campaign,
                        animation_lab, campaign_battle_began,
                        campaign_battle_error)) {
                    std::fprintf(stderr, "%s\n",
                                 campaign_battle_error.c_str());
                }
                bool actor_battle_began = false;
                std::string actor_battle_error;
                if (!campaign_battle_began && trainers.loaded &&
                    !pokered::service_world_actor_battle(
                        interactions, trainers, world, rules,
                        battle_rules, campaign, animation_lab,
                        actor_battle_began, actor_battle_error)) {
                    std::fprintf(stderr, "%s\n",
                                 actor_battle_error.c_str());
                }
                bool battle_began = false;
                std::string wild_error;
                if (!campaign_battle_began && !actor_battle_began &&
                    encounters.loaded &&
                    !pokered::begin_world_wild_battle(
                        encounters, campaign_programs, world, rules,
                        battle_rules, campaign, animation_lab,
                        battle_began, wild_error)) {
                    std::fprintf(stderr, "%s\n", wild_error.c_str());
                }
                if (campaign_battle_began || actor_battle_began ||
                    battle_began)
                    game.mode = pokered::Mode::battle;
            }
            if (!game.paused && game.mode == pokered::Mode::battle_lab)
                pokered::step_battle_animation_lab(animation_lab);
            if (!game.paused && game.mode == pokered::Mode::battle)
                pokered::step_gameplay_battle_animations(animation_lab);
            if (!game.paused && game.mode == pokered::Mode::battle) {
                play_battle_animation_cues(
                    audio, animation_lab.animation);
                if (animation_lab.presentation.phase ==
                        pokered::BattlePresentationPhase::
                            opponent_arrival &&
                    animation_lab.presentation.tick == 0U)
                    play_active_battle_cry(
                        audio, rules, campaign, true);
                else if (animation_lab.presentation.phase ==
                             pokered::BattlePresentationPhase::
                                 player_deployment &&
                         animation_lab.presentation.tick == 0U)
                    play_active_battle_cry(
                        audio, rules, campaign, false);
            }
            if (game.mode == pokered::Mode::battle &&
                pokered::consume_battle_return_to_world(
                    animation_lab))
                game.mode = pokered::Mode::overworld;
            accumulator -= step_seconds;
        }

        // Music and effects use an unscaled wall-clock cadence. Fast-forward
        // accelerates simulation, never pitch or playback speed.
        if (audio.available()) {
            std::string dispatch_error;
            if (world.last_warp.occurred &&
                world.last_warp.simulation_tick !=
                    last_audio_warp_tick) {
                const auto source = std::ranges::find_if(
                    world.maps,
                    [&](const pokered::WorldMap& map) {
                        return map.id ==
                               world.last_warp.source_map_id;
                    });
                const bool going_inside =
                    source != world.maps.end() &&
                    source->tileset_id == 0U;
                audio.play_map_transition(
                    world.last_warp.source_map_id,
                    going_inside);
                last_audio_warp_tick =
                    world.last_warp.simulation_tick;
            }
            if (world.ledge_hop_count !=
                    last_audio_ledge_hop &&
                world.player.map_index < world.maps.size()) {
                audio.play_ledge(
                    world.maps[world.player.map_index].id);
                last_audio_ledge_hop = world.ledge_hop_count;
            }
            if (game.mode == pokered::Mode::title) {
                const bool oak_intro =
                    boot.screen == pokered::BootScreen::oak_text ||
                    boot.screen == pokered::BootScreen::name_menu ||
                    boot.screen == pokered::BootScreen::naming ||
                    boot.screen == pokered::BootScreen::ending;
                (void)audio.request_scene_music(
                    oak_intro ? pokered::AudioScene::oak_intro
                              : pokered::AudioScene::title,
                    dispatch_error);
            } else if (game.mode == pokered::Mode::overworld &&
                world.player.initialized &&
                world.player.map_index < world.maps.size()) {
                (void)audio.request_map_music(
                    world.maps[world.player.map_index].id,
                    dispatch_error);
            } else if (game.mode == pokered::Mode::battle) {
                (void)audio.request_battle_music(
                    animation_lab.presentation.trainer_battle,
                    dispatch_error);
            }
            audio_accumulator =
                std::min(audio_accumulator + bounded_elapsed, 0.25);
            while (audio_accumulator >= audio_step_seconds) {
                audio.step();
                audio_accumulator -= audio_step_seconds;
            }
        }

        if (world.automatic_camera_framing !=
            presentation.automatic_camera_framing) {
            world.automatic_camera_framing =
                presentation.automatic_camera_framing;
            world.camera_region_dirty = true;
        }
        pokered::update_world_camera_region(
            world, window.frame.render_width,
            window.frame.render_height);
        pokered::update_world_view(world, game_elapsed);
        pokered::update_world_presentation(world, bounded_elapsed);
        imgui_new_frame();
        pokered::apply_nearest_sampling(window);
        if (!pokered::render::render_frame(window.frame.renderer, window.frame.render_target,
                                           window.frame.render_width, window.frame.render_height,
                                           game, catalog, boot_content, boot, boot_resources,
                                           animation_lab, world, world_resources) ||
            !pokered::draw_window(window)) {
            std::fprintf(stderr, "could not render frame: %s\n", SDL_GetError());
            render_failed = true;
            break;
        }

        pokered::draw_tools(tools, window.runtime, game, catalog, boot, animation_lab,
                            world, rules, presentation, clocks,
                            renderer_name != nullptr ? renderer_name : "unknown");
        pokered::render::draw_field_menu_overlay(
            world, campaign, campaign_programs, rules);
        imgui_render_layer();
        pokered::apply_nearest_sampling(window);
        pokered::present_window(window);

        const int render_rate = pokered::effective_render_rate(presentation);
        const std::uint64_t target_frame_nanoseconds =
            std::uint64_t{1'000'000'000} / static_cast<std::uint64_t>(render_rate);
        const std::uint64_t frame_elapsed = SDL_GetTicksNS() - frame_started;
        if (frame_elapsed < target_frame_nanoseconds)
            SDL_DelayPrecise(target_frame_nanoseconds - frame_elapsed);

        ++rendered_frames;
        if (options.render_smoke && rendered_frames >= 3) running = false;

        // Persist settings at the point of change. Smoke checks remain
        // side-effect free, and a failed destination is not hammered per frame.
        if (!options.render_smoke && settings_writable && presentation != saved_presentation) {
            if (pokered::save_settings(settings_path, presentation, settings_error))
                saved_presentation = presentation;
            else {
                std::fprintf(stderr, "could not save settings: %s\n", settings_error.c_str());
                settings_writable = false;
            }
        }
        previous_controls = sampled_controls;
    }

    pokered::render::destroy_boot_textures(boot_resources);
    pokered::render::destroy_world_textures(world_resources);
    audio.shutdown();
    pokered::shutdown_window(window);
    if (!options.render_smoke && settings_writable && presentation != saved_presentation &&
        !pokered::save_settings(settings_path, presentation, settings_error))
        std::fprintf(stderr, "could not save final settings: %s\n", settings_error.c_str());
    return render_failed ? 1 : 0;
}
