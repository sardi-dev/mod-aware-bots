# Silvershard Mines playbook

Status: **drafted and implemented in one pass overnight 2026-11-19 (every open question was decided with the recommended option and is listed in section 15 as "decided overnight with the recommended option"; they are open for review). Implemented on `feat/bg-silvershard-plan` (BT-600 to BT-614, section 19 says what was built and where the code differs from the text), unit tested and syntax checked, not yet tested live.** Written against the module `main` at 33505cc and the core at ea5b24a5c1 ("Add the Silvershard Mines
battleground", based on Legends of Azeroth 5.4.8). Silvershard Mines is the first of the two "last" maps of the owner's order (design decision 0032; Silvershard
Mines and Deepwind Gorge after the core maps). It is a **payload map**: three mine carts ride rails to four depots, the team standing next to a cart keeps it,
and a cart that docks pays its keeper. This sheet reuses the pieces of the earlier maps (the per-match record, the leader, the call-out reader, the
decision points, the tier lag, seen-only intel) and spells out what a **moving** objective changes.

Legend: **verified** = read in the core, the world data, the mesh files or our code (file and line given); **inference** = derived from verified
facts; **to check live** = needs a match; **owner question** = a source disagrees with the core, or a choice is open (section 15). Rules are marked
core, good or expert like the class sheets: casual bots follow core rules only and react late.

## 1. Header

| Item | Value | Source |
|------|-------|--------|
| Map id, battleground type id | 727, 708 (`BATTLEGROUND_SM`, the template id) | `battleground_template` row 708, `SharedDefines.h:4037` |
| Team size | **5 to 10 per team** (template MinPlayersPerTeam 5, MaxPlayersPerTeam 10); the guide is the 10 v 10 rated format | read-only query 2026-11-19 |
| Level | **90 only** (template MinLvl 90, MaxLvl 90); the wiki page says 30 to 80 (the 5.0.4 launch), the template wins | query; `the saved wiki notes` |
| Owner's band | the map needs level 90 bots and there are **no random bots at level 80 or above** (2026-11-19: 25 characters at level 90 in total, the probes and the test pool): the map is played only by seeded test bots until the random population reaches 90 | read-only count |
| Random pool | `Weight` 0 (custom SQL `0007_bg_pool_ssm_dg_off.sql`) until lane C's scoring test passes; a later custom file sets 1 | `battleground_template`, `custom/sql/world/0007` |
| Win | first team to **1600** (`BG_SM_MAX_TEAM_SCORE`); a "near victory" message at 1400 (`BG_SM_WARNING_NEAR_VICTORY_SCORE`, sent once); the guide says 1500 (a Legion value) | `BattlegroundSM.h:138-139`, `.cpp:555-569` |
| Time limit | **none** in the class: the match ends at 1600 or when a team falls below the minimum for the early-finish timer; the wiki's "matches can end in a tie" (2012 hotfix) has no counterpart in the core | `.cpp` (no timer), the game wiki |
| Points | **1 point per second for every cart a team keeps** (up to 3 a second) and **+150 for the keeper when a cart docks**; the guide's "1 per 3 s" is a later value | `BattlegroundSM.cpp:509-535` (`MineCartAddPoints`), `:497-505` (150), `.h:165` |
| Preparation | doors 157819 (four of them) closed until the start; 2 minutes of preparation (`LANG_BG_SM_START_TWO_MINUTES`); `StartMaxDist` 50 | `.cpp:58-60`, `:155-166`, template |
| Sources | core `BattlegroundSM.{h,cpp}` and the SQL of ea5b24a5c1; world DB (`waypoint_data` 7270011 to 7270033, `creature_template` 60283, 60378 to 60380, `battleground_template`); the mesh files `data/mmaps/0727_*.mmtile` parsed offline; our tactics (`BgTacticsSM.cpp` compiled out); the game wiki Silvershard Mines (read 2026-11-19); the Skill-Capped rated guide digest (private harvest, own words, Legion era). Notes: `.agents/plans/bots-bg-sheets/sources/silvershard-mines.*.md` | |
| Earlier tactics | the module's own disabled file only (`BgTacticsSM.cpp`, written against an older class: it reads `bg->BgCreatures[slot].IsEmpty()` and names such as `SM_MINE_CART_CONTROL_ALLIANCE` that this header does not have, so it could not compile today); no earlier module has more | read |
| Status | drafted and implemented, review pending | |

### Where sources and the core disagree (all go to section 15)

