# Monk: Brewmaster (tank)

- **Status:** draft, checked against 5.4.8 data and against the bots' code by reading (2026-11-10). Config gap items 1
  and 2 and rotation gap items 1, 2, 3 and 5 are implemented on branch `feat/class-sheet-hpal-brewmaster` (BT-133; compile
  pending, the owner's in-game dungeon test pending); the other rotation items are open. File and line numbers in the
  tables are those of the code before that change. Nothing verified live.
- **Sources:**
  - Method, "Brewmaster Monk Class Guide - MoP Classic" (Phase 1, level 90 raid and dungeon, 23 July 2025), read
    2026-11-10.
  - wowtbc.gg, "Brewmaster Monk PvE Guide, MoP Classic" (no patch line on the page), read 2026-11-10.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, Talent, GlyphProperties, SpellEffect,
    SpellCooldowns, SpellPower, SpellRange, SpellMisc, SpellDuration, SpellRadius; `data/db2/Item-sparse.db2` for glyph item
    ids) through the data reader and short read-only scripts, and the server core's monk spell scripts
    (`spell_monk.cpp`) for the Stagger, Elusive Brew and Provoke mechanics.
  - Not readable: Overgear and Icy Veins (HTTP 403), Wowhead (script-built page). Two further guesses (a Peak of Serenity
    page, which is a current-expansion landing page, and a guide-site URL, 404) gave nothing. Two guides were read,
    not three.
- **Levels covered:** 15 to 60 in detail (5-player dungeons, a human leader who marks targets), 61 to 90 briefly.

Which source was richer: **talents** Method for the picks and the swaps (wowtbc lists every option per tier without a
mark); **glyphs** Method (a raid set and a dungeon set with alternatives; wowtbc has a standard set plus the Breath of
Fire dungeon note); **stat order** Method (wowtbc has none); **opener** equal (Method adds Expel Harm before the pull,
wowtbc the pre-casts); **single target priority** Method (it keeps Guard in the list and gives numbers: Elusive Brew at 7
to 8 charges, Purifying Brew on Red Stagger, Keg Smash at 3 Chi or less, an Energy floor); wowtbc adds Touch of Death and
the Chi builders in order; **area priority** wowtbc (Rushing Jade Wind, Breath of Fire on every target from three targets;
Method only swaps Spinning Crane Kick for Jab); **defensives** wowtbc (an ordered list of five; Method names a set
without order and leaves out Fortifying Brew and Avert Harm); **interrupts, taunt use, the pull, levels 15 to 60, what to
do before a spell is learned, thresholds** neither guide says anything (both have Provoke only in the opener), so those
lines are derived from the 5.4.8 data and marked "derived".

