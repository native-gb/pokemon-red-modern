# Connected world renderer

## Imported campaign world

The importer classifies all 248 Red map slots and decodes all 226 active maps.
That includes the 36 cities, towns, surface routes, sea routes, and Indigo
Plateau maps plus every active interior, gate, cave, dungeon, ship, gym, and
ending room. The 22 unused slots remain explicit in the import accounting and
never become synthetic maps.

The importer reads IDs, dimensions, block grids, tilesets, graphics,
connection transforms, warps, and object events from the verified local ROM.
It also decodes all 72 entries in Red's overworld sprite-sheet table into
normalized directional standing frames. A non-payload Red importer profile
assigns semantic map and sprite keys and validates every decoded header
dimension. The generic engine contains no list of campaign map names,
placements, NPCs, or warp endpoints.

The ROM table contains numeric sprite IDs but no symbolic names. The
Red-specific importer schema assigns stable readable keys such as `oak`,
`girl`, `fisher`, `poke_ball`, and `snorlax`; the generated record retains its
numeric `rom_id` and exact table/graphic source ranges. These aliases are
converter metadata and are not embedded in the generic runtime engine.

The importer expands every 4×4-tile block into a complete 8×8-tile layer.
Runtime rendering does not interpret map blocks or cartridge graphics.

## Local output

Readable generated records are written under:

```text
data/runtime/imports/pokemon_red_us_rev_0/source/world/
    maps/
        0_pallet_town.sexpr
        ...
        247_agathas_room.sexpr
    tilesets/
        tileset_0.sexpr
        ...
    world_spaces.sexpr
    overworld_sprites.sexpr
```

The ignored runtime cache is:

```text
data/runtime/imports/pokemon_red_us_rev_0/compiled/world_maps.bin
```

It contains 24 decoded tilesets, 226 complete tile-ID layers, animation frames,
72 normalized overworld sprites, 924 actor spawns, 813 warp endpoints, and 99
importer-derived world spaces. Surface origins come from the ROM connection
graph. Indoor complexes are grouped through direct indoor warp topology and
their maps are deterministically splayed into non-overlapping coordinates.
Tile layers remain the authoritative terrain representation after loading. A
map is a content and simulation boundary, not a render primitive.

The renderer derives 32×32-tile GPU cache textures on a fixed global grid.
These chunks may contain tiles from several connected maps, and a map may
occupy several chunks. Chunks are packed into a small set of GPU texture pages
so the host can batch consecutive blits from the same texture. The camera
converts global world coordinates into screen coordinates, rejects chunks
outside the viewport, and blits the remaining chunks. Zooming out naturally
exposes more chunks. There is no monolithic world texture, map-sized texture
ownership, or separate overview representation.

Water and flower locations remain identifiable in the imported tile layers.
When their ROM-derived animation phase changes, the renderer blits the new
8×8 frames into their existing GPU cache-page locations. Ordinary render
frames still draw the same visible chunks; they do not rebuild tile geometry
or upload a CPU framebuffer. Future terrain edits will dirty only the globally
addressed chunks containing the changed tiles.

The connected view deliberately separates these layers:

1. cached terrain derived from authoritative tile grids;
2. animated and mutable terrain cache updates;
3. ground effects beneath actors;
4. actors and their shadows;
5. foreground terrain and occlusion;
6. world effects, weather, and transitions;
7. game UI and developer overlays.

Effects and actors therefore use the same world coordinates as terrain without
being part of map-sized images. Effects are drawn after terrain and are never
baked into the terrain cache. A future renderer backend may replace the cache
implementation with instanced tile draws without changing campaign data,
simulation coordinates, or layer semantics.

The actor atlas is uploaded once. Authored spawns become mutable actor
instances, are transformed from map-local 16×16 cells into global coordinates,
and are drawn above terrain at every camera scale. All actors in the current
world space remain resident; ambient simulation does not advance actors in
inactive spaces. Campaign predicates will decide which resident instances are
visible and active.

Red does not store a rectangular roam radius per NPC. Object records contain
`WALK` or `STAY` and an any-direction, vertical, horizontal, or fixed-facing
policy. Map collision and topology supply the cartridge's practical movement
boundary.

The importer therefore gives every Red `WALK` spawn an explicit half-open
movement region equal to its authored map bounds. `STAY` and stationary/scripted
spawns have no movement region because they do not participate in ambient
roaming. The movement executor must reject a roaming destination outside the
actor's region before collision resolution. Regions use global cells rather
than map-local coordinates, so custom content can define multi-map regions or
scripts can explicitly transfer an actor without making map ownership a
universal engine restriction.

