# Full campaign implementation contract

## Objective

Pokemon Red Modern must boot as a native application, accept keyboard and
controller input, present the imported intro/title/menu/new-game flow, and run
the complete Pokemon Red campaign from a locally imported canonical ROM.

The engine is reusable Pokemon RPG machinery. Pokemon Red names, maps, scripts,
tables, layouts, animation programs, audio programs, and compatibility behavior
come from the importer-generated content pack. The engine must not contain a
second handwritten copy of Red's campaign.

This document is the authoritative integration and acceptance contract. The
domain schemas remain in:

- [CONTENT_SCHEMA.md](CONTENT_SCHEMA.md)
- [EXECUTORS_AND_ISA.md](EXECUTORS_AND_ISA.md)
- [CONTENT_PIPELINE.md](CONTENT_PIPELINE.md)
- [IMPORT_OUTPUT_AND_CACHE.md](IMPORT_OUTPUT_AND_CACHE.md)
- [PACKAGES_AND_PATCHES.md](PACKAGES_AND_PATCHES.md)

## Non-negotiable boundaries

- Do not spoof missing behavior with guessed dialogue, fake maps, placeholder
  scripts, or engine-side Red-specific branches.
- Do not silently discard unknown source records or opcodes.
- Do not call a pointer inventory an executable script import.
- Do not place Red map names, object IDs, warp destinations, trainer parties,
  encounter pools, or story conditions in the gameplay executable.
- Importers may contain verified ROM-profile knowledge needed to recover
  semantic content. Runtime code consumes typed, validated generated data.
- Every generated record retains source provenance and a stable symbolic key.
- Numeric fallback keys are allowed only while explicitly unresolved and must
  appear in completeness reports.
- A user-visible simplification must be documented and, when it changes
  original behavior, exposed through a behavior or enhancement setting.
- Renderer layout never decides gameplay topology, collision, trigger
  ownership, encounter membership, or script activation.

## Required launch and campaign flow

The normal application flow is:

```text
validate compiled campaign pack
    -> intro cinematic
    -> title animation and attract behavior
    -> main menu
        -> continue
        -> new game
        -> options
    -> Oak introduction and naming
    -> initial campaign state
    -> overworld
    -> menus / dialogue / encounters / battles / transitions
    -> ending and credits
```

The intro, title, menus, Oak scene, naming flow, and credits are imported flow,
text, layout, animation, graphic, and audio programs. The engine supplies
general flow, menu, dialogue, animation, and audio executors.

Typing is supported anywhere the original asks for a name. An on-screen naming
grid remains available for controller-only play. Text entry validates length
and the active campaign's supported glyph policy.

No debug laboratory or map viewer may substitute for the normal boot flow.
Developer tools can jump to content only through an explicit developer action.

## Controller and binding contract

Pokemon Red Modern follows the proven Tetris Modern and Super Mario Land Modern
controller shape:

- one local player with persistent Primary and Alternate binding profiles;
- semantic actions for up, down, left, right, confirm, back, start, select,
  menu, fast-forward, and application navigation;
- keyboard and controller bindings stored by Gubsy, editable independently,
  and saved immediately;
- automatic assignment of an unclaimed controller at startup and hot-plug;
- SDL add/remove handling without restarting the application;
- a controller-navigable binding editor with add, replace, remove, reset, and
  active-profile selection;
- no gameplay input leaking through while the binding editor owns input;
- controller label detection for Nintendo, Xbox, and PlayStation-style prompts;
- a manual **Rescan controllers** action.

The controller diagnostics view reports each discovery layer separately:

```text
browser gamepads
SDL joysticks
SDL mapped gamepads
Gubsy opened devices
assigned local-player devices
```

On WebAssembly, rescan first asks the user to press a controller button, then
refreshes the browser/SDL gamepad subsystem, refreshes Gubsy devices, and
reclaims an available controller. Native rescan refreshes SDL/Gubsy and
preserves valid assignments.

Default controller bindings use the D-pad, south face button for confirm/Game
Boy A, east face button for back/Game Boy B, Start, Back/Select, and left
trigger for fast-forward. Fast-forward defaults to hold behavior. The controls
enhancements expose enable/disable, hold/toggle, and multiplier settings. It
must not accelerate music playback.

Controller acceptance covers title-to-campaign flow, every semantic action,
hot-plug, detach, rescan, edited bindings, both profiles, persistence after a
fresh runtime, and controller-only menu/name/save operation.

## Exhaustive import contract

