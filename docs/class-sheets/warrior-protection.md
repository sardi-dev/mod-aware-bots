# Warrior: Protection (tank)

- **Status:** checked against 5.4.8 data and against the bots' code by reading (2026-11-10). Implemented, compile and
  live check pending (owner approved 2026-11-10, branch `feat/class-sheet-druid-warrior`): rotation gap list items 2 to 6
  and the Dragon Roar premade; item 1 (the pull) is BT-124, items 7 and 8 and the other config items are open. The
  "bots today" columns describe the state before these changes.
- **Sources:**
  - Method, "Protection Warrior Class Guide - MoP Classic" (Phase 1, level 90 raid and dungeon, 22 July 2025), read
    2026-11-10.
  - wowtbc.gg, "Protection Warrior" in the MoP class guides (MoP Classic, no patch line on the page), read 2026-11-10.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, Talent, GlyphProperties,
    SpellCooldowns, SpellPower, SpellRange, SpellEquippedItems; `data/db2/Item-sparse.db2` for glyph item ids) through
    `spec_data.py` and short read-only scripts.
  - Not readable: Overgear's warrior overview and Icy Veins (both answer HTTP 403 to a scripted fetch); Wowhead needs a
    browser. A third page found (a guide site) is a Classic-era (level 30 cap) guide, not 5.x, and was not used.
- **Levels covered:** 15 to 60 in detail (5-player dungeons, a human leader who marks targets), 61 to 90 briefly.

Which source was richer: **talents, glyphs, stat order** Method (wowtbc lists options without a pick and has no stat
order); **opener, single target, defensives, rage** wowtbc (it has an opener, keeps Revenge and the shout rage lines that
Method leaves out); **area priority** equal (same two lines); **levels 15 to 60, the pull, taunts, interrupts, what to
do before a spell is learned** neither guide says anything, so those lines are derived from the 5.4.8 data and marked
"derived".

## Talents

Tiers and columns are from Talent.dbc. "Bots today" is `AiPlayerbot.PremadeSpecLink.1.2 = 000000`
(`conf/playerbots.conf.dist:2643`; one digit per tier, the column counted from 0, so every tier takes the left talent).

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (000000) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Double Time (Method default) | Juggernaut when the second charge is not needed | 103827 | Juggernaut (103826), differs, but it is Method's named alternative |
| 2 (30) | Enraged Regeneration | Second Wind, Impending Victory (not named by the guides) | 55694 | Enraged Regeneration, same |
| 3 (45) | Disrupting Shout | Piercing Howl, Staggering Shout (not named) | 102060 | **Staggering Shout (107566), differs** |
| 4 (60) | Bladestorm in dungeons, Dragon Roar in raids | Shockwave (not named) | 46924 / 118000 | Bladestorm, same as the dungeon pick (see the warning below) |
| 5 (75) | Vigilance | Mass Spell Reflection, Safeguard (not named) | 114030 | **Mass Spell Reflection (114028), differs** |
| 6 (90) | Bloodbath | Avatar when a raid needs the roots and snares removed; Storm Bolt not named | 12292 | Avatar (107574), differs (Method's raid swap) |

Notes on the picks (data, not guide):

- Disrupting Shout (40 s) shares a 15 s cooldown with Pummel (spell text). Neither Disrupting nor Staggering Shout is cast
  by the tank in a dungeon today (see the gap list), so the tier 3 change needs the rotation line to pay off.
- Bladestorm: the spell text says that during it "you can only perform shout abilities". A tank in Bladestorm cannot
  Taunt, Shield Slam, Shield Block, Revenge or press Last Stand. Method still names it for dungeons (area damage). The
  Protection-only buff 131086 has no description in the data, so whether it lifts that limit is **to check live**.
  Dragon Roar knocks enemies back and down, which also moves a pack away from the tank (derived). Shockwave (frontal
  stun) has neither problem. Owner decision; the guide pick stays until a live test.
- Vigilance: "your Taunt has no cooldown" while it lasts (spell text), which is the guide's "easier multi-target taunt".

Proposed link by the guide (dungeon): `102021` (Double Time / Enraged Regeneration / Disrupting Shout / Bladestorm /
Vigilance / Bloodbath). For a character below 75 only the first four digits are in effect.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and
75 (config comment; the unlock levels are core-side, not in the data files). Config order is major, minor, major,
minor, minor, major (`conf/playerbots.conf.dist:2644`).

| Type | Glyph | Item id | Why (one clause) | Bots today (`43415,45794,43424,43395,49084,83096`) |
|------|-------|--------:|------------------|------------------------|
| Major | Heavy Repercussions | 43415 | Shield Slam hits harder while Shield Block is active | yes (slot 1) |
| Major | Hold the Line | 43424 | Revenge hits harder after a parry | yes (slot 3) |
| Major | Unending Rage (Method) | 43399 | more maximum rage, so Shield Block and Heroic Strike are not wasted at the cap | **no: Incite (83096) in slot 6, from level 75** |
| Major, situational | Incite | 83096 | Demoralizing Shout makes the next 3 Heroic Strike or Cleave free (listed by wowtbc among the main three) | slot 6 |
| Major, situational | Rude Interruption | 43417 | Pummel interrupt gives a damage buff, for kick-heavy fights | no |
| Major, situational | Death From Above, Enraged Speed | 45792, 43413 | mobility | no |
| Minor | none worth taking (Method) | | | Intimidating Shout (45794), Mystic Shout (43395), Thunder Strike (49084): harmless |

Disagreement: wowtbc puts Incite in the main three and Unending Rage under "rage pooling"; Method puts Unending Rage in the
main three and Incite under situational damage. Method is used (it names raid and dungeon). Incite only helps while
Demoralizing Shout is cast often, which the bots do not do (see the gap list).

## Stat order

Method: Hit to 7.5 percent, Expertise to about 15 percent, Strength, Parry, Dodge, Critical Strike, Mastery, Haste. Those
caps are level 90 numbers and cannot be reached with the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason): a **shield is mandatory** (Shield Slam,
Shield Block, Shield Barrier and Devastate all require an equipped shield in SpellEquippedItems; Heroic Strike and
Revenge need a weapon), so a shield beats any other upgrade when the bot has none; then plate (Plate Specialization,
level 50, gives Stamina only in all-plate), Strength, Stamina (not in the guide: the survival stat while the 90 caps are
out of reach), Parry and Dodge, then Mastery (Critical Block, level 80), Critical Strike, Haste last; Hit and Expertise
only near level 90.

