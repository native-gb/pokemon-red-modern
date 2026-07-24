# Audio

Audio is imported content. The ROM importer decodes Red's three audio banks,
362 headers, 785 channel programs, 25,501 sequencer commands, waveforms,
pitch tables, 190 cries, and map/battle music dispatches. It writes:

- `compiled/audio_content.bin`, the runtime cache;
- `source/audio/audio_index.sexpr`, the readable music/effect index;
- `source/audio/map_music.sexpr`, all 248 map music bindings;
- `source/audio/scene_music.sexpr`, semantic boot-scene bindings;
- `source/audio/move_sounds.sexpr`, all 166 move-to-effect records with
  their frequency and tempo modifiers.

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

- imported intro-battle carry, title, and Oak-introduction scene music;
- ROM-timed title crash and version-whoosh cues derived from the imported
  bounce/delay schedule;
- imported per-map music;
- imported wild/trainer battle music;
- imported Pokémon cries during battle deployment;
- imported move IDs emitted by battle animation programs, resolved through
  Red's separately imported `MoveSoundTable` before playback;
- menu-open and menu-confirm/navigation sounds;
- bank-correct go-inside, go-outside/stair, and ledge-hop sounds.
- Pokémon presentation cries and Get Key Item fanfares emitted by campaign
  instructions.

Campaign programs can retain a semantic music scene across movement and warps,
then explicitly restore ordinary imported map music. Oak's Pallet interception
uses this path from “Hey! Wait!” through the completed lab-entry movement.

Additional campaign-specific music changes, victory themes, healing jingles,
and scripted overworld effects must be emitted as ordinary semantic audio
cues by their content programs rather than added as engine map cases. The
verified source census and remaining owners are tracked in
[PRESENTATION_ISA_AUDIT.md](PRESENTATION_ISA_AUDIT.md).

Cry executors select their audio bank directly. They do not start and stop a
dummy music header; that older workaround cleared the stereo mask immediately
before the cry and could make every wild deployment silent.
