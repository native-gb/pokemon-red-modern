# Native GB Pokémon Red Modern

A clean native Pokémon RPG engine, with Pokémon Red as its first locally
imported campaign.

This repository is not an emulator and does not contain Pokémon Red game data.
It will verify a user-supplied supported cartridge, decode it into typed
semantic content, emit readable local source, and compile a runtime pack.
Normal launches load that pack without reparsing the cartridge or generated
source. Runtime logic, rendering, tooling, enhancements, and future authored
campaigns remain independent of ROM layout.

## Current milestone

The repository currently provides:

- a windowed 1280×720 GPU host with a centered 10:9 game view;
- ImGui player tools on F1 and full-screen developer tools on F2;
- fullscreen toggle on F11;
- a flat source tree with a concrete `render/` module;
- incremental Debug, Release, and sanitizer builds;
- snake_case symbols, dense typed IDs, content records, and indexes;
- the indented S-expression reader and canonical diagnostic printer;
- deterministic package `define`/`override` resolution with field provenance;
- a hot-reloadable, isolated battle-animation lab with two placeholder
  battlers and readable source fixtures;
- architecture, coding, distribution, and extraction plans.

No cartridge is required for the scaffold to open.

```sh
./scripts/run.sh
./scripts/run.sh --tools
./scripts/run.sh --render-smoke
./scripts/test.sh
```

See [PLAN.md](PLAN.md) for the implementation order and
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the ownership model. The
campaign coroutine design is in
[docs/SCRIPT_LANGUAGE.md](docs/SCRIPT_LANGUAGE.md). Detailed contracts:

- [content records and indexes](docs/CONTENT_SCHEMA.md);
- [executors and typed ISAs](docs/EXECUTORS_AND_ISA.md);
- [packages and patches](docs/PACKAGES_AND_PATCHES.md);
- [generated source and compiled cache](docs/IMPORT_OUTPUT_AND_CACHE.md).

The current visual development entry point is the
[battle animation lab](docs/BATTLE_ANIMATION_LAB.md).
