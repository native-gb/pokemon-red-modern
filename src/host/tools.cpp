#include "host/tools.hpp"

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
    }
    ImGui::End();

    place(margin + left + gap, top, middle, height, tools.arrange);
    if (ImGui::Begin("Content Indexes", nullptr, kFixedTools)) {
        ImGui::Text("Pack: %s", content::label(catalog.state));
        ImGui::Separator();
        ImGui::Text("Maps             %zu", catalog.maps);
        ImGui::Text("Scripts          %zu", catalog.scripts);
        ImGui::Text("Species          %zu", catalog.species);
        ImGui::Text("Moves            %zu", catalog.moves);
        ImGui::Text("Items            %zu", catalog.items);
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
                const char* renderer_name) {
    if (ImGui::BeginMainMenuBar()) {
        ImGui::TextUnformatted("Pokemon Red Modern");
        ImGui::Separator();
        ImGui::TextUnformatted("F1 Player tools   F2 Developer tools   F11 Fullscreen");
        ImGui::EndMainMenuBar();
    }

    if (tools.layout == ToolLayout::player)
        draw_player_tools(tools, catalog);
    else if (tools.layout == ToolLayout::developer)
        draw_developer_tools(tools, game, catalog, renderer_name);
    tools.arrange = false;
}

} // namespace pokered
