# Arathi Basin playbook

Status: **owner approved 2026-11-12, implemented in the scratch copy (BT-156), not yet compiled against the core nor verified
live** (section 16 lists what was built, where it differs from this sheet and what the live run must check). Written against `main` at 2b4b89e (the Warsong Gulch
commit): the Warsong plan, the tier gating in battlegrounds and the shared call-out pieces are already in the module, and
this sheet reuses them where it says so (section 13, "Shared with Warsong"). Queueing and the human-queue fill are not part
of this sheet.

Legend: **verified** = read in the core, the world data or our code (file and line given); **inference** = derived from
verified facts; **to check live** = needs a match with the headless client. Rules are marked core, good or expert like
the class sheets: casual bots follow core rules only and react late.

## 1. Header

| Item | Value | Source |
|------|-------|--------|
| Map id, battleground type id | 529, 3 (`BATTLEGROUND_AB`) | `battleground_template` row 3, `PvpDifficulty.dbc` |
| Team size | 15 v 15; template minimum 8 per team | `battleground_template` MinPlayersPerTeam 8, MaxPlayersPerTeam 15 |
| Brackets (5.4.8) | five levels wide: 10-14 (index 0), 15-19 (1), 20-24 (2), 25-29 (3), 30-34 (4), 35-39 (5) and so on in fives up to 80-84 (14), 85-89 (15), 90 alone (16) | `PvpDifficulty.dbc` rows for map 529, read 2026-11-12 |
| Owner's band | levels 15 to 30 = brackets 1, 2, 3 and the first level of 4 (30-34) | owner |
| Win | first team to **1600 resources**; the core warns once when a score passes 1400 | `BG_AB_MAX_TEAM_SCORE` 1600, `BG_AB_WARNING_NEAR_VICTORY_SCORE` 1400, `BattlegroundAB.h:117-121` |
| Time limit | **none**. The match ends only at 1600 (`BattlegroundAB.cpp:145-149`) or by forfeit; the 47 minute limit in `Battleground.cpp:220` is arena only | core |
| Early end | a team below 8 players for the premature-finish timer (core default 5 minutes, live config not read) ends the match; winner = more occupied nodes, then more players | `Battleground.cpp:230`, `BattlegroundAB.cpp:518-535` |
| Preparation | 2 minutes with closed gates; the clock is not used by Arathi | `BG_START_DELAY_2M` |
| Sources | core `BattlegroundAB.{h,cpp}` in the core; world DB and client DBC read-only; our tactics; the game wiki Arathi Basin and Arathi Basin strategy pages (read 2026-11-12). Wowhead did not answer (404); Icy Veins, Method, Wowpedia were not retried (403, 404, 402 the same day for Warsong). Notes: `.agents/plans/bots-bg-sheets/sources/arathi-basin.*.md` | |
| Richer source | the **server's own data** by far (rates, timers, positions, rules). The one strategy page that answered adds opening-plan names, node vocabulary and class tips; its article is old (Classic-era, edited 2019) | |
| Status | owner approved 2026-11-12 (all recommended picks); code for G1 to G10 written (BT-156), live test (G11) pending | |

Where the guides and the server differ, the server wins: the main wiki page says 1500 to win, a 6 s capture and ticks every
2 s with other numbers; this core uses 1600, an **8 s cast** (`SpellCastTimes` 8000 ms for spell 21651) and 10 points per
12/9/6/3 s (30 per second at five nodes), which equals the older strategy article.

## 2. Map and positions

Alliance holds the north-west corner (x about 1314, y about 1311), Horde the south-east (x about 684, y about 681). In game
coordinates x runs north and y runs west. Coordinates are map 529.

| Node | Banner x, y, z | Buff x, y, z | Own graveyard (id) | Banner to graveyard | Notes |
|------|----------------|--------------|--------------------|---------------------|-------|
| Stables | 1166.79, 1200.13, -56.71 | 1185.71, 1185.24, -56.36 | 895 at 1201.87, 1163.13 | 51 yd | Alliance's near node, 184 yd from its start |
| Blacksmith | 977.02, 1046.62, -44.81 | 990.75, 1008.18, -42.60 | 894 at 1016.59, 955.18 | 100 yd | centre; two bridges (guide) |
| Farm | 806.18, 874.27, -55.99 | 817.66, 843.34, -56.54 | 893 at 834.73, 784.98 | 94 yd | Horde's near node, 228 yd from its start |
| Lumber Mill | 856.14, 1148.90, 11.18 | 807.46, 1189.16, 11.92 | 897 at 772.76, 1213.11 (z 15.8) | 105 yd | **plateau**, 56 to 67 yd above Blacksmith and Farm |
| Gold Mine | 1146.92, 848.18, -110.92 | 1146.62, 816.94, -98.49 | 896 at 1211.52, 781.56 (z -82.7) | 93 yd | pit; its graveyard is on the rim, 28 yd above the banner |
| Alliance base | start loc 890: 1313.90, 1310.74, -9.01; gate 1284.60, 1281.17 | none | 898 at 1354.70, 1270.27 | | waiting spot 1286.05, 1282.50 |
| Horde base | start loc 889: 684.01, 681.22, -12.92; gate 708.09, 708.45 | none | 899 at 713.71, 638.36 | | waiting spot 702.88, 703.05 |

Sources: banners `BattlegroundAB.h:175-182`, buffs `:199-206`, spirit guides `:209-218`, graveyard ids `:196`, doors
`:185-189`, safe locations `WorldSafeLocs.dbc` ids 889-899, module waiting spots `BattleGroundTactics.cpp:132-133`.
Buff objects (speed 179871, regeneration 179904, berserk 179905): one of the three spawns at each node when the gates
open, is re-rolled after every pickup and respawns after 180 s (`BattlegroundAB.cpp:177-182`, `Battleground.cpp:1805-1825`);
which kind sits at a node is therefore random each time.

Base exit portals (area triggers 3948 at 1394.36, 1303.64 and 3949 at 627.19, 699.96) make a player leave the match
(`BattlegroundAB.cpp:208-219`): no post, camp point or random point may lie within 25 yd of them.

**Distances** (inference from banner coordinates and from the module's 15 waypoint paths, `BattleGroundTactics.cpp:456-647`,
sums of segments; the paths were recorded earlier and are not proven shortest routes). Walking at the base run speed
of 7 yd/s; mounted bots (level 20 and up, `AiPlayerbot.UseGroundMountAtMinLevel = 20`) at about 11.2 yd/s, multiply by 0.625:

| From | Stables | Blacksmith | Farm | Lumber Mill | Gold Mine |
|------|---------|------------|------|-------------|-----------|
| Alliance base | 145 yd, 21 s | 538 yd, 77 s (no direct path, via Stables) | far | 482 yd, 69 s | 462 yd, 66 s |
| Horde base | far | 459 yd, 66 s (via Farm) | 204 yd, 29 s | 518 yd, 74 s | 470 yd, 67 s |
| Stables | - | 393 yd, 56 s | 595 yd, 85 s | 366 yd, 52 s | 382 yd, 55 s |
| Blacksmith | 393 | - | 255 yd, 36 s | 368 yd, 53 s | 422 yd, 60 s |
| Farm | 595 | 255 | - | 327 yd, 47 s | 430 yd, 61 s |

Two facts follow. (1) **Gold Mine and Lumber Mill are the contested nodes**: both are 462 to 518 yd from both bases, about
the same time for either side; Stables and Farm are each side's own near node. (2) **A walk between neighbouring nodes
takes about as long as the 60 s capture timer** (36 to 61 s on foot; Stables-Blacksmith straight is 244 yd but the path is
393 yd, to check live), so on foot a reinforcement that leaves when the assault is announced arrives at the end of the
timer. Guards must stand at the node, and the reserve must sit at the node with the nearest neighbours (Blacksmith).

