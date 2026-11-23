# Twin Peaks playbook

Status: **owner approved 2026-11-18 (answers in section 15); gaps G1 to G5 implemented on `feat/bg-twin-peaks-plan` (BT-470 to BT-474, unit tested and syntax checked, not yet tested live); G6 to G10 open; the middle deadlock rule (G11, BT-694) added 2026-11-19 on `fix/tp-mid-deadlock` (unit tested and syntax checked, not yet tested live).** Written 2026-11-18. Written against the module `main` at
bafcb06 (the Warsong Gulch plan, carrier rule and tier gating are in it) and the core at 3cd5065fb4. Twin Peaks is the third map in the
owner's order (owner direction 2026-11-18, design decision 0032). This sheet reuses the Warsong Gulch plan wherever the map allows
(`docs/bg-sheets/warsong-gulch.md`, owner approved 2026-11-12) and spells out only what differs: the lanes, the respawn geometry,
the longer clock, the base layout and the new owner requirements (leader, tier targets, decision points).

Legend: **verified** = read in the core, the world data or our code (file and line given); **inference** = derived from
verified facts; **to check live** = needs a match with level 85 bots or a human; **owner question** = the sources disagree with
the core and the owner decides (section 15). Rules are marked core, good or expert like the class sheets: casual bots follow
core rules only and react late.

## 1. Header

| Item | Value | Source |
|------|-------|--------|
| Map id, battleground type id | 726, 108 (`BATTLEGROUND_TP`) | `battleground_template` row 108, `SharedDefines.h:4032` |
| Team size | 10 v 10; template minimum 5 per team | `battleground_template` MinPlayersPerTeam 5, MaxPlayersPerTeam 10 |
| Brackets (5.4.8) | **85-89 (index 0) and 90 (index 1) only**; template MinLvl 85, MaxLvl 90 | `PvpDifficulty.dbc` rows for map 726, `battleground_template` |
| Owner's band | not applicable: Twin Peaks opens at 85. On 2026-11-18 the live server has no random bot at 85 or above (305 below 60, 25 at 60-79), so this map is reached last on the level path; the sheet is written ahead | read-only count of `characters.characters` for the bot accounts |
| Win | first team to **3 flag captures** | `BG_TP_MAX_TEAM_SCORE` 3, `BattlegroundTP.h:13` |
| Time limit | the match ends when the instance clock reaches **30 minutes**; the clock also runs in the 2 minute preparation, so play lasts **28 minutes** after the gates open. The core's own timer display starts at 25 and jumps to 27 at the 3 minute mark, so the limit and the display disagree (owner question 2) | `BattlegroundTP.cpp:63`, `:202`, `:85`, `:916`; `Battleground.cpp:247` |
| Timeout result | more captures wins; equal and above 0: the team that captured last; 0-0: draw | `BattlegroundTP.cpp:61-80` |
| Carrier debuffs | the flag itself: **+20 percent damage taken and run speed capped at 14 yd/s**. While both flags are carried: this core casts Focused Assault (+10 percent taken) once after 10 minutes and Brutal Assault (+10 percent and a 7 yd/s cap) once after 15, no stacking (owner question 3) | `BattlegroundTP.cpp:143-167`; spell rows; Wowhead spells 23333, 46392, 46393 |
| Early end | a team below 5 players for the premature-finish timer (core default 5 minutes, live config not read) ends the match | `Battleground.cpp:230`, `World.cpp:1076` |
| Respawn | the dead respawn in 30 s waves at the team's **middle graveyard** (Alliance 1749, Horde 1750), 300 to 345 yd from the own flag stand (owner question 1) | `BattlegroundTP.cpp:864-885`, `Battleground.h:111` |
| Sources | core `BattlegroundTP.{h,cpp}` in the core; world DB and client DBC read-only; our tactics; the 5.4.8 reference core `BattlegroundTP` (same graveyard rule, 27 minute clock, stacking debuffs); the 4.3.4 Cataclysm reference core (distance rule for the graveyard); the game wiki Twin Peaks (read 2026-11-18); Wowhead MoP Classic spell pages for the flag auras (read 2026-11-18); the Skill-Capped Twin Peaks rated-battleground guide (7:48, paid; own words, no quotes; it describes the Legion era). Notes: `.agents/plans/bots-bg-sheets/sources/twin-peaks.*.md` | |
| Wowhead | Based on Wowhead data (MoP Classic spell pages 23333 Horde Flag: damage taken +20 percent, run speed limit with value 14; 46392 Focused Assault and 46393 Brutal Assault: +10 percent damage taken, stacking not stated). The Wowhead zone pages and guides carried no map rules (empty shells, 404, or a 9.2.5 guide whose body was not served): the list is in the sources note | |
| Status | owner approved 2026-11-18 | |

Where the guide or another source differs from the core, **the sheet is written against the core as it is today and the plan
reads the live state instead of assuming** (a debuff stack count, the bot's own respawn position, a clock constant per
map), so one answer to owner questions 1 to 3 changes a constant, not the plan.

### Where sources and the core disagree (all decided by the owner, section 15)

| # | Item | This core (`home`) | 5.4.8 reference core | Other source | Question |
|---|------|--------------------|----------------------|--------------|----------|
| 1 | Where the dead respawn in play | always the team's middle graveyard (1749 / 1750) | the same code | 4.3.4 reference core and the Legion guide: the own start graveyard (1729 / 1728, 132 to 149 yd from the stand) when the player is nearer the enemy flag room than the enemy middle graveyard, else the middle one; the wiki says each faction has two graveyards | 1 |
| 2 | Clock | 30 minutes elapsed (28 of play), display 25 then 27 | 27 minutes elapsed (25 of play), display 25 counting down | the 4.3.4 core closes the match at 27 minutes; the wiki gives no number | 2 |
| 3 | Debuff while both flags are carried | Focused Assault once at 10 minutes, Brutal Assault once at 15, no stack | first stack at 3 minutes, one more a minute, Brutal Assault from the fifth stack, up to 10 stacks (the spells allow 10) | wiki: starts at the 1 minute mark since the hotfix of 2014-03-04 (down from 3 minutes); the guide: stacking | 3 |
| 4 | Team shape | no rule | no rule | the guide plays a tank carrier body, three healers and a floater (a premade shape) | 4 |
| 5 | World DB spawns on map 726 | the script spawns its own flags, buffs and spirit guides | not read | the DB also holds flag stands, ground flags, buffs and guides (section 2) | 5 |

## 2. Map and positions

All coordinates are map 726. Alliance holds the high plateau (stand z 44), Horde the low base (stand z 2.4). This sheet names the two
valley lanes by the middle graveyard they hold: **lane A** (the Alliance graveyard 1749, y about 380-450) and **lane H** (the Horde
graveyard 1750, y about 160-240); the module's path names call them NorthMid and SouthMid. Names are ours; the client's labels differ.

| Thing | Alliance | Horde | Source |
|-------|----------|-------|--------|
| Flag stand (GO 179830 / 179831) | 2118.21, 191.62, 44.05 | 1578.38, 344.04, 2.42 | `SetupBattleground` (`BattlegroundTP.cpp:708-709`) |
| Capture trigger (radius 5) | 5904 at 2119.05, 190.87, 44.06 | 5905 at 1578.09, 344.07, 2.42 | `AreaTrigger.dbc`, `BattlegroundTP.cpp:673-682` |
| Start and preparation graveyard | 1726 at 2142.60, 175.92, 43.65 | 1727 at 1549.54, 346.95, 1.29 | `WorldSafeLocs.dbc` |
| Middle graveyard (respawn in play) | 1749 at 1877.15, 439.11, -4.00 | 1750 at 1816.87, 160.08, 1.81 | `WorldSafeLocs.dbc`, `GetClosestGraveYard` |
| Start graveyard (spirit guide spawned, never a respawn point here) | 1729 at 2168.10, 332.04, 34.70 | 1728 at 1561.16, 212.76, 13.86 | `WorldSafeLocs.dbc`, `BattlegroundTP.cpp:742-761` |
| Speed buff (GO 179871) | 2171.28, 222.33, 43.80 | 1545.40, 304.03, 0.59 | `BattlegroundTP.cpp:711-712` |
| Restoration (GO 179904) | 1952.12, 383.86, -10.29 (lane A) | 1753.96, 242.09, -14.12 (lane H) | `:713-714` |
| Berserk (GO 179905) | 1934.37, 226.06, -17.04 (lane H) | 1725.24, 446.43, -7.83 (lane A) | `:715-716` |
| Doors (closed in the preparation) | 2115.40, 150.18, 43.53; 2156.80, 220.33, 43.48; 2127.51, 223.71, 43.64; and an upper one 2096.10, 166.92, 54.23 | 1556.60, 314.50, 1.22; 1587.09, 319.85, 1.52; 1558.32, 372.71, 1.48; and an upper one 1591.46, 365.73, 13.49 | `:718-726` |
| Entrance posts inside each flag room (module) | (2116.27, 158.93), (2151.20, 220.00), (2122.93, 222.40) | (1560.80, 316.80), (1586.67, 322.13), (1569.07, 368.27) | `BattleGroundTactics.cpp` `TP_GUARD_POSTS_*` |

