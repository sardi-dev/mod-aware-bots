# Eye of the Storm playbook

Status: **owner approved 2026-11-18 (answers in section 15); E1 to E8 implemented on `feat/bg-eots-plan` (BT-580 to BT-586, unit tested and syntax checked, not yet tested live); E9 (live audit, BT-579) waits for the live test.** Written 2026-11-18 against the module
`main` at 8df27aa (the Warsong Gulch, Arathi Basin, Twin Peaks and Battle for Gilneas plans, the node-map table, the leader, the
call-out reader, the decision layer and the awareness layers are in it) and the core at 9744be464a. Eye of the Storm is the
fifth map in the owner's order (owner direction 2026-11-12, design decision 0032). It is the one map that is **both** a node race and a flag
game: this sheet reuses the node-map table of Arathi Basin and Battle for Gilneas (`docs/bg-sheets/battle-for-gilneas.md` section 18) for the
bases and the Warsong Gulch and Twin Peaks flag code for the carrier (`docs/bg-sheets/warsong-gulch.md` section 6), and spells out what
differs: **capture is presence, not a click** (no cast, no contest timer, a progress bar read every 2 s), a **single flag in the
middle** whose value rises with the bases held, two near bases per team, and a team of up to 15. Queueing and the human-queue fill are not
part of this sheet.

Legend: **verified** = read in the core, the world data, the client data or our code (file and line given); **inference** = derived from
verified facts; **to check live** = needs a match; **owner question** = a source disagrees with the core, or a choice is open, and the
owner decides (section 15). Rules are marked core, good or expert like the class sheets: casual bots follow core rules only and react late.

## 1. Header

| Item | Value | Source |
|------|-------|--------|
| Map id, battleground type id | 566, 7 (`BATTLEGROUND_EY`) | `battleground_template` row 7 |
| Team size | **8 to 15 per team** (template MinPlayersPerTeam 8, MaxPlayersPerTeam 15, Weight 4, StartMaxDist 75). The Skill-Capped guide is the 10 v 10 rated format; the random queue is up to 15 v 15 | read-only query 2026-11-18 |
| Brackets (5.4.8) | **35-39, 40-44, 45-49, 50-54, 55-59, 60-64, 65-69, 70-74, 75-79, 80-84, 85-89 and 90 alone** (12 brackets); template MinLvl 35, MaxLvl 90 | `PvpDifficulty.dbc` rows for map 566 (ids 52-54, 156-163, 666), read 2026-11-18 |
| Owner's band | the map is reachable now: on 2026-11-18 there are 56 random bots at 35-59, 25 at 60-84 and none at 85 or above (249 below 35); the bracket 35-39 and up to 55-59 can be played with real random bots, the higher brackets need seeded bots | read-only count of `characters.characters` for the bot accounts |
| Win | first team to **1600 resources** (`BG_EY_MAX_TEAM_SCORE`); the 1400 warning constant is never used and its strings do not exist, so **no 1400 message** | `BattlegroundEY.h:212-213`, `.cpp:257-284` (the warning block is commented out) |
| Rates | 1, 2, 3, 4 bases: **1, 2, 5, 10 points per 2 s tick** = 0.5, 1.0, 2.5, 5.0 per second. Owner ruling: the core's values stay | `BG_EY_TickPoints`, `BG_EY_FPOINTS_TICK_TIME` 2000, `.h:286`, `.cpp:44-54` |
| Flag worth | **75, 85, 100, 500** for 1, 2, 3, 4 bases held by the capturing team; nothing with no base (cannot happen: capture needs an owned base). Owner ruling: the core's values stay | `BG_EY_FlagPoints`, `.h:287`, `.cpp:795-796` |
| Capture | a **progress bar** per base, read every 2 s: net players of one side within **70 yd** (3D) of the base's cap object, clamped to 5 for the Alliance only (finding F1); 8 s at 5 or more net, 40 s for one player. Owner ruling: the core's bar stays; **bots bring 3 or more to capture** | `.cpp:126-255`, `BG_EY_POINT_RADIUS` 70, `BG_EY_POINT_MAX_CAPTURERS_COUNT` 5 |
| Time limit | **none**: the match ends only at 1600 or by forfeit (the 47 minute limit in `Battleground.cpp` is arena only) | `.cpp:42-83` |
| Early end | a team below 8 players for the premature-finish timer (core default 5 minutes, live config not read) ends the match; winner = higher score | `Battleground.cpp`, `BattlegroundEY.cpp:932-940` |
| Preparation | 2 minutes with closed doors; start rooms 184719 (Alliance) and 184720 (Horde) | `.cpp:414-415`, constructor `:34-37` |
| Sources | core `BattlegroundEY.{h,cpp}` in the core; world DB and client DBC read-only (`AreaTrigger.dbc`, `WorldSafeLocs.dbc`, `PvpDifficulty.dbc`, `SpellEffect.dbc`); our tactics; the game wiki Eye of the Storm (read 2026-11-18); the Skill-Capped Eye of the Storm rated-battleground guide (8:30, paid; own words, no quotes; it describes the Legion era); Wowhead spell page 34976 (read 2026-11-18). Based on Wowhead data (the carrier aura has a speed limit and no damage-taken effect). Notes: `.agents/plans/bots-bg-sheets/sources/eye-of-the-storm.*.md` | |
| Earlier tactics | the module's old case only (`BattleGroundTactics.cpp:3375-3712`, strategies balanced, front, back, flag focus) and the authored paths `vPaths_EY` (`:903-980`, `:1452-1468`); no earlier module has more | read |
| Status | owner approved 2026-11-18 | |

### Where sources and the core disagree (all go to section 15)

| # | Item | This core (`home`) | Other source | Question |
|---|------|--------------------|--------------|----------|
| 1 | Team size | 8 to 15 per team | guide 10 v 10 (rated) | none: the data wins; every count scales over 8 to 15 |
| 2 | Capture time | bar: 8 s at 5 or more net, 14 s at 3, up to 40 s alone | guide 6 s (Legion, "unlike random battlegrounds"); wiki: 8 s then 7 s (2015) then 6 s (6.2.0), no figure for this patch | none: owner ruling, the core's bar stays |
| 3 | Rates | 0.5, 1, 2.5, 5 per second | guide 2, 3, 4, 12 per 3 s; wiki 1, 2, 5, 10 (per tick, interval not given) | none: owner ruling; the wiki and the core agree on the numbers |
| 4 | Flag worth | 75, 85, 100, 500 | guide 75, 85, 95, 500; wiki 75, 85, 100, 500 | none: the core and the wiki agree |
| 5 | Win score | 1600, near-victory message never sent | guide none; wiki 1600 | none; a note for the 1400 line (F4) |
| 6 | Carrier debuffs | speed capped at 14 yd/s, nothing else | guide none; Focused and Brutal Assault are patch 6.0.2 (2014-10-14), later than this build; Wowhead page: speed limit 14, no damage taken | none |
| 7 | Capture counts "at most 5" | the Alliance side only (finding F1); Horde net 6 to 10 moves the bar 6 to 10 | the constant's name and the owner ruling ("8 s with 5 or more") | 5 (fork fix, BT-576) |
| 8 | Respawn | nearest owned graveyard or the start room by 3D distance, no river line | guide: same rule; rated "release to team start" is a 2014-12-05 hotfix, later than this build | none: the plan plays the core rule |
| 9 | Flag pickup time | the module casts the 8 s capture spell at the centre flag; a dropped flag is an instant click | wiki: rated pickup 7 s then 6 s (later); centre flag capture 10 s to 8 s in 3.2.0 | 10 (to check live) |
| 10 | Level range | template and brackets 35-90 | wiki infobox 20-80 and "queue from 15"; wiki history: minimum level 35 from 4.0.3a | none: the data wins |
| 11 | Team shape | no rule | guide: 3 healers, 7 damage, a balance druid, a warlock for the gateway, two solo defenders | none: random teams; the role table below replaces it (the Warsong decision) |

## 2. Map and positions

