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

## Persistent implementation operating rules

Work under the full-campaign goal is implementation work, not another planning
exercise.

- Keep a runnable normal boot-to-campaign path and extend it continuously.
- Prefer end-to-end playable slices that connect imported content to real
  executors and presentation; do not stop after emitting inventories, structs,
  or debug laboratories.
- Treat the ROM, verified disassembly, generated provenance, and observable
  original behavior as evidence. Do not guess when the evidence is available.
- Make reasonable architecture decisions autonomously within this contract.
  Ask only when a choice would materially change product intent, distribution
  boundaries, or destructive external state.
- Resolve blockers through importer improvements, semantic lifting, or general
  executor operations. Do not hide them behind fallback dialogue or fake
  success states.
- Preserve a playable build while replacing temporary vertical-slice paths
  with the real flow.
- Use focused checks during active implementation. Run broad importer,
  controller, sanitizer, campaign, and clean-checkout gates at meaningful
  milestones rather than in every bug-fix hot loop.
- Commit at coherent milestones and push regularly to `origin/main`.
- Record remaining gaps and exact acceptance evidence after every major
  campaign milestone.
- Continue toward the complete playable acceptance section while safe,
  in-scope work remains; a completed subsystem is not a reason to stop the
  full-campaign goal.

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
  level-up learnsets, starting moves, machine compatibility, evolution methods,
  evolution requirements, dex ordering, catch rate, experience yield, and dex
  text;
- moves, PP, power, accuracy, type, damage class, target policy, priority,
  critical-hit policy, effect scripts, damage bindings, animation, and audio
  bindings;
- type definitions and the complete attacking/defending type-interaction
  matrix, including immunities and intentionally unused entries;
- stat, accuracy/evasion, critical-hit, ordinary damage, fixed-damage,
  multi-hit, recoil, drain, experience, and level/stat-growth calculation
  profiles;
- statuses, volatile effects, stat stages, immunities, badge modifiers, STAB,
  random-damage ranges, and other ordered damage modifiers;
- capture calculation programs and tables, including species catch rates, ball
  modifiers, current/max HP, status modifiers, shake/result thresholds, Safari
  catch/escape factors, bait/rock changes, and failure outcomes;
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

Calculation machinery is split cleanly: the engine supplies bounded arithmetic,
RNG, comparison, modifier-ordering, and battle/capture state operations; the
imported campaign ruleset selects data tables and compiled formula/effect
programs. Pokemon Red constants, lookup tables, special move behavior, and
formula variants do not become anonymous literals or species/move switches in
C++.

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

## Current campaign integration evidence

The first twenty-eight imported story fibers are executable from the ordinary
overworld. On the north edge of Pallet Town the opening fiber:

- evaluates the imported followed-Oak flag and locks player input;
- reveals Oak, presents both cartridge dialogue programs, and moves him to the
  player;
- consumes the ROM-derived Oak/player walk-to-lab paths and the normal
  Pallet-to-lab warp;
- runs Oak's second lab actor up the imported entrance path, toggles to his
  podium actor, and walks the player up the imported lab path;
- presents the four imported starter-choice speeches and records the two
  followed-Oak flags plus the choose-a-Pokemon gate.

Each of the three starter-ball fibers then:

- activates through the imported ball actor owner and requires the imported
  choose-a-Pokemon gate;
- shows the cartridge prompt through the generic choice executor and permits a
  side-effect-free refusal;
- decodes the player starter, rival counter-starter, and both ball actors from
  the cartridge program;
- removes the chosen ball, presents the imported energetic/receipt text, and
  constructs the level-five owned Pokemon through the imported stat,
  learnset, move, and growth rules;
- presents the ROM-decoded nickname question, then either preserves the
  species name or hands the newest party member to the generic controller and
  typed-input naming owner using the imported alphabet and ten-character
  limit;
- records player/rival starter variables, walks the rival through the
  ROM-derived route, removes his selected ball, and records the got-starter
  flag.

Each counter-starter has a corresponding imported lab-exit fiber. Crossing the
cartridge's y=6 challenge line:

- presents Blue's imported challenge and moves his actor to the player;
- selects the cartridge-derived RIVAL1 party through the imported
  rival-starter variable;
- pauses the campaign fiber while the ordinary owned trainer-battle executor
  runs to a real player-victory or player-defeat outcome;
- resumes with the matching imported result text, restores the party, and
  records the battled-in-Oak's-Lab flag;
