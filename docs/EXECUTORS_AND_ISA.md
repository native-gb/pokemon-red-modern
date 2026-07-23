# Executors and instruction sets

## Principles

Executors are ordinary functions over explicit state. They are not a class
hierarchy, service locator, or collection of map-specific callbacks.

All program source uses indentation-scoped S-expressions and `snake_case`
symbols. One reader creates a generic form tree. Separate compilers accept
different legal forms for predicates, campaign scripts, battle effects,
animations, and audio.

There is no unrestricted `eval`, arbitrary native function lookup, filesystem
access, or Lua runtime.

## Executor set

The engine needs these reusable executors:

- `flow`: title, new game, overworld, battle, ending, and credits modes;
- `scripts`: campaign fibers, calls, waits, results, and input ownership;
- `world`: complete maps, connections, cells, and mutable world state;
- `movement`: player/NPC movement, paths, collision, facing, and following;
- `interaction`: actors, signs, counters, triggers, and trainer sight;
- `transitions`: doors, stairs, warps, fades, and map handoff;
- `dialogue`: text tokens, pagination, prompts, and choices;
- `menus`: start menu, party, bag, Pokédex, shops, naming, and options;
- `encounters`: land, water, fishing, static, gift, and scripted encounters;
- `pokemon`: stats, experience, leveling, evolution, learnsets, and storage;
- `inventory`: transactions, machines, shops, field use, and item effects;
- `battle`: turn order, action resolution, switching, capture, victory, loss;
- `battle_effects`: every move and battle-item effect program;
- `battle_ai`: validated trainer decision programs;
- `animations`: title, world, battle, UI, transition, and credits timelines;
- `audio`: music, sound, cries, priorities, and channel ownership;
- `save`: semantic state serialization and migrations;
- `import`: ROM verification, decode, normalization, source emission, compile.

`step_game` visibly orders simulation executors. `render/frame` reads state and
dispatches concrete GPU draw passes; it is not an executor of game rules.

## Common form tree

The reader has no knowledge of Pokémon operations:

```cpp
struct Form {
    Atom head;
    std::vector<Atom> arguments;
    std::vector<Form> children;
    SourceSpan source;
};
```

Each nonblank line forms a list. Indentation appends child forms to the previous
line. Same indentation closes the previous form; dedentation closes ancestors.

```text
move oak up
move oak right
```

becomes:

```lisp
(move oak up)
(move oak right)
```

Nested forms map directly:

```text
multiply
    last_physical_damage target user
    2
```

becomes:

```lisp
(multiply
    (last_physical_damage target user)
    2)
```

## Shared compiled-program substrate

Every compiled program contains:

- program kind and ISA version;
- typed constant and reference pools;
- compact instructions;
- bounded call-depth and local-slot declarations;
- source map into generated or authored text;
- package and ROM provenance;
- control-flow validation summary.

Programs do not share one opcode enum. They share:

- instruction-pointer representation;
- calls and bounded frames;
- deterministic program ordering;
- typed operation IDs;
- wait conditions;
- instruction budgets;
- debug traces and breakpoints;
- source-map encoding.

## Operations and waits

A yielding instruction starts a native operation:

```cpp
struct OperationId {
    std::uint32_t value;
};
```

Wait conditions are a tagged value:

```text
none
ticks
operation
child_fiber
signal
input
```

Movement, dialogue, menus, battles, transitions, and animations report
completion through typed operation results. Campaign fibers never inspect
another executor's internal state to decide whether it is finished.

## Predicate expressions

Predicates are side-effect-free expression trees. They cannot wait, mutate
state, allocate persistent objects, or call campaign scripts.

Core value operations:

```text
equal
not_equal
less
less_or_equal
greater
greater_or_equal
and
or
not
add
subtract
multiply
divide
minimum
maximum
```

Domain queries include:

```text
flag_set
variable
has_item
item_count
party_has_space
party_contains
species_seen
species_owned
badge_owned
actor_visible
trainer_defeated
current_map
player_level
friendship
battle_result
option_enabled
```