| # | Item | This core (`home`) | Other source | Question |
|---|------|--------------------|--------------|----------|
| 1 | Win score | 1600 | guide 1500, wiki 1600 | none: the core and the wiki agree |
| 2 | Points of a cart | 1 per second per kept cart, 150 at the depot | guide 1 per 3 s of control and 150; wiki "a large bonus" | none: the core wins (decided with 15 of 1) |
| 3 | Control rule | **each player moves the bar 1 per second**; the keeper is whoever is on its side of 50, equal numbers freeze the bar; **the bar never decays** | guide and wiki: "more players in the circle" | none: the core's rule is the majority rule in motion; the plan models the bar (section 3) |
| 4 | Circle radius | 22 yd (3D), a stealthed player does not count | guide "stand inside the circle" | none |
| 5 | Team size and level | 5 to 10, level 90 only | guide 10, wiki 10 and 30 to 80 | none: the data wins; counts scale over 5 to 10 |
| 6 | Track defaults | the **east cart rides to the Diamond depot (the long way, 149 s) and the north cart to the Troll depot (176 s) while nobody clicks a switch**; one click on the east switch sends the east cart to Lava (92 s), one click on the north switch sends the north cart to Diamond (228 s) | guide: Lava and Top are the default ends, a switch sends them to "Long" | 9 (the plan plays the core's defaults) |
| 7 | Tie | no time limit, no tie | wiki: a 2012 hotfix lets a match end in a tie | none |
| 8 | Switch cast | a spell click on NPC 60283 (spell 124492, script `spell_gen_bg_capturing`), cast time and range not read | guide: 3 s cast, damage interrupts | 5 (to check live) |
| 9 | Respawn | one graveyard per team at the start platform (4062 Alliance, 4061 Horde): **every death costs a walk of about 110 to 130 yd to the pit** and 300 to 360 yd to the far depots | guide: the same | none |
| 10 | Legion tools | none | duel that removes two players from the count, demon hunter tank | none: bots use a kill, a slow or a crowd control instead |

## 2. Map and positions

All coordinates are map 727 (`BattlegroundSM.h`, the SQL, the DBC), x north and y west as the core prints them. The map is **point-symmetric about the pit** (about
(740, 184)): the Horde start (633, 224) is the Alliance start (846, 144) turned half a circle, and the Diamond depot is the turn of the Waterfall depot.

| Place | Position (x, y, z) | Notes |
|-------|--------------------|-------|
| Alliance start platform | (846, 144, 329), graveyard 4062 | a raised island of the navmesh (24 polygons, x 821 to 875, y 114 to 169): bots **jump down** to the mine floor (section 18); doors at (852.0, 158.2, 326.8) and (830.1, 143.9, 326.5) |
| Horde start platform | (633, 224, 329), graveyard 4061 | island of 26 polygons (x 607 to 660, y 196 to 253); doors at (652.2, 228.5, 326.9) and (635.6, 208.2, 326.6) |
| Cart pit (all three carts spawn here) | east cart (744.5, 183.5, 319.7), waterfall cart (739.4, 203.6, 319.6), north cart (760.2, 198.8, 319.4) | 109 yd from the Alliance start, 119 yd from the Horde start |
| East track switch | NPC 60283 at (715.6, 101.3, 320.0); mesh vertex 1.7 yd away at (715.5, 102.9, 320.5) | 137 yd from the Alliance start, 148 yd from the Horde start; the east cart's junction is 13 yd from it at (717.2, 114.3) |
| North track switch | NPC 60283 at (847.5, 308.0, 346.6); mesh vertex 1.9 yd away at (846.7, 306.4, 347.1) | 164 yd from the Alliance start, 230 yd from the Horde start; junction at (834.7, 299.8), 15 yd away |
| Waterfall depot (cart 2 ends here) | (567.0, 337.1, 347.3) | 131 yd from the Horde start, 339 yd from the Alliance start: **the Horde's home depot** |
| Lava depot (east cart, switch opened) | (619.5, 79.7, 299.1) | 145 yd from the Horde start, 235 yd from the Alliance start: **the Horde's second home depot** |
| Diamond depot (east cart closed, north cart opened) | (896.0, 27.2, 364.4) | 127 yd from the Alliance start, 328 yd from the Horde start: **the Alliance's home depot** |
| Troll depot (north cart closed) | (778.4, 501.0, 359.7) | 363 yd from the Alliance start, 313 yd from the Horde start: neutral, the far north |
| Buffs | berserking (749.4, 64.3, 369.5) east and (790.0, 281.9, 355.4) west; restoration (539.9, 396.4, 345.7) waterfall and (614.2, 120.9, 294.4) lava | never swapped (`m_BuffChange` false); the restoration buff at lava lies on the lava depot's lowland |

**Rails** (the SQL, 3D lengths, the cart walks at 2.0 yd/s: 0.8 of the 2.5 base walk speed set at spawn, `.cpp:235`, to check live):

| Cart | Route | Length | Time | Keeper's worth (seconds held plus 150) | Junction reached at |
|------|-------|--------|------|-----------------------------------------|---------------------|
| East (cart 1) | to the junction, then 7270012 (switch **opened**) to **Lava** | 75 + 108 = 183 yd | **92 s** | 242 | 38 s |
| East (cart 1) | 7270013 (switch **closed**, the default) to **Diamond** | 75 + 222 = 297 yd | **149 s** | 299 | 38 s |
| Waterfall (cart 2) | 7270021 to **Waterfall** | 247 yd | **124 s** | 274 | none |
| North (cart 3) | to the junction, then 7270032 (closed, the default) to **Troll** | 132 + 220 = 352 yd | **176 s** | 326 | 66 s |
| North (cart 3) | 7270033 (opened) to **Diamond** | 132 + 325 = 457 yd | **228 s** | 378 | 66 s |

The times agree with the guide's 90, 120, 180 and 220 s (inference for the speed; the length comes from the data). **Distances for bots** (inference: the straight line times 1.3 for the mine's winding
ramps, 7 yd/s on foot, 11.2 yd/s on a mount): both bases are 109 and 119 yd from the pit (15 to 17 s on foot, 10 s mounted), so the first cart is a race of equals; the near depots
are 127 to 145 yd from their base, the far depots 313 to 363 yd.

## 3. Mechanics that matter for play (verified in `BattlegroundSM.cpp` unless stated)

**Control is a bar that every player in range moves.** Once a second (`UpdateCartControl`, `.cpp:249-289`) every **alive, not stealthed** player of the match within **22 yd (3D) of a cart** moves that
cart's bar by 1: up for the Alliance, down for the Horde. A player counts for the **first cart in the list order** that is in range only (`break`, `.cpp:278`), so a player between the carts at the
pit counts for the east cart. The bar runs 0 to 100, starts at 50, and **never decays**: a cart nobody stands next to keeps its keeper and keeps paying. A **mounted** player counts (no mount test).
- The **keeper** is the team on its side of 50 (51 or more the Alliance, 49 or less the Horde, exactly 50 nobody; `GetMineCartTeamKeeper`). The first player to stand at a fresh cart makes his team keeper after one second.
- **Equal numbers freeze the bar.** A keeper with one player at the cart holds against one enemy; three enemies against two net 1 a second. To take a cart from a bar at 100 you need **51 net player-seconds**
  (a net of 3 takes 17 s, a net of 5 takes 11 s, a net of 1 takes 51 s); a cart a team just tagged (bar 51 to 55) falls to any net of 2 in 1 to 3 s. **Hardening is presence**: five seconds of two players at the cart move the bar 10 away from 50.
- A visual aura on the cart (116085 Horde, 116086 Alliance, 118001 neutral) and the chat message ("has taken control of a Mine Cart") tell everybody the keeper; the bar itself is private (`m_mineCartsProgressBar`, no getter). The plan therefore **models** the bar from the
  keeper, the bodies of both sides it can see and the time (section 16), the way the Eye of the Storm plan did before its getter. A core hook `GetCartBarStatus(cart)` is proposed as finding BT-607 (section 17), not used.

