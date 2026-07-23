# Content pipeline and extraction roadmap

## Import contract

The public executable accepts a supported, user-supplied cartridge. Import is a
deterministic build:

```text
verify ROM
  -> identify version and language
  -> decode source domains
  -> normalize to semantic records
  -> emit readable deterministic source
  -> resolve typed references
  -> validate invariants and completeness
  -> compile scripts and animation tracks
  -> pack atlases and audio
  -> write compiled runtime pack atomically
```

The cache key includes the ROM digest, importer version, schema version,
enhancement profile, visual profile, and language. A mismatch rebuilds
the cache. The source ROM is never modified.

Normal startup loads the compiled pack. It does not reopen the ROM or parse the
generated source tree. The full lifecycle and directory layout are specified in
[IMPORT_OUTPUT_AND_CACHE.md](IMPORT_OUTPUT_AND_CACHE.md).

## Pre-extraction inventory

Before writing individual decoders, create a catalogue of every relevant ROM
range and assign it to a domain:

1. header, banking, checksums, version identity;
2. pointer tables and bank-local addressing rules;
3. character map, compressed text, names, dialogue, and UI strings;
4. tilesets, blocks, collision roles, maps, connections, warps, signs, objects,
   and coordinate events;
5. sprite sheets, portraits, battle sprites, fonts, UI frames, palettes, and
   animation data;
6. species, stats, types, moves, effects, learnsets, evolutions, growth,
   experience, and Pokédex data;
7. items, prices, shops, machines, field effects, and key-item bindings;
8. trainer classes, parties, rewards, AI, wild encounters, and special battles;
9. map scripts, NPC scripts, movement paths, flags, variables, and event entry
   points;
10. music, instruments, patterns, sound effects, cries, and animation cues;
11. title, naming, healing, travel, ending, and credits sequences;
12. save fields and persistent world-state mappings.

The inventory report records decoded count, expected count, source ranges,
unclaimed bytes, unresolved pointers, duplicate aliases, and validation errors.
“Looks complete” is not an acceptance criterion.

## Typed pack layout

One small manifest names schema versions and files. Large domains remain
separate so tools and mods can replace them independently:

```text
manifest
strings
world
actors
scripts
pokemon
moves
items
trainers
encounters
graphics
palettes
audio
ui
animations
credits
provenance
```

At load time, each domain becomes a typed index keyed by stable IDs. The runtime
does not read source text, JSON, ROM offsets, or asset files from arbitrary
paths during gameplay.

The generated source remains available locally for inspection, source-level
debugging, and copying records into explicit overlay packages.

## Maps

The importer expands the cartridge’s block/tile representation into complete
map geometry. Connections become explicit topology and coordinate transforms.
Warps and map transitions remain semantic objects. Runtime camera and view size
never determine which collision or actor data exists.

## Graphics and audio

Graphics import produces normalized images, sprite metadata, palette sets, and
GPU atlas descriptions. Runtime uploads atlases once and draws batched quads.
It does not composite 8×8 cartridge pieces every frame.

Audio import produces semantic instruments, notes, patterns, loops, effects,
cries, priorities, and channel policies. A clean sequencer/synth consumes that
data. Music and effects have explicit mixing rules so an effect cannot corrupt
an unrelated note channel.

## Provenance

Every decoded record retains:

- supported ROM identity;
- importer and schema version;
- source bank/range or source table;
- decoder name;
- normalization notes;
- optional reference labels.

Provenance is developer metadata. Runtime systems operate only on semantic
records and stable IDs.
