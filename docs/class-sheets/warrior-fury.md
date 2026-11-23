# Warrior: Fury (damage)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). The level 81 and higher
  windows are left for later. The "bots today" columns below describe the code before the 2026-11-11 implementation.
- **Sources:**
  - Method, "Fury Warrior Class Guide - MoP Classic" (Phase 1, level 90 raid and dungeon, 22 July 2025), read
    2026-11-11.
  - wowtbc.gg, "Fury Warrior" in the MoP class guides (MoP Classic, no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, Talent, GlyphProperties, SpellCooldowns,
    SpellPower, SpellEffect, SpellShapeshift, SpellEquippedItems; `data/db2/Item-sparse.db2` for glyph item ids)
    through `spec_data.py` and short read-only scripts.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch, checked 2026-11-10, not retried); Wowhead needs a
    browser.
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer takes the tank's marks and
  follows the leader), 61 to 90 briefly from the guides.

Which source was richer: **talents, glyphs (separate raid and dungeon sets), stat order, multi-target priority** Method
(Whirlwind counts before each Raging Blow, a dungeon build; wowtbc lists every talent option without a pick and has no
stat order); **opener, single target (the Colossus Smash window), cooldown timing, rage thresholds, defensive order**
wowtbc; **levels 15 to 60, what to do before a spell is learned, Titan's Grip against Single-Minded Fury, interrupts**
neither guide says anything (both are level 90 raid guides), so those lines are derived from the 5.4.8 data and marked
"derived".

## Talents

Tiers and columns are from Talent.dbc. "Bots today" is `AiPlayerbot.PremadeSpecLink.1.1 = 001220`
(`conf/playerbots.conf.dist:2663`; one digit per tier, column counted from 0): Juggernaut, Enraged Regeneration,
Piercing Howl, Dragon Roar, Vigilance, Avatar.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (001220) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Double Time or Juggernaut (Method: swap when the second charge is not needed) | Warbringer is a wowtbc option only | 103827 / 103826 | Juggernaut, one of Method's two. No change: a bot charges about once per pull |
| 2 (30) | Enraged Regeneration or Second Wind, fight dependent (Method) | Impending Victory (wowtbc option only) | 55694 / 29838 | Enraged Regeneration, same |
| 3 (45) | Disrupting Shout or Piercing Howl, crowd control dependent (Method) | Staggering Shout (wowtbc option) | 102060 / 12323 | Piercing Howl. **Dead today:** the Fury strategy has no snare edge (the Arms one is `ArmsWarriorStrategy.cpp:82`), so the talent is never used; Disrupting Shout is dead too until the pack interrupt line exists (rotation item 6 of the gap list) |
| 4 (60) | Dragon Roar for single target (Method raid build; wowtbc tags it "single target") | Bladestorm: Method's dungeon and area build ("Bladestorm + Bloodbath as the key pairing"), later in a raid with a cancel macro | 118000 / 46924 | Dragon Roar, the raid pick |
| 5 (75) | Vigilance ("strong, not to be ignored", Method) | Mass Spell Reflection, Safeguard (wowtbc options) | 114030 | Vigilance, same (`WarriorTriggers.cpp:14`, `WarriorActions.cpp:104`) |
| 6 (90) | Bloodbath (Method); Storm Bolt only when the target is immune to crowd control | Avatar is not named for a damage dealer in either guide (wowtbc tags Bloodbath "AoE") | 12292 / 107570 | **Avatar, differs from the guide** (`PremadeSpecLink` comment cites wowtbc, which marks no pick) |

Notes on the picks (data, not guide):

- Dragon Roar (60 s): always a critical hit, ignores armor, and "knocks back and knocks down" every enemy in range
  (spell text), which scatters a dungeon pack away from the tank (derived). Method's dungeon build is Bladestorm: the
  owner decides whether a Fury bot takes Bladestorm for dungeon groups.
- Bladestorm (60 s): "you can only perform shout abilities" while it runs (spell text); a bot cannot cancel it, so for
  six seconds it uses no other button (Method itself says "not recommended early").
- Disrupting Shout (40 s) shares a 15 s cooldown with Pummel (spell text).
- Bloodbath (60 s) adds a bleed to special attacks; Avatar (180 s) is a damage buff that breaks roots and snares.

Proposed link, guide dungeon line: `102021` (Double Time / Enraged Regeneration / Disrupting Shout / Bladestorm /
Vigilance / Bloodbath); raid single target: `102221`. Proposed for the bots now: `001221` (only the level 90 pick), and
`002221` once the pack interrupt line exists. For a character below 75 only the first four digits are in effect.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and
75 (config comment; the unlock levels are core-side, not in the data files). The config order is major, minor, major,
minor, minor, major (`conf/playerbots.conf.dist:2634`); a glyph goes to the first free slot of its type.

