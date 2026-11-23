# Rogue: Subtlety (damage)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). The level 80 burst
  windows are left for later. The "bots today" columns below describe the code before the 2026-11-11 implementation.
- **Sources:**
  - Method, "Subtlety Rogue Class Guide - MoP Classic" (Phase 1, level 90 raid and dungeon, 23 July 2025), read
    2026-11-11 through a scripted fetch (the talent build is an image: only the talent hints in the text are usable).
  - wowtbc.gg, "Subtlety Rogue" in the MoP class guides (MoP Classic, no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, Talent, GlyphProperties,
    SpellCooldowns, SpellPower, SpellEffect, SpellEquippedItems; `data/db2/Item-sparse.db2` for glyph item ids) through
    `spec_data.py` and short read-only scripts.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch, checked 2026-11-10, not retried); Wowhead needs a
    browser.
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the
  tank's marks), 61 to 90 briefly from the guides.

Which source was richer: **talents (Shadow Focus mandatory, the top rows), glyphs, stat order, area priority incl. the
Crimson Tempest threshold** Method (wowtbc lists every talent option without a pick and has no stat order); **opener,
single target with energy thresholds (Shadow Dance at 75 energy, Vanish at 45, Premeditation below 4 points, Ambush
below 5 points or at 3 Anticipation charges), area tiers by target count, defensive order** wowtbc; **levels 15 to 60,
Hemorrhage as the builder before Backstab, what to do before a spell is learned** neither guide says anything (both are
level 90 raid guides), so those lines come from the 5.4.8 data and are marked "derived". The two guides agree on the
single target order; they differ in the finisher rule: Method "Eviscerate when Slice and Dice and Rupture are not about to
expire", wowtbc "Eviscerate at 5 points"; wowtbc is used because it is a clear threshold.

## Talents

Tiers and columns are from Talent.dbc. "Bots today" is `AiPlayerbot.PremadeSpecLink.4.2 = 020102`
(`conf/playerbots.conf.dist:2748`; one digit per tier, column counted from 0): Nightstalker, Combat Readiness, Cheat
Death, Shadowstep, Prey on the Weak, Anticipation.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (020102) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Shadow Focus, "mandatory" (Method) | Nightstalker, Subterfuge (wowtbc options) | 108209 / 14062 / 108208 | Nightstalker. **Differs** (Shadow Focus: abilities cost 75 percent less energy while stealthed; for a stealth specialisation that is the bulk of the opener, and Shadow Dance at 80 stacks with it) |
| 2 (30) | Combat Readiness (Method: top rows; wowtbc defensive list fourth) | Deadly Throw, Nerve Strike | 74001 / 26679 / 108210 | Combat Readiness, same. **Dead today:** no `combat readiness` action exists (`RogueAiObjectContext.cpp:142-190`), so it is never pressed |
| 3 (45) | Method: Elusiveness ("better if used correctly") or Cheat Death; its poison line keeps Leeching Poison up, the tier's third talent | Leeching Poison, see the note | 79008 / 31230 / 108211 | Cheat Death (passive, 85 percent less damage for a short time when a hit would kill, 90 s). Allowed by Method; no code needed |
| 4 (60) | Shadowstep (Method: Burst of Speed only if a mechanic leaves no target) | Cloak and Dagger (wowtbc option) | 36554 / 108212 / 138106 | Shadowstep, same (`RogueTriggers.cpp:147-158`, `AssassinationRogueStrategy.cpp:121-122`) |
| 5 (75) | Prey on the Weak, or Dirty Tricks to keep one target controlled (Method; more use in dungeons) | Paralytic Poison | 131511 / 108216 / 108215 | Prey on the Weak, same. No effect: no bot code disables a target with Gouge, Kidney Shot, Sap or Blind |
| 6 (90) | Anticipation (Method: top rows; wowtbc: Ambush "at 3 Anticipation charges") | Marked for Death, Shuriken Toss | 114015 / 137619 / 114014 | Anticipation, same |

Notes on the picks (data, not guide):

