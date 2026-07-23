# Indented S-expression language

## Decision

Campaign scripts, predicates, battle effects, animations, small content
records, and package overlays share one indentation-based S-expression reader.
There is no Lua runtime and no separate punctuation-heavy grammar for each
domain.

All symbols use `snake_case`:

```text
oak_leads_player
last_physical_damage
title_intro
kadabra_alakazam
```

The reader rejects kebab-case symbols. Quoted human-facing strings may contain
ordinary punctuation and spacing.

## Reader rules

- Every nonblank line is one list.
- Tokens on the line become that list's atoms.
- An indented line becomes a child form of the preceding line.
- A line at the same indentation closes the preceding sibling.
- Dedentation closes ancestors until reaching that indentation.
- Spaces are the indentation unit; tab characters are rejected.
- `;` begins a comment outside a quoted string.
- No colon, comma, brace, explicit `end`, or mandatory parenthesis is needed.
- Blank lines do not affect structure.

```text
move oak up
move oak right
```

is the same tree as:

```lisp
(move oak up)
(move oak right)
```

Nested source:

```text
when
    equal
        last_physical_damage target user
        0
    fail
    return
```

Canonical parenthesized form:

```lisp
(when
    (equal
        (last_physical_damage target user)
        0)
    (fail)
    (return))
```

Canonical Lisp is a debug and test representation. Normal generated and
authored source uses indentation.

## Atoms

The reader recognizes:

- symbols;
- signed integers;
- exact decimal values where a schema permits them;
- quoted UTF-8 strings;
- `true` and `false`;
- optional byte strings in importer-only records.

Domain compilers assign types. The generic reader does not know whether
`bulbasaur` is a species, text key, sprite, or local name.

## One reader, typed compilers

```text
source text
    -> tokens and indentation
    -> generic form tree
    -> domain compiler
    -> typed IR
    -> control-flow/reference validation
    -> compact program or content record
    -> source map
```

Domain compilers are separate:

- content record compiler;
- predicate compiler;
- campaign compiler;
- battle-effect compiler;
- battle-AI compiler;
- animation compiler;
- audio compiler;
- package-overlay compiler.

A syntactically valid `move oak up` form still fails inside a battle-effect
program because that compiler has no `move` opcode.

## Campaign example

```text
script oak_leads_player
    lock input
    face oak down
    say oak oak_dangerous
    move oak up
    move oak up
    wait operation oak
    unlock input
```

Movement remains one visible action per line. Combined direction sequences are
not hidden in argument lists.

## Predicate example

```text
predicate can_receive_lapras
    and
        party_has_space
        not
            species_owned lapras
```

Predicates are side-effect-free and can be embedded in content:

```text
actor_spawn saffron_lapras_giver
    map silph_co_7f
    actor silph_employee
    position 4 7
    visible_when
        not
            flag_set lapras_received
```

## Battle-effect example

```text
effect counter
    when
        equal
            last_physical_damage target user
            0
        fail
        return

    deal_fixed_damage target
        multiply
            last_physical_damage target user
            2
```

Programs avoid temporary locals when direct expression nesting is clearer.
Typed persistent state is declared explicitly when an effect crosses events:

```text
effect bide
    state accumulated_damage int 0
    state turns int 0

    on receive_damage
        add accumulated_damage event_damage

    on turn_end
        add turns 1
```

## Animation example

```text
animation title_intro
    set_position logo 40 -56 native_canvas
    show logo

    parallel
        tween_position logo 40 16 24 ease_out native_canvas
        sequence
            wait 8
            play_sound title_riser

    signal title_ready
```

## Content example

```text
move thunderbolt
    name text.thunderbolt
    type electric
    power 95
    accuracy 100
    pp 15
    effect thunderbolt_effect
    animation thunderbolt
    sound thunderbolt
```

Maps, images, and bulk media retain dedicated formats where appropriate.
Indented forms reference those files; they do not encode every pixel or map
cell as a giant program.

## Package example

```text
package no_trade_evolutions
    version 1 0 0

    override evolution kadabra_alakazam
        trigger level_up
        condition
            level_at_least 37
```

## Source maps and inspection

Every form records file, line, column, indentation depth, package, and optional
ROM provenance. Generated source uses stable formatting so debugger locations
remain meaningful between identical imports.

Developer tools show:

- indented source;
- canonical parenthesized form;
- typed resolved operands;
- compiled instruction;
- active fiber and wait;
- original ROM source range.

See [EXECUTORS_AND_ISA.md](EXECUTORS_AND_ISA.md) for legal operations and
[IMPORT_OUTPUT_AND_CACHE.md](IMPORT_OUTPUT_AND_CACHE.md) for generated-source
and startup behavior.