**Lumber Mill high ground** (verified positions, inference for the rest). Banner, buff and graveyard all stand on the
plateau (z +11 to +16); the other four banners are at z -45 to -111. Our paths reach it by two routes: the south ramp from
the Farm and Blacksmith side (waypoints 785,1019,-39 then 793,1107,5 then 812,1131,10 then 857,1146,11.5) and the east
road along y 1226 from the Stables and Alliance side (958,1227,-35 then 930,1221,-19 then 880,1193,8 then 864,1163,12). A
defender who dies at the Lumber Mill respawns on the plateau, 105 yd from the banner. There is no safe drop from the
plateau for a bot (no jump table like the Warsong ledges): a 56 to 67 yd fall is not planned. Random points drawn up to 15
yd around the banner (today's code) can lie over the edge; posts must be fixed, navmesh-checked points (to check live).

## 3. Mechanics that matter for play (all verified in `BattlegroundAB.cpp` / `.h` unless stated)

**Taking a node**
- A node is taken with a click on its banner: the module casts spell 21651, an **8 s cast** (cast time index 170 in
  `SpellMisc`, 8000 ms in `SpellCastTimes`), from within 10 yd of the banner (`:398`). The click happens when the cast ends.
  Which interrupts stop it is set in `SpellInterrupts.dbc`, not read here; the guide says instant damage stops it and roots
  or damage-over-time effects do not (**to check live**; our code already treats any attacker as an interrupt,
  `CaptureThreat`, `BattleGroundTactics.cpp:5208`).
- **Who can click** (`:412-414`): `m_Nodes == 0 || teamIndex == m_Nodes % 2`. A neutral node can be clicked by anyone; a
  contested or occupied node only by the **opposing** team of the node's contester or owner. The owner of a node, or the team
  that has it contested, cannot click it. A bot casting on its own banner wastes 8 s and changes nothing.
- Neutral to contested for the clicker (`:419-438`); the 60 s timer starts (`BG_AB_FLAG_CAPTURING_TIME`). When it ends the
  node is occupied and the score starts to count (`:52-85`).
- **Contest rules** (`:440-504`):
  - Enemy clicks a node you OCCUPY: it becomes contested for them and **you lose its resource share at once** (the score
    counts occupied nodes only, `:87-89`); they gain nothing until 60 s pass.
  - You click that node while it is contested and it was occupied before (state remembered in `m_prevNodes`): **instant
    restore**, no timer. Defending costs one 8 s cast, assaulting costs a cast plus 60 s.
  - You click a node that is contested and was never occupied (taken from neutral, or flipped back and forth): it becomes
    yours contested with a fresh 60 s timer. Neutral nodes can ping-pong until one side holds the banner area for a
    full 60 s.
- Nothing is neutral for long: after the first click a node is always occupied or contested; at rest the five nodes split
  3-2, 4-1 or 5-0.

**Resources** (`BattlegroundAB.h:192-193`, `.cpp:92-143`): only occupied nodes count.

| Nodes held | Points | Interval | Rate per second | Time from 0 to 1600 |
|------------|--------|----------|-----------------|----------------------|
| 1 | 10 | 12 s | 0.833 | 32:00 |
| 2 | 10 | 9 s | 1.111 | 24:00 |
| 3 | 10 | 6 s | 1.667 | 16:00 |
| 4 | 10 | 3 s | 3.333 | 8:00 |
| 5 | 30 | 1 s | 30.0 | 0:53 |

**Race clock** (inference, used by the plan): `eta(team) = (1600 - score) / rate(nodes held)`. A team is "ahead" when its eta
is shorter than the enemy's. The value of an action is its change in `rate(us) - rate(them)`:

| We hold / they hold | Taking one of their nodes changes (points per second) | Denying one of their nodes (assault, no gain yet) |
|---------------------|--------------------------------------------------------|---------------------------------------------------|
| 1 / 4 | +0.28 for us, -1.67 for them = **1.94** | 1.67 (100 points a minute) |
| 2 / 3 | +0.56, -0.56 = **1.11** | 0.56 (33 per minute) |
| 3 / 2 | +1.67, -0.28 = **1.94** | 0.28 (17 per minute) |
| 4 / 1 | +26.7, -0.83 = **27.5** (five nodes: the match ends in 53 s) | 0.83 (50 per minute) |
| 0 / 5 | +0.83, -26.7 = **27.5** | 26.7 (1600 a minute: the five-node rate is cut to four) |

The two numbers that matter: the **fourth node** (3 to 4 nodes doubles the rate) and the **fifth node** (ends the match). A
team that holds four must never leave its last neighbour unguarded; a team that holds one must never lose it (zero nodes is
a rate of zero and an enemy at five).

**Graveyards and death** (`:622-659`, `Battleground.cpp`): a dead player respawns, in 30 s waves, at the graveyard of the
team's **occupied** node closest (2D) to where he died; contested nodes do not count; with no occupied node, at the base
graveyard (200 to 677 yd from the nodes). Ghosts queued at a node that is lost are moved to the next closest own graveyard
(`:383`). So a defender who dies at a node he holds comes back within a 51 to 105 yd walk of it, and a team with no nodes
comes back from its base (30 to 95 s on foot from the nodes). Graveyards are 50 to 105 yd from their banners.

**Clocks to plan with**: cast 8 s; contest 60 s; banner swap 2 s; resurrection wave 30 s; buffs 180 s; walk between
neighbouring nodes 36 to 61 s on foot; opening arrival 21 to 77 s on foot (section 5); match 8 to 24 minutes.

## 4. Roles by side size

Roles are **assigned**, not rolled. One plan per team per match is held in a per-match team record that also holds the node
situations, rebuilt from the battleground at most every 2 s by whichever bot asks first (the Warsong pattern, `WsgPlan.cpp`
`Refresh`, `REFRESH_MS` 2000, one lock). A human on the team takes a slot but is never commanded.

| Role | Job |
|------|-----|
| GUARD(n) | Stands within 10 yd of node n's banner; fights what comes; restores the node if it is assaulted; leaves only for the node it guards (leash 25 yd) |
| ASSAULT | Takes or flips enemy nodes as one group of 3 to 6; walks together, the weakest-armoured bot casts last; leaves a node when its cast is done and no enemy is within 40 yd |
| MOBILE | The reserve: stages at the Blacksmith (the node with the shortest walks to the others), answers "inc" calls with the two nearest bots, replaces dead guards |
| RAIDER | One or two stealth classes: hit-and-run assaults on the enemy node with the fewest defenders, to deny points (section 6) |

Healers are not a role: one healer goes with the Blacksmith guards, one with the ASSAULT group, the rest spread over the
biggest guard groups.

**Opening groups** (15 players; rounding rule below). The plan is drawn once per team per match with the Warsong weights
(owner, 2026-11-12: 20 offense, 60 balanced, 20 defense; `WsgLogic::DrawPlan`, same enum numbers as `ABBotStrategy`):

| Plan | Home node (Stables or Farm) | Blacksmith | Gold Mine | Lumber Mill | Raiders | Reserve | Idea |
|------|------------------------------|------------|-----------|-------------|---------|---------|------|
| Balanced, "four cap" (60) | 2 | 4 | 4 | 4 | 1 | 0 | take home, Blacksmith and both contested nodes: 4 against 1 if the fights are won |
| Offense, "push" (20) | 1 | 4 | 4 | 4 | 2 | 0 | the same, with a second raider; ASSAULT grows to 5 to 6 after the caps and goes for the fourth node and the fifth |
| Defense, "hold three" (20) | 2 | 5 | 5 at the nearer of Gold Mine and Lumber Mill by path (ties by match parity) | 0 | 0 | 3 at the Blacksmith | three nodes with a strong guard; the guide calls three-and-hold a losing plan against a coordinated team, so it is the beatable one |

Scale to the team size by `round(n / 15 * count)`, keep at least 1 at home and at least 2 at each opened contested node when
the team has 10 or more; below 10 players open home, Blacksmith and the nearer contested node only, and let the reserve pair
take the fourth node once the first three are held. A raider slot without a stealth class goes to the reserve.

**Steady state** (after the opening, 15 players): guard slots come first, from the nodes the team holds:

| Plan | Guards per held node | Typical split with 4 nodes held | ASSAULT | RAIDER | MOBILE |
|------|----------------------|----------------------------------|---------|--------|--------|
| Balanced | 1 at home, 2 at the Blacksmith, Gold Mine and Lumber Mill | 7 guards | 4 | 1 | 3 |
| Offense | 1 everywhere, 2 at the Blacksmith | 5 guards | 5 | 2 | 3 |
| Defense | 2 at home, 3 at the Blacksmith and at the far node | 8 guards | 2 | 0 | 5 |

**Assignment** (deterministic, so tests can pin it; every score is a candidate list per slot so a later optional decision
layer can pick among the top ones, design decision 0014 and 0026). `score(bot, slot)` = class fit + tier fit + level rank + stickiness
- distance cost, filled in this order: GUARD slots of the nodes in danger first (Blacksmith, then the node nearest the
enemy), RAIDER, ASSAULT, then MOBILE with the rest:
- GUARD fit: healers (one per contested node), plate wearers and tanks, ranged damage with crowd control (hunter, mage,
  warlock) because a guard fights beside a channeling caster; ASSAULT fit: melee damage, burst, one healer; RAIDER fit:
  rogue, feral druid (the Warsong stealth rule, `IsStealthClass`); MOBILE fit: hybrids and ranged with mobility.
- Distance cost: closer to the node's banner beats farther (about 0.1 per yard, capped), so the bots that arrive first
  hold it; level beats class fit, as in Warsong (the bracket spans five levels).
- Stickiness: a bot keeps its slot unless an event changes it. **No random re-roll**: today a bot re-rolls its role about 1
  percent of the time at each objective refresh (`BattleGroundTactics.cpp:4371`).

**Reassignment** on these events only: a bot dies (its slot goes to the best spare near the node at once; the dead bot
returns to the slot if it is still open after respawn), a node changes state (captured, assaulted, restored, lost), the
race regime changes (section 6), a bot joins or leaves, a human call (section 8). Never while a bot is casting the capture.

## 5. Opening plan (gates open after the 2 minute preparation; first 60 to 90 s)

Same plan for both factions; the arrival times differ. Bots wait at the gate spots and mount in the last 30 s from level 20
(existing, `CheckMountStateAction.cpp:229-240`). On foot at 7 yd/s (level 15 to 19) the first column; mounted from level 20
the second:

| Group | Alliance arrives | Horde arrives | First task |
|-------|------------------|---------------|------------|
| Home node | Stables 21 s / 13 s | Farm 29 s / 18 s | 2 bots: one casts, one guards; the second walks on to the Blacksmith as reserve |
| Blacksmith | 77 s / 48 s (via Stables, no direct path) | 66 s / 41 s (via Farm) | walk together past the home node; clear the banner area, the caster waits 5 yd behind the group |
| Gold Mine | 66 s / 41 s | 67 s / 42 s | same; the pit is entered by the west or south road (path ends at 1153,888 and 1105,845) |
| Lumber Mill | 69 s / 43 s (east road) | 74 s / 46 s (south ramp) | same; the group climbs the ramp together and spreads over the plateau posts |
| Raider | stealth to the enemy home node (Farm, 677 yd from the base) | stealth to Stables (about 800 yd) | arrives after the enemy home node is occupied; denies, section 6 |

Expectation (inference): the home node is contested at about 30 to 40 s and occupied at 90 to 100 s; the three centre nodes
are contested at 75 to 85 s and occupied at 135 to 145 s, so the first resource tick comes from the home node at about
1:45 and the race really starts at about 2:30. Both teams meet at the Blacksmith, Gold Mine and Lumber Mill at the same time
(Horde about 10 s earlier at the Blacksmith by our paths, to check live). **Contact rule**: a group stops 40 yd short of its
node, counts allies and enemies within 40 yd of the banner, and (good rule) waits up to 15 s for its members (gather radius
25 yd as in Warsong); the caster begins only when no enemy within the usual capture threat radius (core rule, existing
`CaptureThreat`). If a node is held by a stronger force than the group, the group keeps its slot and fights; it does not
wander to another node on its own (casual bots may; that is part of being beatable).

## 6. Mid-game rules

State is read from `BattlegroundAB::GetNodeStatus(n)` (0..4, the core hook, commit 94b3b4237c) and
`Battleground::GetTeamScore(TeamId)`; the record notes the time each node changed state and what it was before (the previous
state is private in the core, the record keeps its own observation). Each node is classified for a team:

| Situation | Core state | What it means | Response |
|-----------|-----------|---------------|----------|
| OURS | occupied by us | scoring | GUARD slots hold it |
| UNDER ASSAULT | contested by them, it was ours | we lose its share now; an 8 s click restores it at once | **RESTORE**: nearest guard and the two nearest MOBILE bots, deadline = assault time + 50 s; kill the capper first if it is still there |
| THEIRS | occupied by them | scoring for them | candidate for ASSAULT (swing score) or RAIDER denial |
| CAPPING | contested by us | 60 s from our cast | the capping group stays at least 15 s, guards stay until it flips; leave only if the area is clear of enemies within 60 yd |
| FLIPPED BACK | contested by them, never occupied | cannot be restored at once; a click starts our own 60 s | fight at the banner, re-click when clear |
| NEUTRAL | state 0 | only before the first click | the opening group of that node |

**Regimes** from the race clock (`Dd = eta(them) - eta(us)` in seconds, margin 90 s with 30 s hysteresis so the plan does
not flap; deterministic):

| Regime | Condition | ASSAULT | GUARD / MOBILE | Idea |
|--------|-----------|---------|----------------|------|
| HOLD | `Dd >= 90` and we hold 3 or more | 2 (raiders only deny) | full guards, MOBILE at the Blacksmith | win the race by keeping what we have; no suicidal assaults |
| EVEN | `-90 < Dd < 90` | per plan (2 to 5) | per plan | take the best swing node when the local odds are good |
| PUSH | `Dd <= -90` | plan size + 2, MOBILE joins | 1 guard at quiet nodes, 2 where the enemy is near | we are losing the race: flip a node or deny |
| CLOSE-OUT | we hold 4, the fifth can be reached by ASSAULT | all of ASSAULT and MOBILE | guards stay | the fifth node ends the match in 53 s, whatever the score |
| LAST-NODE | we hold exactly 1 | 0 at that node | 4 guards there; the rest flip a node | zero nodes is rate zero and a five-node enemy |
| PANIC | `eta(them) <= 180 s` and `eta(us) > eta(them)` | everyone except 1 guard per node | | all in on the nearest enemy node with the weakest guard |

**Hold three or push four.** With the plan draw the default is the balanced four-cap. During the match: at 3-2 and ahead
(HOLD) the team does not push; behind or even it sends ASSAULT at the node with the best `value / eta` (table in section
3, eta from the walking table, minus a penalty for each enemy bot within 40 yd of the banner beyond the group's size);
at 4-1 it closes out. The guide's warning applies: a hold-three team that never pushes loses to a coordinated enemy,
so a team on the defense plan switches to PUSH when it is more than 90 s behind.

**Reinforcing a node under attack ("inc BS 3").** Triggers: (1) the node's state changes to UNDER ASSAULT; (2) a guard
sees 3 or more enemies (2 or more in the 15-19 bracket) within 50 yd of its banner. Responders: the two nearest MOBILE bots
(three when 6 or more enemies are called); never a guard of another node, never more than half of ASSAULT. The responders
do not chase beyond 40 yd of the node. After the attack ends the responders return to MOBILE staging (core rule) and
the guard posts refill.

**Stealth caps and denial (RAIDER).** Pick the enemy node with the fewest enemy bots within 40 yd of its banner that is
farthest from the enemy reserve; stealth to 30 yd; if exactly one defender is near, Sap or Cheap Shot it (expert) or choose
another node; cast the capture once the area is clear; **leave when the cast ends** (the contest runs without the raider;
denying a held node costs the owner its share at once, section 3), unless nobody is within 60 yd, in which case stay for the
60 s. Starting the cast probably breaks stealth (to check live); the sheet assumes it does.

**Late game and the score.** The core's 1400 warning is a human cue; the regimes already cover it through the eta (a team
close to 1600 has a short eta, so the trailing team reaches PANIC and the leader HOLD or CLOSE-OUT). There is no clock, so no
timeout rules; the longest realistic match is a 2-3 split near the middle (24 minutes). The only stalemate is repeated
flipping at one node; the **flip limiter** (expert rule) stops a group from re-assaulting a node that has been flipped back
twice in 3 minutes unless the local odds improved.

**Supporting a human.** A human who stands within 40 yd of a node and is casting the capture, or who calls a node (section 8),
gets the two nearest non-guard bots (RESTORE and TAKE options score +, never taken from a guard slot); a human who is
dead does not change the plan. Bots never order the human.

## 7. Comp-dependent choices

- **No healer on the team**: guard slots +1 at the Blacksmith, MOBILE sits at the Blacksmith regeneration buff when one
  spawned (24 to 63 yd from the banners, section 2).
- **Two or more healers**: one with ASSAULT, one at the Blacksmith, the rest on the biggest guard group; never more than
  two healers in ASSAULT. Healer priority: the caster of the capture first, then the guard under attack.
- **Stealth classes**: take the RAIDER slots; a stealth class that is not a raider uses stealth to scout the neighbour
  node before the group commits (expert).
- **Many ranged or casters**: put them on GUARD slots (a guard that stays 8 to 25 yd from the banner covers the caster),
  melee on ASSAULT.
- **Level spread**: highest levels take the Blacksmith guard slots and the first ASSAULT slots; the lowest take home
  and Farm or Stables posts next to a healer (wiki: do not send low levels against higher-level defenders).
- **Few bots, more humans** (bots only fill): below 10 players the opening opens three nodes (section 4); with 8 to 9
  per team it is one RAIDER at most and no separate MOBILE until three nodes are held.
- **Low brackets (15 to 19, no mounts)**: all walks are 1.6 times longer, so guards at Gold Mine and Lumber Mill are 2 each
  (not 1), MOBILE stays at the Blacksmith, and the "inc" threshold is 2 enemies. At 20 and up mounts shorten the walk; 8 s
  casts and small health pools make any hit an interrupt, so guards matter even more than in the 30s.

## 8. Call-outs (battleground chat)

Short lowercase lines through the bot voice (`BotVoice.h`, rows `bg_<event>`, same design as Warsong). Humans read them;
bots do not parse each other (bots read the node states). Node words: `st` (stables), `bs` (blacksmith), `farm`, `lm`
(lumber mill), `gm` (gold mine). Rate limits and the channel are the Warsong ones (one line per team every 6 s, at most
8 a minute per team, 20 s per bot between its own lines; expert 100 percent of events, good 70, casual 30, casual ones late
or wrong on purpose; instance chat as decided for Warsong).

| Event | Who says it | Example lines |
|-------|-------------|---------------|
| Opening: where the group goes | the first bot of each group, once | `gm group`, `going lm` |
| Enemies approaching or at our node | the nearest guard | `inc bs 3`, `inc farm 5 need help` |
| Node assaulted | the nearest guard, only if no "inc" went out in 10 s | `bs assaulted, restore` |
| Help on the way | one responder | `omw bs` |
| Node restored or flipped by us | the capper | `bs back`, `got lm` |
| Node lost | the nearest bot | `lost gm` |
| Last node / close-out | one expert bot | `we need a 4th`, `take st, 5 nodes ends it` |
| Score | one expert bot, at 1000 and 1400 | `1000-700, we are ahead`, `they have 4, flip one` |

**Human calls** (BT-482). A human's short line is read as information, never as a command, by the shared reader
(`docs/bg-sheets/twin-peaks.md` section 8 lists the rules). `inc <node> <n>` or `help <node>` (or `help` alone, read at the
sender's position) gets one answer from the nearest MOBILE bot that heeds the line (a casual bot may ignore it) and the
responders of section 6 (one when the human said "1"); `take <node>` and `<node> lost` add up to 60 points, less as the line
ages, to that node's swing score among the targets of the good bots (they no longer override the target; experts keep their
numbers, casual bots go to the nearest node); `<node> clear` says nobody is there and cancels an earlier "inc" of that node.
The reader no longer reads `attack`, which is an order.

**Leader and scouts** (BT-481, BT-483). One leader bot per side confirms the stance of the plan (balanced, offense, defense,
hold, push, last node, panic) and says it at the gates, at every change and after a handover, plus the score at 1000 and 1400
and the fifth node; an expert bot within sight of a node of ours warns of an enemy group of the "inc" size while it is still
between 60 and 50 yd from the banner.

## 9. Class jobs on this map

Class sheets own the spell lists; this table says what each class does for the plan. The brackets start at 10 and the owner's
band is 15 to 34, so most spells below exist only "if the bot knows it" (class sheet lists, level gates). Guide tips are
marked W.

| Class | Preferred slot | Jobs |
|-------|----------------|------|
| Rogue | RAIDER, ASSAULT | Stealth to a node, Sap a lone defender (W), Cheap Shot or Kick the capper (kick works on a cast, to check live), Sprint away after the click; never capture while two or more defenders are awake |
| Druid | RAIDER (feral), GUARD (bear or healer) | Prowl to a node, Entangling Roots on the assault group (does not break the capture cast, W: use it on runners); bear guard at the Blacksmith; heals the guards |
| Warrior | GUARD, ASSAULT | Charge the capper, Intercept, Hamstring runners; the guard that stands on the banner; rage from nearby adds at the Farm (W) |
| Paladin | GUARD, ASSAULT | Consecration on the banner to stall solo (W), Hammer or Stun on the capper, Hand of Freedom for a bot in a root; Divine Shield only to save the last guard |
| Priest | GUARD (healer), ASSAULT (healer) | Heal the capper and the guard under attack first, Psychic Scream (W) or Fear at the banner, Dispel on roots and slows; mind control off a cliff is expert-only and not planned |
| Shaman | GUARD, MOBILE | Earthbind and frost shock to slow an assault (W), Wind Shear on casters, totems at bridges (W), heals |
| Hunter | GUARD, MOBILE | Freezing Trap on the banner area against stealth and cappers (W), Frost Trap on a bridge (W), Concussive Shot, Disengage |
| Mage | GUARD, MOBILE | Frost Nova and slows at the banner (W), Polymorph a guard so the group can cast, Polymorph the capper (W), Blink out; Slow Fall is not used for travel |
| Warlock | GUARD, MOBILE | Fear, Howl, Demonic Circle pad out of sight (W), Voidwalker sacrifice shield when outnumbered (W) |
| Monk | ASSAULT, GUARD | Roll to gap-close, Paralysis on a guard, Leg Sweep to stop a capper; Detox |
| Death knight | MOBILE, ASSAULT | Only in brackets from 55 (class starts at 55, inference): Death Grip to pull a capper off the banner, Chains of Ice, Death and Decay on the banner |

Area denial at a banner (traps, Consecration, Death and Decay, Frost Nova) is a good rule for GUARD bots when an enemy is
within 15 yd of the banner and a friend is casting. **Interrupt the capper** is the most valuable single action: an enemy
casting spell 21651 within 40 yd of one of our banners is the first target of every GUARD bot in range (section 13, G7).

## 10. Failure modes and what the bots do

| Failure | Cause | Required behavior |
|---------|-------|-------------------|
| Nodes left empty | after a capture every non-defender walks on to the next node; the first capper leaves after its 8 s | GUARD slots per held node; the capping group stays 15 s and the guards stay until the flip |
| All bots in one fight | 3 nearest nodes picked at random; 5 percent of selections run at "the nearest enemy" up to 500 yd away; defenders all go to the nearest node | guards leash 25 yd; responders limited (section 6); no map-wide chase |
| Casts on a banner that cannot be clicked | the cast is started on the bot's own occupied or contested banner; the core ignores it (`:412-414`) after 8 s | `CanClickNode(state, team)` before the cast: neutral or opposing owner only |
| Wrong node pool | the attack pool includes the team's own contested node and excludes the enemy's (`BattleGroundTactics.cpp:3338`); the Battle for Gilneas code already fixed this | candidates are neutral, enemy occupied, enemy contested only |
| Wandering instead of guarding | objective point is a random ring up to 15 yd; arrival within 4 yd resets the objective | fixed guard posts within 10 yd of the banner |
| Lost on the Lumber Mill | random point beyond the plateau edge; two ramps; no drop shortcut | fixed plateau posts; ramp paths; live audit |
| Camping the enemy graveyard | with no valid target the code walks to a camp point near the enemy base (`:147-148`, `:3255-3290`) | no camping (owner); with every node held hold the posts and wait; hold, close-out |
| Dying alone at an enemy node | 20 percent "silly" bots pick the farthest node; raiders are not planned | only RAIDERS go alone; casual mistakes are limited to one of the rules in section 11 |
| Chain reaction on a lost node | dead bots relocate to the next graveyard (`:383`) and join whichever fight is nearest | after respawn they take the nearest open slot (reassignment) |
| Wasted exits | a bot entering an exit portal trigger leaves the match | keep posts and camp points 25 yd away from 3948 and 3949 |
| Fall and water | Gold Mine pit, Lumber Mill plateau | no jump paths; ramps only; live audit of the graveyard walks (the Battle for Gilneas run found graveyards on navmesh islands; **not yet checked for map 529**) |
| Leaving before the flip | the capture group dismisses itself while the node is still contested | guards stay until the node is occupied (good rule) |
| A buff pulls a guard away | `useBuff` walks to buffs 24 to 63 yd from a banner | buffs only within the leash, or when the node is quiet |

## 11. Skill tiers on this map

Tiers apply in battlegrounds since the Warsong commit (`AI/BotSkill.cpp:53-56`, owner 2026-11-12); tier delay values stay the
config values (expert 0 ms, good 150-400 ms, casual 400-900 ms) and the plan adoption lag is the Warsong one (0, 1000,
3000 ms, `WsgLogic::AdoptLagMs`).

| Tier | Rules it plays |
|------|----------------|
| **Core** (casual and up) | walk to the assigned node; click only a legal banner; cast when no enemy is hitting the bot; guard within 25 yd of the banner while assigned; fight enemies within 40 yd; restore a node assaulted next to it; use a buff when hurt; follow plan changes (late) |
| **Good** (good and expert) | the opening group walks together and waits 15 s for its members; the capping group stays 15 s and guards stay until the flip; answer "inc" with the two nearest reserves; healers heal the caster and the guard under attack; restore within the 50 s deadline; guards hit an enemy capper first; area denial at the banner; hold or push by the race clock regimes; follow human calls |
| **Expert** | hit-and-run denial by raiders with Sap or Kick on the defender; swing-score choice of the assault node and the odds count; close-out and last-node rules; interrupt the enemy capper by stun, silence or kick; stagger two attack groups; flip limiter; accurate call-outs of count and place |

What casual bots get wrong on purpose: react 0.4 to 0.9 s late and adopt a plan change 3 s late; chase one more kill
outside the leash; leave a node after the cast; walk to the nearest node without waiting for the group; skip the call-out;
cast while an enemy is already hitting them; stand on the buff spot.

## 12. Bots today (verified against the code at `main` 2b4b89e), with gaps

All line numbers are in `src/strategy/bg/BattleGroundTactics.cpp` unless stated.

| # | Rule or fact | Where | Status |
|---|--------------|-------|--------|
| 1 | The Arathi case of `selectObjective` has four priorities: (1) 5 percent of selections run at the "enemy player target" up to 500 yd away; (2) if no node is neutral or the enemy's, walk to the enemy base graveyard camp point; (3) 85 percent of "defenders" walk to the nearest node the team owns or the enemy contests; (4) otherwise the 3 nearest neutral, enemy or team-contested nodes are put in a pool and one is picked at random; 20 percent of picks take the farthest | `:3213-3376` (priorities `:3241`, `:3255`, `:3292`, `:3321`) | **wrong**: no assignment, no minimum guard, a map-wide chase and a graveyard camp |
| 2 | The team split: `bg role` = `urand(0, 9)` per bot at accept, 1 percent re-roll at each objective refresh in Arathi (odds `oddsToChangeRole = 1`); a "defender" is `role < defendersProhab` with 3 (balanced), 1 (offense), 6 (defense) of 10, and **2 whenever the enemy is defensive, overriding the bot's own strategy**. The team strategy is drawn uniformly 1 in 3 | `BattleGroundJoinAction.cpp:984`; `:4342-4371`; `:3225-3236`; `:1825-1827` | **wrong** (no assigned roles; override bug; Warsong now draws 20/60/20) |
| 3 | The attack pool treats the team's own contested node as a target and leaves out the enemy-contested ones: `isFriendlyContested` | `:3338-3341` | **wrong** (the core refuses the own click, `BattlegroundAB.cpp:412-414`; the Battle for Gilneas file fixed it with `BfgCanClick`, `BgTacticsBFG.cpp:105-112`) |
| 4 | `atFlag` for Arathi casts spell 21651 on any banner entry within 5.5 yd that is spawned and ready, including the bot's own occupied or contested banner; it does check for a friendly caster nearby and for an attacker (`CaptureThreat`, #754) | `:4545-4900`, `:4694-4730`, `:4855-4895` | **wrong** (no legality test); the threat check is OK |
| 5 | A bot sees a friend casting and then resets its objective, walks a new path: the group does not stay at the node it is taking | `:4694-4730` | **wrong** (no guard) |
| 6 | The objective is a random point within 15 yd of the banner (the radius is `frand(5, 15)`); arriving within 4 yd resets it | `:3365-3375`, `moveToObjective :4124-4129` | **wrong** (patrol, not a post); **to check live** at the Lumber Mill plateau edge |
| 7 | No reinforcement: nothing reacts to a node changing state except the next random selection; no restore priority; no call | whole case | **missing** |
| 8 | No use of the score, the node count or the race (`GetTeamScore` unused) | whole case | **missing** |
| 9 | The "no valid target" fallback camps the enemy base graveyard (`AB_GY_CAMPING_*`) and attacks any enemy within 500 yd | `:147-148`, `:3255-3290` | **wrong** (owner rule: no camping) |
| 10 | Paths: 15 waypoint paths for Arathi; none from either base to the Blacksmith (Alliance goes via Stables, Horde via Farm); no path between Gold Mine and Lumber Mill | `:456-647`, `vPaths_AB :1394` | **missing** (to check live whether the navmesh route is shorter) |
| 11 | Waiting spots at the gates; the start-area guard keeps bots inside 75 yd until the gates open | `:132-133`, `moveToStart :2651-2658`, `Execute :2275-2340` | OK |
| 12 | Buffs: `useBuff` walks to a speed buff within 20 yd and to regeneration or berserk within 50 yd, from any state (`needSpeed` is true outside Warsong and Twin Peaks), pulling a guard off its banner | `:5115-5171` | OK as a stop gap; **wrong** for guards |
| 13 | Mounting: mounts outside combat from level 20, dismounts before the cast | `CheckMountStateAction.cpp:229-240`, `atFlag :4878-4883` | OK |
| 14 | `ArathiStrategy` has `bg check flag`, `bg use buff` triggers only; no plan tick, no call-outs, no `timer bg` reset like Warsong | `BattlegroundStrategy.cpp:92-103` | **missing** |
| 15 | Skill tiers: now apply in battlegrounds (Warsong commit), but nothing in the Arathi case reads the tier | `AI/BotSkill.cpp:53-56` | **missing** (rules not mapped) |
| 16 | Call-outs: none for Arathi (the Warsong pieces exist: `WsgPlan::DeliverCalls`, `OnTeamChat`, rows `bg_*`) | `mod_playerbots.cpp:362` | **missing** |
| 17 | Target choice: no preference for an enemy that is capturing; no healer priority for the capper | `ChooseTargetActions.cpp`, `PartyMemberToHealValue.cpp` | **missing** |
| 18 | Death: `dead` trigger resets the objective; the bot respawns at the closest owned graveyard and selects again | `BattlegroundStrategy.cpp:61-62` | OK (natural reinforcement), but no slot |
| 19 | Graveyard walk and navmesh islands for the five node graveyards | none | **to check live** (Battle for Gilneas had two islands) |
| 20 | `docs/core-hooks.md` does not list `BattlegroundAB::GetNodeStatus` | `docs/core-hooks.md` | doc gap |

## 13. Gap list and proposed changes (ordered)

Sizes are changed-line estimates, not measured. Order is value per size and dependency.

**Shared with Warsong** (reuse, do not copy): the plan draw `WsgLogic::DrawPlan` (20/60/20, same numbers as `ABBotStrategy`);
the tier adoption lag `AdoptLagMs`; `CallLimiter`, `CallChancePercent`, `CallDelayMs`; `ParseHumanCall` with its place
words (add the five node words); the assignment pattern `Candidate` / `ScoreContext` / `Assign` / `TierFit` /
`BotSkillMath::Uses` (generalise from roles to slots); the per-match record idiom of `WsgPlan.cpp` (`Refresh` every 2 s,
one lock, `Forget(instanceId)` at `BattleGroundJoinAction.cpp:1084`); `DeliverCalls` and `BotVoice` rows; `HealPriority`
(`PartyMemberToHealValue.cpp:97`); the target hook pattern of `IsEfcHunter` (`ChooseTargetActions.cpp:85`) for the
capper hunters; `GATHER_RADIUS`, `GATHER_GIVE_UP_MS` and `OPENING_MS`. Recommended small refactor first: move the pieces that are
not Warsong-specific (`DrawPlan`, `AdoptLagMs`, the call limiter and parser, the place words) into `Utils/BgPlanCommon.h`
and let `WsgPlanLogic.h` use it, so Twin Peaks, Battle for Gilneas and Eye of the Storm share one copy.

| # | Change | Files and functions | Size | Risk |
|---|--------|---------------------|------|------|
| G1 | **Quick fixes in the old case** (also a fallback while G3 is built): `CanClickNode(state, team)` mirroring `:412-414`; attack pool = neutral, enemy occupied, enemy contested; remove the 5 percent chase and the graveyard camp; own-strategy override bug; do not cast on an illegal banner | `BattleGroundTactics.cpp` `selectObjective` AB case `:3241-3341`, `atFlag :4694-4895`; pure helper in `Utils/AbPlanLogic.h`; `tests/test_ab_plan.cpp` | 60-110 | low |
| G2 | **Shared extraction** `Utils/BgPlanCommon.h` (see above) | `Utils/WsgPlanLogic.h`, new header | 150-200 moved | low to medium (touches the verified Warsong code; tests must stay green) |
| G3 | **Arathi pure rules**: rate and eta table, swing values, node situations, regimes with hysteresis, opening counts per plan and size, guard slots, slot assignment, options with scores (RESTORE, HOLD, REINFORCE, TAKE, CLOSE_OUT, RAID, STAGE, SUPPORT_HUMAN), flip limiter | new `src/Utils/AbPlanLogic.h`; `tests/test_ab_plan.cpp` | 600-850 with tests | medium |
| G4 | **Team record and decision glue**: record built from `GetNodeStatus`, `GetTeamScore`, the battleground's players (once per 2 s), `Decide`, `PlanChanged`, `DeliverCalls`, `OnTeamChat`, `Forget`; the Arathi case calls it and keeps the old code as the fallback when the plan cannot say | new `src/strategy/bg/AbPlan.{h,cpp}`, `BgTacticsAB.cpp` (`selectObjectiveAb`, `abTick`), `BattlegroundStrategy.cpp` (`ArathiStrategy` triggers), `BattleGroundJoinAction.cpp` (Forget), `mod_playerbots.cpp` (chat hook) | 700-1,000 | medium to high |
| G5 | **Guard behavior at the banner**: fixed posts within 10 yd, leash 25 yd, stay until flip, no leaving when a friend casts, `useBuff` only inside the leash for guards | `BattleGroundTactics.cpp` `atFlag`, `moveToObjective`, `useBuff`; post table in `BgTacticsAB.cpp` | 100-180 | medium |
| G6 | **Posts and paths**: five node post tables (Lumber Mill plateau first), direct base-to-Blacksmith paths, Lumber Mill ramp check, exit-portal clearance, graveyard walk and navmesh island audit | `BattleGroundTactics.cpp` paths, `BgTacticsAB.cpp`; live capture with the headless client | 40-120 + live | medium (needs live data) |
| G7 | **Capper hunting and healer priority**: an enemy casting 21651 within 40 yd of one of our banners is the first target for guards in range; expert bots stun or kick it; healers favor the caster and the attacked guard | `ChooseTargetActions.cpp`, `PvpTriggers.cpp`, `PartyMemberToHealValue.cpp` | 100-180 | medium (shared target code with Warsong) |
| G8 | **Call-outs**: rows `bg_ab_*` (inc, assaulted, omw, back, lost, score) and SQL; node words in the parser; reuse the limiter | `src/Mgr/Text/BotVoice.cpp`, `sql/playerbots/updates/...` | 150-250 | low |
| G9 | **Raiders and denial** (stealth classes) | `AbPlanLogic.h` options, `AbPlan.cpp`, class stealth hooks already in the class strategies | 100-160 | medium (needs live data for stealth breaking) |
| G10 | **Tier gating** of each rule per section 11 (the lines are in G3/G4; this item is the wiring and the casual mistakes) | `AbPlanLogic.h` `Plays`, `AbPlan.cpp` | 60-100 | low |
| G11 | **Core hook documentation** and optionally exposing the node timer to modules (no change recommended, owner question 6) | `docs/core-hooks.md` | 5 | none |
| G12 | BT-500 to BT-503 | **Decision layer asks** (owner 2026-11-18, design decision 0032 point 9): the leader asks the sidecar every 20 s (15 to 30) and after a node event for the stance (D1: the three plans, hold, push, last node, panic; close out is never asked) and for the node the assault group takes (D9: the best three targets by share of the best swing score). The rule plan plays at once; an answer replaces it only when timely, offered, still legal and at 60 percent confidence or more, for two intervals. A decision layer's regime or plan is applied on top of the rule's own, which keeps its hysteresis. Off unless `AwareAi.Enable` and `AwareAi.Pvp.Enable` are 1 | `Utils/BgLeaderAdapters.h` (`StanceOffer`, `Apply`, `NodeTargetOffer`), `Utils/AiPvpDecisionPolicy.h`, `AwareAi/AiPvpDecision.{h,cpp}`, `AbPlan.cpp` (`LeaderTurn`, `DescribeMatch`, `ChooseTargets`) | done, unit tested and syntax checked, live check pending |

Estimated total 2,000 to 3,000 lines with tests, in four steps: (1) G1 and G2 with tests; (2) G3, G4, G10; (3) G5, G7, G8;
(4) G6, G9 and the live audit. Step 1 alone removes the worst of today's behavior (wasted casts, graveyard camp, map-wide
chase) and can ship before the rest.