Where the guides disagree and the pick: Method keeps Shuffle (Blackout Kick) before Tiger Power (Tiger Palm), wowtbc the
reverse, and wowtbc leaves Guard out of its priority list while Method keeps it up. Pick: Method. The data shows why:
Shuffle is mitigation (20 percent parry and 20 percent more Stagger for 6 s), Tiger Power only ignores armor, and Tiger
Palm costs no Chi for a Brewmaster from level 34, so it is never a Chi decision. On the talent tier 3 the guides disagree
too (Method: Ascension, "locked"; wowtbc: its opener uses Chi Brew, and the module's premade takes Chi Brew); see Talents.

## Talents

Tiers and columns are from Talent.dbc. "Bots today" is `AiPlayerbot.PremadeSpecLink.10.0 = 002220`
(`conf/playerbots.conf.dist:2883`; one digit per tier, the column counted from 0). For a character below 75 only the
first four digits are in effect.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (002220) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Tiger's Lust (Method) | Momentum if moving a lot (Method); Celerity (wowtbc lists only) | 116841 / 115174 / 115173 | Celerity (115173), differs; no bot line uses any of the three, so the choice has no effect |
| 2 (30) | Chi Wave (Method: locked) | Zen Sphere, Chi Burst (wowtbc lists only) | 115098 | Chi Wave, same; **no brewmaster line casts it** (gap 5) |
| 3 (45) | Ascension (Method: locked) | Chi Brew (wowtbc opener, module premade), Power Strikes | 115396 / 115399 / 121817 | Chi Brew (115399), differs from Method; it has a line (`MonkStrategies.cpp:131-132`) and Ascension has none to lose |
| 4 (60) | Leg Sweep (Method) | Ring of Peace for knockbacks or kiting (Method), Charging Ox Wave (wowtbc lists only) | 119381 / 116844 / 119392 | Leg Sweep, same; no bot line casts any of the three |
| 5 (75) | Dampen Harm or Diffuse Magic, by encounter (Method) | Healing Elixirs (wowtbc lists only) | 122278 / 122783 / 122280 | **Diffuse Magic (122783), a Method pick, but no line casts it; Dampen Harm has one** (`MonkStrategies.cpp:190-191`) |
| 6 (90) | Rushing Jade Wind (Method: always in dungeons) | Invoke Xuen (Method: pure single target), Chi Torpedo | 116847 / 123904 / 115008 | Rushing Jade Wind, same; line at `MonkStrategies.cpp:81` |

Notes on the picks (data, not guide):

- Chi Brew (data text): restores 2 Chi and, for a Brewmaster, two stacks of Elusive Brew, two charges. Ascension: +1
  maximum Chi, +15 percent Energy regeneration (spell 115396 effects). Both are sound for a bot; the module has code only
  for Chi Brew (cast at Chi 2 or less), so the premade stays (derived).
- Dampen Harm (45 s, 90 s cooldown, usable while stunned): the next attacks within 45 s that deal at least a set share of
  maximum health are halved. Diffuse Magic: 90 percent less spell damage for 6 s, 90 s cooldown. Most dungeon damage on
  a tank is physical, and only Dampen Harm has a bot line, so Dampen Harm is the pick for bots (derived).
- Rushing Jade Wind (6 s cooldown, 40 Energy, "replaces Spinning Crane Kick", generates Chi from 3 targets) is level 90 only.
- Leg Sweep (45 s, knocks down everything within range, 5 s) and Ring of Peace have no line; a tank that stuns the
  pack away from itself is not wanted without a rule (derived), so they stay idle.
- Chi Wave (15 s cooldown in the data, not 8 s) is a 40 yd bouncing bolt (damage to enemies, heal to allies).

Proposed link by the guides (dungeon): `002210` (Celerity / Chi Wave / Chi Brew / Leg Sweep / Dampen Harm / Rushing
Jade Wind). Only tier 5 changes, and only from level 75.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and
75 (config comment; the unlock levels are core-side). Config order is major, minor, major, minor, minor, major
(`conf/playerbots.conf.dist:2884`).

| Type | Glyph | Item id | Why (one clause) | Bots today (`87893,87887,85695,87889,87890,87896`) |
|------|-------|--------:|------------------|------------------------|
| Major | Fortifying Brew | 87893 | more damage reduction, less bonus health (spell text); both guides' first pick; learned at 24 | yes (slot 1, level 25) |
| Major | Breath of Fire | 85685 | targets hit while Dizzying Haze is on them are disoriented for 3 s (Method and wowtbc: the dungeon pick); learned at 18 | **no; Zen Meditation is in its place** |
| Major | Zen Meditation | 85695 | channel Zen Meditation while moving (wowtbc standard, Method alternative); the spell is learned at 82 | yes (slot 3, level 50; **idle until level 82**) |
| Major | Fortuitous Spheres | 87896 | a healing sphere appears near the monk when health drops low (wowtbc standard, Method alternative); needs Gift of the Ox, level 56 | yes (slot 6, level 75; whether a bot collects the sphere is to check, gap 12) |
| Major, optional | Spinning Crane Kick | 85697 | full movement speed while channelling (Method's third dungeon glyph); Spinning Crane Kick is learned at 46; no value for a bot that stands still | no |
| Major, optional | Touch of Death, Enduring Healing Sphere | 85700, 85689 | Touch of Death without Chi but a 2 minute longer cooldown (Method raid pick, rejected for dungeons); sphere duration | no |
| Minor | Spirit Roll, Water Roll | 87887, 87889 | Roll over water and as a spirit; no tanking effect (both guides: the only good minors) | yes (slots 2 and 4) |
| Minor | Zen Flight | 87890 | teaches Zen Flight (needs a flight license); in neither guide | yes (slot 5), harmless |

Disagreement: Method and wowtbc both name Fortifying Brew and Breath of Fire (dungeons); they differ on the rest (Method:
Spinning Crane Kick; wowtbc: Zen Meditation and Fortuitous Spheres). The premade is wowtbc's standard raid set; for the
15 to 60 band the second major slot (level 50) is idle for 32 levels with Zen Meditation (derived from the learn level).
Proposed glyphs: `87893,87887,85685,87889,87890,87896` (slot 3 changed to Breath of Fire). Caveat: a disoriented mob
wanders for 3 s and may stray into the group; whether that matters in a dungeon is **to check live**.

## Stat order

Method: Hit to 7.5 percent, Expertise to about 15 percent, Agility and Stamina, then Mastery, Critical Strike and Haste by
preference (Mastery smoother mitigation, Critical Strike more Elusive Brew and damage but spikier, a little Haste for the
rotation). The caps are level 90 numbers and cannot be reached with the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason): **leather** (Leather Specialization, level 50,
adds Stamina only in all-leather, so keep a full set from 50); **Stamina and Agility** first; **a slow two-handed weapon
(staff or polearm)** over a one-handed one: the Elusive Brew charge rule in the server script gives 3 stacks per
auto-attack crit with a 3.6 s two-hander against 1.5 with a 2.6 s one-hander, from the main hand only; then Critical
Strike (more Elusive Brew crits), Haste; **Mastery is worth nothing below level 80** (Mastery: Elusive Brawler is learned at
80, spell text "more Stagger"); Hit and Expertise only near level 90.

Bots today (`Mgr/Item/StatsWeightCalculator.cpp:639-650`): Stamina 3.5, Agility 2.2, Hit 2.5, Expertise 2.5, Critical Strike
2.0, Dodge 1.5, Haste 1.3, Mastery 1.1, Attack Power 0.5, Armor 0.15; a one-handed main hand is scaled by 0.1
(`:841-848`). That puts Hit and Expertise above Agility and gives Mastery a weight below level 80 (gap 15).

## What the specialisation knows, by level (5.4.8 data)

Learn levels are from SpellLevels (the cast spell's row) and SpecializationSpells. Stances need no talent; every cast
below needs Stance of the Sturdy Ox where the spell links one (see Differences). Resources: Energy (regenerates 10 per
second, +10 percent in the stance), Chi (Jab 1, Keg Smash 2, Expel Harm 1, Chi Brew 2; the guide's "3 Chi or less" for
Keg Smash implies a cap of 4).

| Level | Spell (id, cost, cooldown) |
|------:|----------------------------|
| 1 | Jab (100780, 40 Energy, +1 Chi) |
| 3 | Tiger Palm (100787, 1 Chi, free from 34; Tiger Power 20 s, ignore armor) |
| 7 | Blackout Kick (100784, 2 Chi; gives Shuffle only from 34) |
| 10 | Stance of the Sturdy Ox (115069: 25 percent less damage taken, +10 percent Energy regen, +20 percent Stamina, Stagger), Dizzying Haze (115180, 20 Energy, 40 yd area snare, "high threat", ground target), Vengeance (120267); the specialisation is chosen here |
| 11 | Keg Smash (121253, 40 Energy, 8 s, 8 yd area, +2 Chi, Weakened Blows, Dizzying Haze) |
| 14 | Provoke (115546, 40 yd, 8 s, off the global cooldown; on the Black Ox Statue it taunts everything within 8 yd of it) |
| 18 | Breath of Fire (115181, 2 Chi, 8 yd cone, burn on Dizzying Haze targets), Clash (122057, 40 yd, 35 s, both charge, 4 s stun), Resuscitate (115178) |
| 20 | Detox (115450, poison and disease) |
| 22 | Touch of Death (115080, 3 Chi, 90 s, target health at or below the monk's), Legacy of the Emperor (115921) |
| 24 | Fortifying Brew (115203, 3 min, 20 s, 20 percent less damage, +20 percent health, more Stagger) |
| 26 | Guard (115295, 2 Chi, 30 s cooldown, absorbs for up to 30 s, off the global cooldown, +heals to self), Expel Harm (115072, 40 Energy, 15 s, +1 Chi, heals self) |
| 30 | tier 2 talent (Chi Wave) |
| 32 | Spear Hand Strike (116705, 15 s, melee range, interrupt, 4 s school lock) |
| 34 | Brewmaster Training (117967): Tiger Palm costs no Chi and feeds the next Guard (Power Guard, 30 s); Blackout Kick gives Shuffle (6 s, 20 percent parry, +20 percentage points Stagger) |
| 36 | Elusive Brew (115308, 6 s cooldown, +30 percent dodge for 1 s per stack; stacks come from auto-attack crits), Energizing Brew (115288, 60 s, in combat) |
| 44 | Paralysis (115078) |
| 45 | tier 3 talent (Chi Brew), Desperate Measures (Expel Harm has no cooldown at 35 percent health or less) |
| 46 | Spinning Crane Kick (101546, 40 Energy, 2.25 s channel, 1 Chi from 3 targets) |
| 48 | Avert Harm (115213, 3 min, 6 s, 20 percent of party damage within 10 yd redirected to the monk, cancels at 10 percent health) |
| 50 | Leather Specialization |
| 54 | Crackling Jade Lightning (117952, 40 yd channel, no Energy) |
| 56 | Gift of the Ox (124502, healing spheres on melee damage, walk through to collect) |
| 60 | tier 4 talent (Leg Sweep) |
| 64 | Healing Sphere (115460) |
| 70 | Summon Black Ox Statue (115315, 30 s, 15 min, 40 yd; its Guard on injured allies, its area Provoke) |
| 75 | Purifying Brew (119582, 1 Chi, clears all Stagger), tier 5 talent |
| 80 | Mastery: Elusive Brawler (117906, more Stagger) |
| 82 | Zen Meditation (115176, 3 min, 8 s, 90 percent less damage, any melee hit on the monk breaks it) |
| 90 | tier 6 talent (Rushing Jade Wind) |

Stagger (server script and spell text): the stance turns 20 percent of each physical hit into a damage over time spread
over 10 s; Shuffle, Fortifying Brew and Mastery add to the share (cap 100 percent). The level shows as Light, Moderate or
Heavy Stagger by the damage per tick: below 3 percent of maximum health, below 6 percent, 6 percent and more. Purifying
Brew (level 75) deletes what is left. Before 75 the pending damage is the healer's job.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked
"derived" comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to
`mod-aware-bots-548/src/` (module repository). Relevance numbers: default actions sit at 5.0 plus a fraction, high 20,
interrupt 40, emergency 90 (`strategy/Strategy.h:37-51`); a trigger's action outranks every default action, and the
engine takes the highest relevance first.

### What runs today (verified by reading)

The Brewmaster carries `tank`, `tank assist`, `pull`, `pull back`, `aoe` and, from the common monk line, `dps assist` and
`cure` in combat (`Factory/AiFactory.cpp:533-541`), plus `tank face` (`:545-546`), the generic list with `boost`
(`:316`), and out of combat `tank assist`, `cure` and the `nc` strategy (`:780-785`, `:886` for `mark pull`). Default
actions (`strategy/Classes/monk/MonkStrategies.cpp:37-52`): Guard 5.7, Purifying Brew 5.6, Shuffle (a Blackout Kick) 5.55,
Keg Smash 5.5, Breath of Fire 5.4, Tiger Palm 5.3, Jab 5.1, melee 5.0. Trigger edges: the stance (29, `:177-179`), reach
melee (21, `:181`), Provoke on a lost target (41, `:185-186`), Fortifying Brew (25) and Dampen Harm (24) at 45 percent
health or less (`:118-119`, `:190-191`), Elusive Brew (23, `:192-193`), Black Ox Statue (21, `:194-195`), the three interrupts
(42, 41, 40, `:110-114`, `:124-126`), Chi Brew (21, `:131-132`), Expel Harm (10, `:139-140`), the area edges (25, 24, 23,
`:77-84`), Touch of Death through `boost` (23, `:354-355`).

### The pull (all bands; a human leader marks, the tank pulls marks only, the group waits)

Setup verified by reading: the trigger fires when a mark exists, is alive, not in combat, within 40 yd, and every group
member is alive, out of combat, above 60 percent health and (mana users) 50 percent mana
(`strategy/triggers/MarkTriggers.cpp:49-78`, `:32-45`); then `pull rti target` (`strategy/generic/MarkPullStrategy.cpp:12`).
The request is checked in `PullStrategy::CanDoPullAction` (`strategy/generic/PullStrategy.cpp:137-162`); the monk is exempt
from the "shoot needs a ranged weapon" refusal (`:150-152`) but a pull whose action name is empty is refused (`:157-159`).
The monk's override (`strategy/Classes/monk/MonkStrategies.cpp:368-384`): below the level that teaches Provoke (14) the
name is empty (`:372-373`, BT-072); otherwise, while the target is not yet set, the base name "shoot" passes the check; once
set, Provoke is chosen when it can be cast (`:380-381`), else the base name "shoot" (`:383`).

Does the monk have the warrior's and bear's problem? **Levels 15 to 60: the pull is accepted and Provoke (40 yd) is the
pull; the stall exists only when Provoke cannot be cast.** Verified by reading:

- Provoke 115546 is learned at 14, so a level 15 to 60 tank always has a ranged pull. Levels 10 to 13 (derived: no
  dungeon in this band is that low) refuse the pull with an error tell to the leader (`PullActions.cpp:80-85`).
- When Provoke is on cooldown (8 s), out of line of sight, or the target is not castable, the name falls back to "shoot"
  (`MonkStrategies.cpp:383`; `PullStrategy.cpp:89`), a spell no monk knows; the pull action fails every tick
  (`strategy/actions/PullActions.cpp:209-210`) and the pull ends only at the 15 s limit (`PullStrategy.h:41`,
  `strategy/triggers/PullTriggers.cpp:41-43`), after which the mark trigger fires again. The usual case is a Provoke used
  less than 8 s before the pull (a taunt at the end of the last fight); stall length and frequency are **to check live**.
  Gap 2.
- The stance is kept out of combat by the `nc` strategy at relevance 22 (`MonkStrategies.cpp:292-302`, spec read when the
  strategy is built) and in combat at 29. The mark pull (50) outranks the stance and the pull multiplier blocks every
  other action while a pull is pending or started (`PullStrategy.cpp:183-198`), so a tank without the stance (after a
  resurrection) pulls first. The data links no stance requirement to Provoke (Spell.dbc, shapeshift field empty, unlike
  Jab, Blackout Kick, Tiger Palm and Keg Smash), so the pull should still work; the first Keg Smash and Jab would fail
  until the stance is back. **To check live.**
- The out-of-combat engine has no `pull` strategy for the monk (the Blood death knight got one, `AiFactory.cpp:775-776`);
  `PullStrategy::Get` falls back to the combat engine's strategy (`PullStrategy.cpp:64`), so the request works. Not a gap
  found by reading.

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| P1 | core | the group is ready and a mark is up | be in Stance of the Sturdy Ox (115069, 10) | yes: `MonkStrategies.cpp:292-302` (22) and `:177-179` (29); hole after a resurrection, see above |
| P2 | core | mark within 40 yd | Provoke (115546, 14): the mark runs to the monk at +50 percent speed | yes: `MonkStrategies.cpp:380-381`, range from the spell (`PullStrategy.cpp:121-133`) |
| P3 | good | Provoke on cooldown or not castable (derived: wait for the 8 s, nothing else needed; from 54 Crackling Jade Lightning, 117952, 40 yd, no Energy, is a ranged alternative) | wait, or Crackling Jade Lightning | **gap: falls to "shoot" and stalls (`MonkStrategies.cpp:383`)** |
| P4 | good | the pull target is hit; mobs come to the tank | stay at the pull position | yes: `pull back` (`PullStrategy.cpp:262-268`) |
| P5 | expert | from 70, a pack that stands together (derived: Provoke aimed at the Black Ox Statue taunts every enemy within 8 yd of it) | Summon Black Ox Statue (115315, 70), then Provoke the statue | **missing: no line aims Provoke at the statue; the statue is cast only in combat** (`MonkStrategies.cpp:194-195`, `MonkActions.h:186`) |

### Levels 15 to 29

The Brewmaster has Jab, Tiger Palm, Blackout Kick, Keg Smash (11), Provoke (14), Breath of Fire and Clash (18),
Fortifying Brew (24), Guard and Expel Harm (26). No Brewmaster Training before 34 (so Blackout Kick gives no Shuffle and
Tiger Palm costs Chi), no interrupt before 32, no self heal before 26.

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | always in a fight (derived: the stance is the 25 percent damage cut and the Stagger source) | Stance of the Sturdy Ox (115069, 10) | yes: `MonkStrategies.cpp:177-179` at 29 |
| 2 | core | an enemy attacks someone else (healer, damage), any mob, 8 s | Provoke (115546, 14), 40 yd, off the global | yes: `lose aggro` on the current target (`MonkStrategies.cpp:185-186` at 41), never on a player (`MonkActions.h:186`, `GenericTriggers.cpp:155-161`); the tank picks mobs without aggro first (`strategy/value/TankTargetValue.cpp:74-116`) |
| 3 | core | Keg Smash ready (8 s), Chi 3 or less (Method; wasted Chi otherwise) | Keg Smash (121253, 11) | yes, no Chi test: `MonkStrategies.cpp:47` at 5.5, so it fires at Chi 3 and 4 and wastes 1 to 2 Chi |
| 4 | core | Chi 2 or more and nothing above applies (derived: Blackout Kick is the hardest hit and, from 34, the Shuffle source) | Blackout Kick (100784, 7) | yes through the Shuffle line: `MonkStrategies.cpp:46`, `MonkActions.cpp:40-47`; **before 34 no Shuffle exists, so the line fires at every 2 Chi and starves Breath of Fire (row 7)** |
| 5 | core | Chi below 2, Energy 40 or more | Jab (100780, 1), +1 Chi | yes: `MonkStrategies.cpp:50` at 5.1 |
| 6 | good | Tiger Power missing or under 4 s (Tiger Palm costs 1 Chi until 34) | Tiger Palm (100787, 3) | yes: `MonkStrategies.cpp:49` at 5.3, `MonkActions.cpp:19-29` |
| 7 | good | Chi 2 or more, three or more enemies in front | Breath of Fire (115181, 18) | yes: `MonkStrategies.cpp:48` at 5.4 (any target count) and the area edge (row 11); starved before 34 by row 4 |
| 8 | core | no Guard up, Chi 2 or more; after a Tiger Palm so the Power Guard bonus applies (derived: the data gives 15 percent more absorb, free from 34) | Guard (115295, 26) | yes: `MonkStrategies.cpp:44` at 5.7, a buff on the monk (BT-071) |
| 9 | core | health below 70 percent and Energy 40 or more (derived: a heal of seven weapon hits, 15 s cooldown, +1 Chi, bigger under Guard) | Expel Harm (115072, 26) | **wrong band: only at 65 to 85 percent health, relevance 10 (`MonkStrategies.cpp:139-140`, `strategy/triggers/HealthTriggers.h:72-79`, config `AlmostFullHealth = 85`, `MediumHealth = 65`); nothing below 65 percent** |
| 10 | good | health below 50 percent, or the start of a pack of three or more (derived: 3 min cooldown fits a pull rhythm) | Fortifying Brew (115203, 24) | partly: at 45 percent health or less, relevance 25 (`MonkStrategies.cpp:118-119`), no pack rule |
| 11 | core | three or more enemies within 8 yd of the target (area threat; derived: only Keg Smash and Breath of Fire exist here) | Keg Smash, Breath of Fire | the area edge puts Breath of Fire at 23 above Guard (5.7) and Shuffle (5.55) (`MonkStrategies.cpp:81-83`); inference from the numbers: on every pack of three Guard and Shuffle wait for Chi that Breath of Fire spends; confirm in a debug bot's action log. Gap 3 |
| 12 | good | Dizzying Haze for area threat and a snare (derived: 20 Energy, 8 yd, "high threat", level 10) | Dizzying Haze (115180, 10) | **missing: the action is registered (`MonkActions.h:75`, `MonkAiObjectContext.cpp:186`) but in no strategy; a ground-target cast through a unit target is to check live** |

What a tank does where a key spell is missing (derived): levels 15 to 25 have no Guard and no Expel Harm, so survival is the
stance, Stagger (the healer carries the pending damage) and Fortifying Brew from 24; no interrupt until 32; Breath of Fire
exists from 18 but Shuffle does not before 34. The bots tolerate every unlearned spell: an unknown spell fails the cast
check and the next line is tried (`AI/PlayerbotAI.cpp:3810`, inference from the call sites).

### Levels 30 to 44

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 13 | good | on cooldown (15 s) | Chi Wave (115098, talent at 30) | **missing: no brewmaster line (`MonkStrategies.cpp:37-52`); the action exists (`MonkActions.h:61`)** |
| 14 | core | an enemy casts (current target, a healer, or any caster in range) | Spear Hand Strike (116705, 32), 15 s | yes: `MonkStrategies.cpp:110-114` (42, 41) and `:124-126` (40); melee range only, the shared value looks 30 yd out (`MonkActions.cpp:149-153`) |
| 15 | core | Shuffle missing or under 3 s, Chi 2 or more | Blackout Kick (100784, 34) | yes: `MonkActions.cpp:40-47`, Shuffle at 5.55 (above Keg Smash) |
| 16 | good | Tiger Power missing or under 4 s; free from 34 | Tiger Palm (100787) | yes (row 6); at 5.3 below Breath of Fire, so it casts when Chi is under 2, which is when it is free |
| 17 | core | stacks of Elusive Brew at 7 or more (Method 7 to 8), not already running | Elusive Brew (115308, 36) | yes: `MonkStrategies.cpp:192-193` (23), `MonkActions.cpp:101-108`; **rarely reached at these levels** (see Cooldowns) |
| 18 | good | target health at or below the monk's, 3 Chi free, off cooldown (wowtbc) | Touch of Death (115080, 22) | yes through `boost`: `MonkStrategies.cpp:354-355` (23), `MonkActions.cpp:71-88`; spends the 3 Chi that Shuffle and Guard need, but kills trash |

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 19 | good | Chi 2 or less | Chi Brew (115399, talent at 45): +2 Chi, +2 Elusive Brew stacks, two charges | yes: `MonkStrategies.cpp:131-132` (21), `MonkActions.cpp:111-120` |
| 20 | core | three or more enemies within 8 yd of the target | Spinning Crane Kick (101546, 46), replaces Jab for Chi (Method) | **wrong order: area edge at relevance 24 (`MonkStrategies.cpp:82`) above Guard, Shuffle and Keg Smash; the channel (2.25 s) also blocks every other cast (`AI/PlayerbotAI.cpp:3818`)** |
| 21 | expert | a party member is below 40 percent and the monk above 60 percent (derived: 20 percent of the group's damage moves to the monk; it cancels at 10 percent health) | Avert Harm (115213, 48), 3 min | missing |
| 22 | good | health at or below 35 percent | Expel Harm without cooldown (Desperate Measures, 45) | covered by row 9 once the band is fixed |
| 23 | good | a healing sphere is near (Gift of the Ox, 56) | walk through it | **missing: no code reads spheres or Gift of the Ox (search of `src/`)** |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 24 | good | tier 4 talent at 60 | Leg Sweep (119381): no rule (derived: a stun that pushes the pack off the tank is not wanted) | idle |
| 25 | good | in combat, no statue of the monk's within range | Summon Black Ox Statue (115315, 70), 15 min | yes: `MonkStrategies.cpp:194-195` (21), `MonkActions.cpp:134-147` |
| 26 | expert | two or more enemies not on the tank and a statue standing within 40 yd | Provoke aimed at the statue (area taunt, 8 yd) | missing (P5) |

### Levels 75 to 89

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 27 | core | Heavy Stagger (6 percent of maximum health per tick or more; Method: Red Stagger) | Purifying Brew (119582, 75), 1 Chi, 1 s cooldown | **wider than the guide: Moderate or Heavy Stagger, or health at 40 percent or less (`MonkActions.cpp:31-38`) at 5.6, above Shuffle; each cast is 1 Chi taken from Shuffle and Guard** |
| 28 | good | tier 5 talent | Dampen Harm (122278), halves the next big hits for 45 s | edge at health 45 percent or less (`MonkStrategies.cpp:190-191` at 24); **idle with the premade (Diffuse Magic)** |
| 29 | expert | a spell-heavy pull (derived: 90 percent less spell damage for 6 s) | Diffuse Magic (122783) | missing |
| 30 | expert | a hard pull, the monk above 50 percent health | Zen Meditation (115176, 82): 90 percent less damage, any melee hit breaks it (derived: a tank is hit in melee, so it is rarely usable) | missing |

### Level 90

Guide (Method, wowtbc): Stance, Black Ox Statue up, Guard up, Shuffle, Tiger Power, Elusive Brew at 7 to 8, Purifying Brew
on Red Stagger, Invoke Xuen on cooldown (if taken), Chi Wave, Keg Smash, then Chi builders and spenders. Area: Rushing Jade
Wind up (6 s duration, 6 s cooldown), Breath of Fire on every target from three. The premade matches the dungeon pick; the
area edge puts Rushing Jade Wind at 25 above every upkeep line (`MonkStrategies.cpp:81`), the same order gap as row 20.

## Cooldowns and defensives

The guides give an order (wowtbc: Avert Harm, Guard, Fortifying Brew, Zen Meditation, Diffuse Magic or Dampen Harm;
Method names a set); the thresholds are derived because dungeon trash needs numbers. Thresholds are the bot's own health
percent.

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Stance of the Sturdy Ox | 10 | first line | always | yes (29 in combat, 22 out of combat) |
| Guard | 26 | keep up at all times (Method); second defensive (wowtbc) | on cooldown after a Tiger Palm; 30 s absorb | yes at 5.7; no Tiger Palm first |
| Shuffle (Blackout Kick) | 34 | keep up | Chi 2 and under 3 s left | yes |
| Fortifying Brew | 24 | third (wowtbc) | below 50 percent or at the start of a pack of three or more | 45 percent only |
| Elusive Brew | 36 | 7 to 8 stacks, never 15 | 7 stacks; derived: 4 or more while below 60 percent health or with three enemies on the tank (stacks come slowly) | 7 only |
| Purifying Brew | 75 | on Red Stagger | Heavy Stagger, or Moderate with 3 or more Chi spare | Moderate, Heavy, 40 percent |
| Expel Harm | 26 | Chi builder (Method, wowtbc) | below 70 percent health; also as a Chi builder when Chi is 0 and Energy is spare | 65 to 85 percent only |
| Avert Harm | 48 | first (wowtbc) | an injured member below 40 percent, monk above 60 percent | missing |
| Dampen Harm (talent) | 75 | one of two (both guides) | at the start of a hard pull, or below 60 percent | 45 percent, idle with the premade |
| Diffuse Magic (talent) | 75 | one of two (both guides) | a spell-heavy pull | missing |
| Zen Meditation | 82 | fourth (wowtbc) | rarely, a melee hit breaks it | missing |
| Provoke | 14 | the taunt | an enemy not on the tank, any mob | yes, single target |
| Spear Hand Strike | 32 | not given | any casting enemy in reach | yes, three edges |
| Touch of Death | 22 | wowtbc, on targets below your health | target health at or below the monk's | yes through `boost` |
| Self heal | 26 | not given | Expel Harm; Guard raises it; healing spheres from 56 | spheres not collected |

## Healer triage and mana (healers only)

Not applicable to a tank. The resources are Energy and Chi. Energy builders and spenders (40 each): Jab, Keg Smash, Expel
Harm, Spinning Crane Kick, Rushing Jade Wind; Dizzying Haze 20. Chi: Jab 1, Keg Smash 2, Expel Harm 1, Chi Brew 2 in;
Guard, Blackout Kick and Breath of Fire 2, Purifying Brew 1, Touch of Death 3 out (Tiger Palm free from 34). Sustained
income at the data rates is about 3 Chi per 8 s (Keg Smash 2 plus one Jab), against Shuffle's roughly 2.7 Chi per 8 s, so
every other Chi spend competes with Shuffle (derived, an estimate): the priority order matters more than the extras. The guide's Energy rule
(never below 30) has no counterpart in the bots.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Guard (26), Expel Harm (26), Shuffle and the free Tiger Palm (Brewmaster Training, 34),
  Elusive Brew (36), Avert Harm (48), Black Ox Statue (70), Purifying Brew (75), Zen Meditation (82) and the tier 2 to 6
  talents are not available in parts of the 15 to 60 band; the lists leave them out below their level.
- Purifying Brew is learned at 75 here; Method's "Purifying Brew on Red Stagger" cannot apply earlier, and Stagger is
  unmanaged until then.
- Tiger Palm costs no Chi for a Brewmaster from level 34 (spell text of Brewmaster Training); the guides' "spend Chi on
  Blackout Kick and Tiger Palm" reduces to Blackout Kick.
- Chi Wave has a 15 s cooldown in the data; the guides say only "on cooldown".
- Elusive Brew lasts one second per stack at +30 percent dodge (data and server script); the guides' "7 to 8 charges" is a
  seven or eight second window.
- The guide opener "Expel Harm before the pull" needs level 26; the Black Ox Statue in the opener needs level 70.
- The module's comment (`MonkActions.h:35`) says every monk spell except Touch of Death needs a stance; Spell.dbc links a
  stance requirement to Jab, Blackout Kick, Tiger Palm, Keg Smash, Guard and Expel Harm but not to Provoke, Breath of Fire,
  Dizzying Haze, Clash, Spear Hand Strike, Purifying Brew or Elusive Brew. To check live.
- Method's 7.5 percent Hit and 15 percent Expertise are level 90 caps.
- wowtbc's opener pre-pot and consumables have no counterpart in the bots.

## Gap list (ordered by value for levels 15 to 60)

Config (can go at once):

1. **Done (BT-133).** Tier 5 talent (level 75 only): Dampen Harm instead of Diffuse Magic (Method names both; only Dampen Harm has a line).
   Link `002210`.
2. **Done (BT-133).** Glyphs: slot 3 Breath of Fire (85685) instead of Zen Meditation (85695, spell learned at 82), both guides' dungeon
   pick, working from 50: `87893,87887,85685,87889,87890,87896`.
3. Tier 3: Method says Ascension, wowtbc's opener and the module use Chi Brew; keep Chi Brew (it has a line). Tier 1, 4:
   no line uses any pick; leave.

Rotation (class fixes, wait for the owner's in-game test):

1. **Done (BT-133), without the Chi builder case.** **Expel Harm only at 65 to 85 percent health** (`MonkStrategies.cpp:139-140`, `HealthTriggers.h:72-79`): no self heal
   below 65 percent, where it is needed. Fix: below 70 percent (and free at 35 percent from 45), and as a Chi builder
   when Chi is 0 and Energy is spare. The highest-value fix for 26 to 60 (the only self heal; the healer carries Stagger).
2. **Done (BT-133).** **Pull stalls when Provoke cannot be cast** (`MonkStrategies.cpp:380-383`, `PullStrategy.cpp:89`, `PullActions.cpp:209-210`):
   wait for the 8 s cooldown instead of returning "shoot"; from 54 Crackling Jade Lightning as a ranged alternative.
3. **Done (BT-133).** **Area edges outrank the upkeep lines** (`MonkStrategies.cpp:77-84`): Breath of Fire (23), Spinning Crane Kick (24) and
   Rushing Jade Wind (25) sit above Guard (5.7), Shuffle (5.55) and Keg Smash (5.5); the Spinning Crane Kick channel blocks
   every other cast. Fix: Keg Smash, Guard, Shuffle first; Spinning Crane Kick only when Chi is under 2; Breath of Fire as
   the Chi dump; keep Rushing Jade Wind behind the upkeep lines. Inference; confirm in an action log.
4. **Breath of Fire starved from 18 to 33** (`MonkActions.cpp:40-47`): the Shuffle line is Blackout Kick whenever no Shuffle
   exists, i.e. at every 2 Chi until level 34. Minor; resolved when row 3's order gives Breath of Fire its Chi on packs.
5. **Done (BT-133).** **No Chi Wave line** (`MonkStrategies.cpp:37-52`) although it is the tier 2 premade talent: on cooldown (15 s) at about
   5.45, behind Keg Smash. The action exists (`MonkActions.h:61`).
6. **Purifying Brew on Moderate Stagger** (`MonkActions.cpp:31-38`, relevance 5.6): Heavy only, or Moderate with Chi to
   spare. Level 75 and up.
7. **Brewmaster carries `dps assist` in combat** (`AiFactory.cpp:541`, common line for all three specs) next to
   `tank assist`; the other tank classes carry only the latter. The two choose different targets at the same relevance
   (`TankAssistStrategy.cpp:9`, `DpsAssistStrategy.cpp:8`). Whether they flip the tank's target is **to check live**.
8. **Elusive Brew at 7 stacks** (`MonkActions.cpp:101-108`): rarely reached at levels 36 to 60 (stacks come from
   auto-attack crits only, 3 per crit with a slow two-hander; estimate, to check live); also cast at 4 stacks under
   60 percent health or with three enemies on the tank.
9. **Fortifying Brew only at 45 percent** (`MonkStrategies.cpp:118-119`): also at the start of a pack of three or more.
10. **Dampen Harm at 45 percent health** (`:190-191`): late for a big hit; earlier (60 percent or at the pull). Idle until the
    premade takes it (config 1).
11. **Guard before Tiger Palm** (Power Guard, level 34): Tiger Palm then Guard when both are ready; the bot casts Tiger Palm
    only for Tiger Power upkeep. Small.
12. **Healing spheres are never collected** (no reference in `src/`; the Fortuitous Spheres glyph and Gift of the Ox, level 56,
    make them): to check live how the core spawns them and whether a standing monk collects them.
13. **Black Ox Statue only in combat and its area Provoke never used** (`MonkStrategies.cpp:194-195`, `MonkActions.h:186`):
    from 70; expert.
14. **Missing lines (expert, low value)**: Avert Harm (48), Diffuse Magic and Zen Meditation, Dizzying Haze (registered, in
    no strategy), Clash.
15. **Stat weights**: Hit and Expertise (2.5) above Agility (2.2) and Mastery (1.1) below level 80
    (`Mgr/Item/StatsWeightCalculator.cpp:639-650`); Mastery 0 below 80 and Hit and Expertise only near 90.
16. **Keg Smash at Chi 3 and 4** wastes Chi (`MonkStrategies.cpp:47`): Method says 3 Chi or less.
17. To check live: Provoke cast without the stance (pull after a resurrection); the stance after mounting and dismounting;
    the `nc` stance line reads the specialisation when the strategy is built (`MonkStrategies.cpp:294-302`), so a respec
    or a level up through 10 needs the strategies rebuilt; how Dizzying Haze casts through a unit target; Provoke stall length.

## Implemented on `feat/class-sheet-hpal-brewmaster` (BT-133, compile pending)

What the code does now; the tables above describe the code before it.

- **Expel Harm** (`MonkStrategies.cpp`, `MonkActions.*`): a new trigger "brewmaster expel harm" (brewmaster strategy only)
  fires below 70 percent health with 40 Energy and the spell castable, at relevance 11, above every default action (5.0 to
  5.7) and above the shared 65 to 85 percent edge (10) that stays for all specs. No Chi builder line was added.
- **Pull** (`MonkStrategies.cpp`, `MonkPullStrategy::GetPullActionName`): with Provoke known but not castable the pull names
  Crackling Jade Lightning when that is castable (level 54, new action of the same name) and otherwise keeps naming Provoke,
  so the pull waits for the 8 second cooldown instead of failing as "shoot". Below level 14 the pull is still refused.
- **Area attacks** (`MonkAoeStrategy::InitTriggers`): for a brewmaster the three-enemy edge puts Rushing Jade Wind (5.48),
  the tank's Spinning Crane Kick (5.46) and Breath of Fire (5.44) under Guard (5.7), Purifying Brew (5.6), Shuffle (5.55) and
  Keg Smash (5.5). The tank's Spinning Crane Kick ("spinning crane kick tank") needs Shuffle up and Chi below 2. Windwalker
  keeps the old edge (23 to 25). The spec is read when the strategy is built.
- **Chi Wave** (`BrewmasterMonkStrategy::getDefaultActions`): "chi wave tank" at 5.45, after Keg Smash, "good" tier.
- **Config**: `PremadeSpecLink.10.0 = 002210`, `PremadeSpecGlyph.10.0 = 87893,87887,85685,87889,87890,87896`.
- Not changed, still open: the Shuffle line before level 34, Purifying Brew on Moderate Stagger, `dps assist` next to
  `tank assist`, Elusive Brew and Fortifying Brew thresholds, Dampen Harm timing, Guard after Tiger Palm, the spheres, the
  Black Ox Statue Provoke, the expert lines, stat weights, Keg Smash at 3 or 4 Chi and the to-check items.
