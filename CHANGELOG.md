# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Fixed
- **A finder bot that cannot get into its dungeon leaves the group after three asks** (2026-11-24)
  - Summary: `LfgTeleportRetryAction` asks for the dungeon teleport at most three times (it was six) at 15 s apart; the next time the trigger fires the bot says so in party chat when a player is in the group, and leaves the group like a finished run (`LfgTeleportPolicy`, four unit tests).
  - Why: A bot whose teleport never works (no valid entrance) asked for the teleport again and again, then sat outside the dungeon as a member that never arrived for the rest of the run.
- **A bot on a ledge above its leader searches further for a way down** (2026-11-24)
  - Summary: In `DungeonUnstuckAction::TryWalkOff`, when the bot has no path at all to its leader and the leader stands more than 10 yd lower, the landing search reads rings out to 30 yd (it was 12), allows a drop up to 30 yd (it was 20), and a landing within 4 yd of the leader's height counts as cheaper than any other (`LedgeWalkOffPolicy::WantsLeaderFloor`, `OnLeaderFloor`, four unit tests). The unseen-only teleport rule is unchanged: the bot still walks off the ledge in view.
  - Why: Two bots stood on a wall walk 40 to 63 yd from their leader, 16 yd above it, for more than five minutes: the 12 yd search found no landing with a path to the leader, and the teleport stays held while a player can see the bot.
- **A bot-led run that cannot reach any boss ends early and its dungeon rests** (2026-11-24)
  - Summary: When the leader of a bot-only run (finder run or hand-formed party) has skipped every boss still to do for lack of progress, and no pack was pulled or fought since the first of those skips, the crawl gives the run up: the bots of a finder run are asked to wrap up as at the time limit, and a hand-formed party is called over like a finished one. The dungeon is then left out of bot-only runs and of bot party recruiting for `AiPlayerbot.LfgBotRuns.FailureCooldownHours` (default 4, 0 for no rest; kept in memory). The rule is `LfgStallPolicy` (six unit tests). The built-in default of `AiPlayerbot.LfgBotRuns.Dungeons` now matches `conf/playerbots.conf.dist` (no 14, no 272).
  - Why: Five of six bot-only runs in a day sat out their 40 minutes without reaching a boss: the tank skipped every boss for no progress, waited for the skips to expire and did it again (eight cycles in Gnomeregan, seven skips of the last boss in the Wicked Grotto), tying up five bots for nothing.
- **Bot-only runs skip Gnomeregan and Maraudon's Wicked Grotto for now** (2026-11-24)
  - Summary: `AiPlayerbot.LfgBotRuns.Dungeons` no longer lists 14 (Gnomeregan) and 272 (Maraudon, Wicked Grotto) by default.
  - Why: Five of six bot-only runs in a day sat out their 40 minutes there without reaching a boss (the tank skipped every
    boss for "no progress"); the cause is still open, and a run that cannot reach a boss only ties up five bots.
- **The finder fills a tank no more than a level under the player** (2026-11-24)
  - Summary: `LfgFill` (`DungeonsFor`) now requires a tank it queues, by native spec, party bot or tank switch, to be at least the requesting player's level minus `AiPlayerbot.LfgFill.TankLevelBelowPlayer` (default 1) and never under the dungeon's minimum level (`LfgFillPolicy::TankLevelFloor`, `TankLevelOk`, three unit tests). Healers and damage dealers keep `LevelWindow`. The rule applies while the player is inside the dungeon's level range, like the window; the highest level of a premade group counts as the player's level.
  - Why: In the Stockade the finder filled a level 21 tank for a level 24 player with mobs at 23 to 24, and one pull chained 14 mobs and wiped (night proof run 2026-11-23).
- **The random password setting is read once** (2026-11-24)
  - Summary: `PlayerbotAIConfig.cpp` read `AiPlayerbot.RandomBotRandomPassword` twice in a row; the second read is gone.
  - Why: Leftover of the release cleanup; harmless, but it reads as a mistake.
- **Bots dive to a target deep in the water** (2026-11-23)
  - Summary: The surface swim lifted every point of a water path, the destination too, so a bot chasing a mob on a lake bed
    swam above it and could not hit it. The last point now keeps the destination's depth when it lies more than 1.5 yd
    under the surface; a destination near the surface (a swimming leader) is lifted as before.
  - Why: A melee bot hung on the surface over a Freshwater Eel it could not reach (Lake Everstill).

## [0.1.0] - 2026-11-23

### Added
- **First public release** (2026-11-23)
  - Summary: The module `mod-aware-bots-548` (player bots for SkyFire 5.4.8, client build 18414), the core patches it
    needs in `patches/`, and the optional AI sidecar `services/aware-ai`.
  - Why: The module ran privately for several weeks; this is its first public snapshot. The development history before
    this point is not published, so the entries of earlier private builds are not listed here.