- Shadow Focus: -75 percent energy cost while stealthed (effect of spell 112942); the bots stealth before a pull
  (`GenericRogueNonCombatStrategy.cpp:49`, BT-057).
- Leeching Poison (spell 108211): a non-lethal poison whose weapon strikes heal the rogue. Method's rotation keeps it up.
  It needs a one-line action beside Deadly Poison; until then Cheat Death is the tier 45 pick.
- Honor Among Thieves (level 50, passive): critical hits of any party member give the rogue a combo point, at most once
  per 2 s, in combat. In a dungeon group it works with no code, and it is why a Subtlety bot with Anticipation reaches five
  points sooner.

Proposed link, guide aligned: `220102` (Shadow Focus / Combat Readiness / Cheat Death / Shadowstep / Prey on the Weak /
Anticipation). For a character below 75 only the first four digits are in effect.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and
75 (config comment `conf/playerbots.conf.dist:2631-2636`; the unlock levels are core-side). The config order is major,
minor, major, minor, minor, major; a glyph goes to the first free slot of its type.

| Type | Glyph | Item id | Why (one clause) | Bots today (`42970,43378,42972,43380,43379,42968`) |
|------|-------|--------:|------------------|------------------------|
| Major | Hemorrhaging Veins | 42970 | Method's pick for Subtlety: Sanguinary Vein (level 60) also raises damage on targets with Hemorrhage | yes, in the first slot (level 25): **does nothing before 60** |
| Major | Feint | 42963 | Method's best pair with Redirect; longer Feint, and the bots do cast it at low health | no |
| Major | Redirect | 42972 | Method's best pair; shorter cooldown of a spell the bots never cast (Redirect learned at 81, no action) | yes, **useless for the bots** |
| Major, situational | Smoke Bomb | 42968 | raid utility; the bots cast it only in battlegrounds and arenas (`RogueActions.cpp:70-73`) | yes, **useless in a dungeon** |
| Major, situational | Cloak of Shadows | 45769 | 40 percent less physical damage while Cloak is up; the bots press Cloak at 25 percent health (`AssassinationRogueStrategy.cpp:100-101`) | no |
| Major, derived | Deadly Momentum | 45766 | a kill that gives experience or honor refreshes Slice and Dice and Recuperate to full duration; Energetic Recovery (70) pays energy while Slice and Dice runs | no (in neither guide) |
| Major, situational | Sharp Knives, Sprint | 42958, 42974 | AoE (both guides); movement | no |
| Minor | Safe Fall, Blurred Speed | 43378, 43379 | the only minors Method names | yes |
| Minor | Poisons | 43380 | wowtbc; faster poison application | yes |

Disagreement: Method's best majors are Feint and Redirect, wowtbc's Hemorrhaging Veins, Redirect and Smoke Bomb. The premade
follows wowtbc. Method is used for the picks and the data decides what is a major or a minor (Glyph of Hemorrhage is a
minor glyph, not Hemorrhaging Veins). The slot order is derived: a glyph in a slot must be useful at the level the slot
unlocks. Proposed `PremadeSpecGlyph.4.2`: `45766,43380,42963,43378,43379,42970` = Deadly Momentum, Poisons, Feint, Safe
Fall, Blurred Speed, Hemorrhaging Veins (slot unlocks at 75, Sanguinary Vein is already known since 60).

Item-name typos in the data: item 42970 is spelled "Glyph of Hemorraghing Veins" and 42958 "Glyph of Sharp Knives"; the
premade comment copies the first typo.

## Stat order

Method: Hit and Expertise to about 7.5 percent each, Agility, Haste, Critical Strike, Mastery; optionally 26.5 percent
Hit so the off-hand never misses. wowtbc has no stat order. The caps are level 90 numbers and cannot be reached with the
gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **Daggers in both hands.** Backstab (level 40) needs a main-hand dagger (SpellEquippedItems: item subclass mask 32768),
  Ambush and Hemorrhage hit about 45 percent harder with a dagger (spell texts). Before 40 any one-hander works for the
  builder, but the stealth opener already prefers a dagger.
