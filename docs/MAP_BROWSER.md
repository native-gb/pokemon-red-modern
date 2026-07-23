# Map browser

## First imported slice

The first world slice contains four connected outdoor maps:

- Pallet Town, ROM map ID `0x00`;
- Route 1, ROM map ID `0x0C`;
- Viridian City, ROM map ID `0x01`;
- Route 22, ROM map ID `0x21`.

These maps all use tileset `0`. The importer reads their header-table entries,
dimensions, block-grid pointers, block grids, tileset header, two-bit graphics,
and required block definitions from the verified local ROM.

The importer expands every 4×4-tile block into a complete 8×8-tile layer.
Runtime rendering does not interpret map blocks or cartridge graphics.

## Local output

Readable generated records are written under:

```text
data/runtime/imports/pokemon_red_us_rev_0/source/world/
    maps/
        0_pallet_town.sexpr
        12_route_1.sexpr
        1_viridian_city.sexpr
        33_route_22.sexpr
    tilesets/
        tileset_00.sexpr
```

The ignored runtime cache is:

```text
data/runtime/imports/pokemon_red_us_rev_0/compiled/map_browser.bin
```

It contains the decoded tile atlas and complete tile-ID layer for each map.
After loading, the renderer composes each complete map once and uploads it as
one GPU texture. A frame draws the selected map with one texture operation.

## Controls

The map browser is the initial view when its local cache exists.

- Left/Right selects the previous or next imported map.
- `B` switches between the map browser and battle-animation lab.
- `F2` shows the map ID, dimensions, tileset, and selection controls.

The whole map is centered and fit to the available window area. Integer scaling
is used whenever the map fits at scale one or greater.

## Deliberate omissions

This first slice displays resident map geometry only. Connection seams are not
stitched to adjacent maps, so open edges remain visible where the cartridge
normally fills the scrolling border from a connected map.

The next world passes will add connection transforms, collision cells, warps,
background events, actor placements, and overworld sprite clips as independent
debug overlays.
