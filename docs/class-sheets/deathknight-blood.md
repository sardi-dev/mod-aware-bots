# Death Knight: Blood (tank)

- **Status:** draft, checked against 5.4.8 data and against the bots' code by reading (2026-11-10). Config items 1 and 2
  are not part of that change (open); rotation items 1 to 3 (without Anti-Magic Shell), 4 (Mind Freeze only), 5 and 6 of the gap
  list are implemented on `feat/class-sheet-mistweaver-blooddk` (BT-134, BT-110, compile pending, owner tests them in his next
  dungeon run). Nothing verified live. Every "bots today" claim below describes the code before that change.
- **Sources:**
  - Method, "Blood Death Knight Class Guide - MoP Classic" (Phase 1, level 90 raid and dungeon, 22 July 2025), read
    2026-11-10.
  - wowtbc.gg, "Blood Death Knight PvE Guide, MoP Classic" (page `blood-tank-death-knight`; no patch line on the page),
    read 2026-11-10.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent,
    GlyphProperties, SpellEffect, SpellCooldowns, SpellPower, SpellRuneCost, SpellMisc, SpellDuration, SpellRange;
    `data/db2/Item-sparse.db2` for glyph item ids) through the data reader and short read-only scripts, and the server
    core's death knight scripts (`spell_dk.cpp`, `spell_mastery.cpp`) for the Death Strike, Blood Shield and Death Grip
    mechanics, and the core's talent and glyph unlock code (`Player.cpp`).
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch), Wowhead (script-built page). Two further guesses (a
    guide-site URL and a Wowhead leveling page) answered 404. Two guides were read, not three.
- **Levels covered:** 55 to 90 (a death knight starts at 55). 55 to 59 and 60 to 74 in detail (5-player dungeons, a
  human leader who marks targets), 75 to 89 and 90 shorter.

Which source was richer: **talents** wowtbc (all options per tier with tags; Method names only the tiers 1, 2 and 6 and
the swaps); **glyphs** equal (Method has a raid and a dungeon set with alternatives, wowtbc a standard set, a dungeon pick
and the minor glyphs that Method calls unimportant); **stat order** Method (wowtbc has none); **opener** wowtbc (ten ordered
steps; Method gives examples); **single target priority** wowtbc (15 steps with the disease rule, Soul Reaper, Death Siphon
and the rune split; Method has 11); **area priority** wowtbc (splits two to three targets from four or more); **defensives**
wowtbc (an ordered list of six with Death Pact; Method names five without order); **the pull, taunts, interrupts, Raise
Ally, Death Strike timing, thresholds, levels 55 to 89, what to do before a spell is learned** neither guide says anything
(wowtbc has one Taunt in its opener, Method names Mind Freeze only for its cost), so those lines are derived from the 5.4.8
data and marked "derived".

Where the guides disagree and the pick: Method puts Blood Boil on a Crimson Scourge proc and Death Strike before Death and
Decay and Heart Strike, and keeps the diseases above both; wowtbc puts Rune Strike (only to avoid capping Runic Power) above the
diseases, Soul Reaper below 35 percent before the Death and Decay line, and Death Strike after the proc spells. Pick: wowtbc's
order as the frame (it is the more complete), with Method's rule that Death Strike comes before Death and Decay and Heart
Strike. On the talents Method says Roiling Blood for more area threat where wowtbc tags Plague Leech as the single target pick;
see Talents.

## Talents

Tiers and columns are from Talent.dbc (tier 0 to 5, three columns). Talent.dbc carries no level. This server opens tier N
at level 15 x (N + 1) for every class (core: `Player::CalculateTalentsPoints` is floor(level / 15) and `Player::LearnTalent`
refuses a tier above it, `Player.cpp:21493-21497`, `:21759-21767`; the bots' `BotFactory::InitTalentTiers` uses the same count,
`Factory/BotFactory.cpp:1272-1285`). So a level 55 death knight has **tiers 1 to 3 at once** (levels 15, 30, 45 are already
passed), the fourth at 60, then 75 and 90. The retail schedule for death knights (56, 57, 58, 60, 75, 90) is not in this
server's data or code; it does not apply here. "Bots today" is `AiPlayerbot.PremadeSpecLink.6.0 = 120120`
(`conf/playerbots.conf.dist:2792`; one digit per tier, the column counted from 0).

| Tier (opens at) | Guide pick | Alternative and when | Spell id | Bots today (120120) |
|-----------------|------------|----------------------|----------|---------------------|
| 1 (55) | Plague Leech (Method default, wowtbc tag "single target") | Roiling Blood for more area threat (Method); Unholy Blight (wowtbc lists only) | 123693 / 108170 / 115989 | Plague Leech (column 1), same; **no bot line casts it** (gap 11) |
| 2 (55) | Purgatory (Method) | Anti-Magic Zone when survival is not a problem (Method; wowtbc: raid cooldown); Lichborne (wowtbc lists only) | 114556 / 51052 / 49039 | Purgatory (column 2), same |
| 3 (55) | not named by Method; wowtbc lists three without a pick | Death's Advance, Chilblains, Asphyxiate | 96268 / 50041 / 108194 | Death's Advance (column 0); the module's comment says wowtbc, the text does not show the choice |
| 4 (60) | Death Siphon (wowtbc tag "DPS", in its single target list; the module's choice) | Death Pact (in wowtbc's defensive list), Conversion | 108196 / 48743 / 119975 | Death Siphon (column 1), same; **no bot line casts it**, while Death Pact has one (`GenericDKStrategy.cpp:182-184`) |
| 5 (75) | not named by Method; wowtbc lists three without a pick | Blood Tap, Runic Empowerment, Runic Corruption | 45529 / 81229 / 51462 | Runic Corruption (column 2), the module's choice |
| 6 (90) | Gorefiend's Grasp (Method: kept in dungeons) | Remorseless Winter for many adds (Method); Desecrated Ground (wowtbc lists only) | 108199 / 108200 / 108201 | Gorefiend's Grasp (column 0), same; no bot line casts any of the three |

