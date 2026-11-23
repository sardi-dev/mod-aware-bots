# Core patches

The module runs on SkyFire_548 plus the changes in this folder. They are plain `git format-patch` files, in the order
they must be applied, made against upstream `main` at commit `f61bd333aa` (2026-11-23).

```sh
git -C SkyFire_548 checkout -b bots f61bd333aa
git -C SkyFire_548 am --keep-cr path/to/mod-aware-bots-548/patches/*.patch
```

`--keep-cr` matters: `Player.cpp` has CRLF line endings upstream and the patches keep them. `git apply` of each patch in
order gives the same tree.

The files in this folder are changes to SkyFire and carry SkyFire's license (GPL-3.0).

| Patches | What | Needed by |
|---------|------|-----------|
| 0001 to 0004 | Talent column from `Talent.dbc`; read-only spawn stores and map transports; the result of a refused spell cast; refuse a second login of a character that is already in the world | talent choice by row and column, quest giver and transport searches, casting, bot logins |
| 0005 to 0008 | Read-only getters for battleground nodes, capture points, the Eye of the Storm bar, outdoor capture points, battlefield and encounter state | the battleground plans |
| 0009 to 0012 | Auction listing and bidding in `AuctionHouseMgr`, taking mail money and items in `Player`, with the handlers calling them | the bots' auction house and mail |
| 0013 | The database updater applies SQL from each module's `data/sql/db-auth`, `db-characters`, `db-world` (setting `Updates.Modules`) | the module's own SQL |
| 0014 | `VMAP::ModelIgnoreFlags` for line-of-sight checks (small doodads no longer block spells) | the module's line-of-sight checks pass the flag |
| 0015, 0016 | `ServerScript::OnSessionPacketSend` and `OnSessionPacketReceive` | bots have no socket, their AI reads the packets the server sends them |
| 0017 to 0022 | The Silvershard Mines and Deepwind Gorge battlegrounds, the Tiger's Peak arena, the reworked Temple of Kotmogu rules, carrier positions | the battleground tactics for those maps |
| 0023 | `ThreatManager::GetThreatenedByMeList()` and `HostileReference::GetOwner()`, taken from the `zones-dungeons` branch (commit `87a504a9d4`, DigiD702) | the raid strategies that look for the adds a player holds (Molten Core, Serpentshrine Cavern) |
| 0024 | `GameObjectAI::OnReportUse`, so that the report of a game object use no longer runs `GossipHello` a second time (ported from TrinityCore 3.3.5) | the End Time dungeon strategy calls the hook for the Hourglass of Time (the script that implements it is in the dungeon branches, not in `main`) |
| 0025 | `Duration.h` (the chrono shorthands `Milliseconds`, `Seconds` and the `std::chrono` literals, included from `Object.h` and `Util.h`), `Position::GetPosition()` and `Player::GetTalentSpecialization()` without arguments, taken from the `zones-dungeons` branch (commit `088340f737`, DigiD702) | timers across the module, position copies in movement and strategy code, and the specialization reads of the class strategies; drop it once `main` has them |

## State of this set

- Applies cleanly to the base commit above (checked with `git am --keep-cr` and with `git apply`, patch by patch).
- **Compile-checked on 2026-11-23:** upstream `main` at the base commit plus patches 0001 to 0025 builds with the
  module (`-DAWARE_BOTS=ON`, GCC 14, the whole tree: 0 errors). Patch 0025 came from that build; it brings the pieces
  the module took from the maintainer's `zones-dungeons` branch, on which it ran day to day.
- Parts of this set overlap with upstream: `main` has its own `WorldSession::SetPacketObserver`, which covers the sending
  side of 0015, and its own `PathGenerator::CalculatePath` overload with a start point (the module's version of that
  patch was dropped for it).