- waits the imported delay, presents Blue's exit speech, executes the
  cartridge exit route including its player-x-dependent final step, and hides
  the rival actor.

Entering the Viridian Mart through its ordinary city warp now starts the
imported Oak's Parcel fiber. It:

- detects the map entry through a generic warp-owned trigger and locks input;
- presents both cartridge dialogue programs with campaign-name substitution;
- consumes the ROM simulated-joypad stream to walk the player beside the
  clerk and preserves the source three-frame delay;
- grants the decoded item ID and quantity through the generic inventory owner;
- records the decoded got-parcel event and returns control to the player.

Returning through Pallet's ordinary lab warp and activating Oak with the
parcel starts the imported request fiber. It:

- requires the battled-rival state and decoded parcel stack, locks input,
  presents Oak's complete delivery/thanks text, and removes the item;
- brings Blue in through the cartridge's below/above/side branch selected from
  the player's actual position, with normalized source placement and movement;
- presents the complete request and Pokédex dialogue with imported waits,
  removes both desk Pokédex actors, and records the Pokédex/parcel flags;
- switches the Viridian sleeping/standing old-man actors and configures the
  first Route 22 rival's three flags and visibility;
- reverses the selected Blue path, hides him, and returns input ownership.

Campaign initialization now consumes all 228 cartridge toggleable-object
records rather than a handwritten opening list. All 32 source-default-hidden
actors are hidden through generated map/actor references before play begins.

Walking into either source trigger cell on Route 22 now starts the first rival
encounter. The imported branch:

- selects the three-step or four-step approach from the player's actual row;
- selects the exact RIVAL1 party from the imported rival starter variable;
- presents the before-battle and outcome dialogue around the real trainer
  battle owner;
- advances the beat flag only after player victory, then executes the matching
  seven-step or ten-step exit path, hides Blue, and clears both source gate
  flags;
- exits without granting progression after player defeat.

After winning on Route 22, activating Oak imports his source check-and-set
event, grants the decoded stack of five Poké Balls, presents the complete
catching explanation, and switches later activations to his follow-up text.
Blue's House now has all three Daisy states needed for this opening:

- before the Pokédex, Daisy reports that Blue is at the lab;
- after the Pokédex, she offers the ROM-decoded Town Map item and hides its
  imported world actor only when the grant succeeds;
- at the imported 20-stack bag limit, she presents the full-bag response and
  leaves the event and actor untouched so the player can retry;
- after receipt, she presents the imported Town Map usage text.

Guarded map-presence programs now complete the opening bookkeeping. Blue's
House records its entered flag, Pallet records both post-Poké-Ball flags, and
returning after the Town Map sets Daisy's walking flag while replacing her
sitting actor with the imported roaming actor.

Route 1's Mart employee now runs his imported sample program: the cartridge
check-and-set event, Potion item tuple, receipt text, full-bag response, and
repeat Poké Ball advertisement all execute through the generic inventory
branch operations.

Loose world items are no longer an unresolved dynamic-native interaction.
The campaign pack contains every imported ordinary item name, semantic TM/HM
names, and the cartridge's shared found-item and full-bag pages. A generic
runtime transaction reads the activated actor's imported item ID, grants one,
hides the actor on success, and leaves it available when the imported bag
capacity rejects a new stack. This covers all three Viridian Forest items and
is the same path later maps use.

Pewter Gym is now a complete imported first-badge slice. Brock presents his
cartridge pre-battle pages and hands imported trainer class 34, party 0 to the
ordinary battle owner, producing level-12 Geodude and level-14 Onix. Defeat
ends without progression. Victory presents the BoulderBadge pages, then runs
the source TM34 transaction: full bags retain a retry state, while a later
activation grants TM34 and switches Brock to his post-battle advice. Both
badge-state bits, the gym trainer event, Route 22 event reset, and the two
source actor removals come from the imported program. The gym guide also owns
his cartridge YES/NO advice split and post-Brock response.

Pewter City's pre-Brock gates are now imported as campaign programs. The
museum guide owns the cartridge YES/NO branch, escorts the player to the
museum entrance after NO, presents the arrival page, exits, and returns to his
imported spawn. The roaming Repel NPC owns both cartridge answer branches.
The gym guide responds both to direct activation and to all four ROM-authored
east-exit cells, escorts the player to the gym entrance, presents the arrival
page, exits, and returns to his imported spawn. Brock's imported event disables
all five guide triggers and retains the existing permanent actor removal.

