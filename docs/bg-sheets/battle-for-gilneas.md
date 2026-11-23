# Battle for Gilneas playbook

Status: **owner approved 2026-11-18 (answers in section 15); G1 to G4, G6 and G8 implemented on `feat/bg-gilneas-plan` (BT-550 to BT-553, unit tested and syntax checked, not yet tested live); G5 (routes) and G10 (live audit) wait for the live test.** Written 2026-11-18. Written against the module `main` at b10ba91 (the Warsong
Gulch, Arathi Basin and Twin Peaks plans, the leader, the call-out reader, the awareness layer and the decision layer are in
it) and the core at a750756f47. Battle for Gilneas is the fourth map in the owner's order (owner direction
2026-11-12, design decision 0032). This sheet reuses the Arathi Basin plan wherever the map allows (`docs/bg-sheets/arathi-basin.md`,
owner approved 2026-11-12) and spells out only what differs: three nodes instead of five, a rate table whose third step is a
match ender, one assigned near-node defender, the hold state at three nodes, the retreat rule, two routes for the Horde, and the
core findings. Queueing and the human-queue fill are not part of this sheet.

Legend: **verified** = read in the core, the world data, the client data or our code (file and line given); **inference** =
derived from verified facts; **to check live** = needs a match with level 85 bots or a human; **owner question** = a source
disagrees with the core, or a choice is open, and the owner decides (section 15). Rules are marked core, good or expert like the
class sheets: casual bots follow core rules only and react late.

## 1. Header

| Item | Value | Source |
|------|-------|--------|
| Map id, battleground type id | 761, 120 (`BATTLEGROUND_BFG`) | `battleground_template` row 120, `SharedDefines.h:4031` |
| Team size | 10 v 10; template minimum 5 per team | `battleground_template` MinPlayersPerTeam 5, MaxPlayersPerTeam 10, Weight 2 (read-only query 2026-11-18) |
| Brackets (5.4.8) | **85-89 (index 0) and 90 (index 1)**; template MinLvl 85, MaxLvl 90 | `PvpDifficulty.dbc` rows for map 761 (ids 144 and 667), read 2026-11-18 |
| Owner's band | not applicable: the map opens at 85. On 2026-11-18 the live server has no random bot at 85 or above (305 below 60, 25 at 60-84), so the map is reached last on the level path and the sheet is written ahead; the live run seeds 20 level 85 bots like Twin Peaks | read-only count of `characters.characters` for the bot accounts |
| Win | first team to **2000 resources**; the core warns once when a score passes 1800 | `GILNEAS_BG_MAX_TEAM_SCORE` 2000, `GILNEAS_BG_WARNING_NEAR_VICTORY_SCORE` 1800, `BattlegroundBFG.h:100-104`; `.cpp:142-153`, `:167-171` |
| Rates | 1 node 10 per 12 s, 2 nodes 10 per 6 s, 3 nodes 30 per 1 s (0.83, 1.67 and 30 per second). **Owner confirmed 2026-11-18**: the core's values stay | `GILNEAS_BG_TickIntervals {0,12000,6000,1000}`, `TickPoints {0,10,10,30}`, `BattlegroundBFG.h:170-171`; owner decision 2026-11-18 |
| Time limit | **none**: the match ends only at 2000 (`BattlegroundBFG.cpp:167-171`) or by forfeit. The 47 minute limit in `Battleground.cpp` is arena only | core |
| Early end | a team below 5 players for the premature-finish timer (core default 5 minutes, live config not read) ends the match; winner = more occupied nodes, then more players | `Battleground.cpp:432-457`, `BattlegroundBFG.cpp:515-532` |
| Preparation | 2 minutes with closed gates; the clock is not used by the score | `Battleground.cpp:144`, `BattlegroundBFG.cpp:175-210` |
| Capture | spell 21651, an **8 s cast** (the Arathi Basin spell; `SpellCastTimes` 8000 ms, read for the Arathi sheet), then a **60 s contest** | `BattlegroundBFG.h:97`; `BgTacticsBFG.cpp:676` |
| Sources | core `BattlegroundBFG.{h,cpp}` in the core; world DB and client DBC read-only (`WorldSafeLocs.dbc`, `PvpDifficulty.dbc`); our tactics; the 5.4.8 reference core and the 4.3.4 reference core `BattlegroundBFG`; the game wiki Battle for Gilneas (read 2026-11-18); the Skill-Capped Battle for Gilneas rated-battleground guide (7:31, paid; own words, no quotes; it describes the Legion era). Wowhead gave nothing usable (MoP Classic zone page is an empty shell, see the Twin Peaks sources note), so there is no Wowhead line. Notes: `.agents/plans/bots-bg-sheets/sources/battle-for-gilneas.*.md` | |
| Earlier tactics | none: no earlier module has a Battle for Gilneas case; the module's own file extends the Arathi Basin pattern (`BgTacticsBFG.cpp:7-10`) | read |
| Status | owner approved 2026-11-18; implemented in BT-550 to BT-553, live test pending (section 18) | |

### Where sources and the core disagree (all go to section 15)

| # | Item | This core (`home`) | Other source | Question |
|---|------|--------------------|--------------|----------|
| 1 | Score to win | 2000 | guide 1500 (Legion redesign); wiki 2000 | none: the core and the wiki agree (owner 2026-11-18) |
| 2 | Rates | 0.83, 1.67, 30 per second | guide 1, 3, 5 per second; 5.4.8 reference core 10 per 8 s and 10 per 3 s (1.25, 3.33, 30); 4.3.4 reference core as this core | none: the owner confirmed the core's values on 2026-11-18; the plan reads the node count and the live score, so a later change is a table edit |
| 3 | Where the dead respawn | the closest **owned** node's graveyard by planar distance to the death spot; the start-room graveyard only when the team owns no node | guide: a river line decides between the faction graveyard and the base graveyard | none: the core has no river line; the plan plays the core rule |
| 4 | Start-room respawn | `GILNEAS_BG_GraveyardIds[teamIndex + 5]` on a 5-entry array: **reads past the end** | 5.4.8 reference core: `teamIndex + 3` | 7 (fork fix, BT-468) |
| 5 | Spirit guide slots 1 and 2 | slot 1 (Waterworks) stands at the Mines graveyard, slot 2 (Mines) at the Waterworks graveyard | 5.4.8 reference core: slot 1 at 887.58, 937.34 (Waterworks), slot 2 at 1252.23, 836.55 (Mines) | 7 (fork fix, BT-548) |
| 6 | Team shape | no rule | guide: no tank, 3 healers, a Demon Hunter and a warlock, one solo defender | none: random teams; the role table below replaces it (the Warsong Gulch decision, Twin Peaks question 4) |
| 7 | Capture time | 8 s cast | wiki: 8 s from 2011-02-11, 7 s from 2015-01-08, 6 s from 6.2.0; build 18414 is 2014 | none: the core and the wiki agree for this build |
| 8 | Level range | template and brackets 85-90 | wiki infobox 10-80, wiki text 85 | none: the data wins |

## 2. Map and positions

All coordinates are map 761 (`BattlegroundBFG.h:154-190`, `WorldSafeLocs.dbc`). In game coordinates x runs north and y runs west.
The Alliance starts in the north-west (Gilnean Stronghold, x about 908, y about 1339), the Horde in the east (Horde Landing, x
about 1401 to 1430, y about 977). Node ids are the core's: Lighthouse 0, Waterworks 1, Mines 2. Names of places are ours.

| Node | Banner x, y, z | Buff x, y, z | Own graveyard (id) | Banner to graveyard | Notes |
|------|----------------|--------------|--------------------|---------------------|-------|
| Lighthouse (0, banner 208779) | 1057.79, 1278.29, 3.15 | 1063.57, 1313.42, 4.91 | 1736 at 1034.82, 1335.58, 12.01 | 62 yd | the Alliance's near node, 161 yd from its start; graveyard on a navmesh **island** 9 yd above the banner |
| Waterworks (1, banner 208780) | 980.04, 948.74, 12.65 | 961.83, 977.03, 14.15 | 1738 at 887.58, 937.34, 23.77 | 93 yd | the contested node in the west, 396 yd from the Alliance start and 422 yd from the Horde waiting spot; the graveyard lies 93 yd west of the banner, toward the Alliance's side of the map |
| Mines (2, banner 208781) | 1251.01, 958.27, 5.60 | 1193.09, 1017.46, 7.98 | 1735 at 1252.23, 836.55, 27.79 | 122 yd | the Horde's near node, 152 yd from the Horde waiting spot; graveyard on a navmesh **island** 22 yd above the banner; the buff is 83 yd away on the road to Waterworks |
| Alliance start room | start loc 1740: 908.27, 1338.60, 27.64 | none | 1740 | | door 918.16, 1336.75, 27.63 (object 207177); module waiting spot 908.27, 1338.60 |
| Horde start room | start loc **1799**: 1430.44, 983.86, 0.23 | none | **1739** at 1401.38, 977.12, 7.44 | | door 1396.15, 977.01, 7.43 (object 207178); module waiting spot 1401.38, 977.12 (the graveyard, 5 yd from the door) |

