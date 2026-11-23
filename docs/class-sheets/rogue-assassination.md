# Rogue: Assassination (damage)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). The level 80 burst
  windows are left for later. The "bots today" columns below describe the code before the 2026-11-11 implementation.
- **Sources:**
  - Method, "Assassination Rogue Class Guide - MoP Classic" (Phase 1, level 90 raid and dungeon, 23 July 2025), read
    2026-11-11 through a scripted fetch (the talent build is an image: only the talent hints in the text are usable).
  - wowtbc.gg, "Assassination Rogue" in the MoP class guides (MoP Classic, no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, Talent, GlyphProperties,
    SpellCooldowns, SpellPower, SpellEffect, SpellEquippedItems; `data/db2/Item-sparse.db2` for glyph item ids) through
    `spec_data.py` and short read-only scripts.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch, checked 2026-11-10, not retried); Wowhead needs a
    browser.
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the
  tank's marks), 61 to 90 briefly from the guides.

Which source was richer: **glyphs (raid and dungeon, minor set), stat order, poison line, talent swaps** Method (wowtbc
lists every talent option without a pick and has no stat order); **opener, single target conditions (Rupture at one
point or more, Envenom at 4 to 5, Slice and Dice refreshed by Envenom, Marked for Death at 0 points), AoE tiers, the
defensive order** wowtbc; **levels 15 to 60, what to do before a spell is learned, dagger rules, the Preparation
reset list** neither guide says anything (both are level 90 raid guides), so those lines come from the 5.4.8 data and are
marked "derived". The guides disagree on two points, both resolved below: Envenom at 5 points (Method) against 4 to 5
(wowtbc), and Rupture as a fixed 5 point cast (Method) against any point count (wowtbc).

## Talents

Tiers and columns are from Talent.dbc. "Bots today" is `AiPlayerbot.PremadeSpecLink.4.0 = 022101`
(`conf/playerbots.conf.dist:2738`; one digit per tier, column counted from 0): Nightstalker, Combat Readiness,
Elusiveness, Shadowstep, Prey on the Weak, Marked for Death.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (022101) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Method: "no real alternative" but the pick is in the image; Combat and Subtlety guides name Shadow Focus "always" and "mandatory" | Nightstalker, Subterfuge (wowtbc options) | 108209 / 14062 / 108208 | Nightstalker. **Differs** (derived pick: Shadow Focus, abilities cost 75 percent less energy while stealthed, so every stealth opener is nearly free; ask the owner to confirm from the image) |
| 2 (30) | Not given for Assassination; Subtlety guide names Combat Readiness; wowtbc defensive list has it fourth | Deadly Throw, Nerve Strike | 74001 / 26679 / 108210 | Combat Readiness, same. **Dead today:** no `combat readiness` action exists (`RogueAiObjectContext.cpp:142-190`), so the talent is never pressed |
| 3 (45) | Method: Elusiveness (raid default) or Cheat Death (dungeon, niche raid); Method's poison line also keeps Leeching Poison up, which is this tier's third talent | Leeching Poison (wowtbc option), see the note | 79008 / 31230 / 108211 | Elusiveness. Derived pick for bots: Cheat Death (passive, no button, 85 percent less damage for a short time when a hit would kill, 90 s) |
| 4 (60) | Shadowstep (both); Burst of Speed when a fight needs movement without a target | Cloak and Dagger (wowtbc option) | 36554 / 108212 / 138106 | Shadowstep, same (`RogueTriggers.cpp:147-158`, `AssassinationRogueStrategy.cpp:121-122`) |
| 5 (75) | Not given for Assassination; Combat and Subtlety guides: Prey on the Weak or Dirty Tricks | Paralytic Poison | 131511 / 108216 / 108215 | Prey on the Weak. No effect either way: no bot code disables a target with Gouge, Kidney Shot, Sap or Blind (see the gap list) |
| 6 (90) | Marked for Death (wowtbc single target: "Marked for Death at 0 combo points", also in the opener); Method gives no pick in text | Anticipation, Shuriken Toss | 137619 / 114015 / 114014 | Marked for Death, same (`RogueTriggers.cpp:160-167`: cast with fewer than 2 points, `AssassinationRogueStrategy.cpp:119-120`) |

Notes on the picks (data, not guide):

- Shadow Focus: -75 percent energy cost while stealthed (effect of spell 112942). The bots stealth before a pull
  (`GenericRogueNonCombatStrategy.cpp:49`, BT-057), so the opener and any Vanish window are cheap.
- Cheat Death needs no code. Elusiveness only pays if the bot casts Feint, which it does at 45 percent health after
  Evasion (`AssassinationRogueStrategy.cpp:96-98`), a short window.
- Leeching Poison (spell 108211, a talent, not a spell of the class list): non-lethal poison whose weapon strikes heal
  the rogue. Method's rotation keeps it up. It needs a one-line action beside Deadly Poison; until then Cheat Death is the
  tier 45 pick.
- Marked for Death (60 s, level 90) gives 5 combo points; it is the free builder of an Assassination opener.

Proposed link, guide aligned: `220101` (Shadow Focus / Combat Readiness / Cheat Death / Shadowstep / Prey on the Weak /
Marked for Death). For a character below 75 only the first four digits are in effect.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and
75 (config comment `conf/playerbots.conf.dist:2631-2636`; the unlock levels are core-side). The config order is major,
minor, major, minor, minor, major; a glyph goes to the first free slot of its type.