| Type | Glyph | Item id | Why (one clause) | Bots today (`45792,104136,43399,45794,43395,67482`) |
|------|-------|--------:|------------------|------------------------|
| Major | Unending Rage | 43399 | maximum rage 100 to 120 (Method, both builds) | yes (slot 2, level 50) |
| Major | Bull Rush | 67482 | Charge gives 15 more rage; Charge is known from level 3 (Method, both builds) | yes, but in the third slot (level 75) |
| Major | Raging Wind (Fury only) | 43432 | Raging Blow hits raise the next Whirlwind's damage; Method calls it a must-have for dungeons | no |
| Major | Colossus Smash | 63481 | the Smash also applies the Sunder Armor effect, for a group with no other armor debuff; spell learned at 81 (Method raid default) | no |
| Major, situational | Death From Above | 45792 | Heroic Leap cooldown 15 s shorter; Heroic Leap is learned at 85 (wowtbc standard) | **yes, in the first slot (level 25), where it does nothing until 85** |
| Major, situational | Rude Interruption | 43417 | Pummel interrupt gives a damage buff, for kick-heavy fights | no |
| Major, situational | Enraged Speed, Die by the Sword | 43413, 43422 | mobility while Enraged; Wild Strike and Overpower lengthen Die by the Sword (defensive, Method) | no |
| Minor | Subtle Defender (Method: the only minor worth a slot) | 104136 | removes the threat bonus of Defensive Stance, which a damage bot never uses, so it is a free pick | yes |
| Minor | Intimidating Shout, Mystic Shout | 45794, 43395 | no damage effect (targets tremble in place instead of fleeing; you hover); wowtbc lists Intimidating Shout | yes (harmless) |

Disagreement: wowtbc's standard set is Death From Above, Unending Rage, Bull Rush (the premade's); Method's dungeon set
is Raging Wind, Bull Rush, Unending Rage and its raid set Unending Rage, Colossus Smash, Bull Rush. Method is used for
the picks (it separates raid and dungeon). The slot order is derived: a glyph in a slot must be useful at the level the
slot unlocks. Level 25 to 49 has one major slot: Bull Rush (Charge since level 3); Unending Rage at 50; Raging Wind at 75
(Raging Blow since 30, Whirlwind since 26). Proposed `PremadeSpecGlyph.1.1`: `67482,104136,43399,45794,43395,43432`.

## Stat order

Method: Hit and Expertise to about 7.5 percent each, Strength, Critical Strike, Mastery, Haste; optionally 26.5 percent
Hit so that the off-hand never misses. wowtbc has no stat order. The caps are level 90 numbers and cannot be reached
with the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **Two melee weapons**, then the rest. Crazed Berserker (level 10) lets a Fury warrior dual-wield; Wild Strike needs an
  off-hand weapon (SpellEquippedItems: off-hand weapon slot) and Raging Blow hits "from both weapons". From level 38 the
  specialisation knows both Titan's Grip (a pair of two-handed weapons) and Single-Minded Fury (a pair of one-handed
  weapons, more damage); each applies by the weapons held, and neither guide says which to chase.
