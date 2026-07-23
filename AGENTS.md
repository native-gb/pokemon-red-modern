# Pokémon Red Modern engineering rules

## Mission

Build a clean, native Pokémon RPG engine whose first supported campaign is
Pokémon Red. The neighboring reference repository is evidence about behavior
and cartridge layout. It is not the ownership model, architecture, or source
shape for this repository.

## Runtime boundaries

- `runtime/` owns deterministic rules and mutable game state. It does not
  include SDL, ImGui, filesystem, audio-device, or ROM-parsing headers.
- `content/` owns immutable typed campaign data and import validation. Runtime
  code consumes stable IDs and semantic records, never ROM offsets.
- `presentation/` converts semantic state and events into render/audio commands.
  Presentation timing must not mutate game rules.
- `host/` owns windows, GPU resources, input devices, persistence, and developer
  UI. These types do not leak into runtime code.
- `main.cpp` visibly owns and connects independent domains. Do not introduce an
  `App`, `Engine`, `Context`, service locator, or global registry.

## Data and distribution

- Do not commit ROMs, imported campaign packs, extracted graphics, dialogue,
  music, maps, encounters, or other copyrighted cartridge content.
- The public project may contain original engine code, schemas, validators,
  import logic, tooling, and clearly original test fixtures.
- A supported ROM is decoded locally into a versioned cache. The cache is
  reproducible and disposable; it is never source-of-truth.
- Every imported record carries provenance sufficient to trace it to a ROM
  version and source range without exposing raw offsets to gameplay.

## Code shape

- Prefer explicit structs, enums, spans, and free functions over inheritance.
- Keep ownership and lifetime visible. Host handles use one clear owner.
- Use typed IDs at domain boundaries. Avoid magic integers and stringly-typed
  cross-domain references.
- Organize by cohesive responsibility. Split files near 300–500 lines when a
  natural boundary exists; do not split merely to hit a number.
- Comments explain invariants, ownership, sequencing, or non-obvious source
  evidence. Do not narrate individual statements.
- Keep hot-loop work proportional to changed code. Run focused checks first;
  broad audits are explicit milestones.

## Simulation and scripts

- Advance gameplay on a deterministic fixed step. Rendering may interpolate
  but may never change the number or order of simulation steps.
- World, battle, menus, cutscenes, and transitions publish semantic events.
- Campaign orchestration uses a validated, indentation-scoped script language.
  Compile source to typed IR/bytecode; execute it with resumable coroutines.
- Script waits are explicit. A script owns input when its current instruction
  requires exclusive control.
- Preserve player-visible compatibility intentionally. Do not reproduce unsafe
  shared-memory accidents to obtain incidental bugs.

## Verification

- Core runtime tests use small original fixtures and no cartridge.
- Importer tests may use a locally supplied supported ROM and must skip clearly
  when it is absent.
- Render smoke checks must terminate automatically and must not change desktop
  display settings.
