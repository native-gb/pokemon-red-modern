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
global origins resolved from the ROM connection graph. After loading, the
renderer composes each resident map once as an independent GPU texture.

The camera converts global world coordinates into screen coordinates and culls
maps that do not intersect the viewport. Visible maps are drawn directly at
their global origins. There is no monolithic world texture and no separate
overview representation.

Water and flower locations remain identifiable in the imported tile layers.
The renderer batches their active ROM-derived frames over the static chunks,
so environmental animation does not rebuild map textures.

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
pixel-level inspection. Every visible map uses its full native source texture;
the overview is simply the same world renderer with a distant camera.

## Deliberate omissions

This slice displays resident outdoor geometry and environmental tile animation.
Collision cells, warps, background events, actor placements, mutable blocks,
and overworld sprite clips remain independent content domains. Persistent
whole-zone NPC simulation will use this same coordinate system and camera
rather than cartridge-style screen streaming.
