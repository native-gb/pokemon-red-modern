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
The Red importer will decode its attack programs, subanimations, frame blocks,
base coordinates, tile sets, and transform modes into generated local source
and normalized sprites. Generated files will be untracked and loaded through
the same compiler.

The intended normalized split is:

- battle views own persistent battlers, HUD, and screen layout;
- reusable battle clips own imported temporary sprite frames and positions;
- animation programs sequence clips, battler transforms, sounds, flashes, and
  other Pokémon-specific presentation operations;
- battle-effect programs independently decide damage and move behavior.