## What the specialisation knows, by level (5.4.8 data)

Ids are the spells the player actually casts; the data holds several old rows of the same name.

| Level | Spell (id) |
|------:|------------|
| 1 | Heroic Strike (78, 30 rage), Battle Stance (2457) |
| 3 | Charge (100, 8 to 25 yd, 20 s) |
| 5 | Victory Rush (34428) |
| 10 | Shield Slam (23922, free, 6 s), Defensive Stance (71, learned 9 to 10), Unwavering Sentinel (29144, passive), Vengeance (93098, passive); the specialisation is chosen here |
| 12 | Taunt (355, 30 yd) |
| 16 | Sunder Armor (7386) |
| 18 | Shield Block (2565, 60 rage, 2 charges) |
| 20 | Thunder Clap (6343, 20 rage, 6 s) |
| 22 | Heroic Throw (57755, 30 yd, 30 s) |
| 24 | Pummel (6552, melee range) |
| 26 | Devastate (20243, free, replaces Sunder Armor) |
| 28 | Disarm (676) |
| 30 | Revenge (6572, free, 9 s, hits three), tier 2 talent |
| 36 | Hamstring (1715) |
| 38 | Last Stand (12975, 180 s) |
| 42 | Battle Shout (6673) |
| 44 | Cleave (845, 30 rage) |
| 45 | tier 3 talent |
| 46 | Blood and Thunder (84615, passive: Thunder Clap applies Deep Wounds) |
| 48 | Shield Wall (871, 180 s) |
| 50 | Sword and Board (46953, passive), Plate Specialization (86535, passive) |
| 52 | Intimidating Shout (5246, 90 s) |
| 54 | Berserker Rage (18499, 30 s) |
| 56 | Demoralizing Shout (1160, 60 s) |
| 58 | Ultimatum (122509, passive) |
| 60 | Bastion of Defense (84608, passive), tier 4 talent |
| 62 | Recklessness (1719) |
| 66 | Spell Reflection (23920, 25 s) |
| 68 | Commanding Shout (469) |
| 72 | Intervene (3411, 30 s) |
| 74 | Shattering Throw (64382) |
| 75 | tier 5 talent |
| 76 | Riposte (145672, passive) |
| 80 | Mastery: Critical Block (76857) |
| 81 | Shield Barrier (112048, 20 to 60 rage) |
| 83 | Rallying Cry (97462, 180 s) |
| 85 | Heroic Leap (6544) |
| 87 | Mocking Banner (114192, 180 s, area taunt), Skull Banner (114207), Demoralizing Banner (114203) |
| 90 | tier 6 talent |

