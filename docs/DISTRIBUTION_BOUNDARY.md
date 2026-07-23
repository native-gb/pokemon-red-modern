# Distribution boundary

This is an engineering policy, not legal advice.

The repository distributes original engine code, import schemas, validators,
tools, and original fixtures. It does not distribute ROMs or generated Pokémon
Red campaign packs, graphics, dialogue, music, maps, encounter tables, parties,
or other extracted cartridge content.

A user supplies a supported cartridge locally. The importer verifies it and
creates a private, disposable cache. The executable remains useful as an engine
and tool host without that cache, and original campaigns can target the same
schemas.

Clean architecture still matters after local import:

- runtime code implements game concepts rather than translated assembly;
- import code knows ROM offsets, but gameplay code never does;
- scripts are semantic campaign programs, not instruction-for-instruction CPU
  traces;
- renderer and audio systems consume normalized assets and events;
- provenance makes the origin of imported records auditable;
- generated packs stay ignored by version control.

This boundary reduces what the project redistributes. It is not a guarantee
that every importer, compatibility behavior, name, or use of a third-party
property is legally risk-free. Public releases require jurisdiction-appropriate
legal review.
