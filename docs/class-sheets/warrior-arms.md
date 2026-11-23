# Warrior: Arms (damage)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). Gap item 10 (Colossus
  Smash window: Heroic Strike pooled to 100 rage outside it and from 55 inside, Slam before Overpower inside it) is done
  for expert bots from 81; the Execute thresholds of the level 90 list are left for later. The "bots today" columns
  below describe the code before the 2026-11-11 implementation.
- **Sources:**
  - Method, "Arms Warrior Class Guide - MoP Classic" (Phase 1, level 90 raid and dungeon, 22 July 2025), read
    2026-11-11.
  - wowtbc.gg, "Arms Warrior" in the MoP class guides (MoP Classic, no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, Talent, GlyphProperties, SpellCooldowns,
    SpellPower, SpellEffect, SpellShapeshift, SpellEquippedItems; `data/db2/Item-sparse.db2` for glyph item ids)
    through `spec_data.py` and short read-only scripts.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch, checked 2026-11-10, not retried); Wowhead needs a
    browser.
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer takes the tank's marks and
  follows the leader), 61 to 90 briefly from the guides.

Which source was richer: **talents, glyphs, stat order** Method (it names a default and a situational list, and says
dungeon or raid; wowtbc lists every option without a pick and has no stat order); **opener, single target, cooldown
timing, area (2 and 3 or more targets), defensives** wowtbc (rage thresholds, the Colossus Smash window, Overpower
stacks, an ordered defensive list); **Execute phase, area priority as one list** Method; **levels 15 to 60, what to do
before a spell is learned, dungeon pulls, interrupts** neither guide says anything (both are level 90 raid guides), so
those lines are derived from the 5.4.8 data and marked "derived".

## Talents

Tiers and columns are from Talent.dbc. "Bots today" is `AiPlayerbot.PremadeSpecLink.1.0 = 001021`
(`conf/playerbots.conf.dist:2658`; one digit per tier, column counted from 0): Juggernaut, Enraged Regeneration,
Piercing Howl, Bladestorm, Vigilance, Bloodbath.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (001021) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Double Time (Method) | Juggernaut when the second charge is not needed (Method) | 103827 / 103826 | Juggernaut, Method's alternative. No change: a bot charges about once per pull, so the 12 s cooldown is worth as much as a second charge |
| 2 (30) | Enraged Regeneration (Method names Second Wind for long stretches under 35 percent health) | Second Wind; Impending Victory is named by wowtbc only as an option | 55694 / 29838 | Enraged Regeneration, same |
| 3 (45) | Disrupting Shout (Method default) | Piercing Howl for crowd control (Method); Staggering Shout (wowtbc option) | 102060 / 12323 | **Piercing Howl, differs.** It does something today (the Arms snare edge, `ArmsWarriorStrategy.cpp:82`); Disrupting Shout does nothing until the pack interrupt line exists (rotation item 2 of the gap list) |
| 4 (60) | Dragon Roar for single target (Method), Bladestorm for packs and dungeons (Method dungeon note); wowtbc's own priority casts Bladestorm after Mortal Strike | Shockwave is an option in wowtbc only | 118000 / 46924 | Bladestorm, same as the dungeon pick |
| 5 (75) | Vigilance | Mass Spell Reflection "niche" (Method); Safeguard (wowtbc option) | 114030 | Vigilance, same (`WarriorTriggers.cpp:14`, `WarriorActions.cpp:104`) |
| 6 (90) | Bloodbath | Storm Bolt only for stun-immune targets (Method, Fury page); Avatar is not named for a damage dealer | 12292 | Bloodbath, same |

Notes on the picks (data, not guide):

- Disrupting Shout has a 40 s cooldown and shares a 15 s cooldown with Pummel (spell text), so it is an area kick, not
  a second single kick.
- Bladestorm (60 s): "you can only perform shout abilities" while it runs (spell text). A bot cannot cancel it the way
  wowtbc's opener does ("cancel when Mortal Strike is ready"), so for six seconds it uses no other button. Still the
  better of the two for a dungeon pack, and Method's own dungeon pick.
- Dragon Roar (60 s): always a critical hit, ignores armor, and "knocks back and knocks down" every enemy in range
  (spell text). In a dungeon pack that scatters the mobs away from the tank (derived), so it is the boss-fight pick.
- Vigilance has a 120 s cooldown and reduces a party member's damage taken; the bot casts it on the highest gear score
  damage dealer in range (`WarriorActions.cpp:104-145`).
- Charge (20 s, 12 s with Juggernaut) gives rage and, with Warbringer, roots instead of stunning; nobody names Warbringer.