Facts the priority lists rest on (spell texts and tables of the data): Defensive Stance reduces damage taken, raises
threat and gives 1 rage every 3 s in combat; Shield Slam and Revenge give rage in Defensive Stance; Revenge hits two more
enemies and a dodge or parry resets its cooldown; Devastate and Sunder Armor apply Weakened Armor (stacks to 3);
Thunder Clap applies Weakened Blows (less physical damage dealt); Sword and Board: Devastate can reset Shield Slam;
Ultimatum: a Shield Slam crit makes the next Heroic Strike or Cleave free and a sure crit; Battle Shout and Commanding
Shout also generate rage; Berserker Rage removes fear, sap and incapacitate; Disarm makes a creature "deal significantly
reduced damage". No ability in the list needs a stance (SpellShapeshift is empty for them), so there is no stance
dance below 90; Charge works in any stance.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked
"derived" comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to
`mod-aware-bots-548/src/` (module repository).

### The pull (all bands; a human leader marks, the tank pulls marks only, the group waits)

Setup verified by reading: tanks carry `tank`, `tank assist`, `pull`, `pull back`, `aoe` (`Factory/AiFactory.cpp:377`),
and out of combat `tank assist`, `pull`, `mark pull` (`AiFactory.cpp:750`, `:884-886`). The trigger fires when a mark
exists, is alive, not in combat, within 40 yd and every member is alive, out of combat, above 60 percent health and
(mana users) 50 percent mana (`strategy/triggers/MarkTriggers.cpp:49-84`, `:32-45`); then `pull rti target`
(`strategy/generic/MarkPullStrategy.cpp:12`).

| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| P1 | core | the group is ready and a mark is up | stay in Defensive Stance before the pull (from 10) | **gap: the stance trigger exists only in the combat engine (`TankWarriorStrategy.cpp:74`), the non-combat strategy has none (`GenericWarriorNonCombatStrategy.cpp`); the tank enters combat in Battle Stance and swaps in the first tick (to check live how long that takes)** |
| P2 | core | level 22 and up, mark within 30 yd | Heroic Throw (57755, 22) | yes (`WarriorPullStrategy.cpp:34-35`) |
| P3 | core | levels 12 to 21, mark within 30 yd (derived: Taunt reaches 30 yd and can start a fight, so no run-in is needed) | Taunt (355, 12) | **gap: before level 22 the pull is refused (`WarriorPullStrategy.cpp:24-25`, then `PullStrategy.cpp:157-159`), with an error tell to the leader each time the trigger fires (to check live how often)** |
| P4 | good | Heroic Throw on cooldown (30 s) or not in range (derived: Charge is learned at level 3, 8 to 25 yd, stuns 1.5 s) | Charge (100, 3) | **gap: Charge is not a pull action; with Heroic Throw on cooldown the name falls back to "shoot" (`WarriorPullStrategy.cpp:37`), which a 5.4.8 warrior cannot cast, and the pull stalls until the 15 s limit (`strategy/generic/PullStrategy.h:41`; stall length to check live)** |
| P5 | good | the pull target is hit; mobs come to the tank | stay at the pull position | yes: `pull back` (`PullStrategy.cpp:262-267`) |

