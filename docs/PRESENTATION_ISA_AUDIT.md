# Presentation ISA coverage audit

This ledger separates three different claims:

- **decoded** means the importer can identify the cartridge content;
- **lowered** means generated source contains a typed engine instruction;
- **executable** means the normal campaign runtime performs and waits for it.

A pointer, readable label, renderer experiment, or developer-lab implementation
does not count as executable campaign coverage.

The goal is to find missing presentation dependencies from importer evidence,
not by asking a player to notice each one during a full campaign run.

## Current executable operations

| Domain | Executable campaign operations | Current content coverage |
|---|---|---|
| Dialogue | `say`, pages, confirm, choices | Imported static pages used by lifted campaign programs |
| Naming | imported grid, typed text, confirm/back/start | Player, rival, and Pokémon naming |
| Actors | show/hide, face, place, paths, parallel paths | Lifted campaign paths including continuous Oak lab entry |
| Timing | bounded tick waits and operation waits | Lifted campaign waits represented by current programs |
| Music | `set_music_scene`, `restore_map_music` | Oak interception starts `meet_prof_oak` before “Hey! Wait!” and restores lab map music after entry motion |
| One-shot audio | `emit_audio_cue` | Get Key Item fanfare; presentation cries use imported species IDs |
| Pokémon presentation | `present_pokemon` | Starter inspection opens the imported two-page Red Pokédex data screen |
| Battle handoff | trainer battle request plus result waits | Current lifted trainer programs |

`set_music_scene` stores a content-supplied semantic scene ID in world state.
It persists across a warp and map change. `restore_map_music` releases the
override so the ordinary imported map dispatch becomes active again. Neither
instruction contains a Pallet Town or Oak special case.

`present_pokemon` resolves its species against imported rules and pictures.
The runtime screen uses the imported Pokédex border graphics, classification,
height, weight, description pages, and cry. It is no longer a generic modal.

## Verified script-music census

A source census over the pinned Red disassembly currently finds:

- 9 `PlayMusic` calls;
- 14 `PlayDefaultMusic` calls;
- 19 `SFX_STOP_ALL_MUSIC` references;
- 63 total lines mentioning those operations or their music operands;
- 12 script owners.

The owners are:

```text
CeruleanCity
ChampionsRoom
OaksLab
PalletTown
PewterCity
PewterPokecenter
PokemonTower2F
Route22
SSAnne2F
SSAnneCaptainsRoom
SilphCo7F
VermilionDock
```

Only the Pallet interception lifetime is fully lowered and executable at this
checkpoint. The remaining owners must be decoded into semantic scene changes,
tempo variants, jingles, or stop/restore operations as appropriate. A working
map-music table does not close this item.

## Known missing or partial presentation dependencies

### Campaign audio

- rival meeting music and its alternate-tempo variants;
- Jigglypuff’s Pokémon Center song;
- Pokémon healing jingle ownership;
- Surfing and S.S. Anne departure music changes;
- Champion-room music changes;
- remaining scripted fanfares, cries, and sound effects;
- explicit stop, fade, tempo, and restore semantics where a simple scene
  replacement is insufficient.

### Campaign visuals

- script-owned fades, whiteouts, and palette transitions beyond normal map
  handoff;
- S.S. Anne departure and other world-object sequences;
- binocular and other native modal presentations;
- remaining healing-machine sequencing;
- cut, surf, strength, fishing, elevator, fossil, trade, evolution, and
  move-learning presentations;
- credits and ending flow;
- content-specific camera programs where ordinary player follow/fitted-area
  framing is not enough.

### Menus and interaction owners

- complete party selection and party-member targeting;
- item use, toss, deposit, withdraw, and shop transaction owners;
- the normal Start-menu Pokédex and options screens;
- PC, trade, daycare, prize, vending, elevator, and fossil menus that are
  decoded but not all executable;
- every interaction still reported as `dynamic_untranslated` or with an
  unresolved native routine dependency.

### Battle presentation

- complete result/effectiveness/status text ownership;
- move-learning and evolution handoff;
- all trainer intro/victory variants;
- exact battle transition selection;
- an importer-to-runtime audit proving every emitted animation signal and
  sound binding has a consumer.

## Importer accountability requirement

Each imported campaign program must eventually emit a dependency record for
every source operation that is not ordinary control flow or state mutation.
Every dependency has exactly one status:

```text
lowered
content_only
intentionally_simplified
unresolved
unreachable
```

`content_only` means the operation was fully represented by generated data and
needs no runtime instruction. `intentionally_simplified` requires a documented
modern behavior and, when player-visible, an enhancement/compatibility
setting. `unresolved` is a release blocker for reachable campaign content.

The generated coverage report must group unresolved dependencies by domain,
source owner, ROM provenance, and required executor operation. Import success
must not silently turn an unknown presentation command into ordinary dialogue
or a no-op.

## Acceptance rule

A campaign slice is presentation-complete only when:

1. its source operations have a zero-unresolved dependency report;
2. every lowered yielding operation has explicit completion behavior;
3. transitions restore music, input ownership, camera ownership, and actor
   visibility through normal state transitions;
4. the normal runtime path consumes the generated program;
5. focused playtesting confirms layout and timing after the structural audit
   passes.

Playtesting remains necessary for appearance and feel. It is not the primary
mechanism for discovering missing executor vocabulary.