## 14. Tests

Pure-logic unit tests in `tests/` (run with `bash tests/run.sh`, same style as `test_wsg_plan.cpp`): the rate and eta table
against the core's constants (a test pins 12000/9000/6000/3000/1000 ms and 10/10/10/10/30 points), the swing values; the
click legality truth table for all five node states and both teams (mirrors `:412-414`); node classification from state
and previous state; the regimes with hysteresis (HOLD, EVEN, PUSH, CLOSE-OUT, LAST-NODE, PANIC) on score and node-count
sequences; opening counts per plan for 8 to 15 players; guard slots; slot assignment (counts, stability under a death,
stealth class takes RAIDER, healer spread, tier effect); responder choice (two nearest MOBILE, never a guard); flip limiter;
the node-word parser and call limiter.

Live (lead's run, human queue):
- Headless test client (not part of this repository): `python headless.py --cmd play --queue 3 --accept --hold 1800` in a bracket of
  the owner's band, then the other faction. The scenario harness seeds both teams: `.botscenario <anchor> bg 3 <seconds>
  bots=<30 names> strat=<a>,<h> roles=index|natural` (`BotScenario.cpp` ParseBg; an even roster of 30).
- Read in the log: add an `[BGAB]` evidence line like the Battle for Gilneas `[BGBFG]` lines (node changes with the score,
  a 30 s state line with guards per node); the core's node messages; world states 1776/1777 (resources) and 1778/1779
  (occupied nodes); deaths per role; stuck bots.
- Acceptance: the match finishes by 1600 in 8 to 25 minutes; both sides hold 2 or more nodes at some point; every held node
  has 1 or more guard 90 percent of the time after minute 3; each assault of a held node meets a responder within 45 s;
  no cast on an own banner; no bot stands still 60 s alive and out of combat; nobody leaves through an exit portal or camps
  an enemy graveyard; fewer than 8 call-out lines a minute per team; the human team can win and lose (at least one win and
  one loss in four runs with the same human play); the Lumber Mill is taken and held in at least half of the runs; a node
  under assault is restored at least once per match when guards are present.

## 15. Owner questions (recommended option first)

1. **Plan variety.** (a) Recommended: each team draws balanced four-cap 60 / offense push 20 / defense hold-three 20 once per
   match, the Warsong weights. (b) Always balanced. (c) Uniform draw as today.
2. **Raiders (stealth denial).** (a) Recommended: one rogue or feral druid per team raids the enemy home node in balanced, two in
   offense, hit and run, never the whole team. (b) No raiders: stealth classes fight with the groups.
3. **Behavior at 4-1 and at five nodes.** (a) Recommended: hold the posts, close out the fifth node, no camping of the enemy
   base graveyard. (b) Keep today's camp point near the enemy base when no node is left to take.
4. **Supporting a human.** (a) Recommended: the two nearest non-guard bots join a human who is casting a capture or calls a node,
   and answer his "inc <node>"; bots never order the human. (b) Ignore humans.
5. **Guard strictness.** (a) Recommended: guards leash at 25 yd and leave only for a restore or an "inc" within reach; casual
   bots drift now and then. (b) Strict posts for all tiers (more predictable, less human).
6. **Node timer.** The core keeps the contest timer private; bots infer the 60 s from the moment they saw the state change.
   (a) Recommended: no core change. (b) Add a read-only hook for the remaining time (a core change in the fork, with the
   usual rules, for exact deadlines).
7. **Low bracket for the live run.** (a) Recommended: the bracket of the owner's test character first (15-19 has no mounts and
   is the strictest test of the walking times), then one bracket with mounts (20-24). (b) 30-34 first.
8. **Shared refactor.** (a) Recommended: move the common plan pieces into `Utils/BgPlanCommon.h` before Arathi so Twin Peaks,
   Battle for Gilneas and Eye of the Storm reuse them. (b) Copy what Arathi needs and refactor later.

## 16. As built (BT-156, 2026-11-12)

Files: pure rules `src/Utils/AbPlanLogic.h` (60 tests in `tests/test_ab_plan.cpp`), shared pieces `src/Utils/BgPlanCommon.h` (10
tests in `tests/test_bg_plan_common.cpp`; Warsong's 52 tests unchanged), the team record and glue `src/strategy/bg/AbPlan.{h,cpp}`,
the tactics part `src/strategy/bg/BgTacticsAB.cpp`, the old-case quick fixes and the hooks in `BattleGroundTactics.cpp`, the triggers
in `BattlegroundStrategy.cpp`, the chat rows `sql/playerbots/updates/db_playerbots/2026_11_12_03.sql`.

**Owner picks, as built.** Plan draw 60 balanced / 20 offense / 20 defense per team per match (`BgPlan::DrawPlan`); fixed guard posts
(a ring of 3 to 8 yd around the banner, the ground checked against the banner height so a post never lies over the Lumber Mill edge),
leash 25 yd (casual guards use a loose post up to 22 yd and the 40 yd leash, so they drift); reserve at the Blacksmith; one raider
(two in offense); close-out at four nodes and no camp near the enemy base; the two nearest free bots support a human who casts a
capture or calls a node, bots never order a human; the 60 s timer is inferred from the moment the record saw the state change.

**Where the code differs from the text above (all are choices the tests pin):**
- The LAST-NODE regime needs the enemy to hold three or more nodes. "Exactly one node" would fire in the opening, where one node each
  is normal.
- The opening groups end 240 s after the gates open (`OPENING_SLOT_MS`); after that the free assault count of section 4 rules. During
  the opening a captured node's group becomes its guards and the surplus joins the free assault group or the reserve.
- Walk times are the straight line to the banner times 1.4 (`PATH_DETOUR`), at 7 yd/s and 11.2 yd/s from level 20, not the path
  table. A restore needs the walk plus the 8 s cast to fit in what is left of the contest, and the 50 s deadline.
- Healers: the first goes to a Blacksmith guard slot, the second to the assault group, the last free one is kept for it; no more than
  two in the assault group. Level beats class fit (20 points a level), a healer spread bonus beats five levels.
- The assault target keeps its place while it is within 30 points (or 20 percent) of the best (no flip-flopping while the group walks).
- Casual mistakes built in: the 3 s plan lag, no wait for the opening group, the nearest node instead of the swing node, no regime
  play, no hold after the cast, a loose guard post, the buff spot. Not built: casting while hit, a chase beyond the leash.
- Leash: a guard returns to its post after a fight; there is no hook in the combat engine that stops a chase beyond 25 yd.
- Stagger of two attack groups (expert) is not built: one assault group of up to 6, the reserve joins in a push.

**Gap list status:** G1 done (quick fixes stand alone), G2 done (shared header), G3 done (pure rules), G4 done (record, glue, fallback
to the old case), G5 done except the combat leash, G6 partial (posts with ground check and exit clearance; direct base-to-Blacksmith
paths and the graveyard audit need live data), G7 done (capper hunt, interrupt or stun for experts, healer priority), G8 done, G9
done (stealth approach, lone defender opener; needs live data on stealth and the capture), G10 done, G11 done (`docs/core-hooks.md`).
