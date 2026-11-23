# Install

From a SkyFire_548 checkout to a world server with bots, then the optional AI sidecar. Commands are for Linux, the only
platform the module has been built and run on so far.

1. [What you need](#1-what-you-need)
2. [Apply the core patches](#2-apply-the-core-patches)
3. [Add the module and build](#3-add-the-module-and-build)
4. [Databases](#4-databases)
5. [The living world profile](#5-the-living-world-profile)
6. [First start and checks](#6-first-start-and-checks)
7. [Optional: the AI sidecar](#7-optional-the-ai-sidecar)
8. [Updating and uninstalling](#8-updating-and-uninstalling)

## 1. What you need

- The SkyFire_548 source and its build requirements (see SkyFire's README: a C++23 compiler, GCC 14 or newer on Linux,
  CMake, Boost, OpenSSL and the MySQL client libraries). The module needs nothing beyond them; its sidecar client uses
  Boost.Asio, Beast and JSON, which are part of Boost.
- A working SkyFire server (auth, characters and world databases, client data extracted for the core). If SkyFire
  does not run without the module, fix that first.
- `g++` for the unit tests.
- Optional: Docker (or Python 3.12) and an OpenRouter key for the AI sidecar.

Plan for memory: the maintainer's server runs about 300 random bots. Start with fewer (section 5) and grow while you
watch the world server's update time and memory.

## 2. Apply the core patches

The patches in `patches/` are plain `git format-patch` files made against one upstream commit. Read
[patches/README.md](../patches/README.md) first: it names the base commit (`f61bd333aa` at the time of writing), what
each patch does, and the state of the set.

```sh
git clone https://github.com/ProjectSkyfire/SkyFire_548.git
cd SkyFire_548
git checkout -b bots f61bd333aa
git am --keep-cr /path/to/mod-aware-bots-548/patches/*.patch
```

`--keep-cr` matters: some core files have CRLF line endings upstream and the patches keep them. If `git am` stops,
run `git am --abort` and check that you are on the base commit. The patches carry SkyFire's license (GPL-3.0).

**Known at release:** upstream `main` at the base commit plus these patches builds with the module (compile-checked on
2026-11-23, see "State of this set" in `patches/README.md`). The weeks of live play were on a SkyFire tree with more
changes. If your build reports a missing core function, open an issue with the compiler output.

## 3. Add the module and build

SkyFire builds every folder under `modules/` that has a `src/` folder. The folder name must be exactly
`mod-aware-bots-548`: SkyFire derives the loader function (`Addmod_aware_bots_548Scripts`) from it.

```sh
# from the SkyFire_548 folder
git clone https://github.com/sardi-dev/mod-aware-bots.git modules/mod-aware-bots-548
# or link a checkout you keep elsewhere:
# ln -s /path/to/mod-aware-bots-548 modules/mod-aware-bots-548
```

The module stays out of the build unless `AWARE_BOTS` is on, so a plain SkyFire build is not affected:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX=/opt/skyfire \
  -DAWARE_BOTS=ON
cmake --build build --parallel 4
cmake --install build
```

The module is large. If the compiler runs out of memory, lower `--parallel`. Run the unit tests before you build
(`tests/run.sh` in the module folder); they need only `g++`.

`cmake --install` puts `playerbots.conf.dist` into SkyFire's configuration folder next to `worldserver.conf.dist`
(`<prefix>/etc` on Linux).

## 4. Databases

### The playerbots database

The module keeps its own data in a fourth database. Create it with a user that has rights on that database only
(names here are examples; any name works as long as `PlayerbotsDatabaseInfo` matches):

```sql
CREATE DATABASE `playerbots` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
GRANT ALL PRIVILEGES ON `playerbots`.* TO 'skyfire'@'localhost';
```

Load the base files (the per-table files first, then `playerbots_database_full.sql`), then the updates in name order:

```sh
cd /path/to/mod-aware-bots-548/sql/playerbots
for f in base/*.sql; do
  [ "$(basename "$f")" = playerbots_database_full.sql ] || mysql playerbots < "$f"
done
mysql playerbots < base/playerbots_database_full.sql
for f in updates/db_playerbots/*.sql; do mysql playerbots < "$f"; done
```

The module has no updater of its own for this database: the files in `updates/` are for fresh installs. Tables added
later by a feature (the arena ladder, the liking score) are created by the module itself at start-up.

### The module's SQL for the core databases

`data/sql/db-auth`, `db-characters` and `db-world` hold the module's command permissions, bot name lists, the arena
ladder NPC template and the auction price table. With patch 0013 the core's database updater applies them at start,
once per file, after the core updates:

- `Updates.Modules = 1` in `worldserver.conf` (the default once the patch is in).
- The updater looks for `<module>/data/sql` in the `modules` folder next to its SQL path. If your installed tree has
  no `modules` folder, set `Updates.ModulesSqlPath` to the source `modules` folder.

On a core without patch 0013, run those files by hand in name order on the matching database.

## 5. The living world profile

Copy the example configuration and set the database connection:

```sh
cp /opt/skyfire/etc/playerbots.conf.dist /opt/skyfire/etc/playerbots.conf
```

```ini
PlayerbotsDatabaseInfo = "127.0.0.1;3306;skyfire;<password>;playerbots"
```

Every setting has a comment in the file. SkyFire keeps the last value of a key, so you can also append your changes
at the end of the file and keep the rest as shipped.

`playerbots.conf.dist` keeps several defaults of the base module. For bots that start at level 1, earn their way and
travel instead of teleporting (the behaviour the rest of these documents describe, and how the maintainer runs it),
add this profile:

```ini
# Population: start small and grow (the shipped default is 500; about 300 is the largest population tested)
AiPlayerbot.MinRandomBots = 100
AiPlayerbot.MaxRandomBots = 100

# Random bots start at level 1, level by playing and are not re-rolled on a timer (about once a year at most)
AiPlayerbot.DisableRandomLevels = 1
AiPlayerbot.RandombotStartingLevel = 1
AiPlayerbot.MinRandomBotRandomizeTime = 31536000
AiPlayerbot.MaxRandomBotRandomizeTime = 63072000

# Travel instead of teleporting for their level; a random relocation at most every 7 to 14 days
AiPlayerbot.AutoTeleportForLevel = 0
AiPlayerbot.MinRandomBotTeleportInterval = 604800
AiPlayerbot.MaxRandomBotTeleportInterval = 1209600

# Earn gear like a player: need on usable upgrades, greed on the rest
AiPlayerbot.LootNeedRollLevel = 2
AiPlayerbot.LootGreedRollLevel = 1

# No free upkeep: no repair when summoned, a dead bot runs back to its corpse (the manager rescues it after 10 to 15 minutes)
AiPlayerbot.BotRepairWhenSummon = 0
AiPlayerbot.MinRandomBotReviveTime = 600
AiPlayerbot.MaxRandomBotReviveTime = 900

# Duel spots use bots already at the right level instead of re-levelling or re-gearing them
AiPlayerbot.DuelSpotSeedRelevel = 0
AiPlayerbot.DuelSpotSeedRegear = 0

# Nearby random bots group up for quests
AiPlayerbot.RandomBotGroupNearby = 1

# Let goblin, worgen and pandaren bots reset to level 1 near their own racial start
AiPlayerbot.RandomBotMaps = 0,1,530,571,648,654,860
```

Optional, also in the maintainer's setup: `AiPlayerbot.EnableTravelNodes = 1` (bots route across continents by boats,
zeppelins and flight paths; the graph is built once at the first start, which can take minutes to hours and blocks
that start).

Other settings worth a look:

- `AiPlayerbot.RandomBotAccountCount` (200) is how many bot accounts are created, each with up to 10 characters; its
  comment gives the sizing rule.
- `AiPlayerbot.RandomBotRandomPassword` stays 1 on any server that others can reach. With 0 every bot account's
  password is its account name.
- `AiPlayerbot.RandomBotMaxLevel` is 80 by default; the higher levels are less tested.
- `AiPlayerbot.SkillTiers.ExpertPercent` and `GoodPercent` set how skilled the population is.
- Alt bots: `AiPlayerbot.AllowBotAccounts` lets a player lend a second account's characters to their main account
  (`.playerbots account botmode on <main account>`). `AllowAccountBots`, `AllowGuildBots` and `AllowPlayerBots` open
  other sources of alt bots; set them to your server's taste.
- The economy (`AiPlayerbot.Auction.*`), social life (`AiPlayerbot.Social.*`) and AI layer (`AwareAi.*`) are off. Each
  has an `OnlyBots` list (the AI layer excepted) so you can try it on a few named bots first.

## 6. First start and checks

Start the world server as usual. At the first start the module creates the random bot accounts and characters, which
takes a while; bots then log in in batches (`AiPlayerbot.RandomBotsPerInterval`).

Checks:

- The world server log shows the module's lines (search for `playerbots`). A missing arena ladder NPC template is
  logged once as `[ARENANPC] creature template 9000100 is missing`: the world SQL from `data/sql/db-world` did not run.
- In game, game masters can use `.playerbots rndbot` and `.pmon`; players with the permission from `data/sql/db-auth`
  add their alt bots with `.playerbots bot add <character>`.
- Queue for a dungeon with the finder: with `AiPlayerbot.LfgFill.Enable = 1` bots fill the missing roles within a few
  minutes when fitting bots exist at your level.

## 7. Optional: the AI sidecar

The sidecar (`services/aware-ai/`) gives bots model-written chat lines in player-to-player moments and the battleground
leader a strategy adviser. Everything works without it: the module never contacts it while `AwareAi.Enable = 0`.

**Security first.** The sidecar has no authentication: anyone who can reach its port can change its settings and
spend your budget. Keep it on a private container network with the world server. Never publish port 8090 on a public,
LAN or VPN interface.

### Build and run with Docker

```sh
cd /path/to/mod-aware-bots-548/services/aware-ai
docker build --target runtime -t aware-ai:0.1.0 .

docker network create bots-ai          # or use your server's existing compose network
export OPENROUTER_API_KEY=...          # from your secret store; never write it into a file in the repository
docker run -d --name aware-ai --network bots-ai \
  -e OPENROUTER_API_KEY \
  -e AWARE_AI_CHAT_MODEL=<an OpenRouter model id> \
  -v aware-ai-data:/data -v aware-ai-logs:/logs \
  aware-ai:0.1.0
```

`-e OPENROUTER_API_KEY` without a value passes the key from your shell's environment, so it never appears in a command
line or a file. With Docker Compose, the same idea:

```yaml
services:
  aware-ai:
    build:
      context: ./modules/mod-aware-bots-548/services/aware-ai
      target: runtime
    environment:
      OPENROUTER_API_KEY: ${OPENROUTER_API_KEY:-}
      AWARE_AI_CHAT_MODEL: ${AWARE_AI_CHAT_MODEL:-}
    volumes:
      - aware-ai-data:/data
      - aware-ai-logs:/logs
    networks: [ai]
    # no "ports:" entry: the sidecar is reached only from the ai network
  worldserver:
    networks: [ai]   # plus the networks it already uses

networks:
  ai:
volumes:
  aware-ai-data:
  aware-ai-logs:
```

If the world server does not run in Docker, bind the port to the loopback interface only
(`-p 127.0.0.1:8090:8090`) and point the module at `http://127.0.0.1:8090`.

**No key yet?** Set `AWARE_AI_FAKE=1` instead of a key: the sidecar answers with deterministic decisions and canned
chat at no cost, which is enough to check the wiring.

### Switch it on in the module

```ini
AwareAi.Enable = 1
AwareAi.SidecarUrl = "http://aware-ai:8090"   # the service name on the shared network
AwareAi.Pvp.Enable = 1                        # battleground leader strategy
AwareAi.Chat.Enable = 1                       # player-to-player chat (needs AWARE_AI_CHAT_MODEL on the sidecar)
AwareAi.Budget.MonthlyUsd = 20                # one pool for chat and decisions
```

The effective monthly cap is the lower of `AwareAi.Budget.MonthlyUsd` and the sidecar's `AWARE_AI_MONTHLY_CEILING_USD`
(default 20). Set a spending limit on the OpenRouter key as well, as a backstop. Game masters see the link and budget
with `.bot ai status`; administrators use `.bot ai on`, `off` and `budget` (permissions from `data/sql/db-auth`).

What leaves your server when chat is on: the human's chat line, character names, class, race, level, zone and the
bot's voice, sent to OpenRouter and the model provider it routes to. Chat asks for zero-data-retention routing by
default (`AWARE_AI_CHAT_ZDR=1`). The sidecar strips email addresses, IP addresses and obvious sensitive fields, but it
cannot recognise every private detail typed in free text. Its logs keep prompt context for 90 days
(`AWARE_AI_LOG_RETENTION_DAYS`). All sidecar variables are listed in
[services/aware-ai/README.md](../services/aware-ai/README.md).

## 8. Updating and uninstalling

- **Updating the module**: pull, rerun `tests/run.sh`, rebuild. New files in `data/sql/` are applied by the core's
  updater at the next start. Never edit a module SQL file that was already applied: the updater treats it like an
  edited core update.
- **Updating SkyFire**: rebase the `bots` branch onto the new upstream `main` and resolve conflicts in the patched
  files, or wait for a new patch set in this repository.
- **Turning the module off**: `AiPlayerbot.Enabled = 0` without a rebuild, or build without `-DAWARE_BOTS=ON`.
- **Uninstalling**: `data/sql/uninstall/` removes the module's world data (the ladder NPC and the price table). Drop
  the `playerbots` database and delete the random bot accounts with your usual account tools.
