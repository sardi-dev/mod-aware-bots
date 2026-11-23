# Credits

- **mod-playerbots for AzerothCore** (GPL-2.0-or-later): the playerbots module this project is derived from, and its
  contributors, including the original playerbots authors.
- **Legends of Azeroth Pandaria 5.4.8** (GPL-2.0): its 5.4.8 version of mod-playerbots is the base of this project, and
  several core getters in `patches/` are based on its 5.4.8 core.
- **AzerothCore**: ported pieces keep the header of the file they came from.
- **TrinityCore**: the names that `src/compat/` provides on SkyFire follow TrinityCore's API; code taken from it keeps
  its original header.
- **The Cataclysm Preservation Project**: a reference for 4.3.4 behaviour (timings, ids), named in the comments where
  it was used.
- **Earlier work by the author** (GPL-2.0-or-later): the idea of a stable per-bot personality derived from the guid, and
  its SplitMix64 mixing, in `src/Mgr/Text/BotVoice.cpp`.
- **SkyFire** (GPL-3.0): the core this module runs on.

Every ported file keeps its original license header. See `NOTICE.md` for how the licenses fit together.
