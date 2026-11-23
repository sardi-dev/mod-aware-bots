# Notice

## License of this repository

The module (`src/`, `conf/`, `sql/`, `data/`, `tests/`, `tools/`, `docs/`, `module.cmake`) and the AI sidecar
(`services/aware-ai/`) are licensed under the **GNU General Public License, version 2 or (at your option) any later
version** (GPL-2.0-or-later). The text of version 2 is in `LICENSE`.

Why this license:

- The module is derived from mod-playerbots for AzerothCore and from the 5.4.8 version of it by Legends of Azeroth,
  both released under GPL version 2 "or (at your option) any later version", and from files that carry TrinityCore's
  "version 2 or any later version" header.
- The SkyFire core it runs on is GPL-3.0. GPL-2.0-only code cannot be combined with GPL-3.0 code, but code that is
  "or any later version" can, so the module is offered as GPL-2.0-or-later and the combined server is GPL-3.0.
- The files written for this module carry the header `This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see
  LICENSE)`. Files ported from other projects keep their original header unchanged. Some ported files came without a
  header; they are covered by the license of this repository.

The files in `patches/` are changes to SkyFire_548 and carry SkyFire's license, GPL-3.0.

## What is not here

No game client data is part of this repository (no maps, DBC or DB2 files, models, sounds or images), and none is
needed to read it. The names of classes, spells, zones and bosses are used only to describe game content.

The sidecar needs a key for the OpenRouter service to reach a language model. The key is read from the environment
(`OPENROUTER_API_KEY`) and is never part of this repository.

## Third-party guides

The class, role and battleground sheets in `docs/` were written from player guides, patch notes and the server's own
data. The guides are cited by name and read date; they are summarised, not copied.

See `CREDITS.md` for the projects this work builds on.