## Runtime spatial index

The overworld does not scan every trigger or actor when the player moves or
interacts. Each authored map owns a dense cell index:

- terrain collision is a direct tile lookup followed by a tileset passability
  lookup;
- background interactions are compiled into the cell they occupy;
- each occupied actor cell stores one mutable actor index;
- movement updates only the old and new actor cells;
- faced-cell interaction and destination collision each inspect one cell.

Roaming actors are scheduled through a 256-slot timing wheel. A fixed step
updates only the actors due in that slot, then schedules each actor's next
decision. This keeps cost proportional to active work even when every outdoor
actor remains resident.

Logical positions are integer world cells. Presentation positions interpolate
continuously between previous and current logical transforms. Directional walk
clips advance by distance traveled, so neither a render-rate change nor a
connected-map boundary can introduce a jerk, pause, or foot-phase reset.

## Gameplay camera policy

The complete connected current world space is the default view. The camera
follows the player's interpolated presentation position in both ordinary and
whole-space zooms. Fitting every map in the space changes scale; it does not
change the camera target from the player to the space's bounding-box center.

Maps joined by connections remain one continuous movement surface. Crossing a
connection is not a warp and does not fade, load, snap, or reset camera or
walking interpolation.

Interiors, caves, and dungeons use named world spaces. Connected floors may be
placed with authored offsets and shown simultaneously when zoom permits.
Presentation-only atlas offsets for unrelated spaces never alter gameplay
coordinates.

Future camera regions and sequences are content records interpreted by the
camera director. They may reveal a town or choose a useful default zoom without
locking movement. Automatic zoom respects a manual-zoom override until the
user resets it or explicitly enables automatic reframing.

## Presentation cadence

Simulation remains fixed at 60 Hz. Rendering is independent and defaults to a
144 Hz hard cap with VSync enabled. The developer tools expose:

- a VSync toggle;
- motion interpolation on/off;
- 60, 120, 144, 165, and 240 Hz render limits;
- the effective render cap and measured frame rate;
- a toggle for the top-left FPS overlay.

Disabling motion interpolation intentionally reduces the effective render cap
to 60 Hz. The frame limiter uses precise post-present pacing and does not alter
simulation step count or environmental animation timing.

## Controls

The complete connected player world space is the initial view when its local
cache exists.

- Semantic D-pad actions move and face the player; the default profile provides
  WASD, arrow keys, and the controller D-pad.
- Semantic A/confirm activates the faced actor/background interaction and
  advances dialogue.
- `[` and `]` select imported maps for inspection.
- `Tab` switches between the connected world and selected-map inspection.
- `I/J/K/L` pans the camera.
- `+` and `-` zoom without changing source resolution.
- `0` resets pan and zoom to the fitted view.
- `F3` toggles map, warp, and actor annotations.
- `B` switches between the connected world and battle-animation lab.
- `F2` shows map provenance, world-space placement, last-warp state, camera
  controls, and selection.

Stepping onto an authored warp resolves its destination map and zero-based
destination warp endpoint. `LAST_MAP` exits return through the remembered
outdoor map. A space change switches the resident actor set and camera surface;
authored edge connections remain ordinary smooth movement and never invoke
this warp path.

Camera position and zoom move smoothly toward target values. Zoom is a
multiplier over a fitted view and ranges from the entire connected surface to
pixel-level inspection. The overview is the same renderer and the same terrain
data observed by a distant camera.

## F3 annotations

Annotations use constant-resolution text and zoom-aware anchors:

- cyan/yellow map outlines identify map domains and the selected map;
- inset orange outlines identify imported ambient-roaming regions;
- cyan squares identify authored warp cells and, when readable, show
  destination map and warp IDs;
- green, red, and gold anchors identify NPC, trainer/Pokemon, and item spawns;
- actor labels show local actor ID, sprite ID, imported movement policy, and
  whether it is map-bounded.

At distant whole-Kanto zoom, labels are suppressed while anchors remain. At
selected-map scale, labels remain full-resolution and readable. The overlay
does not mutate or participate in world simulation.

## Remaining world domains

This slice displays every active map, environmental tile animation,
current-space resident actors, imported collision, background triggers,
faced-cell dialogue, ambient roaming, ordinary direct warps, and `LAST_MAP`
returns. Script-selected destinations, special transition policies, campaign
visibility predicates, mutable blocks, ledges and field moves, indexed
automatic triggers, and authored cutscene movement remain campaign-executor
work. Persistent whole-zone NPC simulation uses this same coordinate system
and camera rather than cartridge-style screen streaming.
