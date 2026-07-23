# Imported source and compiled cache

## Goal

Cartridge decoding is a build step, not part of normal startup. The first
successful import produces:

1. a readable normalized source tree for inspection;
2. exhaustive catalog and provenance reports;
3. a compiled runtime pack and preprocessed assets.

Later launches validate a small manifest and load the compiled pack. They do
not decode the ROM and do not parse the full source tree.

## First import

```text
user selects supported ROM
    -> hash and header validation
    -> bounded ROM decoders
    -> typed unresolved records
    -> stable-key assignment
    -> readable source emission
    -> reference and completeness validation
    -> script/effect/animation/audio compilation
    -> atlas and map preprocessing
    -> atomic compiled-pack write
    -> atomic active-manifest update
```

The source ROM is read-only. A failed import leaves the previous valid pack
untouched.

The import implementation has two layers:

- a portable C++ core accepts ROM bytes and returns relative paths and byte
  buffers entirely in memory;
- host adapters obtain the ROM bytes and persist the returned files.

The desktop adapter is a separate command-line executable and performs a
transactional directory replacement. The WebAssembly host calls the same core
inside the game process after the browser supplies a selected file. Python and
runtime code generation are not part of either path.

## Local directory layout

Generated data is ignored by Git:

```text
data/runtime/imports/pokemon_red_us_rev_0/
    import_manifest
    source/
        manifest.sexpr
        boot/
            boot.sexpr
        text/
            pallet_town.sexpr
            battle_system.sexpr
        world/
            maps.sexpr
            connections.sexpr
            warps.sexpr
            triggers.sexpr
            actors.sexpr
        scripts/
            pallet_town.sexpr
            oaks_lab.sexpr
            viridian_city.sexpr
        battle_effects/
            damage.sexpr
            status.sexpr
            unusual_moves.sexpr
        encounters/
            probabilities.sexpr
            maps.sexpr
        animations/
            title.sexpr
            field.sexpr
            battle_moves.sexpr
        pokemon/
            species.sexpr
            learnsets.sexpr
            evolutions.sexpr
        trainers/
            classes_and_parties.sexpr
        moves.sexpr
        items.sexpr
        trainers.sexpr
        encounters.sexpr
        graphics.sexpr
        audio.sexpr
        ui.sexpr
        credits.sexpr
    reports/
        battle_rule_import_summary.txt
        boot_import_summary.txt
        encounter_import_summary.txt
        trainer_import_summary.txt
        completeness.txt
        unresolved.txt
        source_ranges.txt
        index_summary.txt
    compiled/
        battle_rules.bin
        boot_content.bin
        encounters.bin
        trainers.bin
        campaign_programs.bin
        world_maps.bin
        world_interactions.bin
        map_program_index.bin
        battle_animation_frames.bin
        battle_animation_procedural.bin
        battle_pictures.bin
        battle_ui_tiles.bin
        pack.bin
        pack.index
        source_map.bin
        graphics.atlas
        graphics.index
        audio.bin
        audio.index
```

The exact chunk format can change without changing the readable source syntax.
The current vertical slice loads `battle_rules.bin`, `boot_content.bin`,
`encounters.bin`, `pokemon_rules.bin`, `campaign_programs.bin`,
`world_maps.bin`, and `world_interactions.bin` directly. `battle_rules.bin`
contains validated semantic ordinary-damage, critical-hit, capture, and
experience-award formula programs, plus owned-Pokemon stat calculation and
ordinary accuracy/evasion calculation with the cartridge-owned stage ratios.
It also contains the executable ordinary-damage pipeline and all 24 direct
one-stage/two-stage stat-change programs. Unsupported source effects remain
explicit import gaps.
`boot_content.bin` contains normalized boot graphics, layouts, text programs,
title timing tables, and initial-placement content.
`pokemon_rules.bin` contains all imported type slots and interactions, species
stats and starting moves, move definitions and effect bindings, ordered
learnsets, evolutions, growth curves, and machine compatibility.
`encounters.bin` accounts for every one of the 248 source map slots, including
separate land/water rates and ten encounter slots when present. It also owns
the cartridge-derived ten-slot cumulative probability curve and terrain
selection policy. A resolved encounter is materialized as an owned Pokemon
through the imported stat, species, starting-move, and learnset rules before
the ordinary battle executor receives it.
`trainers.bin` owns all 47 trainer classes, their cartridge rewards and AI-use
profiles, and all 391 indexed parties containing 994 party members. It also
owns the imported actor-opponent encoding policy and the complete 190-slot
internal-species-to-Dex mapping. All 346 map-owned opponent actors resolve
through that policy: 334 select a trainer class and indexed party, while 12
select a static species and level. The generic runtime therefore contains
neither Pokemon Red's trainer offset nor Red trainer rosters.
`campaign_programs.bin` contains semantically lifted story fibers. The first
executable slice imports Pallet Town's Oak interception, both ROM dialogue
programs, Oak/player movement streams, the ordinary lab warp, Oak's two lab
actor placements, both lab entry paths, the four starter-choice speeches, and
their progression flags. Its readable peer is
`source/scripts/campaign/pallet_oak_interception.sexpr`. Map IDs, actor IDs,
flags, source addresses, paths, and text are owned by the generated pack; the
runtime implements only generic fiber and world-motion operations.
`world_interactions.bin` contains typed map-local interaction programs and
owner bindings. Its imported trainer-header index covers all 322 ordinary and
static trainer interactions, including sight range, defeated flag, and
before/end/after battle text programs. Header ownership is resolved separately
from the 346 opponent actor parameters because special static encounters can
use a flag bit that differs from their actor index. Normal startup does not
parse the readable `.sexpr` files.

