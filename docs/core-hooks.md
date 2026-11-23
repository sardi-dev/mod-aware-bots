# Core changes the module needs

The module runs on SkyFire 5.4.8 plus the small core changes in `patches/` (see `patches/README.md` for how to apply
them and what they cover). Everything else the module needs is in `src/compat/`.

SkyFire upstream already provides bot sessions (`WorldSession::SetBot`, `IsBot`, `BotActivity`, the async
`BeginBotCharacterLogin` / `PollBotCharacterLogin`, `LoginBotCharacter`, `FinalizeBotTeleport`,
`CreateBotCharacter`; added 2026-09-22) and `WorldSession::SetPacketObserver`.

| Patch | Change | Why the module needs it |
|-------|--------|-------------------------|
| 0015 | Let server scripts see every packet a session sends | bots have no socket; their AI reads the packets the server sends them |
| 0016 | Let server scripts see the packets a session receives | a master's own packets tell its bots what the master does |
| 0001 | Load the talent column from Talent.dbc | talent choice by row and column (MoP talents) |
| 0002 | Expose the spawn stores and a map's transports read-only | searching spawns (quest givers, objects) and the transport under a position |
| 0005 | Let modules read battleground node and capture point state (read-only getters: `BattlegroundAB::GetNodeStatus(node)` 0 neutral, 1 and 2 contested by the Alliance and the Horde, 3 and 4 occupied by them; the Alterac Valley and Eye of the Storm getters) | the Arathi Basin plan classifies every node for each team from the node state and decides who restores, assaults or guards it; the contest timer and the previous node state stay private, so the plan infers the 60 s timer from the moment it saw the state change |
| 0007 | Let modules read the Battle for Gilneas node states (read-only getter `BattlegroundBFG::GetNodeStatus(node)`, the form of the Arathi Basin one) | the Battle for Gilneas plan reads the node state at the moment of the click |
| 0008 | Let modules read the Eye of the Storm capture bar (read-only getter `BattlegroundEY::GetPointBarStatus(point)`, 0 to 100: 50 is the neutral start, the Alliance owns a base at 70 or more, the Horde at 30 or less) | the Eye of the Storm plan reads how hardened a base is; without the getter the module would infer it from the players it sees |
| 0006 | Let modules read capture point, battlefield and encounter state | the outdoor PvP and instance checks of the bots |
| 0003 | Remember the result a spell sent when its cast failed (`Spell::GetCastResult`) | code that casts through `prepare()` cannot tell whether the cast was refused |
| 0004 | Refuse to log in a character that is already in the world | a module that logs characters in without a client must not put one in the world twice |
| 0009 to 0012 | Auction listing and bidding in `AuctionHouseMgr`, taking mail money and items in `Player` | the bots' auction house and mail |
| 0013 | The database updater applies module SQL | the module's own SQL (`data/sql`) |
| 0014 | `VMAP::ModelIgnoreFlags` | the module's line-of-sight checks |
| 0017 to 0022 | Silvershard Mines, Deepwind Gorge, Tiger's Peak, Temple of Kotmogu rules | the battleground tactics for those maps |
