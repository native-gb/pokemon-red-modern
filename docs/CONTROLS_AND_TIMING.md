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
existing user profiles are not replaced at startup. The host snapshots current
controller buttons, triggers, and sticks once per rendered frame after SDL
events have updated the connected-device list.

Controllers are assigned to the local player automatically at startup and on
hot-plug. The F1 player interface exposes manual rescan, SDL/Gubsy detection
diagnostics, assignment state, profile selection, per-action binding editing,
trigger editing, removal, and default restoration.

On the web, a browser may conceal an already connected controller until its
first button gesture. The host samples the browser Gamepad API twice per second
and automatically refreshes SDL/Gubsy when a newly exposed controller appears;
the player does not need keyboard access to open tools and request a rescan.
The manual control-panel rescan remains available for unusual browser or device
mapping failures.

The player tools can be opened or closed without a keyboard by pressing the
bound Menu action or Start and Select together. ImGui never opens, polls, or
navigates with a gamepad; controllers belong exclusively to the semantic game
input layer. An open player or developer tool layout owns the complete input
frame, including its opening and closing frame, so an action cannot also move
the player, advance dialogue, reset the camera, or toggle fast-forward behind
the interface.

## Default bindings

Primary provides arrows and WASD, Z/E/Space for A, X for B, Enter for Start,
Backspace for Select, Escape for the player menu, and left Shift for
fast-forward. The controller defaults use the D-pad, south face for confirm,
east face for back, Start, Back/Select, Y for the player menu, Guide for
application quit, and left trigger for fast-forward. Left shoulder is a
digital fast-forward alternative. Right-stick up/down zooms the world camera;
Select resets its authored zoom. No controller camera-pan binding is present.

Alternate begins with IJKL movement, U/O for A/B, P/Y for Start/Select, Escape
for the player menu, and right Shift for fast-forward. Both profile slots are
fully editable, so these are installation defaults rather than engine rules.

In the overworld, Start opens the modern campaign overlay. Its first playable
slice exposes party HP/status/levels, the imported bag item names and
quantities, and trainer ID/money/play time. B returns from a detail page and
then closes the root; Start closes it from any page. The overlay owns input
while open, so navigation cannot move the player or advance a script behind
it.

Naming screens deliberately use only directional navigation, A/confirm,
B/back, and Start. Arrow keys and the controller D-pad move the cursor,
confirm selects a cell, back erases one character, and Start selects END
immediately once the name is non-empty. Host text entry, WASD, Select, and
Backspace do not acquire alternate meanings while a naming grid owns input.
Both boot naming and in-campaign nicknaming render from the imported native
8-by-8 UI tiles at an integer scale.

World zoom accepts `+`, `-`, and the mouse wheel. Panning or zooming manually
overrides the active map's framing until another map is entered. Player
Settings can disable `Use zone zoom on entry` to preserve a manually
chosen scale across every transition. At any scale, a complete active area
that fits is fixed and centered; an area that does not fit follows the player.

Battle command navigation is spatial rather than list-ordered: left/right
change columns and up/down change rows. Thus Down from FIGHT selects ITEM,
while Right from FIGHT selects PKMN. Once a move is committed, the controller
queues the move animation IDs from the resolved battle events and blocks new
battle input until those imported animations finish.

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
are persisted by Gubsy in the same runtime data root. The right-stick camera
axis is listed in the same editable profile UI as buttons and triggers. Render
smoke checks do not write settings.

## Campaign integration contract

Campaign menus, naming, dialogue, overworld movement, battle selection, and
other player-facing modes must consume the same semantic actions. A script
fiber that owns input may mask or redirect those actions at a deterministic
step boundary; no campaign executor may query SDL or a controller directly.

Fast-forward never changes music pitch or tempo. The audio executor runs on
unscaled wall-clock time at Red's 59.7275 Hz sequencer rate, while deterministic
game events continue to publish ordered sound and music commands from
simulation steps.

## Isolated developer input

Automated live playtests must not synthesize desktop keyboard input or take
window focus. An opt-in local Unix datagram socket feeds semantic actions at
the same host boundary as keyboard and controller input:

```sh
./scripts/run.sh --input-socket /tmp/pokered-modern-input.sock
./scripts/input.sh tap confirm
./scripts/input.sh down right
./scripts/input.sh up right
./scripts/input.sh text RED
./scripts/input.sh submit
```

`POKERED_MODERN_INPUT_SOCKET` selects a different path for the sender helper.
The receiver is disabled unless `--input-socket` is supplied, accepts only
local filesystem-socket traffic, and removes its socket on shutdown. Supported
commands are `tap`, `down`, and `up` for semantic controls; `text`, `erase`,
and `submit` for text entry; tool/annotation toggles; and `quit`. This is host
test instrumentation, not campaign content or a gameplay executor.
