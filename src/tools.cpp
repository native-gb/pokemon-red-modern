#include "tools.hpp"

#include "controls_menu.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace pokered {
namespace {

constexpr ImGuiWindowFlags kFixedTools =
    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

void place(float x, float y, float width, float height, bool arrange) {
    if (!arrange) return;
    ImGui::SetNextWindowPos({x, y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({width, height}, ImGuiCond_Always);
}

void draw_player_tools(ToolState& tools, GubsyRuntime& runtime,
                       const content::CatalogSummary& catalog, PresentationSettings& presentation) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    constexpr float margin = 16.0F;
    constexpr float top = 42.0F;
    constexpr float gap = 12.0F;
    const float width = std::max((display.x - margin * 2.0F - gap) * 0.5F, 1.0F);
    const float height = std::max(display.y - top - margin, 1.0F);

    place(margin, top, width, height, tools.arrange);
    if (ImGui::Begin("Campaign", nullptr, kFixedTools)) {
        ImGui::TextUnformatted("Pokemon Red Modern");
        ImGui::Separator();
        ImGui::Text("Content pack: %s", content::label(catalog.state));
        ImGui::TextWrapped("%.*s", static_cast<int>(catalog.source.size()), catalog.source.data());
        ImGui::Spacing();
        ImGui::Text("Maps: %zu", catalog.maps);
        ImGui::Text("Species: %zu", catalog.species);
        ImGui::Text("Moves: %zu", catalog.moves);
        ImGui::TextWrapped(
            "The local development pack now supplies world and immutable Pokemon rules. "
            "Campaign scripts, complete battle programs, UI, and audio remain in progress.");
    }
    ImGui::End();

    place(margin + width + gap, top, width, height, tools.arrange);
    if (ImGui::Begin("Player Settings", nullptr, kFixedTools)) {
        if (ImGui::BeginTabBar("PlayerSettingsTabs")) {
            if (ImGui::BeginTabItem("Display")) {
                ImGui::Checkbox("VSync", &presentation.vsync);
                ImGui::Checkbox("Motion interpolation", &presentation.motion_interpolation);
                ImGui::Checkbox("Show FPS overlay", &presentation.show_fps);
                constexpr std::array render_rates{60, 120, 144, 165, 240};
                int selected_rate =
                    static_cast<int>(std::find(render_rates.begin(), render_rates.end(),
                                               presentation.render_rate_limit) -
                                     render_rates.begin());
                if (selected_rate >= static_cast<int>(render_rates.size())) selected_rate = 2;
                if (ImGui::Combo("Frame limit", &selected_rate,
                                 "60 Hz\0"
                                 "120 Hz\0"
                                 "144 Hz\0"
                                 "165 Hz\0"
                                 "240 Hz\0"))
                    presentation.render_rate_limit =
                        render_rates[static_cast<std::size_t>(selected_rate)];
                ImGui::Separator();
                ImGui::BulletText("F11 toggles fullscreen");
                ImGui::BulletText("Game view keeps its native 10:9 aspect");
                ImGui::BulletText("GPU render target; no CPU framebuffer");
                ImGui::BulletText("World view supports arbitrary pan and zoom");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Speed")) {
                ImGui::Checkbox("Enable fast-forward", &presentation.fast_forward_enabled);
                ImGui::Checkbox("Toggle instead of hold", &presentation.fast_forward_toggle);
                ImGui::SliderInt("Game speed", &presentation.fast_forward_multiplier, 2, 8, "%dx");
                ImGui::TextWrapped(
                    "Left trigger or the bound fast-forward control accelerates simulation. "
                    "Presentation, audio, music, and real play time remain unscaled.");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Controls")) {
                draw_controls_panel(runtime, presentation, tools.control_status);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void draw_developer_tools(ToolState& tools, GameState& game, const content::CatalogSummary& catalog,
                          BattleAnimationLab& lab, WorldState& maps, const RuleCatalog& rules,
                          PresentationSettings& presentation, const GameClocks& clocks,
                          const char* renderer_name) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    constexpr float margin = 12.0F;
    constexpr float top = 42.0F;
    constexpr float gap = 8.0F;
    const float left = std::clamp(display.x * 0.25F, 280.0F, 360.0F);
    const float middle = std::clamp(display.x * 0.30F, 340.0F, 440.0F);
    const float right_x = margin + left + gap + middle + gap;
    const float right = std::max(display.x - right_x - margin, 1.0F);
    const float height = std::max(display.y - top - margin, 1.0F);

    place(margin, top, left, height, tools.arrange);
    if (ImGui::Begin("Runtime State", nullptr, kFixedTools)) {
        ImGui::Text("Mode: %s", label(game.mode));
        ImGui::Text("Simulation step: %llu", static_cast<unsigned long long>(game.step));
        ImGui::Text("Game time: %.3f s", clocks.game_time);
        ImGui::Text("Real time: %.3f s", clocks.real_time);
        ImGui::Text("Presentation: %.3f s", clocks.presentation_time);
        ImGui::Text("Audio: %.3f s", clocks.audio_time);
        ImGui::Text("Music: %.3f s", clocks.music_time);
        ImGui::Text("Fast-forward: %s", clocks.fast_forward ? "active" : "inactive");
        ImGui::Checkbox("Paused", &game.paused);
        ImGui::Separator();
        ImGui::Text("Renderer: %s", renderer_name);
        ImGui::TextUnformatted("Fixed simulation: 60 Hz");
        ImGui::Text("Render rate: %.1f FPS", static_cast<double>(ImGui::GetIO().Framerate));
        ImGui::Checkbox("Show FPS overlay", &presentation.show_fps);
        ImGui::Checkbox("Motion interpolation", &presentation.motion_interpolation);
        ImGui::Checkbox("VSync", &presentation.vsync);
        constexpr std::array render_rates{60, 120, 144, 165, 240};
        int selected_rate = static_cast<int>(
            std::find(render_rates.begin(), render_rates.end(), presentation.render_rate_limit) -
            render_rates.begin());
        if (selected_rate >= static_cast<int>(render_rates.size())) selected_rate = 2;
        if (ImGui::Combo("Frame limit", &selected_rate,
                         "60 Hz\0"
                         "120 Hz\0"
                         "144 Hz\0"
                         "165 Hz\0"
                         "240 Hz\0"))
            presentation.render_rate_limit = render_rates[static_cast<std::size_t>(selected_rate)];
        ImGui::Text("Effective hard cap: %d Hz", effective_render_rate(presentation));
        ImGui::TextDisabled("Simulation remains fixed-rate; rendering is independent.");
        ImGui::Separator();
        const WorldMap* map = selected_map(maps);
        ImGui::Text("Map: %.*s", static_cast<int>(selected_map_name(maps).size()),
                    selected_map_name(maps).data());
        ImGui::Text("Map record: %zu / %zu", maps.maps.empty() ? 0 : maps.current + 1,
                    maps.maps.size());
        ImGui::Text("View: %.*s", static_cast<int>(label(maps.view).size()),
                    label(maps.view).data());
        ImGui::Checkbox("World annotations (F3)", &maps.show_annotations);
        if (maps.player.initialized) {
            ImGui::Text("Player map index: %zu", maps.player.map_index);
            ImGui::Text("Player cell: %d, %d", maps.player.x, maps.player.y);
            const std::size_t resident_actors = static_cast<std::size_t>(std::count_if(
                maps.actors.begin(), maps.actors.end(), [&](const WorldActorState& actor) {
                    return actor.map_index < maps.maps.size() &&
                           maps.maps[actor.map_index].world_space == maps.current_space;
                }));
            ImGui::Text("Resident actors: %zu / %zu", resident_actors, maps.actors.size());
            ImGui::Text("Spatial map indexes: %zu", maps.spatial.size());
            ImGui::Text("Dialogue: %s", maps.dialogue.open ? "open" : "closed");
            if (maps.last_warp.occurred) {
                ImGui::Text("Last warp: %02X:%u -> %02X:%u at tick %llu",
                            static_cast<unsigned>(maps.last_warp.source_map_id),
                            static_cast<unsigned>(maps.last_warp.source_warp_index),
                            static_cast<unsigned>(maps.last_warp.destination_map_id),
                            static_cast<unsigned>(maps.last_warp.destination_warp_index),
                            static_cast<unsigned long long>(maps.last_warp.simulation_tick));
            }
        }
        if (map != nullptr) {
            ImGui::Text("ROM ID: 0x%02X", static_cast<unsigned>(map->id));
            ImGui::Text("Blocks: %u x %u", static_cast<unsigned>(map->width_blocks),
                        static_cast<unsigned>(map->height_blocks));
            ImGui::Text("Tiles: %u x %u", static_cast<unsigned>(map->width_tiles),
                        static_cast<unsigned>(map->height_tiles));
            ImGui::Text("Tileset: %u", static_cast<unsigned>(map->tileset_id));
            ImGui::Text("World origin: %d, %d tiles", map->global_x_tiles, map->global_y_tiles);
            const WorldSpace* space = current_world_space(maps);
            ImGui::Text("World space: %s [%u]", space == nullptr ? "none" : space->key.c_str(),
                        static_cast<unsigned>(map->world_space));
        }
        if (ImGui::Button("Previous Map")) select_previous_map(maps);
        ImGui::SameLine();
        if (ImGui::Button("Next Map")) select_next_map(maps);
        if (ImGui::Button(maps.view == WorldView::world ? "Show Selected Map"
                                                        : "Show Connected World"))
            toggle_world_view(maps);
        ImGui::Text("Current zoom: %.2fx", maps.zoom);
        ImGui::SliderFloat("Target zoom", &maps.target_zoom, 0.05F, 64.0F, "%.2fx",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::DragFloat2("Camera target", &maps.target_camera_x, 8.0F);
        if (ImGui::Button("Reset Camera")) reset_world_view(maps);
        if (maps.loaded && lab.loaded && ImGui::Button("Toggle Map / Battle"))
            game.mode = game.mode == Mode::overworld ? Mode::battle : Mode::overworld;
        ImGui::Separator();
        ImGui::Text("Animation: %.*s", static_cast<int>(battle_animation_lab_name(lab).size()),
                    battle_animation_lab_name(lab).data());
        ImGui::Text("Program: %zu / %zu", lab.entries.empty() ? 0 : lab.current + 1,
                    lab.entries.size());
        const std::string_view species = battle_animation_lab_species_name(lab);
        ImGui::Text("Pokemon: %.*s", static_cast<int>(species.size()), species.data());
        ImGui::Text("Species: %zu / %zu",
                    lab.imported_assets.pokemon.empty() ? 0 : lab.current_species + 1,
                    lab.imported_assets.pokemon.size());
        if (ImGui::Button("Previous Pokemon")) previous_battle_species(lab);
        ImGui::SameLine();
        if (ImGui::Button("Next Pokemon")) next_battle_species(lab);
        ImGui::Text("Battle UI: %.*s", static_cast<int>(label(lab.ui.mode).size()),
                    label(lab.ui.mode).data());
        ImGui::Text("Player status: %s", lab.ui.player.status.text.c_str());
        if (ImGui::Button("Cycle UI view")) cycle_battle_ui_mode(lab);
        if (ImGui::Button("Previous menu selection")) previous_battle_ui_menu_selection(lab);
        ImGui::SameLine();
        if (ImGui::Button("Next menu selection")) next_battle_ui_menu_selection(lab);
        if (ImGui::Button("Cycle player status")) cycle_battle_ui_status(lab);
        ImGui::Text("Tick: %u", lab.animation.tick);
        ImGui::Text("Effects: %zu", lab.animation.effects.size());
        ImGui::Checkbox("Auto advance", &lab.auto_advance);
    }
    ImGui::End();

    place(margin + left + gap, top, middle, height, tools.arrange);
    if (ImGui::Begin("Content Indexes", nullptr, kFixedTools)) {
        ImGui::Text("Pack: %s", content::label(catalog.state));
        ImGui::Separator();
        ImGui::Text("Imported maps    %zu", maps.maps.size());
        std::size_t actor_count = 0;
        std::size_t warp_count = 0;
        for (const WorldMap& map : maps.maps) {
            actor_count += map.actors.size();
            warp_count += map.warps.size();
        }
        ImGui::Text("Overworld sprites %zu", maps.sprites.size());
        ImGui::Text("Actor spawns     %zu", actor_count);
        ImGui::Text("Warps            %zu", warp_count);
        ImGui::Text("Scripts          %zu", catalog.scripts);
        ImGui::Text("Species          %zu", catalog.species);
        ImGui::Text("Moves            %zu", catalog.moves);
        ImGui::Text("Items            %zu", catalog.items);
        ImGui::Text("Types            %zu", rules.types.size());
        ImGui::Text("Type interactions %zu", rules.type_interactions.size());
        ImGui::Text("Learnset entries %zu", rules.learnsets.size());
        ImGui::Text("Evolutions       %zu", rules.evolutions.size());
        ImGui::Text("Growth curves    %zu", rules.growth_curves.size());
        ImGui::Text("Machines         %zu", rules.machines.size());
        ImGui::Separator();
        ImGui::Text("Pokemon fronts   %zu", lab.imported_assets.pokemon.size());
        ImGui::Text("Pokemon backs    %zu", lab.imported_assets.pokemon.size());
        ImGui::Text("Trainer portraits %zu", lab.imported_assets.trainers.size());
        ImGui::Text("Battle UI tiles  %zu", lab.imported_assets.battle_ui_tiles.size() / 64U);
        ImGui::Spacing();
        ImGui::TextWrapped("Every domain will be a typed index with stable IDs, "
                           "validation, and import provenance.");
    }
    ImGui::End();

    place(right_x, top, right, height, tools.arrange);
    if (ImGui::Begin("Importer and Executors", nullptr, kFixedTools)) {
        ImGui::TextUnformatted("Importer");
        ImGui::BulletText("ROM verification");
        ImGui::BulletText("Decode -> validate -> normalize -> cache");
        ImGui::BulletText("Per-record provenance");
        ImGui::Separator();
        ImGui::TextUnformatted("Runtime executors");
        ImGui::BulletText("World and interaction");
        ImGui::BulletText("Script coroutine VM");
        ImGui::BulletText("Battle and progression");
        ImGui::BulletText("Presentation timelines");
        ImGui::BulletText("Text, UI, audio, and persistence");
    }
    ImGui::End();
}

std::string_view movement_label(const WorldActorSpawn& actor) {
    if (actor.movement == 0xFFU) {
        if (actor.direction_or_axis == 0xD1U) return "stay/up";
        if (actor.direction_or_axis == 0xD2U) return "stay/left";
        if (actor.direction_or_axis == 0xD3U) return "stay/right";
        return "stay/down";
    }
    if (actor.movement == 0xFEU) {
        if (actor.direction_or_axis == 1U) return "walk/vertical";
        if (actor.direction_or_axis == 2U) return "walk/horizontal";
        return "walk/any";
    }
    return "scripted";
}

void draw_world_annotations(const WorldState& world) {
    if (!world.show_annotations || !world.loaded) return;
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const render::WorldProjection projection =
        render::world_projection(static_cast<int>(display.x), static_cast<int>(display.y), world);
    if (projection.scale <= 0.0F) return;

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const WorldMap* selected = selected_map(world);
    const auto label_text = [&](ImVec2 position, ImU32 color, const char* text) {
        const ImVec2 size = ImGui::CalcTextSize(text);
        draw->AddRectFilled({position.x - 2.0F, position.y - 1.0F},
                            {position.x + size.x + 2.0F, position.y + size.y + 1.0F},
                            IM_COL32(5, 9, 13, 210), 2.0F);
        draw->AddText(position, color, text);
    };
    const auto screen_point = [&](float world_x, float world_y) {
        return ImVec2{
            projection.center_x + (world_x - world.camera_x) * projection.scale,
            projection.center_y + (world_y - world.camera_y) * projection.scale,
        };
    };
    for (const WorldMap& map : world.maps) {
        if ((world.view == WorldView::selected && &map != selected) ||
            (world.view == WorldView::world && map.world_space != world.current_space))
            continue;
        const float map_x = static_cast<float>(map.global_x_tiles) * 8.0F;
        const float map_y = static_cast<float>(map.global_y_tiles) * 8.0F;
        const ImVec2 top_left = screen_point(map_x, map_y);
        const ImVec2 bottom_right =
            screen_point(map_x + static_cast<float>(map.width_tiles) * 8.0F,
                         map_y + static_cast<float>(map.height_tiles) * 8.0F);
        if (bottom_right.x < 0.0F || bottom_right.y < 20.0F || top_left.x > display.x ||
            top_left.y > display.y)
            continue;

        const bool selected_map = &map == selected;
        draw->AddRect(top_left, bottom_right,
                      selected_map ? IM_COL32(255, 224, 80, 235) : IM_COL32(92, 210, 255, 175),
                      0.0F, ImDrawFlags_None, selected_map ? 2.0F : 1.0F);
        const bool has_roaming_actors =
            std::any_of(map.actors.begin(), map.actors.end(), [](const WorldActorSpawn& actor) {
                return actor.movement_bounds.has_value();
            });
        if (has_roaming_actors && bottom_right.x - top_left.x > 8.0F &&
            bottom_right.y - top_left.y > 8.0F) {
            draw->AddRect({top_left.x + 3.0F, top_left.y + 3.0F},
                          {bottom_right.x - 3.0F, bottom_right.y - 3.0F},
                          IM_COL32(255, 154, 64, 210), 0.0F, ImDrawFlags_None, 1.5F);
        }
        if (bottom_right.x - top_left.x >= 72.0F || selected_map) {
            std::array<char, 96> map_text{};
            std::snprintf(map_text.data(), map_text.size(), "%s [0x%02X]", map.display_name.c_str(),
                          static_cast<unsigned>(map.id));
            label_text({top_left.x + 3.0F, top_left.y + 2.0F}, IM_COL32(255, 245, 190, 255),
                       map_text.data());
        }

        for (const WorldWarp& warp : map.warps) {
            const ImVec2 center = screen_point(map_x + static_cast<float>(warp.x) * 16.0F + 8.0F,
                                               map_y + static_cast<float>(warp.y) * 16.0F + 8.0F);
            const float radius = std::clamp(6.0F * projection.scale, 3.0F, 10.0F);
            draw->AddRectFilled({center.x - radius, center.y - radius},
                                {center.x + radius, center.y + radius}, IM_COL32(32, 224, 240, 95));
            draw->AddRect({center.x - radius, center.y - radius},
                          {center.x + radius, center.y + radius}, IM_COL32(48, 246, 255, 255), 0.0F,
                          ImDrawFlags_None, 2.0F);
            if (projection.scale >= 0.75F) {
                std::array<char, 64> warp_text{};
                std::snprintf(warp_text.data(), warp_text.size(), "W%u -> %02X:%u",
                              static_cast<unsigned>(warp.index),
                              static_cast<unsigned>(warp.destination_map_id),
                              static_cast<unsigned>(warp.destination_warp_index));
                label_text({center.x + radius + 2.0F, center.y - 7.0F}, IM_COL32(96, 255, 255, 255),
                           warp_text.data());
            }
        }

        for (std::size_t spawn_index = 0; spawn_index < map.actors.size(); ++spawn_index) {
            const WorldActorSpawn& actor = map.actors[spawn_index];
            const auto state = std::find_if(
                world.actors.begin(), world.actors.end(), [&](const WorldActorState& value) {
                    return value.map_index == static_cast<std::size_t>(&map - world.maps.data()) &&
                           value.spawn_index == spawn_index;
                });
            const float actor_x = state == world.actors.end()
                                      ? map_x + static_cast<float>(actor.x) * 16.0F
                                      : state->visual_global_x * 16.0F;
            const float actor_y = state == world.actors.end()
                                      ? map_y + static_cast<float>(actor.y) * 16.0F
                                      : state->visual_global_y * 16.0F;
            const ImVec2 center = screen_point(actor_x + 8.0F, actor_y + 8.0F);
            const ImU32 color = actor.kind == WorldActorKind::item ? IM_COL32(255, 206, 64, 255)
                                : actor.kind == WorldActorKind::trainer_or_pokemon
                                    ? IM_COL32(255, 100, 112, 255)
                                    : IM_COL32(142, 255, 138, 255);
            draw->AddCircleFilled(center, 3.5F, color);
            draw->AddLine({center.x - 7.0F, center.y}, {center.x + 7.0F, center.y}, color, 1.0F);
            draw->AddLine({center.x, center.y - 7.0F}, {center.x, center.y + 7.0F}, color, 1.0F);
            if (projection.scale >= 0.75F) {
                std::array<char, 128> actor_text{};
                const std::string_view movement = movement_label(actor);
                const WorldSprite* sprite = find_world_sprite(world, actor.sprite_id);
                const std::string_view sprite_key =
                    sprite == nullptr ? std::string_view{"unknown"} : std::string_view{sprite->key};
                std::snprintf(actor_text.data(), actor_text.size(), "A%u %.*s [S%u] %.*s%s",
                              static_cast<unsigned>(actor.index),
                              static_cast<int>(sprite_key.size()), sprite_key.data(),
                              static_cast<unsigned>(actor.sprite_id),
                              static_cast<int>(movement.size()), movement.data(),
                              actor.movement_bounds.has_value() ? " map_bound" : "");
                label_text({center.x + 8.0F, center.y + 2.0F}, color, actor_text.data());
            }
        }
    }

    draw->AddText({8.0F, 43.0F}, IM_COL32(230, 245, 255, 255),
                  "F3 annotations: map bounds | orange roam bounds | cyan warps | green NPCs | "
                  "red trainers | gold items");
}

} // namespace

void apply_tool_shortcuts(ToolState& tools, const WindowInput& input) {
    const auto toggle = [&](ToolLayout layout) {
        tools.layout = tools.layout == layout ? ToolLayout::closed : layout;
        tools.arrange = tools.layout != ToolLayout::closed;
    };
    if (input.toggle_player_tools) toggle(ToolLayout::player);
    if (input.toggle_developer_tools) toggle(ToolLayout::developer);
}

void draw_tools(ToolState& tools, GubsyRuntime& runtime, GameState& game,
                const content::CatalogSummary& catalog, const BootState& boot,
                BattleAnimationLab& lab, WorldState& maps,
                const RuleCatalog& rules, PresentationSettings& presentation,
                const GameClocks& clocks, const char* renderer_name) {
    if (ImGui::BeginMainMenuBar()) {
        ImGui::TextUnformatted("Pokemon Red Modern");
        ImGui::Separator();
        if (game.mode == Mode::title) {
            ImGui::Text("Boot: %s", label(boot.screen));
            ImGui::Separator();
            ImGui::TextUnformatted(
                "D-pad Navigate   A/Start Confirm   B Back   Select Case   "
                "F1/F2 Tools   F11 Fullscreen");
        } else if (game.mode == Mode::overworld) {
            const std::string_view map = selected_map_name(maps);
            ImGui::Text("Map: %.*s", static_cast<int>(map.size()), map.data());
            ImGui::Separator();
            ImGui::TextUnformatted("WASD/Arrows Move   E/Z/X/Enter Talk   [] Map   Tab World/Map   "
                                   "IJKL Pan   +/- Zoom   0 Reset   "
                                   "F3 Annotations   B Battle Lab   F1/F2 Tools   F11 Fullscreen");
        } else {
            const std::string_view animation = battle_animation_lab_name(lab);
            ImGui::Text("Animation: %.*s", static_cast<int>(animation.size()), animation.data());
            ImGui::Separator();
            const std::string_view species = battle_animation_lab_species_name(lab);
            ImGui::Text("Pokemon: %.*s", static_cast<int>(species.size()), species.data());
            ImGui::Separator();
            ImGui::Text("UI: %.*s", static_cast<int>(label(lab.ui.mode).size()),
                        label(lab.ui.mode).data());
            ImGui::Separator();
            ImGui::TextUnformatted(
                "Left/Right Animation   Up/Down Pokemon   B Maps   R Restart   F5 Reload   "
                "M UI   PgUp/PgDn Select   S Status   Space Auto   F1/F2 Tools   "
                "F11 Fullscreen");
        }
        ImGui::EndMainMenuBar();
    }

    sync_controls_navigation(tools.layout == ToolLayout::player, tools.controller_navigation);
    if (tools.layout == ToolLayout::player)
        draw_player_tools(tools, runtime, catalog, presentation);
    else if (tools.layout == ToolLayout::developer)
        draw_developer_tools(tools, game, catalog, lab, maps, rules, presentation, clocks,
                             renderer_name);

    if (game.mode == Mode::overworld) draw_world_annotations(maps);

    if (presentation.show_fps) {
        std::array<char, 32> text{};
        std::snprintf(text.data(), text.size(), "%.1f FPS",
                      static_cast<double>(ImGui::GetIO().Framerate));
        ImGui::GetForegroundDrawList()->AddText({8.0F, 25.0F}, IM_COL32(235, 245, 210, 255),
                                                text.data());
    }
    tools.arrange = false;
}

} // namespace pokered