The importer must produce readable normalized source, compiled runtime data,
provenance, references, and accountability reports for every reachable domain.
At minimum:

### World and campaign

- all 248 map slots classified as active, alias, unused, or invalid;
- all 226 active map headers, including outdoor maps, interiors, gates, caves,
  dungeons, ships, gyms, special rooms, and ending spaces;
- block grids expanded to complete cell/tile grids;
- tilesets, collision roles, ledges, counters, water, doors, mutable blocks,
  cut trees, and other field interaction roles;
- connections, warps, destinations, arrival positions, facing, transition
  style, and map-load behavior;
- background events, step triggers, trainer sight, scripted approach regions,
  movement paths, roaming policies, visibility predicates, and toggle tables;
- wild encounter regions, rates, encounter table IDs, slot weights, levels,
  land/water/fishing distinctions, and conditional pools;
- actor definitions and placements, trainer/item/static-Pokemon parameters,
  script ownership, and story-state visibility;
- every campaign script entry, reachable branch, call target, flag, variable,
  text owner, menu owner, and operation dependency.

### Rules and inventory

- species, internal species slots, base stats, types, growth, experience,
  learnsets, evolution, dex ordering, and dex text;
- moves, PP, power, accuracy, type, priority, effect programs, animation, and
  audio bindings;
- statuses, volatile effects, stat stages, immunities, type interactions, and
  capture data;
- items, prices, shops, marts, machines, field uses, battle uses, key-item
  behavior, and storage rules;
- trainers, classes, parties, AI programs, rewards, gifts, trades, static
  encounters, and encounter tables;
- party, PC, Pokédex, bag, money, badges, options, play time, and save schema.

### Presentation and audio

- fonts, glyphs, localized strings, substitutions, choices, naming alphabets,
  pagination hints, and text sounds;
- overworld sprites and clips, Pokemon front/back pictures, trainer portraits,
  icons, tiles, tilemaps, palettes, masks, particles, and effects;
- title, intro, Oak scene, naming, world, battle, healing, travel, ending, and
  credits layouts/animations;
- menus and UI layouts for start, party, bag, moves, summary, Pokédex, shops,
  PC, battle, Safari, naming, options, and save prompts;
- music, sound effects, cries, instruments, envelopes, channel programs,
  priorities, and semantic event bindings.

Every table has a source-range accounting record. Every cross-reference is
resolved to a typed ID or listed as unresolved. Every generated index reports
record count, referenced count, unreferenced count, alias count, and missing
binding count.

## Semantic naming

Generated source is intended to be read and edited. The importer therefore
performs a deterministic semantic naming pass after the ownership graph is
known.

Naming evidence is applied in this order:

1. story/script/trainer/item constant or unique role;
2. unique owned text and map association;
3. unique event parameters and map role;
4. descriptive map-local fallback.

Examples:

```text
map pallet_town
actor pallet_town_technology_guy
warp reds_house_1f_exit
trigger pallet_town_oak_warning
encounter_table route_1_land
script oak_stops_player
```

Raw ROM IDs remain provenance fields. They are not primary authored names.
Renaming updates references atomically. Completion requires zero unexplained
numeric-only keys in active campaign content.

## ISA coverage and anti-spoofing gate

Structural decoding and semantic execution are different completion states:

```text
located
decoded_structure
decoded_data_language
semantically_lifted
compiled
runtime_executed
verified
```

Reports count records in every state. A record may not skip directly from an
address to `verified`.

Campaign-specific choices and sequences live in imported scripts. General
mechanics live in reusable executors. When a routine is unusual:

1. lower it using the existing semantic ISA;
2. add a small general operation if the missing concept is reusable Pokemon
   machinery;
3. add a typed content record if the behavior is data;
4. document a genuinely exceptional native operation and all callers.

An opaque `dynamic_native_script`, unresolved branch, ignored text command,
unknown animation instruction, missing audio opcode, or guessed destination is
a visible blocker. The game must fail validation for required campaign content
rather than pretending the behavior works.

Reachability and ownership reports must prove coverage for:

- campaign, predicate, movement, transition, dialogue, menu, encounter, battle,
  battle-effect, AI, animation, and audio programs;
- every script call and operation;
- every trigger/actor/text/menu binding;
- every move/item/trainer/encounter presentation binding.

## World spaces, groups, and placements

A `MapDef` owns local geometry and local cells. A `WorldSpaceDef` groups maps
that share one continuous gameplay coordinate system and camera surface.