**Scoring.** Each second the core adds one point per kept cart to the keeper (`MineCartAddPoints`, a 1 s timer; `.cpp:509-535`). When a cart is within 6 yd (2D) of one of its two possible depots it stops, the depot opens, the
players of the keeper's team within 22 yd get a scoreboard credit, and **3 s later** the cart is deleted and **the team keeping it at that moment gets 150** (`EventReopenDepot`, `.cpp:471-507`); a cart nobody kept pays nobody. The bar goes back to 50 and the
cart **respawns at its start slot on the next update** (the message "A Mine Cart has arrived from the central depot") and starts rolling at once. A cart is therefore worth to its keeper (seconds still to ride plus 150) points, and the
swing of a flip (we gain it, they lose it) is twice that (section 6). The 3 s at the depot are still part of the control count (players within 22 yd of the stopped cart still move the bar), but a bar at 100 cannot be turned in 3 s by anything.

**Track switches.** The east and the north cart ride a first leg to a junction, then **the path the switch NPC's aura says at the moment the cart is within 0.5 yd of the junction** (`MineCartsMoves`, `.cpp:376-414`). A switch is a spell click on NPC 60283 (spell 124492, the script `spell_gen_bg_capturing` removes
mounts and checks for immunity; a dummy 97372 calls `EventPlayerClickedOnFlag`, which **toggles** the opened and closed auras; `BG_SM_PREVENTION_AURA` 135846 blocks a second click for 3 s, `CheckTrackSwitch`, `.cpp:329-352`). **Anybody may click either way**, a click flips whatever the state was, and
the state **persists** until the next click, so it decides every later cart of the track until somebody changes it. Message "The Eastern (Northern) Crossroads changed tracks!" goes to everybody. The cast time and the
click range of 124492 are not read (to check live); the guide says a 3 s cast that damage interrupts.
- The window to decide for a fresh cart is **38 s for the east cart and 66 s for the north cart** after it spawns; later clicks apply to the next cart of the track. The east junction is 13 yd from its switch and the north junction 15 yd, so **a squad that rides with
  the cart passes within the cart's circle of the switch for the whole of the junction time**.
- The worth of the options (section 2): east closed (Diamond) 299, east opened (Lava) 242; north closed (Troll) 326, north opened (Diamond) 378. A team prefers the depot near its own base: the Alliance **east closed and north opened** (both its carts end at Diamond, 127 yd from its start),
  the Horde **east opened and north closed** (Lava and Waterfall are 131 and 145 yd from its start, the third cart at neutral Troll). Both teams want a **different** state of both switches: a switch war, decided by the **last click before the junction** (section 6).

**Respawn and the start platforms.** A death returns the player to the team's graveyard on its start platform (30 s waves, the core's rule), an island of the mesh 9 yd above the mine: bots walk to a take-off point and jump down (section 18). Nothing in the class
kills players who wait on the platform and nothing stops them leaving it before the gates open beyond `StartMaxDist` 50 (platform and doors).

**Other.** Buffs are placed once and never swapped (berserking at the east and west, restoration at the waterfall and lava ends); there is no flag, no node, no carried objective, no time rule. The 1400 message sends once.

## 4. Roles by side size

Roles are **assigned**, not rolled: one plan per team per match in a per-match record rebuilt from the battleground at most every 2 s by whichever bot asks first (the Arathi, Gilneas and Eye of the Storm pattern, `SmPlan.cpp`),
with the leader, the observations and the decision book of `PvpMatchState`. A human takes a slot and is never commanded. The jobs:

| Job | Duty |
|-----|------|
| **MAIN** | the group at the cart that is worth the fight (the highest scored cart, section 6): inside the 22 yd circle (stay within 8 yd of the cart, fight what comes, no chase beyond 30 yd), healers inside the circle, all of them walk with the cart along its rail |
| **OFF** (one bot per off-cart) | durable mobile kits tag the other carts and hold them against the opponent's off bot (the "pair" of the guide, one per cart here); a lone off bot at a cart holds against one enemy and is outnumbered by two; it calls "inc" when two or more come; the plan never sends it alone against a group (the peel of section 6 adds a second body), the walk-away of a lone off bot is not built (section 19) |
| **SWITCH** (derived, at most one per team at a time) | a good or expert bot of the off group or the squad that rides the cart: walks to the switch while the cart is before the junction and the state is the wrong one for the team and clicks in time (section 6) |
| **FLOATER** | (not built, section 19) with 8 or more players, the third healer or an extra damage dealer follows the main group at 15 to 22 yd and **answers "inc"** of an off cart, to give the off pair a second body |

**Counts** by team size and stance (`SmLogic::OffCount`, one tagger per off cart; the rest are the main group). The plan is drawn once per team per match (60 balanced, 20 offense, 20 defense, the owner's weights, `BgPlan::DrawPlan`); the leader's stance changes it (section 6).

| Players | Balanced: main / off | Offense: main / off | Defense: main / off |
|---------|----------------------|---------------------|---------------------|
| 10 | 8 / 2 | 9 / 1 | 7 / 3 |
| 9 | 7 / 2 | 8 / 1 | 6 / 3 |
| 8 | 6 / 2 | 7 / 1 | 5 / 3 |
| 7 | 5 / 2 | 6 / 1 | 5 / 2 |
| 6 | 4 / 2 | 5 / 1 | 4 / 2 |
| 5 | 3 / 2 | 4 / 1 | 3 / 2 |

The numbers follow the guide's 8 and 2 at 10 players; with fewer players the main group never drops below 3 (a group of 3 beats a lone enemy and a pair). A cart that docks in under 12 s, or that no bot can reach before it docks, gets no off bot.

**Assignment** is deterministic so tests can pin it (a candidate list per slot, the shared pattern): the **off fit** puts the durable mobile kits first (feral and guardian druid, brewmaster, death knight, paladin, rogue, warrior; healers and casters last), good and expert tiers prefer
it, and the **main fit** is the rest with the healers first; a bot keeps its cart unless the counts change (stickiness), the nearest bot wins a cart when the fit is equal, the ETA of the walk to the cart's rail matters. **Reassignment** happens on events only: a death (the
slot goes to the nearest bot at once and the dead bot is dealt again after the respawn), a dock (all carts are scored again), a keeper change, a stance change, a human call. **Casual drift** as decided for Twin Peaks: a casual bot adopts a new plan one refresh late and about a quarter keep their
old cart one more refresh.