## Readable generated source

Generated source is normalized semantic content, not an assembly listing:

```text
script oak_stops_player
    lock input
    face player up
    path oak oak_to_player
    wait operation oak
    say oak oak_dangerous
    set flag oak_met
    unlock input
```

Battle effects and animations are equally inspectable:

```text
effect counter
    when
        equal
            last_physical_damage target user
            0
        fail
        return

    deal_fixed_damage target
        multiply
            last_physical_damage target user
            2
```

```text
animation title_intro
    set_position logo 40 -56 native_canvas
    show logo
    tween_position logo 40 16 24 ease_out native_canvas
```

Source files retain comments containing useful importer provenance where it
does not overwhelm readability:

```text
; source pokered_us_rev_0 bank_05 0x4a12..0x4a2e
; recovered_from PalletTownScript0
```

The importer chooses stable formatting, ordering, and file grouping so two
imports of the same ROM and importer version produce identical text.

## Source maps

Compiled instructions map to:

- generated file and line;
- stable record key;
- original ROM profile and source range;
- importing decoder;
- overlay package and authored file when applicable.

Developer tools can open the readable source at the active instruction. A
breakpoint placed on a generated line resolves to compiled instruction ranges.

## Normal startup fast path

Normal startup reads:

1. active import manifest;
2. compiled-pack header and chunk hashes;
3. enabled effective-pack identity;
4. only the compiled chunks required for the initial mode.

It does not:

- reopen or hash the ROM on every launch;
- parse generated S-expressions;
- relink every symbolic reference;
- regenerate maps or atlases;
- rescan every package record.

The active manifest contains enough identity information to decide whether its
compiled pack is valid.

## Cache identity

The compiled-pack key includes:

- canonical ROM digest and source profile;
- importer version;
- readable-source schema version;
- compiler and ISA versions;
- pack binary-format version;
- compatibility layer hashes;
- enhancement layer hashes;
- enabled package hashes and ordering;
- localization and visual profile;
- atlas and audio preprocessing versions.

Changing only window settings, controls, volume, save data, or debug layout
does not invalidate the pack.

## Rebuild behavior

Rebuilds are explicit and transactional:

```text
import_rom
regenerate_source
compile_source
compile_effective_pack
verify_pack
```

If importer/schema changes require new canonical source, `regenerate_source`
uses the supported ROM again. If only overlays or a compiler version changed,
`compile_effective_pack` can reuse the existing generated canonical source
without decoding the ROM.

Normal startup never performs an expensive rebuild without telling the user.
It reports the invalidation reason and offers the appropriate action.

## Inspecting versus editing

The generated canonical source is an inspection artifact and may be replaced by
a later regeneration. Direct edits are therefore not the normal modification
path.

To modify something:

1. copy the relevant generated record or program into a local package;
2. change it using `define` or `override`;
3. compile the effective pack;
4. retain the canonical generated source unchanged for comparison.

Developer mode may offer an explicit `compile_source` command for experiments.
It is never silently run during normal startup.

## Completeness reports

Import is successful only when the report accounts for:

- every expected source domain and record count;
- every pointer and owned ROM range used by supported decoders;
- every script entry point and recovered branch target;
- every cross-domain reference;
- every move effect and animation binding;
- every map, warp, trigger, actor, trainer, encounter, and text owner;
- every graphic and audio program;
- intentional aliases and unused source regions.

The source tree is useful for human review; the completeness report is what
proves that the importer did not quietly omit content.

## Distribution boundary

Generated canonical source, reports, compiled packs, atlases, and audio chunks
remain local and ignored by version control. The public repository contains the
engine, importers, schemas, compilers, validators, and original fixtures.
