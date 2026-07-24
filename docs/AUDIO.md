# Audio

Audio is imported content. The ROM importer decodes Red's three audio banks,
362 headers, 785 channel programs, 25,501 sequencer commands, waveforms,
pitch tables, 190 cries, and map/battle music dispatches. It writes:

- `compiled/audio_content.bin`, the runtime cache;
- `source/audio/audio_index.sexpr`, the readable music/effect index;
- `source/audio/map_music.sexpr`, all 248 map music bindings;
- `source/audio/scene_music.sexpr`, semantic boot-scene bindings.

The engine does not switch on Red map names. It asks the imported dispatch
table for the current map, battle kind, or semantic scene.

## Runtime behavior

The audio executor advances at 59.7275 Hz on unscaled wall-clock time.
Fast-forward therefore changes game simulation speed without changing music
tempo or pitch.

Two independent music voices support a 24-tick crossfade. Asking for the
already active bank and song is a no-op, so walking or warping between maps
that share music does not restart it.

Each sound effect and cry has an independent executor and PCM voice. Modern
does not reproduce the Game Boy channel-stealing limit: effects may overlap
music and one another without reinterpreting or interrupting a music channel.
The final mixer clamps the combined signal at the device boundary.

Current cue sources are:

- imported title and Oak-introduction scene music;
- imported per-map music;
- imported wild/trainer battle music;
- imported Pokémon cries during battle deployment;
- imported sound IDs emitted by battle animation programs;
- menu-open and menu-confirm/navigation sounds.

Additional campaign-specific music changes, victory themes, healing jingles,
and scripted overworld effects should be emitted as ordinary semantic audio
cues by their content programs rather than added as engine map cases.