Distances (computed from the coordinates; "route" is the module's authored navmesh paths `vPath_TP_*`, 2D polyline length):

| Distance | Alliance side | Horde side |
|----------|---------------|------------|
| Stand to stand, straight line | 561 yd | |
| Route, Alliance stand to Horde stand (through lane H) | 743 yd | |
| Route, Horde stand to Alliance stand (through lane A) | | 814 yd |
| Own middle graveyard to own stand | 345 yd | 301 yd |
| Own middle graveyard to enemy stand | 314 yd | 303 yd |
| Own start graveyard to own stand | 149 yd | 132 yd |
| Middle graveyard to middle graveyard | 285 yd | |
| Own speed buff to own stand | 61 yd | 52 yd |

Inferences from the route data (to check live on the navmesh):
- **Each carrier's shortest way home runs through the lane of its own middle graveyard**: the Alliance carrier's route (Horde
  stand to Alliance stand) runs through lane A, passes its restoration and berserk buffs within 3 yd and the Alliance graveyard within 36 yd; the Horde
  carrier's route runs through lane H, passes its berserk (4 yd) and restoration (26 yd) buffs, the Horde speed buff (16 yd) and the Horde graveyard
  within 17 yd. The two shortest routes make a loop: the Alliance goes out through lane H and home through lane A, the Horde the other way round,
  so the two strike groups do not meet head on in the opening, and each carrier meets the other team's outbound group.
- **Kill window**: along a carrier's run home (route polylines measured against its own middle graveyard), the first 305 to 315 yd (about 45 s at 7 yd/s) lie more than 100 yd
  from its own respawn point (the first 225 to 230 yd, 32 s, more than 150 yd); the next 190 yd (about 27 s) lie within 100 yd of it, where every 30 s wave of its own team joins;
  the last 245 to 315 yd run to the base. A hunter team has to win its fight in the first part of the run or cut at the base entrances.
- **Respawn cost**: a dead Alliance defender is 345 yd from the flag room and a Horde defender 301 yd: about 45 to 50 s on foot,
  half of that mounted (mounts in a battleground: to check live). A dead attacker is about the same distance from the enemy stand
  (314 and 303 yd), so under this core dying is cheap for the attack and expensive for the defense.
- A trip pickup to capture, unbuffed, is about 55 s of approach (mounted) plus 105 to 115 s of carry (743 to 814 yd at 7 yd/s, less with the
  speed buff): **about 3 minutes per capture cycle**, the same as Warsong Gulch.

Water and ledges: the river centre (module point 1840.0, 320.0, -24.945) is the lowest point; both middle graveyards stand on
islands of the navmesh with no walkable link to the rest of the map (7 and 42 polygons in `mmaps/0726_28_31.mmtile`, module
issue 747), so a respawned bot first jumps down to the connected mesh (`BGTactics::tpJumpDown`, `BattleGroundTactics.cpp:2047`).
The time that costs per respawn is **to check live**.

World DB rows on map 726 (read-only query 2026-11-18; the script spawns its own copies): flag stands 179830 at (2117.64, 191.68) and 179831 at
(1578.34, 344.05), a **Horde ground flag 179786 at (2107.04, 210.61, 43.66) inside the Alliance base**, speed buffs 179871 at (2175.87, 226.62) and 179899 at
(1544.55, 303.85), the restoration objects 179904 (five stacked at 1951.18, 383.80) and 179906 (two at 1754.16, 242.13), berserk objects 179905 (two at 1932.83, 226.79)
and 179907 (one at **1737.57, 435.85**, 16 yd from the script's second berserk at 1725.24, 446.43; area trigger 5910 sits at
1737.69, 435.86), spirit guides 13116 at (1879.16, 441.91) and (2178.08, 332.76) and 13117 at (1550.53, 211.55). Whether a match
shows two objects at each place, which one a click reaches, and whether the stray ground flag can be used while a flag is on the
ground, is **to check live** (owner question 5; the core accepts a pick-up from the ground only for a flag in the ground state,
`BattlegroundTP.cpp:537-538`, `:573-574`, within 10 yd of the clicked object).

Our static data today: waiting spots, flag, hide, roam and camping points `TP_*` (`BattleGroundTactics.cpp:97-134`), eight authored
paths `vPaths_TP` (`:1238-1370`) and the two carrier path sets `vPaths_TP_CarrierAlliance/Horde` (`:1381-1391`), the island jump (`:2047`).

## 3. Mechanics that matter for play (verified in `BattlegroundTP.cpp` unless stated)

The flag rules are the Warsong Gulch rules (see `warsong-gulch.md` section 3): same flag spells 23333 and 23335, same click rules, same
dropped flag object ids (179785, 179786), same 10 s drop return, same 3 s "Recently Dropped Flag" (42792), same 23 s flag reset, same capture
condition (your own flag must be on its stand). Differences and Twin Peaks specifics:

- **Capture needs your own flag on its stand**: triggers 5904 and 5905 test `_flagState` of both flags and the picker (`:673-682`). Taking the enemy flag blocks their capture (as in Warsong Gulch).
- **Clock**: ends at 30 minutes of instance time (`:63`), 28 of play. The `_minutesElapsed` display counts from the 3 minute mark (`:82-86`).
- **Debuffs while both flags are carried** (`:143-167`): once at 10 and 15 minutes, no stacks, and the clock restarts whenever a flag returns,
  respawns or is captured (`:111`, `:138`, `:260`, `:293`, `:549`, `:585`). The two spells carry a stack limit of 10 in the client data
  (`SpellAuraOptions.dbc`), so the stacking of the 5.4.8 reference core works with this data. A picker of a ground flag gets the current debuff (`:561-564`, `:597-600`).
- **Dead players**: wave of 30 s (`Battleground.h:111`) at the middle graveyard of the own team during play, the flag-room graveyard before the gates (`:864-885`).
  The start graveyards 1728 and 1729 have spirit guides (`:742-761`) but no respawn rule points at them.
- **Doors**: eight door objects, opened together at the end of the preparation (`:206-216`); three of them stand on the ground and one is higher per base, so a base has three ground entrances.
- **Buffs**: speed, restoration and berserk objects respawn after 180 s (`Battleground.h:119`); the flag carrier does not lose the flag by taking one (to check live).
- **Late joiners and leavers**: a leaving carrier drops the flag (`:617-642`); the dropper gets 42792.
- The area triggers 5908-5911 (at the four valley buffs) and 5914-5921 (start rooms) do nothing in the script (`:683-693`).

**Clocks to plan with**: dropped flag 10 s; recently dropped 3 s; flag reset 23 s; carry 105 to 115 s; capture cycle about 3 minutes; buffs 180 s; respawn wave 30 s;
debuffs 10 and 15 minutes of both flags held in this core; match 28 minutes of play in this core.

## 4. Roles by side size

Roles are **assigned**, not rolled, from one team record, exactly as in Warsong Gulch (`WsgPlan`, `Utils/WsgPlanLogic.h`: role table, score,
fill order, no re-roll, reassignment at deaths). The role table is the Warsong Gulch one; what changes is where each role stands.
Roles for a 10-player team (scale by rounding for 5 to 9; a human takes a slot and is never commanded):

| Role | Balanced | Offense | Defense | Job on Twin Peaks |
|------|----------|---------|---------|-------------------|
| TAKER (T) | 1 | 1 | 1 | Takes the enemy flag and runs it home by the **own lane** (the shortest route, section 2); stealth or mobile class first, highest level |
| BACKUP (B) | 1 | 1 | 0 | Second pickup; takes the flag when T dies at the enemy stand |
| ESCORT (E) | 2 | 4 | 2 | Moves with T inside 12 yd, peels attackers; one is a healer; takes the flag when T is down |
| MID (M) | 3 | 3 | 2 | Holds the two **lane posts** and kills the enemy carrier (EFC) in the kill window (see below); takes buffs on the way |
| HOME (D) | 3 | 1 | 5 | One bot per flag-room entrance post (three posts per base); returns dropped flags; chases the EFC only inside 40 yd of the own stand |

Why HOME stays at 3 and the plan weights stay 20 / 60 / 20: the three posts are the three ground entrances, and a dead defender needs 45 to 50 s to be
back (section 2), so a base with two bots at the posts cannot be refilled in time. The offense plan (1 HOME) is the risky draw, as in Warsong Gulch.

**Lane posts** (MID's places, from the buff data in section 2; the buffs are the only fixed objects on the lanes):

| Post | Alliance MID | Horde MID | Use |
|------|--------------|-----------|-----|
| CUT (on the enemy carrier's route, near our base) | lane H berserk (1934.37, 226.06) | lane A berserk (1725.24, 446.43; the DB row and trigger 5910 say 1737.6, 435.9: owner question 5) | the enemy carrier passes it about 190 yd (Alliance) or 180 yd (Horde) from our stand, inside the kill window |
| COVER (on our own carrier's route) | lane A restoration (1952.12, 383.86) | lane H restoration (1753.96, 242.09) | heals and cover for our carrier; 250 yd (Alliance) or 200 yd (Horde) from our stand |

With 3 MID bots: 2 at CUT, 1 at COVER; with 2: 1 and 1. After the enemy carrier passes CUT alive, the CUT bots chase (nearest 2 hunt, the rest cut at the base
entrances); after the carrier passes its own middle graveyard the hunters stop and fall back to the entrance posts (section 6, state C).

**Assignment**: unchanged (class and spec fit, level rank, tier, tie by GUID hash, candidate lists per role so a decision layer can
pick among the top ones). One change: the **leader** (section 8) is chosen separately and may hold any role; it never takes TAKER unless it is the best fit, so its
calls are not interrupted by carrying duty.

## 5. Opening plan (gates open 2:00 after the match starts; first 60 to 90 s)

During the preparation bots wait at the start spots and mount in the last 30 s (existing: each bot mounts at its own moment in the last 30 s, `CheckMountStateAction.cpp:261-270`; whether a level 85 bot owns a mount is to check live). Same for both factions; coordinates mirror.

| Time after gates | TAKER, BACKUP, ESCORT (strike group of 4) | MID (3) | HOME (3) |
|------------------|--------------------------------------------|---------|----------|
| 0 to 15 s | leave together by the outbound lane (Alliance lane H, Horde lane A); stealth classes go 10 s ahead | CUT bots ride to their lane post; melee bots take the berserk there (healers skip it, as today), a hurt bot takes the restoration; one bot goes to COVER | one bot to each entrance post |
| 15 to 60 s | arrive at the enemy base together; **dismount at 60 yd** (the carrier cannot ride); T and B wait 5 yd behind the group | hold the posts; **expert**: the two fastest ride out to meet the enemy strike group in the valley and open on their taker (slow, stun) so it dismounts and spends a defensive before the pickup | stay |
| 60 to 90 s | enter by the entrance nearest the lane, kill the guards at the stand, **T takes the flag**; the enemy speed buff next to the door is taken by T if within 20 yd of the way out | kill the first enemies that come through; when the enemy carrier appears, state C | return any dropped flag at once |

First pickups arrive 75 to 120 s after the gates open when the group rides (743 to 814 yd mounted at 14 yd/s is 53 to 58 s, plus the dismount and the fight at the stand); a group that walks needs about 2 minutes for the route alone. The strike group fights at the stand only
until T has the flag, then leaves (the flag, not the kills, is the objective). **Casual** bots may chase kills at the stand or ride in alone; that is part of being beatable.

## 6. Mid-game rules

State of the flags decides the plan, read from `BattlegroundTP::GetFlagState` (on base, wait respawn, on player, on ground) and `GetFlagPickerGUID` per team, plus score, elapsed time and the live aura stacks
of both carriers. States A to F are the Warsong Gulch states; what each role does on Twin Peaks:

| State | Event | TAKER, BACKUP, ESCORT | MID | HOME |
|-------|-------|-----------------------|-----|------|
| A both flags home | opening, after a return or the 23 s reset | strike group walks to the enemy base by the outbound lane | hold CUT and COVER | hold the three posts |
| B we carry, ours home | our pickup | T runs home at once by the own lane (carrier rule below); E within 12 yd, the two nearest healers follow at 15 yd and heal it first; B stays as next carrier | COVER bots meet T on the lane and stay with it from the own graveyard to the base; CUT bots return to CUT | hold; kill an enemy taker if one arrives |
| C they carry ours, theirs home | enemy pickup | B and one E take **their** flag at once if it is free (blocks their capture); T and one E hunt | **kill window**: the 2 nearest to the EFC attack and crowd control it; CUT bots intercept; **past the EFC's own middle graveyard** the hunters stop and fall back to the posts | the 2 nearest engage inside 40 yd of the stand, others hold |
| D both carried | second pickup | carrier hides at a hide post and E guards it; do not stand at a flag stand | hunt the EFC with focus fire and crowd control, 6 of the team | 2 stay home |
| E flag on ground | a drop | nearest 2 within 20 yd of it: ours returns it (click), theirs gets picked up by T or B after the dropper's 3 s | one to the drop, one covers | the nearest returns ours at once |
| F post capture, 23 s | a capture | regroup, heal, take the nearest buff, walk toward the enemy base to arrive as the flags respawn | hold | hold |
| G **our attack is wiped** (new) | 3 or more of the strike group and MID died within 15 s and respawned | **respawn at lane graveyard**: check the base first: if the enemy has a carrier or 2 or more enemies are within 80 yd of our stand, run home (HOME role until relieved), else regroup at the own lane post and push again as one group (never one by one) | as left | as before |
| H **their attack is wiped** (new, good) | 3 or more enemy players died within 15 s near our CUT post or at the enemy stand (read from the roster) | the floater (a MID healer or the nearest free bot) and one HOME bot join the strike group to use the time | hold | one HOME bot leaves for the group, two stay |

The carrier does not hold the enemy stand to fight: the **Warsong Gulch carrier rule applies unchanged** (owner 2026-11-17): run at once, a scored option list
(run 100, run near the capture 950, fight rooted 960, fight slowed 800, fight alone 600), helpers (freedom spell, class defensive, speed spell, never an immunity that drops the flag),
and the two nearest healers follow it at 15 yd. The rule's code (`WsgCarrier::Fights` and `UseHelpers`) only runs on map 489 today; it has to run on 726 too (gap G3).
**Carrier lane choice** (decision D4, section 16): the shortest route (the own lane) wins unless the enemy has 3 or more fighters within 60 yd of the first 100 yd of it at the decision (then the other lane, only for the expert tier);
the own lane carries the benefit that the 30 s respawn waves of the own team appear on it.

**Score and time** (use `left` = clock limit minus `elapsed`, limit 30 min in this core, 27 min if owner question 2 is answered by the fix; thresholds are the Warsong Gulch ones because a capture cycle is about 3 minutes on both maps):
- Ahead 2-0: **defense plan**, at most **3** MID bots hold the CUT post of the enemy's carrier lane and nobody camps the enemy graveyard (the module's `TP_GY_CAMPING_*` and the 80 percent camp are removed; the enemy middle graveyard is a respawn spawn camp and the owner rule forbids it). Same answer as the Warsong Gulch sheet (its question 1; section 15 lists the inherited answers).
- Behind with `left` < 5 min: all-in, HOME down to 1. Level with `left` < 4 min: strike group 5, HOME 3. Ahead by one with `left` < 3 min: HOME 5, strike group 2 only to deny their flag. Below 2.5 min a carrier never hides, it runs (these are `ALL_IN_LEFT_MS`, `LEVEL_LATE_LEFT_MS`, `LEAD1_LATE_LEFT_MS` and `RUN_ONLY_LEFT_MS` in `WsgPlanLogic.h`). A new pickup cycle needs about 3 minutes, so a team that does not carry at `left` < 3 min can only deny.
- Equal captures: the last capture wins at timeout (`:74-75`).
- **Stalemate breaker (both flags carried, state D)**: from 5 minutes of state D on, expert and good bots hunt the EFC with all but 2; at 9 minutes the carriers stop hiding and run anyway. **This depends on the core's debuff schedule (owner question 3)**: with the core as it is (first debuff at 10 minutes) the Warsong Gulch numbers stand; with stacking from 1 or 3 minutes the plan reads the aura stacks of both carriers instead of the clock: hunt as soon as any carrier shows a stack, carriers run when any stack shows on the own carrier and the enemy carrier is not under attack. The plan reads `GetAura(46392)` and `GetAura(46393)` stack amounts, so it needs no change when the core changes.

### Middle deadlock (owner decision 2026-11-19, BT-694)

**Evidence (verified, live runs of builds 24 and 25, `LIVE-CHECKS.md`):** five against five bots, seven minutes, no pickup. Per team three bots fought in the middle of the valley (x 1800 to 1860, y 180 to 220), each side with a healer, nobody died, hp stayed 57 to 98; two bots per team stood at their flag room posts. The plan only changed with the clock, so nothing tipped the fight.

**Rule (owner: "Home bots join after 60 s").** The middle fight is *locked* when, at the same refresh, all of this holds: both teams have at least 2 alive bots in combat inside the middle zone (180 yd round the river centre 1840, 320, which holds both lanes, both middle graveyards and the four buffs, and the live fight; the 70 yd `mid` word of section 8 would have missed it), no flag has left its stand, nobody died since the lock began, and no taker got through (a strike bot of ours within 80 yd of their stand, or the plan's base threat at our stand: 2 or more enemies within 80 yd, call-outs counted). After **60 s** of a locked fight the idle HOME guards of that side leave their posts and walk to the middle of their own fighters in the zone (the point is kept 55 yd out of the enemy middle graveyard: the graveyard rule stands). Idle means not in combat and not threatened; a guard that already left stays in the fight while the lock stands.
- **Release (recomputed every refresh):** a kill (the clock restarts at once if the fight goes on, so the next call needs 60 s more), a flag pickup, drop or reset wait, a taker getting through, the match not running, or fewer than 2 fighters on a side for 10 s (the enemy collapsed, or both sides left; a flicker of the combat flag does not release). The guards then go back to their posts or follow the plan's next state at the plan's next decision (the plan state is re-read at once: the epoch changes with the guards that go).
- **Never strip the defence:** no guard leaves while the enemy carries our flag or the base is threatened; a guard sent while the lock stood is sent back the moment either holds. `HOME_KEPT_AT_POST` is 0 (the owner's rule sends the idle ones); raise it to keep one guard in a room nobody threatens yet.
- **Tier (good rule):** expert and good guards join, casual ones keep their post (a casual bot that stands home while the fight tips is a human mistake, on purpose). A casual guard already in a fight stays in it.
- **Call-out:** the leader says it once when the guards go (`mid is stuck, home push mid`, `push mid, home come`, `all in mid, home too`; the leader always calls its plan, the usual limiter and tier delay apply); with no bot leader the first guard that goes says it.
- **Decision point:** the candidate list `join_fight` (100 when called and the base is quiet, else 10) and `hold_posts` (60), pure in `Utils/TpMidLockPolicy.h` (`Replies`); the rule plays the first. Not yet offered to the decision layer (no D9 slot).
- **Inference, to check live:** both sides apply the rule symmetrically, so at 60 s the fight goes from 3 against 3 to 5 against 5. It tips by the variance of more bodies and of the tier mix (casual guards stay home), and mostly because both bases are left open: a bot that slips past takes a flag with nobody home. A kill releases the guards at once, so a cycle of walk out, kill, walk home may repeat every few minutes; the owner may prefer that the guards stay for 20 s after a kill (question 10).
- This changes one line of section 10 ("HOME and MID may leave a zone only for the EFC") by an owner decision: after 60 s of a locked fight the idle HOME guards may leave it.

**Lane and escort after a kill (owner direction 2026-11-19, BT-695).** Evidence: the build 26 runs released the lock by kills again and again and the takers that re-formed walked straight back into the fight.
- **Other lane.** A kill in a locked fight names the lane of our fighters (the nearer middle graveyard of their middle point: lane A the Alliance graveyard 1749, lane H the Horde graveyard 1750). While the fight holds the takers (TAKER, BACKUP, ESCORT) of both teams walk out by lane H (live, build 28: the Horde taker, backup and escort stood at lane H with the Alliance's; the lane A route of section 2 is the Horde carrier's way home), so a fight on lane H sends them first to the post of the other lane on the enemy half of the valley, then on to the stand: the Alliance to the Horde berserk (lane A, 1725, 446) or the Horde restoration (lane H, 1754, 242); the Horde to the Alliance restoration (lane A, 1952, 384) or the Alliance berserk (lane H, 1934, 226). The posts are the buffs' places from the server data; the walk to them is the navmesh's. A fight on lane A changes nothing. 45 yd from the post counts as gone round (per avoidance; a bot that died starts again). Ends after 10 s without a fight, a flag out, takers through, the match, or 180 s. Casual bots keep their route (a human mistake, on purpose). Candidate list `route_choice`: `other_lane` 140 (casual 90, done 10) and `own_route` 100.
- **Escort.** When a call is released by a kill, one of the joined guards (lowest role index) walks with the best ranked alive striker (taker, then backup, then escort) for up to 150 s instead of going home. Never while a flag is out or the base is threatened (checked at the start and at every refresh); ends when the taker or the guard dies, the takers get through, or the base is threatened. Candidate list `release_reply`: `escort_taker` 100 (5 when not safe) and `walk_home` 60.
- **The detour outranks the fight (BT-705, owner direction 2026-11-20).** A taker the plan put on the detour walks on while it is in combat, like a carrier (trigger `bg detour due`, `bg move to objective` at the emergency priority; the path attacker does not stop it); below 30 percent health it follows the normal combat rules. Live check: a probe row of a bot with a `via-lane` line shows it moving towards (1952,384) or (1725,446) while `attackers` is above 0.
- **Stalemate all in (BT-706).** Section 6 stalemate breaker: after 4 minutes with both flags held the good-tier home guards (all indexes) and the first escort hunt the enemy carrier too (score 870), where only the backup hunted (the build 30 run: 13 minutes, both carriers at debuff 10 at their own stands, no carrier killed). Twin Peaks only. Both teams do it, so both bases open: the first to kill the enemy carrier returns its own flag and its carrier at the stand captures. Inference, to check live: whether the extra hunters beat the carrier's healer and guards.
- **Log lines to expect live:** `[BGTP] inst=N A|H midlock lane: a kill in the locked fight on lane X, N takers go round by lane Y while it holds` (N may be 0: only casual takers, or the fight is on lane A), `... midlock lane over (fight-over|flag|through|match|window)`, `... midlock escort: guard G walks with taker T instead of going home`, `... midlock escort over (reason)`.
- **Inference, to check live:** the other lane's post is reachable from where the taker stands (both lanes join the valley floor); the taker walks the longer way round (about 100 to 200 yd more) and meets the enemy's takers on their own half; the escort stays within sight of the taker only as far as the guard's walk allows (it follows the taker's position every 1.5 s, it does not use the escort follow of the strike roles).

## 7. Comp-dependent choices

- **Healers**: the Warsong Gulch rule stands (the two nearest healers follow the carrier, owner 2026-11-17); the **third healer is the floater**: it stands with COVER, moves to the strike group in state H and to the base in state G. A team with one healer: it follows the carrier only while we carry, else it stays with MID COVER.
- **No healers or no tank**: the guide's premade shape (a tank as the carrier's body, three healers, a floater) does not exist in a random team; the role table and the carrier rule replace it (owner question 4).
- **Stealth classes (rogue, feral druid)**: TAKER and BACKUP, 10 s ahead of the group, wait at the enemy entrance, never take the flag alone while 3 or more enemies stand at the stand. The guide adds that a rogue is also the "second solo defender": with 2 or more stealth bots, the second holds an entrance post.
- **Many casters**: MID holds both lane posts with crowd control and interrupts; the strike group uses a melee pair as its front.
- **Level spread**: brackets are 85-89 and 90 alone; the level rule of Warsong Gulch is unchanged (the highest levels take TAKER and MID).
- **Few bots, more humans** (bots only fill queues): the plan scales by rounding; with 5 to 6 per team it is one HOME, one MID, the rest strike.

## 8. Call-outs and the leader (battleground chat)

**Leader** (new, owner 2026-11-18): one leader bot per side picks the plan and calls it. Rule (deterministic): the best of the bots on the team by tier (expert, good, casual), then level, then GUID hash; a human is never the leader; a leader that leaves is replaced at the next refresh.
It calls the plan at the gates ("balanced, <name> takes, 3 home"), and every change of stance ("all in", "turtle", "flag down fr"). Expert bots follow the leader's plan closely (adopt at the next 2 s refresh), good bots adopt it with their tier delay, casual bots **drift**: they adopt a change one refresh late and about a quarter of them keep their own role for one more refresh.
The leader's stance may later come from a decision layer (section 16); the rules below are the fallback.

Short lowercase lines through the bot voice, one channel for all (`WsgPlan::DeliverCalls`, already used on Warsong Gulch; the channel is the Warsong Gulch sheet's question 3 and stays as answered there). Places for Twin Peaks, matched to the zones of section 2:

| Place word | Zone |
|------------|------|
| `fr` | within 45 yd of a flag stand (inside the flag room) |
| `gy` | within 40 yd of a middle graveyard |
| `lane a`, `lane h` | the valley lanes: 120 yd round each middle graveyard, not `fr` |
| `mid` | the river centre, within 70 yd of 1840, 320 |
| `bers a`, `bers h`, `leaf a`, `leaf h` | within 30 yd of the berserk or restoration object of that lane |
| `door` | within 30 yd of an entrance post |

| Event | Who says it | Example lines |
|-------|-------------|---------------|
| Plan or stance | the leader | `balanced, we take, 3 home`, `all in, last cap wins`, `turtle` |
| Our flag taken | the first HOME or MID bot that sees the EFC | `efc lane h`, `they got our flag, <class>` |
| EFC position changes zone | one MID bot, at most once per 10 s | `efc mid`, `efc past gy` |
| We take their flag | T | `got it`, `escorts on me` |
| Carrier hurt (below 40 percent) | T or the nearest ESCORT | `fc low, heals` |
| Flag dropped | nearest bot | `flag down lane a`, `ret!` |
| Enemy group at our door (3 or more within 40 yd) | nearest HOME bot | `3 inc fr`, `help fr` |
| Wipe | a survivor | `our attack dead`, `their attack dead, go` |
| The middle fight is locked, the home guards go (BT-694) | the leader (else the first guard that goes) | `mid is stuck, home push mid`, `push mid, home come` |
| Capture | nearest bot | `cap 1-0` |
| Time and score | the leader | `3 min left, 1-1, last cap wins` |
| After the match | a bot of the winning side, rough personalities only | `gg`, `wp` (the module's end-of-match chat component, `AiChat::HoldForMatchEnd`, already speaks at the end; its wording is to check; no slurs, no personal attacks) |

Rate limits, call shares by tier (expert 100 percent of events, good 70, casual 30 and sometimes late or wrong) and the cap of 8 lines a minute per team are the Warsong Gulch ones.

**Human call-outs are information, never commands** (owner 2026-11-18; implemented in BT-482, `BgCalloutReader` and `Utils/BgCalloutPolicy.h`, one parser for Warsong Gulch, Twin Peaks and Arathi Basin): a human's short line becomes an observation of the side (15 s life, one line per sender per 2 s, repeats dropped), weighed by the plan against what it sees; a casual bot may ignore it (35 percent do). Read: `inc` or `incoming` with a place and an optional number (`inc lane h 3`, `3 inc door`), `help` with a place or alone (then at the sender's own position), `efc` with a place, `fc` or `flag` alone (our carrier), `<place> clear`, with the place words of section 8 above. A line with any other word, a question, a negation or two verbs is chat. Effects: "inc", "help" and "efc <place>" get one "omw" from the nearest HOME or MID bot that heeds the line; enemies reported at the base (fr, door, roof, ramp) count, weighted by confidence and never below what the plan sees, toward the "base is threatened" test that picks the reply to a wiped attack (state G). `back` and `def` are not read (they were orders). A human flag carrier is treated as the team carrier: the strike group escorts it within 12 yd (Warsong Gulch question 4).

## 9. Class jobs on this map

Class sheets own the spell lists; this table says what each class does for the plan. Every tool named exists in 5.4.8 (name check in `Spell.dbc`, 2026-11-18). The guide's own tools without a 5.4.8 twin
(demon hunter mobility, Shadowy Duel, Fel Rush, Vengeful Retreat, Life Grip, Swap and Glide: no row of those names) are replaced by the class tools below, as owner direction 2026-11-18 asks.

| Class | Preferred role | Jobs on Twin Peaks |
|-------|----------------|--------------------|
| Rogue | TAKER, BACKUP, interceptor | Stealth to the stand, Sprint, Shadowstep, Cheap Shot and Kidney Shot on the enemy taker in the valley (pre-pickup interception, expert), Blind on a guard, Kick the healer; the second rogue holds an entrance post. Stealth and Vanish drop the flag: never while carrying (Cloak of Shadows: to check whether it drops it) |
| Druid | TAKER, BACKUP, healer | Cat form into the base, Wild Charge, **Stampeding Roar for the strike group's outbound ride and the carrier's run**, Mass Entanglement, Cyclone on the EFC when it is not the carrier |
| Warrior | TAKER, ESCORT, MID | Charge and Heroic Leap into the valley fight, Intervene to a carrier in trouble, Hamstring and Piercing Howl on the EFC, Shattering Throw against immunities |
| Paladin | ESCORT, TAKER, healer | Hand of Freedom and **Hand of Sacrifice** on the carrier (the guide's carrier body), Hammer of Justice on the EFC; Divine Shield and Blessing of Protection drop the flag: not while carrying |
| Priest | ESCORT (healer), MID | Shield and heal the carrier first, **Pain Suppression** and **Power Word: Barrier** on the carrier's lane, Leap of Faith pulls a teammate out of a pinch, Angelic Feather on the carrier, Mass Dispel, Fear or Silence on the EFC |
| Shaman | ESCORT, MID | Wind Shear on casters, Earthgrab and Earthbind on the lane, **Windwalk Totem** for the carrier's run (the owner's PvP picks already take it), Ghost Wolf does not drop the flag, Hex the EFC |
| Hunter | MID, HOME | Concussive Shot, Freezing Trap and Binding Shot on the lane or at the stand, Disengage, Master's Call frees the carrier from roots, Scatter Shot |
| Mage | MID, HOME | Frost Nova, Frostjaw, Ring of Frost, Polymorph on a defender (never the carrier), Blink, Time Warp for the strike group; Ice Block drops the flag |
| Warlock | HOME, MID | Fear and Howl of Terror on the EFC, Curse of Exhaustion, Unbound Will, Demonic Circle teleport home, Demonic Gateway between the two ends of a lane if both gates can be placed (to check live) |
| Monk | ESCORT, TAKER | Roll and Flying Serpent Kick to close or break away, **Tiger's Lust** on the carrier, Paralysis on a defender, Leg Sweep, Ring of Peace, Detox (the owner's mistweaver pick is Leg Sweep) |
| Death knight | MID, ESCORT | Death Grip and Chains of Ice on the EFC, Strangulate on a healer, Death's Advance, Gorefiend's Grasp to pull the strike group together |

Dispel on the carrier debuffs does not apply (no dispel type, to check live; the same note as Warsong Gulch).

## 10. Failure modes and what the bots do

| Failure | Cause | Required behavior |
|---------|-------|-------------------|
| Respawned bot stands on its graveyard island | both middle graveyards sit on navmesh islands with no walkable link (module issue 747) | keep `tpJumpDown` and `WsgPlan::GraveyardHop` for map 726 (hop to the lane exit when the objective has no path); measure the time lost per respawn (**to check live**) |
| Carrier dies on the enemy's lane | the shortest route differed from the lane of its own graveyard (module issue 873: carriers died near the enemy graveyard) | the own lane is the default of D4; the other lane only for expert bots with 3 or more enemies in the way |
| Defenders respawn 45 to 50 s from home | the core respawns every death at the middle graveyard | HOME holds the three posts and never chases beyond 40 yd; a base with fewer than 2 defenders calls `help fr` |
| Whole team fights in the valley | every bot attacks the nearest enemy | the guide's biggest mistake (a mid teamfight): HOME and MID may leave a zone only for the EFC; the strike group walks through a fight; casual bots may stop and fight (on purpose) |
| Graveyard camping | 2-0 sends 80 percent to the enemy middle graveyard (`BattleGroundTactics.cpp:2898-2907`, `TP_GY_CAMPING_*`) | the plan replaces it by at most 3 bots at CUT; no bot stays within 45 yd of the enemy middle graveyard for more than 20 s (`MustLeaveGraveyard`, with the radius from section 8) |
| Two flag objects, wrong one clicked | the world DB also holds the flag stands, a ground flag in the Alliance base and buffs | **to check live** which object a click reaches; the capture rule needs the script's object (`BattlegroundTP.cpp:497-498`: only `BgObjects[...]` counts); owner question 5 |
| Flag carrier mounted | mounting drops the flag | blocked (`CheckMountStateAction.cpp:242`); keep. The group dismounts at 60 yd before the enemy base |
| Falls and drowning | ledges at the bases, the water in the valley | `tpJumpDown` (islands), `MoveFall` for a bot far above the surface (`BattleGroundTactics.cpp:2282-2293`), surface swim on all maps (BT-097) |
| Carrier waits for a cap that cannot happen | own flag is taken | hide posts (existing), kill the EFC |
| Bots wait for a debuff that decides the game | Focused or Brutal Assault | stalemate breaker (section 6), reading the live stacks |
| Middle fight locked, nobody dies (live, builds 24 and 25) | healers on both sides outheal three attackers each | after 60 s the idle HOME guards join (section 6, "Middle deadlock") |
| Spawn camping the human | repeated kills at 1749 / 1750 | not an objective; the 3-bot cap and the 20 s dwell rule apply |

## 11. Skill tiers, tricks and awareness

**Tier targets** (owner 2026-11-18): against an average human team the win rate should be **expert 75 percent, good 55 percent, casual 35 percent**. The queue mix stays 10 / 60 / 30 per team, so a mixed team lands near 51 percent
(0.1 x 75 + 0.6 x 55 + 0.3 x 35, inference assuming the tiers combine linearly). Same rules at every bracket: Twin Peaks has only brackets 85-89 and 90, and a rule turns on when the bot knows the spell it needs.
Tier effects use the module's tier delays (expert 0 ms, good 150-400, casual 400-900) and have applied in battlegrounds since BT-153 (`BotSkill.cpp`, `HasTier`).

| Tier | Rules it plays on Twin Peaks |
|------|------------------------------|
| **Core** (casual and up) | walk its role's lane or post; take the flag at the stand; run home by the own lane; click an adjacent own dropped flag; attack enemies within 40 yd; use the nearest buff when hurt; follow the leader's plan late; the carrier rule options run, fight alone, fight rooted |
| **Good** (good and expert) | escort within 12 yd and peel; healers heal the carrier first and follow it; MID cuts off chasers at CUT; HOME holds the three posts; hide when the own flag is taken; the 2-0, time and stalemate rules; state G and H reactions; the carrier uses its helpers |
| **Expert** | pre-pickup interception of the enemy taker; focus fire and crowd control chains on the EFC (stun, root, slow, fear, never polymorph on the carrier); kill timing against the 3 s dropped window and the 23 s reset; the split of the pursuit (two routes so one stun does not catch the group); the carrier's other-lane choice; the leader's close following; accurate call-outs |

**Tricks by tier** (owner list: pillar play, crowd control chains, trinket baiting, arena drinking, Vanish into Sap, Mind Control off ledges, fake casting): on Twin Peaks only some apply.
- Crowd control chains on the EFC and the enemy taker: expert always, good a single link, casual none (casual bots use whatever their rotation casts).
- Trinket baiting: expert opens on the enemy carrier with a weak control (a root or a short stun) to draw its trinket, then chains the strong one; good and casual do not.
- Vanish into Sap: an expert rogue defender that stands at an entrance post vanishes and saps the enemy taker when it picks the flag in our room; good rogues Sap before the pickup; casual rogues fight normally.
- Mind Control off ledges and pillar play: no pillars on this map; a Mind Control of the enemy carrier is not used (it would hand the flag logic to a controlled player; to check live) and ledges are not used for kills on this map.
- Arena drinking: not on this map (no safe pause between fights).
- Fake casting: expert bots only, on about 30 percent of their casts, when an enemy interrupter stands within 30 yd of the cast (shared code, gap G9).

**Awareness by tier** (what the bot knows about the enemy; shared model, gap G7):

| Item | Core | Good | Expert |
|------|------|------|--------|
| Enemy carrier | position only when in sight | position from the flag state on the map each refresh | same, plus its distance along the route and whether it has passed its own graveyard |
| Enemy trinket and cooldowns on the carrier | none | sees a big cooldown that was used a moment ago | tracks the trinket (about 2 minutes after a use) and the main defensives, and times its chain after them |
| Diminishing returns on the carrier | none | none | counts the stun, root and fear categories it chained |
| Enemy group at the base | none | counts enemies within 40 yd of its post | also counts how many enemy players are dead and when they respawn (30 s waves) |
| Stealthers | none | notices a stealth enemy that opens on it | remembers the last place a stealthed enemy was seen and sends one bot |

## 12. Bots today (verified against the code at `main` bafcb06), with gaps

| # | Rule or fact | Where | Status |
|---|--------------|-------|--------|
| 1 | Twin Peaks runs the old Warsong Gulch case: `bg role` = `urand(0, 9)` per bot and the 2 percent re-roll; defenders are `role < 3`; no assigned roles | `BattleGroundTactics.cpp:2806-2990` (the gate `bgType == BATTLEGROUND_WS && selectObjectiveWsg(bg)` at `:2812` excludes Twin Peaks), `BattleGroundJoinAction.cpp` | **missing** for map 726 |
| 2 | `WsgPlan` is bound to map 489 and to the core class `BattlegroundWS`: `WSG_MAP_ID` in `Enabled`, `Decide`, `PlanChanged`, `HealPriority`, `OnTeamChat`; `static_cast<BattlegroundWS*>(bg)` in `BgTacticsWSG.cpp:57` and `WsgPlan.cpp:1386, 1410, 1637`. `BattlegroundTP` is a separate class (it derives from `Battleground`), so a cast would be wrong | `WsgPlan.cpp:47, 1281, 1289, 1399, 1673, 1748, 1811` | **missing** (needs a map adapter) |
| 3 | Carrier rule (run at once, helpers, healers follow): map 489 only; Twin Peaks keeps the older single-attacker window and the carrier hides near its own base when the own flag is taken | `WsgCarrier.cpp:27, 217, 226`; `BattleGroundTactics.cpp` `FightPathAttacker`, `CarrierHoldsToFight` | **missing** for 726 |
| 4 | Both carried: 20 percent support the own carrier, 80 percent hunt the EFC (re-rolled at each refresh); lead 2-0: `role < 8` camps the enemy middle graveyard | `BattleGroundTactics.cpp:2885-2907` | **wrong** (dithering, spawn camp) |
| 5 | Defenders: attack the EFC, else 33 percent roam near their own base, else escort, else hold a flag-room entrance post `role % 3`; attackers: 70 percent pursue the EFC, else escort, 5 percent roam, else push to the enemy stand | `:2921-2985` | partly right (posts exist), **wrong** (no strike group, no lanes) |
| 6 | Carrier paths: eight authored paths, two carrier sets that start at the own graveyard lane (run past the own respawn point) | `:1238-1391`, `:2575-2578` | OK (module issue 873), **to check live** after the plan |
| 7 | Mid graveyard islands: `tpJumpDown` hops from the island to the main mesh; `MoveFall` for a bot in the air | `:2047-2090`, `:2282-2293` | OK, **to check live** (time lost) |
| 8 | Dropped flag: no objective; only an accidental pickup or return | the WS case | **missing** (state E exists for 489 only) |
| 9 | EFC hunting, escort peel, healer priority, call-outs, leader: the plan versions exist for 489 only (`ChooseTargetActions.cpp:85-96`, `PartyMemberToHealValue.cpp:117`, `WsgPlan::DeliverCalls`); no leader and no decision points exist for any map | | **missing** for 726 |
| 10 | Match evidence: `[BGTP]` lines for pickups, drops, returns, captures, score and a state line every 30 s (`TpObserve`), the `[BGF]` carrier lines | `:2103-2230` | OK; the plan's own `[BGWSG]` lines need a Twin Peaks tag |
| 11 | Tiers in battlegrounds | `BotSkill.cpp` `HasTier` | OK (since BT-153) |
| 12 | Human chat reader: map 489 only (`WsgPlan.cpp:1811`); the end-of-match chat component accepts 726 (`AwareAi/AiChat.cpp:1181`, `PvpTriggers.cpp:235`) | | **missing** for the plan, OK for chat |

## 13. Gap list and proposed changes (ordered)

Sizes are changed-line estimates, not measured; ledger ids BT-460 to BT-466 hold the findings.

| # | Change | Files and functions | Size | Risk |
|---|--------|---------------------|------|------|
| G1 | **Map adapter**: replace `BattlegroundWS*` in the plan by a small flag view (state, picker, ground flag guid, object) that both core classes fill; per-map data struct (sides, posts, graveyards, buffs, clock, place table) selected by map id | `WsgPlan.{h,cpp}` (or a rename to `FlagPlan`), `BgTacticsWSG.cpp`, `WsgCarrier.cpp`, `Utils/WsgPlanLogic.h` (`MATCH_LIMIT_MS` to a per-map value) | 150-250 | medium (the Warsong plan must not change; its tests pin it) |
| G2 | **Twin Peaks data and lanes**: sides table (stand, entrance posts, hide posts, lane posts CUT and COVER, graveyards, buffs), place words, route data for the kill window, new `TpPlan` helpers (lane, kill window, respawn side, states G and H) in pure logic | new `src/strategy/bg/TpPlan.{h,cpp}`, `Utils/TpPlanLogic.h`, `tests/test_tp_plan.cpp` | 300-450 | medium |
| G3 | **Turn the plan on for 726**: `Enabled`, the gate in `selectObjective`, `wsgTick`, carrier rule, healer follow, EFC hunt and heal priority for map 726; remove the TP 80 percent camp | `BattleGroundTactics.cpp:2812, 2885-2907`, `WsgCarrier.cpp`, `ChooseTargetActions.cpp`, `PartyMemberToHealValue.cpp`, `PvpTriggers.cpp` | 60-120 | medium |
| G4 | **Debuff-aware stalemate and clock**: read the stack count of 46392 and 46393 from both carriers; `left` from the per-map limit | `WsgPlanLogic.h` helpers, `WsgPlan.cpp` | 50-90 | low |
| G5 | **Respawn model**: `RespawnSide` from where the bot stands after release (own side, mid) so state G works for either core rule; island hop for 726 | `TpPlan.cpp`, `WsgPlan::GraveyardHop` data | 60-100 | low |
| G6 | **Leader and call-outs** (shared by all maps): leader election, plan call at the gates and at stance changes, Twin Peaks place words and the human keywords | new shared `BgLeader` helper (`Utils/BgPlanCommon.h` for the pure part), `WsgPlan::OnTeamChat`, text rows | 250-400 shared, 60 Twin Peaks | low |
| G7 | **Awareness model per tier** (shared): trinket, cooldown and diminishing-return tracking on the EFC, enemy dead counts | new shared `BgAwareness` | 200-350 shared, 40 Twin Peaks | medium (per-tick cost: reads only the carrier, no new scan) |
| G8 | **Decision points** D1 to D8 (section 16): the pure option lists and the fallback tables, the request builder and the answer check | `Utils/TpPlanLogic.h` options, shared `BgDecision` adapter | 150-250 shared, 120 Twin Peaks | low (the rules run when nothing answers) |
| G9 | **Tricks** (expert): pre-pickup interception, CC chain and trinket bait on the EFC, split pursuit, Vanish into Sap | class strategies' CC lines, `TpPlan` | 150-250 | medium |
| G10 | **Live audit**: lane posts and the second berserk on the navmesh, the island hop time, mounts in the battleground, duplicate objects, entrance posts | headless runs and the scenario harness (`.botscenario <name> bg 108 <seconds> bots=...`) | 0 code unless a point fails | |

**Implementation status (2026-11-18, branch `feat/bg-twin-peaks-plan`, no live test yet):**

| Gap | Ledger | Done | Where |
|-----|--------|------|-------|
| G1 | BT-470 | map adapter: `BgFlagMap::MapData` and `FlagView`, `BgFlagView` fills the view from either core class | `Utils/BgFlagMap.h`, `strategy/bg/BgFlagView.{h,cpp}`, `WsgPlan.cpp`, `WsgCarrier.cpp` |
| G3 | BT-471, BT-466 | the plan runs on map 726 (roles, 20/60/20 draw, carrier rule, healers on the carrier, EFC hunt, start spots); camp points and the 2-0 camp deleted | `Utils/TpPlanLogic.h` (`Data`), `BattleGroundTactics.cpp` |
| G4 | BT-472 | stalemate rule reads the live stacks of 46392 and 46393; per-map clock (27 minutes, a 30 minute core read live) | `Utils/WsgPlanLogic.h`, `WsgPlan.cpp`, `Utils/BgFlagMap.h` |
| G5 | BT-473 | respawn model: the rule is learned from the last 6 respawns, the no-camping rule follows it; island hop data is the `tpJumpDown` landing | `Utils/TpPlanLogic.h`, `strategy/bg/TpPlan.cpp` |
| G6 | BT-480 to BT-483 | shared match state (`PvpMatchState`), the leader (election by tier, level, GUID hash; stance call at the gates, at changes and after a handover; the time and score; casual drift), the call-out reader for three maps, expert scout calls ("3 inc lane h", "efc past gy"), the decision hook (D1 offer, answer check, D4, D6, D7 offers) | `PvP/`, `Utils/BgLeaderPolicy.h`, `Utils/BgLeaderAdapters.h`, `Utils/BgCalloutPolicy.h`, `Utils/BgScoutPolicy.h`, `strategy/bg/BgCalloutReader.{h,cpp}`, `WsgPlan.cpp`, `AbPlan.cpp` |
| G2 | BT-474 | lane posts CUT and COVER, kill window phases, floater, states G and H, place words, decision lists D4, D6, D7 | `Utils/TpPlanLogic.h`, `strategy/bg/TpPlan.{h,cpp}`, `WsgPlan.cpp` |
| G8 | BT-500 to BT-503 | the leader asks the decision layer (D1 stance, D6 floater, D7 wipe reply; on Arathi Basin the stance and the target node): request builder, fixed slots, answer check, sidecar link, per-match request cap, log lines `[BGAI]`. Off unless `AwareAi.Enable` and `AwareAi.Pvp.Enable` are 1 | `Utils/AiPvpDecisionPolicy.h`, `AwareAi/AiPvpDecision.{h,cpp}`, `AwareAi/AwareAiProtocol.{h,cpp}`, `PvP/PvpMatchState.{h,cpp}`, `WsgPlan.cpp`, `TpPlan.cpp` |

| G11 | BT-694 | the middle deadlock rule: the lock clock, the zone, the reset on a kill or a flag, the idle guards that join, the leader's call-out, the candidate list | `Utils/TpMidLockPolicy.h`, `tests/test_tp_mid_lock.cpp`, `strategy/bg/TpPlan.cpp` (`MidLockTurn`), `WsgPlan.cpp` (`Resolve`, signature, call-out), `Utils/WsgPlanLogic.h` (`JOIN_MID_FIGHT`, `MID_PUSH`) |

Not done and by design out of this step: G6 partly (the leader, the reader and the expert scout calls are done, see below; the call-outs for the wipe states "our attack dead" and "their attack dead, go" are not), G7, G8 partly (D1, D6 and D7 are asked, see the table; D4 has its list but no carrier path set runs the other lane, so it is not asked; D2, D3, D5 and D8 have no list yet), G9, G10. The expert other-lane choice (D4) has its option list but is not wired to a carrier path set. To check live: the lane post points on the navmesh, the second berserk position (owner question 5), the island hop time, the start spots, the floater, states G and H on a real wipe, mounts at level 85.

Estimated total: 700 to 1,200 lines for Twin Peaks alone, 650 to 1,100 more for the shared layers (leader, awareness, decision adapter), with tests; in four steps: (1) G1, G3, G4 (the Warsong plan runs on Twin Peaks), (2) G2, G5 (lanes, states G and H), (3) G6, G7, G8 (shared layers, which the next maps reuse), (4) G9 and G10.
Core proposals (in the fork, not the module, all owner-decided): C1 clock (2 numbers), C2 stacking debuffs (about 35 lines from the 5.4.8 reference), C3 respawn rule (about 25 lines), C4 duplicate-object cleanup (about 10 rows, after evidence).

## 14. Tests

Pure-logic unit tests in `tests/` (`bash tests/run.sh`, same style as `tests/test_wsg_plan.cpp`): role assignment for 5 to 10 players (counts per plan, stability under a death, tier effect, stealth class first, the leader never the taker unless best); lane choice and kill window (positions on the route, own lane default, other lane only when 3 enemies are in the way); `RespawnSide` for both respawn rules;
states G and H from a roster; score and time rules with both clock limits; stalemate thresholds with stack counts 0 to 10; place words for the call-outs; leader election; the decision options: each point returns the same list for the same input, the fallback top option equals the rule, an unknown answer id falls back; the human keyword reader.

Live (the maintainer's run, human queue; Twin Peaks needs level 85 characters):
- Headless test client (not part of this repository): a level 85 human in the queue (the bracket is 85-89); the scenario harness can seed both teams: `.botscenario <name> bg 108 <seconds> bots=<names> strat=<a>,<h> roles=index|natural` (the bg type id is 108; bots at level 85 have to exist: the live server has none yet).
- Read in the log: `[BGTP]` lines (pickups, drops, returns, captures, score, 30 s state), `[BGWSG]` decision lines tagged with the map, `[BGF]` carrier lines, deaths per role, stuck bots, the time between a death and the next position change at a graveyard.
- Measurements for the acceptance: first pickup after the gates (expected 75 to 120 s); carry time per pickup (expected 105 to 130 s); captures per match; carrier deaths inside the kill window versus after the own graveyard; healer distance to the carrier (target within 25 yd for most of the carry); respawn-to-moving time at the islands; HOME bots at the posts while no enemy carries.
- Acceptance: the match finishes by 3 captures or the clock; both sides pick up a flag within 4 minutes of the gates; a human team can win and lose (at least one of each in four runs with the same human play); each team keeps 2 or more bots at its posts while no enemy carries our flag; no bot stands still more than 60 s alive out of combat; fewer than 8 call-out lines a minute per team; the stalemate breaker ends a forced both-carried stalemate.
- **Bench for the tier targets** (design decision 0032): bot-versus-bot matches by tier pair (expert team against good, good against casual, expert against casual), at least 20 matches per pair, win rate per pair and captures, carrier lifetimes and flag hold times; the expert team must beat the others by a clear margin before human tests; then the owner's sessions and the feedback form.

## 15. Owner questions (recommended option first)

The answers of the Warsong Gulch sheet (its questions 1 to 8: the 2-0 behavior, plan variety, chat channel, human carrier, tiers in battlegrounds, stealth takers, spawn camping)
carry over unchanged unless a question below says otherwise. That sheet is marked owner approved on 2026-11-12; the maintainer confirms that its recommended options were the answers.
Questions 1 to 3 and 5 are guide-or-source versus core conflicts, so each one is the owner's to decide (nothing here is resolved silently).

1. **Where the dead respawn in play** (section 1, row 1; fact check 1 in section 17). (a) Recommended: keep the core as it is (always the team's middle graveyard) and plan for it; the plan
   reads where the bot stands after the release, so it works under either rule, and a source for 5.x is still missing (both 5.4.8-era references agree with the core; the guide and the 4.3.4 core are the other side). (b) Port the 4.3.4 rule to the fork:
   a death nearer the enemy flag room than the enemy middle graveyard respawns at the own start graveyard (132 to 149 yd from the stand), else at the middle one (about 25 lines in `BattlegroundTP::GetClosestGraveYard`). (c) Always the start graveyards (not attested by any source).
2. **Clock** (section 1, row 3; section 17). (a) Recommended: fork fix to the 5.4.8 reference's values: end at 27 minutes of instance time (25 of play) and show `25 - minutes`: two numbers, `BattlegroundTP.cpp:63` and `:85`. The core's own timer display already starts at 25 and the Warsong Gulch script ends at 27, so 30 and 28 are the odd ones out. (b) Keep 30 and make the display start at 28 (play time 28 minutes). (c) Leave both (the display stays wrong by about 2 minutes).
3. **Carrier debuffs while both flags are carried.** (a) Recommended: port the 5.4.8 reference's stacking (one stack a minute, Brutal Assault from the fifth stack, up to 10) with the **first stack at 1 minute** as the wiki's hotfix of 2014-03-04 says
   (about 35 lines in `BattlegroundTP.cpp:143-167` and the same in `BattlegroundWS.cpp`; one decision for both maps). (b) Port it with the reference's 3 minutes. (c) Keep the core (10 and 15 minutes, once, no stack). The module reads the live stacks, so no module change follows from the answer.
4. **The guide's team shape** (premade: a tank as the carrier's body, three healers, a floater). (a) Recommended: use the Warsong Gulch role table and carrier rule for random teams, the third healer as the floater. (b) Build the guide's shape when the team has a tank and three healers, else (a).
5. **World DB objects on map 726** (duplicate flag stands, a Horde ground flag in the Alliance base, five stacked restoration rows, a berserk row 16 yd from the script's). (a) Recommended: a live check first (list the game objects of entries 179830, 179831, 179786, 179904-179907 in a running match with the GM probe and click each flag), then, with that evidence, a fork SQL fix (and, if the DB position is the right one, the script's second berserk moved) per the duplicate-spawn rule. (b) Change the script's berserk position to the DB row now. (c) Leave.
6. **How to test with no random bot at 85.** (a) Recommended: the scenario harness with 20 seeded level 85 bots and the owner's level 85 character in the queue; the sheet's G1 to G5 can be unit tested and syntax checked before. (b) Wait until the bots reach 85 on the level path.
7. **Leader choice.** (a) Recommended: best tier, then level, then GUID hash (an expert leads when there is one); a human is never the leader. (b) Highest level regardless of tier. (c) A random bot per match.
8. **How much casual bots drift from the leader's plan.** (a) Recommended: a change reaches a casual bot one refresh late and about a quarter keep their own role for one more refresh. (b) No drift (they follow like good bots, only later).
9. **Camp point.** (a) Recommended: delete the Twin Peaks graveyard camp points (`TP_GY_CAMPING_*`) with the plan, as the Warsong sheet did for its 2-0 rule. (b) Keep them for casual bots only.
10. **Guards after a kill in the middle deadlock (2026-11-19, BT-694).** (a) Recommended for the first live run: a kill releases the guards at once, as the owner's rule reads. (b) The guards stay 20 s after a kill, so the side that just won a fight can finish it before they walk home. Decide after the live run shows how often the cycle repeats.


**Owner answers (2026-11-18):** 1 (a) keep the core's middle graveyards; 2 (a) fork fix to 27 minutes with a `25 - minutes` display; 3 (a) port the stacking debuff with the first stack at 1 minute, for Twin Peaks and Warsong Gulch; 4 (a) the Warsong role table, third healer as floater; 5 (a) live check first, then a fork SQL fix with evidence; 6 (a) seed 20 level 85 test bots in the scenario harness; 7 (a) best tier, then level, then GUID hash; 8 (a) casual bots one refresh late, about a quarter keep their role one more refresh; 9 (a) delete the camp points. Also decided: bots share what they see only through visible battleground call-outs (experts call out more often); Battle for Gilneas rates stay as the core has them until more research settles them.

## 16. Decision points for the decision layer (leader bot, every 15 to 30 s)

Owner direction 2026-11-18: strategy of the leader bot may be decided by a later decision layer (design decision 0014, 0026) every 15 to 30 s, with a rule fallback; moment-to-moment play stays scripted. Design: each decision point is a pure function that turns the state into
a **candidate list** with integer scores (highest first, ties by lower id). The layer, if present, answers with one candidate id; **the fallback is the first candidate**, used whenever there is no answer within 2 s, the id is unknown, or the layer is off.
Candidates never carry free coordinates, only ids the plan already knows (lanes, posts, roles). One request in flight per side; asked every 20 s (15 to 30 s by the leader's tier clock) and at once, but not within 5 s of the last, after a flag event (pickup, drop, return, capture), a wipe or a leader change.

State record sent with each request (about 1 kB, all read from the match record the plan already keeps, no new scan): score both sides; `left` in seconds; both flag states; carriers' class, health band and stack counts; the EFC's fraction of its route and whether it has passed its own middle graveyard; alive bots per role group and their distance to the stand; dead per team and seconds to the next 30 s wave; enemies seen within 60 yd of each post; buffs ready; the current stance, the time the current plan has run, whether a human is the carrier.

| # | Asked when | Candidates (id: meaning) | Fallback score (the rule) |
|---|------------|--------------------------|----------------------------|
| D1 Stance | every ask | `balanced`, `offense`, `defense`, `all_in` (HOME 1, strike 5), `turtle` (HOME 5, strike 2 only to deny) | the rule's stance scores 100 and is the first candidate; the others score below it: in normal play the other two draws (`balanced` 60, `offense` 50, `defense` 50); ahead by two (`defense` rule) `balanced` 40; behind with `left` <= 5 min (`all_in` rule) `offense` 40; level with `left` <= 4 min (`offense` rule) `balanced` 40; ahead by one with `left` <= 3 min (`turtle` rule) `defense` 45 and `balanced` 30. The draw 20 / 60 / 20 at the start is the first answer. *Note 2026-11-18: this row replaces the first draft (`all_in` 100, `defense` 100 or 90 for `turtle`, `offense` 80, `balanced` 60 by condition) with the numbers the code and its tests use (`BgLeaderAdapters.h`, `StanceOffer`): the draft scored `defense` and `turtle` for the same situation and disagreed with the plan's own situations. The decision layer is asked only where a second option scores 45 or more (normal play, ahead by one late), because the other situations have one sensible answer.* |
| D2 Pickup timing | both flags home, strike group out | `go_now`, `wait_group` (until 3 of the group are within 25 yd of T), `wait_reset` (the flag respawns in under 23 s) | `wait_reset` 100 if a reset is running; `wait_group` 80 until the group has gathered or 75 s passed; else `go_now` |
| D3 Outbound route | at the gates and after every capture | `shortest` (the lane of section 2), `other_lane`, `split` (E to the other lane, expert only) | `shortest` 70; `other_lane` 100 only if the enemy carrier is on the shortest lane and our flag is taken; `split` 60 for expert groups of 4 or more |
| D4 Carrier lane | our pickup | `own_lane` (the shortest route, section 2), `other_lane` | `own_lane` 100; `other_lane` 110 only for an expert carrier when 3 or more enemies stand within 60 yd of the first 100 yd of the own lane |
| D5 Hunt size | enemy pickup | `hunt_2`, `hunt_4`, `hunt_6`, `hold_posts` | `hunt_6` 100 while the EFC has not passed CUT and the carrier is alone; `hunt_4` 90 while it is inside the kill window; `hunt_2` 80 after it passed its own graveyard; `hold_posts` 85 when 2 or more enemies are within 60 yd of a post |
| D6 Floater | every ask | `to_strike`, `to_home`, `stay_cover` | `to_strike` 90 if their attack is wiped (state H); `to_home` 95 if ours is wiped or home has fewer than 2 bots; else `stay_cover` 60 |
| D7 Wipe response | after state G | `defend_first`, `push_now`, `regroup_lane` | `defend_first` 100 if the enemy carries or 2 or more enemies are within 80 yd of the stand; `push_now` 90 if the enemy offense is also dead; else `regroup_lane` 70 |
| D8 Stalemate | both flags carried | `hold_hide`, `hunt_all_but_2`, `run_home` | `hunt_all_but_2` 100 after 5 minutes of state D or at the first stack on either carrier; `run_home` 100 after 9 minutes of state D or at 2 stacks on the own carrier when the enemy carrier is not under attack; else `hold_hide` 60 |

The call-out text of the leader is not a decision point: the lines of section 8 follow the chosen stance. Casual bots drift from every answer (section 8); that is where a human can punish them.

**D9 (not wired yet): middle deadlock reply**, asked when the lock holds 60 s: `join_fight` 100 when called and the base is quiet, else 10; `hold_posts` 60. The list is built (`TpMidLock::Replies`) and the rule plays its first candidate; the decision layer has no slot for it, so a model cannot pick `hold_posts` yet.

## 17. Fact checks (owner request, 2026-11-18; nothing in the core was changed)

### 17.1 Twin Peaks respawn: did MoP choose graveyards differently from this core?

Result: **not proven for 5.4.8; the evidence is split by version.**

| Source | What it says | Read |
|--------|--------------|------|
| This core (`home`) | in play, every death goes to the team's middle graveyard (1749 Alliance, 1750 Horde); before the gates the flag-room graveyard; the start graveyards 1728 and 1729 get spirit guides and are never chosen | `BattlegroundTP.cpp:864-885` |
| Reference 5.4.8 core (`references/loa-548`) | **the same code**, line for line (always the middle graveyard in play) | `BattlegroundTP.cpp:861-882` |
| Reference 4.3.4 core (`references/tcpp-434`) | unrated: if the player is nearer (planar) to the enemy flag-room graveyard than to the enemy middle graveyard, the own start graveyard (1729 / 1728), else the own middle graveyard; rated: the start graveyard when the player is in area 5680 (Horde) or 5681 (Alliance), else the middle one; the flag-room graveyard before the start; spirit guides at all six | `BattlegroundTP.cpp:434-462`, `BattlegroundTP.h:148-160`, `:283-304` |
| The Legion-era guide | dying past the centre line sends you to the far graveyard in the own base, on your side to the middle one | digest, own words |
| the game wiki | each faction has two graveyards, a dead player resurrects at one of the two; the page gives no rule and records no change of it after 2011 | read 2026-11-18 |
| Wowhead MoP Classic | the zone page is an empty shell; the guide URLs tried answered 404 or a 9.2.5 user guide whose body was not served: **no usable fact** | sources note |

Reading: the 4.3.4 core and the Legion guide agree on a two-graveyard rule, and the wiki's wording ("one of two") fits it; the only 5.4.8-era code, the reference core, uses the simple rule, but it comes from the same ancestor as this core, so it is not independent proof.
Nothing found says the rule was removed in 5.x, and nothing proves it stayed. Inference only.

Proposed core fix if the owner chooses option (b) of question 1 (fork file `src/server/game/Battlegrounds/Zones/BattlegroundTP.cpp`, function `BattlegroundTP::GetClosestGraveYard`, about 25 lines): keep the not-in-progress branch; in progress and not rated, compute `player->GetDistance2d(enemy flag-room graveyard x, y)` against
`player->GetDistance2d(enemy middle graveyard x, y)` and return the own start graveyard (`TP_GRAVEYARD_START_ALLIANCE` / `TP_GRAVEYARD_START_HORDE`) when the first is smaller, else the own middle graveyard; for a rated match use the areas 5680 and 5681 as the 4.3.4 core does (`isRated()` exists in `Battleground.h:371`). The spirit guides at all four graveyards are already spawned (`:734-761`).
Side notes found while reading (low, nothing to do for the sheet): the four spirit guides go into a two-slot `BgCreatures` array (types 0, 0, 1, 0), so only the last of the first type is remembered for cleanup (`:742-761`); the module's plan works under both rules.

### 17.2 Battle for Gilneas values: win 2000, 10 per 12 s (1 base), 10 per 6 s (2), 30 per 1 s (3)

Result: **the score to win is confirmed; the rates are this core's and the 4.3.4 core's, and the 5.4.8 reference core has different ones. No source read says which is right for build 18414.**

| Source | Win | 1 base | 2 bases | 3 bases | Read |
|--------|-----|--------|---------|---------|------|
| This core | 2000 (warning 1800) | 10 per 12 s (0.83/s) | 10 per 6 s (1.67/s) | 30 per 1 s (30/s) | `BattlegroundBFG.h:102-103`, `:170-171` |
| Reference 5.4.8 core | 2000 (1800) | **10 per 8 s (1.25/s)** | **10 per 3 s (3.33/s)** | 30 per 1 s (30/s) | `references/loa-548/.../BattlegroundBFG.h:100-101`, `:168-169` (the player-bots reference clone has the same) |
| Reference 4.3.4 core | 2000 (1800) | 10 per 12 s | 10 per 6 s | 30 per 1 s | `references/tcpp-434/.../BattlegroundBFG.h:112-113`, `:206-207` (identical to this core) |
| the game wiki | 2000 | not on the page | not on the page | not on the page | read 2026-11-18 (it lists capture times 8 s from 2011, 7 s in 2015, 6 s in 6.2 and the 2014-03-11 MoP hotfix on honor per resources) |
| Wowhead MoP Classic | page is an empty shell | | | | zone 5449 and the 9.2.5 guide 906: no values |
| The Legion-era guide | 1500, 1 / 3 / 5 per second | | | | later redesign, not applicable |

So the owner's values match the core exactly and match 4.3.4; **one mismatch with the 5.4.8 reference: the 1-base and 2-base intervals (12 s and 6 s against 8 s and 3 s)**. Effect of the difference: with two bases held all game a team reaches 2000 in 20 minutes (core) or 10 minutes (reference); one base alone is 0.83/s against 1.25/s; three bases end a match in 67 s either way. The core also has no time limit (checked by search: no clock test in `BattlegroundBFG.cpp` of either core), so the rate decides the length of a lost-node game.
Nothing changed. Recommended for the Battle for Gilneas sheet: keep this core's rates until a 5.x source for the rate is found (owner question for that sheet), and let the plan read the node count and the live score, not the clock. Another known item from the digest, not part of this check: `GetClosestGraveYard` in the BFG class indexes `GILNEAS_BG_GraveyardIds[teamIndex + 5]` on a 5-entry array (inference from reading; for the BFG sheet).
