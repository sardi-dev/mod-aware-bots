# mod-aware-bots

[![License: GPL-2.0-or-later](https://img.shields.io/badge/license-GPL--2.0--or--later-blue)](LICENSE)
[![Core: SkyFire 5.4.8](https://img.shields.io/badge/core-SkyFire%205.4.8%20(18414)-informational)](https://github.com/ProjectSkyfire/SkyFire_548)
[![Release](https://img.shields.io/github/v/release/sardi-dev/mod-aware-bots?include_prereleases)](https://github.com/sardi-dev/mod-aware-bots/releases)
[![Unit tests: 1,700+](https://img.shields.io/badge/unit%20tests-1%2C700%2B-brightgreen)](#tests)

**Player bots for SkyFire 5.4.8 that play like people, not like pets.**

A home server with two or three humans feels empty. This module fills it with a few hundred bots that have their own
plans: they level and quest, fill your dungeon finder queue and say goodbye after the run, fight battlegrounds with a
plan for each map, climb a 2v2 arena ladder, duel in town, sell what they really looted on the auction house, and
remember who was kind to them. You lead them the way you lead players, with raid marks, roles and party chat, instead
of a page of commands.

There are two kinds: **random bots** that live in the world on their own, and **alt bots**, your own characters that
join your group. It ships as a [SkyFire_548](https://github.com/ProjectSkyfire/SkyFire_548) module plus a set of core
patches, an optional AI sidecar, and a design sheet for every class, role and battleground it plays. In a SkyFire
tree the module lives in `modules/mod-aware-bots-548`, the name the build and the configuration use.

> **v0.1.0, the first public release.** Early and honest: what is not proven yet is switched off by default, and every
> design sheet says what was verified. Built on [mod-playerbots for AzerothCore](https://github.com/mod-playerbots/mod-playerbots)
> and the Legends of Azeroth 5.4.8 version of it; their authors built the engine this module stands on
> (see [Credits](#credits-and-license)).

| 34 | 6 | 2v2 | 1,700+ | ~300 |
|:--:|:--:|:--:|:--:|:--:|
| specialisations with rotations | battleground playbooks | bot arena ladder | unit tests | bots in daily use on one server |

- [Why try it](#why-try-it)
- [What the bots do](#what-the-bots-do)
- [Status and known limitations](#status-and-known-limitations)
- [Quick start](#quick-start)
- [Configuration highlights](#configuration-highlights)
- [Requirements](#requirements) and [tests](#tests)
- [Full feature list](docs/FEATURES.md), [comparison](docs/COMPARISON.md), [install guide](docs/INSTALL.md),
  [roadmap](docs/ROADMAP.md)

## Why try it

This module would not exist without mod-playerbots, and much of its code is still theirs. It makes different design
choices, for a small server where a few humans play next to a few hundred bots.

- **They live; you lead.** Bots decide from their own state: they answer an invite because of the level gap, a quest
  nearby or what they just started, and they travel to your group instead of being summoned. In a dungeon they follow
  your marks and roles. The inherited chat commands for your own alt bots are still there when you want them.
- **Nothing is free.** No maintenance, autogear or best-in-slot commands, and the food, taxi and raid cheats are
  removed from the code. Bots pay for repairs, riding and professions, and wear what they looted, bought or crafted.
- **A market that is real.** Auction house sellers are bots that actually looted, gathered or crafted the goods, at
  prices from a researched price table, and buyers pay with gold they earned. Nothing is created out of nothing.
- **Battlegrounds with a plan.** Every supported map has a playbook (goals, positions, roles, call-outs, failure
  modes) written before the code. A leader bot calls the stance, bots call out what they see, and they read short
  human call-outs such as `inc bs 3`.
- **Bots with manners and memory.** Finder bots greet, ask about quests, wait for the loot and say goodbye. With the
  social life on, each bot keeps a liking score for each human: grouping, finished dungeons, trades and thanks raise
  it, rudeness and ninja looting lower it, and it shapes their group offers.
- **Skill you can feel.** Every random bot has a fixed tier (expert, good, casual) that sets how much of its class
  sheet it plays and how fast it reacts. The class sheets come from player guides, checked against the 5.4.8 spell
  data.
- **An AI voice, if you want one.** An optional sidecar asks a language model through OpenRouter for chat lines in
  player-to-player moments and for the battleground leader's strategy, with a rule-based fallback for every call and a
  monthly budget cap.
- **Measured, not guessed.** More than 1,700 unit tests of the decision logic, about 100 tests of the sidecar, and an
  in-game scenario harness for game masters (`.botscenario`, `.botprobe`).
- **Private and plug and play.** The economy, the social life and the AI layer are off until you turn them on; the
  liking table keeps no character names, and random bot accounts get random passwords. Install is a module folder, a
  set of core patches and SQL that the core's updater applies.

The careful version, with the trade-offs, is in [docs/COMPARISON.md](docs/COMPARISON.md).

## What the bots do

| Area | What you see | State |
|------|--------------|-------|
| World and leveling | Random bots quest, grind, gather, travel by road, boat and flight path, visit trainers and vendors, take breaks at inns, and log off for a while like players | in daily use |
| Groups | They answer group invites from their own situation (level gap, a quest nearby, what they just started), travel to your group instead of being summoned, and follow your marks | in daily use |
| Dungeons | The dungeon finder fills the roles missing in a human's queue with fitting bots; finder bots greet, ask about quests, wait for the loot and say goodbye; bot-only parties run dungeons on their own now and then | in daily use, bot-led runs beta |
| Classes and roles | Rotations for all 34 specialisations; class sheets written from player guides and checked against the 5.4.8 spell data drive 28 of them (six sheets are drafts). Tanks pull marks, healers keep shields and heal-over-time effects up within mana guardrails | beta |
| Skill tiers | Every random bot has a fixed tier (expert, good, casual) that sets how much of its class sheet it plays and how fast it reacts | in daily use |
| Battlegrounds | Per-map playbooks for Warsong Gulch, Twin Peaks, Arathi Basin, Battle for Gilneas, Eye of the Storm and Silvershard Mines: a leader bot calls the stance, bots call out what they see, and they read short human call-outs such as `inc bs 3` as information | beta |
| Arenas | A ladder of named 2v2 bot teams with their own rating, scheduled bot-versus-bot matches, a ladder NPC and ladder news in chat | beta (pilot) |
| Duels and 1v1 | Duel spots in towns; a 1v1 engine where ranged bots kite and melee bots chase, by skill tier | beta |
| PvP rules | What a bot saw the enemy use (trinkets, defensives, interrupts), interrupts late in the cast, defensives early, fake casts by experts, a shared kill window and crowd control chain | beta |
| Economy | Random bots list what they looted, gathered or crafted and buy what they need on their faction's auction house, at prices from a researched price table, while keeping gold back for repairs, training, riding and a mount | off by default, beta |
| Social life | A liking score per bot and human (grouping, finished dungeons, trades and thanks raise it, rudeness and ninja looting lower it), lasting playstyles, shared-quest group offers and answers to your emotes | off by default, beta |
| AI layer | An optional sidecar asks a language model through OpenRouter for chat lines in player-to-player moments and for the battleground leader's strategy; every call has a rule-based fallback and a monthly budget cap | off by default, beta |

Every row is detailed, with the document or configuration key behind it, in [docs/FEATURES.md](docs/FEATURES.md).

## Status and known limitations

The module ran for several weeks on a private SkyFire 5.4.8 server with about 300 random bots that level, quest,
group and run dungeons. Not everything is proven, and this is an early release:

- **The patch set.** The module builds on upstream `main` at the base commit in [patches/README.md](patches/README.md)
  plus the 25 patches (compile-checked on 2026-11-23). It ran day to day on a SkyFire tree with more changes, so a
  bare `main` server has not had the same weeks of live play yet.
- **Raids**: the raid strategies (20 raids) have not been run live.
- **Battlegrounds**: each playbook's first line states how far it got. Several are unit tested with live checks still
  pending. Rated battlegrounds and the Tol Barad battlefield are not available on SkyFire.
- **Arenas**: 2v2 only, bot teams against bot teams. Human queues against ladder teams, 3v3 and 5v5 come later.
- **Economy and social life**: off by default and still in soak testing. The auction house uses buyouts only (bots
  never bid).
- **AI sidecar**: it has no authentication, so it must stay on a private network next to the world server. Live use
  costs money at your provider (a monthly cap applies).
- **Shipped defaults**: `conf/playerbots.conf.dist` keeps several defaults of the base module (random start levels,
  periodic re-randomizing, teleports for level, 500 random bots). The largest population tested is about 300. Use the
  [living world profile](docs/INSTALL.md#5-the-living-world-profile).
- **Platform**: built and run on Linux only. Windows is untested.
- **Core gaps**: the places where SkyFire lacks something the module uses are listed in [docs/stubs.md](docs/stubs.md).
- The headless test client used for live measurement is not part of this release.

## Quick start

The full guide, with database and sidecar steps, is [docs/INSTALL.md](docs/INSTALL.md).

```sh
# 1. SkyFire at the base commit named in patches/README.md, plus the core patches
git clone https://github.com/ProjectSkyfire/SkyFire_548.git
cd SkyFire_548
git checkout -b bots f61bd333aa
git am --keep-cr /path/to/mod-aware-bots-548/patches/*.patch

# 2. The module, in a folder of exactly this name
git clone https://github.com/sardi-dev/mod-aware-bots.git modules/mod-aware-bots-548

# 3. Build with the module switched on
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/opt/skyfire -DAWARE_BOTS=ON
cmake --build build && cmake --install build
```

Then create the `playerbots` database and load `sql/playerbots/`, copy `playerbots.conf.dist` to `playerbots.conf`
next to `worldserver.conf`, set `PlayerbotsDatabaseInfo`, and start the world server. The module's SQL for the auth,
characters and world databases is applied by the core's updater on start (`Updates.Modules`, from patch 0013).

## Configuration highlights

All settings are in [conf/playerbots.conf.dist](conf/playerbots.conf.dist), each with a comment. SkyFire does not
reload this file: edits need a restart.

| Key | Default | What it does |
|-----|---------|--------------|
| `AiPlayerbot.Enabled` | 1 | Turns the whole module off without a rebuild |
| `AiPlayerbot.MinRandomBots`, `MaxRandomBots` | 500 | Random bots online; start lower (100 to 300) and grow |
| `AiPlayerbot.RandomBotRandomPassword` | 1 | Random passwords for bot accounts. With 0 the password is the account name: never on a reachable server |
| `AiPlayerbot.SkillTiers.ExpertPercent`, `GoodPercent` | 10, 60 | The share of expert and good bots; the rest are casual |
| `AiPlayerbot.LfgFill.Enable` | 1 | Fill a human's dungeon finder queue with fitting bots |
| `AiPlayerbot.LfgBotRuns.Enable` | 1 | Bot-only dungeon runs now and then |
| `AiPlayerbot.Pvp.Leader.Enable` | 1 | One leader bot per battleground side calls the stance |
| `AiPlayerbot.ArenaTeams.Enable` | 1 | The bot arena ladder |
| `AiPlayerbot.OneVsOne.Enable` | 1 | The 1v1 engine for duels and lone fights |
| `AiPlayerbot.Auction.Enable` | 0 | The bots' auction house (`Auction.OnlyBots` limits it to named bots for a trial) |
| `AiPlayerbot.Social.Liking.Enable`, `Social.Playstyle.Enable` | 0 | The social life; while off nothing is read or written |
| `AwareAi.Enable`, `AwareAi.Chat.Enable` | 0 | The AI layer and its chat; while off the module never contacts the sidecar |
| `AwareAi.Chat.DenyBeingBot` | 0 | When on, the chat prompt tells the bots to stay in character as players; off, a bot asked directly may say it is a bot |

**Privacy of the social life.** With liking on, the module creates `playerbots_social_liking` and
`playerbots_social_offer` in the playerbots database itself (the files in `sql/playerbots/updates/` are for fresh
installs), keeps no character names in them, and a game master can delete a character's rows with
`.playerbots social wipe <character name>|all` (with the AI layer on, the sidecar forgets that player too). See
[docs/social/social.md](docs/social/social.md).

## Requirements

- SkyFire_548 at upstream `main`, at the base commit named in [patches/README.md](patches/README.md), with the patches
  in `patches/` applied (mostly hooks and read-only getters, plus auction and mail helpers and three maps the core
  lacked).
- SkyFire's own build requirements (its README: a C++23 compiler, GCC 14 or newer on Linux, CMake, Boost, OpenSSL,
  the MySQL client libraries). The module needs nothing beyond them.
- A MySQL server with SkyFire's `auth`, `characters` and `world` databases and one more, `playerbots`.
- Optional, for the AI layer: Docker (or Python 3.12) for the sidecar, and an OpenRouter key in the environment.

## Tests

```sh
tests/run.sh                                         # the module's unit tests: only g++ with C++20 needed
cd services/aware-ai && pytest && ruff check . && mypy   # the sidecar, with its dev dependencies installed
```

The unit tests cover the module's pure decision logic (policies for battlegrounds, arenas, duels, pricing, the
reserve fund, liking, playstyles, chat pacing and more) with no game core and no test framework. The sidecar tests
mock OpenRouter and use no network. Run `tests/run.sh` before every build.

## Repository layout

| Folder | Content |
|--------|---------|
| `src/` | The module: bot sessions and managers, the AI engine, strategies, actions, triggers and values, class rotations; `src/Arena/` (the ladder), `src/PvP/`, `src/AwareAi/` (the sidecar client), `src/Scenario/` (the test harness) |
| `conf/` | The example configuration |
| `sql/` | The `playerbots` database schema and data |
| `data/sql/` | The module's SQL for the core databases, applied by the core's updater, and its uninstall files |
| `patches/` | The core patches against SkyFire_548 (see its README) |
| `services/aware-ai/` | The optional AI sidecar (Python, see its README) |
| `tests/` | Unit tests of the pure logic (`tests/run.sh`) |
| `tools/` | Scripts used during the port and the economy audit tools |
| `docs/` | Class, role and battleground sheets, PvP, duels, arena, raids, economy and social design, porting notes |

**Reading the docs.** Ids such as `BT-154` in comments and documents come from the maintainer's private tracker, and
"the owner" in them means the project's maintainer. Numbered "design decisions" are explained in
[docs/decisions.md](docs/decisions.md). Each sheet says at the top what was verified, what is inference and what still
needs a live check.

## Feedback and contributing

This is an early release, and reports from other servers are the most useful thing right now. Please open an issue with the
SkyFire commit, the module commit, the configuration lines you changed and the module's log lines around the problem
(they carry tags such as `[BOTPARTY]`, `[ARENALADDER]` or `[SOCIAL]`). Pull requests are welcome; please run
`tests/run.sh` first and add a test for new decision logic.

## Credits and license

GPL-2.0-or-later, see [LICENSE](LICENSE), [NOTICE.md](NOTICE.md) (how the licenses fit together) and
[CREDITS.md](CREDITS.md).

- **mod-playerbots for AzerothCore** and its contributors, including the original playerbots authors: the bot engine,
  strategies and much of the code here.
- **[Legends of Azeroth Pandaria 5.4.8](https://github.com/Legends-of-Azeroth/Legends-of-Azeroth-Pandaria-5.4.8)**: this module is based on their 5.4.8 version of mod-playerbots, and several
  core getters in `patches/` are based on their core.
- **AzerothCore**, **TrinityCore** and **The Cataclysm Preservation Project**, as listed in `CREDITS.md`.
- **SkyFire**, the core this module runs on. The core patches follow SkyFire's license (GPL-3.0).

Original copyright notices are kept in every file that came from another project. No game client data is part of
this repository.
