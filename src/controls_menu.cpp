#include "controls_menu.hpp"

#include "controls.hpp"
#include "window.hpp"

#include <SDL3/SDL.h>
#include <backends/imgui_impl_sdl3.h>
#include <gubsy/input/binds_profile.hpp>
#include <gubsy/runtime.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

namespace pokered {
namespace {

struct ActionRow {
    ControlAction action;
    const char* label;
};

constexpr std::array kActions{
    ActionRow{ControlAction::left, "Left"},
    ActionRow{ControlAction::right, "Right"},
    ActionRow{ControlAction::up, "Up"},
    ActionRow{ControlAction::down, "Down"},
    ActionRow{ControlAction::confirm, "A / confirm"},
    ActionRow{ControlAction::back, "B / back"},
    ActionRow{ControlAction::start, "Start"},
    ActionRow{ControlAction::select, "Select"},
    ActionRow{ControlAction::menu, "Menu"},
    ActionRow{ControlAction::fast_forward, "Fast-forward"},
    ActionRow{ControlAction::quit, "Quit"},
};

bool save_profile(GubsyRuntime& runtime, const BindsProfile& profile, std::string& status) {
    if (!gubsy_replace_binds_profile(runtime, profile)) {
        status = "Could not save bindings";
        return false;
    }
    status = "Bindings saved";
    return true;
}

void choice_heading(int code) {
    if (code == static_cast<int>(GubsyButton::KB_A))
        ImGui::TextDisabled("Keyboard");
    else if (code == static_cast<int>(GubsyButton::MOUSE_LEFT))
        ImGui::TextDisabled("Mouse");
    else if (code == static_cast<int>(GubsyButton::GP_A))
        ImGui::TextDisabled("Controller");
}

bool draw_button_combo(GubsyRuntime& runtime, const BindsProfile& profile, int binding_index,
                       int action, const char* id, std::string& status) {
    const std::vector<ginput::ButtonBind>& bindings = profile.button_binds();
    const bool adding = binding_index < 0;
    const int current =
        adding ? -1 : bindings[static_cast<std::size_t>(binding_index)].device_button;
    const std::string preview =
        adding ? "Add binding..." : binds_input_label(BindsActionType::Button, current);
    ImGui::SetNextItemWidth(260.0F);
    if (!ImGui::BeginCombo(id, preview.c_str())) return false;

    bool changed = false;
    for (const InputChoice choice : binds_input_choices(BindsActionType::Button)) {
        choice_heading(choice.code);
        const bool selected = choice.code == current;
        if (ImGui::Selectable(choice.label, selected)) {
            BindsProfile updated = profile;
            if (!replace_bind_at(updated, BindsActionType::Button, binding_index, choice.code,
                                 action))
                status = "That binding already exists";
            else
                changed = save_profile(runtime, updated, status);
        }
        if (selected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
    return changed;
}

bool draw_axis_combo(GubsyRuntime& runtime, const BindsProfile& profile, int binding_index,
                     const char* id, std::string& status) {
    const std::vector<ginput::Axis1DBind>& bindings = profile.axis_1d_binds();
    const bool adding = binding_index < 0;
    const int current = adding ? -1 : bindings[static_cast<std::size_t>(binding_index)].device_axis;
    const std::string preview =
        adding ? "Add trigger..." : binds_input_label(BindsActionType::Analog1D, current);
    ImGui::SetNextItemWidth(260.0F);
    if (!ImGui::BeginCombo(id, preview.c_str())) return false;

    bool changed = false;
    for (const InputChoice choice : binds_input_choices(BindsActionType::Analog1D)) {
        const bool selected = choice.code == current;
        if (ImGui::Selectable(choice.label, selected)) {
            BindsProfile updated = profile;
            if (!replace_bind_at(updated, BindsActionType::Analog1D, binding_index, choice.code,
                                 static_cast<int>(AnalogControl::fast_forward)))
                status = "That trigger binding already exists";
            else
                changed = save_profile(runtime, updated, status);
        }
        if (selected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
    return changed;
}

bool draw_action(GubsyRuntime& runtime, const BindsProfile& profile, ActionRow row,
                 std::string& status) {
    const int action = static_cast<int>(row.action);
    ImGui::SeparatorText(row.label);
    bool found = false;
    for (std::size_t index = 0; index < profile.button_binds().size(); ++index) {
        if (profile.button_binds()[index].action != action) continue;
        found = true;
        ImGui::PushID(static_cast<int>(index));
        const bool changed = draw_button_combo(runtime, profile, static_cast<int>(index), action,
                                               "##binding", status);
        ImGui::SameLine();
        const bool remove = ImGui::SmallButton("Remove");
        ImGui::PopID();
        if (changed) return true;
        if (remove) {
            BindsProfile updated = profile;
            if (remove_bind_at(updated, BindsActionType::Button, static_cast<int>(index)))
                return save_profile(runtime, updated, status);
            status = "Could not remove binding";
        }
    }
    if (!found) ImGui::TextDisabled("Unbound");
    ImGui::PushID(action);
    const bool changed =
        draw_button_combo(runtime, profile, -1, action, "##add-binding", status);
    ImGui::PopID();
    return changed;
}

bool draw_fast_forward_axis(GubsyRuntime& runtime, const BindsProfile& profile,
                            std::string& status) {
    ImGui::TextDisabled("Analog trigger");
    bool found = false;
    for (std::size_t index = 0; index < profile.axis_1d_binds().size(); ++index) {
        if (profile.axis_1d_binds()[index].axis_1d !=
            static_cast<int>(AnalogControl::fast_forward))
            continue;
        found = true;
        ImGui::PushID(static_cast<int>(index));
        const bool changed =
            draw_axis_combo(runtime, profile, static_cast<int>(index), "##axis", status);
        ImGui::SameLine();
        const bool remove = ImGui::SmallButton("Remove");
        ImGui::PopID();
        if (changed) return true;
        if (remove) {
            BindsProfile updated = profile;
            if (remove_bind_at(updated, BindsActionType::Analog1D, static_cast<int>(index)))
                return save_profile(runtime, updated, status);
            status = "Could not remove trigger binding";
        }
    }
    if (!found) ImGui::TextDisabled("Unbound");
    return draw_axis_combo(runtime, profile, -1, "##add-axis", status);
}

void draw_profile(GubsyRuntime& runtime, int slot, std::string& status) {
    const BindsProfile* source = gubsy_find_binds_profile(runtime, control_profile_id(slot));
    if (source == nullptr) {
        ImGui::TextUnformatted("Binding profile is unavailable.");
        return;
    }
    const BindsProfile profile = *source;
    ImGui::TextUnformatted("Changes apply immediately and persist between runs.");
    if (ImGui::Button("Reset to defaults")) {
        status = reset_controls(runtime, slot) ? "Default bindings restored"
                                                : "Could not restore default bindings";
        return;
    }
    for (const ActionRow row : kActions) {
        if (draw_action(runtime, profile, row, status)) return;
        if (row.action == ControlAction::fast_forward &&
            draw_fast_forward_axis(runtime, profile, status))
            return;
    }
}

bool assigned_to_player(const GubsyLobbyState& lobby, int device_id) {
    if (lobby.local_players.empty()) return false;
    return std::ranges::any_of(
        lobby.local_players.front().devices, [device_id](GubsyLobbyDeviceAssignment device) {
            return device.type == InputSourceType::Gamepad && device.device_id == device_id;
        });
}

int draw_browser_gamepads() {
#ifdef __EMSCRIPTEN__
    if (emscripten_sample_gamepad_data() != EMSCRIPTEN_RESULT_SUCCESS) {
        ImGui::Text("Browser Gamepad API: unavailable");
        return -1;
    }
    int connected = 0;
    const int slots = emscripten_get_num_gamepads();
    for (int index = 0; index < slots; ++index) {
        EmscriptenGamepadEvent gamepad{};
        if (emscripten_get_gamepad_status(index, &gamepad) != EMSCRIPTEN_RESULT_SUCCESS ||
            !gamepad.connected)
            continue;
        ++connected;
        ImGui::BulletText("Browser %d: %s", gamepad.index, gamepad.id);
        ImGui::Indent();
        ImGui::TextDisabled("mapping %s, %d buttons, %d axes",
                            gamepad.mapping[0] ? gamepad.mapping : "none", gamepad.numButtons,
                            gamepad.numAxes);
        ImGui::Unindent();
    }
    ImGui::Text("Browser Gamepad API: %d connected", connected);
    return connected;
#else
    return -1;
#endif
}

void draw_input_diagnostics(GubsyRuntime& runtime) {
    ImGui::SeparatorText("Detection diagnostics");
    const int browser_count = draw_browser_gamepads();
    int joystick_count = 0;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&joystick_count);
    int mapped_count = 0;
    for (int index = 0; index < joystick_count; ++index) {
        const SDL_JoystickID id = joysticks[index];
        const bool mapped = SDL_IsGamepad(id);
        mapped_count += mapped ? 1 : 0;
        const char* name = SDL_GetJoystickNameForID(id);
        ImGui::BulletText("SDL %u: %s%s", id, name != nullptr && *name ? name : "Unknown joystick",
                          mapped ? "" : " (not mapped as a gamepad)");
    }
    SDL_free(joysticks);
    const int opened_count = static_cast<int>(gubsy_get_gamepads(runtime).size());
    ImGui::Text("SDL joysticks: %d | mapped: %d | Gubsy opened: %d", joystick_count,
                mapped_count, opened_count);
    if (browser_count > 0 && joystick_count == 0)
        ImGui::TextColored({1.0F, 0.75F, 0.25F, 1.0F},
                           "Browser sees the controller; SDL needs a rescan.");
    else if (joystick_count > 0 && mapped_count == 0)
        ImGui::TextColored({1.0F, 0.45F, 0.3F, 1.0F},
                           "SDL sees a joystick without a standard gamepad mapping.");
    else if (mapped_count > opened_count)
        ImGui::TextColored({1.0F, 0.45F, 0.3F, 1.0F},
                           "Gubsy could not open every mapped SDL gamepad.");
}

void draw_controllers(GubsyRuntime& runtime, std::string& status) {
    if (ImGui::Button("Rescan controllers")) {
        gubsy_refresh_gamepads(runtime);
        assign_unclaimed_gamepads(runtime);
        status = gubsy_get_gamepads(runtime).empty() ? "No controllers detected"
                                                     : "Controller scan complete";
    }
#ifdef __EMSCRIPTEN__
    ImGui::TextWrapped("Press a controller button to expose it to the browser, then rescan.");
#else
    ImGui::TextWrapped("Controllers are assigned automatically. Rescan recovers devices that "
                       "were connected before startup.");
#endif
    draw_input_diagnostics(runtime);
    const std::vector<GubsyGamepad> gamepads = gubsy_get_gamepads(runtime);
    if (gamepads.empty()) {
        ImGui::TextDisabled("No controllers detected.");
        return;
    }
    for (const GubsyGamepad& gamepad : gamepads) {
        ImGui::PushID(gamepad.device_id);
        ImGui::SeparatorText(gamepad.name.c_str());
        ImGui::TextDisabled("SDL device %d", gamepad.device_id);
        if (assigned_to_player(gubsy_get_lobby_state(runtime), gamepad.device_id))
            ImGui::TextUnformatted("Assigned to player");
        else if (ImGui::Button("Assign to player")) {
            assign_unclaimed_gamepads(runtime);
            status = assigned_to_player(gubsy_get_lobby_state(runtime), gamepad.device_id)
                         ? "Controller assigned"
                         : "Could not assign controller";
        }
        ImGui::PopID();
    }
}

} // namespace

void sync_controls_navigation(bool visible, bool& navigation_enabled) {
    if (navigation_enabled == visible || ImGui::GetCurrentContext() == nullptr) return;
    navigation_enabled = visible;
    if (visible) {
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoAll);
    } else {
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    }
}

void draw_controls_panel(GubsyRuntime& runtime, PresentationSettings& settings,
                         std::string& status) {
    if (!status.empty()) ImGui::TextDisabled("%s", status.c_str());
    int active = settings.control_profile;
    if (ImGui::RadioButton("Primary", active == 0)) active = 0;
    ImGui::SameLine();
    if (ImGui::RadioButton("Alternate", active == 1)) active = 1;
    if (active != settings.control_profile) {
        if (select_control_profile(runtime, active)) {
            settings.control_profile = active;
            status = "Active profile changed";
        } else {
            status = "Could not change active profile";
        }
    }

    if (ImGui::BeginTabBar("ControlProfiles")) {
        if (ImGui::BeginTabItem("Controllers")) {
            draw_controllers(runtime, status);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Primary bindings")) {
            draw_profile(runtime, 0, status);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Alternate bindings")) {
            draw_profile(runtime, 1, status);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

} // namespace pokered
