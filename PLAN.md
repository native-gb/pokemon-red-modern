# Pokémon Red Modern plan

## Outcome

A fully playable, clean native implementation of Pokémon Red whose copyrighted
campaign content is imported locally from a supported cartridge. The engine
must also be suitable for original campaigns and mods without cartridge-era
rendering or memory constraints.

## Milestone 0 — foundation

- [x] Independent repository and incremental build.
- [x] Windowed GPU host with centered 10:9 view.
- [x] Full-screen F1 player and F2 developer layouts.
- [x] Flat source layout with gameplay isolated from concrete render and host
  APIs.
- [x] Architecture, code-style, distribution, and extraction plans.

## Milestone 1 — reproducible content pack

- [x] Define symbols, typed IDs, dense indexes, catalog records, and source
  diagnostics.
- [x] Parse the indented S-expression notation and resolve deterministic
  package overlays with provenance.
- [x] Compile and execute typed predicates and the first animation timeline
  instruction subset.
- [x] Load readable animation sources into an isolated two-battler visual lab
  with hot reload and deterministic iteration.
- Verify supported ROM identity and header.
- Define pack manifest, schema versions, cache keys, and provenance.
- Emit deterministic readable source for every imported content/program domain.
- Complete the semantic naming and cross-domain reference pass tracked in
  [docs/CONTENT_FOLLOW_UPS.md](docs/CONTENT_FOLLOW_UPS.md).
- Compile the readable source into a startup-ready runtime pack.
- Import text, species, moves, types, items, trainers, encounters, growth,
  maps, tiles, sprites, palettes, audio, and scripts into typed indexes.
- Materialize complete maps rather than retaining the cartridge streaming
  window.
- Add a pack inspector and exhaustive completeness report.

Exit: importing the supported ROM produces the same validated pack every time,
every known source domain is accounted for, and later launches load the
compiled pack without reparsing the ROM or source tree.

## Milestone 2 — Pallet Town vertical slice

- Title/new game/naming flow.
- World rendering, collisions, actors, warps, doors, and smooth movement.
- Text boxes, choices, menus, inventory, party view, and save/load.
- Script VM with blocking movement, dialogue, flags, inventory, and transitions.
- Oak encounter through starter selection.

Exit: a player can begin a game, reach the lab, choose a starter, save, quit,
reload, and continue.

## Milestone 3 — battle and progression

- Wild and trainer battle rules.
- Party stats, experience, levels, status, capture, switching, items, and AI.
- Battle animation timelines and audio events.
- Evolution, move learning, Pokédex, money, blackout, and healing.

Exit: the Route 1/Viridian loop is fully playable, including encounters,
capture, leveling, healing, and persistence.

## Milestone 4 — campaign completion

- All maps, NPCs, scripts, trainers, encounters, shops, gyms, badges, HMs,
  field moves, puzzles, events, Elite Four, ending, and credits.
- Link-compatible battle/trade is a later optional transport boundary; local
  single-player completion does not wait on it.

Exit: a clean save can complete the full campaign with importer completeness
and reachable-content audits passing.

## Milestone 5 — modern enhancements

- Color and animation profiles.
- Rebindable controls, independent text/game speeds, accessibility.
- Optional bug fixes and compatibility presets.
- Expanded availability/no-exclusive rules behind explicit settings.
- Original mod packages compiled into the same typed indexes.
