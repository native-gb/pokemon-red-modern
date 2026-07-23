#include "tools.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdint>

namespace pokered {
namespace {

constexpr ImGuiWindowFlags kFixedTools =
    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

void place(float x, float y, float width, float height, bool arrange) {
    if (!arrange) return;
    ImGui::SetNextWindowPos({x, y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({width, height}, ImGuiCond_Always);
}

void draw_player_tools(ToolState& tools, const content::CatalogSummary& catalog) {
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
        ImGui::TextWrapped("The engine opens without a cartridge. Import and campaign selection "
                           "will live here once the typed content pipeline is connected.");
    }
    ImGui::End();

    place(margin + width + gap, top, width, height, tools.arrange);
    if (ImGui::Begin("Player Settings", nullptr, kFixedTools)) {
        ImGui::TextUnformatted("Display");
        ImGui::BulletText("F11 toggles fullscreen");
        ImGui::BulletText("Game view keeps its native 10:9 aspect");
        ImGui::BulletText("GPU render target; no CPU framebuffer");
        ImGui::Spacing();
        ImGui::TextUnformatted("Controls and accessibility settings will be added "
                               "with the first playable vertical slice.");
    }
    ImGui::End();
}

void draw_developer_tools(ToolState& tools, GameState& game, const content::CatalogSummary& catalog,
                          BattleAnimationLab& lab, MapBrowser& maps,
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
        ImGui::Checkbox("Paused", &game.paused);
        ImGui::Separator();
        ImGui::Text("Renderer: %s", renderer_name);
        ImGui::TextUnformatted("Fixed simulation: 60 Hz");
        ImGui::Separator();
        const WorldMap* map = current_map(maps);
        ImGui::Text("Map: %.*s", static_cast<int>(current_map_name(maps).size()),
                    current_map_name(maps).data());
        ImGui::Text("Map record: %zu / %zu", maps.maps.empty() ? 0 : maps.current + 1,
                    maps.maps.size());
        if (map != nullptr) {
            ImGui::Text("ROM ID: 0x%02X", static_cast<unsigned>(map->id));
            ImGui::Text("Blocks: %u x %u", static_cast<unsigned>(map->width_blocks),
                        static_cast<unsigned>(map->height_blocks));
            ImGui::Text("Tiles: %u x %u", static_cast<unsigned>(map->width_tiles),
                        static_cast<unsigned>(map->height_tiles));
            ImGui::Text("Tileset: %u", static_cast<unsigned>(map->tileset_id));
        }
        if (ImGui::Button("Previous Map")) previous_map(maps);
        ImGui::SameLine();
        if (ImGui::Button("Next Map")) next_map(maps);
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
        ImGui::Text("Scripts          %zu", catalog.scripts);
        ImGui::Text("Species          %zu", catalog.species);
        ImGui::Text("Moves            %zu", catalog.moves);
        ImGui::Text("Items            %zu", catalog.items);
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

} // namespace

void apply_tool_shortcuts(ToolState& tools, const WindowInput& input) {
    const auto toggle = [&](ToolLayout layout) {
        tools.layout = tools.layout == layout ? ToolLayout::closed : layout;
        tools.arrange = tools.layout != ToolLayout::closed;
    };
    if (input.toggle_player_tools) toggle(ToolLayout::player);
    if (input.toggle_developer_tools) toggle(ToolLayout::developer);
}

void draw_tools(ToolState& tools, GameState& game, const content::CatalogSummary& catalog,
                BattleAnimationLab& lab, MapBrowser& maps, const char* renderer_name) {
    if (ImGui::BeginMainMenuBar()) {
        ImGui::TextUnformatted("Pokemon Red Modern");
        ImGui::Separator();
        if (game.mode == Mode::overworld) {
            const std::string_view map = current_map_name(maps);
            ImGui::Text("Map: %.*s", static_cast<int>(map.size()), map.data());
            ImGui::Separator();
            ImGui::TextUnformatted(
                "Left/Right Map   B Battle Lab   F1/F2 Tools   F11 Fullscreen");
        } else {
            const std::string_view animation = battle_animation_lab_name(lab);
            ImGui::Text("Animation: %.*s", static_cast<int>(animation.size()),
                        animation.data());
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

    if (tools.layout == ToolLayout::player)
        draw_player_tools(tools, catalog);
    else if (tools.layout == ToolLayout::developer)
        draw_developer_tools(tools, game, catalog, lab, maps, renderer_name);
    tools.arrange = false;
}

} // namespace pokered
