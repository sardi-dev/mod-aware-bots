# Warsong Gulch playbook

Status: **owner approved, implemented (BT-153, 2026-11-12), first live runs 2026-11-17 (section 17): flag play works, graveyard stalls and a queue leftover found and fixed on `fix/wsg-live-2026-11-17`, not yet re-verified live. Carrier rule (owner, 2026-11-17: the carrier runs home at once, the healers follow it) implemented on `fix/wsg-carrier-runs-home`, syntax checked and unit tested, not yet run live (section 6 "Carrier rule", measurements in section 14)**. Queueing (the Random Battleground conflict and the
human-queue fill) is being fixed in a separate run and is treated here as "being fixed".

Legend: **verified** = read in the core, the world data or our code (file and line given); **inference** = derived from
verified facts; **to check live** = needs a match with the headless client. Rules are marked core, good or expert like
the class sheets: casual bots follow core rules only and react late.

## 1. Header

| Item | Value | Source |
|------|-------|--------|
| Map id, battleground type id | 489, 2 (`BATTLEGROUND_WS`) | `battleground_template` row 2, `BattleMasterList.dbc` |
| Team size | 10 v 10; template minimum 5 per team | `battleground_template` MinPlayersPerTeam 5, MaxPlayersPerTeam 10 |
| Brackets (5.4.8) | five levels wide: 10-14 (index 0), 15-19 (1), 20-24 (2), 25-29 (3), 30-34 (4), 35-39 (5) and so on in fives up to 80-84 (14), 85-89 (15), 90 alone (16) | `PvpDifficulty.dbc` rows for map 489, read 2026-11-12 |
| Owner's band | levels 15 to 30 = brackets 1, 2, 3 and the first level of 4 (30-34) | owner |
| Win | first team to **3 flag captures** | `BG_WS_MAX_TEAM_SCORE` 3, `BattlegroundWS.h:13` |
| Time limit | the match ends when the instance clock reaches **27 minutes**; the clock also runs during the 2 minute preparation, so play lasts about **25 minutes** after the gates open | `BattlegroundWS.cpp:52`, `Battleground.cpp:247`, `BG_START_DELAY_2M` |
| Timeout result | more captures wins; equal and above 0: the team that captured last; 0-0: draw | `BattlegroundWS.cpp:52-69` |
| Carrier debuffs | only while both flags are carried at the same time: **Focused Assault after 10 minutes, Brutal Assault after 15** (details in section 3) | `BattlegroundWS.cpp:123-165` |
| Early end | a team below 5 players for the premature-finish timer (core default 5 minutes, live config not read) ends the match for the higher score | `Battleground.cpp:230`, `World.cpp:1076` |
| Sources | core `BattlegroundWS.{h,cpp}` in the core; world DB and client DBC read-only; our tactics; the game wiki Warsong Gulch page (read 2026-11-12). Wowhead, Icy Veins, Method, Wowpedia were tried and did not answer (404, 403, 402), so the wiki page is the single guide source. Notes: `.agents/plans/bots-bg-sheets/sources/` | |
| Richer source | the **server's own data** by far (numbers, positions, spell values); the wiki adds only role vocabulary, plans and class tips | |
| Status | owner approved 2026-11-12; code for G1 to G10 written (BT-153), live test (G11) pending | |

Where the guide and the server differ, the server wins: the wiki says 20 minutes, a 20 s flag reset, a 5 s dropped-flag
return and a stacking +10 percent debuff from the 1 minute mark; this core uses 25 minutes of play, 23 s, 10 s and a
single +10 percent step at 10 minutes.

## 2. Map and positions

Alliance holds the east end (x about 1500-1540), Horde the west end (x about 920-950). Coordinates are map 489.

| Thing | Alliance | Horde | Source |
|-------|----------|-------|--------|
| Flag stand (GO 179830 / 179831) | 1540.42, 1481.33, 351.83 | 916.02, 1434.41, 345.41 | core `SetupBattleground` (`BattlegroundWS.cpp:658-659`) |
| Capture trigger (radius 5) | 3646 at 1539.89, 1481.36, 352.66 | 3647 at 918.50, 1434.04, 346.05 | `AreaTrigger.dbc`, `BattlegroundWS.cpp:631-640` |
| Start and preparation graveyard | 769 at 1523.81, 1481.76, 352.01 | 770 at 933.33, 1433.72, 345.54 | `WorldSafeLocs.dbc` |
| Graveyard while the match runs | 771 at 1406.65, 1564.74, 328.33 | 772 at 1061.60, 1377.09, 328.56 | `WorldSafeLocs.dbc`, `GetClosestGraveYard` |
| Gates (closed for the 2 minute preparation) | x 1468-1503, y 1457-1494 | x 949-953, y 1422-1460 | `BattlegroundWS.cpp:667-678` |
| Speed buff (GO 179871, spell 23451) | 1449.93, 1470.71, 342.63 | 1005.17, 1447.95, 335.90 | core, `AreaTrigger.dbc` 3686 / 3687 |
| Restoration (GO 179904, spell 23493) | 1317.51, 1550.85, 313.23 | 1110.45, 1353.66, 316.52 | core, triggers 3706 / 3708 |
| Berserk (GO 179905, spell 23505) | 1320.09, 1378.79, 314.75 | 1139.69, 1560.29, 306.84 | core, triggers 3707 / 3709 |

Distances (inference from the coordinates and from our waypoint paths in `BattleGroundTactics.cpp:286-452`): the two
flag stands are 626 yd apart in a straight line; the walkable tunnel route between them is about **850 yd**, so an
unbuffed carrier (7 yd/s) needs about **2 minutes** from pickup to capture; the speed buff (+100 percent for 10 s) saves
about 70 yd.

Map vocabulary used in call-outs (wiki, matched to our paths): **FR** flag room, **GY** the main graveyard of a side,
**tun** the tunnel between the base area and the middle, **ramp** the side opposite the GY, **roof** the area over the
flag room reached from the tunnel (our `..._to_HordeBaseRoof` / `AllianceBaseRoof` paths), **mid** the middle valley
(our `WS_ROAM_POS` 1227.4, 1476.2, 307.5).