Notes on the picks (data and code, not guide):

- Plague Leech (25 s cooldown, 30 yd): consumes Blood Plague and Frost Fever on the target and turns two depleted runes into
  Death Runes. Without Outbreak (learned at 81) the diseases come back through Icy Touch (a frost rune) and Plague Strike
  (an unholy rune), so below 81 it is about rune neutral (derived). Roiling Blood (passive): Blood Boil also triggers
  Pestilence on a diseased target, which needs no new bot line (the bots already cast Blood Boil in packs). Derived pick for
  the bots until a Plague Leech line exists: Roiling Blood; the guides' pick is valid once the line is there.
- Purgatory: a fatal hit is replaced by a Shroud of Purgatory that absorbs healing equal to the damage prevented, the death
  knight dies if the absorb is left when it ends, and a lockout debuff (123981) stops a repeat. Whether the bots' healers heal
  into the shroud is **to check live**.
- Death Pact: heals for 50 percent of maximum health and the minion loses 50 percent of its own (effect values 50 and 50);
  needs an undead minion, which is the ghoul of Raise Dead (46584, 120 s cooldown). The bots' critical health edge casts
  Raise Dead first (relevance 26) and then Death Pact (25). With Death Siphon taken this line is dead; with Death Pact it
  is the emergency heal (derived pick for tier 4).
- Death Siphon: 40 yd, costs one Death Rune, heals for a share of the damage dealt. Gorefiend's Grasp: pulls every enemy
  around a target (hostile or friendly) to that target. A human leader's pull plan does not expect a tank that drags a whole
  room (derived), so the talent stays idle even if a line existed.
- Runic Corruption: a chance for faster rune regeneration when Death Coil, Frost Strike or Rune Strike lands; fits the bots'
  Rune Strike habit (see the priority lists).