### Levels 15 to 29

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | always in a fight | Defensive Stance (71, 10) | yes, in combat (`TankWarriorStrategy.cpp:74`, `WarriorActions.cpp:90-93`) |
| 2 | core | Shield Slam is ready | Shield Slam (23922, 10) | yes (`TankWarriorStrategy.cpp:58`, `:95-96`, `:130` for Sword and Board from 50) |
| 3 | core | an enemy attacks someone else (healer, DPS) | Taunt (355, 12), 30 yd | yes: `lose aggro` on the current target (`TankWarriorStrategy.cpp:81`, `GenericTriggers.cpp:155`); the tank target picks mobs without aggro first (`TankTargetValue.cpp:99-112`). Taunt is declared a melee action (`WarriorActions.h:178`, range 5 in `GenericSpellActions.cpp:165`) although the spell reaches 30 yd (to check live whether the bot walks into melee first) |
| 4 | core | rage 60 or more and no Shield Block up | Shield Block (2565, 18) | yes (`TankWarriorStrategy.cpp:79`) |
| 5 | core | from 20: Weakened Blows missing on the target, or on cooldown for the area | Thunder Clap (6343, 20) | yes (`TankWarriorStrategy.cpp:98`, `:103`; `ThunderClapTrigger` in `WarriorTriggers.cpp:102`) |
| 6 | core | nothing above is ready | Sunder Armor (7386, 16) until level 26, then Devastate (20243, 26), three Weakened Armor stacks | yes: Devastate with Sunder Armor as the alternative (`TankWarriorStrategy.cpp:35`, `:62`, `:97`); Sunder Armor needs a group (`WarriorActions.cpp:25-27`) |
| 7 | core | rage 60 or more (30 per Heroic Strike), after Shield Block | Heroic Strike (78, 1) | yes (`TankWarriorStrategy.cpp:132`) |
| 8 | good | from 24: an enemy casts | Pummel (6552, 24) | partly: only the current target (`GenericWarriorStrategy.cpp:96`, `GenericTriggers.cpp:469`), PvP-only for other casters (`:99-102`) |
| 9 | good | from 28: a long fight against a hard melee hitter (Disarm cuts its damage) | Disarm (676, 28) | yes, on any target that lasts long enough (`TankWarriorStrategy.cpp:115`; wasted on casters, to check live) |

What a tank does where a key spell is missing (derived): level 10 to 11 has no Taunt, so a loose mob stays loose and the
healer must accept it; no Shield Block until 18 (Shield Slam, Heroic Strike and the Defensive Stance are the whole kit);
no Thunder Clap until 20, no Heroic Throw until 22, no Devastate until 26 (Sunder Armor is the filler from 16). The bots
tolerate all of these because an unknown spell is simply not cast.

### Levels 30 to 44

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 10 | core | Revenge is ready (a dodge or parry resets it); hits three | Revenge (6572, 30) | yes (`TankWarriorStrategy.cpp:80`, default `:63`) |
| 11 | core | health at or below the level's danger point | Last Stand (12975, 38) | yes but late: 20 percent (`TankWarriorStrategy.cpp:136`) and "critical health" (`:119-122`) |
| 12 | good | from 30 if chosen: health under half | Enraged Regeneration (55694, talent tier 2) | yes (`TankWarriorStrategy.cpp:121`) |
| 13 | good | from 42: the shout is missing | Battle Shout (6673, 42) | yes (`TankWarriorStrategy.cpp:106`) |
| 14 | good | two or more enemies within 8 yd of the target, from 44 | Cleave (845, 44) instead of Heroic Strike | yes (`GenericWarriorStrategy.cpp:157`, `:174`; see the gap list on rage use) |
| 15 | good | tier 3 talent at 45 | Disrupting Shout (102060) on a casting pack | **gap: never cast by the tank in a dungeon (`GenericWarriorStrategy.cpp:99-102`, PvP only)** |

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 16 | core | health low or a big hit coming | Shield Wall (871, 48) | yes but late: 20 percent (`GenericWarriorStrategy.cpp:89-90`) |
| 17 | core | Devastate procs Sword and Board | Shield Slam at once (46953, 50) | yes (`TankWarriorStrategy.cpp:130`) |
| 18 | good | feared, sapped or incapacitated | Berserker Rage (18499, 54) | yes (`GenericWarriorStrategy.cpp:107`, `:111`); also cast at 20 rage whenever ready (`:71`), which is not the guide's "when Enrage is down" |
| 19 | good | three or more enemies, at the start of a pack fight | Demoralizing Shout (1160, 56) | partly: aoe lists (`GenericWarriorStrategy.cpp:152`, `:169`) and at 50 percent health (`TankWarriorStrategy.cpp:137`); the line at `TankWarriorStrategy.cpp:76` names a trigger "demoralizing shout" that is not registered anywhere in the module (found by search), so it never fires |
| 20 | good | Shield Slam crit gives Ultimatum | Heroic Strike or Cleave free, from 58 | **gap:** no line reads Ultimatum (a search of `src/` finds no such name); Heroic Strike fires at rage 60 only (`TankWarriorStrategy.cpp:132`) |
| 21 | good | a pack is pulled and a caster is not the current target | Intimidating Shout (5246, 52) as a panic button | only as the Last Stand alternative (`TankWarriorStrategy.cpp:37`) |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 22 | core | tier 4 talent at 60: three or more enemies | Shockwave (46968) or Dragon Roar (118000) or Bladestorm (46924) | yes for all three at relevance 25 (`GenericWarriorStrategy.cpp:149-151`); Bladestorm outranks Shield Slam, Shield Block and Revenge there and locks the tank to shouts (see Talents; to check live) |
| 23 | good | on cooldown, from 62 | Recklessness (1719, 62) | **gap: no line in the tank strategy (the trigger exists and is used by Arms and Fury only)** |
| 24 | good | an enemy casts a spell at the tank, from 66 | Spell Reflection (23920, 66) | yes (`TankWarriorStrategy.cpp:124`) |
| 25 | good | a party member is about to be hit hard, from 72 | Intervene (3411, 72) | yes (`TankWarriorStrategy.cpp:126`) |
| 26 | good | the shout is missing, from 68 | Commanding Shout (469, 68), kept up beside Battle Shout (different buff groups) | yes (`TankWarriorStrategy.cpp:75`) |

