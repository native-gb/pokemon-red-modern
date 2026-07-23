# Controls and timing

## Control ownership

The runtime consumes semantic Game Boy actions rather than polling keyboard
scancodes in gameplay:

- left, right, up, and down;
- A/confirm and B/back;
- Start and Select;
- player menu, application quit, and fast-forward.

Gubsy owns physical-device discovery and translates keyboard or controller
inputs through one of two persistent profiles. Primary and Alternate profiles
can each be rebound independently. Missing profiles receive project defaults;
existing user profiles are not replaced at startup.

Controllers are assigned to the local player automatically at startup and on
hot-plug. The F1 player interface exposes manual rescan, SDL/Gubsy detection
diagnostics, assignment state, profile selection, per-action binding editing,
trigger editing, removal, and default restoration.

The player tools can be opened without a keyboard by pressing the bound Menu
action or Start and Select together. ImGui controller navigation is active only
while those tools are visible. An open player or developer tool layout owns
gameplay input, so menu navigation cannot move the player, advance dialogue, or
toggle fast-forward behind the interface.

## Default bindings

Primary provides arrows and WASD, Z/E/Space for A, X for B, Enter for Start,
Backspace for Select, Escape for the player menu, and left Shift for
fast-forward. The controller defaults use the D-pad, face A/B, Start, Back,
Y for the player menu, Guide for application quit, and left trigger for
fast-forward. Left shoulder is a digital fast-forward alternative.

Alternate begins with IJKL movement, U/O for A/B, P/Y for Start/Select, Escape
for the player menu, and right Shift for fast-forward. Both profile slots are
fully editable, so these are installation defaults rather than engine rules.

## Clock domains

Simulation advances at a deterministic 60 Hz. The host maintains separate
monotonic clocks:

- `game_time` and `game_steps` advance only when a deterministic simulation
  step executes;
- `real_time` records unscaled active runtime duration;
- `presentation_time` drives host-side visual timing;
- `audio_time` drives sound-effect scheduling;
- `music_time` drives music and is deliberately independent of fast-forward.

Hold-mode fast-forward scales the simulation accumulator while its semantic
control is down. Toggle mode changes state on the control's rising edge. The
player setting selects a multiplier from 2x through 8x. Pausing stops game time
but does not stop the independent host clocks.

Presentation choices are saved to `data/runtime/settings.cfg`. Binding profiles
are persisted by Gubsy in the same runtime data root. Render smoke checks do not
write settings.

## Campaign integration contract

Campaign menus, naming, dialogue, overworld movement, battle selection, and
other player-facing modes must consume the same semantic actions. A script
fiber that owns input may mask or redirect those actions at a deterministic
step boundary; no campaign executor may query SDL or a controller directly.

Fast-forward must never change music pitch or tempo. Future audio integration
must schedule music against `music_time`, not `game_time`, while deterministic
game events continue to publish their sound and music commands from simulation
steps.