## 5. Opening plan (first 60 to 90 s)

The doors open after the 2 minutes of preparation and **all three carts start rolling at once**. Each team's bots wait on their platform (a spot per role, `moveToStartSM`), **jump down at the gates**, and the plan's first assignment is computed from the static map and the team size (nothing is seen yet).

1. **Pick the main cart** from the scored list of section 6 (the guide fights for the shortest, the port's values favour the north cart, section 6 explains why the leader's rule prefers the cart the team reaches first).
2. **The off bots** go to the other two carts (the east and waterfall carts for the Horde whose depots lie near its base, the north and east carts for the Alliance), **one bot each**, nearest and sturdiest first.
3. **The switch holder** (a good or expert off bot) takes the east switch when the state is wrong for the team and the junction (38 s) can be made, else the north one (66 s): an Alliance holder walks to the north switch and clicks **opened** for its Diamond ride; a Horde holder walks to the east switch and clicks **opened** for the Lava ride.
4. The main group walks in **one blob** (the squad's walk is the slowest member's, as on the node maps) and **enters the 22 yd circle together**: inside the circle every player is +1 a second, so a team that arrives in a row loses the bar to the enemy blob.

Expectation (inference): the first cart is tagged at 15 to 25 s; both teams' main groups meet at the same cart or each tags its own and the other team's blob hits the off bots; a lone off bot dies or
walks away within a minute, so **the first 90 s decide the first dock**. The team that dealt its off bots to the carts the enemy is not heading for keeps 1 to 2 carts ticking at 1 point a second each.

## 6. Mid-game rules

**Facts per cart** (rebuilt every 2 s, `SmPlan.cpp`): live or not (a docked cart is gone for one tick), position (read from the creature, public like a flag on the minimap), keeper (the aura), the seconds still to ride (the route and the rail position, `SmLogic::RemainingSeconds`), before the junction or not,
our bodies in the circle (exact), the enemy's bodies in the circle (**seen only**: an enemy counts when an alive friend stands within 70 yd of it, or a call-out placed it there, weighed by confidence) and the **modelled bar**. The modelled bar starts at 50, moves by the net bodies per second, and is clamped to the keeper's side when the aura says so.