```text
world_space kanto_surface
    environment outdoor

map_placement pallet_town
    world_space kanto_surface
    origin 0 0
    layer 0

map_placement route_1
    world_space kanto_surface
    origin 0 -18
    layer 0
```

Examples of separate spaces include the Kanto surface, a connected cave
system, a building complex, the S.S. Anne, or a dungeon whose connected maps
form a useful continuous topology. Floors are not automatically spaces.
Connectivity and non-overlapping placement determine the grouping; a
multi-floor complex may use one space when its maps can be meaningfully placed,
or several spaces joined by warps when it cannot.

Each placement has:

- `WorldSpaceId`;
- gameplay origin in world cells;
- optional layer/elevation;
- deterministic placement provenance;
- optional generated-source override.

The importer first derives placements from map connections and warp topology.
Contradictions, overlap, disconnected components, or layouts that cannot be
derived are reported. A local generated placement-override source may assign a
space and offset. The engine never contains map-name offset switches.

Gameplay placement and overview presentation are distinct. A debug/all-maps
atlas may splay disconnected spaces using presentation-only offsets. Those
offsets never affect actor coordinates, collision, triggers, encounters, or
warps.

The complete connected current world space is the default gameplay view. The
camera follows the player in that view at every zoom, including when zoomed far
enough to see the complete space. Selecting a single-map inspection view is a
developer action, not the normal campaign presentation.

Maps joined by authored edge connections occupy one continuous space. Walking
across the edge is ordinary continuous movement; it must not invoke a warp,
fade, loading transition, or player relocation. The destination map's triggers,
encounters, palette, music, and map-entry policies update at the deterministic
boundary without breaking visual interpolation.

A cave or dungeon may place all of its connected floors in one world space,
with importer-derived or source-authored offsets that splay the floors without
overlap. When the player zooms out, every floor in that space can be visible
simultaneously. The camera remains centered on the player's interpolated world
position rather than the bounding rectangle's center. If the topology cannot
form one useful non-overlapping space, the importer reports that fact and
creates explicit warp-connected spaces instead of guessing.

Only the current world space is simulated and drawn as the active gameplay
surface by default. Campaign semantics do not depend on screen streaming or
cartridge activation distance. All active actors within the current space
remain resident.

## Camera director

Manual pan/zoom, player following, and content-directed framing are distinct
camera inputs resolved by a general camera director.

Default behavior is:

- draw the complete current world space;
- follow the player's smooth presentation position;
- preserve the user's chosen zoom across connected-map boundaries;
- center on the player even when the entire space fits on screen;
- never block, pause, or take movement input merely to reframe the camera.

Content may define optional camera regions and sequences:

```text
camera_region pallet_town_entry
    world_space kanto_surface
    area 0 0 20 18
    enter_animation pallet_town_reveal
    automatic_zoom 2.0
    only_when automatic_framing_enabled
    unless manual_zoom_active

camera_animation pallet_town_reveal
    parallel
        tween_zoom 0.75 40 ease_out
        follow player
```

This permits a town entrance to reveal the surrounding area and settle toward
the player, or a long narrow route to choose a useful default framing. Camera
programs are imported/authored content, not map-name switches in C++.

Automatic framing is an enhancement and respects manual intent. Once the user
changes zoom, automatic zoom changes stop until the user resets the camera,
enters a policy that explicitly allows reframing, or enables an “always frame
areas” setting. Camera animations continue concurrently with player movement
unless an independent campaign script intentionally locks input.

## Movement presentation and clips

Logical collision remains fixed-step and cell based. Presentation movement is
smooth and cannot snap or pause between cells or connected maps.

The visual asset import and animation indexes include:

- directional idle and walk clips for the player and every mobile NPC;
- alternating walk-foot phases;
- run/bicycle/surf/fishing clips where applicable;
- ledge, door, stair, ladder, hole, warp, spin, bump, and field-action clips;
- trainer approach, scripted path, following, and synchronized movement clips;
- grass, water, terrain, healing, and other movement-coupled effects.

Walking animation phase follows distance traveled, not render-frame count.
Fixed-step positions retain previous/current transforms and the renderer
interpolates them using the unscaled presentation clock. Scripted movement and
ambient NPC movement use the same movement operation and clip selection as
player movement. Entering a connected map cannot reset the foot phase or cause
a one-cell visual stop.

## Spatial indexes and triggers

Runtime lookups do not scan every loaded record.