The modern executor does not reinterpret Red's simulated joypad frame counts
as tile counts. Generated content instead stores the imported actor owner,
trigger cells, map-warp destination, dialogue, exit stream, and event guard.
A generic `escort_player_to` operation finds passable local paths for the
player and guide and feeds them to the existing smooth scripted-motion owner.
The explicitly scripted `MoveSprite` exit retains its source ability to ignore
ordinary terrain collision; autonomous NPC roaming remains collision-bound.

Route 3 and Mt. Moon's ordinary content now use the exhaustive generic owners:
all eight Route 3 trainers, seven Mt. Moon 1F trainers, four B2F Rockets,
imported encounter tables, cave warps, and eight loose items resolve without
map-specific engine code. The B2F fossil sequence is lifted separately because
it is a real campaign gate. Walking onto the ROM trigger cell or activating the
Super Nerd starts imported class 8, party 1 (Grimer, Voltorb, Koffing); defeat
does not advance, while victory records the cartridge event and opens both
fossil choices.

Both Dome and Helix transactions use the imported item ID, quantity,
confirmation, receipt, full-bag response, event bit, toggleable actors, and
Super Nerd movement. Declining changes nothing. A full bag leaves both fossils
available. Success grants the selected fossil, moves the Super Nerd, presents
his final page, removes the unchosen fossil, and switches his later interaction
to the Cinnabar laboratory text. The cartridge's post-victory 4-by-4
no-encounter area is compiled as a content-authored suppression zone and
checked before ordinary encounter rolls.

The runtime operations are generic. The C++ ROM importer emits the actor/map
owners, flags, paths, text, choices, species, and readable
`source/scripts/campaign/*.sexpr` files plus `source/menus/naming.sexpr`, then
compiles the same records into `campaign_programs.bin`. A focused headless
check drives the slice through its real warp, declines once, accepts
Charmander, enters a typed nickname, executes the Squirtle rival battle through
the real battle engine, and verifies party construction, rival selection, ball
visibility, battle outcome, healing, rival exit, final map, and progression
state. The same check then uses the real Viridian City-to-Mart warp, advances
the parcel interruption, and verifies movement, inventory, event state, and
final placement. It returns through Pallet's real lab warp, walks the aisle,
activates Oak from below, and verifies parcel removal, the selected Blue path,
Pokédex progression, all affected actor toggles, and Route 22 gate state. It
then enters the exact Route 22 rectangle, verifies the imported two-Pokémon
RIVAL1 party, returns a battle victory to the campaign continuation, and
verifies Blue's path, visibility, and event mutations. It returns to Oak and
verifies the five-Poké-Ball stack, then fills the imported bag to capacity,
verifies Daisy's failure branch, frees space, retries, and verifies the Town
Map item, flag, and actor removal.

The same fixture then services the guarded Blue's House/Pallet updates and
verifies all three flags plus Daisy's actor swap. It activates the Route 1
clerk and verifies the imported Potion stack and event, then checks all three
Viridian Forest loose items, including full-bag retention followed by a
successful retry, name substitution, inventory stacking, and actor removal.
The Forest's three trainer actors are also required to resolve to imported
trainer-interaction records. It then drives Brock through the imported trainer
party, deliberately fails the TM grant with a full bag while retaining the
badge, frees a slot, retries TM34, and verifies the final advice state.
Before entering the gym, it also enters an exact imported east-gate cell,
advances the smooth city escort, verifies the imported gym-door destination,
and checks that the guide returns without leaving input locked.
It then accounts for all ordinary Route 3/Mt. Moon trainer bindings, enters the
exact B2F gate, verifies the imported three-Pokemon Super Nerd party, returns a
victory, verifies the conditional encounter-suppression zone, takes the Dome
Fossil, checks inventory and both actor removals, and verifies the Super Nerd's
post-choice response.

The Mt. Moon Pokecenter Magikarp transaction is now a fully imported campaign
program rather than a shop-shaped engine special case. The importer derives
the salesman owner, event bit, packed-BCD price, internal-species conversion,
level, and all result text from the verified ROM. Generic economy and Pokemon
storage state use the imported 3000 starting money, six-member party limit,
twelve boxes, and twenty-Pokemon current-box capacity. The transaction
preserves Red's ordering: insufficient money fails before creation, a full
party sends the Pokemon to the selected box, a full selected box charges
nothing and leaves the event clear, and success charges 500 exactly once.
Dynamic receipt text resolves the imported species and box-number fields.

