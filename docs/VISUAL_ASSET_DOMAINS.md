# Visual asset domains

Red does not have one uniform “sprite” format. It has several storage,
compression, composition, animation, and layout systems which happen to become
images on screen. The modern catalog should normalize them into GPU-ready
images and metadata during import rather than reproduce Game Boy VRAM assembly
at runtime.

## Normalized vocabulary

The modern side uses a small common vocabulary:

- **image**: decoded pixels placed in an atlas;
- **sprite**: an image region plus pivot and palette metadata;
- **clip**: ordered sprite frames and durations;
- **tile layer**: a complete or chunked map background built from imported
  tiles;
- **composite**: a fixed arrangement of images used by one view;
- **palette**: DMG, SGB, or modern color mapping;
- **animation program**: time-varying operations over semantic view targets.

Cartridge tiles and OAM pieces are import formats. They should not become the
general runtime rendering API.

## Domain inventory

### Pokémon battle pictures

Each normal species has:

- one compressed front picture with encoded source dimensions;
- one compressed back picture;
- a front and back pivot or placement derived from the view layout;
- palette associations;
- one shared party-icon class.

Red contains 151 normal front/back pairs. The front-picture domain also has
special fossil pictures beyond the normal species set. Ghost and Substitute
are separate battle presentation assets rather than species.

The importer should decode every picture to a transparent normalized image.
The battle renderer owns the front/back anchors and scaling. Title, Pokédex,
evolution, Hall of Fame, and battle views should reference the same decoded
front image rather than importing copies.

Suggested semantic record:

```text
pokemon_visual bulbasaur
    front bulbasaur_front
    back bulbasaur_back
    party_icon plant
```

### Trainer battle portraits

Trainer battle portraits are compressed large pictures similar in purpose to
Pokémon fronts but indexed by trainer class. Red has 45 portrait files,
including player, rival, Oak, and special-purpose pictures.

Trainer portraits are independent of overworld NPC graphics. A trainer class
may use one battle portrait while individual map actors use one or more
different overworld sheets.

```text
trainer_visual bug_catcher
    battle_portrait bug_catcher_portrait
```

### Overworld character sheets

Red has 67 distinct raw overworld sheets, with more semantic sprite IDs because
some IDs alias the same graphics. Most moving character sheets contain twelve
8×8 tiles:

- standing and walking down;
- standing and walking up;
- standing and walking left;
- right-facing frames produced by horizontal flipping.

Four tiles compose one 16×16 frame. The standing frame is reused between walk
steps. Static world objects use four-tile sheets and ignore facing and gait.

The modern importer should expand these into named clips:

```text
sprite_clip oak_walk_down
    frame oak_down_stand duration 8
    frame oak_down_step_left duration 8
    frame oak_down_stand duration 8
    frame oak_down_step_right duration 8
```

The player has additional states which must remain distinct clips: walking,
bicycle, fishing, shrinking, and any scripted presentation form. Surf and
special world presentation may combine a character clip with another effect or
vehicle asset.

### Static overworld objects

Poké Balls, fossils, boulders, papers, the Pokédex, clipboard, Snorlax, Old
Amber, and sleeping actors use the overworld sprite system but are semantic
objects rather than directional characters.

They should still become ordinary sprites. Collision, interaction, item
identity, movement, and map placement belong to world content, not the image.

### World tilesets and blocksets

Red has 19 primary tileset graphics sets and 19 corresponding blocksets.
Tilesets provide decoded 8×8 tile images. Blocksets arrange those tiles into
larger map blocks. Map block IDs then build the background.

The modern map importer should expand this hierarchy:

```text
ROM tiles -> blockset composites -> complete map tile layer
```

Runtime maps should not reconstruct blocksets every frame. Complete map
geometry enables arbitrary camera sizes, smooth transitions, and later
widescreen rendering without changing collision or actor activation.

Collision roles, ledges, warps, doors, cut trees, water, counters, and
bookshelves are world semantics associated with map geometry. They are not
properties inferred from final pixels.

### Animated and stateful world tiles

Flowers, water, spinners, doors, warp pads, healing machines, cut trees, smoke,
shadows, fishing rods, and battle-transition graphics have small animation or
state systems.

Normalize each into either:

- a looping tile clip;
- a one-shot sprite clip;
- a view animation over semantic targets;
- alternate tile-layer state selected by world state.

Do not retain a generic “write this VRAM tile now” operation.

### Party and menu Pokémon icons

Generation I does not contain one unique menu icon per species. Species map to
shared icon archetypes such as plant, quadruped, snake, and bug. The icon
executor animates those shared pieces in party and status views.

Species content therefore references a party-icon clip ID. It does not own a
second miniature copy of its battle picture.

### Battle move-effect graphics

Moves generally do not own individual sprites. They reference:

- one battle-effect animation program;
- shared move-animation tile banks;
- shared frame blocks and coordinate tables;
- shared special assets such as balls, HUD pieces, Ghost, Minimize, and
  Substitute;
- sound and palette cues.