Each map/space compiles immutable spatial data to cell buckets or a compact
cell-range index:

- terrain and collision role;
- one or more background/step/script triggers;
- warp IDs;
- encounter-region IDs;
- connection/edge data;
- trainer sight and approach metadata.

Mutable occupancy maps cells to actor IDs. Movement changes only the source and
destination buckets. Roaming actors use a deterministic timing wheel or
equivalent scheduler, so a fixed step evaluates only actors whose decisions
are due.

Rectangular and polygonal authored regions are rasterized or compiled to
intervals during pack compilation. Runtime queries inspect the player's
current, destination, or faced cells and then evaluate the small candidate set
with typed predicates.

## Warp and transition contract

Warps are content records, not coordinate special cases:

```text
warp reds_house_1f_exit
    source_map reds_house_1f
    area 2 7 1 1
    destination_map pallet_town
    destination_spawn reds_house_door
    facing down
    transition door
```

Stepping onto an enabled warp:

1. completes the current movement operation;
2. locks player input;
3. evaluates the warp predicate;
4. starts the imported transition animation/audio;
5. resolves the destination map, space, spawn, and facing;
6. activates/deactivates world spaces and resident actor sets;
7. places the player outside destination collision;
8. runs destination map-load and arrival scripts;
9. updates music only when the imported music policy requires it;
10. suppresses immediate retrigger until the player leaves or the transition
    explicitly allows chaining;
11. unlocks input after all required operations complete.

Doors, stairs, ladders, elevators, holes, teleporters, ships, Fly, Dig, Escape
Rope, blackout, and scripted relocation use the same transition executor with
typed transition policies.

Connections are not warps. A connection transform only changes map ownership
inside the same continuous world space; it does not run this transition
sequence.

## F3 world and script diagnostics

F3 enables the complete world-debug overlay. Individual layers are also
selectable in F2:

- map bounds, map ID/key, world-space ID, placement origin, layer, and active
  state;
- connection edges and coordinate transforms;
- collision cells, ledges, counters, water, doors, cut trees, and mutable
  blocks;
- warp regions, IDs, predicates, destination map/space/spawn, and transition
  policy;
- background interactions, step triggers, script triggers, and current
  predicate result;
- NPC positions, facing, collision, movement region, active script, current
  instruction, wait condition, and source line;
- trainer sight regions, approach paths, defeated state, and battle binding;
- wild encounter regions, encounter table key/ID, rate, conditional result,
  and expanded species/level/weight pool;
- active campaign fibers, input locks, operation IDs, trigger suppression, and
  transition state.

Labels remain readable at normal zoom and collapse to colored regions/anchors
at whole-space zoom. Expensive text is culled outside the camera. The overlay
reads state and never mutates campaign behavior.

## Compatibility and enhancement policy

Original defects are not silently fixed. Behavior settings provide:

```text
Original
Fixed
Custom
```

`Original` enables intentionally reproduced Gen 1 behavior where the semantic
engine can represent it safely. `Fixed` enables the documented correction set.
`Custom` exposes every fix independently. Unsafe memory aliasing or corruption
is not recreated; an equivalent visible behavior may be implemented explicitly
when valuable and safe.

Compatibility toggles include move-effect mistakes, type-chart mistakes,
critical-hit/stat behavior, status interactions, AI mistakes, encounter/table
mistakes, and other documented campaign-visible defects. Each toggle names the
original behavior, fixed behavior, affected records, save/network relevance,
and verification scenario.

Optional enhancements are categorized separately:

- **Controls:** binding profiles, fast-forward, hold/toggle behavior.
- **Display:** palettes, smooth movement, camera, zoom, world-space view.
- **Interface:** typed naming, text speed, simultaneous information panels.
- **Bag and party:** categories, sorting, filtering, search, richer summaries.
- **Convenience:** faster transitions, remembered cursors, optional prompts.
- **Content:** version-exclusive reconciliation, expanded encounters, optional
  trade-evolution alternatives.
- **Accessibility:** contrast, flashes, animation speed, input assists.

Original-style menus remain available. A modern bag may show categories,
filters, search, and more information simultaneously rather than reproducing
the Game Boy's nested boxes. Enhancements alter presentation or explicitly
selected rules; they do not cause engine-side Pokemon Red content hardcoding.

## Save and deterministic behavior

The semantic save records:

- campaign pack identity and schema version;
- behavior preset and compatibility toggles that affect rules;
- player/world/actor/script state;
- party, storage, bag, Pokédex, badges, flags, variables, and mutations;
- play time, options, and deterministic RNG streams.