The initial compiler uses `current_map pallet_town` as a direct boolean query.
This keeps symbolic map resolution typed without introducing an untyped symbol
value into the expression stack.

Example:

```text
predicate can_receive_lapras
    and
        party_has_space
        not
            species_owned lapras
```

The same predicate compiler serves script branches, actor visibility,
evolution, item eligibility, encounter availability, AI rules, and package
options.

## Campaign ISA

Campaign programs orchestrate native operations. They do not calculate damage,
draw pixels, mix audio, or parse files.

Control operations:

```text
end
require
call
return
jump
when
unless
if
match
repeat
fork
join
cancel
```

Synchronization:

```text
wait ticks 30
wait operation movement
wait signal title_ready
lock input
unlock input
```

State:

```text
set flag oak_met
clear flag oak_waiting
set variable starter_choice 1
add variable safari_steps 1
```

World and actors:

```text
face oak down
move oak up
path oak oak_to_lab
follow oak player
stop_follow player oak
show oak
hide oak
position oak pallet_town 10 6
set_cell pallet_town 12 8 cut_tree_stump
warp player oaks_lab lab_entrance
```

Dialogue and menus:

```text
say oak oak_dangerous
choose starter_confirmation
open_menu party
open_shop viridian_mart
name player
```

Inventory, party, battle, and system:

```text
give_item potion 1
take_item oaks_parcel 1
give_pokemon bulbasaur 5
heal_party
start_battle wild_pidgey
start_trainer_battle rival_lab
play_animation heal_party
play_sound item_received
start_music oaks_lab_theme
save_game
```

Example:

```text
script oak_stops_player
    require oak_not_met
    lock input

    face player up
    show oak
    path oak oak_to_player
    wait operation oak
    say oak oak_dangerous

    follow player oak
    path player pallet_to_lab
    wait operation player
    stop_follow player oak

    set flag oak_met
    unlock input
```

The compiler may lower structured `when`, `if`, and `match` forms to branch
instructions. Imported irreducible control flow may use explicit labels and
jumps in generated source, but authored code should remain structured.

## Battle-effect ISA

Every move effect is a program. Native C++ exposes only general mechanics and
state operations; there is no per-move native callback registry.

Queries:

```text
user
target
move
turn
last_move
last_damage
last_physical_damage
current_hp
maximum_hp
status
volatile
stat_stage
type
move_slot
random
```

Actions:

```text
check_accuracy
calculate_damage
deal_damage
deal_fixed_damage
heal
drain
recoil
apply_status
clear_status
add_volatile
remove_volatile
change_stat_stage
copy_types
copy_battle_stats
copy_stat_stages
copy_moves
replace_move
disable_move
execute_move
force_switch
escape
suppress_action
fail
finish
emit
```

Persistent effects declare typed state and event handlers:

```text
effect bide
    state accumulated_damage int 0
    state turns int 0

    on begin
        add_volatile user bide
        suppress_action

    on receive_damage
        add accumulated_damage event_damage

    on turn_end
        add turns 1
        when
            greater_or_equal turns 2
            deal_fixed_damage target
                multiply accumulated_damage 2
            remove_volatile user bide
            finish
```

Counter:

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

Metronome:

```text
effect metronome
    execute_move user
        random_move
            exclude metronome
            exclude struggle
```

Transform:

```text
effect transform
    copy_types target user
    copy_battle_stats target user
    copy_stat_stages target user
    copy_moves target user
        pp 5
    emit visual_form_changed user target
```

Supported hooks initially include:

```text
begin
before_action
after_accuracy
after_damage
receive_damage
deal_damage
turn_end
switch_out
expire
```

Loops, recursion, effect state, and nested move execution are bounded.
Deterministic RNG is supplied by the battle executor.

## Battle AI

AI programs inspect legal actions and assign deterministic scores:

```text
ai rival_ordinary
    rule prefer_super_effective
        when
            move_is_super_effective candidate_move target
        add_score 20

    rule avoid_disabled
        when
            move_disabled candidate_move
        reject
```

The battle executor chooses among highest-scoring legal actions using its
dedicated deterministic RNG stream.

## Animation ISA

Animations mutate an `AnimationState` containing scene nodes. Render code reads
those nodes; animation instructions never issue SDL calls.

Node kinds include:

```text
sprite
text
rectangle
tile_layer
particle_emitter
external_actor
external_battler
camera
```

Lifetime and assets:

```text
spawn
destroy
show
hide
set_sprite
set_frame
play_clip
set_text
bind
```

Transform and ordering:

```text
set_position
set_scale
set_rotation
set_pivot
set_layer
set_clip_rect
set_mask
tween_position
tween_scale
tween_rotation
follow_path
shake
```

Screen, color, particles, and audio:

```text
set_palette
tween_color
fade_screen
flash
set_camera
tween_camera
shake_camera
emit_particles
stop_particles
play_sound
play_cry
start_music
stop_music
```

Flow:

```text
wait
wait_input
wait_signal
signal
sequence
parallel
call
return
repeat
end
```

Coordinate spaces are explicit:

```text
native_canvas
viewport
world
node_local
```

Title example:

```text
animation title_intro
    scene title
    set_position logo 40 -56 native_canvas
    show logo

    parallel
        tween_position logo 40 16 24 ease_out native_canvas
        sequence
            wait 8
            play_sound title_riser

    wait 18
    show version_label

    parallel
        tween_position version_label 54 48 8 linear native_canvas
        play_sound title_impact

    signal title_ready
```

Battle animation example:

```text
animation thunderbolt
    bind source attacker
    bind destination defender

    parallel
        shake destination 2 12
        sequence
            flash screen electric_flash 4
            wait 2
            flash screen electric_flash 4
        play_sound thunderbolt

    signal hit_frame
```

Animation events align visuals and sound but do not decide whether a move hit or
how much damage it caused.

## Audio programs

Audio has a separate sequencer ISA for notes, instruments, envelopes, loops,
calls, noise, tempo, and priorities. Campaign and animation programs trigger
semantic music, sound, and cry IDs.

The audio executor owns channel arbitration. Music data can never be
reinterpreted as a sound-effect stream because each program and channel has a
validated kind.

## Simulation ordering

A normal fixed step is:

1. sample and arbitrate input;
2. advance game flow and ready campaign fibers;
3. resolve actor intentions and movement;
4. resolve interactions, triggers, encounters, and transitions;
5. advance dialogue and menus;
6. resolve battle actions and effect hooks;
7. advance animation timelines;
8. publish audio cues;
9. complete operation IDs and wake fibers for the next deterministic boundary;
10. record debug traces and update play time.

Rendering may interpolate state but cannot add or remove simulation steps.

## Debugging

Developer tools expose:

- all active fibers, hooks, waits, locals, and call frames;
- current generated source file and line;
- canonical parenthesized S-expression;
- recent instructions and operation completions;
- active animation nodes and tracks;
- battle effect instances and persistent slots;
- instruction budget and stack usage;
- breakpoints by record key or source line.

The generated source and source maps make imported behavior inspectable without
reading raw disassembly during ordinary debugging.

## Implemented foundation

The current engine code includes:

- typed postorder predicate instructions with catalog reference resolution,
  static value checks, bounded stack sizing, and source-located runtime errors;
- explicit predicate state views for flags, variables, inventory, party,
  Pokédex ownership, actor visibility, map, level, friendship, and battle
  result;
- a typed animation timeline compiler for `sequence`, `parallel`, `wait`,
  visibility, position, position tweens, semantic sound cues, and signals;
- an animation executor whose mutable nodes and tweens are independent of SDL
  and concrete rendering.

Campaign, battle-effect, AI, and audio compilers remain the next implementation
slices. Their documented instruction sets are contracts, not claims that every
operation is already executable.
