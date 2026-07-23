# Map browser

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
data/runtime/imports/pokemon_red_us_rev_0/compiled/map_browser.bin
```

It contains decoded tilesets, complete tile-ID layers, and global origins
resolved from the ROM connection graph.
After loading, the renderer composes each complete map once and uploads it as
one GPU texture. It then composes those textures into a full-resolution world
atlas on the GPU. A frame draws either the selected map or the whole outdoor
world with one texture operation.

## Controls

The map browser is the initial view when its local cache exists.

- Left/Right selects the previous or next imported map.
- `Tab` switches between the selected map and stitched world atlas.
- `W/A/S/D` pans the camera.
- `+` and `-` zoom without changing source resolution.
- `0` resets pan and zoom to the fitted view.
- `B` switches between the map browser and battle-animation lab.
- `F2` shows map provenance, global placement, camera controls, and selection.

Zoom is a multiplier over a fitted view and ranges from a broad overview to
pixel-level inspection. The atlas retains every native map pixel; it is not a
pre-shrunk overview texture.

## Deliberate omissions

This slice displays resident outdoor geometry only. Collision cells, warps,
background events, actor placements, mutable blocks, and overworld sprite clips
remain independent content domains. Persistent whole-zone NPC simulation will
be built over the same global coordinate system rather than cartridge-style
screen streaming.