### Levels 75 to 89

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 27 | good | tier 5 talent at 75: cast on the party member who draws attacks | Vigilance (114030) | code exists (`TankWarriorStrategy.cpp:78`, `WarriorTriggers.cpp:13`, `WarriorActions.cpp:99`), but the premade does not take the talent |
| 28 | core | from 81, rage 40 or more | Shield Barrier (112048, 81) | yes (`TankWarriorStrategy.cpp:87`, `WarriorTriggers.cpp:5`) |
| 29 | good | from 83, group takes heavy damage | Rallying Cry (97462, 83) | yes at 20 percent only (`GenericWarriorStrategy.cpp:91`) |
| 30 | good | from 85 | Heroic Leap (6544, 85) to reach a target | yes (`GenericWarriorStrategy.cpp:76`) |
| 31 | good | from 87, four or more enemies | Mocking Banner (114192, 87) | yes (`TankWarriorStrategy.cpp:109`); Skull Banner on cooldown (`GenericWarriorStrategy.cpp:105`) |

### Level 90

Guide (Method, wowtbc): Bloodbath and Avatar on cooldown with Recklessness and Skull Banner; Shield Slam, Revenge,
Thunder Clap for Weakened Blows, Devastate for Weakened Armor; Heroic Strike only with excess rage. The bots have Avatar
(`GenericWarriorStrategy.cpp:104`) and Bloodbath in the area list (`:148`), not in the single target list.

## Cooldowns and defensives

Press timing (the guides say "hardest moments"; the thresholds are derived because dungeon trash needs numbers):

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Shield Block | 18 | high uptime, hold a charge for the worst hit | cast whenever it is down and rage is at least 60 | yes, same |
| Shield Barrier | 81 | spend excess rage, better against magic | rage 40 or more and no Barrier | yes |
| Last Stand | 38 | hardest moments | below about 35 percent health, or at a big pull | **late: 20 percent** |
| Shield Wall | 48 | hardest moments | below about 40 percent health | **late: 20 percent** |
| Enraged Regeneration | 30 (talent) | hardest moments | below about 50 percent health | yes at "critical health", 25 percent by default (`conf/playerbots.conf.dist:667`, `TankWarriorStrategy.cpp:119-122`) |
| Demoralizing Shout | 56 | cooldown | at the start of a pack of three or more, not at 50 percent health | partly, see line 19 |
| Spell Reflection | 66 | not given | against a caster hitting the tank | yes |
| Rallying Cry | 83 | raid cooldown | group damage | 20 percent of the bot's own health |

## Healer triage and mana (healers only)