The battle-animation importer already normalizes all 203 programs and all 39
used special-effect command types. Expanded temporary frames, procedural
tables, and small special graphics remain in the ignored local import.

A move record should bind to an animation ID:

```text
move thunderbolt
    animation thunderbolt
```

It should not duplicate animation tiles or procedural visuals inside the move
definition.

### Battle interface graphics

Battle HUD pieces, HP bars, status glyphs, level markers, Poké Ball indicators,
capture balls, and menu frames form a separate UI asset domain. Their layout is
owned by the battle view.

HP fill is procedural geometry over an exact numeric HP ratio; it is not an
eight-state sprite. Border caps and labels are sprites or font glyphs.

### Fonts, glyphs, and general UI chrome

This domain includes:

- main font and extra glyph banks;
- battle-only glyphs;
- text-box borders;
- cursor and continuation arrows;
- HP and EXP bar pieces;
- Pokédex and trainer-card decorations;
- badges, badge numbers, and leader faces;
- naming-screen and menu symbols.

Fonts should import as a glyph atlas with metrics. UI frames should become
repeatable frame definitions or explicit view-owned composites. Character
encoding belongs to the text importer, while glyph pixels belong here.

### Title, intro, and cinematic composites

These are view-specific assets rather than general world sprites:

- title logo, version label, copyright line, trainer, and cycling Pokémon;
- Gengar/Nidorino intro frames and tilemaps;
- Oak speech and player introduction pictures;
- evolution and Hall of Fame presentation;
- credits and “The End” graphics;
- splash/version screens.

Static images and tilemaps become composites. Their movement, fades, sound
cues, and sequencing belong to animation programs. Absolute coordinates are
allowed inside a compatibility view layout, but not inside reusable Pokémon or
trainer asset definitions.

### Town map and specialized screens

The town map, Pokédex, trainer card, slots, trade sequence, and link-cable
screens each own specialized graphics and tilemaps. SGB borders form another
specialized presentation family.

These assets should be imported once and referenced by dedicated Pokémon view
renderers. They do not justify a general scene graph.

### Emotes and transient world effects

Question, shock, and happy emotes plus smoke, shadow, healing, fishing, and
similar effects are small reusable sprites or clips. Campaign scripts request
them semantically:

```text
show_emote question actor oak
```

The script should not know tile IDs, OAM positions, or atlas coordinates.

### Palettes and color assignment

Palette data is its own asset domain even though it contains no geometry:

- DMG shade mappings;
- SGB color palettes and attribute regions;
- map or scene palette assignments;
- battle flash sequences;
- optional modern color profiles.

Image identity and palette identity remain separate. This permits original
monochrome, SGB-like color, and modern color modes without duplicating every
image.

## What is not a sprite domain

- **Moves** own rules and animation bindings, not unique image collections.
- **Species** own visual references, not embedded pixels inside their stat
  records.
- **NPCs** own actor state, position, facing, and script bindings; their graphic
  is a referenced overworld clip.
- **Maps** own geometry and semantic collision; their decoded background is a
  tile layer.
- **Animations** own time and transforms; their temporary images come from the
  asset catalog.
- **Audio** and cries are separate content domains.

## Catalog additions

The existing `ImageDef`, `SpriteDef`, `SpriteClipDef`, and `PaletteDef` are the
right low-level runtime records. Higher-level content should reference them:

```text
PokemonVisualDef
    front_sprite
    back_sprite
    party_icon_clip

TrainerVisualDef
    battle_portrait
    overworld_clip_set

OverworldClipSetDef
    stand_down
    walk_down
    stand_up
    walk_up
    stand_left
    walk_left
    stand_right
    walk_right

TilesetVisualDef
    tile_atlas
    complete_map_layers
    animated_tile_clips
```

These are typed references, not new render nodes.

## Import and cache shape

Readable local output should make bindings auditable:

```text
source/
    graphics/
        pokemon_visuals.sexpr
        trainer_visuals.sexpr
        overworld_clips.sexpr
        tilesets.sexpr
        ui_graphics.sexpr
        scene_graphics.sexpr

compiled/
    graphics.atlas
    graphics.index
    map_layers.bin
```

The atlas may contain multiple pages. Every source image retains provenance,
decoded dimensions, transparency rules, pivot, palette role, and source ROM
range. Runtime loads the compiled index and atlas without reparsing or
recompositing cartridge tiles.

## Recommended implementation order

1. **Complete:** decode all Pokémon front/back pictures and trainer battle
   portraits into readable bindings and `compiled/battle_pictures.bin`.
2. **Complete:** replace the battle-lab placeholder battlers when imported
   pictures are present; Up/Down and F2 controls iterate every species pair.
3. Decode overworld sheets and generate all facing/walking clips.
4. Decode tilesets and blocksets into complete map layers.
5. Import shared party icons, fonts, battle HUD, and UI chrome.
6. Import title/intro and specialized-screen composites.
7. Import SGB palettes, borders, and remaining transient presentation assets.

The first checkpoint should include a visual browser which iterates every
Pokémon front/back pair and every trainer portrait, reports missing bindings,
and never performs cartridge decompression in the render loop.