The buffs sit on the carrier's lane: the **Horde speed buff (1005, 1448) is 90 yd from the Horde flag stand** and lies
on the Horde tunnel path, so an Alliance carrier passes it on the way out; the Alliance speed buff (1450, 1470) does the
same for a Horde carrier. Restoration and Berserk lie in the valleys on each side of mid.

Our static data today: waiting spots, flag, hide and roam points are `WS_*` constants (`BattleGroundTactics.cpp:62-81`);
13 waypoint paths `vPaths_WS` (`:1367-1381`); ledge jumps `wsJumpDown` (`:1833`). Waypoints are read from the old path
set; reachability on the 5.4.8 navmesh is claimed in the comment at `:1365` and is **to check live** for the roof and
hide points.

## 3. Mechanics that matter for play (all verified in `BattlegroundWS.cpp` unless stated)

**Flags**
- Pickup: click the enemy flag object while its state is on base (`:447-482`). The click breaks stealth and drops
  nothing else. Dropped flag (GO 179785 Alliance, 179786 Horde): the owner's team clicks it within 10 yd to **return it at
  once**; the enemy team clicks it to pick it up again (`:484-552`).
- The dropper gets "Recently Dropped Flag" (42792, 3 s) and cannot pick it up again at once (`:420`).
- A dropped flag **returns by itself after 10 s** (`BG_WS_FLAG_DROP_TIME`, `:88-98`).
- The carrier drops the flag on death or leaving the match (`:755-763`, `:561-586`); Stealth, Divine Shield and similar
  drop it too (wiki; our rotation guard "condition 46" already blocks Stealth and Divine Shield for carriers,
  `BattleGroundTactics.cpp:5266`). Mounting drops it (wiki; our mount code never mounts a carrier, `CheckMountStateAction.cpp:213`).
- **Capture needs your own flag on its stand**: the carrier must reach the trigger at its own flag stand and its own flag
  state must be "on base" (`:631-640`). If the enemy carries your flag, your carrier cannot score. Corollary used in the
  plan: **taking their flag blocks their capture too** (their flag is off its stand).
- After a capture both flags respawn after **23 s** (`:311-312`, `:346`); nobody can pick one up during that time.

**Carrier numbers (spell data, inference where marked)**
- Flag auras 23333 / 23335 (no duration): damage taken **+20 percent** (aura 87), run speed capped at **14 yd/s** (aura 191
  value 14, `Unit.cpp:4563-4569`). So the speed buff (+100 percent, 14 yd/s) is useful to a carrier; mounts are not allowed.
  To check live that the +20 percent shows on a real carrier.
- Both flags carried at once: after **10 minutes** both carriers get Focused Assault (46392, +10 percent damage taken); after
  **15 minutes** it is replaced by Brutal Assault (46393, +10 percent damage taken and run speed capped at **7 yd/s** through
  linked spell 98507). The clock restarts when either flag returns to its stand (`:123-165`). Neither effect stacks.
- Power-ups (180 s respawn): Speed +100 percent run speed 10 s; Restoration heals 10 s and breaks on combat; Berserk +30
  percent damage done and +10 percent damage taken for 60 s. None drops the flag (wiki).

**Dead players**: respawn at the main graveyard (771 / 772) in **30 s waves** (`RESURRECTION_INTERVAL`); during the
preparation at the flag-room graveyard. Both main graveyards are about 155 yd (straight line) from their flag stands and a walk through
the base area away (graveyards were lowered in 4.1, wiki), so a dead defender needs roughly 20-30 s to be back at the flag
room (inference).

**Clocks to plan with**: dropped flag 10 s; recently dropped 3 s; flag reset 23 s; carrier trip about 2 minutes; buffs 180 s;
resurrection wave 30 s; debuffs 10 and 15 minutes of both flags held; match 25 minutes of play.

## 4. Roles by side size

Roles are **assigned**, not rolled. One plan per team, held in a per-match team record, recomputed at the events in the
last column of the table in section 6. Roles for a 10-player team (scale by rounding for 5 to 9; a human on the team
takes a slot but is never commanded):

| Role | Balanced | Offense plan | Defense plan | Job |
|------|----------|--------------|--------------|-----|
| TAKER (T) | 1 | 1 | 1 | Takes the enemy flag and carries it home. Best fit: stealth or mobile, high survival, highest level in the team (the bracket spans five levels) |
| BACKUP (B) | 1 | 1 | 0 | Second pickup; takes the flag when T dies in the enemy base |
| ESCORT (E) | 2 | 4 | 2 | Moves with T inside 12 yd; peels attackers off T; one is a healer when the team has one; takes over the flag when T is down |
| MID (M) | 3 | 3 | 2 | Holds the middle valley and the exits of both tunnels, kills the enemy carrier (EFC) when it passes, cuts off chasers behind our carrier |
| HOME (D) | 3 | 1 | 5 | Holds our flag room (two posts at the entrances) and returns dropped flags; chases the EFC only inside 40 yd of our flag stand |

Balanced matches today's 3 defenders (`defendersProhab = 3`, `BattleGroundTactics.cpp:2777`). The offense plan matches the
old "1 defender", the defense plan the old "6" (here 5 because 1 TAKER always goes). The plan is drawn once per match per
team with weights 20 offense, 60 balanced, 20 defense (the old code drew uniformly; owner question 2).

**Assignment** (deterministic, so tests can pin it; the score is a list of candidates per role so a later optional decision layer can
pick among the top ones):
1. Only bots take part; a human in the team is skipped and counted as a slot (if the human is the FC, see section 6).
2. `score(bot, role)` = class and spec fit + level rank in the team + tier + start-room distance tie-break:
   - TAKER fit: stealth class (rogue, feral druid) 3, mobility or self-heal class (druid, warrior, paladin, hunter) 2,
     other melee 1, healers and casters 0; add survival (plate or leather agility > cloth).
   - ESCORT fit: healers first (one on the FC), then melee with a stun or peel; MID fit: ranged damage with slows, roots
     and interrupts; HOME fit: ranged damage and CC, because they fight at the choke.
   - Tier: expert fits TAKER and MID first, casual bots take HOME and MID last in line, because their reaction delay is
     largest on the carrier (tier effects in section 11).
