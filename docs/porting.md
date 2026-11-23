# Porting notes

How this module moves from Legends of Azeroth's (LoA) 5.4.8 core to SkyFire 5.4.8. Decisions that last are in
design decisions 0011 and 0013 (see `docs/decisions.md`).

## Approach

1. Build SkyFire with this module enabled (`-DAWARE_BOTS=ON`, Ninja `-k 0` so one pass lists every failing file).
2. Fix the core-facing foundation first, then the rest in dependency order (below).
3. Core changes are small, additive hooks, kept as the patches in `patches/` and listed in `core-hooks.md`.

## Rules for the port

- Keep LoA's and AzerothCore's code and behaviour; change only what SkyFire's APIs require.
- Prefer one mechanical rule applied everywhere (recorded below) over hand edits that differ file to file.
- Temporary stubs are listed in `stubs.md` and removed before a feature counts as working.

## Main API differences (from the static survey of 2026-11-02)

| LoA | SkyFire | Plan |
|-----|---------|------|
| Rich `ObjectGuid` (about 930 uses: `GetCounter`, `IsEmpty`, `IsPlayer`, `Empty`, `Create<HighGuid::X>`, `GetRawValue`) | `uint64` GUIDs; `ObjectGuid` is a small packet helper with implicit conversions; `GetObjectGUID()` returns it | Extend SkyFire's `ObjectGuid` with the missing members, additive, in the core (like the TrinityCore compatibility layer, design decision 0008); `->GetGUID().` becomes `->GetObjectGUID().` where a member is called |
| `TC_LOG_*` (about 720) | `SF_LOG_*` | aliases in the module's compatibility header |
| `GetLevel`, `GetClass`, `GetRace`, `GetGender`, `IsDead` on units | `getLevel`, `getClass`, `getRace`, `getGender`, `isDead` | edit call sites (receiver must be a unit) |
| `Trinity::` grid notifiers | `Skyfire::` | edit call sites; the chat command API differs and is rebuilt by hand |
| `PlayerbotsDatabase` connection | none | a SkyFire-style database pool for the `playerbots` database (core or module) |
| `CharacterDatabasePreparedStatement`, `CharacterDatabaseTransaction` | `PreparedStatement`, `SQLTransaction` | aliases |
| `Cell::VisitAllObjects` | `WorldObject::VisitNearbyObject` | edit call sites |
| Scoped type ids (`TYPEID_UNIT`) | `TypeID::TYPEID_UNIT` | edit call sites |
| `PlayerbotScript` hooks, packet hook in `WorldSession::SendPacket`, bot session update and logout | `WorldSession::IsBot`/`SetBot`, async bot login (upstream, 2026-09-22), `WorldScript`, `PlayerScript` | add only the missing hooks |
| Spawn enumeration (`GetAllCreatureData`), `Map::GetTransportForPos` | private stores, GUID lookup only | small const accessors and a position query |

## Order

1. Foundation: compatibility header, `ObjectGuid` extension, session construction, database pool, script hooks.
2. Shared interfaces: `Playerbots.h`, `Utils`, the base `strategy` interfaces, `Mgr`.
3. `AI`, `strategy` base, `value`, `triggers`, `actions`, `generic`.
4. `Factory` and `Manager` (sessions, logins, the database).
5. `Travel`, RPG and the class strategies.
6. Dungeons, battlegrounds, `Scenario`, commands.
