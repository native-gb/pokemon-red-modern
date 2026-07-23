# Public release policy

This document is an engineering and release policy, not legal advice. It is
written around United States copyright and trademark concerns. A qualified
attorney must review an actual public release.

## Position

The project should be released as an independently written monster-RPG engine
with local compatibility importers. It should not be packaged or represented
as a redistributable PC edition of Pokémon Red.

Being more conservative than another reverse-engineering project is useful
engineering precedent, but it is not a license, court ruling, or safe harbor.
The continued availability of another project does not establish that its
architecture or ours is lawful.

Our strongest boundary is:

- the repository contains original engine, tool, schema, and fixture code;
- the user supplies a lawfully obtained, supported ROM locally;
- importers decode that ROM into ignored local source and caches;
- the repository and release archives contain no generated cartridge content;
- runtime code implements semantic game concepts rather than translated CPU
  instructions;
- the engine has substantial use with original campaigns and assets.

## Risk levels

| Release shape | Practical risk assessment |
| --- | --- |
| Private development and local imports | Lower exposure, but not guaranteed lawful |
| Public engine source with original fixtures and no extracted material | Moderate |
| Public ROM-profile importer that reconstructs the complete Red experience | Moderate to high |
| Binary marketed as a Pokémon Red PC port | Moderate to high |
| Generated campaign pack, extracted graphics, music, text, maps, or ROM | High; do not distribute |

The local-import boundary materially reduces redistribution risk. It does not
decide whether every act of reverse engineering, local extraction, adaptation,
or exact presentation is fair use.

## Why the boundary helps

Copyright does not extend to ideas, procedures, processes, systems, methods of
operation, concepts, principles, or discoveries. It can protect the particular
code, graphics, music, writing, audiovisual presentation, and creative
selection or arrangement which embody them.

United States reverse-engineering decisions provide useful support when
intermediate copying is necessary to understand unprotected functional
elements or create interoperability. Their facts are not identical to this
project:

- *Sega v. Accolade* concerned study of interfaces followed by independently
  written games.
- *Sony v. Connectix* concerned a console-compatible emulator whose distributed
  final product did not contain Sony's BIOS.
- this engine is tailored to reconstruct a particular game and therefore has a
  less favorable purpose and market-substitution argument.

Owning a cartridge is an important fact, but it is not a blanket authorization
to make or distribute every adaptation. Likewise, requiring a ROM is an
engineering boundary rather than a complete legal defense.

## Repository requirements

Before every public push or release audit:

- no ROM or ROM fragment is tracked;
- no locally generated pack or readable imported source is tracked;
- no extracted bitmap, tile, font, audio, dialogue, map, party, encounter,
  animation table, or script is tracked;
- no literal or mechanically translated assembly routine is shipped as modern
  runtime code;
- importer code contains bounded decoders, validation, supported-profile
  locations, and independently written semantic lowering;
- provenance distinguishes original fixtures from local imported material;
- `.gitignore`, packaging rules, and CI all reject generated import roots;
- release archives are inspected independently of Git status.

Small facts such as identifiers, counts, hashes, offsets, formats, and
functional relationships may be required by a compatible importer. They must
not become a pretext for embedding expressive cartridge tables in source.

## Importer requirements

Importers must:

- accept only user-selected local bytes;
- verify supported ROM identity;
- never download, discover, upload, or redistribute a ROM;
- never upload generated content or include it in telemetry;
- write generated data only into an explicitly ignored local root;
- make cache invalidation and deletion straightforward;
- keep cartridge data out of crash reports and logs;
- prefer semantic normalized records over instruction-for-instruction traces.

This project should not provide cartridge dumping, decryption, access-control
circumvention, keys, or links to unauthorized copies. If future hardware
support implicates an effective access control, it requires a separate legal
review under 17 U.S.C. §1201.

## Branding requirements

Public packaging should use an original project and product name. Pokémon and
Pokémon Red may be mentioned only as factual compatibility references where
needed.

Do not use:

- the official Pokémon logo as project branding;
- official box art, screenshots, or extracted artwork in release pages;
- wording that suggests endorsement, authorization, or official status;
- Nintendo, Game Freak, Creatures, or Pokémon Company trade dress.

Public pages should contain a clear non-affiliation notice. Trademark ownership
notices and disclaimers reduce confusion but do not authorize use.

## Strengthening noninfringing use

The engine should ship with an original small campaign containing original
species, maps, text, graphics, music, and scripts. This is valuable in its own
right and demonstrates that the engine, schemas, tools, and mod system have
substantial uses independent of Pokémon Red.

The public engine should remain capable of:

- authoring and running original campaigns;
- loading independently created packages;
- validating content and scripts without a Pokémon ROM;
- exercising every executor with original fixtures;
- developing mods which do not include third-party copyrighted material.

## Release gate

No public binary release should be called legally cleared until an IP attorney
has reviewed:

- the exact repository history and current tree;
- importer implementation and generated output;
- build and release archives;
- project name, website, screenshots, and compatibility wording;
- license provenance for every dependency and original fixture;
- the intended countries of distribution.

## Primary references

- [17 U.S.C. §§101–122](https://www.copyright.gov/title17/92chap1.html)
- [17 U.S.C. §1201](https://www.copyright.gov/title17/92chap12.html)
- [Current 37 C.F.R. §201.40 exemptions](https://www.copyright.gov/title37/201/37cfr201-40.html)
- [Sega Enterprises Ltd. v. Accolade, Inc.](https://law.justia.com/cases/federal/appellate-courts/F2/977/1510/305345/)
- [Copyright Office summary of Sony Computer Entertainment, Inc. v. Connectix Corp.](https://www.copyright.gov/fair-use/summaries/sony-connectix-9thcir2000.pdf)

See also [DISTRIBUTION_BOUNDARY.md](DISTRIBUTION_BOUNDARY.md) for the technical
import boundary.