3. Fill TAKER, then BACKUP, then HOME, ESCORT, MID by best score; ties by GUID hash (stable, like the skill tiers).
4. **Reassignment** when a bot dies (its role goes to the best spare in the same group at respawn), when the carrier
   dies or drops (nearest ESCORT or BACKUP becomes TAKER or picks the flag), when the plan changes (section 6), when a
   bot joins or leaves, and never while a bot carries a flag. No random re-roll: today a bot re-rolls its role about 2 percent
   of the time at every objective refresh (section 12, G1).

## 5. Opening plan (gates open at 2:00 after the match starts; first 60 to 90 s)

Same for both factions; only coordinates mirror (Alliance leaves west, Horde east). During the preparation bots wait at
the three start spots and mount inside the last 30 s only from level 20 (existing, `CheckMountStateAction.cpp:229-240`).

| Time after gates | TAKER, BACKUP, ESCORT (strike group of 4) | MID (3) | HOME (3) |
|------------------|--------------------------------------------|---------|----------|
| 0 to 15 s | leave together by the tunnel route; stealth classes go 10 s ahead | speed to the middle valley; one takes the nearest power-up (Restoration or Berserk on our side; healers skip Berserk, as today) | two to the entrance posts of the flag room, one to the roof access or ramp |
| 15 to 60 s | arrive at the enemy tunnel exit together; T and B wait 5 yd behind the group, not in front | hold mid on both sides of the lane, line of sight to our tunnel exit and to their tunnel exit | stay; nobody leaves for a kill |
| 60 to 90 s | enter, kill the guards at the flag stand, **T takes the flag**; E keeps the room clear; the speed buff on the way out is taken by T | kill the first enemies that come through mid; if their carrier appears, section 6 state "they carry" | return any dropped flag at once (click within 10 yd) |

Enemy defenders usually meet the strike group at the stand; the strike group fights at the stand only until T has the flag,
then leaves (the flag, not the kills, is the objective). **Casual** bots may chase kills at the stand; that is part of being
beatable.

## 6. Mid-game rules

State of the flags decides the plan; the table lists what each role does. State is read from `BattlegroundWS::GetFlagState`
(on base, wait respawn, on player, on ground) and `GetFlagPickerGUID` per team, plus score and elapsed time.

| State | Event that triggers it | TAKER, BACKUP, ESCORT | MID | HOME |
|-------|------------------------|-----------------------|-----|------|
| A both flags home | opening, after any return, after the 23 s reset | strike group walks to the enemy FR | hold mid | hold FR |
| B we carry, ours home | our pickup | T runs home at once by the shortest safe route (see "Carrier rule" below), speed buff if within 20 yd of the lane; E within 12 yd, the two nearest healers follow T (15 yd) and heal it first; B stays as the next carrier | go to the carrier's lane, kill what chases T, never leave the lane to chase | hold; kill an enemy taker if one arrives |
| C they carry ours, theirs home | enemy pickup | B and one E take **their** flag at once if it is free (blocks their capture, then hold hidden near our side of mid); T and one E join the hunt | hunt the EFC: the nearest 2 attack, the rest cut its lane | the 2 nearest engage inside 40 yd of our stand, others hold |
| D both carried | second pickup | carrier hides at a hide post, E guard it; do not stand at a flag stand | 6 of the team hunt the EFC with focus fire and crowd control on it | 2 stay home |
| E flag on ground | a drop | the nearest 2 within 20 yd of it: ours returns (click), theirs gets picked up by T or B after the dropper's 3 s | one to the drop, one covers | the nearest returns ours at once |
| F post capture, 23 s | a capture | regroup, heal, take the nearest power-up, walk to the enemy FR so as to arrive as the flags respawn | hold | hold |

**Carrier rule (owner, 2026-11-17)**: a carrier runs home at once and does not hold the enemy stand to fight. Implemented in
`src/strategy/bg/WsgCarrier.{h,cpp}` (facts from the core) and `WsgLogic::CarrierOptions` / `ChooseCarrier`
(`Utils/WsgPlanLogic.h`, pure and tested), asked by `BGTactics::FightPathAttacker` and `CarrierHoldsToFight` for map 489 (Twin
Peaks keeps the older single-attacker window until its sheet). Each decision is a scored option list, so a later decision layer
can pick among the same options; the rules are the fallback.

| Choice (score) | When | Tier |
|----------------|------|------|
| run (100, the default) | nobody on the carrier, no hit for 8 s, two or more attackers, any other enemy within 35 yd, a fighter of ours within 12 yd, or health below 40 percent | core |
| run near the capture (950) | within 25 yd of its own flag on the stand: never stops (a rooted carrier still fights) | core |
| fight rooted (960) | root, stun or fear and at least one attacker: it cannot run | core |
| fight slowed (800) | run speed rate under 0.85 and at least one attacker, not near the capture | core |
| fight alone (600) | exactly one attacker, no other enemy player within 35 yd, no fighter of ours within 12 yd (they peel), health 40 percent or more (casual bots 25 percent, so they fight on a little longer) | core, every tier (owner 2026-11-17: casual carriers run at the enemy stand too) |

