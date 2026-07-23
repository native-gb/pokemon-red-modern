# Code style

## Prefer transparent code

- Use value types, enums, spans, vectors, and explicit ownership.
- Prefer free functions operating on visible state over deep class hierarchies.
- Introduce an abstraction only when it removes a real repeated policy or
  establishes a domain boundary.
- Keep `main.cpp` as the readable composition root.
- Avoid hidden I/O, mutable globals, service locators, and generic event buses.

## Name the game concept

Use `MapId`, `ActorId`, `SpeciesId`, `MoveId`, `ItemId`, `ScriptId`, and
`TextId`, not interchangeable integers. Use names such as `Warp`, `Encounter`,
`PartyMember`, and `ScriptFiber`; do not expose cartridge-era names unless they
are necessary in importer provenance.

## Separate definition from state

Immutable definitions live in the content pack. Mutable instances live in game
state. An NPC definition describes appearance, collision, and script bindings;
an actor instance stores current map position, facing, movement, and active
fiber.

## Errors and validation

- Parse untrusted files into temporary values.
- Accumulate actionable validation diagnostics with domain and source context.
- Publish a content pack only after all indexes and cross-references validate.
- Do not silently substitute missing content in release builds.
- Runtime invariants may assert; user/import errors return diagnostics.

## Files and functions

- One module should have one cohesive reason to change.
- Split naturally around 300–500 lines.
- Keep functions short enough that sequencing is visible, but avoid wrappers
  that merely rename one call.
- Headers expose domain vocabulary and contracts, not implementation machinery.
- Include the corresponding header first.

## Comments

Comments explain ownership, invariants, sequencing, provenance decisions, and
non-obvious compatibility behavior. They do not restate syntax. A compatibility
quirk includes the source evidence and the intended player-visible result.

## Testing pace

During active playtest bug fixing, build incrementally and run only the focused
check relevant to the patch. Exhaustive importer, campaign reachability, and
asset audits run at explicit checkpoints or in CI.