Not applicable to a tank. Rage is the resource: Defensive Stance gives 1 rage per 3 s in combat, Shield Slam and Revenge
give rage in Defensive Stance, shouts give rage (BT-036 keeps them as upkeep only, "won't fix"). Never cap rage
(Method); spend at 60 on Shield Block first, then Heroic Strike (wowtbc: do not delay Shield Block or Barrier).

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Recklessness (62), Spell Reflection (66), Commanding Shout (68), Shield Barrier (81), Rallying
  Cry (83), Heroic Leap (85) and the three banners (87) are not available in the 15 to 60 band; Skull Banner,
  Rallying Cry, Demoralizing Banner and the Shattering Throw lines are left out of the lists below 87, 83 and 74.
- Method leaves Revenge out of its single target list; wowtbc has it; the data has it from level 30 with the full
  spell text (free, three targets, resets on dodge or parry, rage in Defensive Stance), so it stays.
- Method's "Bladestorm for dungeons" conflicts with the spell text ("only shout abilities" during it) for a tank.
- Method's Hit 7.5 and Expertise 15 percent are level 90 caps; they do not steer rolls below that (see Stat order).
- The module config comment says Heroic Leap is level 20; the 5.4.8 spell 6544 is learned at 85 (the level 20 rows of
  that name in the data are old WotLK spells).
- No stance requirements exist for the prot abilities in this data, so "stay in Defensive Stance" is only the
  mitigation and threat bonus; Charge and Berserker Rage work in any stance.
- Disrupting Shout shares Pummel's 15 s cooldown (spell text), so both cannot be used as back to back kicks.

## Gap list (ordered by value for levels 15 to 60)

Config (can go at once):

1. Tier 3 talent: Disrupting Shout instead of Staggering Shout. Pays off only with gap 7.
2. Tier 5 talent: Vigilance instead of Mass Spell Reflection (the Vigilance code exists and is idle).
3. Tier 1: Double Time (Method default) or keep Juggernaut (Method's alternative): low value.
4. Tier 6: Bloodbath instead of Avatar (guide default; level 90 only).
5. Third major glyph: Unending Rage (43399) instead of Incite (83096) (from level 75 only).

Rotation (class fixes, wait for the owner's in-game test):

1. Pull before level 22: use Taunt (355, 30 yd) instead of refusing the pull, and Charge (level 3) as the fallback when
   Heroic Throw is on cooldown, instead of the "shoot" fallback that stalls up to 15 s
   (`WarriorPullStrategy.cpp:24-37`). Highest value: a level 15 to 21 tank cannot pull at all today.
2. Defensive Stance before the pull, out of combat (non-combat strategy has no stance line). **Implemented:** trigger
   "tank defensive stance" in `GenericWarriorNonCombatStrategy.cpp` (group only, not mounted).
3. Thresholds: Last Stand and Shield Wall earlier than 20 percent health; Enraged Regeneration at about half.
   **Implemented:** Shield Wall at 40, Last Stand at 35 when Shield Wall is not available, Enraged Regeneration at 50
   (good tier); constants in `WarriorTriggers.h`, edges in `TankWarriorStrategy.cpp`.
4. Bladestorm: do not start it as the tank (or only when no taunt is needed), or choose another tier 4 talent. Needs a
   live test first. **Implemented both ways:** the tank's Bladestorm action is never useful, and the premade takes
   Dragon Roar (`000200`); Dragon Roar is cast in packs by the aoe strategy and in the tank defaults.
5. Pummel on any casting enemy in a pack, not only the current target; Disrupting Shout in PvE (needs gap 1 talent).
   **Pummel implemented** (action "pummel on pack caster", good tier); Disrupting Shout in PvE is still open.
6. Rage pooling in packs: Cleave (30 rage, relevance 20) fires at 30 rage with two or more enemies, so Shield Block
   (60 rage) is starved (inference from the relevance order, to check live). **Implemented:** the tank's Cleave is not
   useful while Shield Block is ready and rage is below 90.
7. Recklessness (62) on cooldown; Ultimatum free Heroic Strike or Cleave; Demoralizing Shout at the start of a pack
   (and the dead `demoralizing shout` trigger name at `TankWarriorStrategy.cpp:76`). **Only the dead trigger name is
   done:** the edge is removed (the shout is cast by the aoe strategy and at 50 percent health); Recklessness, Ultimatum
   and the pack-start shout are open.
8. Taunt as a ranged action (30 yd) and Berserker Rage only when useful.