While it runs, the carrier helps itself once a second at most (core rule, `WsgCarrier::UseHelpers`): a freedom spell when
rooted or slowed with an attacker on it (Hand of Freedom, Spirit Walk, Burst of Speed, Dash), a class defensive when attacked
and below 65 (expert), 55 (good) or 40 (casual) percent health (Shield Wall, Die by the Sword, Enraged Regeneration, Divine
Protection, Deterrence, Evasion, Desperate Prayer, Icebound Fortitude, Anti-Magic Shell, Astral Shift, Ice Barrier, Unending
Resolve, Fortifying Brew, Barkskin, Survival Instincts) and a run-speed spell while chased (Sprint, Burst of Speed, Speed of
Light, Spirit Walk, Death's Advance, Dash). Spell ids were checked against `Spell.dbc` names. **Never used**: Divine Shield, Ice
Block, Hand of Protection (they drop the flag, wiki). **To check live whether they drop it, then add**: Cloak of Shadows,
Dispersion. Aspect of the Cheetah is left out (a struck hunter is dazed). The route home is unchanged: the navmesh path to
the own stand within 100 yd, else the authored `vPaths_WS_Carrier` tunnel route (**to check live**; "safe" is the tunnel, not the
graveyard shelf).

**Healers follow the carrier (owner, 2026-11-17)**: while our team carries, the two nearest alive healer bots, whatever their
role, take the plan option `heal-carrier` (905: below returning or picking up a dropped flag, above every phase duty) and trail
the carrier at 15 yd (casual 20). A healer of the home guard joins only when the carrier is within 80 yd of it. A following
healer closes up in a fight too (leash 24 yd, it closes to 10), stops to heal a carrier under 90 percent within 28 yd, and
never peels attackers. The heal order (carrier first, `HealPriority`) is unchanged. State D (both flags carried) does not
trigger the in-combat close-up (`TeamFlagCarrierNear` is off when both flags are away from their stands); the healer then follows
by its objective only (**to check live**).

**Score and time** (use `elapsed` = `bg->GetElapsedTime()`, `left` = 27 min minus `elapsed`; deterministic thresholds):
- Ahead 2-0: switch to the **defense plan** (strike group of 3). Do not park the whole team on the enemy graveyard as
  today (`BattleGroundTactics.cpp:2828-2836` sends 80 percent of the team there); at most **3** MID bots hold the enemy
  tunnel exit and the rest stay home (owner question 1).
- Behind by one or two with `left` > 10 min: normal plan. Behind with `left` < 5 min: all-in, HOME down to 1.
- Equal captures: the **last capture wins** at timeout (`:63-64`), so with `left` < 4 min the team that is level strengthens
  both: strike group to 5 and HOME to 3. A capture after the 23 s reset and a trip of 2 minutes is decisive only if the
  enemy cannot answer; so below `left` of 2.5 minutes a carrier does not hide, it runs.
- Ahead by one with `left` < 3 min: HOME 5, MID hold the enemy tunnel, strike group of 2 only to deny their flag.
- **Stalemate breaker (both flags carried, state D)**: from 5 minutes of state D on, expert and good bots hunt the EFC with
  all but 2; at 9 minutes the carriers stop hiding and move toward their own stands anyway, because Focused Assault
  (+10 percent damage taken) starts at 10 minutes and Brutal Assault (run speed 7 yd/s, no speed buff benefit) at 15; do
  not trade the carriers into a stalemate that the debuffs then decide for the stronger team (inference).

## 7. Comp-dependent choices

- **No healer in the team**: E becomes 3 melee bodies, T prefers the speed buff and the Restoration power-up, and MID
  keeps one bot at the restoration spot of our side to pull wounded bots back.
- **Two or more healers**: one heals T only (target priority: carrier, then the lowest ally); the second stays with MID so
  that the lane is held. Never more than two healers in the strike group.
- **Stealth classes (rogue, druid)**: take TAKER and BACKUP; go 10 s ahead of the strike group, wait at the tunnel exit
  for the group, do not take the flag alone while 3 or more enemies stand at the stand.
- **Many casters**: MID holds the tunnel exits and ramp with crowd control and interrupts; HOME stays at the posts; the
  strike group uses a melee ESCORT pair as the front.
- **Level spread**: the bracket spans five levels; the highest-level bots take TAKER and MID, the lowest HOME or ESCORT
  next to a healer (wiki: do not send low levels against higher-level guards).
- **Few bots, more humans** (bots only fill): the plan scales by rounding; with 5 to 6 players per team it is one HOME,
  one MID, the rest strike.

## 8. Call-outs (battleground chat)

Short lowercase lines through the bot voice (`BotVoice.h`, text table rows `bg_<event>` with a variant per personality,
same design as `DungeonChat`). Humans read them; bots do not parse each other (bots read state, not chat).

| Event | Who says it | Example lines |
|-------|-------------|---------------|
| Our flag taken | the first HOME or MID bot that sees the EFC | `efc at tun`, `they got our flag, <class> carrying` |
| EFC position changes zone | one MID bot, at most once per 10 s | `efc mid`, `efc ramp` |
| We take their flag | T | `got it, heading home`, `escorts on me` |
| Carrier hurt (below 40 percent) | T or the nearest ESCORT | `fc low, heals`, `need heals on fc` |
| Flag dropped | nearest bot | `flag down at roof`, `ret!` after the return |
| Enemy group at our FR (3 or more within 40 yd) | nearest HOME bot | `3 inc fr`, `need help fr` |
| Capture | nearest bot | `cap 1-0`, `cap! one more` |
| Time and score | one expert bot | `3 min left, 1-1, last cap wins` |
| Death at an objective | the dying bot (casual only) | `lol` or nothing |

Rate limits: one line per team every 6 s, at most 8 lines a minute per team, 20 s per bot between its own lines; expert
bots call 100 percent of the events, good 70, casual 30 (and sometimes late or wrong, on purpose). The channel is the
battleground group chat; 5.4.8 has no separate battleground chat type in the core's `ChatMsg`, instance chat (0x42) is used
for battleground groups (`ChatHandler.cpp:56`, `:592-640`), and our group chat helper uses raid chat
(`PlayerbotAI.cpp:3159`). Which one the client shows is **to check live** (owner question 3).

**Human calls** (BT-482): a human's short line ("inc fr 3", "help", "efc mid", "fc") is read as information, never as a
command, by the shared reader (`docs/bg-sheets/twin-peaks.md` section 8 lists the rules). "inc", "help" and "efc <place>" get a
one-bot answer ("omw") from the nearest HOME or MID bot that heeds the line (a casual bot may ignore it); a bare "help" is
read at the sender's position. A human flag carrier is treated as the team FC: the strike group escorts it (the existing
`team flag carrier` value already finds it).

