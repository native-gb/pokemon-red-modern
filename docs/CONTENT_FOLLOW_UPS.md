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

- [x] Import every active map header before claiming complete script coverage,
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

The initial map-program inventory now classifies all 248 ROM map slots. It
decodes 226 active map headers and identifies the remaining 22 invalid/unused
slots without treating them as campaign content. The local generated tree
contains one provenance record per slot plus:

- 226 map load-script entry points;
- 1,126 text entries directly owned by map objects and background events;
- 202 background-event interaction bindings;
- 924 actor interaction bindings;
- header aliases and a complete unresolved translation queue.

This is structural coverage, not semantic script coverage. Text referenced
only from machine-code routines, trainer/event-flag ownership, movement paths,
and each routine's campaign-ISA translation remain open. The importer must
continue reporting those gaps explicitly.

The text-command decoder currently emits all 1,126 directly owned map text
programs under `source/text/maps`. Of these, 486 are complete data-language
programs with readable dialogue and formatting operations. The other 640 begin
with `text_asm`; they are indexed as `dynamic_untranslated` because their
branches and referenced dialogue must be recovered while lifting their native
routine. No entry is silently discarded or represented as decoded text when
only its address is known.
