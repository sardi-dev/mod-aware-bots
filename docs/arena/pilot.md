# Arena ladder: the pilot (BT-154)

Named teams of two random bots, a team rating of their own, a light schedule of bot-versus-bot matches, a ladder NPC, bot
chat about results and a JSON snapshot for a status page. Decision record: design decision 0027. Pilot scope: 2v2 only; human queues
(P2) and 3v3, 5v5 and deeper tactics (P3) come later.

## Why the module keeps its own books

The 5.4.8 core keeps no arena rating: a rated arena queue is accepted, but the join handler hard-codes rating 1 and no arena
team object is ever created, so the core's end-of-match rating code never runs. The ladder here is therefore the only rating
there is, and the module chooses which two teams meet (the core pairs two rated groups first come, first served).

## What it does

| Part | File | What |
|------|------|------|
| Rating, clock, schedule, pairing, band, team check | `src/Arena/ArenaMath.h` | the core's own formula (expected score `1 / (1 + 10^((opp - own) / 650))`, change `K * (score - expected)`, K 32 for the first ten games, then 24), tier seeds (expert 1750, good 1500, casual 1300, jitter of 40 from the team id), streaks, the draw rule, the owner's clock (US Eastern with daylight saving, no tz database), the activity weight, the choice of two teams |
| Compositions | `src/Arena/ArenaComp.h` | a profile per specialisation (role, crowd control categories with learn levels from the class sheets, interrupt, self defensive, external defensive, burst), the pair score and the team former |
| Names | `src/Arena/ArenaNames.h` | original word lists, four patterns, a block list, unique names with a Roman numeral fallback |
| Text | `src/Arena/ArenaText.h` | the lines of the ladder pages and the console |
| Chat rules | `src/Arena/ArenaChatMath.h` | which result is worth a line, and the rate limits |
| Play scoring | `src/Arena/ArenaPlanMath.h` | tier profiles, focus score, defensive score, line of sight candidates |
| Books | `src/Arena/ArenaLadder.cpp`, `ArenaState.h` | formation, checks, scheduler, queueing, match poll, booking, console |
| Database | `src/Arena/ArenaStore.cpp` | `CREATE TABLE IF NOT EXISTS` at start-up, load, asynchronous write-through |
| Snapshot | `src/Arena/ArenaSnapshot.cpp` | `arena-ladder.json` in the world server's `LogsDir` |
| Chat | `src/Arena/ArenaChat.cpp` | General channel lines through the bot voice |
| NPC | `src/Arena/ArenaNpc.cpp` | the gossip script `npc_bot_arena_ladder` |
| NPC spawns | `src/Arena/ArenaNpcSpots.h`, `ArenaNpcSpawn.cpp` | the four spots, the spawn decision (pure) and the run-time spawn pass |
| Play | `src/Arena/ArenaPlan.cpp`, `ArenaPlanActions.*` | the shared focus, defensives and cover, as triggers and actions of the `arena` strategy |

## The ladder NPC (Pit Chronicler)

The NPC belongs to the module (owner, 2026-11-12): without the module compiled in it appears nowhere. The core cannot load a
creature template at run time (its template store is private to the object manager), so the split is this:

- **Install data** (world database, applied by hand): `data/sql/db-world/2026_11_12_00_arena_ladder_npc.sql` holds the creature template
  9000100 (`ScriptName npc_bot_arena_ladder`), its gossip menu and its three texts, and no creature rows.
  `data/sql/uninstall/world_arena_ladder_npc_remove.sql` deletes all of it and any creature rows with that entry or the guids
  9300000 to 9300099. Without the module the template row alone is invisible (the core logs one missing ScriptName line for it
  at start-up, which is harmless).
- **Spawns** (module, run time, never in the creature table): `ArenaNpcSpots.h` lists the four spots (Shrine of Seven Stars,
  Stormwind, Orgrimmar, Shrine of Two Moons). The spawn pass runs every 2 s in the world tick (`ArenaNpcSpawn::OnUpdate`). For
  each spot it asks the base map of the continent whether the grid of the spot is loaded and not in the removal state; if the
  creature it remembers for the spot is gone it looks for a creature of the entry within 8 yards (a leftover database spawn is
  adopted, never doubled) and otherwise creates the NPC with `Map::SummonCreature` (no owner, no duration, so a normal grid
  creature without a spawn id: it is never saved, and the core deletes it with its grid). The next load of the grid spawns it
  again.
- **Switches**: `AiPlayerbot.ArenaTeams.Enable = 1` and `AiPlayerbot.ArenaTeams.LadderNpc = 1`. At start-up the module checks
  that the template exists and has a ScriptName; if not it logs `[ARENANPC] creature template 9000100 is missing` once and
  spawns nothing.