**Leader and scouts** (BT-481, BT-483): one leader bot per side confirms the stance of the plan and says it at the gates, at
every change and after a handover, plus the time and score; an expert bot within sight of an enemy group of 3 or more on our
half of the map says "3 inc mid" before it reaches the base. Bots do not parse each other's lines: what a bot says is kept as
an observation of its side, the plans still read the match state.

## 9. Class jobs on this map

Class sheets own the spell lists; this table says what each class does for the plan (guide facts from the wiki are marked W).

| Class | Preferred role | Jobs |
|-------|----------------|------|
| Rogue | TAKER, BACKUP | Stealth up to the stand (W), Sprint on the way out, Kick the enemy healer, Blind or Sap a defender at the stand. Stealth and Vanish drop the flag (W): never while carrying |
| Druid | TAKER, BACKUP, HEALER | Cat stealth into the base (W), shapeshifts (cat, travel) do not drop the flag (W); heals itself on the run; Entangling Roots on the EFC |
| Warrior | TAKER, ESCORT, MID | Charge and slows on the EFC, Intervene or Heroic Leap to a carrier in trouble |
| Paladin | ESCORT, TAKER | Hand of Freedom on T, Hammer or Stun on the EFC; Blessing of Protection and Divine Shield drop the flag (W): not while carrying |
| Priest | ESCORT (healer), MID | Shield and heal T first, Dispel Magic on roots and slows on T, Fear or Silence on the EFC |
| Shaman | ESCORT, MID | Wind Shear on casters, Earthbind or Grounding totem on the lane (W: totems as an escape path), Ghost Wolf does not drop the flag (W) |
| Hunter | MID, HOME | Concussive slows and Freezing Trap on the EFC lane or on our flag stand (W), Disengage; Aspect of the Pack for the strike group (W) |
| Mage | MID, HOME | Frost Nova and slows on the EFC, Polymorph on a defender (never the carrier: damage breaks it), Ice Block drops the flag (W) |
| Warlock | HOME, MID | Fear and slows on the EFC, kills the taker at our stand; high burst |
| Monk | ESCORT, TAKER | Roll to gap-close and escape, Paralysis on a defender, Detox on T |
| Death knight | MID, ESCORT | Only in brackets from 55 (class starts at 55, inference): Death Grip and Chains of Ice on the EFC |

Dispel on carrier debuffs does not apply (Focused and Brutal Assault have no dispel type, to check live).

## 10. Failure modes and what the bots do

| Failure | Cause | Required behavior |
|---------|-------|-------------------|
| Stuck at doors, ledges, bad hide spots | mmaps do not include doors; hide points on the roof | keep `wsJumpDown` and the start-area pull back (`BattleGroundTactics.cpp:2246-2319`); every hide and roam point must be reachable on the navmesh (**to check live**) |
| Carrier stalls at the stand, or fights too long | a carrier hit repeatedly | the carrier rule of section 6 (run at once; fight only rooted, slowed or caught alone; `WsgCarrier.cpp`) |
| Whole team chases kills off the objective | random role and "attack nearest" | role plan: HOME and MID may only leave their zone for the EFC |
| All defenders gone to mid | defenders "roam centre" (`:2877-2881`) | HOME holds two posts at the flag room; chase limit 40 yd |
| Flag room empty while the enemy takes the flag | every bot attacks the EFC after the pickup | rule C above: 2 nearest only |
| Dropped flag lies unattended | no objective for a dropped flag (`:2848-2907`) | state E: nearest 2 within 20 yd |
| Carrier waits for a cap that cannot happen | own flag is taken | hide (existing, `:2837-2847`), plus kill the EFC |
| Graveyard camping all game | 2-0 lead sends 80 percent to the enemy GY (`:2828-2836`) | at most 3 bots, owner question 1 |
| Bots mount while carrying | mount drops the flag | blocked today (`CheckMountStateAction.cpp:213`); keep |
| Bots wait for a debuff that decides the game | 10 minute Focused Assault | stalemate breaker (section 6) |
| Falls and drowning | hide on the roof | `wsJumpDown`; check hide points |
| Spawn camping humans | repeated killing at 771 / 772 | not an objective; the cap above applies |
| Respawned bots stand at their own graveyard | live probe 2026-11-17 (`.mmap path`): from 771 and 772 the navmesh has a normal path to the side's tunnel exit and to mid, none (type 8) to the old graveyard shelf waypoints, the flag room posts or the stand | a bot at its graveyard whose objective has no path from there walks to its tunnel exit first (`WsgPlan::GraveyardHop`, `BGTactics::wsgLeaveGraveyard`) |

## 11. Skill tiers on this map

Today bots in a battleground are **not** tiered: `BotSkill.cpp:50-52` returns expert (no extra delay) for any bot in a
battleground or arena. The owner wants the 10 / 60 / 30 mix to hold here too (owner, 2026-11-12), so the sheet maps rules
to tiers; tier delay values stay the config values (`AiPlayerbot.SkillTiers.*`, expert 0 ms, good 150-400 ms, casual
400-900 ms).

| Tier | Rules it plays |
|------|----------------|
| **Core** (casual and up) | walk its role's lane; take the flag when at the stand; run home with the flag by the shortest path; click an adjacent own dropped flag; attack enemies within 40 yd; use the nearest power-up when hurt; follow the plan state when it changes (late) |
| **Good** (good and expert) | escort within 12 yd, peel for the carrier; healers heal the carrier first; MID cuts off chasers; hunters switch to the EFC within 20 s; HOME holds the posts; use the speed buff on the lane; hide when the own flag is taken |
| **Expert** | focus fire and crowd control on the EFC (stun, root, slow, fear on the carrier, never polymorph); time the pickup with the flag reset; leave the stand the moment T has the flag; kill timing against the 3 s recently-dropped window; stalemate breaker and the last-minute rules; accurate call-outs of position and class; intercept a carrier by route rather than by position |

