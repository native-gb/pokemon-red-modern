# Architecture

## Design objective

Pokémon Red Modern is a data-driven RPG engine, not a translation of Game Boy
machine organization. The cartridge tells the importer what campaign content
exists and provides behavioral evidence. The modern runtime owns semantic
state: maps, actors, parties, inventory, quests, scripts, battles, and animation
timelines.

## Visible ownership graph

`main.cpp` constructs and connects these independent domains:

```text
local ROM -> importer -> immutable ContentPack -> cache
                                  |
input -> deterministic GameState -> semantic events
                                  |
                    RenderState + audio director
                         |                 |
                    GPU renderer       audio device

host window / controls / persistence / ImGui observe the edges
```

There is no all-purpose application context. Dependencies are parameters or
small domain-owned handles. Shutdown follows the visible ownership order.

## Source layout

Keep the top-level code flat until a module has enough concrete files to earn a
directory:

```text
src/
  main.cpp
  catalog.cpp/.hpp
  state.cpp/.hpp
  window.cpp/.hpp
  tools.cpp/.hpp
  input.cpp/.hpp
  save.cpp/.hpp
  scripts.cpp/.hpp
  battle.cpp/.hpp
  audio.cpp/.hpp
  render/
    frame.cpp/.hpp
    overworld.cpp/.hpp
    battle.cpp/.hpp
    menus.cpp/.hpp
    text.cpp/.hpp
    atlases.cpp/.hpp
    utils.cpp/.hpp
```

`render/frame` is the top-level frame dispatcher. The other render files own
specific draw passes and shared GPU helpers. Do not recreate umbrella
`runtime`, `content`, or `host` directories merely to label dependencies.

## Core domains

### Content

One manifest points to many typed indexes:

- strings and localized text;
- world topology, maps, blocks, collisions, warps, triggers, and connections;
- actor definitions, placements, movement policies, and trainer metadata;
- scripts, predicates, flags, variables, and event bindings;
- species, forms, types, moves, learnsets, growth curves, and evolutions;
- items, shops, machines, field uses, and key-item behavior;
- parties, trainer classes, AI profiles, and encounter tables;
- graphics, palettes, UI layouts, animation tracks, music, and sound effects;
- credits and animation sequences;
- provenance and completeness records.

Indexes use stable typed IDs. Imported content is immutable after validation.
Maps are fully materialized during import; cartridge streaming is not a runtime
concept.

### Runtime

`GameState` contains only deterministic mutable state:

- current map and player transform;
- actors and their active script/movement state;
- party, PC storage, inventory, money, Pokédex, badges, and options relevant to
  rules;
- flags, variables, defeated trainers, collected objects, and world mutations;
- active menu, dialogue, transition, encounter, or battle;
- script coroutine stacks and deterministic random streams;
- play time and save metadata.

Runtime systems are explicit passes. A typical overworld step is input
arbitration, scripts, actor intentions, movement/collision, triggers,
interactions, encounters, transitions, then semantic event publication.

### Scripts

Campaign scripts are indentation-scoped source compiled into typed IR and then
compact bytecode. The VM is a resumable coroutine scheduler, not an emulator.

```text
script oak_leads_player:
    lock input
    face oak down
    say oak "..."
    move oak up
    move oak up
    wait movement oak
    unlock input
```

Each command has one semantic action. Coordinates, flags, actors, items, maps,
and text references are typed and resolved during validation. Waits, child
scripts, choices, and movement completion are explicit.

Use native C++ systems for reusable mechanics. Use imported scripts for
campaign-specific sequencing. A script may request a battle or transition; the
corresponding native executor owns its rules.

### Battle

Battle owns turn resolution and publishes outcomes. It does not draw pixels or
drive the audio device. Data tables define species, stats, moves, effects,
learnsets, parties, AI policy, and rewards. Native effect primitives implement
reusable mechanics; imported effect programs compose them where practical.

Compatibility quirks are named rules in a profile. Unsafe memory overlap is not
modeled. If a visible bug is worth preserving, implement its semantic result
directly and test it.

### Rendering and animation sequences

Gameplay publishes events such as `DialogueOpened`, `MoveUsed`,
`DamageApplied`, `WarpStarted`, and `ItemReceived`. Animation timelines
translate these into:

- sprite and tile draw commands;
- camera and palette operations;
- UI composition;
- animation tracks;
- music state and sound cues.

`render/` contains concrete frame dispatchers, render utilities, atlas
resources, and draw functions. The GPU backend owns atlases and batches.
Imported assets are decoded once; the renderer does not reconstruct Game Boy
tiles or sprites every frame. A 160×144 compatibility composition is one view
policy, not an architectural constraint.

## Fixed-step sequencing

Simulation advances at a fixed cadence. The host accumulates monotonic elapsed
time, runs zero or more deterministic steps, then renders once. Rendering may
interpolate walking, cameras, and non-rule animation. Speed-up changes the
number of simulation steps per wall-clock second while audio pitch and music
tempo remain independently controlled.

## Persistence

Saves store semantic state with an explicit version. Writes are atomic and
migrations are one-way, tested transforms. Content-pack identity is recorded so
a save cannot silently load against incompatible campaign data. Host settings,
campaign saves, importer cache, and debug snapshots are separate files.
