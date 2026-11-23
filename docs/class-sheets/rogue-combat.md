# Rogue: Combat (damage)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). The level 80 burst
  windows are left for later. The "bots today" columns below describe the code before the 2026-11-11 implementation.
- **Sources:**
  - Method, "Combat Rogue Class Guide - MoP Classic" (Phase 1, level 90 raid and dungeon, 23 July 2025), read
    2026-11-11 through a scripted fetch (the talent build is an image: only the talent hints in the text are usable).
  - wowtbc.gg, "Combat Rogue" in the MoP class guides (MoP Classic, no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, Talent, GlyphProperties,
    SpellCooldowns, SpellPower, SpellEffect, SpellEquippedItems; `data/db2/Item-sparse.db2` for glyph item ids) through
    `spec_data.py` and short read-only scripts.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch, checked 2026-11-10, not retried); Wowhead needs a
    browser.
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the
  tank's marks), 61 to 90 briefly from the guides.

Which source was richer: **talents (named always-picks), glyphs, stat order, area priority incl. the Crimson Tempest
threshold** Method (wowtbc lists every talent option without a pick and has no stat order); **opener, single target
conditions (Adrenaline Rush and Killing Spree at low energy, Vanish then Ambush, Redirect), the area tiers by target
count, the defensive order** wowtbc; **levels 15 to 60, Blade Flurry as a toggle with a price, what to do before a spell
is learned** neither guide says anything (both are level 90 raid guides), so those lines come from the 5.4.8 data and
are marked "derived". The two guides agree on the single target order and disagree only in wording: Killing Spree
"outside major buffs and cooldowns" (Method) against "at low energy when Adrenaline Rush and Shadow Blades are down"
(wowtbc); the second is the more exact and is used.

## Talents

Tiers and columns are from Talent.dbc. "Bots today" is `AiPlayerbot.PremadeSpecLink.4.1 = 020101`
(`conf/playerbots.conf.dist:2743`; one digit per tier, column counted from 0): Nightstalker, Combat Readiness, Cheat
Death, Shadowstep, Prey on the Weak, Marked for Death.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (020101) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Shadow Focus, "always" (Method) | Nightstalker, Subterfuge (wowtbc options) | 108209 / 14062 / 108208 | Nightstalker. **Differs** (Shadow Focus: abilities cost 75 percent less energy while stealthed, so every stealth opener is nearly free) |
| 2 (30) | Not named for Combat; the Subtlety guide takes Combat Readiness and wowtbc's defensive list has it fourth | Deadly Throw, Nerve Strike | 74001 / 26679 / 108210 | Combat Readiness, same. **Dead today:** no `combat readiness` action exists (`RogueAiObjectContext.cpp:142-190`), so it is never pressed |
| 3 (45) | Method: Elusiveness ("extra damage" through Feint) or Cheat Death (dungeon, "more useful here"); its poison line keeps Leeching Poison up, the tier's third talent | Leeching Poison, see the note | 79008 / 31230 / 108211 | Cheat Death, same as Method's dungeon pick (passive, 85 percent less damage for a short time when a hit would kill, 90 s) |
| 4 (60) | Shadowstep; Burst of Speed for fights with a lot of movement | Cloak and Dagger (wowtbc option) | 36554 / 108212 / 138106 | Shadowstep, same (`RogueTriggers.cpp:147-158`, `DpsRogueStrategy.cpp:133-134`) |
| 5 (75) | Prey on the Weak, or Dirty Tricks to control one specific add (Method) | Paralytic Poison | 131511 / 108216 / 108215 | Prey on the Weak, same. No effect: no bot code disables a target with Gouge, Kidney Shot, Sap or Blind |
| 6 (90) | Anticipation, "always" (Method); Marked for Death as the swap for frequent bursting of an important add | Shuriken Toss | 114015 / 137619 / 114014 | Marked for Death. **Differs** (Anticipation is passive: points built at 5 become charges that the next finisher turns into points) |

Notes on the picks (data, not guide):

- Shadow Focus: -75 percent energy cost while stealthed (effect of spell 112942); the bots stealth before a pull
  (`GenericRogueNonCombatStrategy.cpp:49`, BT-057).
- Cheat Death needs no code. Elusiveness would pay only if the bot used Feint often; it casts Feint at 45 percent health
  after Evasion (`DpsRogueStrategy.cpp:111-113`).
- Leeching Poison (spell 108211): a non-lethal poison whose weapon strikes heal the rogue. Method's rotation keeps it up. It
  needs a one-line action beside Deadly Poison; until then Cheat Death is the tier 45 pick.

Proposed link, guide aligned: `220102` (Shadow Focus / Combat Readiness / Cheat Death / Shadowstep / Prey on the Weak /
Anticipation). For a character below 75 only the first four digits are in effect.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and
75 (config comment `conf/playerbots.conf.dist:2631-2636`; the unlock levels are core-side). The config order is major,
minor, major, minor, minor, major; a glyph goes to the first free slot of its type.

