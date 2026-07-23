# Connected world renderer

## Imported outdoor world

The importer discovers the 36 named outdoor maps in Red's Town Map table. This
includes every city, town, surface route, sea route, and Indigo Plateau. It does
not include interiors, caves, building floors, or dungeon floors; those are
separate resident maps reached through warps.

The importer reads names, IDs, dimensions, block grids, tilesets, graphics, and
connection transforms from the verified local ROM. The engine contains no list
of campaign map names or placements.

The importer expands every 4×4-tile block into a complete 8×8-tile layer.
Runtime rendering does not interpret map blocks or cartridge graphics.

## Local output

Readable generated records are written under:

```text
data/runtime/imports/pokemon_red_us_rev_0/source/world/
    maps/
        0_pallet_town.sexpr
        ...
        35_route_25.sexpr
    tilesets/
        tileset_0.sexpr
        tileset_23.sexpr
```

The ignored runtime cache is:

```text
data/runtime/imports/pokemon_red_us_rev_0/compiled/world_maps.bin
```

It contains decoded tilesets, complete tile-ID layers, animation frames, and
global origins resolved from the ROM connection graph. Tile layers remain the
authoritative terrain representation after loading. A map is a content and
simulation boundary, not a render primitive.

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

The connected world is the initial view when its local cache exists.

- Left/Right selects the previous or next imported map.
- `Tab` switches between the connected world and selected-map inspection.
- `W/A/S/D` pans the camera.
- `+` and `-` zoom without changing source resolution.
- `0` resets pan and zoom to the fitted view.
- `B` switches between the connected world and battle-animation lab.
- `F2` shows map provenance, global placement, camera controls, and selection.

Camera position and zoom move smoothly toward target values. Zoom is a
multiplier over a fitted view and ranges from the entire connected surface to
pixel-level inspection. The overview is the same renderer and the same terrain
data observed by a distant camera.

## Deliberate omissions

This slice displays resident outdoor geometry and environmental tile animation.
Collision cells, warps, background events, actor placements, mutable blocks,
and overworld sprite clips remain independent content domains. Persistent
whole-zone NPC simulation will use this same coordinate system and camera
rather than cartridge-style screen streaming.