The focused campaign fixture fills both the party and current box, accepts the
offer, and verifies the full-box text, unchanged money, and retryable event. It
then clears the box, retries, verifies the imported level-5 Magikarp in box 1
and the 3000-to-2500 money transition, and confirms the permanent no-refunds
branch. The readable source is generated at
`source/scripts/campaign/mt_moon_magikarp_sale.sexpr`.

Cerulean's bridge rival is also a compiled campaign gate. The importer derives
both adjacent trigger cells, the initially hidden Blue actor, approach and two
exit streams, battle/event state, four dialogue phases, RIVAL1 class, and all
three starter-dependent party indexes from the ROM. A generic
`place_actor_at_player_x` operation preserves the cartridge's two-column
entrance without Red-specific engine code; the existing smooth scripted-motion
owner runs the approach and side-dependent exit. Losing hides Blue without
setting the event so the encounter remains retryable. Winning sets the event,
shows Bill's invitation, runs the correct exit, and hides Blue.

The fixture enters the exact bridge cell after choosing Charmander, watches
Blue walk down, verifies imported class 25 party 6 as Pidgeotto, Abra, Rattata,
and Squirtle, returns a victory, and checks the event, hidden actor, and
unlocked input. All three branches are readable at
`source/scripts/campaign/cerulean_rival.sexpr`.

Cerulean Gym is now owned by imported campaign programs as well. Misty's actor
record supplies trainer class 35 and party 0; the reward code supplies the
beat-Misty and got-TM events, Cascade Badge and gym-completion bits, both
junior-trainer defeat bits, and the TM11 item tuple. Her pre-battle, badge,
badge-effects, TM receipt/full-bag/retry, and BubbleBeam explanation text plus
both gym-guide states are decoded from the ROM. The generic battle and
inventory executors preserve the original ordering: a loss advances nothing,
a victory always records the badge/gym state, a full bag leaves TM11
retryable, and a successful grant records the TM event.

The fixture starts Misty from imported map 65, verifies level-18 Staryu and
level-21 Starmie, returns a victory, checks every reward bit and TM11, and
confirms the guide's post-Misty branch. The readable program is generated at
`source/scripts/campaign/cerulean_gym.sexpr`.

Nugget Bridge's final reward and Rocket recruiter are now one imported
transaction with two content-authored entry points: the exact automatic
player cell and direct actor activation. The importer derives actor/trainer
ownership, class 30 party 5, the Nugget tuple, got-Nugget and defeated-Rocket
events, and all six dialogue programs. A full bag shows the ROM text, leaves
the event clear, and smoothly steps the player south once so the cell cannot
hot-loop. Success grants the Nugget before the pitch and battle, matching the
cartridge ordering; victory records the distinct Rocket event and switches the
actor to his final response.

The fixture enters map 35 at the imported cell, verifies the Nugget and event,
materializes level-15 Ekans and Zubat, returns a victory, and checks the final
event and unlocked state. Readable content is generated at
`source/scripts/campaign/route_24_nugget_bridge.sexpr`.

Bill's House is now a seven-program imported progression slice. The importer
derives all three map actors, the hidden PC target cell and required facing,
both alternative Pokémon-form movement streams, the transformed spawn and
exit stream, six related event bits, the S.S. Ticket tuple and item name,
Cerulean's two guard toggles, the Nugget Bridge recruiter toggle, and every
dialogue page from the verified ROM. No Bill coordinate, event, item, or actor
identity lives in the runtime engine.

Two reusable executor capabilities support the slice. `cell_activation`
records the map cell in front of the player plus the player's facing, allowing
ROM hidden events to remain ordinary indexed content.
`actor_path_by_player_facing` selects between imported paths, while
`place_actor_scripted` permits source-authored machine spawns on intentionally
impassable scenery without weakening collision for normal actor placement.
The Route 25 load behavior is preserved: leaving before using the separator
clears the pending event, restores Pokémon-form Bill to his imported spawn,
and permits a retry. Leaving after receiving the ticket records the completed
visit, removes the bridge recruiter and transformed Bill, and exposes Bill's
later rare-Pokémon interaction.

The focused fixture exercises both the interrupted and completed branches. It
accepts Bill's request, verifies his path and hidden actor, leaves to Route 25
and confirms the reset, repeats the request, activates the imported PC target
from the required direction, verifies the transformation and exit events,
receives item 63, checks both Cerulean guard states, leaves again, and verifies
all three final actor toggles and Bill's later dialogue. Readable content is
generated at `source/scripts/campaign/bills_house.sexpr`.