| Type | Glyph | Item id | Why (one clause) | Bots today (`42972,45762,42968,43378,43380,42974`) |
|------|-------|--------:|------------------|------------------------|
| Major | Feint | 42963 | Method's best pair with Redirect; longer Feint, and the bots do cast it at low health | no |
| Major | Redirect | 42972 | Method's best pair; shorter cooldown of a spell the bots never cast (Redirect learned at 81, no action) | yes, **useless for the bots** |
| Major, situational | Smoke Bomb | 42968 | raid utility; the bots cast it only in battlegrounds and arenas (`RogueActions.cpp:70-73`) | yes, **useless in a dungeon** |
| Major, situational | Cloak of Shadows | 45769 | 40 percent less physical damage while Cloak is up; the bots press Cloak at 25 percent health (`DpsRogueStrategy.cpp:115-116`) | no |
| Major, situational | Sprint | 42974 | movement (Method and wowtbc); the bots sprint on every approach (BT-059) | yes (slot 3, level 75) |
| Major, derived | Deadly Momentum | 45766 | a kill that gives experience or honor refreshes Slice and Dice and Recuperate to full duration | no (in neither guide) |
| Major, situational | Blade Flurry | 42957 | Combat only: non-lethal poisons proc more while Blade Flurry runs; the bots run it too long (see the gap list) | no |
| Major, situational | Sharp Knives, Hemorrhaging Veins | 42958, 42970 | AoE (both guides); Hemorrhaging Veins is a Subtlety glyph | no |
| Minor | Safe Fall, Blurred Speed | 43378, 43379 | the only minors Method names | Safe Fall yes |
| Minor | Poisons | 43380 | wowtbc; faster poison application | yes |
| Minor | Killing Spree | 45762 | wowtbc (Combat only): Killing Spree returns the rogue to the starting spot, which keeps a bot where the tank left it | yes (slot 2, level 25), **does nothing until 80** |

Disagreement: Method's best majors are Feint and Redirect, wowtbc's Redirect, Smoke Bomb and Sprint; wowtbc adds Killing
Spree as a minor. The premade follows wowtbc. Method is used for the picks and the data decides what is a major or a
minor. The slot order is derived: a glyph in a slot must be useful at the level the slot unlocks. Proposed
`PremadeSpecGlyph.4.1`: `45766,43380,42963,43378,45762,45769` = Deadly Momentum, Poisons, Feint, Safe Fall, Killing
Spree, Cloak of Shadows.

## Stat order

Method: Hit and Expertise to about 7.5 percent each, Agility, Haste, Mastery, Critical Strike; optionally 26.5 percent
Hit so the off-hand never misses. wowtbc has no stat order. The caps are level 90 numbers and cannot be reached with the
gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **Two one-handed weapons**, any of the weapon types the strikes allow (Sinister Strike and Eviscerate accept daggers,
  swords, maces, axes and fist weapons; SpellEquippedItems). Ambidexterity (level 10) and Combat Potency (level 30: off-hand
  hits can give energy, "slower off-hand weapons have a proportionally higher chance") reward the off-hand.