| Type | Glyph | Item id | Why (one clause) | Bots today (`45761,43378,42972,43380,43379,42968`) |
|------|-------|--------:|------------------|------------------------|
| Major | Vendetta | 45761 | Method's key choice: less bonus, longer duration; Vendetta is learned at 80 | yes, in the first slot (level 25): **does nothing until 80** |
| Major | Feint | 42963 | Method's best pair with Redirect; longer Feint, and the bots do cast Feint at low health | no |
| Major | Redirect | 42972 | Method's best pair; a shorter cooldown of a spell the bots never cast (Redirect learned at 81, no action) | yes, **useless for the bots** |
| Major, situational | Smoke Bomb | 42968 | raid utility (Method); the bots cast it only in battlegrounds and arenas (`RogueActions.cpp:70-73`) | yes, **useless in a dungeon** |
| Major, situational | Cloak of Shadows | 45769 | 40 percent less physical damage while Cloak is up; the bots press Cloak at 25 percent health (`AssassinationRogueStrategy.cpp:100-101`) | no |
| Major, derived | Deadly Momentum | 45766 | a kill that gives experience or honor refreshes Slice and Dice and Recuperate to full duration; the bots upkeep Slice and Dice, so chain pulls keep it for free | no (in neither guide) |
| Major, situational | Sharp Knives, Hemorrhaging Veins | 42958, 42970 | AoE (Method), Fan of Knives applies Weakened Armor; Hemorrhaging Veins is a Subtlety glyph | no |
| Minor | Safe Fall, Blurred Speed | 43378, 43379 | the only minors Method names | yes (both) |
| Minor | Poisons | 43380 | wowtbc; faster poison application, harmless | yes |

Disagreement: Method's minor set is Safe Fall and Blurred Speed, wowtbc's Safe Fall and Poisons (a minor glyph in the
data). Both are harmless; the premade keeps all three. Method separates nothing for dungeons except adding Sharp Knives.

Slot order is derived (a glyph in a slot must be useful at the level the slot unlocks): level 25 to 49 has one major
slot, and Vendetta, Redirect and Smoke Bomb all do nothing there. Proposed `PremadeSpecGlyph.4.0`:
`45766,43380,42963,43378,43379,45761` = Deadly Momentum, Poisons, Feint, Safe Fall, Blurred Speed, Vendetta. Redirect and
Smoke Bomb drop out until the bots cast them.

Item-name typos in the data: item 42970 is spelled "Glyph of Hemorraghing Veins" and 42958 "Glyph of Sharp Knives" (the
property is "Sharpened Knives"); the premade comment copies the first typo.

## Stat order

