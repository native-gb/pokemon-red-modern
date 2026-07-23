# Campaign script language

## Purpose

The script language expresses campaign-specific sequencing: dialogue, actor
movement, flags, choices, item delivery, transitions, and requests to native
systems. It does not implement collision, damage formulas, rendering, audio
mixing, save I/O, or other reusable engine mechanics.

Source is readable, indentation-scoped, and deliberately small. Internally it
compiles to typed bytecode and executes as resumable coroutines. S-expressions
may be useful in compiler internals, but they are not the authoring syntax.

## Syntax principles

- One semantic action per line.
- Indentation forms blocks.
- References resolve to typed IDs during compilation.
- Commands state who does what: `move oak up`, then `move oak right`.
- No combined direction shorthand such as `move oak up right`.
- Waits and input ownership are explicit.
- Conditions are semantic predicates, not raw memory reads.

```text
script pallet_oak_encounter:
    require flag oak_not_met
    lock input
    face player up
    move oak down
    wait movement oak
    say oak text.oak_dangerous
    follow player oak
    path player pallet_lab.entrance
    wait movement player
    stop follow player oak
    unlock input
```

```text
script parcel_delivery:
    if inventory has item.oaks_parcel:
        take item.oaks_parcel 1
        say oak text.parcel_thanks
        give item.pokedex 1
        set flag pokedex_received
    else:
        say oak text.oak_waiting
```

## Command families

The first compiler should cover:

- flow: `if`, `else`, `choose`, `call`, `return`, `parallel`;
- synchronization: `wait`, `lock input`, `unlock input`;
- dialogue: `say`, `prompt`, `name`, `close text`;
- actors: `face`, `move`, `path`, `follow`, `show`, `hide`, `emote`;
- world: `warp`, `transition`, `set tile`, `refresh map`;
- state: `set flag`, `clear flag`, `set variable`, `add variable`;
- inventory: `give`, `take`, `has`, `open shop`;
- party: `give pokemon`, `heal party`, `open party`;
- encounters: `start battle`, `start trainer battle`;
- animation: `play cue`, `start timeline`, `fade`;
- system: `save`, `open menu`, `show credits`.

Commands are versioned. Unsupported commands fail pack validation; they never
become runtime no-ops.

## Compiler pipeline

```text
source
  -> tokens and indentation tree
  -> typed AST
  -> reference resolution
  -> control-flow and ownership validation
  -> normalized IR
  -> compact bytecode
  -> source map and diagnostics
```

Validation checks missing references, impossible paths where statically known,
unbalanced input locks, invalid waits, unsafe recursion, unreachable labels,
and command/domain mismatches.

## Runtime model

Each active script is a `ScriptFiber` containing:

- script ID and instruction pointer;
- typed value stack;
- call frames;
- wait condition;
- owned input/control locks;
- parent/child relationship;
- deterministic creation order;
- source location for tools.

The scheduler advances ready fibers at a deterministic point in the fixed game
step. A command either completes immediately, yields with a wait condition,
starts a child operation and yields, or terminates. Developer tools expose the
fiber list, stack frames, current source line, waits, locks, and recent command
trace.

## Native executor boundaries

Script commands call narrow semantic interfaces owned by native executors:

- world topology and map transitions;
- movement, collision, interaction, and actor lifecycle;
- encounter selection;
- party, progression, evolution, and move learning;
- inventory, shops, machines, and field-item use;
- battle rules and AI;
- dialogue, menus, and text layout;
- render timelines and animation tracks;
- music/effect direction and mixing;
- save/load and migration.

This keeps the language expressive enough for the campaign without turning it
into a second general-purpose engine.
