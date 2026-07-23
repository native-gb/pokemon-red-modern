# Packages and patches

## Model

The canonical ROM import, compatibility fixes, optional enhancements, and mods
all compile into the same typed record model:

```text
canonical imported package
    -> compatibility overlays
    -> selected enhancement overlays
    -> enabled authored packages
    -> resolve references
    -> validate
    -> assign dense IDs
    -> compiled effective pack
```

Package files use indented S-expressions and `snake_case` identifiers.

## Package manifest

```text
package no_trade_evolutions
    display_name "No Trade Evolutions"
    version 1 0 0
    author "Example"
    requires campaign.pokemon_red
    requires_schema 1
    save_namespace no_trade_evolutions
```

Dependencies and load order are explicit. A package cannot inspect arbitrary
files or run host code while loading.

## Record operations

The initial model has two operations.

### Define

`define` creates a stable record and fails if the key already exists:

```text
define type steel
    name text.type_steel
    damage_class physical
    color 168 168 192
```

### Override

`override` changes only listed fields of an existing record and fails if the
record or field does not exist:

```text
override evolution kadabra_alakazam
    trigger level_up
    condition
        level_at_least 37
```

An override may explicitly replace a complete record body, but ordinary field
overrides are preferred because diagnostics and conflict reports remain useful.

## Keyed child records

Patchable collection members should be first-class records.

Instead of:

```text
species kadabra
    evolutions trade alakazam
```

use:

```text
evolution kadabra_alakazam
    from kadabra
    to alakazam
    trigger trade
```

The same applies to:

- learnset entries;
- encounter slots;
- evolutions;
- type interactions;
- status immunities;
- actor spawns;
- triggers;
- warps;
- connections;
- shop entries when independent patching is needed;
- trainer-party members when independent patching is needed.

Tables build owner ranges after all overlays resolve.

## Examples

### Add Steel

```text
package add_steel_type
    version 1 0 0

    define text type_steel
        line "STEEL"

    define type steel
        name type_steel
        damage_class physical
        color 168 168 192

    define type_interaction steel rock
        multiplier 2 1

    define type_interaction steel ice
        multiplier 2 1

    define type_interaction fire steel
        multiplier 2 1

    define type_interaction poison steel
        multiplier 0 1

    define status_immunity steel poison
```

Nothing in the battle executor changes. Species and moves may be overridden to
reference the new type:

```text
override species magnemite
    secondary_type steel

override move metal_claw
    type steel
```

### Replace trade evolutions

```text
package no_trade_evolutions
    version 1 0 0

    override evolution kadabra_alakazam
        trigger level_up
        condition
            level_at_least 37

    override evolution machoke_machamp
        trigger level_up
        condition
            level_at_least 37

    override evolution graveler_golem
        trigger level_up
        condition
            level_at_least 37

    override evolution haunter_gengar
        trigger level_up
        condition
            level_at_least 37
```

### Disable an encounter

Records use an availability predicate instead of a generic deletion operation:

```text
override encounter_slot route_1_pidgey_common
    enabled false
```

### Change one encounter

```text
override encounter_slot route_1_rattata_rare
    species pikachu
    level 5
    weight 8
```

### Add an encounter

```text
define encounter_slot route_1_pikachu_mod
    table route_1_land
    species pikachu
    level 5
    weight 8
    order 35
```

No generic `append` is required because the new record identifies its owner and
order.

### Change a trainer party

For a small party represented as one record, override the complete member list:

```text
override trainer_party brock
    members
        member geodude
            level 12

        member onix
            level 14
            move rock_throw
            move bind
            move bide
```

If independent member patching becomes a real requirement, trainer members can
be promoted to keyed child records. The schema should not begin with a generic
path-patching language.

### Override a script

```text
override script parcel_delivery
    when
        has_item oaks_parcel
        take_item oaks_parcel 1
        say oak parcel_thanks
        give_item pokedex 1
        set flag pokedex_received
        return

    say oak oak_waiting
```

The replacement body compiles through the campaign ISA and receives normal
reference and control-flow validation.

### Add a connected map

```text
package crystal_caverns
    version 1 0 0

    define map crystal_caverns
        width 48
        height 32
        tileset cave
        cells "maps/crystal_caverns.cells"
        music cave_theme

    define connection rock_tunnel_crystal_caverns
        from rock_tunnel_b1f
        edge east
        to crystal_caverns
        destination_edge west
        offset 4

    define encounter_table crystal_caverns_land
        rate 25
        terrain cave

    define encounter_slot crystal_caverns_zubat
        table crystal_caverns_land
        species zubat
        level 24
        weight 30
        order 10
```

## Ordered fields

Ordered records use explicit `order` or `priority`. If two records claim the
same owner and order, validation reports the ambiguity. Order does not depend
on filesystem enumeration or package hash-table iteration.

## Conflict reporting

The resolver records every supplied field:

```text
record evolution.kadabra_alakazam
field trigger
canonical trade
no_trade_evolutions level_up
randomizer_pack item_use
winner randomizer_pack
```

Two packages overriding the same field are not always fatal because explicit
load order may resolve them, but the conflict remains visible. Packages may
declare incompatibility when combining them would be misleading.

## Provenance

Every effective record reports:

- defining package;
- packages that overrode it;
- winning package for each field;
- generated source file and line;
- original ROM range for canonical records;
- compiler and schema version.

Developer tools should answer “why does Kadabra evolve this way?” without
searching every installed package.

## Saves and packages

Saves record:

- canonical campaign key and source identity;
- effective pack hash;
- enabled package IDs and versions;
- stable content keys used by persistent state;
- package-owned save namespaces.

Loading a changed package set resolves stable keys again and runs declared
migrations. Missing required records produce a diagnostic instead of silently
substituting unrelated dense IDs.

## Deliberately absent operations

The first version does not provide generic:

```text
remove
append
insert
move_before
move_after
json_path
byte_patch
instruction_patch
```

Real needs should produce typed record operations or better first-class child
records. This keeps patches understandable and validates them at the domain
level.