This checkpoint owns the progression logic, not all Bill presentation. The
teleporter's exact timed sound/visual sequence and the later four-entry
rare-Pokémon PC viewer remain explicit presentation/UI work; they must be
imported before the Bill slice is considered presentation-complete.

Cerulean's Rocket thief and TM28 recovery are now a six-program imported
progression slice. The importer derives both automatic trigger cells, the
direct actor owner, Rocket class 30 party 4, the defeated event, TM28 item
tuple and name, all three affected actor toggles, the trashed-house owner, and
all seven dialogue programs from the verified ROM. The engine receives only
generic player-cell, actor-activation, trainer-battle, inventory transaction,
flag, dialogue, and actor-visibility operations.

Both automatic approaches face the player and Rocket toward one another before
the challenge. A loss leaves the event clear. A victory presents the imported
give-up response and records the event before attempting the TM transaction,
so a full bag leaves the defeated Rocket available without replaying the
battle. A later activation retries the return, grants TM28 when space exists,
restores the first Cerulean guard, removes the replacement guard, and hides the
Rocket. The trashed-house owner independently selects his before/after response
from actual TM28 possession, matching the cartridge rather than relying on the
battle event.

The focused fixture enters the north automatic cell, verifies imported
level-17 Machop and Drowzee, returns a victory with a deliberately full bag,
checks the persistent event and visible retry actor, frees one stack, retries
TM28, verifies all three actor states, and confirms the house owner's
possession-dependent response. Readable content is generated at
`source/scripts/campaign/cerulean_rocket.sexpr`.

Route 5's Saffron guard and Vermilion's harbor sailor are now content-authored
traversal gates. The Route 5 importer derives both guarded cells, the guard
actor, the shared four-gate status bit, the ordered Fresh Water/Soda
Pop/Lemonade list, and all three responses. With no drink, the program presents
the cartridge response and smoothly moves the player north off the trigger.
With a listed drink, it consumes exactly one, presents the combined response,
and records the shared state; later actor activation uses the thanks response.
No drink ID or gate coordinate lives in the runtime.

The Vermilion importer derives the harbor cell, sailor actor, S.S. Ticket item,
S.S. Anne departure event, and five response programs. Entering without a
ticket presents both source responses and moves the player north. Entering with
Bill's ticket presents the check and acceptance, leaves the key item
unconsumed, and permits the ordinary imported dock warp on the next cell. The
sailor has separate before/after-departure actor responses.

The focused fixture proves the Route 5 no-item rejection, ordered drink
transaction, shared flag, and later guard response, then removes/restores item
63 to prove both Vermilion harbor branches and the ticket's persistence.
Readable content is generated at
`source/scripts/campaign/route_5_gate.sexpr` and
`source/scripts/campaign/vermilion_harbor.sexpr`.

The Route 5 Day Care is now a real semantic Pokémon service. The importer
derives the gentleman actor and all fifteen dialogue programs from the ROM.
The campaign program owns the offer, party-size check, controller party
selection and cancel path, HM-move rejection, deposit, grown/unchanged
responses, party-capacity check, fee offer, decline and insufficient-money
branches, payment, and return presentation. The engine owns only the reusable
Day Care state and operations.

A deposited Pokémon is removed from the party intact and gains one experience
point per completed player step, capped at level 100. Retrieval computes level
from the imported growth curve, charges `(levels grown + 1) * 100`, rebuilds
its four-move level-up set from imported learnsets, recalculates imported stats,
heals it, and returns it to the party. Hidden-machine rejection is derived from
the imported machine table rather than a C++ move list.

The focused fixture deposits a selected owned Pokémon, places it one walking
step below its next imported growth threshold, services that step, verifies
the dynamic one-level response, accepts the ¥200 return, and checks party
ownership, money, level, moves/stats refresh, and full healing. Readable
content is generated at `source/scripts/campaign/daycare.sexpr`.

Hidden items are now imported as one exhaustive global content domain rather
than four Underground Path exceptions. The importer walks all 85 hidden-event
map owners, follows their ROM pointers, selects the `HiddenItems` semantic
function, and cross-validates the resulting 54 records against the separate
cartridge coordinate/index table. Every record therefore owns a ROM map and
target cell, item ID/name, and the exact persistent bit in the 112-slot hidden
item flag array.

The runtime adds one generic any-facing target-cell trigger. A hidden item uses
the ordinary imported inventory transaction and shared ROM presentation: a
full bag leaves its bit clear and permits retry; success grants one item and
sets the coordinate-index bit. The focused fixture activates the first
north/south Underground Path cell, proves full-bag retention, frees a slot,
retries, and verifies the hidden Full Restore and flag. All 54 records are
readable at `source/scripts/campaign/hidden_items.sexpr`.

