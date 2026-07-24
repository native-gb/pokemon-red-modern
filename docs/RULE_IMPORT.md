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
data/runtime/imports/pokemon_red_us_rev_0/compiled/battle_rules.bin
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

The first semantic formula program is also executable. The importer lifts the
ordinary damage path into eight typed instructions covering Explosion defense,
wide-stat scaling, critical level scaling, base damage, caps, STAB, sequential
dual-type effectiveness, and the cartridge's rejection-sampled 217–255 damage
variance. Its readable source is:

```text
source/battle_effects/damage.sexpr
```

The executor is generic: operation ordering and constants come from
`battle_rules.bin`, while type pairs come from `pokemon_rules.bin`.

The original critical-hit path is executable as a second imported semantic
program. Its readable source is:

```text
source/battle_effects/critical_hits.sexpr
```

The importer reads the cartridge's complete high-critical move table and
lowers the base-Speed ratio, Focus Energy branches, ordinary/high-critical
move ratios, random-byte rotation, and strict threshold comparison. This
preserves the original Focus Energy defect through imported program data
without a Red move-ID switch or formula constants in the engine.

The original capture calculation is executable as a third imported semantic
program. Its readable source is:

```text
source/battle_effects/capture.sexpr
```

It contains five ball profiles, three status modifier profiles, and ordered
operations for the first-roll ceiling and rejection sampling, guaranteed
capture, status subtraction, HP-derived capture value, catch-rate comparison,
second roll, and failure shake tier. The numeric constants are read from the
verified cartridge routine during import. The importer also resolves each
ordinary inventory Ball profile's item ID from the cartridge item-name table
and emits that binding beside the profile. Safari Ball remains an unbound
Safari-battle resource rather than pretending to be ordinary bag item 5
(which is Town Map). The runtime searches those imported bindings; it does not
switch on Red item IDs.

The ordinary battle owner now consumes this program from the imported ITEM
command. It commits one bound Ball only after verifying party/current-box
capacity, preserves the generated wild Pokémon instance on success, and
executes the enemy's ordinary action after a failed throw. RUN and party
switching likewise dispatch from the imported command `on_select` symbols.

The original experience-award calculation is executable as a fourth imported
semantic program. Its readable source is:

```text
source/battle_effects/experience.sexpr
```

The importer derives the level divisor and verifies the cartridge's half-add
boost routine before lowering reward-data division, stat-experience awards,
base experience, traded-Pokemon boost, and trainer-battle boost. Division and
boost order remain program-owned so every intermediate floor is preserved.
The future battle owner will schedule participants and Exp. All recipients;
the generic formula executor receives those divisors without knowing Red party
rules or item identities.

Owned-Pokemon stat calculation is executable as a fifth imported semantic
program:

```text
source/battle_effects/stats.sexpr
```

The importer validates HP-DV bit construction, effort-root division, base/DV
combination, level scaling, HP and ordinary-stat additions, and the stat cap
against the verified cartridge routine. The executor consumes the ordered
program and deliberately preserves Gen I's ceiling-square-root effort
rounding.

`PokemonState` and `PartyState` now own mutable identity, trainer provenance,
level/experience, DVs, stat experience, calculated stats, HP, status, and
move/PP state. Construction and experience progression join those instances
to imported species, growth, move, learnset, evolution, experience, and stat
programs. They do not contain Red species or move switches.

Ordinary move accuracy/evasion is executable from the cartridge's complete
13-entry stage-ratio table:

```text
source/battle_effects/accuracy.sexpr
```

The importer validates the reflected-evasion constant, two-stage calculation,
minimum-one behavior, 255 cap, and strict random comparison. The generic
executor therefore preserves maximum accuracy's one-in-256 miss without
owning Gen I ratios.

The ordinary-damage move-effect program is executable:

```text
source/battle_effects/move_effects/ordinary_damage.sexpr
```

Its imported source-effect binding drives accuracy, critical-hit, damage, and
HP application through the generic `BattleState` owner. A complete ordinary
turn now owns speed ordering, PP consumption, deterministic RNG, fainting,
replacement, victory/defeat, and experience awards transactionally.
Unsupported effects reject the turn without consuming PP or HP.

All 24 direct one-stage and two-stage stat programs are also imported from
the cartridge's four six-entry handler families. Their readable programs
select user/target, attack/defense/speed/special/accuracy/evasion, and signed
stage amount. Stat-down programs include the source game's enemy-only random
failure gate before accuracy. The runtime owns only generic `modify_stage` and
`enemy_random_gate` operations.

Wild encounters now join that battle owner through a separate exhaustive
import. `encounters.bin` accounts for all 248 map slots, all 55 active land
tables, all three active water tables, and all 580 explicit species/level
slots. The generated source exposes the exact ten cumulative slot thresholds
and each map's separate land/water rates:

```text
source/encounters/probabilities.sexpr
source/encounters/maps.sexpr
```

Terrain selection constants and indoor-map policy are imported from the
cartridge. After a completed world step, the runtime selects terrain, applies
the rate and slot rolls, applies repel by leading level, and materializes the
result through the imported species, stat, starting-move, and learnset tables.
The resulting owned wild party enters the same `BattleState` used by trainer
battles. Real battle presentation binds the two owned species pictures, names,
levels, HP, status, move names, types, and PP; the animation lab is a separate
developer mode.

Trainer and static-Pokemon actors now enter that same owner through two
independent imported joins. The trainer cache resolves all 346 compact actor
parameters into 334 indexed trainer parties and 12 static species/level
records. The interaction cache resolves all 322 trainer headers into actor
ownership, sight range, defeated flag, and before/end/after text. Activating
an undefeated resolved owner presents its imported text, starts its owned
battle after acknowledgement, and commits the imported defeated flag only on
victory. Static encounters use the same path without masquerading as trainer
parties. Imported nonzero sight ranges are materialized into per-cell indexes;
entering one locks world movement while the resident actor approaches
cell-by-cell, then emits the same battle activation. The remaining special
scripted opponents stay explicit campaign-script work.

The import report keeps status programs at zero and reports 25 bound source
move effects until the remaining effect domains are genuinely executable.

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
move_effect_programs 25
bound_source_move_effects 25
status_programs 0
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