- Then leather, Agility (attack power and critical strike), Haste, Critical Strike, Mastery last (Executioner, level 80:
  finishers and Slice and Dice hit harder). Hit and Expertise matter only near level 90.
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:422-433`, **shared with Assassination**): Hit 2.1, Expertise 2.1, Mastery
  2.0, Haste 1.8, Agility 1.5, Strength 1.1, Critical 1.1. For Subtlety that puts **Mastery above Haste above Critical, the
  opposite of Method's Haste, Critical, Mastery**, and Agility below Hit and Expertise (derived: wrong before the caps are in
  reach). Daggers get x1.5 for both hands from level 10 (`:876-881`). Two-handers are cut to a twentieth for every rogue
  (`:812-829`). Armor penetration (1.2) belongs to a stat the Cataclysm changes removed (inference, to check), harmless.

## What the specialisation knows, by level (5.4.8 data)

Ids are the spells the player actually casts; the data holds old rows of the same name. Energy costs are the SpellPower
values. The global cooldown of every energy ability is 1 s (SpellCooldowns).

| Level | Spell (id) |
|------:|------------|
| 1 | Sinister Strike (1752, 50 energy), Throw (2764) |
| 3 | Eviscerate (2098, 35, finisher) |
| 5 | Stealth (1784) |
| 6 | Ambush (8676, 60, 2 points, from stealth, +45 percent damage with a dagger) |
| 8 | Evasion (5277, 120 s) |
| 10 | Hemorrhage (16511, 30, 1 point, replaces Sinister Strike, +45 percent damage with a dagger, leaves a bleed); Sinister Calling (31220, Agility up); Master of Subtlety (31223, more damage while stealthed and for 6 s after); Deadly Poison (2823); the specialisation is chosen here |
| 12 | Sap (6770) |
| 14 | Slice and Dice (5171, 25, finisher: +40 percent melee attack speed; 12 s per point, 36 s at 5) |
| 16 | Recuperate (73651, 30, finisher heal) |
| 18 | Kick (1766, 15 s) |
| 20 | Find Weakness (91023, Ambush, Garrote and Cheap Shot make your attacks bypass part of the target's armor for 10 s); Crippling Poison (3408) |
| 22 | Gouge (1776, 45) |
| 26 | Sprint (2983, 60 s) |
| 28 | Distract (1725) |
| 30 | Premeditation (14183, 20 s, +2 combo points, the guides use it from stealth); Cheap Shot (1833, 40, 2 points); Wound Poison (8679) |
| 34 | Vanish (1856, 120 s) |
| 36 | Expose Armor (8647, 25) |
| 38 | Blind (2094) |
| 40 | Backstab (53, 35, 1 point, from behind, main-hand dagger); Kidney Shot (408) |
| 44 | Feint (1966, 20) |
| 46 | Rupture (1943, 25, finisher bleed, 8 s plus 4 s per point) |
| 48 | Garrote (703, 45, from stealth); Safe Fall |
| 50 | Honor Among Thieves (51701, a party member's critical hit gives a point, 2 s limit, in combat) |
| 58 | Cloak of Shadows (31224) |
| 60 | Sanguinary Vein (79147, Rupture does more and targets with Rupture, Garrote or Crimson Tempest take more damage); Shroud of Concealment; tier 4 talent |
| 66 | Fan of Knives (51723, 35, 1 point, area) |
| 68 | Preparation (14185, 300 s: resets Sprint, Vanish, Evasion and Dismantle only, spell text) |
| 70 | Energetic Recovery (79152, energy back while Slice and Dice runs) |
| 74 | Shiv (5938) |
| 75 | tier 5 talent; Tricks of the Trade (57934, 78) |
| 80 | Shadow Dance (51713, 60 s: opening attacks without stealth, cheaper Ambush); Mastery: Executioner (76808) |
| 81 | Redirect (73981) |
| 83 | Crimson Tempest (121411, 35, area finisher) |
| 85 | Smoke Bomb (76577) |
| 87 | Shadow Blades (121471, 180 s) |
| 90 | tier 6 talent |

Facts the priority lists rest on (spell texts and data tables):

- Combo points stay on the target; finishers cost 25 to 35 energy. Slice and Dice at 5 points lasts 36 s, longer than
  most dungeon trash fights; a Slice and Dice at 3 points (24 s) is enough there (derived).
- A stealth opener is worth more here than for the other specialisations: Master of Subtlety (10), Find Weakness (20),
  Premeditation (30) and Shadow Focus (talent) all pay out of stealth. Ambush gives 2 points, Premeditation 2.
- Backstab needs a dagger in the main hand and the target's back; Hemorrhage needs neither, costs 30 and is the builder
  before level 40 (derived: the stronger Backstab arrives at 40).
- Sinister Strike is "replaced" by Hemorrhage in the client; the server keeps no override column in its spell code
  (checked: no use of the overridden-spell column in `SkyFire_548/src/server/game`), so whether the bot can still cast
  Sinister Strike is to check live.
- Preparation does not reset Shadowstep.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked
"derived" comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to
`mod-aware-bots-548/src/` at commit `605b17f`.

How the bots rank what they do (read, `strategy/Strategy.h:37-51`): ACTION_DEFAULT 5, ACTION_NORMAL 10, ACTION_HIGH 20,
ACTION_MOVE 30, ACTION_INTERRUPT 40, ACTION_EMERGENCY 90. A trigger edge adds its action with that number; the engine
takes the highest action that is possible and useful. **A Subtlety rogue runs the Assassination strategy**
(`Factory/AiFactory.cpp:481-491`: every spec but Combat gets `assassin`; `RogueAiObjectContext.cpp:58`) together with
`dps assist`, `aoe` and, from the shared melee rule, `behind` (`:567`); every bot outside battlegrounds also gets `boost`
(`:316`). That strategy was written for Mutilate and Envenom: the builder edge is Mutilate (not a Subtlety spell), and the
chain behind it is Mutilate, Backstab, Sinister Strike (`AssassinationRogueStrategy.cpp:20-33`).

Every builder edge is gated by "high energy available", which is 60 energy or more (`GenericTriggers.h:138-142`), above
the cost of Backstab (35) and Hemorrhage (30): up to a second of idle per builder (derived; the regeneration rate is to
check).

### Levels 15 to 29

Known: Hemorrhage (10), Eviscerate, Stealth, Ambush (6), Evasion, Slice and Dice (14), Recuperate (16), Kick (18), Find
Weakness (20), Gouge (22), Sprint (26), Sinister Strike (replaced).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | out of combat, the poison is missing | Deadly Poison (2823, 10), plus a non-lethal poison from 20 (derived: Crippling Poison) | yes: "often" edges, Deadly at 20 and Crippling at 19 (`GenericRogueNonCombatStrategy.cpp:70`, `:72`) |
| 2 | core | a target within about 30 yd and not in combat | Stealth (1784, 5) before the pull (both guides open from stealth) | yes (`GenericRogueNonCombatStrategy.cpp:49`, `RogueTriggers.cpp:48-88`); BT-057 live check pending |
| 3 | core | stealthed, behind the target | Ambush (8676, 6): 2 points and Find Weakness (20); wowtbc: "go in and Ambush as much as possible" | yes: the "in stealth" edge swaps in the stealthed strategy, then `behind target` -> Ambush at 20 (`AssassinationRogueStrategy.cpp:65-66`, `RogueActions.cpp:39-53`, `DpsRogueStrategy.cpp:240-243`). BT-059 live check pending |
| 4 | core | Slice and Dice missing, 3 points or more | Slice and Dice (5171, 14) (guides: at 5 points; 3 points derived, see above) | yes at 25 (`AssassinationRogueStrategy.cpp:78-79`, trigger `RogueTriggers.cpp:210-214`, BT-058) |
| 5 | core | 5 points | Eviscerate (2098, 3) (Rupture comes at 46) | partly: the finisher edge is at **4** points and names Envenom, which Subtlety does not know; the node falls back to Eviscerate (`:84-85`, node `:41-47`) |
| 6 | core | energy for a builder | Hemorrhage (16511, 10): the only builder of the specialisation before Backstab (derived) | **wrong:** the builder edge is Mutilate with the chain Mutilate -> Backstab -> Sinister Strike (`:72-73`, node `:20-33`); Hemorrhage is cast only when its own bleed is missing (`:129-130`, trigger `RogueTriggers.cpp:142-145`). Whether Sinister Strike is castable is to check live |
| 7 | good | Hemorrhage bleed missing on the target | keep Hemorrhage up (wowtbc, Method) | yes at 14 (`:129-130`) |
| 8 | good | target dies within 3 s with 3 points | finisher before the fight ends (derived) | yes at 24, but as Envenom -> Eviscerate (`:87-88`) |
| 9 | good | an enemy casts, from 18 | Kick (1766, 18) | yes, the current target at 42 and an enemy healer at 41 (`:103-108`) |
| 10 | good | health at or under 45 percent | Evasion (5277, 8), then Feint (44) | yes (`:96-98`), at 29 and 28 |
| 11 | good | health at or under 30 percent, 5 points (derived) | Recuperate (73651, 16) | yes (`:135-136`, trigger `RogueTriggers.cpp:179-187`) |
| 12 | expert | a second enemy attacks, peel | Gouge (1776, 22) | **missing:** the action exists (`RogueAiObjectContext.cpp:144`) but no strategy has an edge |
| 13 | expert | a mob alone before the pull | Sap (6770, 12) | **missing for the party:** `RogueCcStrategy` has the edge (`DpsRogueStrategy.cpp:279-282`) but `AiFactory.cpp:481-491` never adds `cc` to a rogue |

Before a spell is learned (derived): below 10 there is no specialisation, the kit is Sinister Strike, Eviscerate and
Stealth; Hemorrhage and the passives come at 10, Find Weakness at 20. The bots tolerate all of these because an unknown
spell is simply not cast.

### Levels 30 to 44

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 14 | core | stealthed, before the first strike | Premeditation (14183, 30): +2 points, then Ambush (wowtbc opener, Method opener) | **missing:** no action, no trigger (`RogueAiObjectContext.cpp:142-190`); the stealthed strategy has no line for it |
| 15 | good | stealthed, not behind | Cheap Shot (1833, 30): stun and 2 points | yes (`DpsRogueStrategy.cpp:242-243`) |
| 16 | core | from 40, behind the target, main-hand dagger | Backstab (53, 40): the main builder (both guides' last line), Hemorrhage kept up beside it | partly: the second link of the Mutilate chain (`AssassinationRogueStrategy.cpp:27-33`); the builder is tried only after Mutilate fails every tick |
| 17 | good | the group has no Weakened Armor source | Expose Armor (8647, 36), 1 point (Method: only if assigned) | yes, sees any caster's Weakened Armor (`RogueTriggers.cpp:216-224`, edge `:90-91`) |
| 18 | good | a second or third enemy attacks the rogue | Vanish (1856, 34): a threat dump, and the stealth that follows opens with Ambush (wowtbc: Vanish then Ambush) | partly: fires as soon as two enemies attack the rogue (`:93-94`), a 2 minute cooldown spent often; the Ambush after it comes from the stealthed strategy |
| 19 | good | health at or under 45 percent | Feint (1966, 44) after Evasion | yes (`:96-98`) |
| 20 | good | from 40, an enemy under attack must stay down | Kidney Shot (408, 40), Blind (2094, 38) | Kidney Shot only as Kick's fallback; Blind has no edge |

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 21 | core | 5 points, Rupture missing | Rupture (1943, 46) before Eviscerate (Method #3: "Rupture at 5 points") | **missing:** the Assassination strategy has no Rupture edge at all, only a node (`AssassinationRogueStrategy.cpp:34-40`); the Combat strategy has it (`DpsRogueStrategy.cpp:101-103`) |
| 22 | core | 5 points, Rupture and Slice and Dice up | Eviscerate (2098, 3) (Method: only if neither is about to expire) | partly: Envenom -> Eviscerate at 4 points (`:84-85`) |
| 23 | good | stealthed | Garrote (703, 48) as the alternative opener | yes: `high energy available` -> Garrote 27, Ambush 26 (`:68-70`), needs stealth |
| 24 | good | from 50 | Honor Among Thieves (51701) | passive; works in a group |
| 25 | good | health at or under 25 percent | Cloak of Shadows (31224, 58) | yes (`:100-101`) |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 26 | core | Rupture up on the target | Sanguinary Vein (79147, 60): the damage bonus needs Rupture, Garrote or Crimson Tempest on the target | no effect today, because Rupture is never cast (line 21) |
| 27 | good | tier 4 talent | Shadowstep (36554) as a gap closer, 8 to 25 yd | yes (`RogueTriggers.cpp:147-158`) |
| 28 | good | from 66, three or more enemies within 8 yd | Fan of Knives (51723, 66) replaces Backstab and Hemorrhage (both guides, 4 to 6 targets) | yes at 15 (`AssassinationRogueStrategy.cpp:110-111`), the Hemorrhage edge still fires at 14 below it |
| 29 | good | from 68 | Preparation (14185, 68) after Vanish | wrong: the trigger also fires for Sprint and Shadowstep (`RogueTriggers.cpp:189-203`); see the cooldown section |
| 30 | good | Slice and Dice up | Energetic Recovery (79152, 70) | passive; works while Slice and Dice is kept up |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 78 | Tricks of the Trade on the tank before the pull and after Vanish (both guides) | on the main tank when the rogue's threat passes half of the tank's (`AssassinationRogueStrategy.cpp:142-144`); no pre-pull cast |
| 80 | Shadow Dance at 75 or more energy when Find Weakness is not on the target; Vanish at 45 or more energy under the same rule; Premeditation inside both; Ambush while stealthed (wowtbc) | **missing:** no Shadow Dance action or edge; Vanish is a threat dump |
| 81 | Redirect when a target switch keeps points | missing (no action) |
| 83 | Crimson Tempest at 5 points on 2 or more targets (Method: all targets; wowtbc: from 7 targets) | missing (no action) |
| 87 | Shadow Blades when Find Weakness is not active | yes at 22 in combat when the buff is missing (`:137-138`), but without the Find Weakness condition |
| 90 | single target (wowtbc order): Shadow Blades, Premeditation, Ambush in stealth, Shadow Dance, Vanish, Preparation, Slice and Dice at 5, Rupture at 5, Eviscerate at 5, Hemorrhage kept up, Backstab; area: Fan of Knives from 4 targets, Crimson Tempest from 7 | Backstab, Hemorrhage, Slice and Dice, Shadow Blades, Tricks, Fan of Knives are there; Premeditation, Shadow Dance, Rupture, Crimson Tempest and the Find Weakness windows are not |

## Cooldowns and defensives

The guides say "on cooldown"; the timing for a dungeon bot is derived (trash fights last 20 to 40 s).

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Premeditation | 30 (20 s) | before the fight, after Vanish, in Shadow Dance | in stealth before every Ambush | missing |
| Shadow Dance | 80 (60 s) | on cooldown when Find Weakness is not on the target | same | missing |
| Vanish | 34 (120 s) | on cooldown when Find Weakness is not on the target, then Ambush | threat dump at two attackers; at 80 and up inside the burst | at "medium threat" only (`AssassinationRogueStrategy.cpp:93-94`) |
| Preparation | 68 (300 s) | right after Vanish | only when Vanish and Evasion are both on cooldown | fires when Vanish, Sprint **or Shadowstep** is on cooldown (`RogueTriggers.cpp:189-203`): Shadowstep is not reset by the spell, and a Sprint on every approach (BT-059) makes it fire on a 60 s cooldown |
| Shadow Blades | 87 (180 s) | on cooldown, when Find Weakness is not active | same | yes, without the Find Weakness condition (`:137-138`) |
| Evasion | 8 (120 s) | defensive (buffed) | health at or under 45 percent | yes (`:96-98`) |
| Feint | 44 | glyph option, Elusiveness | after Evasion, same line | yes |
| Cloak of Shadows | 58 (60 s) | second in wowtbc's defensive list | health at or under 25 percent | yes (`:100-101`) |
| Smoke Bomb | 85 (180 s) | first in wowtbc's defensive list | group takes heavy damage | battlegrounds and arenas only |
| Combat Readiness | 30 (talent, 120 s) | Method defensive talent, wowtbc fourth | health at or under 45 percent | missing (no action) |
| Kick | 18 (15 s) | interrupt | any casting enemy in melee range | yes, the target and healers; any other caster in PvP only (`RogueActions.cpp:75-79`) |
| Recuperate | 16 | not given | health at or under 30 percent, 5 points | yes |

## Healer triage and mana (healers only)

Not applicable. Energy is the resource; the combo points are the second one. Costs: Ambush 60, Backstab 35, Hemorrhage 30,
Eviscerate 35, Rupture 25, Slice and Dice 25, Garrote 45, Cheap Shot 40, Fan of Knives 35, Expose Armor 25, Recuperate 30,
Feint 20.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Shadow Dance (80), Redirect (81), Crimson Tempest (83), Smoke Bomb (85), Shadow Blades (87)
  and Anticipation (90) are not available in the 15 to 60 band; the lists above start without them.
- Method keeps Leeching Poison up while its talent text offers Cheat Death or Elusiveness for the same tier (45): a rogue
  can have only one of them.
- Backstab is learned at 40 (spec spell), not at 4 as an old row suggests; before that Hemorrhage is the builder.
- Glyph of Hemorrhage is a minor glyph here and Hemorrhaging Veins a major one; the premade comment copies the item-name
  typo ("Hemorraghing").
- Preparation resets Sprint, Vanish, Evasion and Dismantle (spell text); wowtbc's "Preparation to reset Vanish" is right
  but the bots' trigger counts Shadowstep.
- Method's Hit and Expertise 7.5 percent and 26.5 percent off-hand rule are level 90 numbers; wowtbc's Shadow Dance and Vanish
  energy thresholds (75, 45) need the level 80 and 34 spells and a clock the bots do not keep.
- Method's opener starts with Tricks of the Trade on the tank; it is learned at 78.

## Gap list (ordered by value for levels 15 to 60)

Config (can go at once):

1. Tier 15: Shadow Focus instead of Nightstalker (Method: mandatory). `PremadeSpecLink.4.2`
   (`conf/playerbots.conf.dist:2748`) to `220102`.
2. Glyphs: `PremadeSpecGlyph.4.2` (`:2749`) to `45766,43380,42963,43378,43379,42970`. Today a level 25 to 59 Subtlety bot
   carries Hemorrhaging Veins (works from 60), Redirect and Smoke Bomb, none of which helps in a dungeon.
3. Tier 30: keep Combat Readiness only together with rotation item 7; otherwise it is a dead talent.

Rotation (class fixes, wait for the owner's in-game test):

1. A Subtlety strategy of its own (core): `AiFactory.cpp:481-491` gives every non-Combat rogue the `assassin` strategy,
   so Subtlety fights with Mutilate, Envenom and Dispatch edges that do nothing for it. The minimum: a `subtlety` strategy
   (or a Subtlety branch of `AssassinationRogueStrategy`) whose builder is Hemorrhage from 10 and Backstab from 40, whose
   finisher edge is 5 points, with Hemorrhage kept up beside Backstab.
2. Check live whether `sinister strike` is still castable for a Subtlety bot (the chain's last link,
   `AssassinationRogueStrategy.cpp:27-33`). If it is not, levels 10 to 39 have no builder at all except the Hemorrhage
   refresh edge, which is the most damaging gap of the sheet.
3. Rupture for Subtlety (and Assassination): "combo points 5 available" -> Rupture if the target lacks it, then
   Eviscerate (`DpsRogueStrategy.cpp:101-103` is the model); from level 46; Sanguinary Vein at 60 depends on it.
4. Premeditation (14183, level 30): a self cast action (`CastBuffSpellAction`, off the global cooldown) and an edge in the
   stealthed strategy before Ambush (`DpsRogueStrategy.cpp::StealthedRogueStrategy`, `:224-228`). Core tier from 30.
5. Preparation trigger (`RogueTriggers.cpp:189-203`, `PreparationTrigger::IsActive`): drop Sprint and Shadowstep from the
   names, require Vanish on cooldown (and Evasion for a bot below 45 percent).
6. Stat weights (`Mgr/Item/StatsWeightCalculator.cpp:422-433`): split Subtlety from Assassination so that Haste and
   Critical stand above Mastery (Method), and keep Agility above Hit and Expertise before level 80. Gear scoring.
7. `combat readiness` action (self buff like `CastEvasionAction`, `RogueActions.h:13-17`) on "low health", beside Evasion
   (`AssassinationRogueStrategy.cpp:96-98`), if the talent stays picked.
8. A Leeching Poison action beside Deadly Poison (`GenericRogueNonCombatStrategy.cpp:72`) for the tier 45 pick; then
   config tier 45 can move to Leeching Poison.
9. Energy gate: builders are held to 60 energy by "high energy available" (`GenericTriggers.h:138-142`) although
   Hemorrhage and Backstab cost 30 and 35; a per-builder gate removes the idle time. Minor; the regeneration rate is to check.
10. Level 80 and up (outside the band): Shadow Dance and Vanish windows keyed to Find Weakness (wowtbc thresholds 75 and 45
    energy), Shadow Blades only when Find Weakness is down, Crimson Tempest at 5 points, Redirect, pre-pull Tricks.
11. Vanish at two attackers (`:93-94`): the 120 s cooldown is spent on every double pull. Owner decision whether a damage
    bot should dump threat that way.
12. Skill tiers: the rogue strategies have no tier gating (`BotSkill` is used only in the tank code). Lines marked good
    and expert above (Kick on any caster, Expose Armor, Vanish timing, Gouge, Sap) are the ones a casual bot should drop.
13. To check live: BT-057 (stealth before the pull) and BT-059 (the stealthed opener swap) are deployed with their live
    check pending; lines 2 and 3 of the 15 to 29 list depend on them.

## PvP class package: bots today (BT-691 to BT-693, 2026-11-19)

Status of the digest's section 12 rules after the Subtlety package (see `docs/pvp/class-packages.md`; all wait for the owner's in-game test):

| Digest rule | Bots today |
|---|---|
| 1 stealth start, Garrote opener | as before (opening actions) |
| 2 Slice and Dice, Sanguinary Vein | as before |
| 3 Shadow Dance, Marked for Death in a setup | held for the burst window (BT-692); the Ambush chain is the existing edge |
| 4 Sap a second enemy in the opener | done for an unaware enemy within 10 yd (BT-691); Vanish into Sap after a Blind is not built |
| 5 Blind the healer, then a stun | Blind, Kidney Shot and Gouge are rows of the control kit (BT-629); the trinket bait is the shared window |
| 6 Kidney Shot on the kill target at 5 points | the kit casts it on the healer; not on the kill target |
| 7 Evasion, Vanish, Smoke Bomb | the shared defensive layer and the older low health edges |
| 8 re-stealth behind a pillar | not built |
| 9 Hemorrhage or Backstab by position | partly (builder edges exist) |
| 10 Tricks of the Trade on the partner | done for an expert at the team go (BT-693) |
| 11 Cheat Death or Elusiveness by comp | config only (the set holds Elusiveness) |
| 12 do not open first against another stealth class | not built |

Set: unchanged. Every pick is a passive, has a talent policy edge, or has its edge (Marked for Death, held for the window).
