# Content schema

## Status

This document defines the in-memory content model that the importer, authored
packages, validators, executors, and developer tools share. It is a contract
for implementation, not a description of Game Boy memory.

All authored and generated identifiers use `snake_case`. Hyphenated identifiers
are rejected by the source compiler so one spelling convention survives from
source files through diagnostics and debug tools.

## Identity and indexes

Every content domain has a distinct typed runtime ID:

```cpp
template <class Tag>
struct Id {
    std::uint32_t value;
};

using MapId = Id<struct MapTag>;
using SpeciesId = Id<struct SpeciesTag>;
using MoveId = Id<struct MoveTag>;
using ScriptId = Id<struct ScriptTag>;
```

IDs are dense indexes used only by a resolved catalog. Source files and saves
refer to stable symbolic keys such as `map.pallet_town`,
`species.bulbasaur`, and `script.oak_leads_player`.

Each table is a transparent value:

```cpp
template <class IdType, class Record>
struct Index {
    std::vector<Record> records;
    std::vector<ContentKey> keys;
};
```

The importer and package resolver maintain a key-to-ID symbol table while
building the catalog. Gameplay receives resolved IDs and performs direct dense
lookups. Developer builds retain the symbol table and provenance sidecar.

## Catalog

`Catalog` is one master object containing many typed indexes, not one
heterogeneous record table:

```cpp
struct Catalog {
    Manifest manifest;

    Index<TextId, TextDef> texts;
    Index<FontId, FontDef> fonts;
    Index<ChoiceId, ChoiceDef> choices;

    Index<TerrainId, TerrainDef> terrain;
    Index<TilesetId, TilesetDef> tilesets;
    Index<MapId, MapDef> maps;
    Index<WorldSpaceId, WorldSpaceDef> world_spaces;
    Index<MapPlacementId, MapPlacementDef> map_placements;
    Index<ConnectionId, ConnectionDef> connections;
    Index<WarpId, WarpDef> warps;
    Index<TriggerId, TriggerDef> triggers;
    Index<ActorDefId, ActorDef> actor_defs;
    Index<ActorSpawnId, ActorSpawnDef> actor_spawns;
    Index<MovementPathId, MovementPathDef> movement_paths;

    Index<FlagId, FlagDef> flags;
    Index<VariableId, VariableDef> variables;
    Index<PredicateId, PredicateProgram> predicates;
    Index<ScriptId, ScriptProgram> scripts;

    Index<TypeId, TypeDef> types;
    Index<TypeInteractionId, TypeInteractionDef> type_interactions;
    Index<StatusId, StatusDef> statuses;
    Index<StatusImmunityId, StatusImmunityDef> status_immunities;
    Index<SpeciesId, SpeciesDef> species;
    Index<LearnsetEntryId, LearnsetEntryDef> learnset_entries;
    Index<EvolutionId, EvolutionDef> evolutions;
    Index<GrowthCurveId, GrowthCurveDef> growth_curves;
    Index<DexEntryId, DexEntryDef> dex_entries;

    Index<MoveId, MoveDef> moves;
    Index<BattleRulesetId, BattleRulesetDef> battle_rulesets;
    Index<DamageFormulaId, DamageFormulaProgram> damage_formulas;
    Index<CaptureFormulaId, CaptureFormulaProgram> capture_formulas;
    Index<ExperienceFormulaId, ExperienceFormulaProgram> experience_formulas;
    Index<BattleEffectId, BattleEffectProgram> battle_effects;
    Index<ItemId, ItemDef> items;
    Index<ItemEffectId, ItemEffectProgram> item_effects;
    Index<MachineId, MachineDef> machines;
    Index<ShopId, ShopDef> shops;

    Index<TrainerClassId, TrainerClassDef> trainer_classes;
    Index<TrainerPartyId, TrainerPartyDef> trainer_parties;
    Index<AiProgramId, AiProgram> ai_programs;
    Index<EncounterTableId, EncounterTableDef> encounter_tables;
    Index<EncounterSlotId, EncounterSlotDef> encounter_slots;
    Index<TradeId, TradeDef> trades;
    Index<GiftId, GiftDef> gifts;

    Index<ImageId, ImageDef> images;
    Index<SpriteId, SpriteDef> sprites;
    Index<SpriteClipId, SpriteClipDef> sprite_clips;
    Index<PaletteId, PaletteDef> palettes;
    Index<AnimationId, AnimationProgram> animations;
    Index<CameraRegionId, CameraRegionDef> camera_regions;
    Index<CameraProgramId, CameraProgram> camera_programs;

    Index<InstrumentId, InstrumentDef> instruments;
    Index<MusicId, MusicProgram> music;
    Index<SoundId, SoundProgram> sounds;
    Index<CryId, CryProgram> cries;

    Index<UiStyleId, UiStyleDef> ui_styles;
    Index<UiLayoutId, UiLayoutDef> ui_layouts;
    Index<MenuId, MenuDef> menus;
    Index<CreditsId, CreditsDef> credits;
};
```

The exact split may grow as real records demand it, but cross-domain references
must remain typed.

## Manifest and provenance

The manifest records:

- pack, schema, importer, compiler, and engine compatibility versions;
- source ROM identity and language;
- canonical campaign key;
- enabled compatibility and enhancement layers;
- compiled domain hashes and dependencies;
- readable-source generation version;
- source and compiled-pack timestamps;
- completeness summary.

Provenance is stored outside hot records:

```cpp
struct Provenance {
    ContentKey record;
    SourceProfileId source_profile;
    std::uint32_t source_bank;
    std::uint32_t source_begin;
    std::uint32_t source_end;
    TextFileId generated_file;
    std::uint32_t generated_line;
    PackageId supplying_package;
};
```

## Text and UI

Text records contain tokens rather than preformatted screen rows:

```text
text oak_dangerous
    speaker oak
    line "It's unsafe!"
    line "Wild POKEMON live in tall grass!"
```

Substitutions remain typed:

```text
text received_item
    line player_name " received "
    value item_name
    line "!"
```

`TextDef` owns tokens, fallback language, pagination hints, and canonical
layout metadata. Fonts, glyph maps, choices, naming alphabets, cursor geometry,
and menu layouts are separate records.

```text
choice starter_confirmation
    option yes text.yes
    option no text.no
    default no
```

## World records

The importer expands every map to a complete cell grid. Runtime camera size
does not determine which geometry, collision, actors, or triggers exist.

```text
map pallet_town
    width 20
    height 18
    tileset overworld
    environment outdoor
    encounter_table pallet_town_land
    music pallet_town_theme
    cells "maps/pallet_town.cells"
```

Connections are explicit coordinate transforms:

```text
connection pallet_town_north
    from pallet_town
    edge north
    to route_1
    destination_edge south
    offset 0
```

Maps that share continuous coordinates belong to one world space:

```text
world_space kanto_surface
    environment outdoor

map_placement route_1
    map route_1
    world_space kanto_surface
    origin 0 -18
    layer 0
```

Crossing a connection inside one world space is ordinary movement, not a warp.
Cave/dungeon floors may be splayed into non-overlapping placements in one
space and remain simultaneously visible. Separate spaces are joined by warps.
Gameplay placement is independent from any presentation-only all-spaces atlas.

Camera regions are optional content:

```text
camera_region pallet_town_entry
    world_space kanto_surface
    area 0 0 20 18
    program pallet_town_reveal
    only_when automatic_framing_enabled
    respect_manual_zoom true
```

Camera programs target a general camera executor. They never encode a map-name
switch in engine code and do not lock movement unless a campaign script
separately requests an input lock.

Warps, triggers, and actor placements are first-class keyed records:

```text
warp pallet_house_exit
    map reds_house_1f
    area 2 7 1 1
    destination pallet_town
    spawn pallet_house_door
    transition door

trigger pallet_oak_warning
    map pallet_town
    area 10 0 1 1
    when oak_not_met
    script oak_stops_player
    once false

actor_spawn pallet_oak
    map pallet_town
    actor oak
    position 10 6
    facing down
    movement_region pallet_town_authored_bounds
    visible_when oak_waiting_in_pallet
    interact oak_pallet_dialogue
```

`MapDef` stores ranges into owner-sorted connection, warp, trigger, and spawn
tables. Those ranges are generated after overlays resolve.

Movement regions use global cells and are optional. A roaming imported Red
actor receives its authored map as a default region. Stationary actors have no
region because they never request ambient movement. A package may define a
larger, disconnected, or multi-map region for an actor intended to travel
between map domains.

## Types and interactions

Types are data, including the Gen 1 physical/special classification:

```text
define type steel
    name text.type_steel
    damage_class physical
    color 168 168 192
```

The type chart is sparse with an implicit `1/1` default:

```text
define type_interaction steel rock
    multiplier 2 1

define type_interaction fire steel
    multiplier 2 1

define type_interaction poison steel
    multiplier 0 1
```

Multipliers are exact rational values. Dual types multiply both defending-type
interactions. Status immunities remain separate:

```text
define status_immunity steel poison
```

No executor may switch over a fixed list of type names.

## Battle, damage, and capture rules

A campaign battle ruleset resolves formula programs and tables explicitly:

```text
battle_ruleset pokemon_red_original
    damage_formula gen_1_original_damage
    capture_formula gen_1_original_capture
    experience_formula gen_1_original_experience
    stat_formula gen_1_original_stats
    accuracy_formula gen_1_original_accuracy
    type_interactions gen_1_type_chart
    critical_hit_profile gen_1_original_critical_hits

battle_ruleset pokemon_red_fixed
    inherit pokemon_red_original
    type_interactions gen_1_fixed_type_chart
    critical_hit_profile gen_1_fixed_critical_hits
```

Damage formula programs consume typed values such as level, power, attack,
defense, STAB, type multipliers, critical state, random roll, and ordered
modifiers. Capture formula programs consume species catch rate, ball profile,
HP, status, Safari factors, and deterministic RNG. They cannot access raw
memory or arbitrary engine state. Accuracy programs consume raw move accuracy,
attacker accuracy stage, target evasion stage, semantic bypass state, and
deterministic RNG; their complete stage-ratio table comes from campaign
content rather than an engine-owned list.

