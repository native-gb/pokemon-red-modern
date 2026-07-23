# Content follow-up tasks

## Semantic naming pass

- [ ] Replace every remaining numeric-only generated record key with a stable
  semantic snake_case key.
- [ ] Name actors only after joining their map object record to scripts, text,
  trainer/item parameters, visibility toggles, and story role. A sprite name is
  evidence, not an actor identity.
- [ ] Cover maps, actors, warps, background events, triggers, text resources,
  movement paths, trainer parties, encounters, shops, audio, effects, and
  scripts.
- [ ] Retain the original ROM ID, map-local index, source ranges, and importer
  decoder beside every semantic key.
- [ ] Emit explicit aliases when several source records intentionally describe
  the same semantic resource.
- [ ] Keep a deterministic fallback key for unresolved records and list every
  fallback in the importer completeness report.
- [ ] Reject duplicate semantic keys and unstable names during pack
  compilation.
- [ ] Update references across generated domains atomically so renaming a
  record cannot leave numeric or stale string references behind.

Naming evidence, strongest first:

1. a unique script, event, trainer, item, or story binding;
2. a unique text owner or explicit map-object constant;
3. a unique map role plus sprite and parameters;
4. a descriptive map-local fallback such as
   `viridian_city_actor_04`.

Do not identify an actor solely from shared artwork. For example, `sprite
youngster` may be used by many unrelated people.

## Script import follow-up

- [ ] Import every active map header before claiming complete script coverage,
  including interiors, caves, dungeons, and special rooms.
- [ ] Emit the ownership graph joining each map, actor, trigger, text entry,
  trainer, toggle, and script entry point.
- [ ] Decode actual data languages directly, including text commands, movement
  paths, choices, and pointer tables.
- [ ] Lift campaign control flow into the typed campaign ISA rather than
  embedding Game Boy instructions or emulating arbitrary CPU code.
- [ ] Represent unusual story routines as semantic scripts where the existing
  ISA is sufficient; add small reusable engine operations only for genuine
  mechanics.
- [ ] Emit an unresolved-script report until every reachable entry point is
  classified, translated, or intentionally assigned to a documented native
  operation.
- [ ] Compile readable script source plus overlays into the runtime cache so
  local edits do not require the ROM or C++ recompilation.

The first script milestone is ownership and entry-point completeness, not
immediate execution. It should make every authored program discoverable and
cross-referenced before the campaign VM begins running Pallet Town.