Method: Hit and Expertise to about 7.5 percent each, Agility, Mastery, Haste, Critical Strike; optionally 26.5 percent
Hit so the off-hand never misses. wowtbc has no stat order. The caps are level 90 numbers and cannot be reached with the
gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **A dagger in the main hand is mandatory**, then everything else. Mutilate, Dispatch and Backstab are cast only with a
  dagger in the main hand (SpellEquippedItems: item subclass mask 32768 = dagger; Dispatch's text says so), and Assassin's
  Resolve (level 10: maximum energy and damage up) needs daggers. Fast off-hand dagger, slow main-hand dagger is the
  usual pairing; neither guide says.
- Then leather, Agility (it feeds attack power and critical strike), Mastery (Potent Poisons, level 80: more poison
  damage), Haste, Critical Strike last. Hit and Expertise matter only near level 90.
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:422-433`, shared with Subtlety): Hit 2.1, Expertise 2.1, Mastery 2.0,
  Haste 1.8, Agility 1.5, Strength 1.1, Critical 1.1. The secondary order after Agility agrees with Method; **Agility is
  below Hit and Expertise**, so a low-level item with a hit or expertise stat beats an agility item (derived: wrong
  before the caps are in reach). Daggers get x1.5 for both hands from level 10 (`:876-881`), so a clearly better sword in
  the main hand can still win and leave Mutilate unusable (inference, to check on a bot's gear). Armor penetration (1.2)
  belongs to a stat the Cataclysm changes removed (inference, to check), harmless. The weapon speed preference
  (`:1181-1199`, config `PreferredSpecWeapons`, default off) asks for a slow main-hand dagger of at least 1.7 s and a fast
  off-hand dagger, the usual pairing.

## What the specialisation knows, by level (5.4.8 data)

Ids are the spells the player actually casts; the data holds old rows of the same name. Energy costs are the SpellPower
values. The global cooldown of every energy ability is 1 s (SpellCooldowns).

| Level | Spell (id) |
|------:|------------|
| 1 | Sinister Strike (1752, 50 energy, 1 point), Throw (2764) |
| 3 | Eviscerate (2098, 35, finisher) |
| 5 | Stealth (1784, 6 s cooldown) |
| 6 | Ambush (8676, 60 energy, 2 points, from stealth, more damage with a dagger) |
| 8 | Evasion (5277, 120 s) |
| 10 | Mutilate (1329, 55, 2 points, both weapons, main-hand dagger); Improved Poisons (14117); Assassin's Resolve (84601, daggers: more maximum energy and damage); Deadly Poison (2823); the specialisation is chosen here |
| 12 | Sap (6770, 35, out of combat) |
| 14 | Slice and Dice (5171, 25, finisher: +40 percent melee attack speed; 12 s per point, 36 s at 5) |
| 15 | Pick Pocket (921) |
| 16 | Recuperate (73651, 30, finisher heal over 6 s per point) |
| 18 | Kick (1766, 15 s, no energy) |
| 20 | Envenom (32645, 35, replaces Eviscerate: Nature damage and a higher poison chance); Crippling Poison (3408) |
| 22 | Gouge (1776, 45, 10 s) |
| 24 | Pick Lock (1804) |
| 26 | Sprint (2983, 60 s) |
| 28 | Distract (1725, 30 energy, 30 s) |
| 30 | Seal Fate (14190, critical hits of single-target builders give an extra point); Cheap Shot (1833, 40, from stealth, 2 points); Wound Poison (8679) |
| 34 | Vanish (1856, 120 s) |
| 36 | Expose Armor (8647, 25, 1 point, Weakened Armor) |
| 38 | Blind (2094, 15 energy, 120 s) |
| 40 | Dispatch (111240, 30, 1 point, replaces Sinister Strike, main-hand dagger, target under 35 percent); Kidney Shot (408, 25, 20 s, finisher stun) |
| 42 | Detect Traps (2836) |
| 44 | Feint (1966, 20 energy, less area damage) |
| 46 | Rupture (1943, 25, finisher bleed, 8 s plus 4 s per point) |
| 48 | Garrote (703, 45, from stealth, silence, 1 point); Safe Fall (1860) |
| 50 | Venomous Wounds (79134, Rupture and Garrote ticks on a poisoned enemy deal extra damage and refund energy) |
| 56 | Disarm Trap (1842) |
| 58 | Cloak of Shadows (31224, 60 s) |
| 60 | Cut to the Chase (51667, Envenom refreshes Slice and Dice to its 5 point duration); Shroud of Concealment (115855); tier 4 talent |
| 66 | Fan of Knives (51723, 35, 1 point, area) |
| 68 | Preparation (14185, 300 s: resets Sprint, Vanish, Evasion and Dismantle only, spell text) |
| 70 | Blindside (121152, passive: Mutilate sometimes makes the next Dispatch free at any health) |
| 74 | Shiv (5938, 10 s, removes an enrage) |
| 75 | tier 5 talent; Tricks of the Trade (57934, 78) |
| 80 | Vendetta (79140, 120 s); Mastery: Potent Poisons (76803) |
| 81 | Redirect (73981, 60 s) |
| 83 | Crimson Tempest (121411, 35, area finisher) |
| 85 | Smoke Bomb (76577, 180 s) |
| 87 | Shadow Blades (121471, 180 s) |
| 90 | tier 6 talent |

Facts the priority lists rest on (spell texts and data tables):

- Combo points stay on the target; the finishers cost 25 to 35 energy. Slice and Dice at 5 points lasts 36 s, longer than
  most dungeon trash fights, so for a dungeon bot a Slice and Dice at 3 points (24 s) is enough (derived).
- Cut to the Chase (level 60) makes every Envenom a Slice and Dice refresh at the 5 point duration: from 60 the upkeep is
  one opening cast and Envenom after that (wowtbc: "maintain Slice and Dice, refreshed by Envenom").
- Mutilate, Dispatch and Backstab need a main-hand dagger. Mutilate gives 2 points, Sinister Strike 1.
- Preparation does not reset Shadowstep (spell text lists Sprint, Vanish, Evasion, Dismantle).
- Poisons are self-cast spells (Deadly Poison from level 10, Wound Poison 30, Crippling Poison 20, Mind-numbing Poison 28);
  Instant Poison has no player-learnable row in this build.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked
"derived" comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to
`mod-aware-bots-548/src/` at commit `605b17f`.

How the bots rank what they do (read, `strategy/Strategy.h:37-51`): ACTION_DEFAULT 5, ACTION_NORMAL 10, ACTION_HIGH 20,
ACTION_MOVE 30, ACTION_INTERRUPT 40, ACTION_EMERGENCY 90. A trigger edge adds its action with that number; the engine
takes the highest action that is possible and useful. A level 10 and up rogue of this specialisation gets the
`assassin` strategy (`Factory/AiFactory.cpp:481-491`) together with `dps assist`, `aoe` and, from the shared melee rule,
`behind` (`:567`); every bot outside battlegrounds also gets `boost` (`:316`). Subtlety runs the same strategy, so any
change here also moves Subtlety.

Every builder edge is gated by "high energy available", which is 60 energy or more (`GenericTriggers.h:138-142`), above
the cost of Mutilate (55), Sinister Strike (50) and Dispatch (30): up to about half a second of idle per builder
(derived; the regeneration rate is to check).

### Levels 15 to 29

Known: Sinister Strike, Eviscerate then Envenom (20), Stealth, Ambush (6), Evasion, Mutilate (10), Sap (12), Slice and
Dice (14), Recuperate (16), Kick (18), Crippling Poison (20), Gouge (22), Sprint (26), Distract (28).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | out of combat, the poison is missing | Deadly Poison (2823, 10), plus a non-lethal poison from 20 (derived: Crippling Poison, Leeching Poison once picked) | yes: "often" edges, Deadly at 20 and Crippling at 19 (`GenericRogueNonCombatStrategy.cpp:70`, `:72`) |
| 2 | core | a target within about 30 yd and not in combat | Stealth (1784, 5) before the pull (both guides open from stealth) | yes (`GenericRogueNonCombatStrategy.cpp:49`, `RogueTriggers.cpp:48-88`); BT-057 live check pending |
| 3 | good | stealthed, behind the target | Ambush (8676, 6): 2 points. wowtbc opens with Garrote (48) and a Shadowstep first; before 48 Ambush is the opener (derived) | yes: the "in stealth" edge swaps in the stealthed strategy, then `behind target` -> Ambush at 20, `not behind` -> Cheap Shot (`AssassinationRogueStrategy.cpp:65-66`, `RogueActions.cpp:39-53`, `DpsRogueStrategy.cpp:240-243`). BT-059 live check pending |
| 4 | core | Slice and Dice missing, 3 points or more | Slice and Dice (5171, 14) (Method: at 5 points; 3 points derived, see above) | yes at 25 (`AssassinationRogueStrategy.cpp:78-79`, trigger `RogueTriggers.cpp:210-214`, BT-058) |
| 5 | core | 4 or 5 points | Envenom (32645, 20) (Method: 5; wowtbc: 4 to 5, always 5 under 35 percent) | yes at 4 points, priority 25, **the same number as Slice and Dice** (`:84-85`), so the order between them is not set by priority; Eviscerate stands in below 20 (node `:41-47`) |
| 6 | core | energy for a builder | Mutilate (1329, 10), needs a main-hand dagger | yes at 13 (`:72-73`), gated at 60 energy; node falls back to Backstab then Sinister Strike (`:20-33`) |
| 7 | good | target dies within 3 s with 3 points | finisher before the fight ends (derived) | yes: "target with combo points almost dead" -> Envenom at 24 (`:87-88`) |
| 8 | good | an enemy casts, from 18 | Kick (1766, 18) | yes, the current target at 42 and an enemy healer at 41 (`:103-108`) |
| 9 | good | health at or under 45 percent | Evasion (5277, 8), then Feint (44) | yes (`:96-98`), at 29 and 28 |
| 10 | good | health at or under 30 percent, 5 points (derived) | Recuperate (73651, 16) | yes (`:135-136`, trigger `RogueTriggers.cpp:179-187`) |
| 11 | expert | a second enemy attacks, peel | Gouge (1776, 22) | **missing:** the action exists (`RogueAiObjectContext.cpp:144`) but no strategy has an edge for it |
| 12 | expert | a mob alone before the pull | Sap (6770, 12) | **missing for the party:** `RogueCcStrategy` has the edge (`DpsRogueStrategy.cpp:279-282`) but `AiFactory.cpp:481-491` never adds `cc` to a rogue |

Before a spell is learned (derived): below 10 there is no specialisation, the kit is Sinister Strike, Eviscerate and
Stealth; Mutilate and Envenom wait for 10 and 20. The bots tolerate all of these because an unknown spell is simply not
cast.

### Levels 30 to 44

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 13 | good | stealthed, not behind | Cheap Shot (1833, 30): stun and 2 points | yes (`DpsRogueStrategy.cpp:242-243`) |
| 14 | good | target under 35 percent | Dispatch (111240, 40), cheaper than Mutilate and a dagger spell | yes, 14 above Mutilate (`AssassinationRogueStrategy.cpp:126-127`); the game rejects it above 35 percent |
| 15 | good | the group has no Weakened Armor source | Expose Armor (8647, 36), 1 point (Method: only if assigned) | yes, sees any caster's Weakened Armor (`RogueTriggers.cpp:216-224`, edge `:90-91` at 23) |
| 16 | good | a second or third enemy attacks the rogue | Vanish (1856, 34) as a threat dump (derived; neither guide uses it that way) | partly: fires as soon as two enemies attack the rogue (`:93-94`), a 2 minute cooldown spent often; Preparation is the reset but see the cooldown section |
| 17 | good | from 38, an enemy under attack must stay down | Blind (2094, 38), Kidney Shot (408, 40) | Kidney Shot only as Kick's fallback (`DpsRogueStrategy.cpp:40-47` node); Blind has no edge |
| 18 | good | health at or under 45 percent | Feint (1966, 44) after Evasion | yes (`:96-98`) |

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 19 | core | 5 points, Rupture missing on the target | Rupture (1943, 46) before Envenom (Method #4; wowtbc: maintain at 1 point or more) | **missing:** the Assassination strategy has no Rupture edge at all, only a node (`AssassinationRogueStrategy.cpp:34-40`); the Combat strategy has it (`DpsRogueStrategy.cpp:101-103`). Subtlety shares this gap |
| 20 | good | stealthed, in front or behind, no Shadowstep | Garrote (703, 48): silence, 1 point (wowtbc opener) | yes: `high energy available` -> Garrote 27 then Ambush 26 (`AssassinationRogueStrategy.cpp:68-70`), needs stealth to cast |
| 21 | good | health at or under 25 percent | Cloak of Shadows (31224, 58) | yes (`:100-101`) |
| 22 | good | from 50 | Venomous Wounds (79134) | passive, nothing to press; works when Rupture is up on a poisoned target, which makes line 19 worth more (derived) |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 23 | core | Envenom at 4 or 5 points | Envenom refreshes Slice and Dice to the 5 point duration (Cut to the Chase, 60) | yes, nothing extra to do; the Slice and Dice edge only recasts a missing buff |
| 24 | good | tier 4 talent | Shadowstep (36554) as a gap closer, 8 to 25 yd | yes (`RogueTriggers.cpp:147-158`) |
| 25 | good | from 66, three or more enemies within 8 yd | Fan of Knives (51723, 66) as the builder | yes at 15 (`AssassinationRogueStrategy.cpp:110-111`), in the `aoe` strategy at 15 too (`DpsRogueStrategy.cpp:266-267`) |
| 26 | good | from 68 | Preparation (14185, 68) after Vanish | wrong: the trigger also fires for Sprint and Shadowstep (`RogueTriggers.cpp:189-203`); see the cooldown section |
| 27 | expert | from 70, Blindside proc | Dispatch at any health (121152) | the Dispatch edge casts whenever the game allows it, so the proc is used (`:126-127`) |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 78 | Tricks of the Trade on the tank before the pull and after Vanish (Method, wowtbc) | on the main tank when the rogue's threat passes half of the tank's (`:142-144`); no pre-pull cast |
| 80 | Vendetta on cooldown, in the opener (both guides) | yes at 15 once the target lacks it (`:123-124`, trigger `RogueTriggers.cpp:169-177`) |
| 81 | Redirect when a target switch keeps points (wowtbc) | missing (no action) |
| 83 | Crimson Tempest as the area finisher from three or four targets (Method: 2 targets Rupture) | missing (no action) |
| 85 | Smoke Bomb as a raid defensive | battlegrounds and arenas only (`RogueActions.cpp:70-73`) |
| 87 | Shadow Blades on cooldown | yes at 22 in combat when the buff is missing (`:137-138`, `RogueTriggers.cpp:205-208`) |
| 90 | single target (wowtbc order): Vendetta, Shadow Blades, Vanish at low energy and Preparation, Slice and Dice, Rupture (1 point or more), Marked for Death at 0 points, Envenom at 4 to 5, Dispatch under 35 percent or on a Blindside proc, Mutilate; area: Rupture on every target, Fan of Knives from 4 targets | Vendetta, Shadow Blades, Marked for Death (fewer than 2 points), Dispatch, Mutilate, Fan of Knives are there; Rupture, Preparation by its real list, Vanish at low energy, pre-pull Tricks are not |

## Cooldowns and defensives

The guides say "on cooldown"; the timing for a dungeon bot is derived (trash fights last 20 to 40 s).

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Vendetta | 80 (120 s) | on cooldown, opener | on cooldown at a pack of elites or a boss | yes (`AssassinationRogueStrategy.cpp:123-124`) |
| Shadow Blades | 87 (180 s) | on cooldown, opener | same | yes (`:137-138`) |
| Vanish | 34 (120 s) | at low energy, then Mutilate or Dispatch and Tricks (wowtbc) | as a threat reset when two enemies attack the rogue; at 80 and up inside the burst as the guide says | at "medium threat" only (`:93-94`) |
| Preparation | 68 (300 s) | when Vanish is on cooldown | only when Vanish and Evasion are both on cooldown | fires when Vanish, Sprint **or Shadowstep** is on cooldown (`RogueTriggers.cpp:189-203`): Shadowstep is not reset by the spell, and a Sprint cast on every approach (BT-059) makes it fire on a 60 s cooldown; the 300 s spell is spent on a minor reset |
| Evasion | 8 (120 s) | defensive (buffed) | health at or under 45 percent | yes (`:96-98`) |
| Feint | 44 | glyph option, Elusiveness | after Evasion, same line | yes |
| Cloak of Shadows | 58 (60 s) | second in wowtbc's defensive list | health at or under 25 percent | yes (`:100-101`) |
| Smoke Bomb | 85 (180 s) | first in wowtbc's defensive list | group takes heavy damage | battlegrounds and arenas only |
| Combat Readiness | 30 (talent, 120 s) | fourth in wowtbc's defensive list | health at or under 45 percent | missing (no action) |
| Cheat Death | 45 (talent) | Method dungeon pick | passive | not picked (Elusiveness) |
| Kick | 18 (15 s) | interrupt | any casting enemy in melee range | yes, the target and healers; any other caster in PvP only (`RogueActions.cpp:75-79`) |
| Marked for Death | 90 (60 s) | at 0 points | with fewer than 2 points | yes |
| Recuperate | 16 | not given | health at or under 30 percent, 5 points | yes |

## Healer triage and mana (healers only)

Not applicable. Energy is the resource; the combo points are the second one. Costs: Mutilate 55, Sinister Strike 50,
Dispatch 30, Fan of Knives 35, Envenom 35, Rupture 25, Slice and Dice 25, Ambush 60, Garrote 45, Cheap Shot 40,
Expose Armor 25, Kidney Shot 25, Recuperate 30, Feint 20.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Vendetta (80), Redirect (81), Crimson Tempest (83), Smoke Bomb (85), Shadow Blades (87) and
  Marked for Death (90) are not available in the 15 to 60 band; the lists above start without them. Rupture is learned at
  46, not 20 as some old rows suggest.
- Method's multi-target line "maintain Blade Flurry" is a Combat spell (spec 260, level 10): Assassination does not
  know it. The guide copies the line from the Combat page.
- Method's rotation keeps Leeching Poison up while its talent text offers Cheat Death or Elusiveness for the same tier.
  The three are one tier (45): a rogue can have only one of them.
- Method's Hit and Expertise 7.5 percent and 26.5 percent off-hand rule are level 90 numbers.
- Preparation resets Sprint, Vanish, Evasion and Dismantle (spell text); wowtbc's "Preparation to reset Vanish" is right
  but the bots' trigger counts Shadowstep.
- Instant Poison has no player-learnable row; the guides do not use it. Leeching Poison is a talent in 5.4.8 (level 45).
- Glyph of Killing Spree and Glyph of Hemorrhage are minor glyphs in this data, Glyph of Hemorrhaging Veins and Sharp
  Knives are major. The premade comments copy a typo ("Hemorraghing") from the item name.
- Method's "Envenom at 5 combo points" and wowtbc's "4 to 5, pool energy, refresh at 1 second" are raid refinements
  that need a clock the bots do not keep.

## Gap list (ordered by value for levels 15 to 60)

Config (can go at once):

1. Glyphs: `PremadeSpecGlyph.4.0` (`conf/playerbots.conf.dist:2739`) to `45766,43380,42963,43378,43379,45761`. Today
   a level 25 to 79 Assassination bot carries Vendetta, Redirect and Smoke Bomb, none of which does anything in a dungeon.
2. Tier 15: Shadow Focus instead of Nightstalker (`PremadeSpecLink.4.0`, `:2738`), pending the owner's check of
   Method's talent image.
3. Tier 45: Cheat Death instead of Elusiveness (dungeon pick of Method, no button needed). Together: `220101`.
4. Tier 30: keep Combat Readiness only together with rotation item 6; otherwise it is a dead talent.

Rotation (class fixes, wait for the owner's in-game test):

1. Rupture for Assassination (and Subtlety): an edge "combo points 5 available" -> Rupture (if the target lacks it) then
   Envenom, as `DpsRogueStrategy.cpp:101-103` does for Combat; file `AssassinationRogueStrategy.cpp::InitTriggers`
   (`:84-85` is today the only finisher edge). From level 46. Core. Method puts Rupture fourth, wowtbc "at 1 point or more".
2. Envenom against Slice and Dice tie at 25 (`AssassinationRogueStrategy.cpp:79`, `:85`): give Slice and Dice 26 so a
   missing buff is recast before the finisher; with Cut to the Chase (60) the Envenom already refreshes it.
3. Preparation trigger (`RogueTriggers.cpp:189-203`, `PreparationTrigger::IsActive`): drop Sprint and Shadowstep from the
   names, require Vanish on cooldown (and Evasion for a bot below 45 percent), as spell text and both guides read.
4. A main-hand dagger rule for gear (`Mgr/Item/StatsWeightCalculator.cpp:876-881`): a non-dagger main hand scores x0.1
   for Assassination from level 10 (Mutilate and Dispatch fail without one). Gear scoring, not strategy.
5. Stat weights (`StatsWeightCalculator.cpp:422-433`): Agility above Hit and Expertise until the level 90 caps are in
   reach. Gear scoring.
6. `combat readiness` action (a self buff like `CastEvasionAction`, `RogueActions.h:13-17`) on "low health", beside Evasion
   (`AssassinationRogueStrategy.cpp:96-98`), if the talent stays picked.
7. A Leeching Poison action beside Deadly Poison (`GenericRogueNonCombatStrategy.cpp:72`) for the tier 45 pick, replacing
   Crippling Poison when the talent is known (Method poison line). Config tier 45 becomes Leeching Poison then.
8. Energy gate: builders are held to 60 energy by "high energy available" (`GenericTriggers.h:138-142`); a gate per
   builder cost (Mutilate 55, Dispatch 30) removes the half-second idle. Minor; the regeneration rate is to check.
9. Vanish at two attackers (`:93-94`): with the 120 s cooldown spent on every double pull, Preparation (300 s) is the
   only reset. Owner decision whether a damage bot should dump threat that way; the tank's job is to hold it.
10. Peel and crowd control (expert tier): an edge for Gouge (22) and Kidney Shot (40) when an enemy beats on the rogue,
    and `cc` (Sap, 12) on rogue bots (`AiFactory.cpp:481-491`). Only worth it with the tier 5 talent Prey on the Weak or
    Dirty Tricks, which then stops being dead.
11. Pre-pull Tricks of the Trade on the tank (level 78) and Redirect (81), Crimson Tempest (83): level 75 and up, outside
    the band; the actions are missing.
12. Skill tiers: the rogue strategies have no tier gating (`BotSkill` is used only in the tank code). Lines marked good
    and expert above (Kick on any caster, Expose Armor, Gouge, Rupture refinements, Vanish timing) are the ones a casual
    bot should drop.
13. To check live: BT-057 (stealth before the pull) and BT-059 (the stealthed opener swap) are deployed with their live
    check pending; the opener lines 2 and 3 depend on them. Also to check: whether Sinister Strike stays castable after
    Dispatch replaces it (the chain Mutilate -> Backstab -> Sinister Strike falls back to it when a bot has no dagger).

## PvP playbook (arena, 5.4.8)

New section, 2026-11-19. Sources: player guides (5.x: one 5.2 battleground guide, one 5.4 arena guide with macros and rotation, one 5.4.8 basics guide with rough captions, a PvE damage clip) and community guides (a 5.2 Subtlety guide that mentions the spec), the 5.x patch notes, the server's own data (`data/dbc/` through `spec_data.py` and short read-only scripts) and the module code read on `main`. Lines say "(guide)" when a player guide gives them, "(adapted)" when they come from the Subtlety rogue arena material and were checked against the Assassination spell list, "(derived)" when they follow from the 5.4.8 data and game knowledge. About 45 percent of the section is sourced, 10 percent adapted, 45 percent derived. Where nothing reliable exists the line says GAP.

Where the spec stands: no rogue of any specialisation is in the MoP Classic EU top 100 ladder of 2026-11-18 (96 players in 3v3, 54 in 2v2), a community tier list of July 2025 rates Assassination C in 3v3 and C in 2v2, and the 5.2 guide already said Subtlety is the arena rogue and Assassination is better in battlegrounds. A niche pick: the value of a rule is lower than for a ladder spec.

### Main plan (guide)

1. Poisons and bleeds are the damage: keep Deadly Poison on, Rupture up (it returns energy through Venomous Wounds) and Slice and Dice up.
2. Open from stealth on a healer or caster (Garrote silence, then Cheap Shot) or on a melee target (Cheap Shot), then build with Mutilate to five points and Rupture.
3. One go button: Vendetta (+30 percent damage on the target, 20 s, 120 s), Shadow Blades, Marked for Death and Envenom while the target is stunned; stun whenever it helps and use Vanish for the killing blow.

### Opener (guide, derived)

- Subterfuge keeps stealth for 3 s after a hostile action, so Garrote, Cheap Shot and Ambush can follow each other and give about five points, then Rupture (guide). The three cost 145 energy and the maximum is 120 with daggers (Assassin's Resolve, +20), so the full triple needs the old PvP four-piece set bonus (+30 maximum energy, patch 5.2) or Shadow Focus (-75 percent while stealthed: the three cost 36). A bot has no set bonus, so Shadow Focus is the derived tier 15 pick; with Subterfuge alone the opener is two spells.
- Burst opener (guide): Garrote, Slice and Dice, step away without breaking stealth, Cheap Shot, cooldowns, Envenom, Mutilate twice, Envenom, Kidney Shot. Trinkets and engineering gloves break the stealth: use them after the first stun.
- Premeditation (30) before the opener gives free points for Slice and Dice (adapted). Prep room (adapted): Tricks of the Trade on the partner in stealth, do not open first when another stealth class is unrevealed, Sap a second enemy. GAP: map specific routes.
- Poisons (derived): Deadly (10) and Crippling (20) by default; Wound Poison (30) against a healer, Mind-numbing (28) against casters; Paralytic and Leeching are the tier 75 and tier 45 talents.

### Priority outside a window (guide, derived)

| Priority | Tier | Line |
|---|---|---|
| 1 | core | Slice and Dice up (24 to 36 s at three to five points); Rupture up at five points (24 s) |
| 2 | core | Envenom at four or five points with time left on both; Envenom refreshes Slice and Dice to its five point length (Cut to the Chase, 60) |
| 3 | core | Mutilate as the builder (55 energy, two points); Dispatch on a Blindside proc (70) or a target under 35 percent |
| 4 | good | Kick on a heal or a crowd control cast (15 s, 5 s lock); Garrote from stealth on the healer |
| 5 | good | Kidney Shot at five points only in a go or as a peel; Recuperate (16) in downtime |
| 6 | expert | Wound Poison on the healer's team, Shiv (74) for the concentrated non-lethal effect, Rupture on up to three targets for energy |

Stat order (guides): mastery first in all of them; second crit in two (haste is weak in PvP outside Shadow Dance) and haste in one. Take mastery, then crit, then haste; hit and expertise 3 percent each are enough (derived from the guides). Resilience gems are a 5.2 habit; from 5.3 resilience is off most gear.

### Go window (guide, derived)

Trigger (derived): the enemy healer is stunned, blinded or silenced by the team, its trinket is down, Vendetta is ready and the kill target has no immunity or Cloak of Shadows. Order (guide, spell text): Marked for Death (five points), Vendetta, Shadow Blades (87), Envenom at five points, Mutilate twice, Envenom, Kidney Shot as the closing stun; Vanish for the last hit or to restealth. Preparation (68) resets Sprint, Vanish, Evasion and Dismantle, not Cloak of Shadows (data).

### Crowd control (guide, adapted)

Cheap Shot (4 s, +1 s with its glyph) then Kidney Shot is about 7 s, Kidney then Cheap Shot 8 s (adapted from the stun category). Garrote silence 3 s (+1 s with its glyph). Blind (38, healers; shares the fear category): with Dirty Tricks it costs no energy and no longer breaks from your poison and bleed damage (data), so a blinded healer stays blinded under Rupture; the Blind glyph (clears damage over time) is then not needed. Paralytic Poison: a stun at four stacks outside the stun category and a 4 s root on Shiv. Prey on the Weak: 10 percent more damage on a disabled target. Nerve Strike: the stunned target deals 25 percent less damage on players for 6 s after the stun (50 before 5.4.7). Dismantle (52, 60 s) against a warrior, ret or death knight burst.

### Defensives

Elusiveness (Feint cuts all damage by 30 percent for 5 s, 20 energy), Combat Readiness (up to 50 percent less melee damage, useless against casters), Cheat Death (survive at 10 percent and take 85 percent less for 3 s, 90 s), Cloak of Shadows (58, 5 s, magic), Evasion (100 percent dodge, 10 s, melee only and facing), Vanish (34), Smoke Bomb (85), Shadowstep and Burst of Speed (clears slows even with Sprint up since 5.4.8). Kidney Shot at one point for Nerve Strike is a guide's trick. Evasion, Cloak and Vanish by role: adapted.

### Matchups and partners

Guide: against warriors take Nerve Strike (warriors are a rogue's nemesis); a 5.4 player wins damage against warriors, death knights and shadow priests; against mages Garrote first to deny a Blink from Cheap Shot, Smoke Bomb against casters with slows; against hunters take Shadowstep. Partners (adapted): a caster that locks the healer and a melee that stuns; Kidney the kill target when the partner lands a trap or fear.

### PvP set (no ladder data)

Premade today: `PremadeSpecLinkPvp.4.0 = 112201` (Subterfuge, Nerve Strike, Elusiveness, Burst of Speed, Prey on the Weak, Marked for Death) and glyphs `64493,43376,42969,43379,43378,45769` (Blind, Distract, Cheap Shot, Blurred Speed, Safe Fall, Cloak of Shadows); it was copied from the Subtlety guide. The Assassination guides agree on Elusiveness and Marked for Death, split on Subterfuge or Shadow Focus, Combat Readiness or Nerve Strike, Shadowstep or Burst of Speed, Prey on the Weak or Paralytic Poison, and name the Garrote and Feint glyphs that the set lacks.

### Differences from the guides (5.4.8)

- Assassin's Resolve is +20 percent damage and +20 energy (a guide says 25 percent); Shadow Focus is -75 percent energy (a guide says 35); Nerve Strike is 25 percent on players since 5.4.7; Evasion is 2 minutes and 10 s since 5.4.0; Shuriken Toss costs 40 since 5.3 and Cloak and Dagger needs Stealth.
- Level gates: Vendetta 80, Shadow Blades 87, Preparation 68, Redirect 81, Smoke Bomb 85, Shiv 74, Tricks of the Trade 78, Dismantle 52, Blindside 70, Venomous Wounds 50, Rupture 46, Garrote 48.

### Bots today (read on `main`, 2026-11-19)

| Rule | Bots today |
|---|---|
| Stealth and a Garrote or Ambush opener | partly (`AssassinationRogueStrategy.cpp:69`; the stealth edge needs a pull target within about 30 yd, `GenericRogueNonCombatStrategy.cpp:53-55`; the arena start room is not read: to check) |
| Opener order Garrote, Cheap Shot, Ambush by energy | missing (single steps exist in the stealth strategy, `DpsRogueStrategy.cpp:239-254`) |
| Slice and Dice at three points, Rupture at five, Envenom at four | exists (`AssassinationRogueStrategy.cpp:78,85,88`) |
| Dispatch on Blindside or under 35 percent | partly (`:134`, energy 30, no Blindside read) |
| Vendetta, Shadow Blades, Marked for Death | partly: each fires on its own when ready (`RogueTriggers.cpp:168,177-185,210-213`); no window, no order |
| Kick on a heal or a crowd control cast | exists (`AssassinationRogueStrategy.cpp:111,114,124`, through the interrupt gate) |
| Blind the healer, Dirty Tricks | missing (the action exists, `RogueAiObjectContext.cpp:187`; no edge) |
| Evasion, Combat Readiness, Feint, Cloak of Shadows, Smoke Bomb | partly (`:96-110`, `:138`; no melee or caster split; Feint is not used before a go) |
| Wound Poison, Mind-numbing Poison, Shiv | missing (Deadly and Crippling only, `GenericRogueNonCombatStrategy.cpp:76-78`) |
| Rupture on several targets | missing |
| Dismantle on a burster | missing |
| Tricks of the Trade on the partner at the go | partly (main tank only, `AssassinationRogueStrategy.cpp:148`) |

### Gap list (ordered by value; rotation items wait for the owner's in-game test)

Config:

1. `PremadeSpecLinkPvp.4.0` tier 15 to Shadow Focus (digit 2) for bots without the set bonus (derived); the rest stays (both Shadowstep and Burst of Speed have code).
2. Glyphs: add Garrote and Feint, drop Distract and Safe Fall (guide); item ids to look up.

Rotation:

1. The healer lock tied to a window: Garrote or Cheap Shot, Blind with Dirty Tricks, Kidney, with the stun category respected (expert).
2. The ordered go: Marked for Death, Vendetta, Shadow Blades, Envenom, only with the healer controlled and the target without an immunity (expert).
3. Wound Poison against a healer and Mind-numbing against casters (good).
4. Evasion against melee, Cloak against casters, Feint before an expected burst (good).
5. Vanish and restealth after a go behind a pillar (expert).
6. Rupture on several targets (expert).