In-game trades are now a centralized ROM-driven content domain rather than
bespoke NPC code. The importer exhaustively decodes all ten 14-byte trade
records: requested and received internal species are normalized to Pokédex
species, names come from the cartridge's internal-order name table, and each
record retains its dialogue-set selector, nickname, and exact completion bit.
It also decodes all three five-response dialogue families plus the shared
cable and exchange presentation.

The engine adds only two generic party operations: branch when the selected
party member is not a requested species, and replace a valid selected member
with an imported received species at the offered member's level. Received
stats and moves use the imported rules, random DVs and trainer ID use the
deterministic world random stream, and nickname, original trainer, and
completion event come from imported content. Decline, cancel, wrong-species,
success, and completed-trade responses remain campaign-program branches.

The currently reached campaign slice binds the Route 5 Underground Path actor
to record 9 (male Nidoran for SPOT) and the Vermilion Trade House actor to
record 4 (Spearow for DUX), deriving both actor owners and trade selectors from
their map data and scripts. The focused fixture completes SPOT, checks the
replacement's species, nickname, level, original trainer, and exact event, then
verifies the actor changes to its imported after-trade response. The full
ten-record catalogue and two current bindings are readable at
`source/scripts/campaign/in_game_trades.sexpr`. The other eight records are
already decoded; their actor bindings will be added as their later maps enter
the playable campaign.

The critical S.S. Anne campaign path is now imported end to end. Blue's 2F
encounter derives its two trigger cells, hidden actor and spawn, three/four-step
approaches, both post-battle exits, RIVAL2 class and all three
starter-dependent parties, dialogue, and persistent map-script state from the
ROM. A loss hides and resets Blue for retry; a victory records the normalized
`wSSAnne2FCurScript` state, presents the Cut-master lead, runs the side-specific
exit, and removes him.

The captain is a retryable semantic inventory transaction. His actor, both
background interactions, HM01 tuple/name, got-HM and rubbed-back events, and
all seven text programs are imported. Rubbing his back records its event
before the item attempt as on the cartridge. A full bag retains HM01 and leaves
the reward event clear; a later activation retries, grants HM01, and changes
the captain to his healthy response.

Ship departure uses a generic exact warp-arrival trigger rather than a map
presence approximation. The importer cross-validates both ship doors, dock
warp, got-HM predicate, four contiguous departure events, and the source
three-plus-two automatic north-step counts. Exiting after HM01 records the
ship-left/started/walked-out/past-guard states and escorts the player through
the dock into Vermilion. This exposed and fixed a general return-warp defect:
maps sharing a connected render space can no longer overwrite `LAST_MAP`
merely because the combined space is outdoor; only a real cross-space outdoor
departure updates the return point.

The focused fixture materializes Blue's level-specific Pidgeotto, Raticate,
Kadabra, and Wartortle party, completes his imported exit, proves the captain's
full-bag retry and HM01 grant, then takes the actual ship warp and verifies all
four departure events and the final Vermilion coordinate. Readable content is
generated at `source/scripts/campaign/ss_anne.sexpr`.

The exact horn, smoke, scrolling ship-erasure presentation and optional
ship-room NPC conversations remain presentation/content-completion work; the
required rival → captain → HM01 → departure progression is executable.

The title-through-Brock playtest slice now seeds New Game's imported Pallet
Town `LAST_MAP` owner, so Red's upstairs-to-downstairs path exits the house
instead of returning upstairs. Decoded Pokémon Center nurses are executable
through one generic service owner using the five shared ROM text resources,
HEAL/CANCEL choice, full HP/status/PP healing, and a stored blackout
checkpoint.

All four standard battle command symbols now have live owners. FIGHT retains
ordinary imported move execution; RUN exits wild battles and rejects trainer
escape; ITEM selects an available ROM-bound Ball profile, runs the imported
capture formula, preserves the caught instance in party/current box, and
gives the enemy its turn after failure; PKMN selects another usable party
member and gives the enemy its switch turn. The modern Start overlay exposes
read-only party, bag, and trainer state during this playtest pass.

This is not full-campaign completion. The next campaign blocker begins with
the explicitly prioritized title-through-Brock playable acceptance pass.
Later map programs still require semantic lifting and the remaining acceptance
gates above stay open.

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