Proposed link (the guides' dungeon picks, with the two derived changes): `020020` (Roiling Blood / Purgatory / Death's
Advance / Death Pact / Runic Corruption / Gorefiend's Grasp). Only tiers 1 and 4 change.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and 75
(core: `Player::InitGlyphsForLevel`, `Player.cpp:21081-21099`). Config order is major, minor, major, minor, minor, major
(`conf/playerbots.conf.dist:2794`). A death knight of 55 to 74 has **four slots** (two major, two minor): Festering Blood,
Army of the Dead, Pestilence, the Long Winter. Path of Frost and Anti-Magic Shell join at 75.

| Type | Glyph | Item id | Why (one clause) | Bots today (`104049,43550,43548,104101,43671,43533`) |
|------|-------|--------:|------------------|------------------------|
| Major | Pestilence | 43548 | wider Pestilence radius (data: radius only, no refresh); Method's dungeon pick and wowtbc standard | yes (slot 3, level 50) |
| Major | Anti-Magic Shell | 43533 | the Shell absorbs all magic damage up to its limit; first pick of both guides | yes (slot 6, level 75), but **the bots cast the Shell in PvP only**, so it is idle in PvE (gap 3) |
| Major | Festering Blood | 104049 | Blood Boil treats every target as diseased; wowtbc standard, Method alternative | yes (slot 1, level 25) |
| Major, Method | Regenerative Magic | 104048 | shorter Shell cooldown if it runs its full time | no |
| Major, situational | Outbreak | 43826 | Outbreak has no cooldown and costs 30 Runic Power (wowtbc: dungeons or with Plague Leech; Method alternative); Outbreak is learned at 81 | no |
| Major, situational | Loud Horn, Dark Simulacrum | 104047, 45800 | area pick (Method), niche | no |
| Minor | Army of the Dead | 43550 | the ghouls no longer taunt (spell text), so they do not pull mobs off the tank (derived); wowtbc standard | yes (slot 2, level 25) |
| Minor | the Long Winter | 104101 | Horn of Winter lasts an hour (wowtbc standard) | yes (slot 4, level 50) |
| Minor | Path of Frost | 43671 | fall damage; no tanking effect | yes (slot 5, level 75), harmless |

Disagreement: Method's dungeon set is Pestilence, Anti-Magic Shell, Regenerative Magic; wowtbc's is Festering Blood,
Pestilence, Anti-Magic Shell. The premade is wowtbc's. Both Shell glyphs wait for a PvE Shell line. No change proposed.

## Stat order

Method: Hit to 7.5 percent, Expertise to about 15 percent, Strength, Parry and Dodge, Mastery, Haste, Critical Strike. wowtbc
has no order. The caps are level 90 numbers and cannot be reached with the gear of levels 55 to 89.

For loot rolls and quest rewards below level 90 (derived, with the reason): **plate** (Plate Specialization, level 55, adds
Stamina only in all-plate, so keep a full set); **a slow two-handed weapon** in the main hand (the bots' calculator already
assumes it, `Mgr/Item/StatsWeightCalculator.cpp:838`, `:1134-1139`; the data cannot confirm it); **Strength and Stamina**
first (Stamina is the survival stat while the 90 caps are out of reach); Parry and Dodge; **Mastery is worth nothing below
level 80** (Mastery: Blood Shield is learned at 80, and both core scripts that build the shield check level 80); Haste;
Critical Strike; Hit and Expertise only near level 90.

Bots today (`Mgr/Item/StatsWeightCalculator.cpp:617-635`): Stamina 3.0, Expertise 3.0, Defense 2.5, Mastery 2.4, Parry 2.0,
Dodge 2.0, Hit 2.0, Strength 1.3, Agility 0.2, Attack Power 0.2, Armor 0.15; no Haste or Critical Strike entry in that block
(defaults to check). Gaps (gap 13): Strength below Stamina, Expertise, Defense, Mastery and Hit; Mastery weighted below level
80; Defense Rating was removed from the game in patch 5.0 (to check against the item data).

## What the specialisation knows, by level (5.4.8 data)

Learn levels are from SpellLevels (the cast spell's row) and SpecializationSpells; the spells below are in the death knight
class skill line (SkillLineAbility 796). Rune cost uses the data's own order (blood, unholy, frost, death). Resources: six
runes (two blood, two unholy, two frost; a Death Rune pays any cost) and Runic Power.

| Level | Spell (id, cost, cooldown) |
|------:|----------------------------|
| 55 | Frost Presence (48266, the only presence until 57), Death Grip (49576, 30 yd, 25 s, free, taunts for 3 s unless Tranquil Grip), Icy Touch (45477, frost rune, 30 yd, Frost Fever), Plague Strike (45462, unholy rune, melee, Blood Plague), Blood Strike (45902, blood rune), Death Coil (47541, 40 Runic Power, 40 yd), Death Gate (50977); passives Veteran of the Third War (50029), Blood Rites (50034), Plate Specialization (86537), Vengeance (93099), Death Strike enabler (89832, the 5 s damage record); three talent tiers open |
| 56 | Death Strike (49998, unholy and frost rune, +20 Runic Power), Blood Boil (48721, blood rune, area), Pestilence (50842, blood rune), Raise Dead (46584, 120 s) |
| 57 | Blood Presence (48263), Mind Freeze (47528, melee range, 15 s, 4 s school lock) |
| 58 | Dark Command (56222, 30 yd, 8 s, +200 percent threat for 3 s; Blood only), Strangulate (47476, blood rune, 30 yd, 60 s, 5 s silence) |
| 60 | Heart Strike (55050, replaces Blood Strike, hits up to three), Death and Decay (43265, unholy rune, 30 yd ground, 10 s, 30 s cooldown), tier 4 talent |
| 62 | Icebound Fortitude (48792, 180 s, 12 s), Scent of Blood (49509, passive) |
| 64 | Rune Tap (48982, blood rune, 30 s, off the global cooldown, heals 10 percent of maximum health), Improved Blood Presence (50371, passive), Unholy Presence (48265) |
| 65 | Rune Strike (56815, 30 Runic Power, melee, cannot be dodged, blocked or parried), Horn of Winter (57330, 20 s, +10 Runic Power) |
| 66 | Blood Parasite (49542, passive), Path of Frost (3714) |
| 68 | Anti-Magic Shell (48707, 45 s, 5 s, absorbs 75 percent of spell damage up to 50 percent of maximum health), Scarlet Fever (81132, passive: Blood Boil refreshes the diseases, Blood Plague applies Weakened Blows) |
| 70 | Will of the Necropolis (81164, passive: below 30 percent health Rune Tap is refreshed and free, 25 percent less damage, 45 s limit) |
| 72 | Raise Ally (61999, 30 Runic Power, 40 yd, 600 s), Sanguine Fortitude (81127, passive: Icebound Fortitude 30 percent more) |
| 74 | Dancing Rune Weapon (49028, 90 s, 12 s, 20 percent parry for the death knight) |
| 75 | tier 5 talent |
| 76 | Vampiric Blood (55233, 60 s, 10 s, +15 percent maximum health, +25 percent healing received), Empower Rune Weapon (47568, 300 s, +25 Runic Power), Riposte (145676, passive) |
| 78 | Bone Shield (49222, 60 s cooldown, 5 min, 20 percent less damage while a charge remains) |
| 80 | Mastery: Blood Shield (77513), Army of the Dead (42650, blood, unholy and frost rune, 600 s, 4 s channel) |
| 81 | Outbreak (77575, 30 yd, 60 s, free, both diseases) |
| 84 | Crimson Scourge (81136, passive: Blood Boil and Death and Decay sometimes cost no rune; the proc buff is 81141, 15 s) |
| 87 | Soul Reaper (114866, blood rune, 6 s, extra damage after 5 s below 35 percent health) |
| 90 | tier 6 talent |

Facts the priority lists rest on (spell text, effect tables and the core scripts):

- **Death Strike** (`spell_dk.cpp:792-842`, `:874-882`): heals for 20 percent of the damage taken from non-player sources in the
  last 5 s, at least about 7 percent of maximum health (effect 2); each stack of Scent of Blood adds 20 percent to both. The
  5 s damage record only counts while Blood Presence is up (it is zeroed otherwise). It cannot be parried, and in Blood
  Presence it adds a Blood Shield. Blood Rites (55): the frost and unholy runes it uses come back as Death Runes, and Death
  Strike damage is +40 percent.
- **Blood Shield** exists from level 80 only (Mastery: Blood Shield). The core has two scripts for it: one inside the Death
  Strike script that casts an absorb sized as the mastery percentage of **maximum health** (`spell_dk.cpp:835-840`), and
  one on the Death Strike heal that sizes it as the mastery percentage of the **healing** (`spell_mastery.cpp:188-232`); the
  5.4.8 SQL binds both (`spell_dk_death_strike` to 49998, `spell_mastery_blood_shield` to 45470). The spell text says a share
  of the healing. Whether the two add up and how big the shield is at 80 and above is **to check live** (the first would
  make the shield independent of the damage taken).
- **Death Grip** (25 s) forces the target to attack the death knight for 3 s and adds threat; the Tranquil Grip glyph removes
  that. Dark Command has an 8 s cooldown; Death Grip is the only taunt at 55 to 57.
- **Diseases** last 30 s (Blood Plague 55078, Frost Fever 55095). Pestilence only spreads what is on the target. Blood Boil
  refreshes diseases only from 68 (Scarlet Fever).
- **Runes:** Death Strike takes one unholy and one frost rune; Heart Strike, Blood Boil, Pestilence, Rune Tap, Soul Reaper
  and Strangulate take a blood rune; Plague Strike and Death and Decay an unholy rune; Icy Touch a frost rune. Runic
  Power comes from most rune spells (+10, Death Strike +20, Army of the Dead +30, Horn +10, Empower Rune Weapon +25) and is
  spent on Rune Strike (30), Death Coil (40) and Raise Ally (30). Changing presence consumes the stored Runic Power.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked "derived"
comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to `src/` of the module
repository. Relevance numbers: default actions sit at 5.0 plus a fraction, normal 10, high 20, interrupt 40, emergency 90
(`strategy/Strategy.h:37-51`); a trigger's action outranks every default action, and the engine takes the highest
relevance first.

### What runs today (verified by reading)

The Blood death knight carries `blood` (also registered as `tank`, `Classes/deathknight/DKAiObjectContext.cpp:46-47`),
`tank assist`, `pull`, `pull back` in combat (`Factory/AiFactory.cpp:518-523`), `tank face` (`:545-546`), and out of combat
`tank assist`, `pull` (`:775-779`) and `mark pull` (`:886-887`). There is no `aoe` strategy: the area edge sits in the
generic death knight strategy.

Default actions (`strategy/Classes/deathknight/BloodDKStrategy.cpp:92-104`): Dancing Rune Weapon 5.9, Soul Reaper 5.7,
Heart Strike 5.65 (alternative Blood Strike, `:57-64`), Rune Strike 5.6, Icy Touch 5.5, Death Coil 5.2, Horn of Winter 5.1,
melee 5.0; the Blood Boil edge is at 5.85 (`:142-143`). Trigger edges, relevance in brackets: Raise Dead (26) and Death
Pact (25) at critical health, 25 percent (`GenericDKStrategy.cpp:182-184`); Vampiric Blood (25) at critical health
(`BloodDKStrategy.cpp:120-121`); Blood Tap (25, `:113-114`); Icebound Fortitude (25) and Rune Tap (24) under 40 percent
(`GenericDKStrategy.cpp:188-190`); Army of the Dead (24) and Death Strike (23) under 45 percent, the low health setting
(`BloodDKStrategy.cpp:117-119`); Dark Command (23, `:115-116`); Outbreak (23, `:139-140`); Bone Shield (23, `:134-135`); Icy Touch
(22) and Plague Strike (22) when the disease is missing (`:122-125`); Mind Freeze (21) on the current target and on an
enemy healer (`GenericDKStrategy.cpp:173-177`); Death Strike (21) when both unholy runes are about ready (`BloodDKStrategy.cpp:129-130`);
Vampiric Blood (21) under 90 percent health (`:145-146`); Empower Rune Weapon (21) with every rune down
(`GenericDKStrategy.cpp:218-219`); Death and Decay (29), Pestilence (14) and Blood Boil (13) with three or more
enemies within 8 yd (`:204-207`, `strategy/triggers/GenericTriggers.h:300-304`); Rune Strike (13) whenever it can be cast
(`BloodDKStrategy.cpp:111-112`); Raise Dead (11, `:150-151`) and Horn of Winter (11, `GenericDKStrategy.cpp:178-179`).
Out of combat (`GenericDKNonCombatStrategy.cpp:46-53`): sharpening stone, Horn of Winter (21), Bone Shield (21); **no
presence line**.

Presences are applied only as a prerequisite of single actions: Rune Strike, Icy Touch, Heart Strike, Death Strike and Dark
Command carry "blood presence" in their action nodes (`BloodDKStrategy.cpp:43-79`). The presence trigger that would keep it
up is declared but never registered (`DKTriggers.h:207-213`, no entry in `DKAiObjectContext.cpp`), as in the AC original.
An impossible prerequisite (a spell not yet learned) is skipped and the action itself runs (`strategy/Engine.cpp:206-217`,
`PushAgain` skips prerequisites).

### The pull (all bands; a human leader marks, the tank pulls marks only, the group waits)

Setup verified by reading: the trigger fires when a mark exists, is alive, not in combat, within 40 yd and every group member
is alive, out of combat, above 60 percent health and (mana users) 50 percent mana (`strategy/triggers/MarkTriggers.cpp:49-84`,
`:32-45`); then `pull rti target` (`strategy/generic/MarkPullStrategy.cpp:12`). The request is checked in
`PullStrategy::CanDoPullAction` (`strategy/generic/PullStrategy.cpp:137-162`).

The death knight's override (`Classes/deathknight/DeathKnightPullStrategy.cpp:14-27`, base action "icy touch" at
`DeathKnightPullStrategy.h:43`): Death Grip when it can be cast; else Dark Command when Icy Touch cannot be cast and Dark
Command can; else Icy Touch.

**Does the death knight have the warrior's, bear's and monk's problem? No, with one small exception.** Verified by reading:

- The fallback name is "icy touch", a real spell, never "shoot". The "shoot needs a ranged weapon" refusal
  (`PullStrategy.cpp:150-152`) therefore never applies (the death knight is not in the exempt list, but the name is never
  "shoot"), and the name is never empty (`:157-159`). Death Grip (55) is known from the first level, so there is no band
  where the pull is refused.
- The pull action runs only the named action: `PullAction::Execute` calls `DoSpecificAction` (`strategy/actions/PullActions.cpp:209`),
  which creates the action and executes it without prerequisites or alternatives (`strategy/Engine.cpp:324-365`). The
  `death grip` node's alternative "icy touch" (`GenericDKStrategy.cpp:60-66`) is never used by the pull; the choice lives in
  `GetPullActionName` alone.
- The one stall: Death Grip on cooldown (25 s, usually spent in the last fight) **and** no frost rune ready **and**, below 58,
  no Dark Command. The name falls to "icy touch", the cast is refused for lack of a rune, the pull action returns false every tick
  and the pull ends only at the 15 s limit (`strategy/generic/PullStrategy.h:41`, `strategy/triggers/PullTriggers.cpp:41-43`), after
  which the mark trigger fires again. A frost rune comes back in about 10 s, so the stall is short and rare (estimate; **to check
  live**). From 58 Dark Command (8 s) closes it in most cases.
- While a pull target is set every other action is multiplied to zero (`PullStrategy.cpp:183-199` and `MagePullMultiplier`
  `:201-229` with `strategy/PassiveMultiplier.cpp:34-54`), **including "blood presence"**. A death knight without the
  presence (after a resurrection, a login, or at 55 to 56) pulls and fights the first seconds without it (row P1).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| P1 | core | the group is ready and a mark is up | be in Blood Presence (48263, 57); Frost Presence (48266, 55) before that (derived: it is the only presence known, and it adds Runic Power) | **gap: no out-of-combat line; the presence is cast only as a prerequisite of the first Rune Strike, Icy Touch, Heart Strike, Death Strike or Dark Command after the pull ends (`BloodDKStrategy.cpp:43-79`); nothing casts Frost Presence for a Blood death knight at 55 to 56** |
| P2 | core | mark within 30 yd, Death Grip ready | Death Grip (49576, 55): the mark arrives at the tank and is taunted for 3 s | yes: `DeathKnightPullStrategy.cpp:20-21`, range 30 yd from the spell (`PullStrategy.cpp:121-133`) |
| P3 | core | Death Grip on cooldown | Icy Touch (45477, 55), 30 yd, a frost rune | yes: `DeathKnightPullStrategy.cpp:26`; short stall without a frost rune, see above |
| P4 | good | Death Grip on cooldown and no frost rune | Dark Command (56222, 58), 30 yd | yes: `DeathKnightPullStrategy.cpp:23-24`; none below 58 |
| P5 | good | the mark is a caster, or stands alone (derived: Death Grip puts one mob in melee at once, which is good for a caster that would stand and cast) | Death Grip | **gap: no check. A mark inside a pack of three or more is gripped out of the pack with the same rule, and the tank may not hold what then comes; use Icy Touch (no yank, the pack walks in together) for a mark with three or more enemies within 10 yd of it. No code reads the pack around the mark** |
| P6 | good | the pull target is hit; mobs come to the tank | stay at the pull position | yes: `pull back` (`PullStrategy.cpp:262-268`) |

### Levels 55 to 59

The tank has Frost Presence (55), Blood Presence (57), Death Grip, Icy Touch, Plague Strike, Blood Strike, Death Coil, Death
Strike (56), Blood Boil and Pestilence (56), Raise Dead (56), Mind Freeze (57), Dark Command and Strangulate (58). Three
talent tiers are open at 55. No Heart Strike (60), no Death and Decay (60), no Icebound Fortitude (62), no Rune Tap (64), no
Rune Strike (65), no Horn of Winter (65), and no defensive cooldown of any kind. Survival is Blood Presence (10 percent less
damage, 25 percent more Stamina, more armor) and Death Strike.

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | always (see P1) | Blood Presence (48263, 57); Frost Presence (48266) at 55 to 56 | partly, see P1 |
| 2 | core | Frost Fever or Blood Plague missing on the target (30 s each; derived: recast on the first missing tick, since Outbreak is learned at 81) | Icy Touch (45477, 55), then Plague Strike (45462, 55) | yes: edges at 22 on the target's own aura missing (`BloodDKStrategy.cpp:122-125`, `DKTriggers.h:108-118`); only the current target, other mobs get diseases through Pestilence or Roiling Blood (see rows 7 and 14) |
| 3 | core | a frost and an unholy rune ready, and damage was taken in the last 5 s or health is under 90 percent (derived: the heal is 20 percent of the last 5 s of damage, minimum about 7 percent of maximum health; from 56) | Death Strike (49998, 56) | **partly, wrong trigger: fires when both unholy runes are about ready, with no damage or health condition (`BloodDKStrategy.cpp:129-130`, `DKTriggers.cpp:46`), and under 45 percent health (`:117-119`). The frost rune is spent first by the Icy Touch filler at 5.5 (`:97`), so Death Strike is often refused for lack of a frost rune (inference from the relevance numbers; to check live in an action log)** |
| 4 | core | a blood rune ready | Blood Strike (45902, 55), Heart Strike (55050) from 60 | yes: Heart Strike default 5.65 with Blood Strike as its alternative when Heart Strike is unknown (`BloodDKStrategy.cpp:57-64`, `:98`) |
| 5 | core | an enemy attacks someone else (healer, damage), any mob | Dark Command (56222, 58); Death Grip (49576, 25 s) before 58 and when Dark Command is on cooldown | yes: `lose aggro` on the current target (`BloodDKStrategy.cpp:115-116`, `strategy/triggers/GenericTriggers.cpp:155-161`), never on a player (`DKActions.h:32-38`); the Dark Command node falls back to Death Grip (`BloodDKStrategy.cpp:73-79`); the tank target picks mobs without aggro first (`strategy/value/TankTargetValue.cpp`). Death Grip as the taunt also drags a straying caster back (good) |
| 6 | good | an enemy casts | Mind Freeze (47528, 57), 15 s, melee range | partly: only the current target and an enemy healer (`GenericDKStrategy.cpp:173-177`); any other caster only in PvP (`:196-200`) |
| 7 | good | three or more enemies within 8 yd of the target, from 56 | Blood Boil (48721, 56), then Pestilence (50842, 56) when the target has a disease | yes: the area edge (`GenericDKStrategy.cpp:204-207`); Pestilence needs one of the bot's own diseases on the target (`DKActions.cpp:45-54`). Blood Boil below 68 does not refresh diseases (Scarlet Fever, 68) |
| 8 | good | Runic Power 40 or more (derived: the only Runic Power spender until Rune Strike at 65) | Death Coil (47541, 55) | yes: default 5.2 (`BloodDKStrategy.cpp:103`) |
| 9 | good | before the first pull, and once more at critical health | Raise Dead (46584, 56): the ghoul is a minion for Death Pact | partly: in combat at 11 (`BloodDKStrategy.cpp:150-151`, its own 3 min stamp `DKActions.cpp:32-43`) and at critical health (`GenericDKStrategy.cpp:182-184`); not before the pull |
| 10 | expert | a caster stands outside melee range | Strangulate (47476, 58), 30 yd silence, 60 s | **gap: PvP only (`GenericDKStrategy.cpp:196-200`)** |

What a tank does where a key spell is missing (derived): 55 to 56 has no Dark Command (Death Grip is the taunt, 25 s), no
Blood Presence (Frost only) and no Death Strike at 55; 57 has no Dark Command; no Heart Strike until 60 (Blood Strike);
nothing else active until Icebound Fortitude at 62. The bots tolerate every unlearned spell: an impossible action falls to
its alternative or is dropped (`strategy/Engine.cpp:206-217`, `:238-245`).

### Levels 60 to 74

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 11 | core | a blood rune ready | Heart Strike (55050, 60), hits up to three | yes (row 4) |
| 12 | good | three or more enemies, or on cooldown in a single target fight (Method: on cooldown, 30 s) | Death and Decay (43265, 60), 30 yd ground | partly: only with three or more enemies (29, `GenericDKStrategy.cpp:204-205`); never against one or two. Ground placement at the pack is **to check live** |
| 13 | core | health under 50 percent, or a big pull (derived) | Icebound Fortitude (48792, 62) | **late: under 40 percent (`GenericDKStrategy.cpp:188-190`)** |
| 14 | good | health under 60 percent and a blood rune free (derived: 10 percent of maximum health, off the global cooldown) | Rune Tap (48982, 64) | **late: under 40 percent (`GenericDKStrategy.cpp:188-190`, `DKActions.h:371-376`)** |
| 15 | good | Runic Power 30 or more and no rune spell ready, or Runic Power near the cap (guides: spend Runic Power; wowtbc: only to avoid the cap) | Rune Strike (56815, 65) | **different: cast whenever it can be cast (`BloodDKStrategy.cpp:111-112`, 13), which outranks Heart Strike (5.65) and delays rune spells by one global cooldown each time; cheap, and Runic Corruption likes it; to check live** |
| 16 | good | the buff is missing | Horn of Winter (57330, 65) | yes (`GenericDKNonCombatStrategy.cpp:50-51`, `GenericDKStrategy.cpp:178-179`, default 5.1) |
| 17 | good | a caster hits the tank with spells and Mind Freeze is used (derived), or health under 70 percent against casters | Anti-Magic Shell (48707, 68) | **gap: PvP only (`GenericDKStrategy.cpp:196-200`)** |
| 18 | good | diseases at 3 s or less, from 68 | Blood Boil (refreshes both, Scarlet Fever) | partly: the Blood Boil edge fires at 3 s from level 56 on (`BloodDKStrategy.cpp:142-143`, `DKTriggers.cpp:109-119`); below 68 it refreshes nothing and costs a blood rune |
| 19 | core | an ally is dead | Raise Ally (61999, 72), 30 Runic Power, 600 s | **missing: no action, trigger or name in `src/` (search; ledger BT-110)** |
| 20 | good | on cooldown | Dancing Rune Weapon (49028, 74) | yes: default 5.9, above Heart Strike (`BloodDKStrategy.cpp:101`) |
| 21 | good | tier 4 talent at 60 | Death Pact (48743) at critical health after Raise Dead | code exists (`GenericDKStrategy.cpp:182-184`); the premade takes Death Siphon (no line) |

What a tank does where a key spell is missing (derived): below 62 there is no active defensive at all; Rune Tap comes at 64,
Rune Strike at 65 (Death Coil is the Runic Power sink until then), Anti-Magic Shell at 68, Raise Ally at 72, Dancing Rune
Weapon at 74.

### Levels 75 to 89

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 22 | core | health under 60 percent or at the start of a pack of three or more (derived) | Vampiric Blood (55233, 76) | **wrong band: fires under 90 percent health at 21 (`BloodDKStrategy.cpp:145-146`), i.e. on cooldown from the first scratch, and at critical health at 25 (`:120-121`)** |
| 23 | good | every rune on cooldown (wowtbc: and Runic Power will not cap) | Empower Rune Weapon (47568, 76) | yes (`GenericDKStrategy.cpp:218-219`); no cap check |
| 24 | core | no Bone Shield charge left, in and out of combat | Bone Shield (49222, 78) | yes (`BloodDKStrategy.cpp:134-135`, `GenericDKNonCombatStrategy.cpp:52-53`) |
| 25 | core | from 80, in Blood Presence | Blood Shield through Death Strike (Mastery, 77513) | covered by row 3; see the two scripts above |
| 26 | expert | a hard pull in the opener, 7 s before the pull (both guides put Army of the Dead in the opener) | Army of the Dead (42650, 80), 4 s channel, 600 s | **wrong use: cast at 24 under 45 percent health (`BloodDKStrategy.cpp:117-119`), which is a 4 s channel in the worst moment; the Death Strike edge beside it is fine** |
| 27 | core | a disease missing | Outbreak (77575, 81), 60 s; Icy Touch and Plague Strike while it is on cooldown (wowtbc) | yes (`BloodDKStrategy.cpp:139-140`, `DKTriggers.cpp:87-95`, with the 22 edges as the fallback) |
| 28 | core | the Crimson Scourge buff is up | Blood Boil or Death and Decay for no rune (84) | partly: Blood Boil only (`DKTriggers.cpp:76`, `:109-113`, buff 81141); Death and Decay is not tied to the proc |
| 29 | good | a target at or under 35 percent health (wowtbc), a blood rune free | Soul Reaper (114866, 87) | **wrong: default 5.7 on any target, taking the blood rune that Heart Strike would use (`BloodDKStrategy.cpp:102`)** |

### Level 90

Guide (Method, wowtbc): Blood Presence, Horn of Winter, Bone Shield, Dancing Rune Weapon, the diseases (Outbreak, else Icy
Touch and Plague Strike), Empower Rune Weapon with all runes down, Soul Reaper under 35 percent, Death Strike, Blood Boil or
Death and Decay on Crimson Scourge, Death and Decay on cooldown, Heart Strike, Rune Strike (Death Siphon with Death Runes if
taken). Area: Pestilence to keep the diseases on everything, Blood Boil in place of Heart Strike from four targets. The bots
match the single target skeleton and differ in the rows above; the tier 6 talent has no line.

## Cooldowns and defensives

The guides give an order (wowtbc: Rune Tap, Vampiric Blood, Icebound Fortitude, Dancing Rune Weapon, Anti-Magic Shell, Death
Pact; Method names five without order and adds no number); the thresholds are derived because dungeon trash needs numbers.
Thresholds are the tank's own health percent.

**Death Strike and Blood Shield rule (derived from the data).** Press Death Strike whenever its frost and unholy runes are
ready, **except** when a disease is missing and the unholy or frost rune is the only one that can reapply it (diseases come
first, Method step 3). Reason: it heals 20 percent of the last 5 s of damage, at least about 7 percent of maximum health, so a
Death Strike without recent damage still heals the minimum and (from 80) refreshes the shield, while a Death Strike after a
big hit is the best heal the tank has. Hold it only for that disease case. Do not hold it for more damage: the 5 s window
rolls, and the runes come back as Death Runes (Blood Rites) that Heart Strike and Blood Boil can use. Below 57 the damage
record is empty (no Blood Presence) and the heal is the minimum. Keep Bone Shield up out of combat so the first hits are cut
by 20 percent (level 78).

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Blood Presence | 57 | whole fight | before the pull, after every resurrection | combat prerequisite only (see P1) |
| Death Strike | 56 | main self heal | the rule above | wrong trigger, see row 3 |
| Bone Shield | 78 | recast at no charges | out of combat and in combat | yes |
| Rune Tap | 64 | first in wowtbc's list | under 60 percent with a blood rune free; free again below 30 percent (Will of the Necropolis) | under 40 percent only |
| Icebound Fortitude | 62 | listed | under 50 percent, or the start of a pack of three or more; 50 percent less damage from 72 | under 40 percent only |
| Vampiric Blood | 76 | listed | under 60 percent, or a pack of three or more | under 90 percent (on cooldown) and at critical health |
| Dancing Rune Weapon | 74 | on cooldown | on cooldown | yes (default 5.9) |
| Anti-Magic Shell | 68 | listed | against a caster hitting the tank, or under 70 percent with casters present | PvP only |
| Death Pact | 60 (talent) | on the ghoul | at critical health after Raise Dead | code exists; idle with the premade |
| Army of the Dead | 80 | opener, 7 s before the pull | before a hard pull only (expert) | at under 45 percent health (wrong) |
| Empower Rune Weapon | 76 | all runes down | all runes down, Runic Power below 75 | yes, no cap check |
| Dark Command, Death Grip | 58, 55 | one Taunt in wowtbc's opener | any enemy not on the tank; Death Grip when Dark Command is on cooldown or before 58 | yes, current target only |
| Mind Freeze | 57 | not given | any casting enemy in reach (15 s) | target and enemy healer only |
| Strangulate | 58 | not given | a caster out of melee range | PvP only |
| Raise Dead | 56 | opener | before the pull, then at critical health | in combat once per 3 min, critical health |
| Raise Ally | 72 | not given | a dead healer first, then the tank or a damage dealer, when 30 Runic Power are free | missing |

## Healer triage and mana (healers only)

Not applicable to a tank. The resources are runes and Runic Power. Blood runes pay Heart Strike (Blood Strike), Blood Boil,
Pestilence, Rune Tap and Soul Reaper; unholy runes pay Plague Strike and Death and Decay; frost runes pay Icy Touch; Death
Strike pays one unholy and one frost rune. Rule (guides): Death Strike takes the frost and unholy runes, Heart Strike the blood
runes, Icy Touch and Plague Strike only for diseases. The bots break the first part by casting Icy Touch as a filler (row
3). Runic Power: Rune Strike last in Method and only at the cap in wowtbc; the bots spend it at 30 (row 15). Do not change
presence in combat: it consumes the stored Runic Power.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Bone Shield (78), Vampiric Blood (76), Dancing Rune Weapon (74), Outbreak (81), Crimson Scourge
  (84) and Soul Reaper (87) are not available in the 55 to 77 band; the lists leave them out below their level.
- Talents: the guides give a level 90 build. This server opens three tiers at 55 and the others at 60, 75 and 90 (floor of
  level over 15), not at 56, 57, 58, 60, 75, 90.
- wowtbc's "Blood Boil refreshes them" holds only from level 68 (Scarlet Fever); below that Blood Boil does not touch the
  diseases.
- Method and wowtbc say Death and Decay "on cooldown"; the data gives it a 30 s cooldown and one unholy rune.
- Death Strike's heal and the Blood Shield are not described in either guide; the data and the two core scripts decide (see
  "What the specialisation knows"). The shield exists from 80 only.