**Options per cart** (a scored list, the decision point D18; the first is the rule's answer): TAKE a neutral cart (the cart's worth), FLIP an enemy cart (twice the worth when feasible), GUARD our cart (a share of the worth that grows with the enemy's bodies), SKIP. Scores are whole numbers:

| Cart state | Option | Score | Feasible only if |
|------------|--------|-------|------------------|
| none keeps it | TAKE | worth minus the walk in seconds | our ETA is below the seconds still to ride minus 5 |
| they keep it | FLIP | 2 times the worth minus the walk minus the flip seconds | ETA plus the flip seconds (the margin over 49 divided by our net bodies, `FlipSeconds`) is below the seconds still to ride minus 5 |
| we keep it | GUARD | worth times the enemy bodies seen over our bodies plus one, at most the worth; a quarter of the worth with no enemy seen | the cart has at least 12 s to ride |
| any | SKIP | 0 | a cart that cannot be reached, or docks in under 12 s |

The **worth** is the seconds still to ride plus 150. The opening adds a first-arrival bonus: 2.5 points for every second the team's meeting time with a cart is shorter than the enemy's from the two start platforms (at most 10 s either way, only for the first 90 s): +20 for the Alliance at the north cart, +25 for the Horde at the waterfall cart; after the opening the enemy's arrival is read only from what the team sees.
at the opening and from what the team sees afterwards.

**Decision rules (the plan)**
1. **Main cart** = the top scored cart; the main group stays with it along its rail (it **escorts** a cart we keep and **contests** one we do not). The main group is the team minus the off bots of section 4. A change of the top cart moves the main group only when the new top scores 40 points more (hysteresis against flipping between carts) or the old cart docked or became infeasible.
2. **Off bots** hold one cart each: tag it and keep inside the circle (the walk-away from a group is not built, section 19). A cart whose enemy bodies reach two more than ours but is worth more than 60 percent of the main cart's score gets a **second** bot from the main group (the "peel", good rule).
3. **After a dock** every cart is scored again: the docked cart is at the pit again (bar 50), at 92 to 228 s of ride; the two teams' bots are 130 to 360 yd away, so the nearest bots tag it while the rest follow the main plan (the guide's "everybody goes to the next spawn" is exact only when the dock was near the pit; the score list handles it).
4. **Wipe** (the main group has no live member in 40 yd of its cart): the survivors and the respawning bots **regroup at the pit** rather than trickle in, then take the best reachable cart (good rule); a cart that docks before they arrive is not contested (SKIP).
5. **Ahead** (leader's stance DEFENSE or TURTLE: ahead by 100 or more, or ahead by 150 late, section 16): guards on every cart we keep, off bots stay, **the main group sits on the cart that is nearest to a dock** and does not start a fight it must walk far for. **Behind** (OFFENSE, ALL_IN): one off bot only, the main group flips the cart worth most, and the leader says so.
6. **The score race**: ETA to 1600 for a team = (1600 minus score) over its points a second (kept carts, at least 0.5); the stance rules read the difference (the same shape as the Arathi race clock, `SmLogic::ChooseStance`).
7. **Leash**: a bot in a cart's circle fights what comes and does not leave the cart by more than 30 yd for a kill (core rule); a casual bot follows a fleeing target out about a third of the time (a human mistake on purpose); an expert never chases, a good bot goes back after 3 s.
8. **Stealth** (rogues, feral druids, mages): a stealthed player is **not counted** (`SPELL_AURA_MOD_STEALTH`), so stealth is for the **approach** and the **switch**; the bot leaves stealth to enter the circle.

**The switch rule** (`SmLogic::SwitchChoice`). For a track, the **preferred state** of a team is the better of the two ends by (worth plus 60 for the team's home depot, minus 60 for the enemy's home depot): the Alliance east closed and north opened, the Horde east opened and north closed
(section 3). A switch bot acts only when all hold: the cart of the track is **before its junction**, the state is **not the preferred one**, the gain is at least 40 points, the bot is within reach (its walk plus the cast plus 4 s is under the seconds to the junction) and **no group of
two or more enemies is seen within 20 yd of the switch** (it does not fight for it alone). Good bots click as soon as they are in range and the state is wrong; **expert bots wait at the switch and click in the last 8 s before the junction** (the last click wins; they re-read the state
before the cast ends and cancel the cast when the enemy already turned it the right way, because a click **toggles**). Casual bots do not hold switches; a casual bot standing at a switch because the cart passes it does not click it.

## 7. Comp-dependent choices

| Team | Change |
|------|--------|
| 3 or more healers | the third and fourth heal the **off pair's** nearest bot (an off bot with a healer is a real hold); the plan counts them as FLOATERS (section 4) |
| No healers | **no off bots**: everybody is MAIN (a lone bot cannot be healed and hold a cart), the stance is unchanged |
| Many stealth classes | stealth holds the switch and tags the far cart; the plan does not count them in the circle until they leave stealth |
| Few durable kits | the off fit falls to the nearest bot with a self-heal or an escape; a healer is never an off bot |
| Casters | stay inside the circle behind the melee at 25 to 35 yd from the cart's centre only when the target is a seen enemy; a caster at the edge of the circle still counts (22 yd) |

## 8. Call-outs (battleground chat)

Short human lines, rate limited per team (6 s) and per bot (20 s), at most 8 a minute per team, said by the bot that sees it; experts call more often, casual bots call 30 percent and late (the shared limiter, `BgPlan::CallLimiter`).
Words for places: **lava** (the east cart), **mid** or **middle** or **waterfall** or **wf** (the waterfall cart), **top** or **troll** (the north cart), **long** or **diamond** (the Diamond depot), and for the switches **east** or **es** and **north** or **ns** ("east switch", "north sw"; the words "switch", "sw", "track" and "cart" may stand in a line).

| Call | Who | Example |
|------|-----|---------|
| Opening | the leader | "main on lava, pair mid and top" |
| Stance | the leader (at the gates, at every change, after a handover) | "all out", "hold what we have" |
| Inc | the bot that sees two or more enemies (an expert early, at 60 yd) | "inc mid 3", "mid 3 inc" |
| Taken / lost | the bot at the cart when the keeper changes | "got lava", "lost top" |
| Switch | the switch holder | "east switch set", "north set, diamond" |
| Docked | the nearest bot of the keeper | "mid docked, next cart" |
| Wiped | the survivor | "wiped at lava, regroup pit" |

**Reading a human**: the reader (`BgCalloutPolicy.h`) knows the words above on map 727: `inc lava 3`, `help top`, `lost mid`, `take lava`, `clear lava`. A line becomes an observation of the side: a report of enemies at a cart adds to the bodies the plan counts there (never lowers what it sees);
a claim ("take lava") adds 60 points to that cart for good bots; a casual bot heeds 65 percent. Nothing is a command.

## 9. Class jobs on this map

| Class and spec | Job |
|----------------|-----|
| Feral and guardian druid, brewmaster monk, death knight, paladin, warrior | off bot (durable and mobile), the switch holder for the good tier; death knights and frost mages **slow** the opponents that walk towards a cart (the guide's slows on the slope) |
| Rogue, feral druid | stealth approach, **switch holder** (expert), tags the far cart while the enemy sits at the main cart; opens on the one defender at a switch |
| Healers | inside the circle of the main group, **the carrier-healer rule** (heal the lowest within the circle first), a spare healer heals the off bot |
| Mages, warlocks, hunters, shadow priests | damage inside the circle; slows and crowd control on the enemy's healer when it stands inside the circle; an expert caster kicks from the edge |
| Balance and other druids | cyclone (or the shared PvP crowd control) on the nearest enemy that stands in the circle alone, speed to chase carts |
| Everyone | the **target order** of the PvP rules: expert healer first with a numbers edge and the squishy ones otherwise, good healer first, casual the lowest health (`BGTactics::FightOrder`) |

## 10. Failure modes and what the bots do

| Failure | What the bots do |
|---------|------------------|
| Platform island: no path off the start platform | take-off and landing points, `JumpTo` (kept from the old file, the points are re-checked against today's mesh, section 18) |
| Falling into a mesh pocket (the 137-polygon lowland at x 805 to 968, y 168 to 422 and the 69-polygon lava floor at x 540 to 600, y 41 to 150) | to check live; a bot with no path for 10 s is treated as stuck by the shared stuck-bot code and dealt again |
| A bot fights beyond the circle | the leash of section 6 (a bot of any tier is brought back after a few seconds) |
| Everyone walks to the pit after each dock (a trickle) | the score list: only the bots nearest the new cart go, the rest follow the main plan |
| Two switch clickers undo each other forever | the last-8-s rule; expert bots wait, good bots click once per cart; a click is never repeated within the 3 s lock |
| A click toggles the wrong way (the enemy clicked during the cast) | the cast is cancelled when the state turned the right way before it ends |
| A cart docks while nobody is near | nothing: the keeper still gets 150 (the bar does not decay) |
| A dead bot walks 110 to 360 yd back | the plan deals the nearest respawning bots to the nearest cart; no bot is held on the platform |
| The cart is off the mesh (up to 9.9 yd from the nearest walkable point at three spots of the Diamond route) | the bot targets a point **ahead on the rail but on the mesh** (the nearest walkable point within 12 yd); the circle is 22 yd so it counts anyway |

## 11. Skill tiers on this map

| Rule | Core | Good | Expert | Casual |
|------|------|------|--------|--------|
| Walk to the assigned cart, stay in the circle, fight what comes | yes | yes | yes | yes, but follows a target out of the circle a third of the time |
| Enter the circle together with the blob | | yes | yes | arrives alone 30 percent of the time |
| Scored cart list (FLIP only when feasible) | | yes | yes | nearest cart |
| Off bot walks away from a group, calls "inc" | | yes | yes | stays and dies |
| Switch holder | | clicks when in range | waits for the last 8 s, cancels a wrong toggle, uses stealth to arrive | never holds a switch |
| Target order | | healer first | healer first with an edge, squishy otherwise | lowest health |
| Call-outs | 30 percent, late | 70 percent | 100 percent, early inc at 60 yd | 30 percent, late |
| Reads human call-outs | 65 percent | yes | yes | 65 percent |

## 12. Bots today (verified against the code at `main` 33505cc)

| Rule | Where | State |
|------|-------|-------|
| The map's tactics | `BgTacticsSM.cpp` (444 lines) wrapped in `#ifdef BOT_HAVE_BG_SM`, which nothing defines, and the same flag around five sites of `BattleGroundTactics.cpp` (:2397, :2425, :2712, :3370, :4009) | **missing**: the map plays the generic objectives (`BgTacticsGeneric.cpp`) today; and the file **could not compile**: it reads `bg->BgCreatures[slot].IsEmpty()` (the slot is a `uint64`) and uses `SM_MINE_CART_CONTROL_*`, `BG_SM_PATH` and other names the new header does not have |
| Control model | the disabled file: "control is by numbers"; one bot per team per switch by `bg role` | **wrong**: the core's rule is a bar every player moves, the keeper is on its side of 50, a bar never decays |
| Switch click | the disabled file calls `bg->EventPlayerClickedOnFlag(bot, switchNpc)` | **wrong**: it skips the spell click, the cast, the interrupt and the range; **and** it assumed the opened state is the Horde's for the east track only, the default being the other way round |
| Roles and squads | `bg role` 0 to 9, re-rolled with 1 percent a tick (`oddsToChangeRole = 1`, :4009) | **missing**: no plan, no main group, no off bots |
| Leader, stance, call-outs, reader words, chat rows, decision points | none for map 727 | **missing** |
| Intel | `getPlayersInArea` over the whole roster | **wrong** (not seen-only) |
| Start platform jump-down | the disabled file (`SM_LEDGES`) | **kept**; to check live against the mesh of today |
| The strategy "silvershard" and the map's triggers | `BattlegroundStrategy.cpp:257`, `AiFactory.cpp:1058` | present: a refresh timer and the buff triggers |
| Target order by tier | `BGTactics::FightOrder` (node maps) | **missing** on this map (the tick does not call it) |

## 13. Gap list and proposed changes (ordered)

| # | Gap | Files and functions | Size | Risk |
|---|-----|---------------------|------|------|
| S1 | The map has no plan; the old file cannot compile | `Utils/SmPlanLogic.h` (new, pure: the rails, the bar model, the cart options, the allocation, the switch choice, the stance), `tests/test_sm_plan.cpp` | large | low (pure, tested) |
| S2 | No tactics for the moving carts | `strategy/bg/BgTacticsSM.cpp` (rewritten), `strategy/bg/SmPlan.{h,cpp}` (record, roster, refresh, leader turn), `BattleGroundTactics.{h,cpp}` (the `#ifdef`s removed, `smTick`, the strategy draw), `BattleGroundJoinAction.cpp` (Forget) | large | medium (movement against a moving target; live check) |
| S3 | The switches | the switch job in `SmPlan.cpp`, the click through the spell click in `BgTacticsSM.cpp` (`atObjectiveSM`) | medium | medium (cast time and range unknown) |
| S4 | No leader, call-outs, reader words, chat rows, decision points | `BgCalloutPolicy.h` (`MapKind::SM`, the words), `BgLeaderPolicy.h` and `AiPvpDecisionPolicy.h` (D18, D19, BOOK_SIZE 21), `BgLeaderAdapters.h` (the offers, `CandidateName`), `SmPlan.cpp` (the calls), `sql/playerbots/updates/db_playerbots/2026_11_19_02.sql` | medium | low |
| S5 | No leash, no target order | `BgTacticsSM.cpp` (`smTick`), `Utils/SmPlanLogic.h` (`LeashAction`) | small | medium (combat engine interplay) |
| S6 | Live audit | headless client, the scenario harness | medium | the cast time of the switch click, the cart speed, the jump-down, the pockets of the mesh |

## 14. Tests

- **Pure** (`tests/test_sm_plan.cpp`, `tests/test_sm_leader.cpp`): the rails (lengths, times, the junction times, the remaining seconds from a position), the bar model (steps, clamps, flip seconds), the options and their feasibility, the allocation (counts by size and stance, stickiness, fit), the switch choice (preferred state per team, the window, the last-8-s rule, no click with a group at the switch),
  the stance and the race, the leash, the reader words, the decision lists.
- **Syntax pass**: `tools/syncheck/syncheck.sh module` on every changed `.cpp` and its includers.
- **Live** (lane C, after the deploy): a 5 v 5 with the test pool (probes at level 90, `.playerbots bot addclass`, `.debug bg` as noted in `LIVE-CHECKS.md`), read the `[BGSM]` lines (`state`, `cart`, `switch`, `assign`, `dock`), accept: the match ends at 1600, both sides tag all three carts in the first 60 s, a dock pays 150 to the keeper at that moment, the east switch is clicked by a bot with a cast that a hit interrupts, no bot stands on the start platform after the gates, the scoreboard shows carts controlled for bots.
- **Acceptance**: the match finishes; both sides play the objective; a human team can win and lose.

## 15. Owner questions (recommended option first; **every one decided overnight with the recommended option**, 2026-11-19)

| # | Question | Decision (recommended option first) |
|---|----------|--------------------------------------|
| 1 | Guide or core where they disagree (1600 against 1500, 1 point a second per cart against one per 3 s, the control rule, level 90 only, 5 to 10 players, no tie) | **The core**: decided overnight with the recommended option |
| 2 | The main cart: the top of the scored list (worth minus the walk, plus the first-arrival bonus) or always the shortest cart (the guide's Lava) | **The scored list**: it moves with the switch state and the keeper, decided overnight with the recommended option |
| 3 | Off bots: 2 balanced, 1 offense, 3 defense, one bot per cart | **As the table**: decided overnight with the recommended option |
| 4 | Switch holders: good and expert off bots, expert last-8-s click, casual never | **Yes**: decided overnight with the recommended option |
| 5 | The click: the real spell click (`HandleSpellClick`, a cast that damage interrupts) or the battleground function directly (the old file) | **The spell click**: a bot does what a human does, decided overnight with the recommended option |
| 6 | Stealth: a stealthed player does not count in the circle, so stealth is only the approach and the switch | **Yes**: decided overnight with the recommended option |
| 7 | The leash: 30 yd, a casual bot follows a target out a third of the time | **Yes**: decided overnight with the recommended option |
| 8 | A read-only getter for the cart bar in the core (a hook like the Eye of the Storm one) | **Not now**: the plan models the bar from what it sees; the hook is a proposal (BT-607), decided overnight with the recommended option |
| 9 | The core's default tracks (east to Diamond, north to Troll) differ from the guide's (Lava and Top): play the core's, name the carts by track (lava = the east cart) | **Play the core's**: decided overnight with the recommended option |
| 10 | The decision points: D18 (which cart the main group takes) and D19 (the switch: click now, wait for the last moment, leave) asked only when two options score 45 or more | **Wired**: decided overnight with the recommended option |
| 11 | The map in the random pool (`Weight` 1) | **Not by this branch**: lane C's scoring test decides; the file stays `0007` until a probe run passes, decided overnight with the recommended option |
| 12 | The call-out words: lava, mid (waterfall, wf), top (north, troll), long (diamond), es and ns for the switches | **As listed**: decided overnight with the recommended option |
| 13 | Stances reuse the shared ones (balanced, offense, defense, all in, turtle) with the thresholds of section 6 | **Yes**: decided overnight with the recommended option |

## 16. Decision points for the decision layer (leader bot, every 15 to 30 s)

Every point is a list of options with a deterministic whole-number score, best first; **the first is the rule's answer and the fallback**; a decision layer may answer with another id of the same list (judged by the leader policy: match, side, leader epoch, state version, offered id).

| Point | Offered when | Options (id: meaning) | Rule |
|-------|--------------|------------------------|------|
| D1 stance | always (the shared stance list) | balanced, offense, defense, all in, turtle | the race clock, section 6 |
| D18 cart | two carts score within 45 of each other (a real choice) | 0 east (lava) cart, 1 waterfall (mid) cart, 2 north (top) cart | the best score of the table of section 6 |
| D19 switch | a switch bot of the team has a duty | 0 click now, 1 wait for the last moment, 2 leave the switch | click now for good tier, wait for expert, leave when the window is gone or a group is seen at the switch |

**The bar model** (`SmLogic::StepBar`, `ClampToKeeper`): `bar += net bodies * seconds` toward the side with more bodies (Alliance up), clamped 0 to 100, and clamped to the keeper's side when the aura says (Alliance at least 51, Horde at most 49, none exactly 50); `FlipSeconds` is the seconds until our net bodies carry the bar across 50.

## 17. Core and data findings (read only; nothing in the core was changed)

| Id | Finding |
|----|---------|
| BT-606 | **Defaults of the tracks**: no creature or spawn row gives the switch NPCs an opened aura, so with nobody clicking the east cart rides to Diamond (149 s) and the north cart to Troll (176 s); the "opened" state is the one with the shorter east route (Lava). Not a bug: a naming trap for anybody reading the guide |
| BT-607 | **No getter for the cart bar** (`m_mineCartsProgressBar` private, only a per-player world state): proposed `BattlegroundSM::GetCartBarStatus(uint8 cart)` and `GetTrackSwitchOpened(uint8 track)`, read-only like the Eye of the Storm getter; not requested tonight |
| BT-608 | A player within 22 yd of two carts counts for the **first of the list** only (the `break`); the three carts are within 22 yd of one another at the pit for the first seconds, so a bot between them at the start counts for the east cart |
| BT-609 | The keeper at the dock is read **3 s after the cart stops**, not at the stop: a late flip inside the 3 s is possible only with a bar already near 50 |
| (to check live) | The cart walk speed (2.0 yd/s is inferred from 0.8 of the base 2.5); the cast time and the range of the switch click (spell 124492); whether a bot mounts in the starting area; the fall into the mesh pockets |

## 18. Navmesh coverage of map 727 (the mmap gap)

`data/mmaps/` has **four of the nine tiles** of the 3 by 3 neighbourhood of the map files (`0727_30_30`, `30_31`, `30_32`, `31_31`; missing `29_30`, `29_31`, `29_32`, `31_30`, `31_32`). Parsed offline (the Detour header and polygons; world x is the tile's second axis, `x = rcZ`, `y = rcX`, `z = rcY`):

| Tile | Polygons | World extent of the vertices (x, y) | What is there |
|------|----------|-------------------------------------|---------------|
| `30_31` (the map) | 1104 | x 533 to 973, y 0 to 533, z 276 to 403 | **everything that is played**: the pit, the platforms, all rails, all four depots, both switches, all four buffs |
| `30_30` | 99 | x 712 to 793, y 533 to 581 | the northern end of the Troll side (beyond y 533), outside the objectives |
| `31_31` | 37 | x 469 to 533, y 62 to 424 | the western rim of the waterfall end |
| `30_32` | 2 | x 875 to 906, y -3 to 0 | a sliver at the Diamond end |

Result (verified by the parse): the playable area (x 533 to 973, y 2 to 533) lies **inside the one complete tile**; the five missing tiles hold no geometry that a player can stand on (they are the map files' empty neighbours), so **there is no hole in the bots' paths**. Details:
- The main component has **799 polygons** and contains the pit, the four depots, the four buffs, both junctions and both switch NPCs (the nearest mesh vertex is 1.7 yd from the east switch and 1.9 yd from the north one, so bots walk to the NPC and click from 3 yd).
- **Two start islands** (24 and 26 polygons at z 329) are **disconnected** from it: a bot cannot path off its platform. The old take-off and landing points are still within 1 yd of the mesh: Alliance take-off (823.0, 144.7, 329.0) and landing (817.0, 145.5, 320.7), Horde take-off (659.8, 229.8, 329.0) and landing (665.5, 229.0, 320.7). **Design**: the plan's `moveToStartSM` keeps the bots on their platform and `atObjectiveSM` jumps them down at the gates and after every resurrection.
- **Two pockets** are disconnected from the main component: 137 polygons at z 314 to 318 (x 805 to 968, y 168 to 422) and 69 polygons at z 276 to 287 (x 540 to 600, y 41 to 150, the lava floor under the Lava depot). A bot that falls there has no path back (to check live; the stuck-bot code deals it again).
- **Rails against the mesh**: every waypoint of the seven paths is within 9.9 yd of the main component (nine waypoints are 8.1 to 9.9 yd away: the east and north routes to Diamond, the waterfall route at its 18th point, the north first leg at its 8th point); with a circle of 22 yd every cart is always in reach of walkable ground. Where the rail runs above the floor by more than 12 yd the bot targets a mesh point ahead.
- **Consequence for the design**: the bot's target is **a mesh point near the cart's rail position plus a lead of the walk's time**, never the cart's raw position (`SmLogic::RailPointAhead`), and never anything on the islands or the pockets.

## 19. As built (BT-610 to BT-613, 2026-11-19)

Where the code lives: the pure rules `src/Utils/SmPlanLogic.h` (rails, bar model, options, counts, deal, switches, stance, leash, words); the plan `src/strategy/bg/SmPlan.{h,cpp}` (record, roster, carts, bodies and the bar model per team, the leader's turn, the jobs, the calls, the decision lists);
the tactics `src/strategy/bg/BgTacticsSM.cpp` (`moveToStartSM`, `selectObjectiveSM`, `smTick`, `atObjectiveSM`, `atSwitchSM`); the reader words `BgCalloutPolicy.h` (`MapKind::SM`); the decision points `BgLeaderPolicy.h`, `AiPvpDecisionPolicy.h`
(D18 and D19, the book of 21), `BgLeaderAdapters.h` (`CandidateName`); the chat rows `sql/playerbots/updates/db_playerbots/2026_11_19_02.sql`; the tests `tests/test_sm_plan.cpp` and `tests/test_sm_leader.cpp`. The compile flag `BOT_HAVE_BG_SM` is gone, and so is the placeholder header
`src/compat/BattlegroundSM.h`, which hid the real class from every file of the module.

| Gap | Ledger | Done |
|-----|--------|------|
| S1 | BT-610 | the rails with ride, junction and remaining seconds, the point ahead and the meeting point; the bar model and the flip seconds; take, flip, guard and skip as a scored list with the opening's first-arrival bonus; counts by size and stance; the deal; switches; stance; leash; words |
| S2, S3 | BT-611, BT-612 | the record (a refresh every 2 s by the first bot that asks, no new scan); the main cart with a hysteresis of 40 points; one off bot per off cart; the peel; one switch holder per team; the walk to the meeting point on the rail, the stay within 8 yd, stealth and mount left inside the circle; the jump off the platform; the click through `Unit::HandleSpellClick`; the cancel of a wrong toggle; the leash |
| S4 | BT-604, BT-613 | reader words, rows, calls, D18 and D19 published and applied, target order by tier |
| S6 | BT-614 | waiting for the live test |

**Where the code differs from the text above (choices the tests pin, and what is not built):**
- **Not built**: the FLOATER job (section 4); the off bot's walk-away from a group (section 6 rule 2; the peel is built); the regroup at the pit after a wipe (rule 4: the dead and the survivors are dealt again by the cart list and a "wiped" call is said, nobody is held back to gather);
  the casual bot that "arrives alone 30 percent of the time" and the casual heed of 65 percent of the reports (the reports go into the team's counts for everybody; casual bots are late through the tier lag and drift); the expert's early "inc" at 60 yd (an "inc" is said when the team counts two or more
  enemies at a cart); a stealth approach of the switch holder (stealth is only left, never started); the class jobs of section 9 beyond the off fit, the target order and the stealth rule (slows, kicks and crowd control are the shared class strategies).
- The claim bonus (60 points) is added to the cart for the whole team, not for good bots only, because the plan is one list per team.
- The first-arrival bonus of the opening is not the "25 points" of the first draft but 2.5 points a second of lead in the meeting time with the cart from the two start platforms (at most 10 s either way): +20 for the Alliance at the north cart, +25 for the Horde at the waterfall cart, 0 at the east cart; it counts for the first 90 s while the cart is near its start.
- The plan's refresh also runs before the gates open (carts are spawned then): the jobs exist at the first tick, bots stay on the platforms until the gates open.
- The main cart ties: with the default switches the north cart scores 326, the east cart 299 and the waterfall cart 274 before the walk; the Alliance (bonus at the north cart) goes north, the Horde (bonus at the waterfall cart, 281 against 278 and 277) goes to the waterfall cart. So at the start the two main groups
  do **not** meet; each takes the cart it reaches first and the off bots meet at the east cart and the other. The flip option then sends a main group to the enemy's cart when the bar can be turned before it docks. To check live whether this reads as play.
- A switch holder is allowed to be a member of the main group when it rides the cart of that track; the duty does not take a bot from the main group otherwise unless it is the nearest eligible one (the score of `PickSwitchHolder`).
- The decision layer's answer to D19 only replaces the rule's act while a holder exists; D18 only replaces the main cart for a cart that scores.

**To check live (lane C, the SSM scoring test and a 5 v 5 or 10 v 10 of test bots at level 90):** the cart speed (2.0 yd/s inferred) and so the ride times; the cast time and range of the click (spell 124492) and whether `HandleSpellClick` works for a bot at 2 to 4 yd from the NPC;
the jump-down points; whether bots find standing places inside the circle on the three spots of the rails that are 8 to 10 yd from the mesh; the first 90 s (who goes where, the two main groups missing each other); the stop-and-go of a bot that follows a rolling cart (it re-targets when it stops 8 yd behind); the leash (whether `AttackStop` holds against the combat engine);
the call-outs (rows, rates); the scoreboard credits for bots; a dock paying 150 to the keeper at that moment; the stance changes; the pockets of the mesh (a fall that leaves a bot stuck); and the `[BGSM]` lines (`state`, `keeper`, `cart-new`, `cart-gone`, `switch`, `switch-duty`, `switch-click`, `switch-cancel`, `leash`, `jump-down`, `BGSM-LEAD`, `BGSM-CALL`).
