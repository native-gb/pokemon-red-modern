# Battle animation lab

## Purpose

The lab isolates visual battle-animation development from battle rules,
campaign progression, and the reference executable. With a local Red import,
it renders the selected species' back picture as the player and front picture
as the opponent. It runs readable animation programs against the semantic
targets `attacker`, `defender`, and `battle_screen`.

The battle view owns battler layout. Animation programs may transform those
targets and own temporary effects created with `spawn`. There is no general
scene graph.

## Running

```sh
./scripts/run.sh
```

The current animation name appears in the top bar.

- Left/Right selects the previous or next animation.
- Up/Down selects the previous or next Pokémon. The F2 tools expose equivalent
  buttons and exact coverage counts.
- `R` restarts the current animation.
- Space enables or disables automatic iteration.
- `F5` reparses and recompiles the source directory without rebuilding C++.
- `F2` opens executor state and the auto-advance control.

## Source

Small synthetic parser fixtures live in:

```text
tests/fixtures/battle_animations/
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
./scripts/import_rom.sh
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
    source/animations/procedural_profile.sexpr
    source/graphics/pokemon_visuals.sexpr
    source/graphics/trainer_visuals.sexpr
    compiled/battle_animation_frames.bin
    compiled/battle_animation_procedural.bin
    compiled/battle_pictures.bin
```

The decoder is compiled into the standalone native importer and is not linked
into the game executable. The shell command builds and invokes that tool. A
browser build can compile the same decoding sources into a separate importer
WebAssembly module without Python, a subprocess, or native filesystem access.

Normal runs consume these locally imported programs. The committed fixtures
are test inputs, not a fallback campaign. Every ROM special-effect command is
retained by name in generated source and all 39 used special-effect routines
lower without a generic fallback.

The importer reads Red's timing operands, coordinate tables, palette sequences,
particle paths, special-picture data, and animation tiles from the verified
ROM. It expands procedural particles into local frame visuals and writes the
remaining wave, palette, Minimize, and Substitute material to
`battle_animation_procedural.bin`. The readable
`procedural_profile.sexpr` exposes the small tables for inspection without
placing them in the repository.

The generated report `reports/battle_animation_summary.txt` lists any
procedural effect which could not be lowered. The supported Red profile
currently reports zero.

The same portable import pass decodes all 151 species front pictures, all 151
back pictures, and all 47 trainer-class portrait bindings. The trainer table
resolves to 45 unique compressed portraits because two class bindings share
existing art. Readable files retain display names, dimensions, and ROM
provenance. `battle_pictures.bin` contains already decoded shade pixels, so
normal rendering never decompresses cartridge pictures.

The executor has Pokémon-specific operations for picture form, horizontal
squish, and wave phase in addition to ordinary movement, visibility, palette,
sound, and temporary sprites. The lab previews the imported Minimize and
Substitute graphics and ROM-driven screen offsets. Its wave preview applies
the current imported horizontal phase to the composition; the final battle
renderer will apply the same table per scanline.

The two internal animations named `enemy_flash` and `player_flash` are
misleading in isolation. Their ROM routines restore or reload an existing
Pokemon picture; they are not standalone flashing effects and can legitimately
produce no visible change when the picture is already present.

The verification view uses Red's fixed battle picture boxes: the enemy 7 by 7
tile picture begins at `(96, 0)`, the player picture begins at `(8, 40)`, and
the six-row message region begins at `y = 96`. Fronts preserve the original
integer horizontal centering and bottom alignment. Backs reproduce
`ScaleSpriteByTwo`: only the upper-left 28 by 28 pixels of the 4 by 4 source
picture are doubled into the 7 by 7 player box.

The intended normalized split is:

- battle views own persistent battlers, HUD, and screen layout;
- persistent targets move through offsets from renderer-owned anchors;
- imported temporary sprite pieces retain exact 160 by 144 canvas positions;
- animation programs sequence clips, battler transforms, sounds, flashes, and
  other Pokémon-specific presentation operations;
- battle-effect programs independently decide damage and move behavior.