- Then leather, Agility (attack power and critical strike), Haste, Mastery (Main Gauche, level 80: main-hand hits can
  strike again), Critical Strike last. Hit and Expertise matter only near level 90.
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:395-406`): Hit 2.1, Expertise 2.0, Agility 1.9, Haste 1.7, Mastery 1.6,
  Critical 1.4, Strength 1.1, attack power 1.0, armor penetration 1.8. The secondary order agrees with Method; **Agility is
  below Hit and Expertise**, so a low-level item with a hit or expertise stat beats an agility item (derived: wrong before
  the caps are in reach). Two-handers are cut to a twentieth for every rogue (`:812-829`). The weapon speed preference
  (`:1181-1190`, config `PreferredSpecWeapons`, default off) asks for a slow main hand (2.6 s or more) and a fast off hand
  (1.5 s or less): the off-hand half runs against the Combat Potency text above (inference, to check; harmless while
  the option stays off). Armor penetration is a stat the Cataclysm changes removed (inference, to check), harmless.

## What the specialisation knows, by level (5.4.8 data)

Ids are the spells the player actually casts; the data holds old rows of the same name. Energy costs are the SpellPower
values. The global cooldown of every energy ability is 1 s (SpellCooldowns).

| Level | Spell (id) |
|------:|------------|
| 1 | Sinister Strike (1752, 50 energy, 1 point), Throw (2764) |
| 3 | Eviscerate (2098, 35, finisher) |
| 5 | Stealth (1784) |
| 6 | Ambush (8676, 60, 2 points, from stealth) |
| 8 | Evasion (5277, 120 s) |
| 10 | Blade Flurry (13877, toggle, 10 s: up to 4 more enemies take 40 percent of the damage, energy regeneration -20 percent, lasts until cancelled); Ambidexterity (13852, off-hand damage up); Vitality (61329, energy regeneration and attack power up); Deadly Poison (2823); the specialisation is chosen here |
| 12 | Sap (6770) |
| 14 | Slice and Dice (5171, 25, finisher: +40 percent melee attack speed; 12 s per point, 36 s at 5) |
| 16 | Recuperate (73651, 30, finisher heal) |
| 18 | Kick (1766, 15 s) |
| 20 | Revealing Strike (84617, 40, 1 point: finishers do more to that target, Sinister Strike may give an extra point); Crippling Poison (3408) |
| 22 | Gouge (1776, 45) |
| 26 | Sprint (2983, 60 s) |
| 28 | Distract (1725) |
| 30 | Combat Potency (35551); Cheap Shot (1833, 40, from stealth); Wound Poison (8679) |
| 32 | Ruthlessness (14161, finishers may add a point) |
| 34 | Vanish (1856, 120 s) |
| 36 | Expose Armor (8647, 25) |
| 38 | Blind (2094) |
| 40 | Adrenaline Rush (13750, 180 s: energy regeneration +100 percent, melee attack speed +20 percent, global cooldown of the strikes and finishers shortened); Kidney Shot (408) |
| 44 | Feint (1966, 20) |
| 46 | Rupture (1943, 25, finisher bleed, 8 s plus 4 s per point) |
| 48 | Garrote (703, 45, from stealth); Safe Fall |
| 50 | Restless Blades (79096, damaging finishers shorten Adrenaline Rush, Killing Spree, Redirect, Shadow Blades and Sprint per point) |
| 58 | Cloak of Shadows (31224) |
| 60 | Bandit's Guile (84654, Sinister Strike and Revealing Strike build a damage bonus of up to 30 percent, then it fades and starts again); Shroud of Concealment; tier 4 talent |
| 66 | Fan of Knives (51723, 35, 1 point, area) |
| 68 | Preparation (14185, 300 s: resets Sprint, Vanish, Evasion and Dismantle only, spell text) |
| 74 | Shiv (5938) |
| 75 | tier 5 talent; Tricks of the Trade (57934, 78) |
| 80 | Killing Spree (51690, 120 s, 7 hits and +50 percent damage); Mastery: Main Gauche (76806) |
| 81 | Redirect (73981) |
| 83 | Crimson Tempest (121411, 35, area finisher) |
| 85 | Smoke Bomb (76577) |
| 87 | Shadow Blades (121471, 180 s) |
| 90 | tier 6 talent |

Facts the priority lists rest on (spell texts and data tables):

- Combo points stay on the target; finishers cost 25 to 35 energy. Slice and Dice at 5 points lasts 36 s, longer than
  most dungeon trash fights; a Slice and Dice at 3 points (24 s) is enough there (derived).
- Blade Flurry is a toggle: energy regeneration -20 percent (effect of spell 13877) for as long as it runs, in return for
  hits on up to four more enemies. A single target gains nothing and loses a fifth of its energy.
- Adrenaline Rush doubles energy regeneration, so it belongs where energy has run out (wowtbc: "when low on energy").
- Restless Blades (50) turns every damaging finisher into a shorter cooldown on Adrenaline Rush; the finisher cadence is
  worth more than a second cast.
- Preparation does not reset Shadowstep.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked
"derived" comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to
`mod-aware-bots-548/src/` at commit `605b17f`.

How the bots rank what they do (read, `strategy/Strategy.h:37-51`): ACTION_DEFAULT 5, ACTION_NORMAL 10, ACTION_HIGH 20,
ACTION_MOVE 30, ACTION_INTERRUPT 40, ACTION_EMERGENCY 90. A trigger edge adds its action with that number; the engine
takes the highest action that is possible and useful. A level 10 and up rogue of this specialisation gets the `combat`
strategy (`DpsRogueStrategy`, `Factory/AiFactory.cpp:481-491`) together with `dps assist`, `aoe` and, from the shared
melee rule, `behind` (`:567`); every bot outside battlegrounds also gets `boost` (`:316`).

Every builder edge but Revealing Strike is gated by "high energy available", which is 60 energy or more
(`GenericTriggers.h:138-142`), above the cost of Sinister Strike (50): about half a second of idle per builder
(derived; the regeneration rate is to check).

### Levels 15 to 29

Known: Sinister Strike, Eviscerate, Stealth, Ambush (6), Evasion, Blade Flurry (10), Slice and Dice (14), Recuperate
(16), Kick (18), Revealing Strike (20), Gouge (22), Sprint (26).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | out of combat, the poison is missing | Deadly Poison (2823, 10), plus a non-lethal poison from 20 (derived: Crippling Poison) | yes: "often" edges, Deadly at 20 and Crippling at 19 (`GenericRogueNonCombatStrategy.cpp:70`, `:72`) |
| 2 | core | a target within about 30 yd and not in combat | Stealth (1784, 5) before the pull (both guides) | yes (`GenericRogueNonCombatStrategy.cpp:49`, `RogueTriggers.cpp:48-88`); BT-057 live check pending |
| 3 | good | stealthed, behind the target | Ambush (8676, 6): 2 points (both guides open with Ambush) | yes: the "in stealth" edge swaps in the stealthed strategy, then `behind target` -> Ambush at 20 (`DpsRogueStrategy.cpp:86-87`, `RogueActions.cpp:39-53`, `DpsRogueStrategy.cpp:240-243`). BT-059 live check pending |
| 4 | core | Slice and Dice missing, 3 points or more | Slice and Dice (5171, 14) (guides: at 5 points; 3 points derived, see above) | yes at 25 (`DpsRogueStrategy.cpp:96-97`, trigger `RogueTriggers.cpp:210-214`, BT-058) |
| 5 | core | Revealing Strike missing on the target, from 20 | Revealing Strike (84617, 20): right after Slice and Dice (both openers) | yes at 14 (`DpsRogueStrategy.cpp:135-136`, trigger `RogueTriggers.cpp:137-140`) |
| 6 | core | 5 points | Eviscerate (2098, 3) (Rupture comes at 46) | yes: "combo points 5 available" -> Rupture 21, then Eviscerate 20 (`:101-103`) |
| 7 | core | energy for a builder | Sinister Strike (1752, 1) | yes at 13 (`:93-94`), gated at 60 energy |
| 8 | good | target dies within 3 s with 3 points | finisher before the fight ends (derived) | yes at 22 (`:105-106`) |
| 9 | good | an enemy casts, from 18 | Kick (1766, 18) | yes, the current target at 42 and an enemy healer at 41 (`:118-123`) |
| 10 | good | health at or under 45 percent | Evasion (5277, 8), then Feint (44) | yes (`:111-113`), at 29 and 28 |
| 11 | core | two or more enemies within 8 yd; one enemy | Blade Flurry (13877, 10) on; off again when a single enemy is left (derived from the -20 percent energy price) | **wrong:** on at 24 for two enemies (`:263-265`) and from the `boost` strategy (`:272-276`), and no code ever cancels it; to check live that the aura survives the fight |
| 12 | good | health at or under 30 percent, 5 points (derived) | Recuperate (73651, 16) | **missing for Combat:** only the Assassination strategy has the edge (`AssassinationRogueStrategy.cpp:135-136`) |
| 13 | expert | a second enemy attacks, peel | Gouge (1776, 22) | **missing:** the action exists (`RogueAiObjectContext.cpp:144`) but no strategy has an edge |
| 14 | expert | a mob alone before the pull | Sap (6770, 12) | **missing for the party:** `RogueCcStrategy` has the edge (`DpsRogueStrategy.cpp:279-282`) but `AiFactory.cpp:481-491` never adds `cc` to a rogue |

Before a spell is learned (derived): below 10 there is no specialisation, the kit is Sinister Strike, Eviscerate and
Stealth; Revealing Strike waits for 20. The bots tolerate all of these because an unknown spell is simply not cast.

### Levels 30 to 44

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 15 | good | stealthed, not behind | Cheap Shot (1833, 30): stun and 2 points | yes (`:242-243`) |
| 16 | good | the group has no Weakened Armor source | Expose Armor (8647, 36), 1 point (Method: only if assigned) | yes, sees any caster's Weakened Armor (`RogueTriggers.cpp:216-224`, edge `DpsRogueStrategy.cpp:156-157` at 23) |
| 17 | good | from 40, a pack of elites or a boss | Adrenaline Rush (13750, 40) at low energy (wowtbc: "when low on energy"; Method: on cooldown) | partly: from `boost`, only when the group is out-levelled two to one (balance 50 or less), so a normal 5-player pull never gets it (`GenericTriggers.cpp:416-424`, `AttackerCountValues.cpp:55-109`; inference, to check live) |
| 18 | good | a second or third enemy attacks the rogue | Vanish (1856, 34) as a threat dump (derived; neither guide uses it so) | partly: fires as soon as two enemies attack the rogue (`:108-109`), a 2 minute cooldown spent often |
| 19 | good | health at or under 45 percent | Feint (1966, 44) after Evasion | yes (`:111-113`) |

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 20 | core | 5 points, Rupture missing | Rupture (1943, 46) before Eviscerate (both guides: at 5 points) | yes (`DpsRogueStrategy.cpp:101-103`; node falls back to Eviscerate `:61-67`) |
| 21 | good | stealthed | Garrote (703, 48) as the alternative opener | yes: `high energy available` -> Garrote 27, Ambush 26 (`:89-91`), needs stealth |
| 22 | good | from 50 | Restless Blades (79096) | passive; works when finishers are cast, which line 20 does |
| 23 | good | health at or under 25 percent | Cloak of Shadows (31224, 58) | yes (`:115-116`) |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 24 | core | Sinister Strike and Revealing Strike keep coming | Bandit's Guile (84654, 60): the damage bonus grows only through those two strikes | yes: they are the builders |
| 25 | good | tier 4 talent | Shadowstep (36554) as a gap closer, 8 to 25 yd | yes (`RogueTriggers.cpp:147-158`) |
| 26 | good | from 66, three or more enemies within 8 yd | Fan of Knives (51723, 66) replaces Sinister Strike (wowtbc, 4 to 6 targets) | yes at 15 in the `aoe` strategy (`DpsRogueStrategy.cpp:266-267`), no Rupture check |
| 27 | good | from 68 | Preparation (14185, 68) after Vanish | wrong: the trigger also fires for Sprint and Shadowstep (`RogueTriggers.cpp:189-203`); see the cooldown section |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 78 | Tricks of the Trade on the tank before the pull and after Vanish (both guides) | on the main tank when the rogue's threat passes half of the tank's (`DpsRogueStrategy.cpp:158-161`); no pre-pull cast |
| 80 | Killing Spree at low energy when Adrenaline Rush and Shadow Blades are down (wowtbc) | default action at 5.1 whenever it can be cast (`:77`), regardless of the other buffs |
| 81 | Redirect when a target switch keeps points | missing (no action) |
| 83 | Crimson Tempest at 5 points from 7 targets (wowtbc; Method: more than 6) | missing (no action) |
| 87 | Shadow Blades in the opener and on cooldown | **missing for Combat** (the edge exists only in the Assassination strategy, `AssassinationRogueStrategy.cpp:137-138`) |
| 90 | single target (wowtbc order): Shadow Blades, Adrenaline Rush at low energy, Killing Spree at low energy, Vanish then Ambush, Preparation, Slice and Dice at 5, Revealing Strike, Rupture at 5, Eviscerate at 5, Sinister Strike; area: Blade Flurry on, Fan of Knives from 4 targets, Crimson Tempest from 7 | Slice and Dice, Revealing Strike, Rupture, Eviscerate, Sinister Strike, Fan of Knives, Killing Spree and the cooldowns are there; Shadow Blades, Vanish-Ambush, Crimson Tempest are not |

## Cooldowns and defensives

The guides say "on cooldown"; the timing for a dungeon bot is derived (trash fights last 20 to 40 s).

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Adrenaline Rush | 40 (180 s) | on cooldown at low energy | at a pack of elites or a boss, when energy is empty | only when outmatched 2 to 1 (`boost`), see line 17 |
| Killing Spree | 80 (120 s) | outside Adrenaline Rush and Shadow Blades, low energy | same | default at 5.1, no condition; the dungeon code already blocks it in some encounters (`strategy/dungeons/CataGb/CataGbMultipliers.cpp:62`) |
| Shadow Blades | 87 (180 s) | on cooldown, opener | on cooldown at a pack of elites or a boss | missing for Combat |
| Blade Flurry | 10 | on for two or more targets | on for two or more enemies in 8 yd, off for one | on at 2 or more, never off |
| Vanish | 34 (120 s) | at low energy, then Ambush | threat dump at two attackers | at "medium threat" only (`:108-109`) |
| Preparation | 68 (300 s) | when Vanish is on cooldown | only when Vanish and Evasion are both on cooldown | fires when Vanish, Sprint **or Shadowstep** is on cooldown (`RogueTriggers.cpp:189-203`): Shadowstep is not reset by the spell, and a Sprint on every approach (BT-059) makes it fire on a 60 s cooldown |
| Evasion | 8 (120 s) | defensive (buffed) | health at or under 45 percent | yes (`:111-113`) |
| Feint | 44 | glyph option, Elusiveness | after Evasion, same line | yes |
| Cloak of Shadows | 58 (60 s) | second in wowtbc's defensive list | health at or under 25 percent | yes (`:115-116`) |
| Smoke Bomb | 85 (180 s) | first in wowtbc's defensive list | group takes heavy damage | battlegrounds and arenas only |
| Combat Readiness | 30 (talent, 120 s) | fourth in wowtbc's defensive list | health at or under 45 percent | missing (no action) |
| Kick | 18 (15 s) | interrupt | any casting enemy in melee range | yes, the target and healers; any other caster in PvP only (`RogueActions.cpp:75-79`) |

## Healer triage and mana (healers only)

Not applicable. Energy is the resource; the combo points are the second one. Costs: Sinister Strike 50, Revealing Strike 40,
Fan of Knives 35, Eviscerate 35, Rupture 25, Slice and Dice 25, Ambush 60, Garrote 45, Cheap Shot 40, Expose Armor 25,
Recuperate 30, Feint 20.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Killing Spree (80), Redirect (81), Crimson Tempest (83), Smoke Bomb (85), Shadow Blades (87)
  and Anticipation (90) are not available in the 15 to 60 band; the lists above start without them.
- Method keeps Leeching Poison up while its talent text offers Cheat Death or Elusiveness for the same tier (45): a rogue
  can have only one of them.
- Glyph of Killing Spree is a minor glyph here (wowtbc lists it among minors, correct), and Glyph of Hemorrhaging Veins and
  Sharp Knives are major.
- Preparation resets Sprint, Vanish, Evasion and Dismantle (spell text); the bots' trigger counts Shadowstep.
- Method's Hit and Expertise 7.5 percent and 26.5 percent off-hand rule are level 90 numbers.
- Method's opener lists Shadow Blades; it is learned at 87 here.
- The module's weapon speed preference (slow main hand, fast off hand) is copied from a Wrath of the Lich King rule; the
  5.4.8 text of Combat Potency favours a slow off hand.

## Gap list (ordered by value for levels 15 to 60)

Config (can go at once):

1. Tier 15: Shadow Focus instead of Nightstalker, tier 90: Anticipation instead of Marked for Death (both "always" in
   Method). `PremadeSpecLink.4.1` (`conf/playerbots.conf.dist:2743`) to `220102`.
2. Glyphs: `PremadeSpecGlyph.4.1` (`:2744`) to `45766,43380,42963,43378,45762,45769`. Today a level 25 to 79 Combat bot
   carries Redirect, Smoke Bomb and Killing Spree, none of which does anything before level 80 or in a dungeon.
3. Tier 30: keep Combat Readiness only together with rotation item 5; otherwise it is a dead talent.

Rotation (class fixes, wait for the owner's in-game test):

1. Blade Flurry as a real toggle (core, biggest for this specialisation): the spell lasts until cancelled and costs 20
   percent of the energy regeneration, but `DpsRogueStrategy.cpp:263-265` (`light aoe`, two enemies) and `:272-276` (the
   `boost` edge, which fires without any enemy count) only switch it on. Fix: drop the `boost` edge, keep the `light aoe`
   edge (maybe at three), and add a trigger "fewer than two enemies in 8 yd while the aura is up" with an action that
   removes the aura. Files `DpsRogueStrategy.cpp::RogueAoeStrategy` / `RogueBoostStrategy`, `RogueTriggers.*`,
   `RogueActions.*`. To check live first: does the aura survive the end of a fight and leaving combat.
2. Preparation trigger (`RogueTriggers.cpp:189-203`, `PreparationTrigger::IsActive`): drop Sprint and Shadowstep from the
   names, require Vanish on cooldown (and Evasion for a bot below 45 percent).
3. Adrenaline Rush (`DpsRogueStrategy.cpp:272-276`, `AdrenalineRushTrigger`): a rule for dungeon packs of three or more or
   elites, in addition to the "outmatched 2 to 1" rule, once energy is below 40 (wowtbc). Good tier.
4. A main hand and off hand scoring for Combat that keeps Agility above Hit and Expertise before level 80
   (`Mgr/Item/StatsWeightCalculator.cpp:395-406`). Gear scoring.
5. `combat readiness` action (self buff like `CastEvasionAction`, `RogueActions.h:13-17`) on "low health" beside Evasion
   (`DpsRogueStrategy.cpp:111-113`), if the talent stays picked.
6. Shadow Blades for Combat (level 87): an edge like `AssassinationRogueStrategy.cpp:137-138` in
   `DpsRogueStrategy::InitTriggers`; and Killing Spree gated by the Adrenaline Rush and Shadow Blades auras instead of an
   unconditional default (`:77`). Outside the band.
7. Recuperate edge for Combat (`DpsRogueStrategy.cpp`, copy of `AssassinationRogueStrategy.cpp:135-136`). Good tier.
8. Energy gate: builders are held to 60 energy by "high energy available" (`GenericTriggers.h:138-142`) although Sinister
   Strike costs 50; a per-builder gate removes the half-second idle. Minor; the regeneration rate is to check.
9. Vanish at two attackers (`:108-109`): the 120 s cooldown is spent on every double pull. Owner decision whether a damage
   bot should dump threat that way.
10. Leeching Poison action beside Deadly Poison (`GenericRogueNonCombatStrategy.cpp:72`) for the tier 45 pick; peel and
    crowd control (Gouge, Kidney Shot, `cc` for Sap, expert tier). Only worth it with Prey on the Weak or Dirty Tricks.
11. Pre-pull Tricks of the Trade on the tank (78), Redirect (81), Crimson Tempest (83): level 75 and up, outside the band.
12. Skill tiers: the rogue strategies have no tier gating (`BotSkill` is used only in the tank code). Lines marked good
    and expert above (Kick on any caster, Expose Armor, Adrenaline Rush timing, Vanish) are the ones a casual bot should
    drop.
13. To check live: BT-057 (stealth before the pull) and BT-059 (the stealthed opener swap) are deployed with their live
    check pending; lines 2 and 3 depend on them.

## PvP playbook (arena, 5.4.8)

New section, 2026-11-19. Sources: player guides (5.x: four consistent videos of patches 5.4.7 and 5.4.8 by one player covering talents, glyphs, stat order, rotation, Insight and openers, one early MoP video, a battleground commentary without tactics), a community raid guide for MoP Classic (rotation basics only), the 5.x patch notes, the server's own data (`data/dbc/` through `spec_data.py` and short read-only scripts) and the module code read on `main`. Lines say "(guide)" when a player guide gives them, "(adapted)" when they come from the Subtlety rogue arena material and were checked against the Combat spell list, "(derived)" when they follow from the 5.4.8 data and game knowledge. About 50 percent of the section is sourced, 10 percent adapted, 40 percent derived. Where nothing reliable exists the line says GAP.

Where the spec stands: no rogue of any specialisation is in the MoP Classic EU top 100 ladder of 2026-11-18 (96 players in 3v3, 54 in 2v2), a community tier list of July 2025 rates Combat C in 3v3 and B in 2v2 (the only rogue spec above C). A 5.4 arena guide calls it good in duels and not viable in trees because it lacks the burst of the other two. A niche pick: the value of a rule is lower than for a ladder spec.

### Main plan (guide)

1. Combat is a ramp spec: Revealing Strike on the target (finishers +35 percent, Kidney Shot 8 s at five points), Sinister Strike to build, Slice and Dice up, finishers spent on Eviscerate, Kidney Shot or Recuperate.
2. Insight climbs one stage (10, 20, 30 percent damage) for every four Sinister or Revealing Strikes; spend Shadow Blades and Killing Spree at moderate or deep Insight and use Adrenaline Rush in the opener to ramp faster.
3. Damaging finishers shorten Adrenaline Rush, Killing Spree, Shadow Blades, Redirect and Sprint by 2 s per point (data), so spending points is also the cooldown rotation.

### Opener (guide, derived)

- Against melee: Revealing Strike, Cheap Shot, then Sinister Strike or Slice and Dice; against a caster or healer: Revealing Strike, Garrote, Cheap Shot, then build (guide).
- The quicker Insight trick (guide): open with Adrenaline Rush and press Revealing Strike repeatedly instead of Sinister Strike; it costs 40 energy against 50 (5.4.0 data), adrenaline fills the energy, and Insight reaches moderate in the opener.
- A Paralytic Poison Shiv roots for 4 s, which stops a warrior's Charge after a stun and a mage's Blink (an early MoP guide; data).
- Energy (derived): Combat has 100 maximum energy, so the triple opener (Garrote 45, Cheap Shot 40, Ambush 60) is not affordable; Shadow Focus (-75 percent while stealthed) is the opener talent for a bot. Premeditation (30) gives the early points. Prep room (adapted): Tricks of the Trade on the partner, do not open first when another stealth class is unrevealed.
- GAP: no source for the start room or the first position.

### Priority outside a window (guide, derived)

| Priority | Tier | Line |
|---|---|---|
| 1 | core | Revealing Strike on the target (24 s); Slice and Dice up (three or more points) |
| 2 | core | Sinister Strike to build; at five points Eviscerate (damage and cooldown time), Kidney Shot (stun) or Recuperate (heal) |
| 3 | good | Adrenaline Rush in the opener or when energy falls under 40; Revealing Strike spam under it to ramp |
| 4 | expert | Killing Spree and Shadow Blades at moderate or deep Insight, not on cooldown; Blade Flurry with two or more enemies near (up to four extra targets at 40 percent; since 5.3 the extra hits ignore resilience when the main target is a player) |
| 5 | good | Kick on a heal or a crowd control cast; Garrote on the healer from stealth |
| 6 | expert | Shiv root after a stun on a warrior or mage; Wound Poison against a healer; Rupture in deep Insight (it keeps the Insight it was cast under: raid guide) |

Stat order (guides): the 5.4.8 guide says mastery, crit, haste (mastery and crit work on every ability including Killing Spree, haste does not), the 5.4.7 guide of the same player says mastery, haste, crit, the 5.2 guide says mastery and crit with haste weak: take mastery, then crit, then haste (derived from the latest guide). Agility gems; hit and expertise 3 percent each.

### Go window (guide, derived)

Trigger (derived): moderate or deep Insight, the healer stunned, blinded or silenced by the team, the kill target without an immunity. Order: Adrenaline Rush (if not already), Shadow Blades, Killing Spree (7 strikes over 3 s, +50 percent damage, 120 s; it ends beside the target, and the Glyph of Killing Spree returns you to the start), Eviscerate at five points between them. Killing Spree is also a defensive: the target loses you for its length (guide). Keep Killing Spree out of a team fight unless it is the closing move.

### Crowd control (guide, adapted)

Cheap Shot (4 s, glyph +1 s) then Kidney Shot at five points with Revealing Strike (about 8 s, halved by the stun category if second); Kidney first and Cheap Shot after is the longer chain outside stealth. Gouge (22), Blind (38, healers; the glyph clears damage over time), Sap (12), Garrote silence 3 s (glyph +1 s). Nerve Strike: the stunned target deals 25 percent less damage on players (50 before 5.4.7); Combat Readiness is the pick against melee cleaves (guide). Paralytic Poison: a stun at four stacks outside the stun category and the Shiv root. Dismantle (52) against a warrior or ret burst.

### Defensives

Evasion (8, 2 min, 10 s, melee only), Cloak of Shadows (58), Vanish (34), Combat Readiness (up to 50 percent less melee damage), Elusiveness (Feint, 30 percent for 5 s), Smoke Bomb (85), Cheat Death (survive at 10 percent), Recuperate, Burst of Speed (clears slows) or Shadowstep; Killing Spree as an escape (guide). Evasion, Cloak and Vanish by role: adapted.

### Matchups and partners

Guide: Combat Readiness against melee cleaves, Nerve Strike against casters and melee in general; a warrior fears after a stun, so root it with a Shiv and do not stun into a fear; open Garrote on a mage and Shiv it. Derived: against a melee train: Evasion, Combat Readiness, Dismantle the burster, Shiv root; partners that stun and lock the healer give the ramp the time it needs; a second rogue shares the stun category.

### PvP set (no ladder data)

Premade today: `PremadeSpecLinkPvp.4.1 = 112201` (Subterfuge, Nerve Strike, Elusiveness, Burst of Speed, Prey on the Weak, Marked for Death) and glyphs `64493,43376,42969,43379,43378,45769` (Blind, Distract, Cheap Shot, Blurred Speed, Safe Fall, Cloak of Shadows); it was copied from the Subtlety guide. The Combat guides agree on five of the six tiers (Subterfuge, Nerve Strike or Combat Readiness, Elusiveness, Burst of Speed, Prey on the Weak or Paralytic Poison, Marked for Death) and name the Garrote and Redirect glyphs that the set lacks.

### Differences from the guides (5.4.8)

- Sinister Strike costs 50 energy and Revealing Strike 40 (5.4.0); Revealing Strike lasts 24 s (5.3); Nerve Strike is 25 percent on players (5.4.7); Killing Spree is 7 strikes over 3 s with +50 percent damage (a guide says 3.5 s); Shadowstep has a 20 s cooldown (a guide says 24); Cloak and Dagger needs Stealth (5.3); Combat has no dagger bonus, so 100 maximum energy.
- Level gates: Revealing Strike 20, Ruthlessness 32, Adrenaline Rush 40, Restless Blades 50, Bandit's Guile (Insight) 60, Preparation 68, Killing Spree 80, Redirect 81, Smoke Bomb 85, Shadow Blades 87.

### Bots today (read on `main`, 2026-11-19)

| Rule | Bots today |
|---|---|
| Revealing Strike upkeep, Sinister Strike, Slice and Dice | exists (`DpsRogueStrategy.cpp:94,97,139`; `RogueTriggers.cpp:145-148`) |
| Rupture then Eviscerate at five points | exists (`DpsRogueStrategy.cpp:101-103`) |
| Kidney Shot as the go stun, 8 s with Revealing Strike | partly (Kidney is the fallback of Kick, `:42`, with the arena guard `RogueActions.cpp:65-69`; no Revealing Strike aware use) |
| Adrenaline Rush in the opener, Revealing Strike spam | partly: it fires at energy under 40 for a good tier bot (`RogueTriggers.cpp:263-275`; edge `DpsRogueStrategy.cpp:281`) |
| Killing Spree and Shadow Blades by Insight | partly and in the wrong order: Killing Spree is the default action (`DpsRogueStrategy.cpp:77`), Shadow Blades fires whenever in combat (`RogueTriggers.cpp:210-213`); no Insight read |
| Blade Flurry on two or more targets | exists (`DpsRogueStrategy.cpp:271-273`) |
| Kick on a heal or a crowd control cast; stealth openers | exists (`:122-131`, `:239-254`) |
| Evasion, Combat Readiness, Cloak of Shadows, Vanish, Smoke Bomb | partly (`:109-119,147`; no melee or caster split) |
| Shiv root, Blind on the healer, Redirect, Wound Poison | missing (Blind has an action but no edge, `RogueAiObjectContext.cpp:187`; no Redirect or Shiv action; Deadly and Crippling Poison only, `GenericRogueNonCombatStrategy.cpp:76-78`) |
| Preparation | exists (`RogueTriggers.cpp:197`) |

### Gap list (ordered by value; rotation items wait for the owner's in-game test)

Config:

1. `PremadeSpecLinkPvp.4.1` tier 15 to Shadow Focus (digit 2) for bots with 100 maximum energy (derived); tier 30 stays Nerve Strike (or Combat Readiness against melee, both have code).
2. Glyphs: add Garrote (guide) and the Glyph of Killing Spree (a minor glyph: returns you to the start, derived); item ids to look up.

Rotation:

1. An Insight aware go: Adrenaline Rush at the opener, Killing Spree and Shadow Blades at moderate or deep Insight (expert).
2. A Revealing Strike aware Kidney Shot as the stun, and Blind on the healer in a window (expert).
3. Eviscerate spending that shortens Adrenaline Rush, Killing Spree and Shadow Blades (good).
4. A Shiv root after a stun on a warrior or mage, Wound Poison against a healer (expert).
5. Evasion against melee, Cloak against casters, Combat Readiness against a melee train (good).
