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
- Existing campaign fibers cover Oak interception, all starters, naming,
  the first lab rival battle, parcel handoff/return, Pokédex request, Route 1
  item, first Route 22 rival, Viridian Forest loose items and trainers,
  Pewter escorts, and Brock's battle/badge/TM retry.

## Player pass

Run:

```sh
./scripts/run.sh
```

Then exercise one continuous save-less New Game:

1. leave Red's bedroom and house;
2. trigger Oak, choose/name a starter, and finish the lab rival encounter;
3. reach Viridian through connected Route 1, receive the parcel, and return it;
4. revisit a Pokémon Center and verify accept/cancel plus HP/status/PP;
5. defeat the first Route 22 rival and receive Oak's imported Ball stack,
   then catch a wild Pokémon, switch party members, fight, and run;
6. traverse Route 2 and Viridian Forest into Pewter;
7. enter Pewter Gym, defeat Brock, and receive the badge/TM branch;
8. use Start throughout to inspect party, bag, money, ID, and play time.

Report several observations together. Screenshots are most useful for layout;
map name, actor, last dialogue, and exact action are most useful for logic.

## Explicit gaps after this handoff

- The modern Start overlay is read-only. Direct party-member selection,
  item-use targeting, Pokédex, options, and save/continue remain separate
  owners.
- PKMN currently rotates to the next usable party member rather than opening
  the final party-selection layout.
- Mart purchase UI is decoded as a shop interaction but is not yet executable.
- Blackout relocation can now use a stored healing checkpoint, but the
  transition/presentation owner is not connected.
- Move-learning replacement, evolution presentation, field moves, exact
  battle messages/animations, and full audio remain later slices.