The runtime maintains separate clock domains:

- `game_time` is deterministic simulated time. Fast-forward advances it by the
  selected multiplier and gameplay timers use it.
- `real_time` is monotonic unscaled play/session time. It ignores
  fast-forward, is retained for statistics and diagnostics, and is never used
  to change deterministic campaign outcomes.
- `presentation_time` is unscaled rendering/interpolation time.
- `audio_time` is unscaled device/sequencer time, so music tempo and pitch do
  not change under fast-forward.

Fast-forward increases fixed simulation steps per real second rather than
changing the duration of a simulation step. Audio events produced by those
steps remain ordered, while music continues on `audio_time`. Settings and
debug tools display both game and real elapsed time.

UI-only settings and bindings live in host settings, not campaign saves.
Original save import/export is a separate adapter with explicit loss reporting.
Future local native link features require compatible campaign/rule manifests.

## Implementation milestones

### 1. Controls

- port the proven Gubsy semantic binding, profiles, persistence, hot-plug,
  rescan, diagnostics, and controller-only menu flow;
- add focused virtual-controller acceptance.

### 2. Complete imports and accountability

- import all active map geometry and every domain listed above;
- finish ownership joins and semantic naming;
- emit readable source, compiled indexes, provenance, and gap reports;
- reject required unresolved content.

### 3. World spaces and warps

- compile map placements into named spaces;
- support placement overrides without engine hardcoding;
- load/render/simulate interiors, caves, dungeons, and special rooms;
- default to a player-following complete-space view and support simultaneous
  cave/dungeon floor layouts;
- preserve smooth movement and directional clips across connected-map edges;
- implement non-blocking, content-authored camera regions and sequences;
- implement transitions and all warp policies.

### 4. Executors and campaign programs

- complete campaign/predicate/movement/dialogue/menu/transition/encounter
  executors;
- complete battle, unusual move, item, AI, animation, and audio coverage;
- eliminate reachable untranslated native script placeholders.

### 5. Normal boot flow

- import and execute intro, title, main menu, options, continue/new game,
  Oak/naming, and initial campaign setup;
- make normal startup the default rather than a laboratory view.

### 6. Full campaign wiring

- connect story scripts, trainers, encounters, items, HMs, field actions,
  battles, capture, leveling, evolution, shops, healing, save/load, ending, and
  credits;
- complete controller play from boot through Hall of Fame and optional areas.

### 7. Debugging and enhancements

- finish F3 overlays and F2 inspector;
- add behavior presets, granular compatibility fixes, and categorized optional
  enhancements;
- add left-trigger fast-forward with separate game, real, presentation, and
  audio clocks;
- add modern typed naming, bag filtering/search, and richer information views.

### 8. Verification and release gates

- zero required unresolved records, references, programs, bindings, or unknown
  source ranges;
- importer determinism and clean-cache rebuild;
- focused executor and content-accountability tests;
- virtual and physical controller acceptance;
- campaign checkpoints covering every map/space, warp type, story branch,
  trainer class, encounter kind, item/field action, and special battle effect;
- complete new-game-to-credits playthrough plus optional content;
- save/load and original-save adapter tests;
- debug/release/sanitizer/WASM builds;
- clean checkout requiring only the user's supported ROM.

## Playable acceptance

The project is not “wired up” merely because maps render or dialogue boxes
open. Completion means a user can:

- launch into the real intro/title/menu flow;
- use a controller without touching the keyboard;
- create names, choose a starter, and complete Oak's opening scripts;
- walk seamlessly across connected maps and warp through every required
  building, cave, dungeon, ship, gate, and special space;
- retain smooth directional walking animation while the complete current world
  space follows the player at arbitrary zoom;
- hold left trigger to accelerate gameplay without accelerating music and
  inspect both scaled game time and unscaled real time;
- inspect party/Pokédex/bag/options, buy/use/store items, save, and continue;
- encounter, battle, capture, train, evolve, switch, heal, and manage Pokemon;
- obtain badges and HMs, use field actions, complete story gates, defeat the
  Elite Four, enter the Hall of Fame, and reach credits;
- visit and complete intended optional content;
- select Original, Fixed, or Custom compatibility behavior;
- enable optional modern interface and convenience enhancements;
- press F3 anywhere and understand the active world, triggers, scripts,
  trainers, warps, and encounters.

Anything missing from this path remains an explicit tracked gap.