Proposed link, guide dungeon line: `102021` (Double Time / Enraged Regeneration / Disrupting Shout / Bladestorm /
Vigilance / Bloodbath). Proposed for the bots now: keep `001021` until the pack interrupt line exists, then `002021`.
For a character below 75 only the first four digits are in effect.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and
75 (config comment; the unlock levels are core-side, not in the data files). The config order is major, minor, major,
minor, minor, major (`conf/playerbots.conf.dist:2634`); a glyph goes to the first free slot of its type.

| Type | Glyph | Item id | Why (one clause) | Bots today (`45792,104136,43399,45794,43395,67482`) |
|------|-------|--------:|------------------|------------------------|
| Major | Unending Rage | 43399 | maximum rage 100 to 120, so Heroic Strike and Slam are not wasted at the cap (Method) | yes (slot 2, level 50) |
| Major | Bull Rush | 67482 | Charge gives 15 more rage; Charge is known from level 3 (Method) | yes, but in the third slot (level 75) |
| Major | Colossus Smash (Method default) | 63481 | the Smash also applies the Sunder Armor effect, for a group with no other armor debuff; spell learned at 81 | no |
| Major, situational | Sweeping Strikes | 43428 | rage on every Sweeping Strikes hit (learned at 60); Method and wowtbc for packs | no |
| Major, situational | Death From Above | 45792 | Heroic Leap cooldown 15 s shorter; Heroic Leap is learned at 85 (wowtbc standard) | **yes, in the first slot (level 25), where it does nothing until 85** |
| Major, situational | Rude Interruption | 43417 | Pummel interrupt gives a damage buff, for kick-heavy fights | no |
| Major, situational | Die by the Sword | 43422 | Overpower and Wild Strike lengthen Die by the Sword (Method, defensive) | no |
| Major, situational | Enraged Speed, Resonating Power | 43413, 43430 | mobility; Thunder Clap damage and cooldown up 50 percent (wowtbc area) | no |
| Minor | Subtle Defender (Method: the only minor worth a slot) | 104136 | removes the threat bonus of Defensive Stance, which a damage bot never uses, so it is a free pick | yes |
| Minor | Intimidating Shout, Mystic Shout | 45794, 43395 | no damage effect (targets tremble in place instead of fleeing; you hover); wowtbc lists Intimidating Shout | yes (harmless) |

Disagreement: wowtbc's standard set is Death From Above, Unending Rage, Bull Rush; Method's is Unending Rage, Colossus
Smash, Bull Rush. Method is used for the picks (it separates raid and dungeon), and the slot order is derived: a glyph in
a slot must be useful at the level the slot unlocks. Level 25 to 49 has one major slot, so it takes Bull Rush (Charge
known since 3); Unending Rage follows at 50; the third slot at 75 takes the Method default (Colossus Smash, useful from 81)
or Sweeping Strikes (useful from 75), an owner choice. Proposed `PremadeSpecGlyph.1.0`:
`67482,104136,43399,45794,43395,63481`.

## Stat order

Method: Hit and Expertise to about 7.5 percent each, Strength, Critical Strike, Mastery, Haste (Mastery and Haste about
equal). wowtbc has no stat order. The caps are level 90 numbers and cannot be reached with the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- A **two-handed weapon** first: Seasoned Soldier (level 10, spell text) gives +25 percent physical damage and takes 10
  rage off Whirlwind and Thunder Clap only while a two-handed melee weapon is held (SpellEquippedItems: two-handed
  axe, mace, polearm, sword). A one-handed weapon is a large downgrade for an Arms bot.
