# Title-to-Brock bulk playtest

This is the current narrow acceptance target. It is intentionally a player
bulk-test handoff, not a claim that the whole campaign or exact Red
presentation is finished.

## Connected in this pass

- New Game imports both Red's upstairs start and Pallet Town as the previous
  outdoor map. Upstairs stairs lead downstairs and the downstairs
  `LAST_MAP` door leads outside.
- Start opens a modern in-game overlay with party, bag, and trainer views.
- Wild ITEM consumes a Ball through the cartridge-imported item/profile
  binding and capture formula. Captured instances retain stats, moves, DVs,
  level, and current state and enter the party or current box.
- Failed capture attempts execute the enemy action. Wild RUN exits, trainer
  RUN is rejected, and PKMN switches to the next usable party member before
  the enemy action.
- Every decoded Pokémon Center nurse uses the imported shared text family,
  HEAL/CANCEL choice, HP/status/PP healing, and a saved healing checkpoint.
  The tileset's ROM-derived counter roles extend actor activation across the
  counter rather than relying on a Center-specific coordinate.
- Existing campaign fibers cover Oak interception, all starters, naming,
  the first lab rival battle, parcel handoff/return, Pokédex request, Route 1
  item, first Route 22 rival, Viridian Forest loose items and trainers,
  Pewter escorts, and Brock's battle/badge/TM retry.
- Oak's original off-camera lab object swap is lowered into continuous
  movement for the modern whole-room camera. Starter inspection presents the
  imported front picture and cry before confirmation, and receipt emits the
  imported Get Key Item fanfare.
- Naming grids use directional navigation plus confirm/back only; presentation
  labels and choice cursors no longer overlap or point backward.
- Overworld sheets import four-phase directional walk clips, and presentation
  interpolates actors and the player continuously across Red's 16-frame
  logical cell steps.
- Red's upstairs room is placed one world cell above the downstairs room.
- Map-entry camera profiles give Pallet Town, Route 1, Viridian, Route 22, and
  interiors useful starting views without carrying fitted-space scale between
  them. Manual zoom/pan remains in force until the next map entry, or forever
  when automatic camera adjustment is disabled.
- Real battle turns now queue their imported move animations, including
  opponent-side mirroring, instead of leaving the animation executor isolated
  in the developer lab. Move-name, faint, EXP, and terminal result pages own
  the sequence, and terminal state returns to the world only after those pages
  are acknowledged. The two-column command menu navigates spatially.
- Gameplay battles and the developer Battle Lab own separately initialized
  mutable view state. Lab species cycling, animation queues, visibility, and
  UI state cannot leak into a campaign battle.
- A faint hides the defeated side in presentation state without changing its
  species identity. A later send-out event carries the replacement party
  index; that event changes the active battler and reveals it only after the
  faint and EXP presentation has completed.
- Grass cells repaint their imported foreground pixels over the player's
  feet. A total party defeat now heals and relocates to the last Center, or
  home before the first Center, halves money, and opens a blackout message.
- Trainer-battle instructions declare a defeat policy. The lab rival resumes
  its owning script, heals the party, and unlocks input after either result;
  ordinary trainers and wild encounters use the shared blackout route. A
  completed battle outcome is retained until its waiting campaign fiber has
  consumed it.
- The field menu includes a guarded QUIT action that returns to a freshly
  initialized title flow without changing the saved campaign.

## Player pass

Run:

```sh
./scripts/run.sh
```

Then exercise one continuous New Game:

1. leave Red's bedroom and house;
2. trigger Oak, choose/name a starter, and finish the lab rival encounter;
3. reach Viridian through connected Route 1, receive the parcel, and return it;
4. revisit a Pokémon Center and verify accept/cancel plus HP/status/PP;
5. defeat the first Route 22 rival and receive Oak's imported Ball stack,
   then catch a wild Pokémon, switch party members, fight, and run;
6. traverse Route 2 and Viridian Forest into Pewter;
7. enter Pewter Gym, defeat Brock, and receive the badge/TM branch;
8. use Start throughout to inspect party, bag, money, ID, and play time;
   verify QUIT defaults to NO and YES returns to the title;
9. save from Start, restart, choose Continue, and verify position, party,
   inventory, flags, NPC state, and play time.

Report several observations together. Screenshots are most useful for layout;
map name, actor, last dialogue, and exact action are most useful for logic.

## Explicit gaps after this handoff

- Direct party-member selection, item-use targeting, Pokédex, and options
  remain unfinished Start-menu owners. SAVE and title-menu CONTINUE use the
  modern semantic S-expression slot.
- PKMN currently rotates to the next usable party member rather than opening
  the final party-selection layout.
- Mart purchase UI is decoded as a shop interaction but is not yet executable.
- Move-learning replacement, evolution presentation, field moves, the full
  set of battle result/effectiveness pages, and remaining scripted jingles/effects
  remain later slices. Map, title, Oak-introduction, and battle music, cries,
  battle-animation sounds, and common menu sounds are active.
