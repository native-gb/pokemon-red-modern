# Native GB Pokémon Red Modern

A clean native Pokémon RPG engine, with Pokémon Red as its first locally
imported campaign.

This repository is not an emulator and does not contain Pokémon Red game data.
It will verify a user-supplied supported cartridge, decode it into typed
semantic content, and cache that content locally. Runtime logic, rendering,
tooling, enhancements, and future authored campaigns remain independent of ROM
layout.

## Current milestone

The repository currently provides:

- a windowed 1280×720 GPU host with a centered 10:9 game view;
- ImGui player tools on F1 and full-screen developer tools on F2;
- fullscreen toggle on F11;
- a flat source tree with a concrete `render/` module;
- incremental Debug, Release, and sanitizer builds;
- architecture, coding, distribution, and extraction plans.

No cartridge is required for the scaffold to open.

```sh
./scripts/run.sh
./scripts/run.sh --tools
./scripts/run.sh --render-smoke
```

See [PLAN.md](PLAN.md) for the implementation order and
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the ownership model. The
campaign coroutine design is in
[docs/SCRIPT_LANGUAGE.md](docs/SCRIPT_LANGUAGE.md).
