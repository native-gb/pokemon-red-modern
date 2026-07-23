# Pokemon and battle-rule import

## Implemented immutable domains

The standalone importer decodes the supported user ROM into readable source
under:

```text
data/runtime/imports/pokemon_red_us_rev_0/source/rules/
    types.sexpr
    type_interactions.sexpr
    moves.sexpr
    species.sexpr
    learnsets.sexpr
    evolutions.sexpr
    growth_curves.sexpr
    machines.sexpr
```

The corresponding runtime cache is:

```text
data/runtime/imports/pokemon_red_us_rev_0/compiled/pokemon_rules.bin
```

Normal gameplay loads that cache and never reads the ROM or reparses the
generated source. The cache contains:

- all 27 type ID slots, including the eleven intentionally unused slots;
- all 82 explicit type-interaction entries, with the implicit default
  multiplier remaining one;
- all 165 move records and names, including raw 0–255 accuracy, PP, power,
  type, animation ID, and effect-program ID;
- all 151 Dex-ordered species joined through all 190 internal species slots;
- every base stat, type, catch rate, experience yield, starting move, growth
  curve, and 55-bit TM/HM compatibility set;
- all 728 ordered level-up learnset entries;
- all 72 level, item, and trade evolutions;
- six semantic growth routines materialized into complete level 1–100
  experience lookup tables;
- all 55 TM/HM-to-move mappings.

The importer validates counts, source boundaries, pointer domains,
terminators, identities, unique internal-to-Dex joins, move and type
references, progression order, and runtime-cache structure. The report at
`reports/rule_import_summary.txt` records exact counts and source spans.

## Implemented table executors

Generic runtime queries now execute directly against the validated cache:

- sparse dual-type effectiveness, including duplicate-type suppression;
- exact level-up move lookup with source ordering preserved;
- TM/HM compatibility-bit lookup;
- level-to-experience and experience-to-level lookup;
- level, item, and trade evolution eligibility.

These functions contain no species, move, type, machine, or evolution names.
They are the table-backed progression layer beneath future party, battle, and
menu owners. They do not make the formula/effect domains below complete.

## Engine boundary

The runtime loader is campaign-neutral. It knows how to validate typed
species, move, progression, type, and machine records, but contains no Pokemon
Red names, base stats, learnsets, evolution targets, compatibility sets, or
type matchups.

Move records bind a numeric source effect to a stable generated effect key.
The effect program itself is not inferred from the move table. Standard
damage, unusual move behavior, statuses, capture, critical hits, experience
awards, and other ordered calculations are separate compiled program domains.
Until those programs are semantically imported and executable, the move
record is complete data but not a complete playable battle implementation.

This distinction is enforced in the import report:

```text
semantic_move_effect_programs 0
```

The count must eventually equal the complete reachable effect-program set.
It must not be replaced by move-name switches or Red-specific constants in
the generic runtime.

## Compatibility profiles

Original, Fixed, and Custom rulesets will reference the same immutable species,
move, progression, and machine indexes. They may select different imported or
engine-authored semantic programs for known behavior defects such as the Gen I
critical-hit, Focus Energy, type-chart, stat, and status mistakes. A
compatibility toggle changes program bindings; it does not duplicate or mutate
the source tables.