What casual bots get wrong on purpose: react 0.4 to 0.9 s late to a flag change; chase one more kill at the stand; walk
alone to the stand; skip the call-out; return a flag late; sit on the power-up spot; fall one step behind the carrier.

## 12. Bots today (verified against the code), with gaps

As built: strike roles on a take-flag objective continue in combat outside 30 yd of the enemy stand at 40 percent health or more (25 percent for casual), unless rooted or held by more than one melee attacker. At the stand they fight as usual. This also applies in Twin Peaks; live movement remains to be checked.

| # | Rule or fact | Where | Status |
|---|--------------|-------|--------|
| 1 | The team split comes from `bg role` = `urand(0, 9)` per bot at queue accept, and every objective refresh re-rolls it with 2 percent odds (about once every 10 s: "often" is a 1-in-5 random trigger); a bot is a "defender" when `role < 3`. So the defender count per team varies 0 to 6 and drifts | `BattleGroundJoinAction.cpp:1009`; `BattleGroundTactics.cpp:2803`, `:4317-4344`, `:2537-2553` | **wrong** (no assigned roles) |
| 2 | Offense, balanced, defense per team (`WS_STRATEGY_*`, 0 to 2) never reaches `defendersProhab`: the switch tests the values 0-3, 4-7 and 8-9 and offense (1) and defense (2) both fall in the "balanced" case, so only "enemy is defensive" changes the split | `BattleGroundTactics.cpp:2779-2800`, `BattleGroundTactics.h:28-31` | **wrong** (dead feature) |
| 3 | `SetSafePos` uses the random point only when its height equals `VMAP_INVALID_HEIGHT_VALUE`, otherwise the exact origin; the radius is dead, so every roamer stands on the same point | `BattleGroundTactics.cpp:2750-2765` | **wrong** (inverted test) |
| 4 | Defenders: attack the EFC, else 33 percent roam near their own base, else 70 percent escort, else roam mid (75 yd radius, dead) | `:2850-2882` | **wrong** (no flag-room posts; Twin Peaks has them, Warsong does not: `:2872`) |
| 5 | Attackers: 70 percent pursue the EFC, else escort the team carrier, 5 percent roam, else push to the enemy flag; no TAKER, no group, no timing | `:2883-2907` | **missing** (no strike group) |
| 6 | Carrier: run home; hide near own base when the own flag is taken (3 hide spots); **now** runs at once, fights only when rooted, slowed or caught alone, and uses a freedom spell, a defensive and a speed spell (section 6 "Carrier rule"); the two nearest healers follow it | `WsgCarrier.cpp`, `WsgPlanLogic.h` (`CarrierOptions`, `PickHealerFollowers`), `BgTacticsWSG.cpp` `protectFcWsg`, `BattleGroundTactics.cpp` `FightPathAttacker`, `CarrierHoldsToFight` | implemented 2026-11-17, **to check live** (hide spot reachability, 3 hide posts, route home, the spells) |
| 7 | Both carried: 20 percent support own carrier, 80 percent hunt the EFC (re-rolled each refresh) | `:2814-2826` | **wrong** (dithering; no focus) |
| 8 | Lead 2-0: 80 percent of the team (role < 8) camps the enemy graveyard | `:2828-2836` | **wrong** (also anti-fun) |
| 9 | EFC hunting: the "attack enemy flag carrier" action is useful only for a bot that is itself a carrier; non-carriers only walk toward the EFC's position and fight what is near | `ChooseTargetActions.cpp:26-32`, `PvpTriggers.cpp:215-227` | **missing** (no focus fire, no crowd control on the EFC) |
| 10 | Escort: `bg protect fc` moves within 5 yd of the carrier when 20 yd away and not in combat; no peeling, no healing priority | `BattleGroundTactics.cpp:5047-5072`, `BattlegroundStrategy.cpp:74-75` | **missing** |
| 11 | Dropped flag: no objective; only an accidental pickup or return when within 25 yd of the flag object while walking | `:4518-4916` | **missing** |
| 12 | Pickup and capture at the stand: pickup by `go->Use`, capture by calling the core's trigger handler directly (the area-trigger packet is bit-packed); no check for enemies at the stand, no waiting for the group | `:4746-4916` | OK for mechanics, **wrong** for tactics |
| 13 | Power-ups: speed buff only within 20 yd and when the bot carries or no flag is taken; regen when health or mana is below the medium limit; berserk not for healers or carriers; others within 50 yd | `:5074-5129` | OK; **missing** deliberate routing of the carrier over the speed buff |
| 14 | Time and score: no use of the clock, the debuffs or the tie-break; only "2-0 and zero" camping | whole WS case | **missing** |
| 15 | Call-outs: none (no battleground chat code at all; `DungeonChat` is the pattern) | no file | **missing** |
| 16 | Skill tiers: switched off in battlegrounds | `AI/BotSkill.cpp:50-52` | **wrong** against the owner's direction |
| 17 | Objective is a one-shot position; refreshed by arrival (4 yd), path end, combat end, death, the 60 s `timer bg` and the "often" check; a carrier snapshot can be old by up to ~10 s | `:4091-4110`, `BattlegroundStrategy.cpp:57-58`, `:88-89` | OK as a stop gap; replace with state reads |
| 18 | Paths, jumps and waiting spots | `:286-452`, `:1367-1381`, `:1833` | **to check live** on the 5.4.8 navmesh |

The same code runs Twin Peaks (`AiFactory.cpp:975`): items 1 to 5 and 7 to 17 affect both maps.

## 13. Gap list and proposed changes (ordered)

Sizes are changed-line estimates, not measured. Order is value per size and dependency.