- **Why it is safe**: the pass runs on the world thread after `sMapMgr->Update` has waited for the map threads (the same point
  where the ladder moves bots), so nothing else touches a map while it looks at a grid and adds a creature. No core hook is
  used: the map script hooks fire for terrain tiles on a map thread, not for loaded objects.
- Log lines start with `[ARENANPC]` (the first spawn of each spot per run at info level, later ones at debug).
- To add a capital: one row in `ArenaNpcSpots::SPOTS` (continents only).

## Threads

The ladder tick runs in the world update script, at the point of `World::Update` where the map threads have finished
(`sMapMgr->Update` waits for them), so it may queue bots and end an arena directly. Map threads only read: the NPC, the
skill tier hook and the play code copy out of the RAM books under one lock. The database is touched only at start-up
(creating and loading) and through the asynchronous queue (one worker thread, so writes keep their order).

## Flow of a match

1. A scheduler pass runs every 60 s (plus or minus 10). It needs weight above zero on the owner's clock, a successful roll
   (`weight * PeakMatchesPerHour / 60`), a world update time within `MaxUpdateMs`, no match running (the cap is 1), and no
   human waiting in that bracket's rated 2v2 census.
2. It lists the teams whose members are all online and free (no group, no fight, no dungeon, no queue), picks the team that
   has waited longest and its closest opponent (`PickPair`), builds a party of each, and queues both in the same tick.
3. The bots accept the invitation with the usual battleground status action. The ladder sees the arena, remembers which
   side each team fights on, and cuts a bot-only match as a draw after 12 minutes (`EndBattleground(WINNER_NONE)`).
4. When the core has set the winner the match is booked once: both ratings, records and streaks change, the rows are queued
   for the database, the winner (or the team that ended a streak) may tell the General channel, and the snapshot is marked
   for writing. A member that left early, a human in the arena, or an arena that vanished voids the match (no rating change).
5. The teams are free again when the bots have left the arena (their party dissolves), at the latest after 5 minutes.

## Teams

Formed from online random bots of level 60 and up (`MinLevel`), per faction and 5-level band, one new team per run, the
best pair first of the tier that is furthest below its target mix. A team plays at its weakest member's tier (a narrow
change in `BotSkill.cpp`: inside a ladder match `TierOf` and `ExtraReactionMs` use the team's tier). Teams are checked at
every run: both members must be in the same band (the team moves up with them), a deleted character is replaced once a
week at most, and a team that stays out of step is retired after `RetireAfterChecks` checks. Retired names stay reserved.

## Play v1

Per team, once per tier interval (500, 1000 or 2000 ms), the first bot that ticks picks the focus: a deterministic score of
hp deficit, role, reach, sight, stickiness, immunity and crowd control (`FocusScore`), kept until another target leads by the
swap margin and the hold time has passed. `EnemyPlayerValue` returns that focus to both damage dealers. Defensives follow
`DefensiveScore` (tier thresholds 25, 40 and 55 hp percent, pressure, no healer in reach, a recent use counts against), with
a predicted-lethal rule for experts. Good and expert bots below 40 percent hp that enemy casters target look for cover on 24
ring points (path at most 1.6 times the straight line, out of the casters' sight, in sight of the partner). The top
candidates are logged at debug level as `[ARENAPLAN]`, so that a later selector can choose among the ones near the best.

Not in v1 (P2 and P3): crowd control chains with diminishing returns, peel, the kill window, hazards of the maps, map rules.

## Operating it

- `.playerbots ladder [show|matches|form|match|snapshot]` from the console: the teams, the newest matches, a formation run,
  a scheduler pass that skips the clock, the roll and the world load, and a snapshot write.
- `AiPlayerbot.ArenaTeams.ClockOffsetMinutes` moves the owner's clock (480 turns 19:00 into 03:00); `StartupDelaySeconds` and
  `FormationIntervalSeconds` can be short for a test. All settings are in `conf/playerbots.conf.dist`.
- Log lines start with `[ARENALADDER]` (teams, matches, results) and `[ARENAPLAN]` (debug).
- `AiPlayerbot.ArenaTeams.Enable = 0` turns the feature off completely.

## Known limits

- A pair of the same faction goes through the core's queue pairing, which has not been seen with bots yet; verify live.
- A team whose composition stops scoring (a re-randomised spec) rests; it is not given a new partner.
- The ladder NPC shows the 2v2 ladder only. It appears up to 2 s after the grid of its spot loads and is gone while that grid is unloaded; a spot nobody (human or bot) is near has no NPC.
- The map hazards (Dalaran Sewers waterfall, Ring of Valor pillars) are not part of the cover score yet.