All coordinates are map 566 (`BattlegroundEY.h:228-234`, `.cpp:412-505`, the DBC). x runs north and y west in game coordinates. The Alliance starts in
the north (x about 2524), the Horde in the south (x about 1803); the **bases are in the four corners, the flag in the middle on a platform
about 30 yd lower**. Point ids are the core's: Fel Reaver 0, Blood Elf 1, Draenei Ruins 2, Mage Tower 3. **Near bases**: Alliance Mage Tower and
Draenei Ruins, Horde Fel Reaver and Blood Elf (nearest by distance from the start room, the guide's split).

| Base | Id | Flag and capture trigger (x, y, z) | Cap object (center of the 70 yd circle) | Flag trigger id | Buff trigger | Own graveyard (id) | Flag to graveyard | Side |
|------|----|-------------------------------------|------------------------------------------|-----------------|--------------|--------------------|-------------------|------|
| Fel Reaver Ruins | 0 | 2044.28, 1729.68, 1189.96 | 2024.60, 1742.82, 1195.16 | 4514 (also 4515 ignored) | 4569 at 2046.33, 1748.81, 1190.03 | 1105 at 2013.06, 1677.24, 1182.13 | 62 yd | Horde near (south-west) |
| Blood Elf Tower | 1 | 2048.83, 1393.65, 1194.49 | 2050.49, 1372.24, 1194.56 | 4476 (4512) | 4568 at 2050.46, 1372.26, 1194.56 | 1106 at 2012.40, 1455.41, 1172.20 | 75 yd | Horde near (south-east) |
| Draenei Ruins | 2 | 2286.56, 1402.36, 1197.11 | 2301.01, 1386.93, 1197.18 | 4518 (4519) | 4571 at 2302.69, 1391.27, 1197.77 | 1107 at 2351.22, 1455.63, 1185.33 | 85 yd | Alliance near (north-east) |
| Mage Tower | 3 | 2284.48, 1731.23, 1189.99 | 2282.12, 1760.01, 1189.71 | 4516 (4517) | 4570 at 2283.38, 1748.98, 1189.71 | 1108 at 2355.30, 1683.71, 1173.15 | 87 yd | Alliance near (north-west) |
| Centre flag (stand 184141) | - | 2174.78, 1569.05, 1160.36 | - | - | - | - | - | middle, 196 to 216 yd from each base |
| Alliance start room | start loc 1103 | 2523.76, 1596.13, 1247.70 | - | 4531 (r 28) | - | 1103 (main) | - | door 184719 at 2527.60, 1596.91, 1240.70 |
| Horde start room | start loc 1104 | 1803.02, 1539.97, 1249.19 | - | 4530 (r 28) | - | 1104 (main) | - | door 184720 at 1803.21, 1539.49, 1242.19 |

The flag-capture triggers have a radius of **3 yd** (`AreaTrigger.dbc`); the capture of the bar uses the **cap object** with a radius of
**70 yd** (3D). The cap object is 21 to 29 yd from the flag. One of the three buffs (speed, restoration, berserk) spawns at each buff trigger
when the doors open and is re-rolled by the base class (`BattlegroundEY.cpp:95-110`, `m_BuffChange` constructor `:26`), so the kind at a base is
random each match; a buff is 18 to 21 yd from its flag, inside the guard's post.

**Distances** (straight lines in the plane from the coordinates; "path" is the straight line times 1.4, the Arathi Basin detour factor, an
inference because the module's EY paths are only waypoints; time at 7 yd/s on foot, 11.2 yd/s mounted (`AbLogic::MOUNT_SPEED`, the 60 percent mount;
the 100 percent mount is 14 yd/s and is to check live):

| From | Mage Tower | Draenei Ruins | Fel Reaver | Blood Elf | Centre flag |
|------|------------|---------------|------------|-----------|-------------|
| Alliance start | **275**, path 385, 55 s / 34 s | **306**, path 428, 61 s / 38 s | 498 | 516 | **350**, path 490, 70 s / 44 s |
| Horde start | 518 | 503 | **307**, path 430, 61 s / 38 s | **286**, path 400, 57 s / 36 s | **373**, path 522, 75 s / 47 s |
| Centre flag | 196 | 201 | 207 | 216 | - |

Between bases: Fel Reaver to Blood Elf 336, Blood Elf to Draenei Ruins 238, Draenei Ruins to Mage Tower 329, Mage Tower to Fel Reaver 240,
the two diagonals 407 and 412. Facts that follow:
1. **The near bases are as far as the centre** (275 to 307 yd against 350 to 373): a team gets to its two bases and the middle within about
   15 s of each other on the walk, and **the Alliance reaches the centre about 23 yd (2 s mounted) before the Horde** (350 against 373). To check
   live whether the routes change that.
2. **The centre is 196 to 216 yd from every base**: a carrier at the centre is the same distance from all four bases (the guide's "pick the base
   that is yours"), and a trip is 14 s at the 14 yd/s cap, 18 s mounted at 11.2, 29 s on foot.
3. **A graveyard is 62 to 87 yd from its flag**; from the cap object (the circle's center) it is 68 yd for Fel Reaver (inside the 70 yd circle), 86 for Draenei Ruins,
   94 for Blood Elf and 107 for Mage Tower (outside). A defender who dies at a base comes back in the next wave about 60 to 90 yd
   away, **at the base**; a bot that dies at the centre or at an enemy base comes back at the **nearest owned graveyard**, see the table below.

**Respawn geometry** (core rule: the graveyard of the held base nearest to the death spot by 3D distance, else the start room, `:869-920`; waves
every 30 s). Distances in yards from the death spot to the graveyard that is chosen when a team holds **only its own two near bases**:

| Death at | Alliance (Mage Tower, Draenei Ruins held) | Horde (Fel Reaver, Blood Elf held) |
|----------|--------------------------------------------|--------------------------------------|
| Centre flag | Draenei Ruins graveyard, 211 | Fel Reaver graveyard, 196 |
| Own near base | that base's graveyard, 85 to 87 | that base's graveyard, 62 to 75 |
| The enemy's near bases | the other own base, 309 to 315 | the other own base, 277 to 280 |

With **no base** the whole team comes out in the start room: 275 to 520 yd from every base. With **all four** held a bot dies and returns at the nearest
base graveyard (the centre: Fel Reaver 196 yd for both teams, so the Alliance's dead from the centre come out on the Horde's side if it holds
Fel Reaver). So **holding bases is also holding the way back**: three or four bases mean the dead return at the fight in 60 to 90 yd, the team
without bases walks 280 to 520 yd. The mid-platform edge and the fall: the core has no fall rule and no void respawn; `WorldSafeLocs` 1129 and 1130
(2174.90, 1594.51, 1126.51 and 2173.64, 1534.45, 1129.24) lie 34 yd below the stand and the script never reads them. The edge geometry is **position needed**
(to check live with the headless client).

Our static data today: the node positions, 5.0 yd random points around them, the authored paths `vPaths_EY` for the start-room legs (`BattleGroundTactics.cpp:903-980`), the retreat spots and the camping spots
(`:192-195`), the platform jump-down of the start rooms (`eyJumpDown`, `:1955-2040`). No posts, no 70 yd circles, no centre geometry.

## 3. Mechanics that matter for play (verified in `BattlegroundEY.cpp` / `.h` unless stated)

**Taking a base is standing in it.** Every 2 s the core lists the alive, unstealthed, uninvisible players of each team within 70 yd (3D) of the base's cap
object (`CanCaptureTowerPoint`, `Player.cpp:21048`; mounted players count) and moves the bar by the **net** number (`:204-255`).
- Bar 0 to 100, **starts at 50**. Alliance owns at **70 or more**, Horde at **30 or less**, between the two nobody. Alliance: net of at most 5 per tick (the
  clamp, `:211`). **Horde: no clamp**, so a net of 6 to 10 moves the bar 6 to 10 (F1).
- A neutral base is taken when the bar reaches the side's threshold from 50: **20 points**. Seconds = 2 x ticks (the tick that brings the first players in
  counts, so add up to 2 s):

| Net players at the base | 1 | 2 | 3 | 4 | 5 | 6 | 7 to 9 | 10 |
|-------------------------|---|---|---|---|---|---|--------|----|
| Take a neutral base, seconds | 40 | 20 | **14** | 10 | **8** | 8 (Horde) | 6 (Horde) | 4 (Horde) |

  The Alliance does not go below 8 s whatever its number (the clamp); a Horde of 7 to 9 takes it in 6 s, of 10 in 4 s. This is F1.
- **A base is lost the moment its bar leaves the owner's side**, for the first enemy player in the list: **a freshly captured base (bar at 70 to 74 for the Alliance, 21 to 30 for the Horde: the last
  tick overshoots by up to net minus 1) falls to a net of one to five enemies in one tick (2 s)**; the base then stands neutral and the owner stops scoring at once.
- **The owner's presence hardens the bar toward 100 (Alliance) or 0 (Horde)**; there is no decay, the bar stays where the last tick left it. A base held by one
  defender is 100 percent after 60 s, by three after 20 s, by five after 12 s (from 70 to 100). To neutralise a hardened base from 100: net 3, 22 s; net 5,
  14 s; net 10, 8 s. To **flip** it (neutralise and capture): about 30 s at a net of 5 from a hardened base, 28 s at a net of 3 from a fresh one.
- The **margin** of a base for its owner = bar points the enemy needs to neutralise it (Alliance: bar minus 69; Horde: 31 minus bar), 1 to 5 for a fresh capture (the Horde's overshoot can reach 9),
  31 for a hardened one. The core keeps the bar private (finding F3, hook); until a getter exists the plan **infers** the margin from the time the base has been
  ours and the defenders seen there (`margin = min(31, 1 + net_defenders x seconds_held / 2)`, a lower bound).
- Taking a base **brings no cast, no click, no contest timer**. The players at a base **fight while they capture**: a base with 3 attackers and 3 defenders is a net of
  0 and does not move; the side with more players wins it. Stealth classes **do not count** until they leave stealth (the click on the flag also breaks it).

**Resources** (`.cpp:44-54`, `.h:286`): all held bases score every 2 s for the team that holds them; **a neutral base scores for nobody**.

| Bases held | Points per 2 s | Per second | From 0 to 1600 alone |
|------------|----------------|------------|-----------------------|
| 1 | 1 | 0.5 | 53:20 |
| 2 | 2 | 1.0 | 26:40 |
| 3 | 5 | **2.5** | **10:40** |
| 4 | 10 | **5.0** | **5:20** |

**The flag** (`.cpp:539-645`, `:762-799`, `GameObject.cpp:1937-2040`, `Player.cpp:21029`):
- One flag in the centre, spawned when the doors open. Picking it up: the module casts the 8 s capture spell at the stand (`BattleGroundTactics.cpp:5001-5035`); an enemy hit interrupts
  (`CaptureThreat`); **a dropped flag is an instant click** and anyone may take it. The pickup removes stealth and invisibility and is refused for a
  player with Recently Dropped Flag (the dropper).
- The carrier gets spell 34976: **run speed capped at 14 yd/s** and nothing else (no damage-taken aura, unlike the Warsong Gulch flag; spell rows and the Wowhead page agree). A carrier on a 60 percent mount
  (11.2 yd/s) is not slowed; whether a mount is kept on a pickup is **to check live**.
- A killed or leaving carrier **drops the flag**; it lies for **8 s**, then **returns to the centre** (`RespawnFlagAfterDrop`). Anyone takes it again within the 8 s.
- **Capture**: the carrier walks into the 3 yd trigger of a base **his team holds** (the base must be owned and under control when he arrives). It adds 75, 85, 100 or 500
  points for 1 to 4 bases held, and **the flag returns to the centre 8 s later**. A base that falls while the carrier is on the way cannot be used.
  Fel Reaver also has a 2 s distance check inside the capture tick (`:247-252`, a workaround for a covered trigger).
- No stand-still rule and no timer on the carrier: the flag can be carried as long as the carrier lives; no Focused or Brutal Assault in this build.

**Race clock** (inference): `eta(team) = (1600 - score) / rate(bases)`. A flag capture adds an instant jump; with a cycle of about 60 to 80 s (8 s cast, about 200 yd run,
8 s respawn, about 200 yd back) the flag is worth about 1.1, 1.2, 1.4 and 7.1 points a second at 1, 2, 3 and 4 bases (75, 85, 100 and 500 divided by 70 s). **Against the bases'
income of 0.5, 1.0, 2.5 and 5.0 a second**: with one base the flag doubles the income, with two it matches it, with three it is half, with four it is **more than all the bases**
(a 500 capture is a third of the match). Value of an action = its change in `rate(us) - rate(them)` plus the flag effect:

| We hold / they hold | Take a neutral base | Neutralise an enemy base (they lose at once) | Flip (neutralise and take) |
|---------------------|---------------------|-----------------------------------------------|----------------------------|
| 0 / 0 | +0.5 | - | - |
| 1 / 1 or 0 / 1 | +0.5 | 0.5 | 1.0 |
| 2 / 0 | **+1.5** (third base) | - | - |
| 2 / 1 | **+1.5** (third base) | 0.5 | **2.0** |
| 2 / 2 | - | 0.5 | 2.0 |
| 1 / 3 | - | **1.5** (their 2.5 falls to 1.0) | 2.0 |
| 3 / 0 | **+2.5** (the fourth: 5 a second and a 500 flag) | - | - |
| 3 / 1 | - | 0.5 | **3.0** (the fourth) |
| 0 / 4 | - | **2.5** (their flag falls from 500 to 100) | 3.0 |
| 4 / 0 (ours) | - | - | an enemy neutralising any of ours costs us **2.5 a second and 400 on the flag at once** |

Four decide a match: **the third base** (+1.5, and the flag from 85 to 100), **the fourth base** (+2.5, and the flag from 100 to **500**, a match in about 2 minutes), **denying the enemy's third or fourth**
and **a flag capture at three or four bases**. A 0.5 a second step (1 base to 2) does not decide anything alone.

**Graveyards and death**: see section 2. The start-room graveyard is only used when the team holds no base (no array error in this class, unlike Battle for Gilneas, section 17).

**Clocks to plan with**: bar tick 2 s; fresh base lost to one enemy in 2 s; hardening 12 to 60 s; carrier trip 14 to 29 s; dropped flag 8 s; flag respawn after a capture 8 s; flag pickup
cast 8 s; resurrection wave 30 s; buffs 180 s; walk start room to near base 34 to 61 s; opening arrival 25 to 60 s (section 5); a match is 6 to 15 minutes at three bases against one (inference).

## 4. Roles by side size

Roles are **assigned**, not rolled. One plan per team per match, held in the per-match team record, rebuilt from the battleground at most every 2 s by whichever bot asks first
(the Arathi and Gilneas pattern, `AbPlan.cpp` `Refresh`, one lock, `PvpMatchState` for the leader and the observations). A human takes a slot and is never commanded.
Jobs of the node plan (`AbLogic::Job`): **GUARD(base)**, **ASSAULT** (the opening groups and the group that takes or flips a base), **MOBILE** (reserve, floater healer), **RAIDER** (stealth). The flag adds **two
derived jobs** that are not slots: **TAKER** (the best fit of the middle group at the moment the flag is free) and **ESCORT** (the nearest two non-guard bots and the two nearest healers to a friendly carrier).

| Job | Duty |
|-----|------|
| GUARD(base) | within 25 yd of the base's flag (post ring 3 to 12 yd, **within the 70 yd circle by a wide margin**), fights what comes, stays until killed or the base is lost; **pinned defenders**: one per near base (two in the defense plan) |
| ASSAULT | squads of 3 or more at the opening; later the group that takes the best swing base; the **middle group** fights for the flag; walks together |
| MOBILE | the reserve at the base cluster nearest the fight: answers "inc" with the two nearest; the floater healer (the third healer) goes here |
| RAIDER | stealth classes only (expert): the ink pair, section 6 |
| TAKER (derived) | picks up the centre flag when it is clear (eight seconds of cast) and carries it to the best owned base, section 6 |
| ESCORT (derived) | two nearest non-guards within 12 yd of the carrier, healers heal the carrier first (the Warsong rule, 905) |

**Opening counts**. The plan is drawn once per team per match with the owner-approved weights (**60 balanced, 20 offense, 20 defense**, `BgPlan::DrawPlan`, as Warsong Gulch, Arathi Basin and Battle for Gilneas), counts per team size
with the largest-remainder rule `BgPlan::Apportion`. **A base squad is 3 or more** (the owner's capture rule): a squad of 3 takes a neutral base in 14 s, of 4 in 10 s, of 5 in 8 s. Near bases: Alliance Mage Tower (3) and Draenei Ruins (2); Horde Fel Reaver (0) and Blood Elf (1).

| Players | Balanced: squad A / squad B / middle | Offense "ink pair": squad A / squad B / middle / raiders | Defense "four corners": squad A / squad B / middle |
|---------|--------------------------------------|-----------------------------------------------------------|-----------------------------------------------------|
| 15 | 3 / 3 / 9 | 3 / 3 / 7 / 2 | 4 / 4 / 7 |
| 12 | 3 / 3 / 6 | 3 / 3 / 4 / 2 | 4 / 4 / 4 |
| 10 | 3 / 3 / 4 | 3 / 3 / 4 / 0 (raiders from 12) | 3 / 3 / 4 |
| 8 | 3 / 3 / 2 | 3 / 3 / 2 / 0 | 3 / 3 / 2 |

Idea: the guide's plan is "one defender per near base, everyone else into the middle for the teamfight". With the core's bar a **lone** defender takes 40 s and loses to one enemy, so the opening
sends a **squad of 3**, and **after the base is held and hardened one stays pinned, the rest walk on to the middle** (200 yd, 18 to 29 s). The middle group of 7 to 9 meets the squads' returners at about 80 s. At 8 players the middle starts with 2 and
**the squads' returners** (4 of 6) fill it; below 8 the match does not start. The offense plan adds the ink pair (expert, stealth classes, from 12 players: section 6); the defense plan keeps **4** at each base and a smaller middle: 2 pinned each, the plan for a team
that expects a strong middle push.

**Steady state** (after the opening; guard slots first, from the bases the team holds):

| Plan | Pinned per near base, per far base | ASSAULT / middle | RAIDER | MOBILE |
|------|------------------------------------|------------------|--------|--------|
| Balanced | 1 per near base, 0 on a far base until held | the rest | 0 | 2 (incl. floater healer) |
| Offense | 1 per near base | the rest | 2 | 1 |
| Defense | 2 per near base, 1 per far base | 1 to 3 fewer | 0 | 3 |
| **Hold state (3 bases)** | **2 per held base** (with 8 to 11 players 1) | **the middle group is the carrier and reserve** | 0 | 3 |
| **Hold state (4 bases)** | **2 per base, 3 at the two bases nearest the enemy's start** | 0 (everyone is the flag group) | 0 | 3 |

The numbers are a first table, tuned from live runs (as the Arathi and Gilneas sheets did). Scale with `Apportion`, at least 1 guard per held base.

**Assignment** (deterministic so tests can pin it; every score is a candidate list per slot, as Arathi and Gilneas): `score(bot, slot)` = class fit + tier fit + level rank + stickiness - distance cost
(`AbLogic::Assign`, `SlotScore`). The **pinned defender fit** is Gilneas' near-node fit (`DefenderFit`): rogue, mage, hunter first (Sap or Frost Nova and Blink or traps, Feign Death and Disengage), then kits with a self-heal or
an escape (druid, death knight, paladin, monk, warlock), then the rest, healers last; good and expert tiers prefer it; level beats class fit. The **TAKER fit** is the Warsong taker fit (`WsgLogic`): stealth or
mobile and sturdy (rogue, feral druid, warrior, death knight, paladin, hunter), healers and casters last; the highest levels first. Healers: the first and second go with the middle group, the third is the
**floater** (to the losing base or the carrier). Stickiness and **no random re-roll** (today `oddsToChangeRole = 1` re-rolls a role 1 percent of the time, `BattleGroundTactics.cpp:4357-4375`).

**Reassignment** on events only: a bot dies (its slot goes to the best spare near the base at once, the bot returns to it if it is still open after the respawn), a base changes owner, the regime changes, a bot joins or leaves,
the flag changes state, a human call (section 8). Never while a bot carries the flag or while it is the nearest to a base the group is mid-capture. **Casual drift** as decided for Twin Peaks: adopts a change one refresh late and about a quarter keep their own slot one more refresh.

## 5. Opening plan (gates open after the 2 minute preparation; first 60 to 90 s)

Same plan for both factions; only the base names and the 22 yd head start of the Alliance in the centre differ. Bots wait at the start-room waiting spots (`EY_WAITING_POS_*`) and mount in the last 30 s (`CheckMountStateAction.cpp`;
whether low and high level bots own a mount is to check live). The platform jump-down of the start room (`eyJumpDown`) is kept.

| Group | Alliance | Horde | First task |
|-------|----------|-------|------------|
| Squad A | Mage Tower, arrives 34 to 55 s | Fel Reaver, 38 to 61 s | walks together, **enters the 70 yd circle together** (all of the squad within 30 s of each other, wait up to 15 s at 100 yd for members, the good rule), stays in the circle until the bar says taken; the pinned defender stays, the others leave when the base has 3 net players for 30 s (hardened) or the first wave is over |
| Squad B | Draenei Ruins, arrives 38 to 61 s | Blood Elf, 36 to 57 s | the same |
| Middle group | centre flag, arrives 44 to 70 s | centre flag, 47 to 75 s | walks together to 40 yd short of the flag, counts allies and enemies within 40 yd of the stand, waits up to 15 s for members (good rule); **the nearest taker casts when no enemy within 25 yd is attacking** (core rule, `CaptureThreat`); the group fights what comes |
| Raiders (offense) | stealth to the enemy's nearer base after the centre fight starts | the same | section 6 "Ink" |

Expectation (inference): both near bases are taken at 50 to 75 s after the gates (arrival plus 14 s plus the tick); the first flag is a race of the two middle groups at 45 to 80 s; the team with two bases and the flag leads. The flag's value follows the bases held (75 with one, 85 with two), so the first capture is
worth most to the team with the second base; the first 2 minutes decide the base count (2 bases at about 1 minute, 3 bases when a squad's return wins the centre). **A group that stands against a stronger group
at the centre keeps its slot and fights; it does not wander to a far base on its own** (casual bots may; part of being beatable).

## 6. Mid-game rules

State is read from the point owners (`BattlegroundEY::GetPointState`, `GetPointOwnedByTeam`), the flag (`GetFlagState`, `GetFlagPickerGUID`, `GetDroppedFlagGUID`) and the scores. Every base is classified for a team by the **owner and the
margin** (for a team: OURS (margin known or inferred), UNDER_ASSAULT (enemy seen at it, net against us), NEUTRAL, THEIRS, CAPPING (our group is in the circle with a net edge), the same names as `AbLogic::NodeSituation`; no FLIPPED_BACK, because there is no contest memory).

**Regimes** from the race clock (`Dd = eta(them) - eta(us)` in seconds, margin 90 s with 30 s hysteresis, `AbLogic::ChooseRegime` with a new map row):

| Regime | Condition | ASSAULT / middle | GUARD / MOBILE | Idea |
|--------|-----------|------------------|----------------|------|
| OPEN | first 240 s after the gates | the opening groups | the opening groups | section 5 |
| EVEN | `-90 < Dd < 90`, no special case | per plan: win the centre, take the flag, take the best swing base | pinned guards, reserve | the 0.5 a second steps are not worth a base walk |
| **THIRD** (close out one) | we hold 2 and a third base is neutral or enemy held and our odds there are good | **the middle group's spare, the squad returners and MOBILE** (at least 3 together) | guards stay | the third base is +1.5 a second and the flag 85 to 100 |
| **FOURTH** (the ender) | we hold 3, the enemy 1 or fewer, the fourth is neutral or the enemy's last: odds there `>= 0` | **all of ASSAULT and MOBILE, at least 4 together**; the carrier runs the flag home at once | 1 guard per base on hold | a match in about 2 minutes (5 a second, 500 flag) |
| **HOLD** | we hold 3 or 4 and either FOURTH is not good (odds under 0, fewer than 4 free) or we hold all 4 | the flag group only | the hold-state table, section 4 | the digest's missing rule: no leaving a base to go back to the middle |
| PUSH | `Dd <= -90` and no third or fourth chance | plan size + 2, MOBILE joins | 1 guard at quiet bases, 2 where the enemy is near | we are losing the race: flip or deny |
| LAST-NODE | we hold at most 1 base and they hold 3 or 4 | 0 at that base | 4 guards there, the rest neutralise | zero bases is rate zero and no flag |
| PANIC | `eta(them) <= 180 s` and `eta(us) > eta(them)` | everyone except 1 guard per base | | all in on the nearest enemy base with the weakest guard, or on the enemy carrier |

**The hold state** (replaces the old camp, owner rule: no camping). At 3 bases the core pays 2.5 a second and the flag 100; the enemy's single base earns 0.5. The hold state is **the guard table, the carrier runs the flag home, the middle is not contested**:
- guards by the table (2 per held base; **3 at the base the enemy attacks**), MOBILE stages at the base cluster nearest the enemy's start room (the enemy's dead walk from there, 280 to 520 yd), never in or at the enemy start room;
- the group that carries the flag stays; the others are the reserve: **they do not return to the middle** while the hold lasts (the digest rule), unless the flag is free and the enemy's middle group is dead;
- the only options are RESTORE (re-take a base the enemy neutralised), REINFORCE, HOLD, STAGE and SUPPORT_HUMAN (the Arathi list); an enemy neutralising any base of ours gets the two nearest MOBILE bots (three when six or more are called) and the nearest guard, deadline 50 s from the first sighting;
- no bot stays within 45 yd of an enemy start-room graveyard (1103 or 1104) for more than 20 s (the Warsong and Twin Peaks dwell rule); the bases are 275 to 307 yd from them, so no post lies inside;
- the stance is called once and changes only on an event.

**Capture only with 3 or more nearby** (owner ruling; digest rule 6). A bot never starts a **take** alone: a base is taken by a group that has **at least 3 of its own within 70 yd of the cap object or that reaches 3 within 30 s**; with fewer than 3
free bots the plan holds what it has (**good rule**; casual bots go alone and are punished). Exceptions: a **pinned defender** stays at its base alone (that is a hold, not a take) and the **expert ink pair** (below) goes to an empty base in twos (20 s).
A group **leaves a base only when the bar is hardened** (3 net for 30 s, or the margin is 15 or more, with the getter) **or the enemy is not seen within 70 yd** (the base would fall to a net of one in 2 s). Under assault the group counts the net: it stays when `friends - enemies >= 1` at the circle, retreats
when `odds <= -3` (below).

**Pinned defenders** (digest rule 2). One per near base after the squad's first wave. They stand within 12 yd of the base's flag (the post ring), inside the 70 yd circle and in cover, fight what comes, call "inc" (good rule) and **never leave for the middle**; the enemy
squad that arrives sees a net of 1 against 1: **the defender's job is to hold the bar until the reserve comes, not to win the fight**.

**The flag in the middle** (the one flag, the digest's central rule, and the Warsong carrier code). The middle group's decision at the flag, from the state of the flag:

| Flag state | Event | Our group | Others | Tier |
|------------|-------|-----------|--------|------|
| A on the stand, nobody around | opening, after a return | the **nearest taker casts**, the others stand 20 yd off in a half circle facing the entries, the healers behind | squads at the bases | core |
| B we pick it up (cast done) | our pickup | **TAKER runs at once** by the carrier rule to the best owned base (`Carrier` below); ESCORT within 12 yd, healers follow at 15 yd; the rest of the middle group **leaves the middle** (the digest rule) and splits: the nearest 3 stay as the flag-stand guard until the 8 s respawn is over, the others go to the base that is attacked or the next base to take | pinned guards stay | core (taker), good (leave rule) |
| C they carry | enemy pickup | the 2 nearest attack the EFC (focus fire, crowd control), 2 cut the route to the base it needs (the **enemy-held** base nearest the EFC), the healers stay out of its reach; **a thin base of theirs on its path is neutralised first** (the margin trick below) | pinned guards at the targeted base stand fast | good |
| D flag on the ground (8 s) | a drop | the nearest 2 within 20 yd click it **at once** (instant, no cast); a dropper's team mate within 10 yd takes it; if the enemy is nearer the nearest of ours blocks | one covers | core |
| E 8 s after a capture | capture | regroup, heal, take the nearest buff, walk to the middle to arrive at the respawn; **no rest** (the flag is back in 8 s) | hold | core |
| F we hold 0 bases | no capture possible | **deny**: the nearest taker (good, expert) picks the flag up when no enemy is within 25 yd and kites toward our start side to keep the enemy from the 75 to 100 points; core bots do not pick it up | the group takes a base first | good |

**Carrier** (reuse of `WsgCarrier` and `WsgLogic::ChooseCarrier`, Utils/WsgPlanLogic.h; a map adapter in `BgFlagView` for the Netherstorm flag, aura 34976): a carrier runs home at once by the shortest safe route to a **base that stays ours until it arrives**, uses its
defensives and its speed, and fights only when it cannot run (rooted, slowed) or has been caught alone. The decision is a scored list (run 100, run near the capture 950, fight rooted 960, fight slowed 800, fight alone 600; casual bots fight on at 25 percent health)
and the helper spells are the Warsong ones (a freedom spell when rooted, a class defensive below 65, 55 or 40 percent health, a run-speed spell while chased; never Divine Shield, Ice Block or Hand of Protection). **The base choice is new** (decision D16, section 16): candidates are
the owned bases; score `200 - distance/3 + 40 x guards at the base - 50 x enemies seen within 70 yd of it - 120 (the base is about to fall: an enemy net edge at it) + 60 (a retreat toward our side)`; a **base without a guard within 70 yd** or a base
the enemy is neutralising is dropped from the list. The carrier re-chooses every 2 s (a base can fall in transit) and never turns round after crossing 60 percent of the way.

**The margin trick** (expert, a rule only this core has): a newly captured base has a margin of 1, so **one bot standing in the circle for one tick neutralises it**, and the flag cannot be captured there. Against an enemy carrier going to a base of theirs that is fresh (held under 60 s) or
guarded by 1, the nearest healthy mobile bot **goes into the circle** (stealth first, leaving stealth in the circle) to neutralise it: it loses the enemy the base income and the 75 to 500 capture. Option NEUTRALISE_FOR_FLAG in D17.

**Ink** (expert, from the digest). When the middle fight is lost (`odds <= -3`) or won with the retreat done and at least two stealth bots are alive: the pair goes to an **enemy base with no defender seen** and **stays inside the circle unstealthed** (a net of 2 takes it in 20 s; a net of 1 is 40 s and is not planned): one visible bot would be
neutralised in 2 s by one enemy. The digest's six second ink does not exist in this core (owner ruling: the core's bar stays). The pair is called in chat first (section 8). Against a hardened base (margin 15 or more) the pair only holds the circle's bar while the group comes
(the margin is in the getter), it does not try to flip.

**Retreat to the nearest own base when a local fight is lost** (a rule the guide gives, as Gilneas): the fight is measured as `odds = alive friends within 40 yd of the point - alive enemies seen within 40 yd of it`; a group is losing when `odds <= -3`, or `<= -2` with no healer alive. RETREAT (score 930) walks the survivors to the nearest held base (the dead come out at the same base's graveyard), where they
push again as one group when 4 or more are together or 25 s have passed. Casual bots never retreat. Not for a pinned defender or a carrier.

**Score and time**. No clock. Late game: the 1600 count is visible; a team that is 300 behind with fewer bases plays PUSH, a team that is ahead with 3 or 4 bases plays HOLD. **Flag at four bases: all-in.** A flip limiter as in Arathi Basin (no third neutralisation of the same base in 3 minutes unless the local odds improved by 2) keeps the groups from
spinning at one base. Stalemate: both teams at 2 and 2 with the centre split is a 1.0 a second race: the first flag capture and the first third base decide it.

**Supporting a human.** A human who stands within 70 yd of a base or calls it gets the two nearest non-guard bots; bots never order the human. A human carrier is escorted and healed.

## 7. Comp-dependent choices

- **No healer**: the pinned defender slots go to the sturdiest kits; the middle group fights near the buff (the buffs are inside the base circles); the carrier runs earlier (health below 55).
- **Two or more healers**: both with the middle group, the third floats between the guard cluster and the carrier; the first healer follows the carrier when we carry (Warsong rule).
- **Stealth classes**: RAIDER slots in the offense plan and the ink pair; a stealth class that is not a raider scouts the enemy's near base before the squads commit (expert); stealth bots do not count in a circle until they leave stealth.
- **Many casters**: the middle group with crowd control on the edge; melee on the taker and the first fight; a caster guard stays 8 to 25 yd from the flag in the circle.
- **Level spread** (85-89 and 90 alone, 35-39 and others): the highest levels take the pinned defender slots and the taker (the Arathi rule); lower levels without mounts (the module's mount level is 20) walk 55 to 75 s, so the opening times above are the **mounted** ones plus 20 s.
- **Few bots, more humans** (bots only fill): at 8 to 10 players squads are 3 and 3 and the middle starts at 2 to 4 (section 4 table); a bot never takes a base alone.
- **Guide comp** (three healers, balance druid, warlock gateway, demon hunter): not used; the gateway goes into the plan only if both ends can be placed (to check live); a **balance druid** with Wild Charge and Stampeding Roar helps the carrier (Dash is the carrier's speed spell today).

## 8. Call-outs (battleground chat)

Short lowercase lines through the bot voice (`BotVoice.h`, rows `bg_ey_<event>`, same design as Arathi Basin and Gilneas; the SQL row set is new). Humans read them; bots do not parse each other (they read the point states). Base words: `fr` (also `fel`, `reaver`), `be` (also `blood`, `bloodelf`, `tower`),
`dr` (also `draenei`, `ruins`), `mt` (also `mage`, `magetower`), and `mid` (also `middle`, `centre`) for the flag. Rate limits and the channel are the shared ones (one line per team every 6 s, at most 8 a minute per team, 20 s per bot between its own lines; expert 100 percent of events, good 70, casual 30, casual ones late or wrong on purpose).

| Event | Who says it | Example lines |
|-------|-------------|---------------|
| Plan or stance | the leader, at the gates and at every change | `balanced, 3 on mt, 3 on dr, rest mid`, `third base, all go`, `hold 3, stay on bases` |
| Opening: where a group goes | the first bot of each squad, once | `going mt`, `dr squad here`, `mid group at the flag` |
| Flag taken | the carrier or the nearest | `got flag`, `flag to mt`, `need heals on fc` |
| Flag dropped or lost | the nearest | `flag down mid`, `efc running to fr` (good, expert) |
| Enemies approaching or at our base | the nearest guard | `inc be 4`, `inc mt 2 need help` |
| Base lost or neutralised | the nearest guard | `lost fr`, `mt neutral, retake` |
| Help on the way | one responder | `omw dr` |
| Base taken | the squad | `got mt`, `dr is ours` |
| Third or fourth base | one expert bot | `need the third`, `4 bases and flag ends it` |
| Retreat | one survivor of the losing group | `mid lost, back to dr` |
| Ink | the pair, once, before it starts | `ink fr`, `inking be` |
| Stealth seen | the nearest guard (good, expert) | `stealth at mt` |
| Score | one expert bot at 800 and 1400 | `800-700, we are ahead`, `they have 3, take one` |

**Team intel only through call-outs** (owner decision 2026-11-18): the enemy counts near a base are what a friendly bot sees within **70 yd** of the base (the core's own circle: a friend inside it is a capturer too) plus the call-outs and a human's lines in the last 15 s, not the battleground's full roster (`PvpObs`; experts keep an observation 15 s). This is the Gilneas rule; the cost is one pass of friends
against enemies per refresh (15 x 15 distance tests, no new scan).

**Human calls** (BT-482). Read by the shared reader (`BgCalloutReader`; `docs/bg-sheets/twin-peaks.md` section 8): `inc <base> <n>` or `help <base>` gets one answer from the nearest MOBILE bot; `take <base>` and `<base> lost` add up to 60 points to that base's swing score among the good bots' targets;
`efc`, `fc` and `flag` are the carrier words already read on the flag maps (Warsong and Twin Peaks). The reader needs a map kind for 566 and the five words (`mid` as a place for the flag).

**Leader** (BT-481; Twin Peaks decisions): one leader per side, best tier then level then GUID hash, the incumbent stays after the gates, a leader always calls its stance, a human is never the leader. It calls the drawn plan at the gates ("balanced, <name> takes mt, <name> dr, rest mid"), the stance at every change, and the score at 800 and 1400. An expert bot within sight of a base of ours warns of an enemy group while it is still 100 to 70 yd from the circle.

## 9. Class jobs on this map

Class sheets own the spell lists; this table says what each class does for the plan. Legion tools (Demon Hunter glide, Legion talents, the priest knock) are replaced by their 5.4.8 equivalents, as the owner directed (Twin Peaks decision). Names were checked for presence in `Spell.dbc` in the earlier maps' passes (Frost Nova, Blink, Cyclone, Demonic Gateway, Stampeding Roar, Death Grip, Ring of Frost,
Freezing Trap, Sap, Cheap Shot, Dash, Vanish, Prowl, Feign Death, Camouflage, Leap of Faith, Mass Dispel, Hand of Freedom). Every tool is used only if the bot knows it.

| Class | Preferred slot | Jobs on Eye of the Storm |
|-------|----------------|--------------------------|
| Rogue | pinned defender, RAIDER, TAKER | Sap the lone attacker, Cheap Shot or Kick the carrier's chaser, Vanish to leave a lost fight, Sprint with the flag; stealth to a base and **out of stealth inside the circle** (the first choice for the lone defender, guide) |
| Mage | pinned defender, middle | Frost Nova and slows in the circle, Blink out, Polymorph a chaser, Ring of Frost at the flag stand; the lone defender's kit (guide) |
| Hunter | pinned defender, MOBILE | traps in the circle (Freezing Trap on a carrier chaser), Feign Death, Disengage and Concussive Shot for the carrier's cover; the lone defender's kit (guide) |
| Druid | TAKER (feral), healer, MOBILE | Dash and Stampeding Roar for the carrier, Prowl then in the circle, Cyclone and Entangling Roots on chasers, heals |
| Warrior | TAKER, middle | the sturdy taker: Intervene on the carrier, Charge the enemy carrier, Hamstring runners |
| Paladin | healer, TAKER, GUARD | the holy paladin heals the carrier; Hand of Freedom on a rooted carrier, Hammer of Justice on a chaser; **never Divine Shield or Hand of Protection on the carrier** |
| Priest | healer | Power Word: Shield on the carrier, Leap of Faith to pull a bot out of the middle, Mass Dispel on immunities, Psychic Scream at the base, Pain Suppression and Power Word: Barrier in the centre fight |
| Shaman | middle, healer | Earthbind and Wind Shear at the flag, Spirit Walk for the carrier, Healing Tide and Spirit Link in the centre fight |
| Warlock | MOBILE, middle | Fear at the base, Create Healthstone for the group, Demonic Gateway between the middle and a base only if both can be placed, Unbound Will |
| Monk | middle, healer | Roll to close, Paralysis on a guard, Leg Sweep, Tiger's Lust on the carrier, Detox on the carrier's root |
| Death knight | middle, TAKER | Death Grip on the enemy carrier or a capper in the circle, Chains of Ice, Death and Decay in the circle |

**Target order in a node fight** (the shared decision of 2026-11-18): expert, the healer first when we hold a numbers edge, otherwise drain the squishy damage dealers' cooldowns first; good, the healer first; casual, the lowest health. **The enemy carrier first** for every ESCORT-side bot within reach and the **capper first** for every pinned guard (the Arathi
rule `CapperGuid`: the player whose presence moves the bar).

## 10. Failure modes and what the bots do

| Failure | Cause | Required behavior |
|---------|-------|-------------------|
| Camping the enemy graveyard | the old case's Priority 7 when all four bases are held (`BattleGroundTactics.cpp:3697-3708`, `EY_GY_CAMPING_*`, `:194-195`) | hold state (section 6); dwell rule 45 yd / 20 s |
| Chase across the map | Priority 3, a 20 percent run at "enemy player target" up to 250 yd (`:3510-3521`), and "defender" chases the enemy carrier without a leash (`:3523-3545`) | removed; leash 25 yd for guards, hunt only by the carrier rule |
| Lone captures | the old case sends bots one by one to the nearest unowned base within 125 yd (`:3487-3508`); a lone bot takes 40 s and falls to one enemy | squads of 3 (the owner rule); a base is left only when hardened or when the enemy is not seen |
| Scatter at the opening | `bg role` is `urand(0, 9)`, `defendersProhab` 2 to 4 of 10, the others pick one of the unowned bases at random (`:3645-3695`) | assigned squads and pinned defenders |
| Capture area too generous | the carrier calls `HandleAreaTrigger` when within 5 yd of a random point 5 yd around the base position (`:3455-3470`), which is 1.5 to 2.9 yd from the real trigger of radius 3: **a carrier can capture from up to 13 yd away where a human needs 3 yd** | the carrier walks to within 2.5 yd of the trigger's position (data in section 2) before the call; `AtFlag` checks the 3D distance to the trigger |
| Flag capture in a dying base | the carrier targets "the nearest owned base" by distance only (`:3433-3485`) and does not re-check | route choice D16 with the re-check every 2 s |
| Everyone in the middle, bases empty | strategies "flag focus" and "front focus" (2 to 3 "defenders" of 10) | pinned defenders; hold state |
| Standing in the circle stealthed | a stealthed or invisible player does not count in the bar (`CanCaptureTowerPoint`) | guards stand in the circle unstealthed; the ink pair enters the circle visible |
| Mounted capture loops | the old code dismounts and stops before the cast and returns (`:4831-4852`) | dismount only at the flag; capture of bases needs no stop |
| Knocked or fallen off the middle platform | edges (to check live), deaths on the fall | edge distance rule 8 yd when 2 or more enemies are within 30 yd (expert), the digest rule 14; position needed |
| Flag click while the dropper's debuff runs | `Recently Dropped Flag` blocks the dropper | the carrier that dropped does not try again for 3 s; another bot takes it |
| Dead bots respawn at the wrong base | no core bug found in this class (section 17); the module's ghost handler teleports to the nearest friendly guide (`ReleaseSpiritAction.cpp:225-232`) | keep |
| A guard pulled off its flag by a buff | `useBuff` walks to a speed buff within 20 yd and to regeneration or berserk within 50 yd | buffs only inside the leash (they are 18 to 21 yd from the flag) |

## 11. Skill tiers on this map

Tiers apply in battlegrounds (BT-153); tier delays are the config values and the plan adoption lag the Warsong one (0, 1000, 3000 ms).

| Tier | Rules it plays |
|------|----------------|
| **Core** (casual and up) | walk to the assigned base; stand in the circle and fight; the carrier runs home at once and takes the nearest owned base; pick up a dropped flag; never a lone take at a base with a seen enemy; follow plan changes (late); guard within 25 yd |
| **Good** (good and expert) | the opening squad of 3 walks together and waits 15 s; the pinned defender calls "inc"; leave a base only when hardened; the third and fourth base urgency; the hold state; the retreat at `odds <= -3`; the carrier's base choice by D16; the flag-stand guard after a pickup; healer-first targeting; follow human calls and the leader's stance; deny pickup with 0 bases |
| **Expert** | the ink pair; the margin trick (neutralise a fresh base before the carrier arrives); the swing-score choice of the base and the odds count; the flip limiter; floater to the losing base; squishy-cooldown target order when not ahead; early warnings; edge avoidance at the centre |

What casual bots get wrong on purpose: react 0.4 to 0.9 s late and adopt a plan change one refresh late; walk to the nearest base without waiting for the group; leave a base after the bar flips; chase one more kill outside the leash; skip the call-out; never retreat; stand on a buff spot; about a quarter keep their slot one more refresh; capture the flag at the nearest base even when it is
about to fall. **Tier targets** (owner 2026-11-18): against an average human team expert 75, good 55, casual 35 percent; the 10 / 60 / 30 mix lands near 51 percent (inference).

## 12. Bots today (verified against the code at `main` 8df27aa), with gaps

All line numbers are in `src/strategy/bg/BattleGroundTactics.cpp` (the EY case of `selectObjective`, `:3375-3712`) unless stated; there is no `BgTacticsEY.cpp` yet: the Eye of the Storm code lives in the big case and in `atFlag` (the Battle for Gilneas case has its own file `BgTacticsBFG.cpp`).

| # | Rule or fact | Where | Status |
|---|--------------|-------|--------|
| 1 | The case picks per bot: carrier first (Priority 1), a nearby unowned base within 125 yd (2), a 20 percent run at an enemy within 250 yd (3), the "defender" duties (4), the flag strategy (5), the strategy objectives (6), the camp (7) | `:3375-3712` | **wrong**: no assignment, a map-wide chase, a camp |
| 2 | Roles: `bg role` is `urand(0, 9)` per bot at accept, a "defender" is `role < defendersProhab` (flag focus 2, front or back focus 3, balanced 4); each team's strategy is drawn uniformly of four (`EYBotStrategy`), not the 60 / 20 / 20 plan; role re-roll 1 percent per refresh | `:3394-3408`, `:1856-1858`, `:4357-4375`, `BattleGroundJoinAction.cpp:1043`, `BattleGroundTactics.h:50-56` | **wrong** (no pinned defenders, no squads) |
| 3 | No capture rule: bots go to an unowned base one at a time, and a lone bot stays in the circle or leaves after the first tick | `:3487-3508` | **missing** (the owner's rule: 3 or more) |
| 4 | Carrier: nearest owned base by distance, the capture call is `HandleAreaTrigger` within 5 yd of a random point 5 yd around the base position; no re-check; retreat spot with no base; no route choice, no escorts, no healer follow, no use of `WsgCarrier` | `:3430-3485`, `:192-193` | **wrong** (generous capture range) and **missing** (route, escorts, helpers) |
| 5 | Flag in the middle: the "defender" role chases the enemy carrier by position (leash none), supports the friendly carrier by position, picks the flag up when it is spawned; the cast needs a calm area (`CaptureThreat`) and the stop-and-cast loop at the stand | `:3523-3583`, `:4795-4850`, `:5001-5035` | partly OK (cast rule, calm area); **wrong** (no leave-the-centre rule, no flag-stand guard, no deny rule) |
| 6 | Hold state: none; with all four held the whole team walks to the enemy start room | `:3697-3708` | **wrong** (owner rule: no camping) |
| 7 | No use of the score, the base count or the race: `GetTeamScore` is not read in the EY case; no third or fourth base urgency; no retreat; no regimes | whole case | **missing** |
| 8 | Paths: authored `vPaths_EY` from each start room to two crossroads, the bases and the flag, a jump-down from the start platform | `:903-980`, `:1452-1468`, `:1955-2040` | OK as data; **to check live** (they were authored for the 3.3.5 map; this map's platforms were lowered in 5.3.0) |
| 9 | The point state is read through the core getters (`GetPointState`, `GetPointOwnedByTeam`, hook commit `94b3b4237c`) with the owner as a team id (ALLIANCE or HORDE), converted correctly | `:3381-3392` | OK; no bar getter (F3) |
| 10 | Triggers: `EyeStrategy` has the Arathi `bg check flag` and `bg use buff` triggers only; no plan tick, no `timer bg` reset, no call-outs | `BattlegroundStrategy.h:66`, `.cpp:110` | **missing** |
| 11 | Leader, call-out reader, decision layer: map kinds are Warsong Gulch, Twin Peaks, Arathi Basin and Battle for Gilneas only | `Utils/BgCalloutPolicy.h:42-63`; `AbPlan.cpp`, `WsgPlan.cpp` | **missing** for 566 |
| 12 | Intel: the case reads "enemy player target" and "enemy flag carrier" values (the whole battleground), not what the team sees | `:3510-3545` | **wrong** against the 2026-11-18 rule |
| 13 | Target choice: no preference for the carrier's chasers or a capper; no healer priority for the carrier; the shared tier target order is wired for the node plan only | `ChooseTargetActions.cpp`, `PartyMemberToHealValue.cpp` | **missing** for 566 |
| 14 | Mounting: mounts outside combat and dismounts at the flag cast and for the capture call; whether a carrier keeps a mount | `:4831-4852`, `CheckMountStateAction.cpp` | OK; **to check live** (carrier) |
| 15 | Buffs: `useBuff` walks to a speed buff within 20 yd and to regeneration or berserk within 50 yd from any state | `BattleGroundTactics.cpp` `useBuff` | OK as a stop gap; wrong for guards (leash) |
| 16 | Respawn: the core rule, no bug found | core | OK (section 17) |

## 13. Gap list and proposed changes (ordered), implementation list by size

Sizes are changed-line estimates, not measured. Ledger ids BT-570 to BT-579 hold the findings (the core ones are fork proposals). Size classes: **S** under 200, **M** 200 to 600, **L** 600 or more.

**Reuse, do not copy**: the node-map table (`BgNodeMap.h`, with the Arathi Basin and Battle for Gilneas rows) and the plan over it (`AbPlanLogic.h` rules, `AbPlan.{h,cpp}` record and glue): the race clock, `EtaSeconds`, `SwingValue`, `ChooseRegime`, `Assign`, `Apportion`, the candidate pattern, `ScoreTargets`, the responders, the flip limiter, the retreat and the ghost
rules, the seen-only intel, `DeliverCalls`, the voice rows, the leader (`BgLeaderPolicy.h`), the reader (`BgCalloutReader`), the decision lists D1, D9, D13 and D14 and `HuntCapper`, `HealPriority`; and the flag code: `WsgLogic::ChooseCarrier` and `CarrierOptions`, `WsgCarrier::Fights` and `UseHelpers`, `WsgPlan` healer follow, `BgFlagView`/`BgFlagMap::FlagView`. What does **not** fit and
needs a model switch in the table: node states (`STATE_*`, `CanClickNode`, `Classify` with the contest and `prevState`), `CAST_TIME_MS` and `CAPTURE_TIME_MS`, the click range, `IsContestedNode`, `NearerContested`, `BASE_TO_*`, a single `home` node per team (EotS has two near bases) and a hub node (EotS has the centre flag, not a node).

| # | Change | Files and functions | Size | Risk |
|---|--------|---------------------|------|------|
| E1 | **Quick fixes in the old case** (also the fallback while the plan is built): remove the 20 percent chase and the defender's map-wide chase, no camp (hold state instead), no role re-roll for 566, squads of 3 and pinned defenders by team index from the roster until the record exists, the carrier walks to within 2.5 yd of the trigger, `HandleAreaTrigger` only inside it, re-check the base every 2 s; new pure `Utils/EyPlanLogic.h`; the EY case moves to a new `strategy/bg/BgTacticsEY.cpp` (the BFG pattern) | `BattleGroundTactics.cpp:3375-3712`, `:4357-4375`, new `BgTacticsEY.cpp`, `Utils/EyPlanLogic.h`, `tests/test_ey_plan.cpp` | M (200 to 350 + tests) | low |
| E2 | **Third row of the node-map table**: `Kind::EYE`, 4 nodes, 1600, tick table `{0,2000,2000,2000,2000,0}` / `{0,1,2,5,10,0}`, the four base positions and words (`fr`, `be`, `dr`, `mt`), start rooms, regime thresholds (`closeOutNodes` 3 with an odds condition, `holdMinNodes` 3, last node 0 to 1 against 3), new fields: `captureModel` (CLICK or PRESENCE), `nearNodes[2][2]` (two near bases), `centre` point, `flagPoints[4]`, `circleRadius` 70. `AbLogic` functions take the model: PRESENCE has no cast and no contest, `Classify` from owner and inferred margin, `CanClickNode` never used, options MASS and HOLD replace TAKE and RESTORE; the 60 Arathi and the Gilneas tests are the guard | `Utils/BgNodeMap.h`, `Utils/AbPlanLogic.h`, `strategy/bg/AbPlan.{h,cpp}` (`ReadNodes` reads `BattlegroundEY::GetPointState`), `BgLeaderAdapters.h` | L (500 to 800 changed) | medium to high (shared, verified code) |
| E3 | **Eye of the Storm pure rules**: the capture table (net to seconds, margin, hardening time), the rate and eta with the flag value, swing values (section 3), regimes THIRD, FOURTH and HOLD, opening counts 8 to 15 for the three plans, guard counts and the hold table, pinned defender fit, squad size and the "3 or more" start rule, the leave-when-hardened rule, `odds` and the retreat test, the ink conditions, the margin trick, the dwell rule, option lists with scores | `Utils/EyPlanLogic.h`, `tests/test_ey_plan.cpp` | L (600 to 900 with tests) | medium |
| E4 | **Flag plan**: a `BgFlagView` adapter for `BattlegroundEY` (state, picker, dropped guid, aura 34976, the centre stand, the owned bases' triggers) so `WsgCarrier::Fights` and `UseHelpers` run for 566; `NearOwnCapture` = within 25 yd of the chosen base's trigger; new pure `EyFlagLogic` (pickup rule, carrier base choice D16, EFC response D17, the leave-the-middle rule, deny pickup, flag-stand guard, escorts and healer follow reused); the dropped-flag click rule | `strategy/bg/BgFlagView.{h,cpp}`, `WsgCarrier.cpp`, `WsgPlan.{h,cpp}` (`SurroundingsOf` for 566), `Utils/EyFlagLogic.h` (new), `tests/test_ey_flag.cpp` | L (600 to 900) | medium (touches the verified carrier code) |
| E5 | **Team record and decision glue**: record from the point owners, scores and the flag, the roster once per 2 s, `Decide`, `PlanChanged`, `DeliverCalls`, `OnTeamChat`, `Forget`; the EY case calls it and keeps E1 as the fallback; the plan draw 60 / 20 / 20 for 566; `EyeStrategy` triggers (`bg plan tick`) | `AbPlan.cpp`, `BattleGroundTactics.cpp` (`GetBotStrategyForTeam`), `BattlegroundStrategy.cpp`, `BattleGroundJoinAction.cpp` (Forget), `mod_playerbots.cpp` (chat hook) | M to L (400 to 600) | medium to high |
| E6 | **Leader, call-outs and the decision layer for 566**: `MapKind::EYE` and the base words in the reader (`mid`, `fc`, `efc`, `flag` already read), voice rows `bg_ey_*` and SQL, D1 and D9 with the base names, D13 and D14 reuse, new D15 to D17 (section 16) | `Utils/BgCalloutPolicy.h`, `BgLeaderAdapters.h`, `BgLeaderPolicy.h`, `AiPvpDecisionPolicy.h`, `AwareAi/AiPvpDecision.cpp`, `src/Mgr/Text/BotVoice.cpp`, `sql/playerbots/updates/...` | M (250 to 400) | low to medium |
| E7 | **Seen-only intel, capper and carrier hunt, healer priority, tier target order for 566**: the circle-based counts (70 yd), `HuntCapper` for the capper in the circle, `HealPriority` for the carrier, the shared target order | `AbPlan.cpp` (`CountNear`), `ChooseTargetActions.cpp`, `PartyMemberToHealValue.cpp` | S to M (100 to 200) | medium (shared code) |
| E8 | **Core, fork** (section 17): F1 clamp (BT-576), F2 stale spirit guide (BT-577), F3 bar getter (BT-578) | `BattlegroundEY.cpp:211`, `:647-693`, `BattlegroundEY.h` | 4 + 2 + 5 lines | low |
| E9 | **Live audit**: posts and circles, the start-room paths on the 5.3.0 map, the platform edge, mounts and the carrier on a mount, the flag pickup cast (8 s), the first 90 s timings, the Alliance's 23 yd head start, buffs per base, the carrier's capture range after E1 | headless runs and the scenario harness (`.botscenario <name> bg 7 <seconds> bots=...`) | 0 code unless a point fails | |

Total estimate 2,300 to 3,700 lines with tests, in four steps: **(1)** E1 and E8 (the camp, the chase, the capture range and the core quirks) can ship before the rest; **(2)** E3, E2 and E5 with tests (the plan); **(3)** E4 and E6 (the flag plan and the call-outs); **(4)** E7 and E9. E2 is the one risky step: the node-map table
gets a second capture model and the Arathi and Gilneas tests stay as the guard; if it proves too invasive the fallback is a separate `EyPlan` record that shares only the leader, the reader and the pure helpers (owner question 9).

## 14. Tests

Pure-logic unit tests in `tests/` (`bash tests/run.sh`, same style as `test_ab_plan.cpp` and `test_bfg_plan.cpp`): the rate and eta table against the core's constants (a test pins 2000 ms, 1, 2, 5, 10 points, 75, 85, 100, 500 and 1600); the capture table (net 1 to 10 to seconds, the Alliance clamp, the Horde
unclamped, hardening and neutralising times, the margin inference); swing values for every (ours, theirs) pair; the regimes with hysteresis (OPEN, EVEN, THIRD, FOURTH, HOLD, PUSH, LAST-NODE, PANIC) on score and base-count sequences; **a regression test that a team with all four bases never sends
a group to the enemy start room** and that **a base with a squad inside the circle is never "no target"**; opening counts for 8 to 15 players and the three plans (a squad is at least 3); pinned defender fit order (rogue, mage, hunter, then the kits, healers last) and tier preference; stability under a death; stealth
class takes a RAIDER slot only with two; the "3 or more" start rule and the leave-when-hardened rule; `odds` and the retreat test at the thresholds; the ink conditions (no defender seen, two stealth bots, visible inside the circle); the margin trick conditions; the carrier's base choice (the owned bases, a falling base dropped, ties,
the re-choice every 2 s); the flag-state table A to F; the dwell rule at 45 yd / 20 s; the base word parser (`fr`, `be`, `dr`, `mt`, `mid`, the aliases); the stance and base-target offers (the same list for the same input, the first option is the rule, an unknown id falls back); the seen-only enemy count within 70 yd; `NodeMap` row
data against the core's constants.

Live (lead's run, human queue):
- Headless test client (not part of this repository): a human in the queue; the bracket 35-39 and up to 55-59 has real random bots (56 at 35-59 on 2026-11-18); the scenario harness seeds both teams for the other brackets:
  `.botscenario <name> bg 7 <seconds> bots=<20 names> strat=<a>,<h> roles=index|natural` (the Twin Peaks and Gilneas form; up to 30 for 15 v 15).
- Read in the log: a `[BGEY]` plan line (guards per base, the stance, flag state, carrier, the chosen base), `[BGEY]` base-change lines (owner changes, bar margin if the getter exists), the core's messages (taken, lost, flag), world states 2749 / 2750 (resources) and 2752 / 2753 (bases), deaths per role,
  stuck bots, the carrier's route and arrival time, the time between a capture attempt and the flip, **where a ghost stands** per held set.
- Acceptance: the match finishes by 1600 within 8 to 25 minutes; both sides hold 2 or more bases at some point; every held base has 1 or more guard 90 percent of the time after minute 4 (3 or more in the circle during a first take); each enemy approach to a held base meets a responder within 45 s; no
  bot within 45 yd of an enemy start graveyard for more than 20 s; **no lone take of a base with an enemy seen**; the carrier captures at least one flag per team per match when the team holds a base; the carrier is never sent to a base that falls (checked in the log); the flag is picked up within 2 minutes of the gates by one team;
  no bot stands still 60 s alive and out of combat; fewer than 8 call-out lines a minute per team; the human team can win and lose (one win and one loss in four runs with the same human play); the third base is taken and held in at least half of the runs.
- **Bench for the tier targets** (design decision 0032): bot-versus-bot matches by tier pair, at least 20 per pair, before human tests (as the Twin Peaks and Gilneas sheets).

## 15. Owner questions (recommended option first)

Questions 5 to 8 are core findings (fork proposals, nothing in the core was changed); 1 to 4 and 9 to 12 are choices.

1. **Plan variety and the opening counts.** (a) Recommended: each team draws balanced "middle" 60 / offense "ink pair" 20 / defense "four corners" 20 once per match (the Warsong, Arathi and Gilneas weights), with the section 4 counts: squads of 3 (4 in defense) at the two near bases and a middle group. (b) Always the guide's plan (one defender per near base, everyone else in the middle): a lone defender takes 40 s, falls to one enemy and costs a base; not recommended with the core's bar. (c) Uniform draw.
2. **Pinned defenders.** (a) Recommended: one pinned defender per near base after the squad's first wave (two in the defense plan), chosen by the Gilneas kit order (rogue, mage, hunter first). (b) Two per near base in every plan (4 of 15 at home, a smaller middle). (c) Only the squad stays until the next event.
3. **The "3 or more" capture rule.** (a) Recommended: no bot starts a **take** alone (good rule; casual bots may); a pinned defender stays alone at its base; the expert ink pair goes to an empty base in twos (20 s); a group leaves a base only when the bar is hardened or no enemy is seen within 70 yd. (b) Strict: nobody ever leaves a base alone, the plan never takes a base with fewer than 3. (c) No rule, bots take what they find (today).
4. **The hold state at 3 and 4 bases.** (a) Recommended: at 3 bases the group takes the fourth only when the odds there are 0 or better and 4 or more are free (FOURTH), else it holds (2 guards per base, the flag group is the reserve, no return to the middle); at 4 bases it holds with 3 guards at the two bases nearest the enemy and the flag group goes all-in on the 500 flag. (b) Always take the fourth at 3 (a failed try costs the third). (c) Never hold, play the even plan.
5. **F1, the capture clamp (BT-576, fork fix).** The Horde side of the bar is not clamped at 5 net per tick (the Alliance side is): 8 Horde against 2 move the bar 6, 10 against none 10, so a Horde group of 7 to 10 takes a base in 4 to 6 s and the Alliance never faster than 8 s. (a) Recommended: clamp both sides to 5 (`.cpp:211`, 3 lines), as the constant's name and the owner's "8 s with 5 or more" say. (b) Leave it; the plan counts the Horde's extra speed (the capture table has it). (c) Probe live first, fix after.
6. **F2, the stale spirit guide (BT-577, fork fix).** A lost base keeps its old owner's spirit guide until someone captures it (`EventTeamLostPoint` never deletes it; Arathi Basin does). (a) Recommended: one `DelCreature(Point)` line (the Arathi form). (b) Leave it (nothing sends a player there). 
7. **F3, the bar getter (BT-578, hook).** (a) Recommended: add `BattlegroundEY::GetPointBarStatus(point)` (4 lines, the form of the two existing getters) to the hooks branch, so the plan reads the margin and sees an assault as the bar moves, instead of inferring it from seen enemies. (b) Keep the inference (works, a lower bound, blind to an assault until an enemy is seen).
8. **F4, the missing 1400 message.** The constant exists and the message and sound do not (`.cpp:261-270` is commented out and the strings are not in `Language.h`). (a) Recommended: no action (cosmetic; the plan's own score call-out at 1400 covers the human cue). (b) Add the strings and the warning (client string table, larger than a line). 
9. **Generalise the node-map table or a separate EotS record.** (a) Recommended: add the second capture model to the table (E2) and run the three maps through it, with the 60 Arathi and the Gilneas tests as the guard; the same table then serves Temple of Kotmogu's zones later. (b) A separate `EyPlan` record that shares the leader, reader and pure helpers (smaller risk, about 800 lines of parallel code). (c) E1 only for now.
10. **The flag pickup cast.** The module casts the 8 s capture spell at the centre flag (the wiki says the client does the same in this era); a dropped flag is an instant click. (a) Recommended: keep the cast, require no enemy within 25 yd, the group stands by; verify the 8 s live with a GM probe. (b) Check live first and decide after. 
11. **The ink pair.** (a) Recommended: an expert option for two stealth bots, only against a base with no defender seen, **visible inside the circle** (20 s), never against a hardened base; rare with 10 percent experts, as in Gilneas. (b) Drop it. (c) Allow it for good bots (more raids, more traded bases).
12. **How to test.** (a) Recommended: real random bots at brackets 35-39 to 55-59 (56 bots on 2026-11-18, the queue needs 8 per side with humans filling) and the seeded 85-89 bracket via the harness; E1 and E3 and the tests can be done and syntax checked before. (b) Wait for the 85 and 90 brackets. 
13. **The carrier's capture range after E1 (low).** The module's call from up to 13 yd away is wider than a human's 3 yd (section 10). (a) Recommended: tighten to 2.5 yd (humans cannot do it either; owner rule: bots never exploit gaps). (b) Keep.


**Owner answers (2026-11-18):**
- All recommended options are approved.
- The core fixes F1 (clamp the bar on both sides), F2 (delete a lost base's spirit guide) and F3 (bar getter) go in now; F1 gets a live GM check after the build.
- The bot carrier's capture range tightens to 2.5 yd.

## 16. Decision points for the decision layer (leader bot, every 15 to 30 s)

The same design as Twin Peaks section 16 and Gilneas section 16 (design decision 0032 point 9): each decision point is a pure function that turns the state into a **candidate list** with integer scores (highest first, ties by lower id); the layer, if present, answers with one candidate id; **the fallback is the
first candidate**, used when there is no answer within 2 s, the id is unknown, or the layer is off. Candidates never carry free coordinates, only ids the plan already knows (bases, groups, flag actions). One request in flight per side; asked every 20 s and at once, but not within 5 s of the last, after a base event, a wipe, a flag event, a retreat or a leader change.

State record sent (about 1 kB, from the match record the plan already keeps, no new scan): scores and etas of both sides; the four base owners with their age (seen margin or the inferred one); **seen** enemies near each base and our bots near each base (counts only); the flag state (centre, carried by whom, on the ground, seconds to its return); alive bots per group and their distance to the centre; dead per team and seconds to the next 30 s wave; the stance, the time the plan has run, the healers alive, the stealth bots alive; whether a human is on the team.

| # | Asked when | Candidates (id: meaning) | Fallback score (the rule) |
|---|------------|--------------------------|----------------------------|
| D1 Stance (existing) | every ask | `balanced`, `offense`, `defense`, `hold`, `push`, `last_node`, `panic` (`close_out` is the rule's own and never asked) | the Arathi list, `BgLeaderAdapters.h` `StanceOffer`: the rule's stance 100 and first; in even play the other two plans 60 / 50 / 50, `hold` 55 only at 3 or more bases, `push` 52 when they hold 3 or more; at the gates the first answer is the 60 / 20 / 20 draw. **One change**: `canHold = ourNodes >= 3` |
| D9 Base target (existing) | every ask while ASSAULT has a target | the up to three targetable bases with `NodeTargetOffer` (the best 100, the others their share of the best swing score) | the plan's own `ScoreTargets` with the section 3 swing values and THIRD and FOURTH first; `NodeName` and `CandidateName` get `fel_reaver`, `blood_elf`, `draenei_ruins`, `mage_tower` (ids offset after Gilneas) |
| D13 Fight response (existing, for the middle fight and a base fight) | the fight state changes, at most every 5 s, and every 20 s while it runs | `fight_on`, `retreat`, `ghost` (called ink in chat), `reinforce` | `fight_on` 100 while `odds >= -2` and a healer lives; `retreat` 100 when `odds <= -3` or `<= -2` with no healer (70 otherwise); `ghost` 110 only for an expert pair of stealth bots with an empty enemy base; `reinforce` 90 when `odds` is -1 or -2 and a MOBILE bot is within 60 s |
| D14 Floater and healers after a won fight (existing) | after a middle fight is won (the flag is ours or free and the enemy group is dead) | `stay_mid`, `to_losing_base`, `to_carrier` | `to_carrier` 100 while we carry; `to_losing_base` 100 while a base of ours is assaulted; `stay_mid` 60 |
| **D15 Flag action** (new) | the flag is on its stand or on the ground and the middle group has a chance, at most every 5 s | `take_now`, `wait_clear`, `deny_only`, `ignore` | `take_now` 100 when no enemy is within 25 yd of the stand and we hold 1 or more bases; `wait_clear` 90 when enemies are near and `odds >= 0`; `deny_only` 80 when we hold 0 bases and the enemy holds 1 or more (good, expert); `ignore` 40 |
| **D16 Carrier destination** (new) | we carry, at once and every 2 s | the owned bases (up to 4 ids), `hold_back` (wait for escorts, 40 s at most) | the section 6 score `200 - distance/3 + 40 x guards - 50 x enemies seen - 120 (about to fall) + 60 (toward our side)`; the best owned base 100 first; `hold_back` 60 only when the path is blocked and an escort is within 25 yd; a base without a guard or one the enemy is neutralising is not offered |
| **D17 Enemy carrier response** (new) | the enemy carries, at once and every 5 s | `hunt_all`, `hunt_two_cut`, `neutralize_target_base`, `ignore` | `hunt_all` 100 at 4 enemy bases (500) or within 100 yd; `hunt_two_cut` 100 at 3 or fewer bases (the nearest 2 attack, 2 cut the route); `neutralize_target_base` 110 for an expert when the target base is fresh (held under 60 s) or guarded by 1; `ignore` 30 |

The third-base action (THIRD), the fourth-base action (FOURTH), RESTORE and the hold state are never asked: each has one sensible answer. The call-out text of the leader follows the chosen stance. Casual bots drift from every answer (section 4); that is where a human can punish them. D15 to D17 are new ids after D14 of `BgLeaderPolicy.h` (`BOOK_SIZE` grows from 16 to 18).

## 17. Core and data findings (read only; nothing in the core was changed)

All are small and additive. The fork rules apply (branch `fix/...` or the hooks branch, owner's yes before any push). Graveyards, spirit guides and world states were checked item by item: **the graveyard ids, their coordinates, the spirit guide indices, the trigger positions and the point enum agree**
(Fel Reaver 0 to 1105, Blood Elf 1 to 1106, Draenei Ruins 2 to 1107, Mage Tower 3 to 1108, each graveyard within 62 to 87 yd of its flag), and **there is no start-room array error** (the class uses the main graveyard id through a switch, `.cpp:869-885`; Battle for Gilneas F1 does not recur here).
The initial world states set both resource counters and the four base icon triples (`.cpp:819-866`); a flag-state world state after a capture (2769 and 2770 stay at 2 for the capturing team) is cosmetic and **to check live** only.

**F1. The capture bar is clamped for the Alliance only (BT-576, medium, inference from reading).** `SkyFire_548/src/server/game/Battlegrounds/Zones/BattlegroundEY.cpp:211`:

```cpp
        m_PointBarStatus[point] += (m_CurrentPointPlayersCount[2 * point] - m_CurrentPointPlayersCount[2 * point + 1] < BG_EY_POINT_MAX_CAPTURERS_COUNT) ? m_CurrentPointPlayersCount[2 * point] - m_CurrentPointPlayersCount[2 * point + 1] : BG_EY_POINT_MAX_CAPTURERS_COUNT;
```

The array holds the Alliance count at `2 * point` and the Horde count at `2 * point + 1`. The difference is a positive number for the Alliance, and the test `difference < 5` clamps only the positive side: a negative difference (a Horde edge) is **added unclamped**. So 8 Horde players against 2 Alliance move the bar by 6 per tick, a Horde group of 7 to 9 takes a neutral base in 6 s and one of 10 in 4 s, while the Alliance
never takes one in less than 8 s (net 5 or more). The constant's name `BG_EY_POINT_MAX_CAPTURERS_COUNT` and the owner's ruling "8 s with 5 or more players" say the clamp is meant for both sides. **Fix**: clamp the difference to -5..5 (3 lines):

```cpp
        int32 const net = int32(m_CurrentPointPlayersCount[2 * point]) - int32(m_CurrentPointPlayersCount[2 * point + 1]);
        m_PointBarStatus[point] += std::max<int32>(-BG_EY_POINT_MAX_CAPTURERS_COUNT, std::min<int32>(net, BG_EY_POINT_MAX_CAPTURERS_COUNT));
```

Effect today (inference): a numbers edge of 6 or more is worth more to the Horde than to the Alliance, on take and on neutralise. To check live with a GM probe of 8 players of each side at a neutral base; the plan reads the capture table with this effect until the fork fix is in (a `NetClamp` constant in the pure helper).

**F2. A lost base keeps the old owner's spirit guide (BT-577, low, inference from reading).** `EventTeamLostPoint` (`.cpp:647-693`) despawns the banners and the honor trigger but never calls `DelCreature(Point)`, so the spirit guide created at the capture (`:733-735`) stays, while the Arathi Basin class deletes it when a node is lost (`BattlegroundAB.cpp:381-385`). The
next capture of the point deletes it and spawns the new owner's guide. Effect: a ghost of the old owner can still be resurrected at a base the team no longer holds; nothing sends a player there (`GetClosestGraveYard` reads only owned points), so the effect is small. **Fix**: `DelCreature(Point);` next to the existing `DelCreature(Point + 6)` (1 line).

**F3. No bar getter for modules (BT-578, low, hook).** The class has read-only getters for the owner and the state of a point (`GetPointOwnedByTeam`, `GetPointState`, `BattlegroundEY.h:361-368`, hook commit `94b3b4237c`) but not for the bar, so a module cannot see how close a base is to falling or how hardened it is. **Fix**: next to them, in the public part:

```cpp
    // Read-only capture bar of a point (0 Horde, 50 neutral start, 100 Alliance), for modules such as player bots.
    int32 GetPointBarStatus(uint8 point) const
    {
        return point < EY_POINTS_MAX ? m_PointBarStatus[point] : int32(BG_EY_PROGRESS_BAR_STATE_MIDDLE);
    }
```

Goes into the core patches (`patches/`) with a row in `docs/core-hooks.md`. Without it the plan infers the margin (section 3).

**F4. The 1400 near-victory warning does not exist (low, note).** `BG_EY_WARNING_NEAR_VICTORY_SCORE` is declared and never read; the block in `UpdateTeamScore` is commented out and the language strings and the sound constant are not defined (`BattlegroundEY.cpp:261-270`). No action proposed (owner question 8).

**Smaller notes** (no action asked): the template's `StartMaxDist` is 75 (the start-room check is on); the Fel Reaver flag has a 2 s distance workaround (`:247-252`) and the other three bases the client trigger only, so a bot that skips the trigger (the module calls `HandleAreaTrigger` directly) is not
bound by the 3 yd rule of the core (section 10, row 5); the unused `WorldSafeLocs` rows 1109 to 1133 and the two rows under the centre are dead data; the near-victory and weekend honor constants (`BG_EY_NotEYWeekendHonorTicks`) are the only users of `m_HonorTics`; a bar kept at 69 and 70 by a net-zero fight re-fires the capture and loss events at every crossing (one message and one spirit guide change each).

**Ledger**: BT-570 to BT-579 in the findings ledger (kept outside this repository).

## 18. As built (BT-580 to BT-586, 2026-11-18)

Where the code lives: the pure rules `src/Utils/EyPlanLogic.h` (map data, the capture bar, the squads and pinned defenders, the old
case) and `src/Utils/EyFlagLogic.h` (D15 to D17 and the duties); the map as data, the third row of `src/Utils/BgNodeMap.h`; the node
plan `src/Utils/AbPlanLogic.h` and `src/strategy/bg/AbPlan.{h,cpp}` (the file names stay); the map's tactics
`src/strategy/bg/BgTacticsEY.cpp`; the carrier `WsgCarrier` over `BgFlagView`; the reader words `BgCalloutPolicy.h`; the lists
`BgLeaderAdapters.h`; the chat rows `sql/playerbots/updates/db_playerbots/2026_11_19_01.sql`; the tests `tests/test_ey_plan.cpp`,
`test_ey_node_plan.cpp`, `test_ey_flag.cpp`, `test_ey_leader.cpp`.

| Gap | Ledger | Done |
|-----|--------|------|
| E1, E8 glue | BT-580, BT-581 | the old case in its own file: no chase, no camp, squads of 3 (4 in the defense plan from 12 players), one pinned defender per near base (two in the defense plan), a take group of 3 to 4 that never starts with fewer than 3 free, the hold state, one flag taker; no role re-roll; the plan draw 60 / 20 / 20; the carrier scores only inside 2.5 yd of the trigger and chooses its base again every 2 s |
| E2 | BT-582 | the third row of the node-map table with a presence capture model; the plan plays the bases (guards, regimes, responders, retreat, ghost pair, seen-only intel in the circles) |
| E3, E4 | BT-583, BT-584 | the pickup, the carrier's base and the answer to an enemy carrier as scored lists; the duty of every free bot per flag state; the Warsong carrier code runs on map 566 |
| E5, E6 | BT-585 | the reader words and voice rows; D15 to D17 published and applied |
| E7 | BT-586 | capper hunt in the circles, carrier first for the healers, enemy carrier first for good and expert bots |
| E9 | BT-579 | waiting for the live test |

**Where the code differs from the text above (choices the tests pin):**
- The old case of E1 stays as the fallback; the node plan decides whenever it has a record and a slot for the bot.
- "Leave a base only when hardened" is built as the guard count: a base whose bar margin is below 15 keeps a squad of 3 as its guards
  (`EyeGuardCount`), so the squad's bots are released by the slot assignment when the margin passes 15, and not by a rule of the bot.
- The third base has no regime of its own: the even regime's swing score already puts the third base first (+1.5 a second); the
  close-out regime is the fourth base only, and it needs the odds there at 0 or better and 10 or more bots alive (an approximation of "4 free").
- The hold state is the guard table (2 per held base, 3 where the enemy is) and an assault count of the free bots minus the reserve;
  the reserve stages at the held base nearest the enemy's start room.
- After a flag pickup the cover group is not kept at the stand: the bots staged there keep their stage while the flag waits to respawn,
  then fall back to the plan; the "3 stay for the 8 s" of the sheet is the stage duty while the flag is on its stand or waiting.
- The healers that follow the carrier are the two nearest free healers (duty HEAL_CARRIER), not the Warsong record's healer-follow; the heal
  order puts the carrier first (30 points).
- D16 can list "hold back" (wait for the escorts); the rule never picks it and an answer of it is not executed. D14 (the reserve's staging)
  is not offered for this map; D12 and D13 are as in Gilneas (D12 is not asked).
- The neutraliser (expert) walks to the base's post ring in view; there is no stealth in it.
- The leader's stance call uses the shared stance words; "balanced, <name> takes mt" is not built.
- The decision line of the carrier (`[BGWSG]`) is the two-flag plan's and is not written on this map; the evidence lines are `[BGEY]`.
- Carrier surroundings come from the plan's record (positions refreshed every 2 s), not a scan.

**To check live (the real random bots at brackets 35-59 and 20 seeded level 85 bots):** the first 90 s (squads arrive, the near bases are
taken in 14 s at a net of 3, the middle group casts the flag, the Alliance's head start); a base left only when hardened; one bot casts
the centre flag and the cover stands 26 to 30 yd off (whether that ring is on the platform: edge geometry is not read); the carrier scores
at a base that still stands and not from 13 yd away; the escorts and healers follow; a dropped flag is clicked at once; the enemy carrier
is hunted and cut off; the hold state at 3 and 4 bases and the fourth base with the odds rule; a ghost pair visible inside the circle;
the call-out reader on `fr`, `be`, `dr`, `mt`, `mid`; the `[BGEY]` and `[BGAB]` lines; the capture clamp fix of the core
with a GM probe; whether the carrier keeps a mount; the 8 s pickup cast.
