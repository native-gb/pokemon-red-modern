# Modern Campaign Save

The current native campaign save is intentionally independent of the Game Boy
SRAM layout. It is a readable, versioned, indented S-expression file at:

```text
data/runtime/saves/pokemon_red_modern.sexpr
```

SAVE in the Start menu writes one atomic slot. When that slot exists,
CONTINUE appears at the top of the title menu. A temporary file is completed
before it replaces the previous slot, so an interrupted write does not
partially overwrite a working save.

The first schema records:

- player/rival names, options, trainer ID, money, and play time;
- campaign flags and variables;
- bag stacks, party, storage boxes, and daycare state;
- HP, status, moves, PP, stats, DVs, and stat experience for each Pokémon;
- current map, cell, facing, last outdoor map, and world RNG;
- every world actor's map, spawn identity, position, facing, and visibility;
- the last healing checkpoint.

Example shape:

```text
pokemon_red_modern_save 1
    player_name "RED"
    rival_name "BLUE"
    options 0 0 0
    trainer_id 12345
    money 3000
    play_steps 8124
    flags 1 0 0 1
    variables 0 7
    random_state 3221344269
    player 38 3 6 down 0
    inventory 20
        stack 4 5
    party
        pokemon
            species 4
            level 5
            experience 135
            ...
    storage 20 0
        box 0
        ...
    daycare false 0
    actors
        actor 0 0 5 4 down true
```

The loader validates the complete document into temporary state before
committing it. It then rebuilds actor collision and trainer-sight indexes from
the restored actor positions.

Saving is available only at a stable overworld boundary. An active battle,
script fiber, or input-locked event must finish first. Transient dialogue,
menus, interpolation, battle presentation, camera position, and host settings
are not campaign state and are not serialized.

Game Boy save import/export remains a separate future adapter. It must report
loss when the modern semantic state cannot be represented in original SRAM.