Move effect programs decide semantic behavior—damage, status, recoil, drain,
multi-hit, fixed damage, transform, metronome, and other effects—while formula
programs calculate their numeric results. Learnsets, starting moves, machine
compatibility, and evolutions remain separate indexed records rather than
per-species code.

## Species and progression

```text
species bulbasaur
    dex_number 1
    name text.bulbasaur
    primary_type grass
    secondary_type poison
    base_hp 45
    base_attack 49
    base_defense 49
    base_speed 45
    base_special 65
    catch_rate 45
    experience_yield 64
    growth_curve medium_slow
    front_sprite bulbasaur_front
    back_sprite bulbasaur_back
    cry bulbasaur
    dex_entry bulbasaur
```

Learnset entries and evolutions have their own stable keys:

```text
learnset_entry bulbasaur_leech_seed
    species bulbasaur
    method level_up
    level 7
    move leech_seed
    order 20

evolution kadabra_alakazam
    from kadabra
    to alakazam
    trigger trade
```

Evolution conditions compile through the shared predicate compiler:

```text
evolution golbat_crobat_example
    from golbat
    to crobat
    trigger level_up
    condition
        friendship_at_least 220
```

The first campaign does not define later-generation content; the schema simply
does not hardcode trade and level as the only possible conditions.

## Moves and battle effects

```text
move thunderbolt
    name text.thunderbolt
    type electric
    power 95
    accuracy 100
    pp 15
    effect thunderbolt_effect
    animation thunderbolt
    sound thunderbolt
```

`MoveDef` is declarative. Individual behavior belongs to a
`BattleEffectProgram`. Standard damage is still an effect program invoking
general native formulas; unusual moves do not receive hidden per-move C++
callbacks.

## Items, machines, and shops

```text
item potion
    name text.potion
    price 300
    pocket medicine
    effect heal_fixed
    parameter 20
    usable_in_battle true
    usable_in_field true

machine tm24
    item tm24
    teaches thunderbolt
    reusable false

shop viridian_mart
    entry poke_ball order 10
    entry antidote order 20
    entry parlyz_heal order 30
```

Shop entries may become first-class records if overlay requirements need
independent keys. The schema should prefer keyed children whenever users need
to patch one entry without replacing its siblings.

## Trainers and encounters

Trainer parties are authored records:

```text
trainer_party youngster_1
    class youngster
    ai ordinary
    reward_factor 1

    member rattata
        level 11

    member ekans
        level 11
```

Encounter slots are independently addressable:

```text
encounter_table route_1_land
    rate 25
    terrain grass

encounter_slot route_1_pidgey_common
    table route_1_land
    species pidgey
    level 3
    weight 51
    order 10
```

Static encounters, gifts, trades, fishing, and Safari encounters have explicit
records rather than overloaded encounter-slot flags.

## Graphics and animation

Graphics records describe normalized imported assets:

```text
sprite title_logo
    image title_logo_rgba
    pivot 80 24
    filtering nearest

sprite_clip player_walk_down
    frame player_down_left duration 8
    frame player_down_stand duration 8
    frame player_down_right duration 8
    frame player_down_stand duration 8
```

Pokémon view renderers own their layouts. They expose only the semantic targets
that an active animation is allowed to modify:

```text
title view
    title_logo
    version_label

battle view
    attacker
    defender
    battle_screen
```

The animation state contains transform and visibility overrides for those
targets plus temporary effects it explicitly spawns. It does not contain a
general scene graph and does not construct battle, title, world, or menu
screens.

## Audio

Music, sound, and cry programs contain normalized sequencer operations and
instrument references. Animation and scripts refer to semantic IDs:

```text
sound title_impact
    priority 80
    program title_impact_program

music pallet_town_theme
    tempo 112
    channel square_1 pallet_square_1
    channel square_2 pallet_square_2
    channel wave pallet_wave
```

Channel ownership and interruption policy are explicit content. A sound effect
cannot accidentally reinterpret a music channel's note data.

## Secondary indexes

The resolved pack precomputes:

- connections, warps, triggers, and actors by map;
- scripts by map, actor, trigger, and stable key;
- encounters by map and terrain;
- learnset entries and evolutions by species;
- trainer parties by owning actor or trainer key;
- type interactions by attacking/defending pair;
- status immunities by type/status pair;
- animation and sound bindings by move and event;
- text by language and fallback chain;
- provenance by record key.

These indexes are compiled products. They are never rediscovered by scanning
every record during the game loop.

## Validation

Catalog publication requires:

- unique keys and valid `snake_case` symbols;
- all references resolved to the correct ID type;
- all owner ranges sorted, contiguous, and in bounds;
- no duplicate type-interaction or status-immunity pairs;
- valid rational multipliers and nonzero denominators;
- map coordinates, warps, spawns, and collision grids in bounds;
- reachable script and animation control flow;
- bounded stacks, loops, calls, and persistent-state slots;
- complete move, item, trainer, encounter, UI, graphic, and audio bindings;
- no unresolved importer source ranges;
- declared overlay conflicts resolved.

Validation reports generated source file and line, package provenance, and ROM
source range when available.