| # | Change | Files and functions | Size | Risk |
|---|--------|---------------------|------|------|
| G1 | **Team record and assigned roles**: a per-team plan object (plan draw 20/60/20, role list, state), pure functions for score, assignment and reassignment; replace `urand(0, 9)` and the 2 percent re-roll for WS and TP | new `src/strategy/bg/WsgPlan.{h,cpp}`; `BattleGroundJoinAction.cpp` (role set at accept); `BattleGroundTactics.cpp` `resetObjective`; `tests/test_wsg_plan.cpp` | 250-400 | medium |
| G2 | **Fix the two bugs**: strategy switch (offense and defense count) and inverted `SetSafePos` test; fold into G1 or land first | `BattleGroundTactics.cpp:2750-2800` | 15-25 | low |
| G3 | **State-driven objectives** replacing the WS case of `selectObjective` for WS (TP follows after its sheet): states A to F, strike group, MID lanes, HOME posts, ahead/behind/time rules | `selectObjective` WS case, new `WsgPlan` decision helpers; HOME posts from section 2 | 200-350 | medium to high |
| G4 | **Dropped flag objective and return** (state E) | `selectObjective`, `atFlag` | 60-100 | low |
| G5 | **EFC kill**: non-carrier target priority for the EFC within reach, crowd control and focus (reusing class strategies' interrupt and CC lines), sharing the target in the team record | `ChooseTargetActions.cpp` (`AttackEnemyFlagCarrierAction`), `PvpTriggers.cpp` (`EnemyFlagCarrierNear`), possibly `EnemyPlayerValue.cpp` | 120-200 | medium (shared with TP, DG, EotS) |
| G6 | **Escort behavior**: stay within 12 yd, peel attackers, healer prioritizes the carrier | `BGTactics::protectFC`, a healer target value | 100-180 | medium |
| G7 | **Skill tiers in battlegrounds**: apply the delay and the rule-line gating for bots in a battleground | `AI/BotSkill.cpp` (`HasTier`), `BotSkillMath.h` (no change), BG rules above | 40-80 | low to medium (reaction cost for 20 bots at once) |
| G8 | **Call-outs**: a `BgChat` module like `DungeonChat`, text rows, rate limits, config | new `src/Mgr/Text/BgChat.{h,cpp}`, text table rows, `conf/playerbots.conf.dist` | 250-400 | low |
| G9 | **Carrier route and hide**: route over the near speed buff, avoid the enemy graveyard lane, reachable hide posts | `selectObjective` hasFlag branch, path set | 60-120 | medium, needs live data |
| G10 | **Stalemate and last-minute rules** (clock, debuffs, tie-break) | `WsgPlan` pure helpers + tests | 60-100 | low |
| G11 | **Live path audit**: roof, hide posts, jumps | headless runs only | 0 code unless a point fails | |

Estimated total 1,100 to 1,900 lines with tests, in four steps: (1) G2, G1, tests; (2) G3, G4, G10; (3) G5, G6, G7; (4) G8, G9.

## 14. Tests

Pure-logic unit tests in `tests/` (run with `bash tests/run.sh`, same style as `test_skill.cpp`):
role assignment (counts per plan for 5 to 10 players, stability under a death, tier effect, stealth class first), state
machine (A to F from flag states, ground flag, both carried), score and time rules (2-0, tie at 3 minutes, behind and
ahead), stalemate thresholds, call-out rate limiter.

Live (lead's run, human queue):
- Headless test client (not part of this repository): `python headless.py --cmd play --queue 2 --accept --hold 1800` for a
  25-minute match, a level inside the owner's band (brackets 1 to 4), then the other faction. The scenario harness can seed
  both teams: `.botscenario <anchor> bg 2 <seconds> bots=<20 names> strat=<a>,<h> roles=index|natural` (`BotScenario.cpp` ParseBg).
- Read in the log (debug lines already present): `[BGF]` carrier decisions, `[BGCAP]` flag interaction, `[BGFIGHT]` carrier
  fights, the core's flag messages (pickup, drop, return, capture), world states 1581/1582 for captures, deaths per role.
- **Rerun measurements for the carrier rule** (from a human-queue match with `[BGWSG]` at info level; the human's presence turns the
  lines on): (1) carry time per pickup (core message "picked up" to "captured", "dropped" or the carrier's death; the
  `[BGWSG] ... carrier ... carry=<s>` lines give the carry seconds at each change); (2) captures per match; (3) carrier deaths at the
  enemy stand: carriers whose last `pos` or snapshot lies within 45 yd of the enemy flag stand (the baseline was most of 10);
  (4) healer distance to the carrier: the `fc=<name>@<yards>` field of the healers' `[BGWSG]` lines (`healer=1`, kind `heal-carrier`)
  and the probe's position snapshots (`bgstate.py` style); target: a following healer within 25 yd for most of the carry; (5) the
  carrier lines: choice and why, how often `fight-alone`, `fight-slowed` and `fight-rooted` occur and what followed; (6) time to the
  first pickup after the gates (baseline 8:56) from the `take-flag` lines.
- Acceptance: the match finishes by 3 captures or the clock; both sides pick up a flag within 3 minutes of the gates; the
  human team can win and can lose (at least one capture and one loss in four runs with the same human play); each team
  keeps 2 or more bots at its flag room while no enemy carries our flag; no bot stands still for more than 60 s alive
  and out of combat; fewer than 8 call-out lines a minute per team; the debuff states are reached in a forced stalemate
  and the stalemate breaker ends it.

## 15. Twin Peaks: what carries over (sheet in a later run)

Verified: Twin Peaks uses the same flag spells, 3 captures, 23 s reset, 10 s drop timer and 10 and 15 minute debuffs
(`BattlegroundTP.h:13-17`), but a **30 minute** instance clock (`BattlegroundTP.cpp:63`), brackets 85-89 and 90 only, and
mid-field graveyards (1749 / 1750) instead of base graveyards. Our code already runs the Warsong case on Twin Peaks data
(`AiFactory.cpp:975`, `selectObjective`, `vPaths_TP`, `tpJumpDown`, `TP_GUARD_POSTS_*`, `vPaths_TP_Carrier*`).

Carries over unchanged: the team record, roles and assignment (G1), state machine A to F (G3), dropped flag rule (G4),
EFC kill and escort (G5, G6), tiers (G7), call-outs (G8), stalemate and clock rules (G10, with 30 minutes), class jobs
table. Needs its own sheet: positions (flags, gates with three entrances per base, buffs), graveyards, carrier lanes that
pass the owner's own mid graveyard, water in the valley, ledge jumps, and the longer match.

## 16. Owner questions (recommended option first)

1. **Lead 2-0 behavior.** (a) Recommended: defense plan, at most 3 bots hold the enemy tunnel exit, the rest stay home;
   no spawn camping. (b) Keep today's 80 percent graveyard camp. (c) No change of plan when ahead.
2. **Plan variety per match.** (a) Recommended: each team draws offense 20 / balanced 60 / defense 20 once per match, as
   the old code drew uniformly. (b) Always balanced. (c) Uniform draw as today.
3. **Chat channel for call-outs.** (a) Recommended: the battleground group chat (instance chat) if the headless client shows
   it to humans, else raid chat. (b) Say (local only). (c) No call-outs for now.
4. **Human flag carrier.** (a) Recommended: the strike group and MID escort a human carrier (follow within 12 yd, peel). (b) Bots
   ignore a human carrier and play their own plan.
5. **Skill tiers in battlegrounds.** (a) Recommended: apply the tiers and the rule lines of section 11 in battlegrounds, using the
   same 10 / 60 / 30 mix. (b) Keep battleground bots all expert for now. (c) Apply delay only.
6. **Test bracket order for the live run.** (a) Recommended: the bracket of the owner's test character first (15-19, 20-24 or
   25-29), then one more bracket; low brackets have no mounts and small spell lists, so the plan's escort share is 3 there.
   (b) Level 30-34 first. (c) Level 10-14 first.
7. **Stealth classes as takers.** (a) Recommended: rogues and feral druids take TAKER and BACKUP when they exist in the
   team. (b) Any class by score only.
8. **Spawn camping rule for the human.** (a) Recommended: no bot stays at the enemy main graveyard (771 / 772) for more
   than 20 s unless the enemy carries our flag and the carrier is on that lane. (b) Allow camp at 2-0 only.

## 17. First live runs (2026-11-17, bracket 15-19, tier 1 only, AI layer off)

Three matches on the live server with a headless probe as the human (a level 17 warrior in the Horde queue) and the
random bots of both factions. Measured from the probe's packets, the core's battleground messages and `.mmap path` from
a GM probe standing in the match.

- **Queue:** the human queued at 16:48:54 and got its popup after 28 s (first run); the later run entered the battleground 43 s after the queue. Bots queued at
  the next `CheckBgQueue` tick (30 s): 29 to 32 bots queued for a 10 v 10 because every eligible bot reads the same stale
  census; the surplus stayed queued (the release in `CheckBgQueue` never ran, fixed as BT-387).
- **Core:** a battleground starts with 5 or more per side invited, so a human who declines leaves a 4 v 5 or 5 v 5 match that
  the free slot queue fills to 10 v 10 within minutes.
- **Match 2 (human queue, 17:19 to 17:46, gates at 17:21:21):** first flag pickup 8:56 after the gates; ten pickups in 25
  minutes (4 Alliance, 6 Horde), each carrier lived 1 to 67 s (median about 25 s) before the flag dropped,
  every dropped flag was returned within 1 to 2 s by a defender; no capture message came, so the score stayed 0 to 0 (inference: the
  match ended on the clock; the probe's hold ended in the same minute and the end message was not seen). In match 1 (bots
  only, 16:49 to 17:18, the human had declined) a Horde bot was seen at its own flag stand carrying the flag after 11
  minutes (inference: a capture; a spectator gets no battleground messages).
- **Graveyard stall:** after a death the bots stood alive at their own main graveyard (771 / 772) for a long time: in
  match 2, 21 percent of the Horde's and 25 percent of the Alliance's alive bot samples after the gates were within 15 yd
  of their own graveyard; in match 1, 9 of 10 Alliance bots stood there for 100 to 190 s after a death. The engine said
  `move to objective failed` every tick. `.mmap path` from the graveyards: Alliance 771 (1406.65, 1564.74, 328.33) to the
  old graveyard shelf waypoints, the flag room posts and the stand is type 8 (no path), to the tunnel exit (1348, 1461) type
  1 (47 points) and to mid type 1 (60 points); Horde 772 (1061.6, 1377.09, 328.56) to the shelf waypoints type 8, to the
  tunnel exit (1124.4, 1462.3) type 1 (44 points), to mid type 1 (63 points). Inference: a far objective is also refused as a short
  path, which `MoveTo` treats as no path. The fix sends such a bot to its tunnel exit first (BT-386).
- **Call-outs:** 10 lines in 27 minutes in the battleground chat, all at flag events ("got it, do not lose me", "fc dying",
  "efc is at tun now", "flag dropped fr"), well below the limit of the sheet. No `inc` call: no three enemies reached a stand.
- **Tier 1 gaps seen:** the carrier does not leave a fight at the enemy stand (three Alliance bots were on the Horde stand at
  once, each carrier dropped within 70 s); a bot picks up a dropped flag next to the enemy hunters and drops it again after
  1 s (two cases); no healer was seen following a carrier; the strike group reaches the enemy half in waves of four.
  These need the plan's own debug lines (`[BGWSG]`, `[BGF]`, `[BGCAP]` are debug level and the live logger is at info) to
  attribute; see the proposals in the run report. Answer (2026-11-17): the carrier rule and the healer follow of section 6, and
  `[BGWSG]` lines at info level in any match with a human (section 14).
- **Reading of the first run's carriers** (probe snapshots every 10 s of match 2, inference from positions): three carries of the Horde flag by Alliance
  bots were still at x 931-955 (the Horde stand is at x 916) and in combat 18 to 60 s after their pickup,
  two of them died there and one fought about 25 s at the stand and died at the Horde tunnel exit (x 1122). That is the stand
  fight the carrier rule removes.