- Then plate (Plate Specialization, level 50, gives Strength only in all-plate), Strength, Critical Strike, Mastery
  (Strikes of Opportunity from level 80), Haste last; Hit and Expertise matter only near level 90.
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:451-463`): Strength 2.5, Hit 2.0, Critical 1.9, Mastery 1.75,
  Expertise 1.4, Haste 0.8, a two-handed weapon wanted and a one-handed one cut to a tenth (`:832-843`). That order
  agrees with Method (Strength first, Haste last). The armor penetration weight (1.7) belongs to a stat the Cataclysm
  changes removed (inference, to check), harmless.

## What the specialisation knows, by level (5.4.8 data)

Ids are the spells the player actually casts; the data holds several old rows of the same name. Rage costs are the
SpellPower values divided by 10.

| Level | Spell (id) |
|------:|------------|
| 1 | Heroic Strike (78, 30 rage, 1.5 s cooldown, no global cooldown), Battle Stance (2457) |
| 3 | Charge (100, 8 to 25 yd, 20 s, gives rage) |
| 5 | Victory Rush (34428, free, after a kill) |
| 7 | Execute (5308, 30 rage, target under 20 percent) |
| 10 | Mortal Strike (12294, free, 6 s, gives 10 rage); Seasoned Soldier (12712, passive); the specialisation is chosen here |
| 14 | Enrage (13046, passive: critical hits of Mortal Strike, Bloodthirst, Colossus Smash, Devastate and Shield Slam enrage you) |
| 16 | Sunder Armor (7386, 15 rage) |
| 18 | Slam (1464, 25 rage) |
| 20 | Thunder Clap (6343, 20 rage, 10 with a two-hander, 6 s) |
| 22 | Heroic Throw (57755, 30 s, ranged) |
| 24 | Pummel (6552, 15 s) |
| 26 | Whirlwind (1680, 30 rage, 20 with a two-hander) |
| 30 | Overpower (7384, 10 rage, no cooldown); Taste for Blood (56636, passive); tier 2 talent |
| 32 | Deep Wounds (115768, passive; Mortal Strike, Bloodthirst and Devastate leave a bleed) |
| 34 | Berserker Stance (2458) |
| 36 | Hamstring (1715, 10 rage) |
| 42 | Battle Shout (6673, 60 s, gives rage) |
| 44 | Cleave (845, 30 rage) |
| 45 | tier 3 talent |
| 46 | Blood and Thunder (84615, passive: Thunder Clap applies Deep Wounds) |
| 50 | Plate Specialization (86101, passive) |
| 52 | Intimidating Shout (5246, 90 s) |
| 54 | Berserker Rage (18499, 30 s: Enrage, +10 rage, breaks fear, sap and incapacitate) |
| 56 | Die by the Sword (118038, 120 s: +100 percent parry, 20 percent less damage); Unbridled Wrath (143268, passive) |
| 60 | Sweeping Strikes (12328, 30 rage, 10 s: hits one more enemy for 75 percent); tier 4 talent |
| 62 | Recklessness (1719, 180 s: +30 percent critical chance on special attacks) |
| 66 | Spell Reflection (23920) |
| 68 | Commanding Shout (469, gives rage) |
| 72 | Intervene (3411) |
| 74 | Shattering Throw (64382, 300 s: armor down 20 percent or breaks an immunity) |
| 75 | tier 5 talent |
| 80 | Mastery: Strikes of Opportunity (76838) |
| 81 | Colossus Smash (86346, 20 s: ignores armor, +armor weakening, Physical Vulnerability); Sudden Death (29725, passive) |
| 83 | Rallying Cry (97462) |
| 85 | Heroic Leap (6544, 45 s) |
| 87 | Skull Banner (114207, 180 s), Demoralizing Banner (114203), Mocking Banner (114192) |
| 90 | tier 6 talent |

Facts the priority lists rest on (spell texts and tables of the data):

- No ability of this list needs a stance (SpellShapeshift is empty for them), so there is no stance dance. Battle
  Stance "generates high rage from normal melee attacks"; Berserker Stance "some rage from melee and some from damage
  taken" (level 34). Neither changes damage in the spell text.
- Mortal Strike gives 10 rage and applies Mortal Wounds; a critical hit enrages. Taste for Blood: each Mortal Strike hit
  grants 2 uses of Overpower and a target dodge grants 1; they stack to 5 (aura 60503).
- Execute costs 30 rage and is "only usable on enemies below 20 percent health"; it is a fixed hit, not a rage dump,
  in this data. Sudden Death (81) only resets the Colossus Smash cooldown and makes an Overpower free after an Execute
  hit.
- Heroic Strike and Cleave are "use when you have more rage than you can spend" (spell text).
- Whirlwind is in the Arms specialisation list (spec 71, level 26) as well as Fury's; neither guide lists it for Arms.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked
"derived" comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to
`mod-aware-bots-548/src/` at commit `605b17f`.

How the bots rank what they do (read, `strategy/Strategy.h:37-51`): ACTION_DEFAULT 5, ACTION_HIGH 20, ACTION_MOVE 30,
ACTION_INTERRUPT 40, ACTION_EMERGENCY 90. A trigger edge adds its action with that number; the default actions of
`ArmsWarriorStrategy::getDefaultActions` (`ArmsWarriorStrategy.cpp:47-54`) are 5.2 down to 5.0. The engine takes the
highest action that is possible and useful. The Arms strategy runs for an Arms-specialised warrior and for every warrior
below level 10 (`Factory/AiFactory.cpp:378-381`; a level 10 and up warrior without a specialisation gets the Fury
one); both damage specialisations also get the `aoe` and `dps assist` strategies.

Two facts from the code that shape the list: the "mortal strike" trigger is a debuff trigger that also needs the target
to live at least 8 seconds at the group's estimated damage (`GenericTriggers.cpp:322-330`), and Mortal Strike leaves no
aura of that name (it applies Mortal Wounds), so on a longer fight the edge is "always" and on a short one only the
default entry at 5.1 casts it (read; effect to check in an action log).

### Levels 15 to 29

Known: Heroic Strike, Charge, Victory Rush, Execute, Mortal Strike, Seasoned Soldier, Sunder Armor (16), Slam (18),
Thunder Clap (20), Heroic Throw (22), Pummel (24), Whirlwind (26).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | target out of melee, 8 to 25 yd | Charge (100, 3): opener and gap closer, +20 rage | yes: "enemy out of melee" edge (`GenericWarriorStrategy.cpp:72`), chain charge, heroic leap, reach melee (`:39-40`, `:76-77`) |
| 2 | core | Mortal Strike ready | Mortal Strike (12294, 10), on cooldown | yes (`ArmsWarriorStrategy.cpp:64` at 23, default `:51`) |
| 3 | core | target under 20 percent, rage 30 or more | Execute (5308, 7) | **wrong order:** fires first, at 40, above Mortal Strike (`GenericWarriorStrategy.cpp:78`, trigger `HealthTriggers.h:135-139`); both guides cast Mortal Strike before Execute, and Mortal Strike is free and gives rage while Execute costs 30 |
| 4 | core | a kill just happened | Victory Rush (34428, 5), free heal | yes at 40 (`GenericWarriorStrategy.cpp:79`) |
| 5 | core | rage 60 or more | Heroic Strike (78, 1) as the rage dump | yes (`ArmsWarriorStrategy.cpp:77-80`) |
| 6 | good | from 18, nothing else ready, rage 25 or more (derived: both guides end the list with Slam, and a bot that waits for 60 rage leaves the global cooldown empty) | Slam (1464, 18) | **partly:** only at rage 60 or more, above Heroic Strike (`ArmsWarriorStrategy.cpp:77-79`); between 25 and 59 rage the bot auto-attacks |
| 7 | good | from 24: an enemy casts | Pummel (6552, 24) | partly: the current target only (`GenericWarriorStrategy.cpp:96`), any other caster in PvP only (`:99-102`) |
| 8 | good | two or more enemies within 8 yd, from 20 | Thunder Clap (6343, 20; 10 rage with a two-hander) | yes at 25 (`GenericWarriorStrategy.cpp:154`, `:171`; `WarriorActions.cpp:289`) |
| 9 | good | two or more enemies within 8 yd, from 26 | Whirlwind (1680, 26; 20 rage with a two-hander) | yes at 24, **above Mortal Strike (23)** (`GenericWarriorStrategy.cpp:156`, `:173`); BT-033 and the comment at `:155` say "Fury only", which the data contradicts (spec 71 learns it at 26) |
| 10 | good | a long fight where the group has no armor debuff (derived: Weakened Armor stacks to 3; Colossus Smash only brings it from 81) | Sunder Armor (7386, 16) | yes, lowest default at 5.05; group only, and not when a warrior tank is alive (`WarriorActions.cpp:28-52`, `ArmsWarriorStrategy.cpp:52`) |
| 11 | expert | target out of melee, Charge on cooldown, from 22 | Heroic Throw (57755, 22) as a ranged filler (Method Arms #11) | missing for damage dealers (only the tank has it, `TankWarriorStrategy.cpp:116`) |

Before a spell is learned (derived): no Slam before 18 (Mortal Strike, Heroic Strike and Execute are the kit); no
Thunder Clap before 20; Whirlwind and Pummel wait for 26 and 24. The bots tolerate all of these because an unknown spell
is simply not cast.

### Levels 30 to 44

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 12 | core | Taste for Blood is up (Mortal Strike hit or a dodge) | Overpower (7384, 30), 10 rage, cannot be dodged | yes at 24 (`ArmsWarriorStrategy.cpp:72-73`), **above Mortal Strike (23)**; Method and wowtbc both put Mortal Strike first |
| 13 | good | from 36: an enemy runs from the group or chases a member who is not the tank | Hamstring (1715, 36) | yes, through the snare edge, Hamstring as the alternative to Piercing Howl (`ArmsWarriorStrategy.cpp:82`, `:32`; target pick `SnareTargetValue.cpp:15-55`) |
| 14 | good | from 42: the shout is missing | Battle Shout (6673, 42), before the pull | partly: combat only, at 29 (`ArmsWarriorStrategy.cpp:61`); the non-combat strategy has no shout line (`GenericWarriorNonCombatStrategy.cpp:20-33`), so the first combat tick spends the global cooldown on it |
| 15 | good | two or more enemies within 8 yd, from 44 | Cleave (845, 44) instead of Heroic Strike | yes (`GenericWarriorStrategy.cpp:157`, `:174`) |
| 16 | good | an enemy cast at the group, tier 3 talent (45) | Disrupting Shout (102060) on a casting pack | **gap:** PvP maps only (`GenericWarriorStrategy.cpp:99-102`, `WarriorActions.cpp:254-257`) |

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 17 | core | two or more enemies | Thunder Clap on cooldown also refreshes Deep Wounds (Blood and Thunder, 46); wowtbc: Deep Wounds with Thunder Clap on all targets | yes (as line 8) |
| 18 | good | Enrage is down | Berserker Rage (18499, 54), +10 rage; also breaks fear, sap, incapacitate | partly: cast on cooldown whenever rage is 20 or more, Enrage state not read (`GenericWarriorStrategy.cpp:71`, priority 40); crowd control edges `:107`, `:111` |
| 19 | good | health at or under 40 percent | Die by the Sword (118038, 56) | yes (`GenericWarriorStrategy.cpp:93`; the node falls back to Enraged Regeneration, `:54`) |
| 20 | good | health at or under 50 percent (derived, as for the tank sheet) | Enraged Regeneration (55694, talent tier 2) | partly: Arms at 65 percent (the "medium health" edge, `ArmsWarriorStrategy.cpp:86`), Fury at 25 (`FuryWarriorStrategy.cpp:66`); early for Arms, late for Fury |
| 21 | good | health at or under 25 percent, from 52 | Intimidating Shout (5246, 52) as a panic button | yes, Arms only (`ArmsWarriorStrategy.cpp:84`) |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 22 | core | two or more enemies, keep it up | Sweeping Strikes (12328, 60), 30 rage | yes at 27 (`GenericWarriorStrategy.cpp:153`, `:170`; `ArmsWarriorStrategy.cpp:30`) |
| 23 | good | tier 4 talent at 60: a pack of three or more | Bladestorm (46924), or Dragon Roar (118000) on a boss | yes, Bladestorm as the first default (`ArmsWarriorStrategy.cpp:48`) and at 25 in packs (`GenericWarriorStrategy.cpp:149`) |
| 24 | good | on cooldown in a fight that matters, from 62 | Recklessness (1719, 62) | yes but gated: the boost trigger fires only when the group's level sum is at most half the enemies' (elites count three times, bosses twenty; `WarriorTriggers.h:104`, `GenericTriggers.cpp:416-424`, `AttackerCountValues.cpp:55-109`), so ordinary open-world mobs never get it (`ArmsWarriorStrategy.cpp:69`) |
| 25 | expert | from 68: both shouts, for rage (both guides: "shouts generate rage") | Commanding Shout (469, 68) beside Battle Shout | missing for damage dealers (only the tank casts it, `TankWarriorStrategy.cpp:75`); BT-036 keeps shouts as upkeep only |
| 26 | expert | from 74: single target, armor debuff in the opener (Method, wowtbc) | Shattering Throw (64382, 74) | partly: only against an immunity (`ArmsWarriorStrategy.cpp:88`, `WarriorActions.cpp:158-212`) |

### Levels 75 to 89

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 27 | good | tier 5 talent at 75: an ally who draws attacks | Vigilance (114030) | yes (`GenericWarriorNonCombatStrategy.cpp:27`, picks the best geared damage dealer) |
| 28 | core | from 81: Colossus Smash not on the target | Colossus Smash (86346, 81), then Mortal Strike, Execute, Slam, Overpower inside the 6 s window (wowtbc: 4 casts, 5 with 3 Overpower) | partly: cast whenever the debuff is missing at 24 (`GenericWarriorStrategy.cpp:80`, trigger `WarriorTriggers.h:43-47`); the window logic (Slam beats Overpower inside it, Berserker Rage and Heroic Leap inside it) is missing |
| 29 | good | from 83 and 87 | Rallying Cry (97462), Skull Banner (114207), Demoralizing Banner (114203) | yes (`GenericWarriorStrategy.cpp:89-92`, `:105`; banners in the aoe lists) |
| 30 | good | from 85, window or gap | Heroic Leap (6544, 85) | gap closer only (`GenericWarriorStrategy.cpp:76`) |

### Level 90

Guide, single target (above 20 percent health; Method's list with wowtbc's conditions): Dragon Roar (outside the
Colossus Smash window), Bloodbath, Skull Banner, Recklessness, Shattering Throw if assigned, Colossus Smash when the
debuff is off, Berserker Rage while Enrage is down, Mortal Strike, Overpower (first at 4 or more stacks, wowtbc),
Slam (inside the window or at 100 rage and more), Overpower, Slam, Battle Shout; Execute at 100 rage and more outside the
window and freely inside it when the target is under 20 percent; Heroic Strike from 115 rage (wowtbc). Opener: Battle
Shout, Shattering Throw, Charge, Mortal Strike, cooldowns, Colossus Smash, Bladestorm (wowtbc).

Bots: Bloodbath, Recklessness (gated), Skull Banner, Dragon Roar, Bladestorm, Colossus Smash, Mortal Strike, Overpower,
Slam and Heroic Strike (at 60 rage), Execute are all there; the order and the rage thresholds differ (Execute above
Mortal Strike, Overpower above Mortal Strike, Slam and Heroic Strike at 60 instead of 100 to 115, Colossus Smash with no
window logic). Area: Sweeping Strikes, Thunder Clap, Bladestorm are in the aoe lists; Whirlwind is added by the bots.

## Cooldowns and defensives

Press timing (the guides say "on cooldown"; the thresholds are derived because dungeon trash needs numbers):

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Berserker Rage | 54 | on cooldown when Enrage is down | when Enrage is down and rage is 20 or more | cast at 20 rage whenever ready, Enrage not read |
| Sweeping Strikes | 60 | keep up with 2+ targets | when two or more enemies are within 8 yd | yes |
| Recklessness | 62 | on cooldown | at the start of a pack of elites or a boss | yes, balance gated |
| Bladestorm / Dragon Roar | 60 | on cooldown | Bladestorm at a pack of 3 or more, Dragon Roar on a boss | Bladestorm first default; Dragon Roar default |
| Colossus Smash | 81 | on cooldown when not on the target | same | yes |
| Skull Banner | 87 | on cooldown | on cooldown in a pack or boss | yes |
| Die by the Sword | 56 | defensive | health at or under 40 percent | yes |
| Enraged Regeneration | 30 (talent) | defensive | health at or under 50 percent | 65 percent for Arms (`ArmsWarriorStrategy.cpp:86`) |
| Intimidating Shout | 52 | not given | health at or under 25 percent with enemies in melee | yes (`ArmsWarriorStrategy.cpp:84`) |
| Rallying Cry | 83 | first of wowtbc's defensive list | group takes heavy damage | yes at 20 percent of the bot's own health |
| Vigilance | 75 (talent) | external on a teammate | on cooldown on the best geared damage dealer | yes |

Defensive order of wowtbc (raid, level 90): Rallying Cry, Demoralizing Banner, Vigilance, Shield Wall, Enraged
Regeneration, Defensive Stance. Shield Wall is not an Arms spell and the bots never take Defensive Stance as a damage
dealer; the 15 to 60 defensive set is Victory Rush, Enraged Regeneration, Die by the Sword (56), Intimidating Shout (52).

## Healer triage and mana (healers only)

Not applicable. Rage is the resource. Charge gives 20, Mortal Strike 10, Berserker Rage 10, the shouts 10 each; Battle
Stance gives high rage from melee hits. Rage costs: Slam 25, Execute 30, Heroic Strike 30, Cleave 30, Whirlwind 30
(20 with a two-hander), Thunder Clap 20 (10), Sweeping Strikes 30, Overpower 10. The only free damage buttons are Mortal
Strike and Colossus Smash, so they never wait for rage.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Recklessness (62), Shattering Throw (74), Colossus Smash and Sudden Death (81), Heroic Leap
  (85) and the banners (87) are not available in the 15 to 60 band; the lists above start without them.
- Bladestorm cancelling (wowtbc's opener and single target list) is not something a bot does; Bladestorm runs 6 s.
- Sudden Death: wowtbc says to Execute "to keep Sudden Death". In this data Sudden Death resets the Colossus Smash
  cooldown and makes an Overpower free after an Execute hit; it does not enable Execute (the bots' comment at
  `ArmsWarriorStrategy.cpp:90-93` says the same).
- Whirlwind: neither guide lists it for Arms; the 5.4.8 specialisation list gives it to Arms at 26. Method's area list
  uses Thunder Clap and Sweeping Strikes. The bots cast it for Arms (BT-033 assumed they did not).
- Method's Hit 7.5 and Expertise 7.5 percent and wowtbc's rage thresholds (Execute 80 or 100, Heroic Strike 115) are level
  90 numbers with Unending Rage's 120 rage cap and Colossus Smash windows; they do not steer the 15 to 60 band.
- Impending Victory (talent, no learn level) replaces Victory Rush; the data row says so. Neither guide picks it.
- Method's glyph default Colossus Smash is a level 81 spell; the glyph works only from there.
- Berserker Stance under heavy damage (wowtbc): the 5.4.8 text gives Berserker Stance rage from damage taken, not
  mitigation, so the advice is about rage income. The bots hold Battle Stance for Arms (`WarriorActions.cpp:85-89`).

## Gap list (ordered by value for levels 15 to 60)

Config (can go at once):

1. Glyph order: Bull Rush first (Charge known from level 3), Unending Rage second, the Method default Colossus Smash (or
   Sweeping Strikes) third, instead of Death From Above first. A level 25 to 84 Arms bot carries a glyph that does
   nothing. `PremadeSpecGlyph.1.0` at `conf/playerbots.conf.dist:2659`: `67482,104136,43399,45794,43395,63481`.
2. Tier 3 talent: Disrupting Shout instead of Piercing Howl, only together with rotation item 2 (`PremadeSpecLink.1.0`
   `002021`, `conf/playerbots.conf.dist:2658`). Without the interrupt line the Arms snare edge still uses Piercing Howl.
3. Tier 1: Double Time (Method default) or keep Juggernaut: no change, low value. Tiers 2, 4, 5, 6 already match.

Rotation (class fixes, wait for the owner's in-game test; file and function each would touch):

1. Execute below Mortal Strike (core, all levels from 7): the generic Execute edge
   (`GenericWarriorStrategy::InitTriggers`, `GenericWarriorStrategy.cpp:78`, 40) lowered for Arms by a per-spec edge in
   `ArmsWarriorStrategy::InitTriggers` (near `:64`, e.g. 22, under Mortal Strike's 23). Both guides cast Mortal Strike
   first; with 30 rage per Execute and a six second Mortal Strike the current order starves the free button during the
   last fifth of every fight.
2. Pummel on any casting enemy in melee range for damage dealers (good): the tank already has it
   (`TankWarriorStrategy.cpp:154-156`, `CastPummelOnPackCasterAction`, `WarriorActions.cpp:284-287`); add the same edge in
   `ArmsWarriorStrategy::InitTriggers`. With the tier 3 talent, Disrupting Shout on a casting pack
   (`CastDisruptingShoutOnEnemyCasterAction::isUseful`, `WarriorActions.cpp:254-257`, PvP only today). A dungeon pack
   caster is the one thing a damage dealer can add that the tank cannot.
3. Slam as the filler from level 18 (good): a "medium rage available" (40) edge for Slam under Mortal Strike and
   Overpower in `ArmsWarriorStrategy::InitTriggers` (`:77-80` keeps the 60 rage Heroic Strike dump); the bot idles on the
   global cooldown between 25 and 59 rage today.
4. Whirlwind under Mortal Strike for Arms: `GenericWarriorStrategy.cpp:156` and `:173` (24 against Mortal Strike's 23), a
   per-spec number or an Arms check in `CastWhirlWindAction` (`WarriorActions.h:130`); and correct the "Fury only"
   comments (BT-033, `GenericWarriorStrategy.cpp:155`, `:172`).
5. Mortal Strike above Overpower (`ArmsWarriorStrategy.cpp:72-73`, 24 to 22): both guides; low impact because Taste for
   Blood lasts 12 s.
6. Battle Shout before the pull (good, level 42 and up): a non-combat edge in `GenericWarriorNonCombatStrategy::InitTriggers`
   (`GenericWarriorNonCombatStrategy.cpp:20-33`), instead of the first combat global cooldown
   (`ArmsWarriorStrategy.cpp:61`).
7. Berserker Rage when Enrage is down (good): the "light rage available" edge (`GenericWarriorStrategy.cpp:71`) reads rage
   only; add the Enrage aura (12880) to the action or the edge; the crowd control edges stay.
8. Enraged Regeneration at 50 percent for Arms (`ArmsWarriorStrategy.cpp:86` at 65, "medium health") and Fury alike
   (`FuryWarriorStrategy.cpp:66` at 25): reuse the tank sheet's good-tier edge (`TankEnragedRegenerationHealthTrigger`,
   `WarriorTriggers.cpp:149-155`).
9. Recklessness gate (`ArmsWarriorStrategy.cpp:69`, `WarriorTriggers.h:104`): on an ordinary pull it never fires; the
   gate keeps it for elite packs and bosses, which is a sound dungeon rule. Owner decision whether expert bots drop the
   gate.
10. Level 81 and above (expert, outside the 15 to 60 band): Colossus Smash window rules (Slam over Overpower, Berserker
    Rage and Heroic Leap inside it, Heroic Strike freely), Heroic Strike and Execute thresholds toward 100
    (`ArmsWarriorStrategy.cpp:77-80`, `GenericWarriorStrategy.cpp:80`).
11. Skill tiers: the Arms strategy has no tier gating (`BotSkill` is used only in the tank code,
    `WarriorActions.cpp:286`, `WarriorTriggers.cpp:151`); every bot plays the whole list. The lines marked good and
    expert above (Pummel on any caster, Berserker Rage by Enrage state, Shattering Throw, Heroic Throw, rage thresholds)
    are the ones a casual bot should drop.
12. Commanding Shout (68) and Heroic Throw (22) for damage dealers (expert, low): `ArmsWarriorStrategy::InitTriggers`.

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 8 players of this specialisation (rating 2443 to 2834). 2v2: 8 players (rating 2666 to 2858). Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** 3v3 (8 players)
- **BT-324 result:** updated (BT-450); 2v2 differs on tier 30 (BT-454)
- **Set now:** `PremadeSpecLinkPvp.1.0 = 110112`, `PremadeSpecGlyphPvp.1.0 = 67482,43412,43421,104136,43395,45792` (was `67482,43412,43419,104136,43395,45792`)

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Double Time 100% / 100% / 16 of 16 | Double Time | Double Time | matches |
| 2 (30) | Enraged Regeneration 50% / 62% / 9 of 16, Second Wind 50% / 38% / 7 of 16 | Second Wind | Second Wind | tie, kept; 2v2 prefers Enraged Regeneration (5 of 8) |
| 3 (45) | Staggering Shout 75% / 88% / 13 of 16, Disrupting Shout 25% / 12% / 3 of 16 | Staggering Shout | Staggering Shout | matches |
| 4 (60) | Shockwave 75% / 75% / 12 of 16, Bladestorm 25% / 25% / 4 of 16 | Shockwave | Shockwave | matches |
| 5 (75) | Safeguard 100% / 100% / 16 of 16 | Safeguard | Safeguard | matches |
| 6 (90) | Storm Bolt 75% / 88% / 13 of 16, Avatar 25% / 12% / 3 of 16 | Storm Bolt | Storm Bolt | matches |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Bull Rush (67482) | 100% | 100% | 16 of 16 | yes | yes |
| major | Death From Above (45792) | 75% | 100% | 14 of 16 | yes | yes |
| major | Mortal Strike (43421) | 62% | 50% | 9 of 16 | no | yes |
| major | Colossus Smash (63481) | 50% | 25% | 6 of 16 | no | no |
| major | Blitz (43419) | 12% | 25% | 3 of 16 | yes | no |
| minor | the Blazing Trail (85221) | 50% | 50% | 8 of 16 | no | no |
| minor | Thunder Strike (49084) | 38% | 38% | 6 of 16 | no | no |
| minor | Burning Anger (80588) | 50% | 25% | 6 of 16 | no | no |
| minor | Gushing Wound (43398) | 38% | 25% | 5 of 16 | no | no |
| minor | Intimidating Shout (45794) | 12% | 38% | 4 of 16 | no | no |
| minor | Bloody Healing (43412) | 25% | 12% | 3 of 16 | yes | yes |
| minor | Mystic Shout (43395) | 0% | 0% | 0 of 16 | yes | yes |
| minor | the Subtle Defender (104136) | 0% | 0% | 0 of 16 | yes | yes |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **What changed (3v3, 8 players):** the major glyph slot that held Blitz (12 percent) now holds Mortal Strike (62 percent; on 5.4.8 it raises the healing the warrior receives while Mortal Strike is on the target). The tiers already matched.
- **2v2 disagrees (BT-454):** tier 30 is split in 3v3 (Second Wind 50, Enraged Regeneration 50) and leans to Enraged Regeneration in 2v2 (62 percent, pooled 9 of 16 against 7 of 16): Second Wind stays (3v3 pick, one of the top two); a duel-minded set would take Enraged Regeneration. Mortal Strike is 50 percent in 2v2 (pooled 56 percent, still the third major). Minor slots: no majority in either bracket (Blazing Trail 50 pooled).
- **Fury and protection** share this set or follow it (BT-324 stays open for them: no ladder player in either).

## PvP class package: bots today (BT-673 to BT-676, 2026-11-19)

Status of the digest's section 12 rules after the Arms package (see `docs/pvp/class-packages.md`; all wait for the owner's in-game test):

| Digest rule | Bots today |
|---|---|
| 1 rotation, Slam in the window | as before (partly: nodes and the Colossus Smash window edge) |
| 2 Recklessness and Skull Banner with 80 rage | done for a player target (BT-675); PvE unchanged |
| 3 Thunder Clap to spread Deep Wounds | not built |
| 4 Storm Bolt or Shockwave after Charge or Leap | done (BT-673); "Storm Bolt late" is not modelled (it is cast the tick after the closer) |
| 5 Charge only after the enemy used its mobility | not built (the awareness book has no mobility) |
| 6 Pummel by class and school | the shared interrupt gate (BT-521) |
| 7 Shattering Throw only on an immunity | already so (the trigger reads Divine Shield, Ice Block, the protection hands) |
| 8 Demoralizing Banner early against stealth | not built |
| 9 Disarm after a stun | done (BT-674) |
| 10 Staggering Shout against melee openers | not built (talent policy: a slowed runner) |
| 11 Berserker Rage to break a fear | the shared break layer (the catalog row) |
| 12 Heroic Strike only at the rage cap | as before |
| 13 swap off Karma or Spell Reflection | Karma is an untouchable target (BT-676); Spell Reflection is not read |

Set: unchanged. Storm Bolt and Shockwave now have a rule; Second Wind and Enraged Regeneration split the ladder 50/50 and the set keeps Second Wind.