Sources: banners `BattlegroundBFG.h:154-159`, objects `:46-48`, `:80-93`; buffs `:185-190`; graveyard ids `:173`; doors `:162-168`;
template start locs from `battleground_template` row 120 and `WorldSafeLocs.dbc` rows 1735-1740, 1798, 1799. The script spawns every
object itself: the world DB holds no creature and no game object on map 761 (read-only count 2026-11-18). One of the three buff
objects (speed 179871, restoration 179904, berserk 179905) spawns at each node when the gates open and is re-rolled by the base
class (`BattlegroundBFG.cpp:199-204`, `m_BuffChange` at `:23`, `Battleground.cpp:1805-1825`), so which kind sits at a node is random each time.

Notes on the data:
- The Horde start location is a separate safe location (1799, 34 yd east of the door and 7 yd lower); the Alliance uses its graveyard row. Bots stand at the module's waiting spots near the doors. Not a problem.
- The template's `StartMaxDist` is 0, so the core's start-room position check (`Battleground.cpp:306-326`) is off for this map (Arathi Basin and Twin Peaks have 75). The doors are closed, so nothing escapes; the module keeps its own start-area guard. Low, owner question 13.
- The script ignores the area triggers 6447, 6448 and 6265-6269 (start rooms and buff areas); there is no exit portal in this map's script (`BattlegroundBFG.cpp:221-240`), unlike Arathi Basin, so no clearance radius applies.
- The module records a structure that its ground-level query does not see at **1207, 953, 18** (a bridge or floor on the road between the Mines and Waterworks, `BattleGroundTactics.cpp:2274`). Read as: the Horde road crosses a built structure there. To check live (section 10).

**Distances** (straight lines from the coordinates; "path" is the straight line times 1.4, the Arathi Basin detour factor, which is an
inference because the module has no recorded paths for this map and moves by navmesh search; time at 7 yd/s on foot and 14 yd/s
mounted, the 100 percent ground mount, speed of mounts at level 85 is to check live):

| From | To Lighthouse | To Waterworks | To Mines |
|------|---------------|---------------|----------|
| Alliance start (908, 1339) | 161 yd, path 225, 32 s foot / 16 s mounted | 396 yd, path 554, 79 s / 40 s | 512 yd, path 717, 102 s / 51 s |
| Horde waiting spot (1401, 977) | 457 yd, path 640, 91 s / 46 s | 422 yd, path 591, 84 s / 42 s | 152 yd, path 213, 30 s / 15 s |
| Lighthouse banner | - | 339 yd, path 475, 68 s / 34 s | 374 yd, path 524, 75 s / 37 s |
| Waterworks banner | 339 | - | 271 yd, path 379, 54 s / 27 s |