- Then plate (Plate Specialization, level 50, gives Strength only in all-plate), Strength, Critical Strike, Mastery
  (Unshackled Fury from level 80: more damage while Enraged), Haste last; Hit and Expertise matter only near level 90.
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:436-448`): Strength 2.5, Expertise 2.5, Hit 2.3, Critical 2.2, Mastery
  2.0, Haste 0.8. Order agrees with Method (Strength first, Haste last, Mastery over Haste). Weapons
  (`:808-873`): before 38 a one-handed pair is wanted (two-handers cut to a twentieth); once Titan's Grip is known one-handed
  weapons are cut to a tenth and slow two-handers are wanted in both hands, so the bots chase the Titan's Grip pair and
  never Single-Minded Fury (no guide support either way). The armor penetration weight (2.1) belongs to a stat the
  Cataclysm changes removed (inference, to check), harmless.

## What the specialisation knows, by level (5.4.8 data)

Ids are the spells the player actually casts; the data holds several old rows of the same name. Rage costs are the
SpellPower values divided by 10.

| Level | Spell (id) |
|------:|------------|
| 1 | Heroic Strike (78, 30 rage, 1.5 s cooldown, no global cooldown), Battle Stance (2457) |
| 3 | Charge (100, 8 to 25 yd, 20 s, gives rage) |
| 5 | Victory Rush (34428, free, after a kill) |
| 7 | Execute (5308, 30 rage, target under 20 percent) |
| 10 | Bloodthirst (23881, free, 4.5 s, gives 10 rage, heals, double critical chance); Crazed Berserker (23588, passive: dual wield, more melee damage); the specialisation is chosen here |
| 14 | Enrage (13046, passive: critical hits of Mortal Strike, Bloodthirst, Colossus Smash, Devastate and Shield Slam enrage you: +10 rage, +10 percent physical damage) |
| 16 | Sunder Armor (7386, 15 rage) |
| 18 | Wild Strike (100130, 30 rage, off-hand strike, needs an off-hand weapon) |
| 20 | Thunder Clap (6343, 20 rage, 6 s) |
| 22 | Heroic Throw (57755, 30 s, ranged) |
| 24 | Pummel (6552, 15 s) |
| 26 | Whirlwind (1680, 30 rage) |
| 30 | Raging Blow (85288, 10 rage, no cooldown, needs "Raging Blow!": becoming Enraged gives a use, up to 2); tier 2 talent |
| 32 | Deep Wounds (115768, passive; Bloodthirst leaves a bleed) |
| 34 | Berserker Stance (2458) |
| 36 | Hamstring (1715, 10 rage) |
| 38 | Titan's Grip (46917, passive), Single-Minded Fury (81099, passive) |
| 42 | Battle Shout (6673, 60 s, gives rage) |
| 44 | Cleave (845, 30 rage) |
| 45 | tier 3 talent |
| 50 | Bloodsurge (46915, passive: Bloodthirst has a 20 percent chance to make the next 3 Wild Strikes free with a 1 s global cooldown); Plate Specialization (86110, passive) |
| 52 | Intimidating Shout (5246, 90 s) |
| 54 | Berserker Rage (18499, 30 s: Enrage, +10 rage, breaks fear, sap and incapacitate) |
| 56 | Die by the Sword (118038, 120 s: +100 percent parry, 20 percent less damage); Unbridled Wrath (143268, passive) |
| 58 | Meat Cleaver (12950, passive: Whirlwind damage adds targets to the next Raging Blow, stacking) |
| 60 | Flurry (12972, passive); tier 4 talent |
| 62 | Recklessness (1719, 180 s: +30 percent critical chance on special attacks) |
| 66 | Spell Reflection (23920) |
| 68 | Commanding Shout (469, gives rage) |
| 72 | Intervene (3411) |
| 74 | Shattering Throw (64382, 300 s: armor down 20 percent or breaks an immunity) |
| 75 | tier 5 talent |
| 80 | Mastery: Unshackled Fury (76856) |
| 81 | Colossus Smash (86346, 20 s: ignores armor, Physical Vulnerability) |
| 83 | Rallying Cry (97462) |
| 85 | Heroic Leap (6544, 45 s) |
| 87 | Skull Banner (114207, 180 s), Demoralizing Banner (114203), Mocking Banner (114192) |
| 90 | tier 6 talent |

Facts the priority lists rest on (spell texts and tables of the data):

- No ability of this list needs a stance (SpellShapeshift is empty for them), so there is no stance dance. Battle
  Stance "generates high rage from normal melee attacks"; Berserker Stance (level 34) "some rage from melee and some from
  damage taken". Neither changes damage in the spell text.
- Enrage is the engine of the specialisation: a Bloodthirst critical hit (twice the normal chance) enrages, which grants
  a Raging Blow use; Berserker Rage enrages on demand. Mastery from level 80 makes Enraged damage larger.
- Heroic Strike and Cleave are "use when you have more rage than you can spend" (spell text); Heroic Strike has no global
  cooldown, so it is cast between global cooldowns.
- Execute costs 30 rage and is "only usable under 20 percent health", a fixed hit in this data.
- Slam is Arms only (spec 71); Wild Strike is Fury only (spec 72); Whirlwind is in both specialisation lists.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked
"derived" comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to
`mod-aware-bots-548/src/` at commit `605b17f`.

How the bots rank what they do (read, `strategy/Strategy.h:37-51`): ACTION_DEFAULT 5, ACTION_HIGH 20, ACTION_MOVE 30,
ACTION_INTERRUPT 40, ACTION_EMERGENCY 90. A trigger edge adds its action with that number; the default actions of
`FuryWarriorStrategy::getDefaultActions` (`FuryWarriorStrategy.cpp:35-44`) are 5.7 down to 5.0 (Recklessness, Bloodbath,
Bloodthirst, Dragon Roar, Sunder Armor, Execute, melee). The engine takes the highest action that is possible and
useful. A level 10 and up warrior without Arms gets the Fury strategy (`Factory/AiFactory.cpp:378-381`), together with
`aoe` and `dps assist`.

What this means for Bloodthirst: it is a default at 5.5 while Raging Blow and the Bloodsurge Wild Strike are edges at 29
(`FuryWarriorStrategy.cpp:57-59`), so whenever a Raging Blow use exists the bot casts it first and Bloodthirst waits.
In a pack (two or more enemies within 8 yd) Thunder Clap (25) and Whirlwind (24) rank above Bloodthirst too
(`GenericWarriorStrategy.cpp:154-156`, `:171-173`), and Whirlwind is castable at 30 rage.

### Levels 15 to 29

Known: Heroic Strike, Charge, Victory Rush, Execute, Bloodthirst, Crazed Berserker, Sunder Armor (16), Wild Strike (18),
Thunder Clap (20), Heroic Throw (22), Pummel (24), Whirlwind (26).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | target out of melee, 8 to 25 yd | Charge (100, 3): opener and gap closer, +20 rage | yes: "enemy out of melee" edge (`GenericWarriorStrategy.cpp:72`), chain charge, heroic leap, reach melee (`:39-40`, `:76-77`) |
| 2 | core | Bloodthirst ready (4.5 s) | Bloodthirst (23881, 10), on cooldown: +10 rage, a heal, and its critical hits enrage | yes, but at the bottom of the list (default 5.5, `FuryWarriorStrategy.cpp:38`) |
| 3 | core | target under 20 percent, rage 30 or more | Execute (5308, 7) after Bloodthirst | **wrong order:** fires first, at 40, above Bloodthirst (`GenericWarriorStrategy.cpp:78`, trigger `HealthTriggers.h:135-139`); Method casts Execute after Bloodthirst and Raging Blow |
| 4 | core | a kill just happened | Victory Rush (34428, 5), free heal | yes at 40 (`GenericWarriorStrategy.cpp:79`) |
| 5 | core | Bloodthirst on cooldown, rage 30 or more (derived: from 18 until Bloodsurge at 50 the specialisation has no other filler, and both guides end the list with Wild Strike) | Wild Strike (100130, 18), needs an off-hand weapon | **gap:** only as a Bloodsurge proc (`FuryWarriorStrategy.cpp:59`), a passive Fury learns at 50; levels 18 to 49 never cast it |
| 6 | core | rage 60 or more once Wild Strike is the filler (derived: with the filler at 30 a dump at 40 would starve it) | Heroic Strike (78, 1) | partly: at 40 rage and at the lowest rank, 5.1 (`FuryWarriorStrategy.cpp:64`, `GenericTriggers.h:106`); right as long as nothing else spends rage, wrong once item 5 exists |
| 7 | good | two or more enemies within 8 yd, from 26 | Whirlwind (1680, 26) | yes at 24 (`GenericWarriorStrategy.cpp:156`, `:173`) |
| 8 | good | from 24: an enemy casts | Pummel (6552, 24) | partly: the current target (`GenericWarriorStrategy.cpp:96`) and an enemy healer (`FuryWarriorStrategy.cpp:62`); any other caster in PvP only (`:99-102`) |
| 9 | good | two or more enemies, from 20 | Thunder Clap (6343, 20) for the group (Weakened Blows) | yes at 25, **above Whirlwind**; neither guide lists it for Fury (area is Whirlwind and Raging Blow), so it costs 20 rage that Whirlwind or Wild Strike would use (`GenericWarriorStrategy.cpp:154`, `:171`) |
| 10 | expert | target out of melee, Charge on cooldown, from 22 | Heroic Throw (57755, 22) | missing for damage dealers (only the tank has it, `TankWarriorStrategy.cpp:116`) |

Before a spell is learned (derived): until 18 the kit is Bloodthirst, Heroic Strike and Execute; Whirlwind and Pummel
wait for 26 and 24. The bots tolerate all of these because an unknown spell is simply not cast.

### Levels 30 to 44

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 11 | core | Enraged (a Bloodthirst critical hit or Berserker Rage) and Raging Blow has 2 uses | Raging Blow (85288, 30), 10 rage, before Bloodthirst (Method) | yes, but at 29 above Bloodthirst even with one use (`FuryWarriorStrategy.cpp:57`) |
| 12 | core | Bloodthirst ready | Bloodthirst on cooldown, then Raging Blow with one use (Method order: Raging Blow at 2 uses, Bloodthirst, Wild Strike on a proc, Raging Blow, Wild Strike) | **wrong order**, see item 11 |
| 13 | good | from 38: weapons | Titan's Grip (46917) or Single-Minded Fury (81099), by the weapons held | not a rotation matter; gear scoring chases Titan's Grip (see Stat order) |
| 14 | good | two or more enemies, from 44 | Cleave (845, 44) instead of Heroic Strike | yes (`GenericWarriorStrategy.cpp:157`, `:174`) |
| 15 | good | from 42: the shout is missing, before the pull | Battle Shout (6673, 42) | partly: combat only, at 29 (`FuryWarriorStrategy.cpp:54`); the non-combat strategy has no shout line (`GenericWarriorNonCombatStrategy.cpp:20-33`) |
| 16 | good | from 36: an enemy runs from the group | Hamstring (1715, 36) | **gap:** the Fury strategy has no snare edge (Arms has one, `ArmsWarriorStrategy.cpp:82`) |

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 17 | core | Bloodsurge proc (20 percent on Bloodthirst) | Wild Strike (100130), free, 1 s global cooldown, at once | yes at 29 (`FuryWarriorStrategy.cpp:59`), above Bloodthirst; Method puts Bloodthirst first |
| 18 | good | Enrage is down | Berserker Rage (18499, 54), +10 rage; also breaks fear, sap, incapacitate | partly: cast on cooldown whenever rage is 20 or more, Enrage state not read (`GenericWarriorStrategy.cpp:71`, priority 40); crowd control edges `:107`, `:111` |
| 19 | core | three or more enemies, from 58 | Whirlwind two times (3 targets) or three times (4 or more) before each Raging Blow (Meat Cleaver, 58; Method) | **wrong order:** Raging Blow is 29 and Whirlwind 24, so the stacks only build while no Raging Blow use exists (`FuryWarriorStrategy.cpp:58`, `GenericWarriorStrategy.cpp:156`) |
| 20 | good | health at or under 40 percent | Die by the Sword (118038, 56) | yes (`GenericWarriorStrategy.cpp:93`; the node falls back to Enraged Regeneration, `:54`) |
| 21 | good | health at or under 50 percent (derived, as for the tank sheet) | Enraged Regeneration (55694, talent tier 2) | partly: only at 25 percent for Fury (`FuryWarriorStrategy.cpp:66`) |
| 22 | good | health at or under 25 percent, from 52 | Intimidating Shout (5246, 52) as a panic button | **gap** for Fury (Arms has the edge at `ArmsWarriorStrategy.cpp:84`) |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 23 | core | on cooldown, from 62 | Recklessness (1719, 62) | yes, first default, ungated (`FuryWarriorStrategy.cpp:36`), as both guides say; Arms is gated by a balance check, Fury is not |
| 24 | good | tier 4 talent at 60 | Dragon Roar (118000), Bladestorm (46924) in packs | yes (`FuryWarriorStrategy.cpp:41`; aoe lists `GenericWarriorStrategy.cpp:149-150`) |
| 25 | expert | from 68: both shouts, for rage (both guides: "shouts generate rage") | Commanding Shout (469, 68) beside Battle Shout | missing for damage dealers (only the tank casts it, `TankWarriorStrategy.cpp:75`); BT-036 keeps shouts as upkeep only |
| 26 | expert | from 74: single target, armor debuff in the opener (Method, wowtbc) | Shattering Throw (64382, 74) | partly: only against an immunity (`ArmsWarriorStrategy.cpp:88` is Arms only; Fury has no edge at all) |

### Levels 75 to 89

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 27 | good | tier 5 talent at 75 | Vigilance (114030) | yes (`GenericWarriorNonCombatStrategy.cpp:27`) |
| 28 | core | from 81: Colossus Smash not on the target; wowtbc: when Enraged, with a Raging Blow use and 80 rage or more | Colossus Smash (86346, 81); inside its window Raging Blow, Bloodthirst, Execute, Wild Strike, Heroic Strike freely | partly: cast whenever the debuff is missing (`GenericWarriorStrategy.cpp:80`); the conditions and window logic are missing |
| 29 | good | from 83 and 87 | Rallying Cry (97462), Skull Banner (114207) | yes (`GenericWarriorStrategy.cpp:89-92`, `:105`) |
| 30 | good | from 85, inside the window or as a gap closer | Heroic Leap (6544, 85) | gap closer only (`GenericWarriorStrategy.cpp:76`) |

### Level 90

Guide, single target (Method's order with wowtbc's conditions): Recklessness, Skull Banner, Shattering Throw if assigned,
Dragon Roar (when Colossus Smash is down), Colossus Smash, Storm Bolt (only inside the window, if talented), Berserker
Rage while Enrage is down, Heroic Strike from 100 to 110 rage, Raging Blow at 2 uses, Bloodthirst, Wild Strike with
Bloodsurge, Raging Blow, Wild Strike, Battle Shout; Execute (under 20 percent) at 100 rage and more outside the window
and before Wild Strike inside it. Charge below 85 rage (wowtbc). Opener: potion, Charge, Bloodthirst, Recklessness,
Skull Banner, Shattering Throw, Dragon Roar, Colossus Smash, Berserker Rage, Heroic Leap, Storm Bolt, then Raging Blow
and Bloodthirst (wowtbc). Area: Recklessness, Skull Banner, Bloodbath, Bladestorm, Berserker Rage and Bloodthirst to stay
Enraged, Whirlwind two or three times before each Raging Blow, Whirlwind when no use exists (Method).

Bots: Recklessness, Bloodbath, Bloodthirst, Dragon Roar, Raging Blow, Wild Strike on a proc, Execute, Colossus Smash,
Skull Banner and Avatar (instead of the guide's Bloodbath) are there; the order and rage thresholds differ (Execute and
Raging Blow above Bloodthirst, Heroic Strike at 40 instead of 100 to 110, no Colossus Smash conditions, no Storm Bolt,
Whirlwind after Raging Blow).

## Cooldowns and defensives

Press timing (the guides say "on cooldown"; the thresholds are derived because dungeon trash needs numbers):

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Berserker Rage | 54 | on cooldown when Enrage is down | when Enrage is down and rage is 20 or more | cast at 20 rage whenever ready, Enrage not read |
| Recklessness | 62 | on cooldown | at the start of a pack of elites or a boss | on cooldown at once (default 5.7) |
| Dragon Roar / Bladestorm | 60 | Dragon Roar on cooldown (wowtbc: only when Colossus Smash is down) | Dragon Roar on a boss, Bladestorm at a pack of 3 or more | Dragon Roar default; both in the aoe lists |
| Colossus Smash | 81 | on cooldown with Enrage up, a Raging Blow use and 80 rage | same | whenever the debuff is missing |
| Skull Banner | 87 | on cooldown | on cooldown in a pack or boss | yes |
| Die by the Sword | 56 | defensive | health at or under 40 percent | yes |
| Enraged Regeneration | 30 (talent) | defensive | health at or under 50 percent | 25 percent only |
| Intimidating Shout | 52 | not given | health at or under 25 percent with enemies in melee | missing for Fury |
| Rallying Cry | 83 | first of wowtbc's defensive list | group takes heavy damage | yes at 20 percent of the bot's own health |
| Vigilance | 75 (talent) | external on a teammate | on cooldown on the best geared damage dealer | yes |

Defensive order of wowtbc (raid, level 90): Rallying Cry, Demoralizing Banner, Vigilance, Shield Wall, Enraged
Regeneration, Defensive Stance. Shield Wall is not a Fury spell and the bots never take Defensive Stance as a damage
dealer; the 15 to 60 defensive set is Bloodthirst's heal, Victory Rush, Enraged Regeneration, Die by the Sword (56),
Intimidating Shout (52).

## Healer triage and mana (healers only)

Not applicable. Rage is the resource. Charge gives 20, Bloodthirst 10, Berserker Rage 10, Enrage 10, the shouts 10 each.
Costs: Raging Blow 10, Hamstring 10, Thunder Clap 20, Wild Strike 30 (free on a Bloodsurge proc), Execute 30, Heroic
Strike 30, Cleave 30, Whirlwind 30. Bloodthirst is the only free button of the early kit, so it never waits for rage.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Recklessness (62), Shattering Throw (74), Colossus Smash (81), Heroic Leap (85) and the banners
  (87) are not available in the 15 to 60 band; the lists above start without them.
- Raging Blow is learned at 30 here, and its two-use limit comes from the "Raging Blow!" aura; Wild Strike is learned at
  18 but Bloodsurge only at 50, so the guide's "Wild Strike to spend Bloodsurge" has no meaning below 50.
- Titan's Grip and Single-Minded Fury are both learned at 38; neither guide mentions either.
- Method's Hit 7.5 and Expertise 7.5 percent, wowtbc's rage thresholds (Heroic Strike and Execute 100 to 110) and the
  26.5 percent off-hand hit rule are level 90 numbers with Unending Rage's 120 rage cap; they do not steer the 15 to 60
  band.
- Bladestorm cancelling (Method's later build) is not something a bot does; Bladestorm runs 6 s.
- Method's Storm Bolt rule (inside the Colossus Smash window) needs the level 90 talent, which the bots do not take.
- Method's glyph default Colossus Smash is a level 81 spell; the glyph works only from there.
- Berserker Stance under heavy damage (wowtbc): the 5.4.8 text gives Berserker Stance rage from damage taken, not
  mitigation, so the advice is about rage income. The bots hold Battle Stance in a group and Berserker Stance when
  solo (`WarriorActions.cpp:85-94`, BT-032), a choice no guide makes; to check live which gives better rage.
- Impending Victory (talent, no learn level) replaces Victory Rush; neither guide picks it.

## Gap list (ordered by value for levels 15 to 60)

Config (can go at once):

1. Glyph order: Bull Rush first (Charge known from level 3), Unending Rage second, Raging Wind (Method's dungeon
   must-have) third, instead of Death From Above first. A level 25 to 84 Fury bot carries a glyph that does nothing.
   `PremadeSpecGlyph.1.1` at `conf/playerbots.conf.dist:2664`: `67482,104136,43399,45794,43395,43432`.
2. Tier 6 talent: Bloodbath instead of Avatar (Method; Avatar is no damage dealer pick in either guide). Level 90 only:
   `PremadeSpecLink.1.1` `001221`.
3. Tier 3 talent: Disrupting Shout instead of Piercing Howl (the Fury Piercing Howl is never cast), together with rotation
   item 6 (`002221`).
4. Tier 4 and tier 1: Dragon Roar stays (Method raid); Bladestorm is Method's dungeon pick, owner decision; Juggernaut or
   Double Time, no change.

Rotation (class fixes, wait for the owner's in-game test):

1. Wild Strike as the filler from level 18 (Bloodthirst on cooldown, rage 30 or more, off-hand weapon): a default entry in
   `FuryWarriorStrategy::getDefaultActions` (`FuryWarriorStrategy.cpp:35-44`, e.g. 5.35, above Sunder Armor). Levels 18
   to 49 have no other filler, so the bot auto-attacks between Bloodthirsts today. Core tier.
2. Execute below Bloodthirst and Raging Blow: the generic edge (`GenericWarriorStrategy.cpp:78`, 40) lowered for Fury (a
   Fury edge under 29). Both guides cast Execute after the Enrage buttons; at 30 rage per Execute the current order
   skips Bloodthirst for the last fifth of every fight.
3. Bloodthirst above Wild Strike and a single Raging Blow use (`FuryWarriorStrategy.cpp:57-59` at 29, Bloodthirst
   default 5.5): a Bloodthirst edge at 30, Raging Blow first only at 2 uses (aura "Raging Blow!" 131116, stack count).
   Bloodthirst critical hits are the main Enrage source, so delaying it lowers Raging Blow uses. The registered but
   unwired `BloodthirstBuffTrigger` ("bloodthirst", `WarriorTriggers.h:110`) tests for a self aura of that name, which 5.4.8
   does not have, so it is true whenever the spell can be cast and may serve as the edge (to check).
4. Heroic Strike dump from 40 to 60 once Wild Strike is a filler (`FuryWarriorStrategy.cpp:64`): at 40 it would eat the
   30 rage the filler needs. From 81 raise it toward 100 and spend it freely inside the Colossus Smash window.
5. Pack order for Fury (levels 26 to 60): Whirlwind before Raging Blow when three or more enemies are near (Meat Cleaver
   from 58), Thunder Clap below Whirlwind or not at all (`GenericWarriorStrategy.cpp:154-156`, `:171-173`); today
   Bloodthirst is starved in packs while Whirlwind is castable.
6. Pummel on any casting enemy in melee range for damage dealers, good tier (the tank already has it:
   `TankWarriorStrategy.cpp:154-156`, `CastPummelOnPackCasterAction`, `WarriorActions.cpp:284-287`); with the tier 3
   talent, Disrupting Shout on a casting pack (`WarriorActions.cpp:254-257`, PvP only today).
7. Battle Shout before the pull (non-combat edge in `GenericWarriorNonCombatStrategy.cpp`, level 42 and up), instead of
   the first combat global cooldown (`FuryWarriorStrategy.cpp:54`).
8. Berserker Rage when Enrage is down (`GenericWarriorStrategy.cpp:71` reads rage only); the crowd control edges stay.
9. Enraged Regeneration at 50 percent for Fury (`FuryWarriorStrategy.cpp:66` at 25) and Intimidating Shout as the 25
   percent panic button (Arms has it, Fury has not).
10. Snare edge for Fury (Hamstring from 36, Piercing Howl if ever taken): the Fury strategy has none; Arms'
    `ArmsWarriorStrategy.cpp:82` is the model.
11. Sunder Armor (`FuryWarriorStrategy.cpp:42`, 5.3) sits above Execute and Heroic Strike: with Wild Strike added it
    should rank below them or be dropped when the group has an armor debuff provider (derived; it only casts in a group
    without a warrior tank, `WarriorActions.cpp:28-52`).
12. Skill tiers: the Fury strategy has no tier gating (`BotSkill` is used only in the tank code, `WarriorActions.cpp:286`,
    `WarriorTriggers.cpp:151`); every bot plays the whole list. The lines marked good and expert above (Pummel on any
    caster, Berserker Rage by Enrage state, Heroic Throw, Commanding Shout, Colossus Smash conditions, Heroic Strike
    thresholds) are the ones a casual bot should drop.
13. Level 81 and above: the Colossus Smash conditions and window of wowtbc (Enrage up, a Raging Blow use, 80 rage;
    Execute and Heroic Strike thresholds of 100 to 110), Dragon Roar only while Colossus Smash is down, Heroic Leap
    inside the window. Expert tier, outside the 15 to 60 band.

## PvP play (player guides, 5.x, read 2026-11-19)

- **Sources:** four player guides were filed for Fury, but only a 5.0.4 duel clip is Fury PvP; two long 5.4.8 guides teach the Arms rotation (Mortal Strike, Overpower, Slam) and are used only for warrior-wide material (talents, glyphs, gems, stat order, macros); one 5.4.8 guide is PvE (Siege of Orgrimmar) and is used for rotation basics only. No coach course. Facts only, nothing copied; data checked against this server's 5.4.8 tables.
- **Coverage:** there is no Fury arena guide in the material. Most lines below are adapted from the Arms plan and checked against Fury's own spells, or GAP.

**What the guides say.** Win by burst on a controlled target: pool rage, pop everything together, spend it in seconds; a duel clip kills in six seconds. Crit keeps Enrage up, and Enrage gives rage and damage; the 5.4.8 Enrage text lists Bloodthirst and Colossus Smash crits for Fury. PvE window (5.4.8): Colossus Smash on cooldown; inside it Storm Bolt, Raging Blow, Bloodthirst, Raging Blow; outside it Bloodthirst on cooldown, Raging Blow when Enraged, Wild Strike only when nothing else is up; Heroic Strike only to avoid capping rage; Whirlwind for Meat Cleaver on groups; Bladestorm outside the window; stay on the target because the auto attack hits hard.

**Arena plan (adapted from the Arms plan).** Stun first so damage cannot be dodged or parried: Charge (stun), then Storm Bolt or Shockwave, Storm Bolt late in the movement; trade Charge and Heroic Leap after the enemy used theirs; Recklessness and Skull Banner together with the rage pooled; Pummel the healer and school-aware casts (Polymorph to deny a Blink, nature school against a shaman); Disarm after a stun on a big physical hitter; Defensive Stance and Demoralizing Banner against stealth openers; Berserker Rage against fear teams. Talent picks by comp (from an Arms guide, warrior wide): Bladestorm plus Bloodbath against casters with partner stuns (and Bladestorm as a "free trinket" in duels: it ignores loss of control); Shockwave plus Storm Bolt when the team needs to control the healer; Double Time because more charges mean more interrupts.

**Talents and glyphs, guides against the set** (`PremadeSpecLinkPvp.1.1 = 110112`, the Arms set; glyphs `67482,43412,43419,104136,43395,45792`):

| Tier | Guides (warrior wide) | Set |
|---|---|---|
| 15 | Double Time | Double Time |
| 30 | Second Wind (Enraged Regeneration is the other pick) | Second Wind |
| 45 | Disrupting Shout (battlegrounds) or Staggering Shout | Staggering Shout |
| 60 | Bladestorm (duels, caster teams), Shockwave (against warrior, enhancement, affliction), Dragon Roar (fun) | Shockwave |
| 75 | Safeguard | Safeguard |
| 90 | Storm Bolt (also Bloodbath, Avatar) | Storm Bolt |

Glyphs: Bull Rush (rage per Charge) everywhere, Death From Above, Blitz for interrupt comps, Rude Interruption against a mage, Colossus Smash against a hunter, Intimidating Shout (minor) with a hunter partner. Stat order (steers rolls only, PvP guide, 5.4.7 values): hit and expertise caps, crit, strength, PvP power, mastery, PvP resilience, haste; PvE: hit and expertise 7.5 percent, crit, mastery, haste. Crit first is common to both.

**Differences from the guides (5.4.8).** Shockwave has a 40 s cooldown, not the 5.0.4 clip's 20 s. Staggering Shout roots only enemies that are already snared, so it needs Hamstring or Piercing Howl first. Berserker Rage (level 54) removes and grants immunity to fear, sap and incapacitate. Charge stuns (or roots with Warbringer) and gives rage; Intervene is replaced by Safeguard. Fury has Die by the Sword from level 56 and Unbridled Wrath (rage when stunned or immobilized, level 56), which no guide mentions. Wild Strike applies Mortal Wounds (a healing reduction). Bloodthirst has double crit chance. Colossus Smash is learned at 81, so Fury has no window before that. Bladestorm is immune to loss of control, so a macro that cancels it before another action matters. Level gates: Bloodthirst 10, Wild Strike 18, Whirlwind 26, Raging Blow 30, Titan's Grip 38, Bloodsurge 50, Berserker Rage 54, Recklessness 62, Skull Banner 87.

**Bots today (read from `src/strategy/Classes/warrior/`, ordered by value).**
1. Storm Bolt and Shockwave are in no Fury or Arms edge (only the Tank strategy casts Storm Bolt); no chain with Charge or Leap.
2. Charge and Heroic Leap fire whenever the enemy is out of melee (`GenericWarriorStrategy.cpp:74-79`), with no regard to the enemy's own mobility.
3. Recklessness is a default action cast as soon as it is ready (`FuryWarriorStrategy.cpp:36`); Skull Banner has its own edge (`GenericWarriorStrategy.cpp:107`); no rage pool.
4. Spell Reflection and Disarm are Tank-only; Safeguard, Staggering Shout and Mass Spell Reflection have no cast (the set holds two of them); Bladestorm only in the area strategy.
5. Berserker Rage has break edges (`GenericWarriorStrategy.cpp:109,113`) but another edge spends it whenever Enrage is missing (`:73`).
6. Pummel on the enemy healer exists (`FuryWarriorStrategy.cpp:70`), behind the interrupt gate; Die by the Sword (`GenericWarriorStrategy.cpp:95`), Enraged Regeneration (`FuryWarriorStrategy.cpp:75-76`) and Intimidating Shout (`:81`) exist and are in the shared catalogs.
7. The Fury rotation edges (Raging Blow charges, Bloodthirst, Wild Strike on Bloodsurge, Heroic Strike at high rage) exist (`FuryWarriorStrategy.cpp:62-72`); Colossus Smash is a generic edge (`GenericWarriorStrategy.cpp:82`).

**Research update (public patch notes 5.1 to 5.4.7 and spell data, 2026-11-19; lines marked derived come from game knowledge and the 5.4.8 data).** In 5.4 Storm Bolt stuns 4 s and adds an off-hand attack for Fury, Bladestorm has a 60 s cooldown (and in this server's text is immune to loss of control and movement impairing effects, with only shouts usable), Hamstring is off the global cooldown, Safeguard (not Intervene) breaks roots and snares, Warbringer roots 4 s, Enraged Regeneration heals 10 percent at once plus 10 percent over 5 s, Disrupting Shout shares a 15 s cooldown with Pummel, Intimidating Shout is 90 s (5.1), Avatar breaks movement impairing effects (5.1). Data: Recklessness 3 min, 12 s, +30 percent crit; Storm Bolt 30 s, 30 yd; Spell Reflection 25 s, 5 s, no shield requirement shown. Derived: two of the three stuns (Charge, Storm Bolt, Shockwave) go on two targets, not three on one, because they share one category; Shield Wall is not available to a Titan's Grip warrior; the go is a controlled healer plus a stun on the target, then Recklessness with Skull Banner, Avatar, Colossus Smash, Raging Blow, Bloodthirst. Extra gap lines: Recklessness is a default (`FuryWarriorStrategy.cpp:36`) cast at once; the shared catalog lists Shield Wall for all warriors (`Utils/PvpSpellCatalog.h:169`), so the defensive picker may try it on Fury (to check).

**Proposals.** Config (no code): none is supported by Fury-specific evidence; the Fury set is a copy of Arms, with Staggering Shout and Safeguard idle. Rotation (waits for the owner's in-game test): Charge or Leap, then Storm Bolt or Shockwave; trade Charge after the enemy's; Recklessness plus Skull Banner with the rage pooled; Spell Reflection and Safeguard as reactions; Staggering Shout only after a snare.

**Open questions.** (1) May the Fury set become its own set once a Fury PvP guide exists (more PvP guides are being collected). (2) Titan's Grip or Single-Minded Fury for PvP. (3) Bladestorm as a stun break.
