# Battle animation lab

## Purpose

The lab isolates visual battle-animation development from battle rules,
campaign progression, imported Pokémon assets, and the reference executable.
It renders two original placeholder battlers and runs readable animation
programs against the semantic targets `attacker`, `defender`, and
`battle_screen`.

The battle view owns battler layout. Animation programs may transform those
targets and own temporary effects created with `spawn`. There is no general
scene graph.

## Running

```sh
./scripts/run.sh
```

The current animation name appears in the top bar.

- Left/Right selects the previous or next animation.
- `R` restarts the current animation.
- Space enables or disables automatic iteration.
- `F5` reparses and recompiles the source directory without rebuilding C++.
- `F2` opens executor state and the auto-advance control.

## Source

Committed original fixtures live in:

```text
data/dev/battle_animations/
```

For example:

```text
animation scratch
    spawn slash_left slash
    set_position slash_left 106 34 native_canvas
    tween_position slash_left 122 50 5 ease_out native_canvas
    destroy slash_left
```

The directory loader recursively reads `.sexpr` files in deterministic path
order. Syntax and reference failures include the relative file, line, and
column.

## Red import boundary

The committed fixtures contain no cartridge graphics or animation programs.
Import the real programs and graphics from a locally owned Pokemon Red US Rev
0 ROM:

```sh
./scripts/import_battle_animations.sh
```

The importer verifies SHA1
`ea9bcae617fdf159b045185467ae58b2e4a48b9a`. It decodes 203 attack programs,
86 subanimations, 122 frame blocks, 177 base coordinates, two tile sets, and
the player-side transform rules. Its ignored local output is:

```text
data/runtime/imports/pokemon_red_us_rev_0/
    source/animations/battle_moves/
        001_pound.sexpr
        033_tackle.sexpr
        165_struggle.sexpr
    compiled/battle_animation_frames.bin
```

Normal runs prefer these imported programs when the directory exists and
otherwise use the committed fixtures. Every ROM special-effect command is
retained by name in generated source. Common battler movement, visibility, and
delay effects are normalized now; the other named signals remain explicit
implementation work rather than silently disappearing.

The generated report `reports/battle_animation_summary.txt` lists every
procedural effect type which still lowers to a visible named signal. Palette
changes, short and long flashes, and the X-stat spiral-ball sequence are
compiled into normal animation operations and imported frame visuals.

The remaining first-pass signals are grouped work rather than unknown data:

- particles: water droplets, falling leaves/petals, and upward balls;
- screen transforms: whole-screen shake and scanline waves;
- picture transforms: minimize, substitute, transform, squish, and rapid
  back-and-forth movement;
- UI presentation: enemy HUD shake.

The two internal animations named `enemy_flash` and `player_flash` are
misleading in isolation. Their ROM routines restore or reload an existing
Pokemon picture; they are not standalone flashing effects and can legitimately
produce no visible change when the picture is already present.

The verification view uses Red's fixed battle picture boxes: the enemy 7 by 7
tile picture begins at `(96, 0)`, the player picture begins at `(8, 40)`, and
the six-row message region begins at `y = 96`.

The intended normalized split is:

- battle views own persistent battlers, HUD, and screen layout;
- persistent targets move through offsets from renderer-owned anchors;
- imported temporary sprite pieces retain exact 160 by 144 canvas positions;
- animation programs sequence clips, battler transforms, sounds, flashes, and
  other Pokémon-specific presentation operations;
- battle-effect programs independently decide damage and move behavior.