Three facts follow. (1) **Waterworks is a coin flip by distance** (396 against 422 yd). The wiki and the guide say the Alliance
arrives first because the Horde lead-ups cross water or jump (the wiki names crowd control in the water as the Horde's risk); that
is route data, not straight-line data, so it is **to check live** (section 10, owner question 12). (2) **A walk between two nodes
takes about as long as the 60 s contest** (34 to 68 s), so a reinforcement sent when an assault is announced arrives at the end of
the timer: the near node needs a defender standing at it. (3) **The enemy's near node is 512 yd (Alliance) or 457 yd (Horde)
from your start but only 152 to 161 yd from the enemy's**, so a team that holds all three nodes meets the enemy's dead where they
come out (section 3).

**Respawn geometry** (core rule, section 3): a dead player respawns in a 30 s wave at the graveyard of the **closest owned node**
(planar distance from the death spot to the graveyard), else at the start room. Graveyard to banner distances, from the table above:
Lighthouse 62 yd, Waterworks 93 yd, Mines 122 yd. Distances from each banner to the three node graveyards:

| Death at the banner of | Lighthouse graveyard (1736) | Waterworks graveyard (1738) | Mines graveyard (1735) |
|------------------------|-----------------------------|-----------------------------|------------------------|
| Lighthouse | **62** | 381 | 483 |
| Waterworks | 391 | **93** | 294 |
| Mines | 435 | 364 | **122** |

So a defender who dies at a node he holds comes back 62 to 122 yd from it in the next wave (plus the jump down from the two island
graveyards, to check live); a defender who dies at a node he does **not** hold comes back at another owned node, 294 to 483 yd away,
or in the start room.

Our static data today: the waiting spots (`BgTacticsBFG.cpp:55-56`), the two island tables and take-off and landing points
(`:145-238`) and the node state read from the spawned banner object (`:33-79`). No paths, no posts, no two routes.

## 3. Mechanics that matter for play (all verified in `BattlegroundBFG.cpp` / `.h` unless stated)

**Taking a node** (the Arathi Basin rules, three nodes)
- A node is taken with a click on its banner, within 10 yd (`:398`). The module casts spell 21651, an 8 s cast; the click lands when the cast ends. A hit on the caster probably interrupts (`CaptureThreat`, `BgTacticsBFG.cpp:660`; to check live).
- **Who can click** (`:411`): `m_Nodes == 0 || teamIndex == m_Nodes % 2`. A neutral node can be clicked by anyone; a contested or occupied node only by the **opposing** team of the node's contester or owner. The owner, or the team that has it contested, cannot click it: a cast on it wastes 8 s (the module already checks this, `BfgCanClick`, `BgTacticsBFG.cpp:108-113`).
- Neutral to contested for the clicker; a 60 s timer starts (`:416-436`, `:74-107`). When it ends the node is occupied.
- **Contest rules** (`:438-501`):
  - Enemy clicks a node you OCCUPY: it becomes contested for them, **you lose its share at once** (only occupied nodes score, `:109-112`), they gain nothing until 60 s pass (`:481-501`).
  - You click that node while it is contested and was occupied before (`m_prevNodes`): **instant restore**, no timer (`:459-477`). Defending costs one 8 s cast, assaulting costs a cast plus 60 s.
  - You click a node that is contested and was never occupied (flipped back and forth): it becomes yours contested with a fresh 60 s timer (`:441-457`).
- After each state change the new banner object appears **2 s later** (`:245-249`); the module reads the state from the spawned banner and sees "unknown" during that window (`BgTacticsBFG.cpp:37`). There is no state getter in this core (owner question 9).

**Resources** (`.h:170-171`, `.cpp:114-165`): only occupied nodes count; contested nodes count for nobody.

| Nodes held | Points | Interval | Rate per second | Time from 0 to 2000 |
|------------|--------|----------|-----------------|----------------------|
| 1 | 10 | 12 s | 0.833 | 40:00 |
| 2 | 10 | 6 s | 1.667 | 20:00 |
| 3 | 30 | 1 s | **30.0** | **1:07** |

**Race clock** (inference, as in Arathi Basin): `eta(team) = (2000 - score) / rate(nodes held)`. The value of an action is its change
in `rate(us) - rate(them)`. With three nodes the table is short: the rate steps are 0.83, 0.83 and then 28.3 per second.

| We hold / they hold (occupied) | Take a neutral node | Flip an enemy node (they lose at once, we gain after 60 s) | Deny only (assault, no gain yet) |
|--------------------------------|---------------------|-------------------------------------------------------------|----------------------------------|
| 0 or 1 / 0 | +0.83 | - | - |
| 0 or 1 / 1 | +0.83 | 0.83 + 0.83 = **1.67** | 0.83 |
| 0 or 1 / 2 | - | 0.83 + 0.83 = 1.67 | 0.83 |
| 2 / 0 | **+28.3** (three nodes) | - | - |
| 2 / 1 | - | 28.3 + 0.83 = **29.2** (three nodes, they have none) | 0.83 |
| 0 / 3 | - | 0.83 + 28.3 = **29.2** | **28.3** (their 3-node rate is cut to 2) |
| 3 / 0 (ours) | - | - | an enemy assault on any of our nodes costs us **28.3 per second at once** |

Three events decide a match: **taking the third node** (+28.3), **flipping the enemy's last node while we hold two** (+29.2) and
**denying a 3-0** (28.3 at the moment of the click). Everything else is worth 0.8 to 1.7 per second. **The score barely matters, the
node count does**: a team 1000 points behind passes the leader after about 35 s at three nodes (the enemy has none then); a 2-1
position is worth 0.83 per second, 50 points a minute. This is why the guide's map is "mostly a team fight" and why the Arathi plan's regimes carry over with different
thresholds (section 6).

**Graveyards and death** (`:617-651`, `Battleground.cpp`): a dead player respawns, in 30 s waves (`RESURRECTION_INTERVAL` 30000,
`Battleground.h:113`), at the closest owned node's graveyard, else at the start-room graveyard. The start-room case reads the
array past its end in this core (finding F1, section 17). The Alliance start graveyard is 161 yd from the Lighthouse and the Horde's
152 yd from the Mines: **a team that holds all three nodes meets the enemy's waves 150 to 160 yd from the enemy's near node**, 396
to 512 yd from the other two (inference from the coordinates and the rule; with the F1 fix).

**Clocks to plan with**: cast 8 s; contest 60 s; banner swap 2 s; resurrection wave 30 s; buffs 180 s; walk between two nodes
34 to 68 s; opening arrival 15 to 102 s (section 5); typical match 6 to 12 minutes, up to 20 minutes for a 2-1 split with no flip (inference).

## 4. Roles by side size

Roles are **assigned**, not rolled. One plan per team per match is held in a per-match team record that also holds the node
situations, rebuilt from the battleground at most every 2 s by whichever bot asks first (the Arathi pattern, `AbPlan.cpp`
`Refresh`, one lock, `PvpMatchState` for the leader and the observations). A human on the team takes a slot but is never commanded.
The jobs are Arathi Basin's: GUARD(node), ASSAULT, MOBILE and RAIDER; what changes is the table of counts.

| Role | Job |
|------|-----|
| GUARD(n) | stands within 10 yd of node n's banner (fixed post, ring 3 to 8 yd), fights what comes, restores the node if it is assaulted, leaves only for the node it guards (leash 25 yd). **GUARD(near node) with a count of one is the near-node defender** |
| ASSAULT | the Waterworks group at the opening, afterwards the group that takes or flips the best swing node; walks together, the caster waits 5 yd behind the group |
| MOBILE | the reserve and floater: stages at the hub (Waterworks) or, while we hold all three, at the node nearest the enemy's start room; answers "inc" calls with the two nearest bots; the floater healer (the third healer) goes here |
| RAIDER | stealth classes only: the ghost pair (section 6); with fewer than two stealth bots the slot goes to the Waterworks group |

**Opening counts** (the plan is drawn once per team per match with the owner-approved weights, 60 balanced, 20 offense, 20 defense
as in Warsong Gulch and Arathi Basin, `BgPlan::DrawPlan`; owner question 1 offers other picks). Near node = Lighthouse for the
Alliance, Mines for the Horde. Counts per team size, with the largest-remainder rule of `BgPlan::Apportion`:

| Players | Balanced "middle": near / Waterworks | Offense "two ghosts": near / Waterworks / raiders | Defense "hold two": near / Waterworks |
|---------|--------------------------------------|---------------------------------------------------|----------------------------------------|
| 10 | 1 / 9 | 1 / 7 / 2 | 2 / 8 |
| 9 | 1 / 8 | 1 / 6 / 2 | 2 / 7 |
| 8 | 1 / 7 | 1 / 5 / 2 | 2 / 6 |
| 7 | 1 / 6 | 1 / 5 / 1 | 2 / 5 |
| 6 | 1 / 5 | 1 / 4 / 1 | 2 / 4 |
| 5 | 1 / 4 | 1 / 4 / 0 | 1 / 4 |

Idea: the guide's plan is balanced (one defender, nine at Waterworks, a 9 v 9 fight where the winner takes the node); offense adds
two stealth raiders that go for the enemy's near node while the fight is on, so a lost fight can still be answered; defense keeps a
second bot at the near node as the reserve that answers "inc" and anchors the retreat. A raider slot without two stealth bots goes
to the Waterworks group. The Waterworks group is never below 4, the only contested place.

**Steady state** (after the opening; 10 players; guard slots first, from the nodes the team holds):

| Plan | Guards per held node (near, Waterworks, far) | Typical split | ASSAULT | RAIDER | MOBILE |
|------|----------------------------------------------|---------------|---------|--------|--------|
| Balanced | 1, 2, 1 | two held (near, Waterworks): 3 guards | 4 | 0 | 3 |
| Offense | 1, 1, 1 | two held: 2 guards | 5 | 2 | 1 |
| Defense | 2, 3, 2 | two held: 5 guards | 2 | 0 | 3 |
| **Hold state (three held), any plan** | **2, 2, 3** (the far node is the one next to the enemy's start room) | 7 guards | 0 | 0 | 3 |

The numbers are a first table, tuned from live runs (the Arathi Basin sheet did the same). Scale to the team size with
`Apportion`, with at least 1 guard per held node.

**Assignment** (deterministic, so tests can pin it; every score is a candidate list per slot, so the decision layer can pick among
the top ones; same function as Arathi Basin, `AbLogic::Assign`, `ScoreContext`, `TierFit`): `score(bot, slot)` = class fit + tier fit +
level rank + stickiness - distance cost. What is new is the **fit for the near-node defender slot**: the guide trusts only three
kits to hold a node alone, rogue (stealth, Sap, Vanish), mage (Frost Nova, Blink, Ice Block) and hunter (traps, Feign Death,
Disengage); then bots with a self-heal or an escape (druid, death knight, paladin, monk, warlock); then the rest; healers last. Tier fit
prefers good and expert bots for this slot (a lone defender who drifts loses the node), level beats class fit as everywhere.
Healers: the first goes with the Waterworks group; a second heals the same group; a third is the **floater** (moves by information:
to the node that is losing, section 6). Stickiness and **no random re-roll**: today a bot re-rolls its role 1 percent of the time
(`BattleGroundTactics.cpp:4349-4372`).

**Reassignment** on events only: a bot dies (its slot goes to the best spare near the node at once; the dead bot returns to it if it
is still open after the respawn), a node changes state, the regime changes, a bot joins or leaves, a human call (section 8).
Never while a bot is casting the capture. **Casual drift** as decided for Twin Peaks: a casual bot adopts a change one refresh late
and about a quarter keep their own slot one more refresh.

## 5. Opening plan (gates open after the 2 minute preparation; first 60 to 90 s)

Same plan for both factions except the route (section 6, "Two routes"). Bots wait at the waiting spots and mount in the last 30 s
(`CheckMountStateAction.cpp`; whether level 85 bots own a mount is to check live).

| Group | Alliance | Horde | First task |
|-------|----------|-------|------------|
| Near-node defender | Lighthouse, 16 to 32 s | Mines, 15 to 30 s | the defender casts at once if no enemy is near (the enemy reaches it in 100 s or more), then **stays** (post within 10 yd, leash 25 yd) |
| Waterworks group (8 or 9) | 40 to 79 s | 42 to 84 s | walk together; the Horde splits in two columns (section 6); stop 40 yd short, count allies and enemies within 40 yd of the banner, wait up to 15 s for members (good rule); the caster starts only when no enemy is within the capture threat radius (core rule, `CaptureThreat`) |
| Raiders (offense) | stealth to the Mines, about 100 s on foot | stealth to the Lighthouse, about 90 s | arrive after the enemy's near node is occupied; deny it by a click and leave when the cast ends (the Arathi rule) unless nobody is within 60 yd; the pair goes together (expert), a lone raider only if the node has no defender in sight |

Expectation (inference): the near node is contested at 25 to 40 s and occupied at 85 to 100 s, so the first resource tick comes at
about 1:40; the Waterworks fight is in progress at 45 to 90 s; the winner casts after the area is clear and holds the node at about 3:10;
from then the winner is at 2 nodes (1.67 per second against 0.83). **If a group stands against a stronger group at Waterworks it keeps its slot
and fights; it does not wander to another node on its own** (casual bots may; part of being beatable). A defender whose friend is
casting next to him stays at his post (today the code resets the objective and picks a new node, `BgTacticsBFG.cpp:623-647`).

## 6. Mid-game rules

State is read from the node situations (neutral, ours, under assault, theirs, capping, flipped back: the Arathi table, section 6 of
its sheet) and `Battleground::GetTeamScore`; the record notes when each node changed state and what it was before. Each node is
classified for a team exactly as in `AbLogic::Classify`.

**Regimes** from the race clock (`Dd = eta(them) - eta(us)` in seconds, margin 90 s with 30 s hysteresis, deterministic; the Arathi
names, three-node thresholds):

| Regime | Condition | ASSAULT | GUARD / MOBILE | Idea |
|--------|-----------|---------|----------------|------|
| OPEN | first 240 s after the gates | the opening groups | the opening groups | section 5 |
| EVEN | `-90 < Dd < 90`, no special case | per plan | per plan | take the best swing node when the local odds are good |
| **THIRD** (close out) | we hold two nodes and the third can be clicked (neutral, enemy occupied, enemy contested) | **all of ASSAULT and MOBILE**, guards stay | 1 guard per held node | **the third node is worth 28 per second and ends the match in about a minute** (third-node urgency) |
| **HOLD** | **we hold all three** | 0 | the hold-state table, section 4: 2, 2, 3 guards and 3 MOBILE | nothing to take; the enemy comes in waves from its start room; no leaving, no camping |
| PUSH | `Dd <= -90` and no third-node chance | plan size + 2, MOBILE joins | 1 guard at quiet nodes, 2 where the enemy is near | we are losing the race: flip a node or deny |
| LAST-NODE | we hold at most one node and they hold two or three | 0 at that node | 4 guards there if we hold one; the rest flip | zero nodes is rate zero |
| PANIC | `eta(them) <= 180 s` and `eta(us) > eta(them)` | everyone except 1 guard per node | | all in on the nearest enemy node with the weakest guard |

THIRD and HOLD replace Arathi Basin's CLOSE-OUT and HOLD: with three nodes the "fifth node" is the third and the 3-0 position is the
hold state. A team at 2 nodes ahead on the clock does **not** sit still (the Arathi HOLD with three of five nodes): the third node is
the objective at every time and score. A plan that holds two nodes and stops pursuing the third is the 20-minute game.

**The hold state** (replaces the camp, owner rule: no camping; Arathi Basin question 3). When we hold all three, `selectObjective`
finds no neutral, enemy-occupied or enemy-contested node. Today that case sends everyone to the enemy start room
(`BgTacticsBFG.cpp:395-428`). It also sends **every bot but the capper away while we are still capturing the third node**, because a
friendly-contested node is neither in the attack pool nor "ours": the 60 s the third node is contested are exactly the seconds in
which the enemy's counter-assault decides the match (section 12, row 3). The hold state does this instead:
- the capping group stays until the node is occupied (good rule), guards stay at their posts;
- guards by the table (2, 2, 3), MOBILE staged at the far node's approach (the node nearest the enemy's start room), never at the enemy's start room;
- the only options are RESTORE, REINFORCE, HOLD, STAGE and SUPPORT_HUMAN (the Arathi option list); an enemy assault on any of our nodes gets the two nearest MOBILE bots (three when six or more enemies are called) and the nearest guard, with the 50 s deadline of the Arathi sheet;
- no bot stays within 45 yd of an enemy start-room graveyard (1739 or 1740) for more than 20 s (the dwell rule of the Warsong Gulch and Twin Peaks sheets): the far node itself is 152 to 161 yd from it and is a post, not a camp;
- the stance is called once and changes only on an event.

**Third-node urgency** (a rule the guide gives). At 2 nodes held the plan scores the third node first: `SCORE_CLOSE_OUT` (940,
above TAKE 760, below RESTORE 960) for every ASSAULT and MOBILE bot within a path of 90 s, the Arathi rule with the three-node table.
The cast starts only when the area is clear (core); the group stays through the 60 s (good); the next-nearest guards of the two held
nodes do not leave. At 2 against 1 the enemy's only node is the target; at 2 against 0 with the third neutral it is the third node.

**Retreat to the hub when a local fight is lost** (a rule the guide gives, adapted to this core). The fight at a node is measured
as `odds = alive friends within 40 yd of the point - alive enemies seen within 40 yd of it` (enemies only as seen, section 8). A
group is **losing** when `odds <= -3`, or `odds <= -2` with no healer alive among the friends (constants `RETREAT_ODDS` and
`RETREAT_ODDS_NO_HEALER`, tuned live). The response is the option RETREAT (score 930, between THIRD at 940 and REINFORCE at 900, a good rule): the
survivors leave the fight and walk to the **rally node**, the nearest node we hold (the near node, or the hub if we hold Waterworks
and the group was at the far node); they stand within 25 yd of its banner, where the dead respawn in the next 30 s wave (62 to 122 yd
away, section 2), and push again **as one group** when 4 or more are together or 25 s passed. Reason in the core's rules: a dead
bot respawns at the nearest owned graveyard, so dying alone at an enemy node means a walk of 300 to 500 yd, while a retreat to the
own node turns the dead into reinforcements. Casual bots never retreat (they fight to the death, on purpose); the retreat does not
apply to a capping group within 15 s of its flip or to a guard at a held node (a guard restores or dies at its post).

**Ghost** (expert, from the guide). When the Waterworks fight is going badly (the same `odds` test), at least two stealth bots
(rogue, feral druid; a hunter with Feign Death and Camouflage; mage with Invisibility is not planned) are alive, the enemy's near node
has at most one defender seen, and the retreat has not started: the near-node defender and one fighter leave unseen for the enemy's
near node, **both start the cast** so that one interruption does not stop it (a second click that lands after the first is ignored by the core,
`BattlegroundBFG.cpp:411`), and the group slows runners and peels. Only offered with two stealth bots; with one, that bot joins the near node's defender. Called in chat before it starts
(section 8). In 5.4.8 stealth breaks when the capture cast starts (to check live), so the pair stays unseen until the click.

**Two routes** (a rule the guide gives). The Horde's Waterworks group splits into two columns when it has 6 or more bots, one on
each of the two lead-ups, so a single stun and area spell in the water does not catch everyone; the stronger column (more healers)
takes the shorter route. The Alliance takes the road in one column. The routes need **two via points per side on the navmesh**
that are not in the module yet (position needed: record the two Horde lead-ups and the Alliance road with the headless client, the
bridge structure at 1207, 953, 18 is on one of them, to check live); until they are measured the plan plays one route and the
decision D3 offers `short_route` only. The columns meet 40 yd short of the banner and the Horde casters cast only when both are in.

**After a won fight** (the guide's rule). The group takes the best swing node (THIRD first); the floater healer and 3 damage dealers go to
the node where the enemy comes out (the enemy's near node while they hold it; the far node while we hold all three), the rest guard.
Nobody stays in the enemy's start room.

**Late game and the score.** The core's 1800 warning is a human cue; the regimes cover it through the eta (a team near 2000 has a
short eta). No clock, so no timeout rules; the only stalemate is repeated flipping at one node, handled by the **flip limiter**
(expert rule, the Arathi `FlipLimiter`: no third assault of a node that was flipped back twice in 3 minutes unless the local odds
improved by 2).

**Supporting a human.** A human who stands within 40 yd of a node and casts the capture, or who calls a node (section 8), gets the two
nearest non-guard bots (the Arathi rule); bots never order the human.

## 7. Comp-dependent choices

- **No healer on the team**: the near-node defender slot goes to the sturdiest kit and the Waterworks group fights near the buff at Waterworks (34 yd from the banner, restoration when it spawned).
- **Two or more healers**: both with the Waterworks group; the third is the floater. Healer priority: the capper first, then the guard under attack.
- **Stealth classes**: RAIDER slots (offense plan) and the ghost option; a stealth class that is not a raider scouts the enemy's near node before the group commits (expert).
- **Many casters**: Waterworks group with crowd control in the water and at the lead-ups; melee on ASSAULT; a caster guard stays 8 to 25 yd from the banner.
- **Level spread** (85-89 and 90 alone): highest levels take the near-node defender slot and the first ASSAULT slots, the Arathi rule.
- **Few bots, more humans** (bots only fill): below 8 players the offense plan has no raiders (table above); a team of 5 plays near 1, Waterworks 4.
- **Guide comp** (no tank, three healers, mobility and gateway): not used; a warlock with Demonic Gateway places the pair between the near node and Waterworks only if both can be placed (to check live).

## 8. Call-outs (battleground chat)

Short lowercase lines through the bot voice (`BotVoice.h`, rows `bg_bfg_<event>`, same design as Arathi Basin; the SQL row set is new).
Humans read them; bots do not parse each other (they read the node states). Node words: `lh` (Lighthouse, also `light`, `lighthouse`),
`ww` (Waterworks, also `water`, `waterworks`), `mines` (also `mine`, `mn`). Rate limits and the channel are the shared ones (one line per
team every 6 s, at most 8 a minute per team, 20 s per bot between its own lines; expert 100 percent of events, good 70, casual 30,
casual ones late or wrong on purpose).

| Event | Who says it | Example lines |
|-------|-------------|---------------|
| Plan or stance | the leader, at the gates and at every change | `balanced, 1 mines, rest ww`, `third node, all go`, `hold 3, stay on nodes` |
| Opening: where a group goes | the first bot of each group, once | `ww front`, `ww back`, `going lh` |
| Enemies approaching or at our node | the nearest guard | `inc ww 4`, `inc mines 2 need help` |
| Node assaulted | the nearest guard, only if no "inc" went out in 10 s | `ww assaulted, restore` |
| Help on the way | one responder | `omw ww` |
| Node restored or taken by us | the capper | `ww back`, `got lh` |
| Node lost | the nearest bot | `lost mines` |
| Third node | one expert bot | `need the third`, `take mines, 3 nodes ends it` |
| Retreat | one survivor of the losing group | `ww lost, back to lh` |
| Ghost | the pair, once, before it starts | `ghost mines`, `inking lh` |
| Stealth seen | the nearest guard (good, expert) | `stealth at lh` |
| Score | one expert bot at 1000 and 1800 | `1000-600, we are ahead`, `they have 2, flip one` |

**Team intel only through call-outs** (owner decision 2026-11-18). The plan's enemy counts near a banner come from what a friendly bot
sees plus what the call-outs and a human's lines say, **not** from the battleground's full roster: an enemy counts for a node when
an alive bot of our side stands within 70 yd of it (a stand-in for sight, no line of sight test) or when a call-out placed it there
within the last 15 s (`PvpObs`, the observation store of BT-480, filtered by tier). Experts also count enemies seen earlier (the
observation lives 15 s). Arathi Basin's plan reads the whole roster today (`AbPlan.cpp:752-790`, `CountNear`): finding F7 and owner
question 6. The cost is one pass of friends against enemies per 2 s refresh: 10 x 10 distance tests, no new scan.

**Human calls** (BT-482). Read by the shared reader (`docs/bg-sheets/twin-peaks.md` section 8 lists the rules): `inc <node> <n>` or
`help <node>` gets one answer from the nearest MOBILE bot that heeds the line (65 percent of casual bots do) and the responders of
section 6; `take <node>` and `<node> lost` add up to 60 points to that node's swing score among the targets of the good bots;
`<node> clear` cancels an earlier "inc". The reader needs a map kind for 761 and the three node words; nothing else changes.

**Leader** (BT-481; rule of the Twin Peaks decisions): one leader bot per side, best tier then level then GUID hash, the incumbent
stays after the gates, a leader always calls its stance, a human is never the leader. It calls the drawn plan at the gates ("balanced,
<name> takes mines, rest ww"), the stance at every change, and the score at 1000 and 1800. An expert bot within sight of a node of
ours warns of an enemy group while it is still 60 to 50 yd from the banner.

## 9. Class jobs on this map

Class sheets own the spell lists; this table says what each class does for the plan. Guide tools without a 5.4.8 twin (the Demon
Hunter's mobility, Legion talents) are replaced by the class tools below, as the owner directed for Twin Peaks. Every tool is used only
if the bot knows it (level gates are the class sheets' business). Names were checked for presence in `Spell.dbc` (2026-11-18): Camouflage,
Feign Death, Vanish, Prowl, Stealth, Demonic Gateway, Create Healthstone, Ring of Peace, Mass Entanglement, Ursol's Vortex, Ring of Frost,
Frost Nova, Blink, Stampeding Roar, Tiger's Lust, Death Grip, Mass Dispel, Leap of Faith, Binding Shot, Freezing Trap, Cyclone, Hex, Sap,
Cheap Shot.

| Class | Preferred slot | Jobs on Battle for Gilneas |
|-------|----------------|----------------------------|
| Rogue | near-node defender, RAIDER | stealth to a banner, Sap the lone defender before the click (expert), Cheap Shot or Kick the capper, Vanish to leave a lost fight, Sprint; the first choice for the lone defender (guide) |
| Mage | near-node defender, ASSAULT (fire for burst) | Frost Nova and slows at the banner, Blink out, Polymorph a guard so the group can cast, Ring of Frost on the lead-up; the lone defender's kit (guide) |
| Hunter | near-node defender, MOBILE | traps on the banner area against stealth and cappers, Feign Death and Camouflage for the ghost, Disengage, Concussive Shot, Binding Shot on the lead-up; the lone defender's kit (guide) |
| Druid | RAIDER (feral), ASSAULT (balance), healer | Prowl, Mass Entanglement and **Ursol's Vortex** on a lead-up, Cyclone in the fight (guide), Stampeding Roar for the group's walk, heals |
| Warrior | ASSAULT, GUARD | Charge the capper, Intervene, Hamstring runners, burst in the fight (guide) |
| Paladin | healer, GUARD | the holy paladin is the guide's healer; Hand of Freedom for a bot in a root, Hammer of Justice on the capper, Divine Shield only to save the last guard |
| Priest | healer | discipline layering of cooldowns (guide), Pain Suppression and Power Word: Barrier on the group in the fight, Leap of Faith out of a pinch, Mass Dispel on immunities, Psychic Scream at the banner |
| Shaman | ASSAULT (elemental burst), healer | Earthbind and Earthgrab on the lead-up, Wind Shear on casters, Hex, Healing Tide and Spirit Link in the Waterworks fight |
| Warlock | MOBILE, ASSAULT | Fear and Howl at the banner, **Demonic Gateway** between near node and Waterworks, Create Healthstone for the group (guide), Unbound Will |
| Monk | ASSAULT, healer (mistweaver) | Roll to close, Paralysis on a guard, Leg Sweep and Ring of Peace in the water (guide), Tiger's Lust, Detox |
| Death knight | ASSAULT | Death Grip on a capper, Chains of Ice, Death and Decay on the banner; a squishy target of the enemy's focus (guide) |

**Target order in a node fight** (the shared decision of 2026-11-18, not a map rule): expert, the healer first when we hold a numbers
edge, otherwise drain the squishy damage dealers' cooldowns first (the guide's order); good, the healer first; casual, the lowest
health. **The capper first** for every GUARD within reach (the Arathi rule `CapperGuid`).

## 10. Failure modes and what the bots do

| Failure | Cause | Required behavior |
|---------|-------|-------------------|
| Camping the enemy start room | no valid target (`BgTacticsBFG.cpp:395-428`, `BFG_GY_CAMPING_*`); with the F1 fix the enemy's dead come out there | hold state (section 6); dwell rule 45 yd / 20 s |
| Everyone leaves while the third node is captured | the same code: a friendly-contested node is no target | the capping group and the guards stay until the flip |
| Nodes left empty | a captured node's group walks on; the first capper leaves after its 8 s | GUARD slots per held node; near-node defender stays |
| Scatter at the opening | `bg role` is `urand(0, 9)`, 3 "defenders" in 10, the others pick one of the 3 nearest nodes at random, 20 percent the farthest | assigned counts (section 4) |
| Map-wide chase | 5 percent of selections run at the "enemy player target" up to 500 yd (`:381-393`) | removed; leash 25 yd, responders limited |
| Lone defender with no answer | one bot at the near node, the group 300 yd away | the reserve rule: one MOBILE answers "inc", the retreat anchors at the near node; a lost near node is a normal event |
| Dying alone at the enemy's node | casual picks, 20 percent "silly" | retreat (good), only raiders go alone |
| Whole group stunned in the water | one column through the lead-up | two columns (Horde, 6 or more); crowd control of the casters; positions to record |
| Graveyard islands | Lighthouse and Mines graveyards have no walkable link to the main navmesh (`BgTacticsBFG.cpp:133-143`) | keep the jump-down tables (`BFG_ISLANDS`); measure the time lost per respawn (to check live) |
| Dead bots respawn at the wrong place | F1 (start-room read past the array) and F2 (spirit guide slots swapped), section 17 | fork fixes; until then the module's ghost handler teleports to the nearest friendly guide (`ReleaseSpiritAction.cpp:225-232`) |
| A guard pulled off its banner | `useBuff` walks to a buff within 20 or 50 yd; the Mines buff is 83 yd from its banner | buffs only inside the leash, or when the node is quiet |
| Fall and water | the structure at 1207, 953, 18 and the lead-ups | no jump paths; the module's `MoveFall` test is Twin Peaks only because it dropped bots here (`BattleGroundTactics.cpp:2267-2280`); live audit |
| Leaving before the flip | the capture group dismisses itself | guards stay until the node is occupied (good rule) |
| Cast on an own banner | none today: `BfgCanClick` already checks | keep |

## 11. Skill tiers on this map

Tiers apply in battlegrounds (BT-153); tier delays are the config values and the plan adoption lag the Warsong one (0, 1000, 3000 ms).

| Tier | Rules it plays |
|------|----------------|
| **Core** (casual and up) | walk to the assigned node; click only a legal banner (`BfgCanClick`); cast when no enemy is hitting the bot; guard within 25 yd of the banner while assigned; fight enemies within 40 yd; restore a node assaulted next to it; use a buff when hurt; follow plan changes (late); jump down from the island graveyards |
| **Good** (good and expert) | the near-node defender holds and calls "inc"; the opening group walks together and waits 15 s; the capping group stays through the flip; third-node urgency; the hold state; the retreat at `odds <= -3`; answer "inc" with the two nearest reserves; the 50 s restore deadline; guards hit the capper first; healer-first targeting; follow human calls; the stance of the leader |
| **Expert** | the ghost pair; Sap or Cheap Shot on the lone defender; the Horde's second route (D3); swing-score choice of the node and the odds count; squishy-cooldown target order when not ahead; the flip limiter; floater to the losing node; early warnings; accurate counts in call-outs |

What casual bots get wrong on purpose: react 0.4 to 0.9 s late and adopt a plan change one refresh late; walk to the nearest node
without waiting for the group; leave a node after the cast; chase one more kill outside the leash; skip the call-out; never retreat;
stand on a buff spot; about a quarter keep their slot one more refresh. **Tier targets** (owner 2026-11-18): against an average human team
expert 75, good 55, casual 35 percent; the 10 / 60 / 30 mix lands near 51 percent (inference).

## 12. Bots today (verified against the code at `main` b10ba91), with gaps

All line numbers are in `src/strategy/bg/BgTacticsBFG.cpp` unless stated.

| # | Rule or fact | Where | Status |
|---|--------------|-------|--------|
| 1 | `selectObjectiveBFG` copies the old Arathi case: (1) 5 percent of selections run at the "enemy player target" up to 500 yd; (2) with no node to take, run at an enemy within 500 yd or **camp the enemy start room**; (3) 85 percent of "defenders" go to the nearest node the team owns, the enemy contests, or the team is contesting; (4) otherwise the 3 nearest neutral, enemy or enemy-contested nodes (all three of them) go into a pool and one is picked at random; 20 percent of picks take the farthest | `:347-499` (priorities `:382`, `:407`, `:435`, `:456`) | **wrong**: no assignment, a map-wide chase, a camp |
| 2 | Roles: `bg role` is `urand(0, 9)` per bot at accept (1 percent re-roll per refresh in the BFG case), a "defender" is `role < 3`; both teams are "balanced" because `GetBotStrategyForTeam` has no BFG case; there is no near-node defender, no counts | `:360-375`; `BattleGroundJoinAction.cpp:1043`; `BattleGroundTactics.cpp:1826-1861`, `:4349-4372` | **wrong** (the guide's one-defender rule is missing) |
| 3 | `hasValidTarget` is false when no node is neutral, enemy occupied or enemy contested, including **while we capture the third node**, so every bot but the capper walks to the enemy start room | `:396-428` | **wrong** (new finding; owner rule: no camping) |
| 4 | The click rule is mirrored and a banner is cast on only when legal; a friend casting makes the bot drop its objective and pick another node | `:105-113`, `:603`, `:623-647` | OK for the click; **wrong** for a guard (no stay) |
| 5 | The objective point is a random point 2 to 6 yd around the banner, z lifted 0.7 yd (the banner sits in a navmesh hole) | `:123-131`, `:493-498` | **wrong** as a post (patrol); the lift is right |
| 6 | No use of the score, the node count or the race (`GetTeamScore` only in the log): no third-node urgency, no hold, no retreat, no regime | whole case; `:315-328` | **missing** |
| 7 | Island graveyards: take-off and landing tables for the Lighthouse and Mines graveyards, a jump when the bot stands on an island | `:133-238`, `:508-530` | OK, **to check live** (time lost per respawn) |
| 8 | Match evidence: `[BGBFG] node-change` on every state change and a state line every 30 s; the node state is read from the banner objects, "unknown" during the 2 s swap | `:276-329`, `:33-79` | OK; no core getter (finding F3) |
| 9 | Triggers: `GilneasStrategy` has the Arathi `bg check flag` and `bg use buff` triggers only; no plan tick, no `timer bg` reset, no call-outs | `BattlegroundStrategy.cpp:219-229` | **missing** |
| 10 | Leader, call-out reader, decision layer: map kinds are Warsong Gulch, Twin Peaks and Arathi Basin only | `Utils/BgCalloutPolicy.h:42-63`; `AbPlan.cpp`, `WsgPlan.cpp` | **missing** for 761 |
| 11 | Intel: the Arathi plan counts enemies from the whole roster (`CountNear`); the BFG case sees only what the bot sees | `AbPlan.cpp:752-790` | **wrong** in Arathi Basin against the 2026-11-18 rule (finding F7); not applicable to the BFG case today |
| 12 | Waiting spots at the doors; the start-area guard keeps bots inside 75 yd until the gates open | `:55-56`, `moveToStartBFG :332-345` | OK |
| 13 | Buffs: `useBuff` walks to a speed buff within 20 yd and to regeneration or berserk within 50 yd from any state | `BattleGroundTactics.cpp` `useBuff` | OK as a stop gap; **wrong** for guards (the Mines buff is 83 yd from its banner) |
| 14 | Mounting: mounts outside combat, dismounts before the cast | `:671-674`, `CheckMountStateAction.cpp` | OK |
| 15 | Paths: none recorded; the bots move by navmesh search; no two routes | none | **missing**, to check live (section 10) |
| 16 | Target choice: no preference for a capper; no healer priority for the capper; the shared tier target order is not wired here | `ChooseTargetActions.cpp`, `PartyMemberToHealValue.cpp` | **missing** for 761 |
| 17 | Respawn: the core rule, with F1 and F2 | core | **wrong in the core**, section 17 |

## 13. Gap list and proposed changes (ordered)

Sizes are changed-line estimates, not measured. Ledger ids BT-540 to BT-549 hold the findings.

**Reuse, do not copy**: the plan draw (`BgPlan::DrawPlan`), tier lag, call limiter, call chance, `Apportion`, the candidate pattern and
`Assign` of `AbPlanLogic.h`, the per-match record idiom of `AbPlan.cpp` (`Refresh` every 2 s, one lock, `Forget`), `DeliverCalls` and
the voice rows, the leader (`BgLeaderPolicy.h`), the reader (`BgCalloutReader`), `HuntCapper` and `HealPriority`, the flip limiter, the
responder choice, the observation store. What in `AbPlanLogic.h` and `AbPlan.cpp` is **Arathi specific** and needs a map table:
`NODE_COUNT`, `TICK_*`, `NODE_POS`, `NodeWord`, `HomeNode`, the Blacksmith as the hub, `IsContestedNode`, `GuardCount`, `OpeningFor`,
`NearerContested`, `BASE_TO_*`, `EXIT_PORTAL`, and about 70 uses of `NODE_COUNT` in the glue. The one design choice is whether to
generalise (owner question 10).

| # | Change | Files and functions | Size | Risk |
|---|--------|---------------------|------|------|
| G1 | **Quick fixes in the old case** (also the fallback while G3 and G4 are built): remove the 5 percent chase; no camp, a hold state instead (stay at posts); a node we are capturing counts as ours so nobody leaves; the guard stays when a friend casts; attack pool with the third-node weight; no re-roll for 761; deterministic near-node defender by team index from the roster (the lowest GUID hash among the best fit) until the record exists | `BgTacticsBFG.cpp` `selectObjectiveBFG`, `atObjectiveBFG`; `BattleGroundTactics.cpp:4372`; pure helper `Utils/BfgPlanLogic.h`; `tests/test_bfg_plan.cpp` | 120-220 + tests | low |
| G2 | **Map table for the node plan**: lift the Arathi specifics (list above) into a `NodeMap` data struct selected by map id, with the Arathi data unchanged and its 60 tests as the guard; or a copy (owner question 10) | `Utils/AbPlanLogic.h`, new `Utils/BgNodeMap.h`, `AbPlan.{h,cpp}` | 400-700 changed | medium to high (verified Arathi code) |
| G3 | **Battle for Gilneas pure rules**: three-node rate and eta table, swing values, regimes with THIRD and HOLD, opening counts per plan and size 5 to 10, guard counts including the hold state, defender fit, `odds` and the retreat test, ghost conditions, option lists with scores (RESTORE, THIRD, RETREAT, REINFORCE, HOLD, STAGE, GHOST, RAID, SUPPORT_HUMAN), dwell rule | `Utils/BfgPlanLogic.h`, `tests/test_bfg_plan.cpp` | 600-850 with tests | medium |
| G4 | **Team record and decision glue**: record from the banner state (or the getter), `GetTeamScore`, the roster (once per 2 s), `Decide`, `PlanChanged`, `DeliverCalls`, `OnTeamChat`, `Forget`; the BFG case calls it and keeps the old code (G1) as the fallback | `strategy/bg/BfgPlan.{h,cpp}` (or `AbPlan` over the map table), `BgTacticsBFG.cpp`, `BattlegroundStrategy.cpp` (`GilneasStrategy` tick triggers), `BattleGroundJoinAction.cpp` (Forget), `mod_playerbots.cpp` (chat hook) | 700-1,000 | medium to high |
| G5 | **Posts, routes and island data**: three node post tables (ground-checked), the two Horde lead-ups and the Alliance road as via points, the bridge structure at 1207, 953, 18, island hop times; needs a live capture | `BgTacticsBFG.cpp`, `BfgPlanLogic.h` data; headless run | 80-200 + live | medium (live data) |
| G6 | **Leader, call-outs and the decision layer for 761**: map kind and node words in the reader, voice rows `bg_bfg_*` and SQL, D1 and D9 reuse with the Gilneas node names, new D12 to D14 (section 16) | `Utils/BgCalloutPolicy.h`, `BgLeaderAdapters.h` (`NodeName`, `CandidateName`), `AiPvpDecisionPolicy.h`, `AwareAi/AiPvpDecision.cpp`, `src/Mgr/Text/BotVoice.cpp`, `sql/playerbots/updates/...` | 250-400 | low to medium |
| G7 | **Seen-only intel** (owner decision of 2026-11-18): enemy counts from friends in sight and call-outs; the same change for Arathi Basin (F7) | `BfgPlan.cpp` (`CountNear`), `AbPlan.cpp:752-790`, `PvpObservationMath.h` | 150-250 (+100 for Arathi) | medium |
| G8 | **Capper hunt, healer priority and the tier target order for 761** | `ChooseTargetActions.cpp`, `PartyMemberToHealValue.cpp`, `BgTacticsBFG.cpp` (`HuntCapper` for BFG) | 100-180 | medium (shared code) |
| G9 | **Core, fork** (three small items, section 17): F1 start-room index, F2 spirit guide slots, F3 node state getter | `BattlegroundBFG.cpp:648`, `BattlegroundBFG.h:176-183`, `:236-` | 1 + 2 + 4 lines | low |
| G10 | **Live audit**: lane posts and routes, the island hop time, mounts at level 85, spirit guides per owned set, the start-room respawn, the structure at 1207, 953, 18 | headless runs and the scenario harness (`.botscenario <name> bg 120 <seconds> bots=...`) | 0 code unless a point fails | |

Estimated total 2,000 to 3,000 lines with tests if the Arathi plan is generalised (G2) and 2,500 to 3,500 if it is copied, in four steps:
(1) G1 and G9, which remove the worst of today's behavior (the camp, the mass exit during the third capture, the scatter) and can
ship before the rest; (2) G3, G2, G4, G6 with tests; (3) G5, G7, G8; (4) G10.

## 14. Tests

Pure-logic unit tests in `tests/` (`bash tests/run.sh`, same style as `test_ab_plan.cpp`): the rate and eta table against the core's
constants (a test pins 12000/6000/1000 ms and 10/10/30 points and the 2000 limit); swing values for every (ours, theirs) pair; click
legality for the five node states and both teams; node classification; the regimes with hysteresis (OPEN, EVEN, THIRD, HOLD, PUSH,
LAST-NODE, PANIC) on score and node-count sequences; **a regression test that a node we are capturing is never "no target" and never
sends a group to the enemy start room**; opening counts for 5 to 10 players and the three plans; guard counts including the hold state;
defender fit order (rogue, mage, hunter, then the kits, healers last) and tier preference; stability under a death; stealth class takes
a RAIDER slot only with two; `odds` and the retreat test at the thresholds; the ghost conditions; the dwell rule at 45 yd / 20 s; the
node word parser (`lh`, `ww`, `mines`); the stance and node-target offers (the same list for the same input, the first option is the rule, an
unknown id falls back); the seen-only enemy count.

Live (lead's run, human queue; the map needs level 85 characters):
- Headless test client (not part of this repository): a level 85 human in the queue (bracket 85-89); the scenario harness seeds both teams:
  `.botscenario <name> bg 120 <seconds> bots=<20 names> strat=<a>,<h> roles=index|natural` (20 seeded level 85 bots, as for Twin Peaks; the live server has none yet).
- Read in the log: `[BGBFG]` node-change and state lines (plus a plan line with guards per node and the stance, like `[BGAB]`), the core's node messages, world states 1776/1777 (resources) and 1778/1779 (occupied bases), deaths per role, stuck bots, the time between a death and the next position change at an island graveyard, **where a ghost stands and which guide it uses** when the team owns Waterworks only, only the Mines, and nothing (F1 and F2).
- Acceptance: the match finishes by 2000 within 4 to 20 minutes; both sides hold 2 or more nodes at some point; every held node has 1 or more guard 90 percent of the time after minute 4; each assault of a held node meets a responder within 45 s; no bot within 45 yd of an enemy start graveyard for more than 20 s; no cast on an own banner; nobody leaves a capture in progress; no bot stands still 60 s alive and out of combat; fewer than 8 call-out lines a minute per team; the human team can win and lose (one win and one loss in four runs with the same human play); the third node is taken and held in at least half of the runs; a node under assault is restored at least once per match when guards are present.
- **Bench for the tier targets** (design decision 0032): bot-versus-bot matches by tier pair, at least 20 per pair, before human tests (as the Twin Peaks sheet).

## 15. Owner questions (recommended option first)

Questions 7 and 12 are guide-or-source versus core conflicts or core findings, so they are the owner's; nothing is resolved silently.

1. **Plan variety and the opening counts.** (a) Recommended: each team draws balanced "middle" 60 / offense "two ghosts" 20 / defense "hold two" 20 once per match (the Warsong and Arathi weights), with the counts of section 4: one near-node defender in balanced and offense, two in defense. (b) Always the guide's plan (one defender, the rest at Waterworks). (c) Uniform draw.
2. **The hold state at three nodes.** (a) Recommended: guards 2 / 2 / 3, three MOBILE at the far node, no camping, dwell rule 45 yd / 20 s at the enemy start graveyards. (b) Keep today's camp point near the enemy start room when no node is left to take. (c) Everyone stands at Waterworks.
3. **Who defends the near node.** (a) Recommended: the best fit by the guide's three kits (rogue, mage, hunter), then sturdy kits, tier and level after the fit. (b) Tier first (an expert always defends). (c) Random among the Waterworks group.
4. **Third-node urgency.** (a) Recommended: at two nodes held the third node is the first objective for all ASSAULT and MOBILE bots, guards of the two held nodes stay. (b) Only the two nearest bots go, to keep the guards strong.
5. **Retreat to the hub.** (a) Recommended: good and expert bots retreat to the nearest held node when `odds <= -3` (or `-2` with no healer), regroup there with the next wave and push as one group; casual bots never retreat; experts may ghost with two stealth bots instead. (b) Nobody retreats. (c) Every tier retreats.
6. **Team intel in the plan.** (a) Recommended: Battle for Gilneas counts enemies only from what a friendly bot sees (70 yd) and from call-outs, and Arathi Basin gets the same change as a separate item (G7, finding F7, since its plan reads the whole roster today). (b) Copy Arathi's roster read for consistency now and change both maps later.
7. **Core fixes in the fork** (section 17). F1: `GILNEAS_BG_GraveyardIds[teamIndex + 5]` to `teamIndex + 3` (the 5.4.8 reference core's value, BT-468). F2: swap the Waterworks and Mines rows of `GILNEAS_BG_SpiritGuidePos` (BT-548). (a) Recommended: both as one small fork fix branch, then confirm live with the GM probe after the next build. (b) Probe live first, fix after. (c) Leave them; the bots work around F1 and F2 by teleporting to the nearest guide, humans do not.
8. **The score and the rates.** No question: owner confirmed on 2026-11-18 (2000 to win; 10 per 12 s, 10 per 6 s, 30 per 1 s). Recorded so it is not reopened.
9. **Node state getter for modules.** (a) Recommended: add `BattlegroundBFG::GetNodeStatus(node)` (4 lines, the Arathi Basin form) to the hooks branch, so the plan stops inferring state from the banner object and has no "unknown" window. (b) Keep the derivation (works, 2 s blind after each change).
10. **Generalise or copy the Arathi plan.** (a) Recommended: lift the Arathi specifics into a map table (G2) and run both maps through it, with the 60 Arathi tests as the guard; the same table serves Eye of the Storm next. (b) Copy `AbPlan` and `AbPlanLogic` into Gilneas files (faster, 1,500 lines of duplicate to keep in step). (c) G1 only for now.
11. **How to test with no random bot at 85.** (a) Recommended: the scenario harness with 20 seeded level 85 bots and the owner's level 85 character in the queue, as decided for Twin Peaks; G1, G3 and the tests can be done and syntax checked before. (b) Wait until bots reach 85.
12. **The Horde's two routes.** (a) Recommended: measure the two lead-ups live first (G5), then the Horde splits in two columns from 6 bots; until measured one route. The Alliance takes the road. (b) Both sides split. (c) No split.
13. **Template `StartMaxDist` 0 (low).** (a) Recommended: leave it (the doors are closed; the module has its own guard). (b) Set it to 75 like Arathi Basin with a fork SQL file.


**Owner answers (2026-11-18):**
- All 13 recommended options are approved.
- The core fixes F1 (graveyard fallback index) and F2 (swapped spirit guide rows) go in now, with a live GM check after the build. F3 (node getter) goes in with them.
- Arathi's roster-based enemy count is retrofitted to seen-only intel as a separate item after Gilneas (BT-547).
- The Arathi plan is generalised into a node-map table, with the Arathi tests as the guard.

## 16. Decision points for the decision layer (leader bot, every 15 to 30 s)

The same design as Twin Peaks section 16 (design decision 0032 point 9): each decision point is a pure function that turns the state into a
**candidate list** with integer scores (highest first, ties by lower id); the layer, if present, answers with one candidate id; **the
fallback is the first candidate**, used when there is no answer within 2 s, the id is unknown, or the layer is off. Candidates never
carry free coordinates, only ids the plan already knows (nodes, groups, routes). One request in flight per side; asked every 20 s and
at once, but not within 5 s of the last, after a node event, a wipe, a retreat or a leader change.

State record sent (about 1 kB, from the match record the plan already keeps, no new scan): scores and etas of both sides; the three
node situations with their age; **seen** enemies near each node and our bots near each node (counts only); alive bots per group and
their distance to the Waterworks banner; dead per team and seconds to the next 30 s wave; the stance, the time the plan has run, the
healers alive, the stealth bots alive; whether a human is on the team.

| # | Asked when | Candidates (id: meaning) | Fallback score (the rule) |
|---|------------|--------------------------|----------------------------|
| D1 Stance (existing) | every ask | `balanced`, `offense`, `defense`, `hold`, `push`, `last_node`, `panic` (`close_out` is the rule's own and never asked) | the Arathi list, `BgLeaderAdapters.h` `StanceOffer`: the rule's stance 100 and first; in even play the other two plans 60 / 50 / 50, `hold` 55 only with all three nodes, `push` 52 when they hold a node; hold: `balanced` 60, `push` 45; push: `balanced` 60, `hold` 40; last node: `push` 50, `defense` 35; panic: `push` 55, `last_node` 45 when only one node is ours. At the gates the first answer is the 60 / 20 / 20 draw. **One change for this map**: the hold stance counts only at three nodes, which is what `canHold = ourNodes >= 3` already says |
| D3 Opening route (new id D12) | at the gates, Horde only, after the routes are measured | `short_route`, `long_route`, `split` | `split` 100 when the Waterworks group has 6 or more bots; else `short_route` 100; `long_route` 70; the Alliance is not asked |
| D13 Fight response (new) | the Waterworks fight state changes, at most every 5 s, and every 20 s while it runs | `fight_on`, `retreat`, `ghost`, `reinforce` | `fight_on` 100 while `odds >= -2` and a healer lives; `retreat` 100 when `odds <= -3` or `odds <= -2` with no healer (and 70 otherwise); `ghost` 110 only for an expert group with 2 stealth bots alive and at most one defender seen at the enemy near node; `reinforce` 90 when `odds` is -1 or -2 and a MOBILE bot is within 60 s |
| D14 Floater and healers after a won fight (new) | after a Waterworks fight is won (we hold it and the enemy group is dead) | `stay_hub`, `to_enemy_near`, `to_own_near`, `to_far_node` | `to_enemy_near` 100 while the enemy holds its near node; `to_far_node` 100 while we hold all three; `stay_hub` 60; `to_own_near` 50 |
| D9 Node target (existing) | every ask while ASSAULT has a target | the up to three targetable nodes with `NodeTargetOffer` (the best 100, the others their share of the best swing score) | the plan's own `ScoreTargets`, with the three-node swing values of section 3 and `THIRD` first; `NodeName` and `CandidateName` get `lighthouse`, `waterworks`, `mines` |

The third-node action (THIRD), RESTORE and the hold state are never asked: each has one sensible answer. The call-out text of the
leader follows the chosen stance. Casual bots drift from every answer (section 4); that is where a human can punish them. D12 to D14 are
new ids after the arena points D10 and D11 of `BgLeaderPolicy.h`.

## 17. Core and data findings (read only; nothing in the core was changed)

All three are small and additive. The fork rules apply (branch `fix/...` or the hooks branch, owner's yes before any push).

**F1. The start-room respawn reads past the end of an array (BT-468, medium, inference from reading).**
`SkyFire_548/src/server/game/Battlegrounds/Zones/BattlegroundBFG.cpp:647-648`:

```cpp
    // If not, place ghost on starting location
    if (!good_entry)
        good_entry = sWorldSafeLocsStore.LookupEntry(GILNEAS_BG_GraveyardIds[teamIndex + 5]);
```

`GILNEAS_BG_GraveyardIds` has `GILNEAS_BG_ALL_NODES_COUNT` = 5 entries `{ 1736, 1738, 1735, 1740, 1739 }`
(`BattlegroundBFG.h:173`), so `teamIndex + 5` is index 5 or 6, past the end; the start-room ids are at indices 3 (Alliance, 1740) and
4 (Horde, 1739). The 5.4.8 reference core reads `BG_BFG_GraveyardIds[teamIndex + 3]` (`BattlegroundBFG.cpp:684` there). **Fix: replace 5 by
3** (one line). What happens today is not observed: the array is a `const` table in the header, so the next object in memory decides;
the likely result is an id that is no graveyard, `LookupEntry` returns null, `Player::RepopAtGraveyard` then leaves the player where he
died (`Entities/Player/Player.cpp:5765-5813`, "if no grave found, stay at the current location"). When it applies: any death while the team owns no
node, which is the first minutes of every match and any time a team has lost all three. Bots are not affected: the module's ghost handler
teleports a dead bot to the nearest friendly spirit guide (`ReleaseSpiritAction.cpp:225-232`), and the start-room guides exist from the
preparation on (`BattlegroundBFG.cpp:189-191`). A human would be a ghost with no guide in range. To check live: a GM `.die` on a test
character in the first minute.

**F2. The spirit guide positions of Waterworks and Mines are swapped (BT-548, medium, inference from reading and the reference core).**
`BattlegroundBFG.h:176-183`, indexed by node (`_NodeOccupied` calls `AddSpiritGuide(node, GILNEAS_BG_SpiritGuidePos[node]...)`,
`.cpp:347-350`):

```cpp
    {1034.82f, 1335.58f, 12.0095f, 5.15f},     // Lighthouse
    {1252.23f, 836.547f, 27.7895f, 1.60f},     // Waterworks   <- these coordinates are the Mines graveyard (1735)
    {887.578f, 937.337f, 23.7737f, 0.45f},     // Mine         <- these are the Waterworks graveyard (1738)
```

`WorldSafeLocs.dbc` rows 1735 (Mines graveyard) and 1738 (Waterworks graveyard) confirm the coordinates, and `GILNEAS_BG_GraveyardIds[1]`
= 1738 and `[2]` = 1735 confirm the node order. The 5.4.8 reference core has the rows the other way round (`BattlegroundBFG.h:176-181`
there: index 1 = 887.578, 937.337, 23.7737, index 2 = 1252.23, 836.547, 27.7895). **Fix: swap the two rows** (the numbers and the
comments). Effect today (inference): the guide that spawns when Waterworks is owned stands at the Mines graveyard and the one that
spawns when the Mines are owned stands at the Waterworks graveyard, while `GetClosestGraveYard` sends the dead to the graveyard of the
owned node. So a team that owns only the Mines (the Horde's usual opening) has its dead placed at the Mines graveyard with no guide
there; a team that owns only Waterworks has its guide at the Mines island. The module's ghost handler then teleports the bot to the
nearest friendly guide: in the first case the Horde start room (205 yd from the Mines graveyard, against 378 yd to the guide at the
Waterworks graveyard); in the second the Mines island (378 yd from the Waterworks graveyard) for both teams, which for the Alliance is
nearer than its own start room (402 yd), so an Alliance bot that owns only Waterworks comes back next to the Horde's near node. A human
ghost has no guide in view. This also fits the module's earlier live observation of bots standing on the Mines island for minutes (`BgTacticsBFG.cpp:133-143`,
which blamed the island). To check live: own Waterworks only, die, read the ghost's position and the guide it is queued at.

**F3. No node state getter for modules (BT-549, low).** Arathi Basin, Alterac Valley and Eye of the Storm have read-only getters (hook
commit `94b3b4237c`, `docs/core-hooks.md`); Battle for Gilneas has none, so the module reads the spawned banner object and sees an
"unknown" state for the 2 s of each banner swap (`BattlegroundBFG.cpp:245-249`). **Fix: add to the public part of
`BattlegroundBFG.h`**, next to `GetPrematureWinner`:

```cpp
    // Read-only node state, for modules such as player bots that pick nodes to attack or defend.
    uint8 GetNodeStatus(uint8 node) const
    {
        return node < GILNEAS_BG_DYNAMIC_NODES_COUNT ? m_Nodes[node] : uint8(GILNEAS_BG_NODE_TYPE_NEUTRAL);
    }
```

(the form of `BattlegroundAB.h:264-268`; 0 neutral, 1 and 2 contested by the Alliance and the Horde, 3 and 4 occupied by them). Goes into the
core patches (`patches/`) with a row in `docs/core-hooks.md`.

**Smaller notes** (no action asked): the template's `StartMaxDist` is 0 (owner question 13); the Horde start is safe location 1799, not the
graveyard 1739; `GILNEAS_BG_OBJECT_GATE_A_2` and `GILNEAS_BG_OBJECT_GATE_H_2` are declared and never spawned (one gate object per side is
enough); `RemovePlayer` of the class is empty (`BattlegroundBFG.cpp:219`), so a leaving player takes no score cleanup (no effect on play).

**Ledger**: BT-540 to BT-549 in the findings ledger (kept outside this repository); BT-468 is the F1 row from the Twin Peaks pass and stays the id for F1.

## 18. As built (BT-550 to BT-553, 2026-11-18)

Where the code lives: the map as data `src/Utils/BgNodeMap.h` (Arathi Basin and Battle for Gilneas rows), the rules in `src/Utils/AbPlanLogic.h`
(each function takes the map, default Arathi Basin; 1,111 unit tests pass, the 60 Arathi ones unchanged), the Gilneas-only helpers and the
old-case rules in `src/Utils/BfgPlanLogic.h`, the team record and glue in `src/strategy/bg/AbPlan.{h,cpp}` (the file names stay), the map's tactics
in `src/strategy/bg/BgTacticsBFG.cpp`, the target order `src/Utils/BgTargetOrderPolicy.h`, the lists D12 to D14 in `src/Utils/BgLeaderAdapters.h`,
the chat rows `sql/playerbots/updates/db_playerbots/2026_11_19_00.sql`. The old random-role case is kept as the fallback when the plan cannot say.

| Gap | Ledger | Done |
|-----|--------|------|
| G1 | BT-550 | the old case: near-node defender from the roster, best node by swing value, a node we capture counts as ours, hold posts 2 / 2 / 3, no chase, no camp, no role re-roll, plan draw 60 / 20 / 20, node state through `BattlegroundBFG::GetNodeStatus` |
| G2 | BT-551 | the Arathi plan generic over the node map; Gilneas is the second row |
| G3, G4 | BT-552 | opening counts, guard counts, hold state, third node, retreat, ghost, seen-only intel, calls, `[BGBFG]` log lines |
| G6, G8 | BT-553 | node words in the reader, chat rows, D12 to D14 and the D9 node names, the capper hunt and the healer priority (the Arathi ones, now on both maps), the target order by tier |
| G5, G10 | | waiting for the live test: the Horde's two routes (D12 is written and not asked), the posts and the island hops, mounts at level 85, spirit guides per owned set, the start-room respawn |

**Where the code differs from the text above (choices the tests pin):**
- The third node (the close-out regime) is a good rule at Gilneas and an expert rule at Arathi Basin (`MapData::closeOutTier`).
- D14 has three candidates, not four: the sheet's "to the enemy's near node" and "to the far node" are the same node, so one id `to_far_node`.
- The ghost pair is two **expert** stealth bots (rogue or feral druid); with 10 percent experts it is rare, as the sheet says (an expert rule).
- The retreat group is every non-guard, non-raider bot of the team within 60 yd of the fight; a guard restores or dies at its post.
- The dwell rule of 45 yd at the enemy start graveyards is kept by the posts (no post lies inside, a test pins it), not by a timer.
- The leader's stance call uses the shared stance words; the "1 mines, rest ww" wording of the sheet is not built.
- The target order (G8) applies to Arathi Basin too, since it runs in the shared tick; casual bots keep their own target.
- Seen-only intel is on for Gilneas only; Arathi Basin keeps reading the roster until BT-547.
- A raider slot without two stealth bots is not made: the bots stay in the Waterworks group.
- Enemies are "seen" when an alive friend stands within 70 yd (no line of sight test).

**To check live (20 seeded level 85 bots and a human):** the opening arrival times and which side wins Waterworks; the defender holds its node;
the capping group stays through the third capture; the hold posts and the reserve at the far node; a retreat in a lost Waterworks fight and the regroup
after it; the ghost (needs two expert stealth bots); the retreat and ghost call-outs; where a ghost stands and which guide it uses after the F1 and F2
fixes; the call-out reader on `ww`, `lh`, `mines`; the `[BGBFG]` plan lines (guards at the posts per node).
