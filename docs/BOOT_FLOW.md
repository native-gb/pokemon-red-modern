# Normal boot flow

Normal startup is a campaign path, not a debug laboratory. Pokemon Red
presentation data is imported from the user's supported ROM into
`compiled/boot_content.bin`; generic runtime code owns only the state machine,
input semantics, and rendering machinery.

## Implemented vertical slice

The importer currently accounts for:

- the logo, copyright/Game Freak strip, Red Version strip, title trainer and
  ball graphics;
- all 16 cartridge-selected title Pokemon and the complete bounce, scroll, and
  ball-position timing tables;
- main-menu and option labels and option-row text;
- Professor Oak, Nidorino, player, rival, and shrink-sequence portraits;
- all eight opening Oak text programs and both sets of default names;
- UI/font/textbox/status tiles, fade/delay tables, and the imported New Game
  world placement.

The runtime currently executes:

- animated title entry, bouncing ball, and title-Pokemon cycling;
- the main menu's New Game and Options paths;
- interactive option rows;
- Oak dialogue pages, portrait transitions, default-name menus, and the
  imported controller naming grid plus ordinary typed name entry;
- the shrink ending and transfer into the imported initial world placement.

All boot images are normalized once by the importer and uploaded once as GPU
textures. The renderer does not decode the ROM or rebuild images per frame.

## Required remaining work

This slice is not the completed boot contract. The following remain explicit
gaps:

- import and execute the splash, music, cries, and exact sound-event programs;
- import and render the cartridge palette/SGB programs and exact fades;
- replace page-at-a-time dialogue with the complete text timing, input-wait,
  and text-sound program;
- add Continue only after semantic save/load and initial-state construction
  exist, and implement the clear-save chord;
- construct the complete New Game party, bag, flags, variables, Pokédex,
  options, clocks, and save state before campaign control begins;
- validate exact boot timing and presentation against imported program
  provenance rather than guessed host timing.

The boot path is complete only when title, Continue/New Game, Oak/naming, and
initial campaign setup are controller-playable from a clean imported cache.
