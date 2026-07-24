#include "controls.hpp"

#include <SDL3/SDL.h>
#include <gubsy/input/binds_profile.hpp>
#include <gubsy/runtime.hpp>

#include <algorithm>

namespace pokered {
namespace {

constexpr int kPrimaryProfile = 6101;
constexpr int kAlternateProfile = 6102;
constexpr float kTriggerThreshold = 0.25F;

void bind(BindsProfile& profile, GubsyButton button, ControlAction action) {
    bind_button(profile, button, static_cast<int>(action));
}

void bind(BindsProfile& profile, Gubsy1DAnalog axis, AnalogControl action) {
    bind_1d_analog(profile, axis, static_cast<int>(action));
}

void bind(BindsProfile& profile, Gubsy2DAnalog axis,
          Analog2DControl action) {
    bind_2d_analog(profile, axis, static_cast<int>(action));
}

void bind_gamepad(BindsProfile& profile) {
    bind(profile, GubsyButton::GP_DPAD_LEFT, ControlAction::left);
    bind(profile, GubsyButton::GP_DPAD_RIGHT, ControlAction::right);
    bind(profile, GubsyButton::GP_DPAD_UP, ControlAction::up);
    bind(profile, GubsyButton::GP_DPAD_DOWN, ControlAction::down);
    bind(profile, GubsyButton::GP_A, ControlAction::confirm);
    bind(profile, GubsyButton::GP_B, ControlAction::back);
    bind(profile, GubsyButton::GP_START, ControlAction::start);
    bind(profile, GubsyButton::GP_BACK, ControlAction::select);
    bind(profile, GubsyButton::GP_Y, ControlAction::menu);
    bind(profile, GubsyButton::GP_GUIDE, ControlAction::quit);
    bind(profile, GubsyButton::GP_LEFT_SHOULDER, ControlAction::fast_forward);
    bind(profile, Gubsy1DAnalog::GP_LEFT_TRIGGER, AnalogControl::fast_forward);
    bind(profile, Gubsy2DAnalog::GP_RIGHT_STICK,
         Analog2DControl::camera_zoom);
}

BindsProfile default_profile(int slot) {
    BindsProfile profile;
    profile.id = slot == 0 ? kPrimaryProfile : kAlternateProfile;
    profile.name =
        slot == 0 ? "PokemonRedModernPrimaryV1" : "PokemonRedModernAlternateV1";
    if (slot == 0) {
        // Preserve familiar Game Boy and PC conventions in one editable slot.
        bind(profile, GubsyButton::KB_LEFT, ControlAction::left);
        bind(profile, GubsyButton::KB_A, ControlAction::left);
        bind(profile, GubsyButton::KB_RIGHT, ControlAction::right);
        bind(profile, GubsyButton::KB_D, ControlAction::right);
        bind(profile, GubsyButton::KB_UP, ControlAction::up);
        bind(profile, GubsyButton::KB_W, ControlAction::up);
        bind(profile, GubsyButton::KB_DOWN, ControlAction::down);
        bind(profile, GubsyButton::KB_S, ControlAction::down);
        bind(profile, GubsyButton::KB_Z, ControlAction::confirm);
        bind(profile, GubsyButton::KB_E, ControlAction::confirm);
        bind(profile, GubsyButton::KB_SPACE, ControlAction::confirm);
        bind(profile, GubsyButton::KB_X, ControlAction::back);
        bind(profile, GubsyButton::KB_ENTER, ControlAction::start);
        bind(profile, GubsyButton::KB_BACKSPACE, ControlAction::select);
        bind(profile, GubsyButton::KB_ESCAPE, ControlAction::menu);
        bind(profile, GubsyButton::KB_LSHIFT, ControlAction::fast_forward);
    } else {
        // The alternate slot is independent and keeps a compact right-hand map.
        bind(profile, GubsyButton::KB_J, ControlAction::left);
        bind(profile, GubsyButton::KB_L, ControlAction::right);
        bind(profile, GubsyButton::KB_I, ControlAction::up);
        bind(profile, GubsyButton::KB_K, ControlAction::down);
        bind(profile, GubsyButton::KB_U, ControlAction::confirm);
        bind(profile, GubsyButton::KB_O, ControlAction::back);
        bind(profile, GubsyButton::KB_P, ControlAction::start);
        bind(profile, GubsyButton::KB_Y, ControlAction::select);
        bind(profile, GubsyButton::KB_ESCAPE, ControlAction::menu);
        bind(profile, GubsyButton::KB_RSHIFT, ControlAction::fast_forward);
    }
    bind_gamepad(profile);
    return profile;
}

int profile_id(int slot) {
    return slot == 0 ? kPrimaryProfile : kAlternateProfile;
}

bool gamepad_assigned(const GubsyLobbyState& lobby, int device_id) {
    for (const GubsyLobbyPlayer& player : lobby.local_players) {
        const bool found =
            std::ranges::any_of(player.devices, [device_id](GubsyLobbyDeviceAssignment device) {
                return device.type == InputSourceType::Gamepad && device.device_id == device_id;
            });
        if (found) return true;
    }
    return false;
}

} // namespace

bool register_controls(GubsyRuntime& runtime, int profile) {
    BindsSchema schema;
    (void)schema.add_action(static_cast<int>(ControlAction::left), "Left", "Game Boy");
    (void)schema.add_action(static_cast<int>(ControlAction::right), "Right", "Game Boy");
    (void)schema.add_action(static_cast<int>(ControlAction::up), "Up", "Game Boy");
    (void)schema.add_action(static_cast<int>(ControlAction::down), "Down", "Game Boy");
    (void)schema.add_action(static_cast<int>(ControlAction::confirm), "A / Confirm", "Game Boy");
    (void)schema.add_action(static_cast<int>(ControlAction::back), "B / Back", "Game Boy");
    (void)schema.add_action(static_cast<int>(ControlAction::start), "Start", "Game Boy");
    (void)schema.add_action(static_cast<int>(ControlAction::select), "Select", "Game Boy");
    (void)schema.add_action(static_cast<int>(ControlAction::menu), "Menu", "Application");
    (void)schema.add_action(static_cast<int>(ControlAction::quit), "Quit", "Application");
    (void)schema.add_action(static_cast<int>(ControlAction::fast_forward), "Fast-forward",
                            "Enhancements");
    (void)schema.add_axis_1d(static_cast<int>(AnalogControl::fast_forward), "Fast-forward",
                             "Enhancements");
    (void)schema.add_axis_2d(
        static_cast<int>(Analog2DControl::camera_zoom),
        "Camera zoom", "Camera");
    gubsy_register_binds_schema(runtime, schema);

    for (int slot = 0; slot < kControlProfileCount; ++slot) {
        const BindsProfile defaults = default_profile(slot);
        const BindsProfile* existing =
            gubsy_find_binds_profile(runtime, defaults.id);
        if (existing == nullptr) {
            if (!gubsy_replace_binds_profile(runtime, defaults))
                return false;
            continue;
        }
        const bool has_camera_zoom = std::ranges::any_of(
            existing->axis_2d_binds(),
            [](const ginput::Axis2DBind& binding) {
                return binding.axis_2d ==
                    static_cast<int>(
                        Analog2DControl::camera_zoom);
            });
        if (!has_camera_zoom) {
            BindsProfile migrated = *existing;
            bind(migrated, Gubsy2DAnalog::GP_RIGHT_STICK,
                 Analog2DControl::camera_zoom);
            if (!gubsy_replace_binds_profile(runtime, migrated))
                return false;
        }
    }
    return select_control_profile(runtime, profile);
}

int control_profile_id(int profile) {
    return profile >= 0 && profile < kControlProfileCount ? profile_id(profile) : -1;
}

bool select_control_profile(GubsyRuntime& runtime, int profile) {
    if (profile < 0 || profile >= kControlProfileCount) return false;
    return gubsy_set_lobby_player_binds_profile(runtime, 0, profile_id(profile));
}

bool reset_controls(GubsyRuntime& runtime, int profile) {
    if (profile < 0 || profile >= kControlProfileCount) return false;
    return gubsy_replace_binds_profile(runtime, default_profile(profile));
}

void assign_unclaimed_gamepads(GubsyRuntime& runtime) {
    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
    if (gamepads == nullptr) return;
    for (int index = 0; index < count; ++index) {
        const int device = static_cast<int>(gamepads[index]);
        if (!gamepad_assigned(gubsy_get_lobby_state(runtime), device))
            gubsy_toggle_lobby_player_device(runtime, 0,
                                             {InputSourceType::Gamepad, device});
    }
    SDL_free(gamepads);
}

bool control_down(GubsyRuntime& runtime, ControlAction action) {
    return gubsy_lobby_player_action_down(runtime, 0, static_cast<int>(action));
}

ControlButtons read_controls(GubsyRuntime& runtime) {
    const bool trigger = gubsy_lobby_player_axis_1d_down(
        runtime, 0, static_cast<int>(AnalogControl::fast_forward), kTriggerThreshold);
    const ginput::Vec2 camera =
        gubsy_lobby_player_axis_2d(
            runtime, 0,
            static_cast<int>(Analog2DControl::camera_zoom));
    return {
        .left = control_down(runtime, ControlAction::left),
        .right = control_down(runtime, ControlAction::right),
        .up = control_down(runtime, ControlAction::up),
        .down = control_down(runtime, ControlAction::down),
        .confirm = control_down(runtime, ControlAction::confirm),
        .back = control_down(runtime, ControlAction::back),
        .start = control_down(runtime, ControlAction::start),
        .select = control_down(runtime, ControlAction::select),
        .menu = control_down(runtime, ControlAction::menu),
        .quit = control_down(runtime, ControlAction::quit),
        .fast_forward = control_down(runtime, ControlAction::fast_forward) || trigger,
        .camera_zoom = camera.y,
    };
}

} // namespace pokered