- Death Grip is a taunt in the data (25 s, 3 s forced attack) and the only taunt before 58; neither guide uses it that way
  (one Taunt in wowtbc's opener).
- Dark Command is a Blood specialisation spell at 58 (8 s cooldown).
- Raise Ally (72) is in the data and in neither guide.
- wowtbc's "Rune of the Fallen Crusader" at the top of its list is a weapon enchant, not a spell; the bots have no enchant
  rule for it.
- The module's comment names `blood-tank-death-knight` as the wowtbc page; the spec URL `blood-death-knight` answers 404.
- Method's Hit 7.5 percent and Expertise 15 percent are level 90 caps.
- The Method note "Unholy Presence gives haste": data agrees (48265); not used by a tank.

## Gap list (ordered by value for levels 55 to 60 and the first dungeons)

Config (can go at once):

1. Tier 4 talent: Death Pact (column 0) instead of Death Siphon. The bots have a Death Pact line (Raise Dead, then Death Pact at
   critical health, `GenericDKStrategy.cpp:182-184`) and none for Death Siphon; wowtbc lists Death Pact among its
   defensives. Derived.
2. Tier 1 talent: Roiling Blood (column 0) instead of Plague Leech, until a Plague Leech line exists; Method names it for area
   threat and it needs no line. Derived. Link `020020`.
3. Keep tiers 2, 3, 5, 6 and all six glyphs: they are the guides' picks. The Anti-Magic Shell glyph is idle until row 17 of the
   priority lists is fixed.

Rotation (class fixes, wait for the owner's in-game test):

1. **Done (BT-134).** **Death Strike trigger** (`BloodDKStrategy.cpp:129-130`, `:97`, `DKTriggers.cpp:46`): fires on unholy runes alone, with no
   damage or health condition, and the Icy Touch filler at 5.5 takes the frost rune first. Fix: drop the Icy Touch default
   (both guides use Icy Touch only for diseases), fire Death Strike on frost and unholy runes ready with the rule above. The
   survival tool of the specialisation from level 56. Inference from relevance numbers; confirm in an action log.
2. **Done (BT-134), out of combat only, with two new triggers ("tank blood presence", "tank frost presence"); the unregistered
   `DKPresenceTrigger` stays unregistered, it is silent while any presence is up.** **Blood Presence before the pull** (`BloodDKStrategy.cpp:43-79`, `DKTriggers.h:207-213`, `GenericDKNonCombatStrategy.cpp:46-53`,
   `PullStrategy.cpp:183-229`): add an out-of-combat and in-combat presence line for the Blood spec (Blood Presence from 57,
   Frost Presence at 55 to 56); register the existing trigger. After a resurrection or a login the tank pulls without it.
3. **Done (BT-134), without Anti-Magic Shell; Army of the Dead stays unused in groups.** **Defensive thresholds** (`GenericDKStrategy.cpp:188-190`, `BloodDKStrategy.cpp:117-121`, `:145-146`): Icebound Fortitude at 50
   percent (62), Rune Tap at 60 percent (64), Vampiric Blood at 60 percent and not at 90 (76), Army of the Dead out of the
   emergency list (80), Anti-Magic Shell in PvE against a casting enemy (68, `:196-200` is PvP only).
4. **Done (BT-134), Mind Freeze only.** **Interrupts in PvE** (`GenericDKStrategy.cpp:173-177`, `:196-200`): Mind Freeze (57) on any casting enemy in reach, not only
   the current target and an enemy healer; Strangulate (58) on a caster out of reach.
5. **Done (BT-134, BT-110), for every death knight spec.** **Raise Ally missing** (ledger BT-110): model it on the druid's combat resurrection (trigger "combat party member dead" at
   `ACTION_HIGH + 9`, `strategy/Classes/druid/GenericDruidStrategy.cpp:133-134`; out of combat "party member dead",
   `GenericDruidNonCombatStrategy.cpp:124-125`; action base `ResurrectPartyMemberAction`, `strategy/actions/GenericSpellActions.h:194-205`).
   Spell 61999 from level 72, 30 Runic Power, 600 s; whether the data allows it in combat is to check live.
6. **Done (BT-134), with a stricter rule: any other hostile within 8 yards of the mark, not three within 10, sends the pull to Icy Touch;
   a caster is a mana user (the module has no other test).** **Death Grip pull rule** (`DeathKnightPullStrategy.cpp:20-21`): grip a caster or a lone mark; Icy Touch for a mark with three
   or more enemies around it.
7. **Blood Boil refresh edge below 68** (`BloodDKStrategy.cpp:142-143`, `DKTriggers.cpp:109-119`): Scarlet Fever arrives at 68; below
   it the edge spends blood runes for nothing.
8. **Death and Decay and Heart Strike in small groups** (`GenericDKStrategy.cpp:204-207`): the guides use the single target list
   with Pestilence for two or three targets and Blood Boil in place of Heart Strike from four; the bots take Blood Boil and
   Pestilence from three. Death and Decay against one or two enemies is not used.
9. **Rune Strike at 30 Runic Power** (`BloodDKStrategy.cpp:111-112`): outranks Heart Strike (5.65). To check live whether it
   starves the rune spells; Runic Corruption likes it.
10. **Soul Reaper on every target** (`BloodDKStrategy.cpp:102`): only under 35 percent health, otherwise it eats the blood rune
    (87).
11. **Missing lines**: Plague Leech, Death Siphon, Gorefiend's Grasp, Lichborne, Death's Advance (all behind a talent choice).
12. **Pull stall** (short, rare, `PullStrategy.h:41`): Death Grip on cooldown, no frost rune, below 58 no Dark Command; wait
    for the rune.
13. **Stat weights** (`Mgr/Item/StatsWeightCalculator.cpp:617-635`): Mastery 0 below 80; Strength above Expertise, Hit and
    Defense; drop Defense; Hit and Expertise only near 90.
14. To check live: Blood Shield size at 80 and above (two core scripts, `spell_dk.cpp:835-840` and `spell_mastery.cpp:188-232`);
    whether healers heal into the Purgatory shroud; Death and Decay ground placement; the first seconds of a fight without the
    presence; the action order in a debug bot (Icy Touch against Death Strike; Rune Strike against Heart Strike); units of the
    `high unholy rune` test (`DKTriggers.cpp:42-46` reads a millisecond rune cooldown, the comment at `DKTriggers.h:52-57` says
    the core exposes readiness).
