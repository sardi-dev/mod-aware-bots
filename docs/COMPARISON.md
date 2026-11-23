# How this module compares

**mod-aware-bots-548 builds on mod-playerbots.** Its engine (bot sessions, the strategy, action, trigger and value
system, most class rotations, the dungeon and raid strategies) comes from
[mod-playerbots for AzerothCore](https://github.com/mod-playerbots/mod-playerbots) through the
[Legends of Azeroth 5.4.8](https://github.com/Legends-of-Azeroth/Legends-of-Azeroth-Pandaria-5.4.8) version of it,
adapted to SkyFire. Their original headers are kept in every file that came from them. Nothing here is meant as a
judgement of that work: mod-playerbots is far more mature, has many contributors and years of testing, and serves a
different core.

What follows are the **design choices** this module made differently, for one kind of server: a small or home server
on SkyFire 5.4.8 where a few humans play next to a few hundred bots and want the bots to feel like other players.

## Design choices

"Common alternative" describes a design many playerbot setups use, including the defaults of the base module where
that is stated. It is not a claim about every project.

| Design choice | This module | Common alternative | Trade-off |
|---------------|-------------|--------------------|-----------|
| Core | SkyFire_548 (client 5.4.8, build 18414) | mod-playerbots runs on AzerothCore (client 3.3.5a); the Legends of Azeroth version runs on their own 5.4.8 core | Only for SkyFire; where the core lacks something the module uses, `docs/stubs.md` lists it |
| Core changes | A numbered patch set against upstream `main` (`patches/`): hooks and read-only getters, auction and mail helpers, a module SQL updater, and three maps the core lacked | The base modules need a modified core as well (a dedicated core branch, or changes across the core) | You apply patches before building; until the hooks land upstream, each SkyFire update may need a rebase |
| Control | Autonomous: bots act from their own state and are led like players (raid marks, roles, ready checks, party chat); the inherited chat commands for alt bots remain, no new command system was added | A rich command set (and often addon buttons) to direct bots | Less direct control; gaps in behaviour show instead of being covered by a command |
| Upkeep | No maintenance, autogear or best-in-slot commands; no food, taxi or raid cheats (removed from the code); no free repair or revive on death | mod-playerbots offers maintenance and autogear commands, switchable in its configuration | Bots are weaker and slower to gear; a missing behaviour (a trainer visit, a repair) is a visible bug |
| Levels and travel | With the living world profile (`docs/INSTALL.md`): random bots start at level 1, level by playing and travel by road, boat and flight path | Random levels on creation, periodic re-randomizing (new level, gear and gold) and teleports to level zones. The shipped `.conf.dist` still has these base defaults | The world fills slowly after a fresh start; travelling bots can get stuck (a stranded-bot recovery exists) |
| Economy | Bots list goods they really looted, gathered or crafted and buy with buyouts from gold they earned, at prices from a researched table with a vendor floor, keeping a reserve for repairs, training, riding and a mount | A separate auction house bot that creates listings from item lists and buys at configured rates | A thinner auction house that depends on the bot population; off by default and still in soak testing |
| Battlegrounds | A written playbook per map (six so far) built in code: a leader bot, stances, call-outs, a reader for human call-outs, skill tiers | Per-map tactics in code (routes and objectives), without a written plan behind them | Much more work per map; maps without a playbook play worse |
| Arenas | Named bot teams with their own rating ladder, schedule and ladder NPC, because the 5.4.8 core keeps no arena rating | Bots form groups on demand to fill arena queues | 2v2 only and bot against bot in this release |
| Class play | Class sheets per specialisation, compared across several player guides and checked against the 5.4.8 spell data before any rotation change; play split into expert, good and casual tiers | Rotations tuned in code per specialisation | Slower to change; every change has a written reason |
| Information | Bots act on what a player could see: a book of what each bot saw the enemy use, no shared hidden state; in raids, facts the base code read from boss scripts are now read from what the bots see | Bot logic that reads server-side state where it is convenient | Bots make mistakes a cheating bot would not |
| Social life | Optional liking score per bot and human, lasting playstyles, shared-quest offers, answers to emotes, with privacy rules (no names stored, wipe command) | Varies | More state to keep and explain; off by default |
| AI | Optional sidecar via OpenRouter for player-to-player chat and battleground strategy, with a monthly budget cap and a rule-based fallback for every call; PvE stays deterministic | Some setups add language model chat through a separate add-on, with a local or hosted model | Costs money at the provider, and chat lines leave your server when chat is on; no local model support is documented yet |
| Testing | More than 1,700 unit tests of pure decision logic, about 100 sidecar tests, an in-game scenario harness, live benches with a headless test client | Varies | The headless client is not in this release |
| Defaults | The economy, social life and AI layer are off; random bot accounts get random passwords | Varies | You switch features on yourself |

## When to use which

- **You run AzerothCore 3.3.5a**: use mod-playerbots. It is the mature original, with a large community.
- **You run the Legends of Azeroth 5.4.8 core**: their own bots fit their core.
- **You run SkyFire 5.4.8 and want a living world for a few players**: try this module, and tell us where it falls
  short.
- **You want bots as tools you command** (gear on demand, upkeep by command): the base modules' command sets fit that
  better than this module's design.

## Credits

See `CREDITS.md` and `NOTICE.md`. If you find code here that is not credited correctly, please open an issue.
