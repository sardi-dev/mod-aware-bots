# Death Knight: Frost (damage, melee)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). A pre-pull Army of the
  Dead and the remaining level gates are left for later. The "bots today" columns below describe the code before the
  2026-11-11 implementation.
- **Sources:**
  - Method, "Frost Death Knight Class Guide - MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 22 July 2025), read
    2026-11-11 through a scripted fetch (the talent builds are images; the text names swaps and one glyph set for raid and dungeon).
  - wowtbc.gg, "Frost Death Knight Rotation & Talent Build - MoP Classic" (no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent, GlyphProperties,
    SpellEffect, SpellCooldowns, SpellPower, SpellRuneCost, SpellMisc, SpellDuration, SpellAuraOptions, SpellAuraRestrictions;
    `data/db2/Item-sparse.db2` for glyph item ids) through `spec_data.py` and short read-only scripts; the core source
    (the SkyFire core: `scripts/Spells/spell_dk.cpp`, `Entities/Player/Player.cpp`, `Entities/Unit/StatSystem.cpp`,
    `Spells/Auras/SpellAuraEffects.cpp`, `SpellAuraDefines.h`) for the rune and Runic Power mechanics, the procs and the aura
    types the core does not implement.
  - Not readable: Overgear and Icy Veins (HTTP 403, as noted in the skill), Wowhead (needs a browser). Two guides were compared,
    not three.
- **Levels covered:** 55 to 70 in detail (a death knight starts at 55; leveling and 5-player dungeons, a damage dealer that
  follows the leader and the tank's marks), 71 to 90 as brief notes at the end of the priority lists.

Which source was richer: **talents** wowtbc for the options (all three per tier, one tag), Method for the swaps (Plague Leech and
Roiling Blood, Purgatory and Anti-Magic Zone, Gorefiend's Grasp and Remorseless Winter, Blood Tap and Runic Corruption); **glyphs**
Method (a core set, six alternatives, a statement that minors do not matter) against wowtbc (a standard set with the minors and two
named situational uses); the sets differ, see Glyphs; **stat order** Method only (wowtbc has none); **opener** wowtbc (eight steps
with the pre-pull timing; Method has seven); **single target priority** wowtbc (16 lines with the Killing Machine rule, the
Soul Reaper line and the disease rule; Method has 13 and does not mention Killing Machine); **area** wowtbc (it names the
four-target case; Method has two lines); **defensives** wowtbc (an ordered list of four; Method has no section);
**cooldown timing** Method (Army of the Dead only before the pull, Raise Dead and Pillar of Frost on cooldown). **Levels 55 to
89, what to do before a spell is learned, thresholds, dual wield against two-hander, interrupts, the pull, Death Strike use for a
damage dealer, and which procs work in this core**: neither guide says anything, so those lines come from the 5.4.8 data and are
marked "derived".

Where the guides disagree, and the pick:

- **Killing Machine.** wowtbc: Frost Strike on the proc (or at 90 or more Runic Power), above Howling Blast and Obliterate. Method
  does not name the proc and puts Frost Strike last, only "to spend Runic Power". The data says the proc buffs the next Obliterate
  or Frost Strike with a guaranteed critical strike (51124), so both are valid; wowtbc's order is kept (it is the only one that uses
  the proc, and it fits the dual wield build the calculator assumes). The bots follow wowtbc (`DKFrostStrikeProcTrigger`).
- **Plague Leech.** Method puts it at step 9 (two spent runes, Blood Plague under 3 s), wowtbc at step 14, after the rune spenders.
  Both need it to be a rune refund; below level 81 there is no Outbreak, so the diseases come back through Howling Blast and Plague
  Strike (derived: net rune neutral). Not used by the bots at all.
- **Blood Tap charges.** Method: 10 or more; wowtbc: 9 or more. The core lets the spell fire from 5 charges (cost 5, cap 12,
  `spell_dk.cpp:131`, `:1253`); the guides' 9 and 10 are tuning for level 90, not a rule.
- **Order of the disease line.** Method keeps the diseases above Army of the Dead and Raise Dead; wowtbc puts them after the pre-pull
  cooldowns and the Empower Rune Weapon line. Both mean the same in play (the cooldowns are cast before the pull). Method's order is
  used for the list below.
- **Talents.** The module's premade follows wowtbc's page (Plague Leech, Anti-Magic Zone, Asphyxiate, Death Pact, Blood Tap,
  Remorseless Winter); Method's default is Plague Leech, Purgatory, Gorefiend's Grasp at 90, with Blood Tap or Runic Corruption.
  Questions 1 to 4.
- **Hit and Expertise.** Method: both to about 7.5 percent. The caps are level 90 numbers; the module's calculator uses 8 percent
  melee hit and **26** for expertise (`Mgr/Item/StatsWeightCalculator.h:25-28`), a WotLK number that no 5.4.8 character reaches
  (gap 12).

## Talents

Tiers and columns are from Talent.dbc. This server opens tier N at level 15 x (N + 1) for every class (`Player::CalculateTalentsPoints`
is floor(level / 15); `BotFactory::InitTalentTiers`, `Factory/BotFactory.cpp:1272-1285`, learns one talent per open tier), so a
level 55 death knight has **tiers 1 to 3 at once**, the fourth at 60, then 75 and 90. "Bots today" is
`AiPlayerbot.PremadeSpecLink.6.1 = 112001` (`conf/playerbots.conf.dist:2798`; one digit per tier, column counted from 0): Plague
Leech, Anti-Magic Zone, Asphyxiate, Death Pact, Blood Tap, Remorseless Winter. The talent-override swap (`BotOverrideSpell`,
`AI/PlayerbotAI.cpp:3741`) matters for Frost: **Asphyxiate replaces Strangulate**, so the premade's PvP "strangulate" lines cast
Asphyxiate (a 5 s stun, 30 s).

| Tier (opens at) | Guide pick | Alternative and when | Spell id | Bots today (112001) |
|-----------------|------------|----------------------|----------|---------------------|
| 1 (55) | Plague Leech (Method default; wowtbc lists all three) | Roiling Blood for heavy AoE (Method); Unholy Blight (wowtbc lists only) | 123693 / 108170 / 115989 | Plague Leech (column 1), same; **no bot line casts it** (gap 8) |
| 2 (55) | Purgatory (Method default) | Anti-Magic Zone as a raid defensive (Method, wowtbc tag "Raid CD"); Lichborne (wowtbc lists only) | 114556 / 51052 / 49039 | **Anti-Magic Zone (column 1), differs from Method**; the action exists (`DKActions.h:173`) but **no trigger casts it** (search of `src/`), so the pick is inert |
| 3 (55) | not named by either guide | Death's Advance, Chilblains, Asphyxiate | 96268 / 50041 / 108194 | Asphyxiate (column 2); only PvP lines would use it |
| 4 (60) | not named by Method; Death Pact is in wowtbc's defensive list | Death Siphon, Conversion | 48743 / 108196 / 119975 | Death Pact (column 0), same as wowtbc; the line exists (`GenericDKStrategy.cpp:182-184`) |
| 5 (75) | Blood Tap (wowtbc "Standard" for Unholy; Method: best damage, harder) | Runic Corruption (Method: easier, recommended for newer players and challenge modes), Runic Empowerment | 45529 / 51462 / 81229 | Blood Tap (column 0), same; **no Frost line casts it** (Blood has one, `BloodDKStrategy.cpp:115-116`; gap 7) |
| 6 (90) | Gorefiend's Grasp (Method: better in dungeons, rarely swapped) | Remorseless Winter if Gorefiend's is not needed (Method), Desecrated Ground | 108199 / 108200 / 108201 | Remorseless Winter (column 1), the wowtbc list entry; no bot line casts any of the three |

Notes on the picks (data and code, not guide):

- **Plague Leech** (25 s, 30 yd): consumes both diseases on the target and turns two depleted runes into Death Runes. Method pairs it
  with Outbreak (learned at 81). Below 81 the diseases are reapplied by a Howling Blast (frost or death rune) and a Plague Strike
  (unholy rune), two runes for two runes (derived), so the talent buys nothing before 81 and has no bot line.
- **Roiling Blood** (passive): Blood Boil also casts Pestilence on a diseased target. The bots already cast Blood Boil and
  Pestilence with three or more enemies (`GenericDKStrategy.cpp:212-215`), so the pick needs no new line. Derived pick for the bots.
- **Purgatory** (passive): a fatal hit becomes a Shroud of Purgatory that absorbs healing equal to the damage prevented; the death
  knight dies if the absorb is left when it ends, and a lockout (123981) stops a repeat. Passive, so it needs no line. Whether the
  bots' healers heal into the shroud is **to check live** (same open item as the Blood sheet).
- **Anti-Magic Zone** (2 min, placed anywhere, 3 s in this data, as Method says): cuts spell damage in the zone by 40 percent. A 3 s
  window needs a human to time it; no bot line exists. In a 5-player group with bots the pick does nothing.
- **Death's Advance** (passive part: +10 percent movement speed and no slow below 70 percent, spell 124285; active: +30 percent
  speed and no slow for 6 s, 30 s cooldown): the passive part helps a melee damage dealer keep up with the group and stay on target. Derived pick. Chilblains
  (passive: Frost Fever slows, Chains of Ice roots) is the other passive option.
- **Runic Corruption** (passive, 45 percent proc on a Death Coil, Frost Strike or Rune Strike hit: +100 percent rune regeneration
  for 3 s, spell 51460 effects of aura 110): a Frost Strike dump feeds it. **Blood Tap** needs 5 charges (2 per Death Coil, Frost
  Strike or Rune Strike, cap 12) and a depleted rune; it is one trigger line for the bot (the Blood spec has it).
- Gorefiend's Grasp (60 s) pulls every enemy around a target (hostile or friendly) to it; a human leader's pull plan does not
  expect a damage dealer that drags a room (derived), so the talent stays idle even if a line existed.

Proposed link (the guides' dungeon picks with two derived changes): **`020001`** (Roiling Blood / Purgatory / Death's Advance /
Death Pact / Blood Tap / Remorseless Winter). Only tiers 1, 2 and 3 change; tier 5 follows question 4 (`020021` with Runic
Corruption).

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`, effect text from Spell.dbc. Slots unlock in pairs
at 25, 50 and 75; each configured glyph goes to the first free slot of its type, so a death knight of 55 to 74 has **four slots**
(two major, two minor) and the third pair joins at 75. "Bots today" is `PremadeSpecGlyph.6.1 = 43541,45806,43548,43550,104101,43533`
(`conf.dist:2799`): Death Grip, Tranquil Grip, Pestilence, Army of the Dead, the Long Winter, Anti-Magic Shell.

| Type | Glyph | Item id | Why (one clause) | Bots today |
|------|-------|--------:|------------------|------------|
| Major | Pestilence | 43548 | wider Pestilence radius; Method's core glyph, wowtbc standard; the bots cast Pestilence with three or more enemies | yes (slot 3, level 50) |
| Major | Dark Succor | 68793 | derived: after a kill that yields experience, in Frost or Unholy Presence, the next Death Strike within 15 s is free and heals at least 20 percent of maximum health (spell 101568); the existing Death Strike lines use it unchanged | no |
| Major | Anti-Magic Shell | 43533 | Method core, wowtbc standard (absorbs all magic damage up to the limit); **the bots cast the Shell in PvP only**, so it is idle in PvE (gap 11) | yes (slot 6, level 75) |
| Major | Death Grip | 43541 | wowtbc standard (+5 yd); **no Frost bot line casts Death Grip** (search of `src/`: only the Blood pull and the Blood taunt fallback), so it is inert | yes (slot 1, level 25) |
| Major | Icebound Fortitude | 43536 | wowtbc "burst damage reduction" (shorter cooldown, shorter duration, spell 58673) | no |
| Major | Regenerative Magic, Loud Horn, Outbreak, Festering Blood, Dark Simulacrum | 104048, 104047, 43826, 104049, 45800 | Method alternatives (Regenerative Magic needs the Shell line, Outbreak has no Frost line, Festering Blood is an AoE pick) | no |
| Major | Mind Freeze, Death and Decay, Pillar of Frost, Death Coil | 43549, 43542, 43553, 45804 | not in either guide: cooldown for runic power, a slow, immunity to loss of control with a slow, a shield on allies | no |
| Minor | Army of the Dead | 43550 | wowtbc standard; the ghouls no longer taunt, and the bots cast Army of the Dead in hard fights | yes (slot 4, level 50) |
| Minor | Tranquil Grip | 45806 | wowtbc standard; Death Grip no longer taunts (only matters if a grip line exists) | yes (slot 2, level 25) |
| Minor | the Long Winter | 104101 | wowtbc standard; Horn of Winter lasts an hour (the data: 5 minutes otherwise) | yes (slot 5, level 75) |
| Minor | Path of Frost, Death Gate, Resilient Grip, Horn of Winter, the Geist, the Skeleton, Death's Embrace, Foul Menagerie, Corpse Explosion | 43671, 43673, 43672, 43544, 43535, 104099, 43539, 43551, 43827 | cosmetic, travel or teaching; the Geist and Skeleton change Raise Dead | no |

Disagreement: Method's dungeon set is Pestilence, Anti-Magic Shell, Loud Horn; wowtbc's is Death Grip, Pestilence, Anti-Magic Shell
with Tranquil Grip, Army of the Dead and the Long Winter. The premade is wowtbc's. **Proposed (derived): `43548,43550,68793,45806,104101,43533`**:
Death Grip (inert) replaced by Dark Succor, and Army of the Dead moved into the level 25 minor slot so that it is active from
the first pair; Tranquil Grip, Long Winter and the Shell stay. Question 6.

## Stat order

Method: Hit and Expertise to about 7.5 percent each, then Strength, Mastery, Critical Strike, Haste. wowtbc has no stat order. The
caps are level 90 numbers.

For loot rolls and quest rewards from 55 to 89 (derived, with the reason):

- **Plate** (Plate Specialization, level 55, adds Stamina only in all-plate, so keep a full set) and **Strength first**: Strength is
  the damage stat, Pillar of Frost adds 20 percent of it at 68 (51271), and no Agility or Intellect does anything for the
  specialisation.
- **Hit and Expertise only up to the caps and only near level 90** (Method). Below 90 the caps are not reachable with the gear of
  levels 55 to 89 in practice (inference); the calculator's overflow values (8 percent, 26) are not the 5.4.8 numbers (gap 12).
- **Mastery is worth nothing before level 80**: Mastery: Frozen Heart (77514, +frost damage) is learned at 80 (SpecializationSpells).
  Method ranks it second after Strength at 90; it must not outrank anything before 80.
- **Haste is better than Method's last place** in this core (derived from reading the formula): the rune recovery time is
  10 s times (1 minus the haste percent), and the haste term sums haste rating and the melee haste auras (`Player.cpp:21210-21231`),
  so Haste speeds the runes as well as the swing; Critical Strike gives Killing Machine crits and Frost Strike crits. Treat the two as
  equals (live trace would settle it).
- **Dual wield or two-hander:** see "Weapons" below; neither guide says anything.

Bots today (`Mgr/Item/StatsWeightCalculator.cpp:465-477`): Strength 2.5, Agility 0.5, Attack Power 0.5, **Armor Penetration 2.7**,
Hit 2.3, Critical 2.2, Haste 2.1, **Mastery 2.4 at every level**, Expertise 2.5, Spell Power -1.0, Melee DPS 7.0. Gaps: armor
penetration rating does not exist in 5.4.8 (derived; the weight is dead), Mastery counts before level 80 (the other specs'
blocks already use `lvl >= 80 ? x : 0`, e.g. lines 380, 417, 518), Hit and Expertise above Strength. Proposed (derived): Strength
2.5, Critical 2.2, Haste 2.1, Hit 1.9, Expertise 1.9, **Mastery 0 below 80 and 2.4 from 80**, drop Armor Penetration; keep Attack Power
0.5 and Melee DPS 7.0.

### Weapons (derived)

Data: **Threat of Thassarian** (66192, level 74) makes Death Strike, Obliterate, Plague Strike and Frost Strike also hit with the
off-hand and adds 50 percent to Frost Strike when dual-wielding; **Might of the Frozen Wastes** (81333, level 74) adds 40 percent to
Obliterate and 30 percent to the other strikes with a two-hander, and its autoattack part (aura 344) is **not implemented in this core**
(`SpellAuraEffects.cpp:394`, `SpellAuraDefines.h:391`). Before 74 neither exists: a dual wielder's off-hand adds autoattacks only, a
two-hander's strikes use the bigger weapon. The calculator forces dual wield one-handers for Frost (`StatsWeightCalculator.cpp:821`
multiplies a two-hander by 0.05, and `:1157-1162` prefers a slow one-hander); the data cannot say which is better at 55 to 73.
Question 5. The Dual Wield skill for death knights is assumed (the module checks `CanDualWield` on equip, `EquipAction.cpp:208`).

## What the specialisation knows, by level (5.4.8 data)

A Frost death knight knows the death knight class line (SkillLineAbility 796) plus the Frost rows of SpecializationSpells (spec 251).
Ids are the spells the player casts. Rune costs are from SpellRuneCost in the data's order (blood, unholy, frost, death; the core's
`RUNE_BLOOD 0, RUNE_UNHOLY 1, RUNE_FROST 2, RUNE_DEATH 3`); Runic Power costs are tenths in SpellPower (350 is 35). The global
cooldown is **1.0 s**.

| Level | Spell (id, cost, cooldown) |
|------:|----------------------------|
| 55 | **Frost Presence** (48266: the only presence until 57; Runic Power gain +20 percent is aura 329, **not implemented in this core**; crowd control duration reduced); **Howling Blast** (49184, 1 frost rune, +10 RP, no cooldown, 30 yd, the target and 50 percent to others near it, Frost Fever on every target); **Frost Strike** (49143, **35 RP**, no rune, 115 percent weapon damage; replaces Blood Strike); Icy Touch (45477, frost rune, 30 yd, Frost Fever); Plague Strike (45462, unholy rune, melee, Blood Plague); Death Coil (47541, 40 RP, 40 yd); Death Grip (49576, 25 s, 30 yd, taunts 3 s); Death Gate (50977); passives **Blood of the North** (54637: both blood runes are Death Runes), **Icy Talons** (50887: melee haste, data +45 on aura 319, which the rune formula counts), Plate Specialization (86113); tiers 1 to 3 open |
| 56 | **Death Strike** (49998, unholy and frost rune, 185 percent weapon damage, +20 RP; heals at least 7 percent of maximum health, and the 5 s damage record only counts in Blood Presence); Blood Boil (48721, blood or death rune, area, 50 percent more on diseased targets); Pestilence (50842, blood or death rune, spreads the diseases); Raise Dead (46584, free, 120 s, a ghoul for 60 s) |
| 57 | Blood Presence (48263); **Mind Freeze** (47528, free, 15 s, melee range, 4 s school lock, no global cooldown) |
| 58 | **Obliterate** (49020, unholy and frost rune, **250 percent weapon damage plus 12.5 percent per own disease**, +20 RP); Chains of Ice (45524, frost rune, 60 percent slow for 8 s, Frost Fever); Strangulate (47476, blood rune, 30 yd, 60 s, 5 s silence; replaced by Asphyxiate with the tier 3 talent) |
| 60 | **Death and Decay** (43265, unholy rune, 30 s, 10 yd ground, 10 s); Unholy Aura (55610, passive party aura: melee and ranged attack speed); tier 4 talent |
| 62 | Icebound Fortitude (48792, 180 s, 12 s, 20 percent less damage, stun immunity) |
| 63 | **Killing Machine** (51128 with the buff 51124, 10 s): autoattacks sometimes make the next Obliterate or Frost Strike a certain critical strike |
| 64 | Unholy Presence (48265, haste +10 percent; not for Frost) |
| 65 | **Horn of Winter** (57330, 20 s, +10 RP, attack power +10 percent for the party within 100 yd, 5 min); **Improved Frost Presence** (50385: Frost Strike costs 15 less, so **20 RP** in Frost Presence) |
| 66 | Brittle Bones (81328: Frost Fever adds physical vulnerability, a group utility); Path of Frost (3714) |
| 68 | **Pillar of Frost** (51271, 1 frost rune, 60 s, 20 s, **+20 percent Strength**, no global cooldown); Anti-Magic Shell (48707, 45 s, 5 s, absorbs 75 percent of spell damage up to 50 percent of maximum health) |
| 70 | **Rime** (59057: Obliterate has a 45 percent chance to make the next Howling Blast or Icy Touch cost no runes and give no RP, aura "Freezing Fog" 59052, 15 s) |
| 72 | Raise Ally (61999, 30 RP, 40 yd, 600 s) |
| 74 | Threat of Thassarian (66192); Might of the Frozen Wastes (81333) |
| 75 | tier 5 talent |
| 76 | Empower Rune Weapon (47568, 300 s, all runes ready, +25 RP) |
| 80 | Mastery: Frozen Heart (77514); Army of the Dead (42650, one rune of each type, 600 s, 4 s channel) |
| 81 | Outbreak (77575, 60 s, 30 yd, free, both diseases) |
| 83 | Necrotic Strike (73975, all specs) |
| 87 | Soul Reaper, Frost (130735, unholy rune, 6 s, extra shadow damage after 5 s on a target below 35 percent health) |
| 90 | tier 6 talent |

**Resources (derived, arithmetic on the rows above).** Six runes: **two Death Runes** (the blood slots, Blood of the North), two
unholy, two frost. Base recovery is 10 s per rune; Icy Talons' +45 on the haste term would make it **5.5 s** (the core multiplies
by 1 minus the haste percent), plus haste rating. That is about 1.1 runes per second against one global cooldown per second, so a
Frost death knight has runes for most globals and is limited by the 1 s global cooldown, not by the runes (inference from the
formula; a live trace of rune cooldowns would confirm). Each rune spent gives 10 Runic Power (20 for a two-rune spell), so the
dump is **Frost Strike: 35 RP before 65, 20 RP from 65** (Frost Presence). Value from the spell effects (weapon percent, not a
simulation): Obliterate 275 for two runes with both diseases (about 137 per rune), Death Strike 185 for two runes, Frost Strike 115
for 35 or 20 RP and no rune, Howling Blast a Frost spell (150 plus 0.738 attack power) for one rune with an area part. So
**Obliterate is the best two-rune button, Frost Strike the best use of Runic Power, and Death Strike a 33 percent smaller
Obliterate that heals only the 7 percent minimum outside Blood Presence** (derived).

## How the bots run this specialisation (read)

- A Frost death knight gets the `frost` combat strategy (`FrostDKStrategy`, `Factory/AiFactory.cpp:526`) with `frost aoe` and
  `dps assist`, the generic list with `boost` (`:316`), `behind` for melee damage dealers (`:569`), `threat` (`:559`), and out of
  combat `nc` and `dps assist` (`:777-781`). There is **no `cc`, `cure` or `pull` strategy** for a damage dealer (the pull is
  the Blood tank's, `DeathKnightPullStrategy.cpp`), so the DPS death knight never uses Death Grip or Chains of Ice in PvE
  (verified: the only "death grip" casts are the Blood pull and the Blood taunt fallback).
- Rank of an action (`strategy/Strategy.h`): ACTION_DEFAULT 5, NORMAL 10, HIGH 20, INTERRUPT 40, EMERGENCY 90. The engine takes the
  highest action that is useful and possible and **tries the next one when a cast fails in the same tick**, so an unaffordable
  button costs no time (as in the monk sheet).
- Default list (`FrostDKStrategy.cpp:79-95`): Soul Reaper 5.75, Obliterate 5.72, Death Strike 5.7, Frost Strike 5.4, Horn of Winter
  5.1, melee 5.0. Edges, relevance in brackets: Army of the Dead (26, `boost`, `:130-131`); Frost Strike at Runic Power 89 percent or
  a Killing Machine proc (5.95, `:114-115`, `DKTriggers.cpp:122-125`); Pillar of Frost (6.05, `:105-106`); Howling Blast with both
  frost-slot runes ready within 2 s (5.9, `:117-118`, `DKTriggers.cpp:45`); Howling Blast on Freezing Fog (5.72, `:111-112`); **Death
  Strike under 90 percent health (5.8, `:121-122`)**; Raise Dead in combat (11, `:124-125`); "blood strike" with both blood-slot
  runes ready (5.2, `:127-128`, which for Frost is Frost Strike after the spec swap); Howling Blast then Icy Touch when Frost Fever
  is missing (22, `:134-136`); Plague Strike when Blood Plague is missing (22, `:137-138`); Howling Blast with three or more enemies
  within 8 yd (24, `:143-147`). Shared (`GenericDKStrategy.cpp`): Mind Freeze on the target and on an enemy healer (21, `:173-177`);
  Horn of Winter (11, `:178-179`); Raise Dead then Death Pact at critical health (26 and 25, `:182-184`); Icebound Fortitude (25)
  and Rune Tap (24) under 40 percent (`:189-192`); Raise Ally on a dead member (29, `:197-198`); Death and Decay (29), Pestilence
  (14), Blood Boil (13) with three or more enemies (`:212-215`); Empower Rune Weapon with every rune on cooldown (21, `:226-227`);
  PvP only: Mind Freeze and Strangulate on any caster, Anti-Magic Shell, Chains of Ice (`:204-211`). Out of combat
  (`GenericDKNonCombatStrategy.cpp:276-289`): sharpening stone, Horn of Winter (21), Bone Shield (inert for Frost); the two presence
  edges are the tank's.
- **Presences** are applied only as a prerequisite of single actions: Icy Touch, Obliterate, Frost Strike, Howling Blast and Rune
  Strike carry "frost presence" (`FrostDKStrategy.cpp:33-71`); Pillar, Plague Strike and Death Strike carry none. Nothing keeps a
  presence up out of combat for a damage dealer. An unlearned presence is skipped (`Engine.cpp:206-217`, as in the Blood sheet).
- Skill tiers: no Frost line is gated by `BotSkill` (it appears only in the Mind Freeze pack caster action for Blood).

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked "derived" comes
from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to `mod-aware-bots-548/src/strategy/Classes/deathknight/`
at commit `06b4020`. `FS` is `FrostDKStrategy.cpp`, `GS` is `GenericDKStrategy.cpp`, `TR` is `DKTriggers.cpp`, `AC` is `DKActions.h`.

### Levels 55 to 57

Known: Frost Presence (the only presence), Howling Blast, Frost Strike, Icy Touch, Plague Strike, Death Coil, Death Gate, Blood of
the North, Icy Talons; 56 Death Strike, Blood Boil, Pestilence, Raise Dead; 57 Blood Presence, Mind Freeze. Three talent tiers
open at 55. **No Obliterate (58), no Killing Machine (63), no Death and Decay (60), no Horn of Winter (65), no defensive cooldown.**
At 56 and 57 Death Strike is the only two-rune attack, so it is a damage button here and not just a heal (derived).

| # | Tier | Condition | Spell (id, learned at) | Bots today (file:line or gap) |
|--:|------|-----------|------------------------|-------------------------------|
| 1 | core | before the first attack and after every resurrection | Frost Presence (48266, 55) | partly: prerequisite of Icy Touch, Frost Strike, Howling Blast (`FS:33-71`); cast by the first of them, in combat. No out-of-combat line (gap 5) |
| 2 | core | Frost Fever missing (30 s) | Howling Blast (49184, 55), else Icy Touch (45477, 55) | yes: 22 (`FS:134-136`); Howling Blast also puts the disease on every target near the first |
| 3 | core | Blood Plague missing | Plague Strike (45462, 55) | yes: 22 (`FS:137-138`) |
| 4 | core | a frost or death rune ready (derived: two frost and two death runes cannot all be spent on diseases) | Howling Blast (49184, 55) | partly: 5.9 only when **both frost slots** are ready within 2 s (`TR:45`); the two Death Runes are never read (gap 6) |
| 5 | core | two runes (unholy and frost) ready, 56 | Death Strike (49998, 56): attack and minimal heal | yes: default 5.7 (`FS:92`); at 5.8 under 90 percent health (`FS:121-122`), see row 11 |
| 6 | core | Runic Power 35 or more, nothing higher castable | Frost Strike (49143, 55) | yes: default 5.4 (`FS:93`) and 5.95 at 89 percent (`FS:114-115`) |
| 7 | good | an enemy casts, 57 | Mind Freeze (47528, 57), melee range, 15 s | partly: only the current target and an enemy healer (`GS:173-177`); a caster that is not the target is ignored in PvE (gap 9) |
| 8 | good | three or more enemies within 8 yd of the target | Howling Blast (24, `FS:143-147`), then Pestilence (50842, 56) and Blood Boil (48721, 56) | yes (`FS:143-147`, `GS:212-215`); Pestilence needs an own disease on the target (`DKActions.cpp:46-56`) |
| 9 | good | off cooldown, in a fight that will last, 56 | Raise Dead (46584, 56): a ghoul for 60 s, then 60 s of cooldown | yes: 11 (`FS:124-125`); the "3 minute stamp" of the action is an epoch time (gap 4) |
| 10 | good | health under 50 percent, a rune pair ready | Death Strike heal | see row 11 |

### Levels 58 to 62

As above, plus Obliterate, Chains of Ice, Strangulate (58), Death and Decay and the tier 4 talent (60), Unholy Aura (60, a party
attack speed buff from the Frost and Unholy specialisations), Icebound Fortitude (62).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 11 | core | unholy and frost (or death) runes ready | **Obliterate** (49020, 58) in place of Death Strike | **wrong order:** the Death Strike edge at 5.8 under 90 percent health sits above Obliterate at 5.72 (`FS:88`, `FS:121-122`), and the two cost the same runes, so a Frost death knight that is not at full health casts Death Strike where Obliterate belongs: 33 percent less damage, no Rime roll, no Killing Machine crit, for the 7 percent minimum heal (gap 1) |
| 12 | core | Runic Power 35 or more | Frost Strike (49143, 55) | yes (row 6) |
| 13 | core | three or more enemies, 60 | Death and Decay (43265, 60) at the pack | yes: 29 (`GS:212-215`); Unholy rune cost, 10 s, 30 s cooldown |
| 14 | core | health under 40 percent | Icebound Fortitude (48792, 62) | yes: 25 (`GS:189-192`) |
| 15 | good | health under 40 percent, a rune pair ready, in a fight (derived: the minimum heal is 7 percent in Frost Presence) | Death Strike | partly: the 90 percent edge replaces it (row 11) |
| 16 | expert | a mob that runs from the group at low health (derived) | Chains of Ice (45524, 58): 60 percent slow, Frost Fever | missing (PvP only: `GS:210-211`, `DKTriggers.cpp:132-142`) |

### Levels 63 to 67

As above, plus Killing Machine (63), Unholy Presence (64, not for Frost), Horn of Winter and Improved Frost Presence (65), Brittle
Bones and Path of Frost (66).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 17 | core | Killing Machine up (51124) or Runic Power 90 or more | Frost Strike (49143): a certain critical strike | yes: 5.95 (`FS:114-115`, `TR:122-125`) |
| 18 | core | Runic Power 20 or more from 65 (Frost Presence) | Frost Strike | yes (row 6); the cost fell from 35 to 20 at 65 and the 89 percent edge no longer matters |
| 19 | good | the buff is missing (10 percent attack power for the party) | Horn of Winter (57330, 65) | yes: out of combat 21 (`GenericDKNonCombatStrategy.cpp:278-279`), in combat 11 (`GS:178-179`), default 5.1 |
| 20 | good | Frost Fever and Blood Plague on every pack target | Pestilence (50842) before Blood Boil | yes (row 8) |

### Levels 68 to 70

As above, plus Pillar of Frost and Anti-Magic Shell (68), Rime (70).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 21 | core | off cooldown, in a fight that will last (both guides: on cooldown; derived: not on a mob that dies in 5 s) | **Pillar of Frost** (51271, 68): +20 percent Strength for 20 s | partly: 6.05 on cooldown in any fight (`FS:105-106`); unknown below 68 (see the unknown-name table) |
| 22 | core | Freezing Fog (Rime) up | **Howling Blast** (49184) free, above Obliterate (Method step 10, wowtbc step 11) | **wrong order:** 5.72, equal to Obliterate (`FS:111-112`, `:88`); the engine order of equals is **to check** (gap 3) |
| 23 | core | unholy and frost (or death) runes ready | Obliterate | yes (row 11, once the Death Strike edge is fixed) |
| 24 | core | Frost Strike | as rows 12 and 17 | yes |
| 25 | good | casters hit the group with spells, Mind Freeze used | Anti-Magic Shell (48707, 68) | PvP only (`GS:204-208`, gap 11) |

### Levels 71 to 90, brief

| Level | Guide and data | Bots today |
|------:|----------------|------------|
| 72 | Raise Ally (61999, 30 RP, 600 s) for a dead member | yes (`GS:197-198`, BT-134) |
| 74 | Threat of Thassarian and Might of the Frozen Wastes: dual wield or two-hander becomes a real choice | the calculator forces one-handers (question 5) |
| 75 | tier 5 talent: Blood Tap (9 or 10 or more charges, a depleted rune) | the premade takes it, **no Frost line** (gap 7) |
| 76 | Empower Rune Weapon with every rune on cooldown and Runic Power not capping (wowtbc) | yes (`GS:226-227`), no Runic Power check |
| 80 | Mastery: Frozen Heart; Army of the Dead only before the pull (Method) | passive; Army of the Dead is cast at 26 in a hard fight (`FS:130-131`), a 4 s channel in combat |
| 81 | Outbreak instead of Howling Blast and Plague Strike for the diseases (wowtbc opener); Plague Leech | **no Frost Outbreak line** (the Unholy and Blood strategies have `dk outbreak`); the disease edges keep both diseases up (gap 8) |
| 87 | Soul Reaper below 35 percent health (wowtbc) | **wrong:** default 5.75 on any target (`FS:91`), eating a rune pair before Obliterate (gap 10) |
| 90 | Method: Frost Presence, Horn, diseases, Army, Raise Dead, Empower Rune Weapon, Pillar, Blood Tap, Plague Leech, Howling Blast on Rime, Obliterate on unholy runes, Howling Blast, Frost Strike | the skeleton matches; the tier 6 talent has no line |

## Cooldowns and defensives

The guides give an order for the defensives (wowtbc: Icebound Fortitude, Anti-Magic Shell, Death Pact, Anti-Magic Zone); the
thresholds are derived because dungeon trash needs numbers. Thresholds are the death knight's own health percent.

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Pillar of Frost | 68 | on cooldown (both) | on cooldown when the pull is an elite, a pack of three or more, or the bot has "balance" 50 or less | on cooldown in any fight |
| Raise Dead | 56 | on cooldown / pre-pull | on cooldown in a fight; a ghoul lasts 60 s | edge 11 (see gap 4) |
| Empower Rune Weapon | 76 | all runes on cooldown and Runic Power not capping | the same | all runes on cooldown only |
| Army of the Dead | 80 | only before the pull (Method, wowtbc "pre-pull") | before a hard pull, as an expert line; not in the middle of a fight | `boost` edge 26: a 4 s channel in a hard fight |
| Blood Tap | 75 talent | 9 or 10 or more charges, a depleted rune | 5 or more charges and a depleted rune (the core's own limit) | no Frost line |
| Soul Reaper | 87 | below 35 percent (wowtbc) | target below 35 percent health and an unholy rune free | on any target |
| Icebound Fortitude | 62 | wowtbc defensive 1 | health under 50 percent, or a boss hit | under 40 percent |
| Anti-Magic Shell | 68 | wowtbc defensive 2 | an enemy caster hits the group and Mind Freeze is used | PvP only |
| Death Pact | 60 talent | wowtbc defensive 3 | critical health after Raise Dead | yes: Raise Dead then Death Pact (`GS:182-184`) |
| Death Strike | 56 | not named | health under 50 percent, or after a kill with the Dark Succor glyph | health under 90 percent |
| Mind Freeze | 57 | not named | any casting enemy in reach (15 s) | the target and an enemy healer |
| Strangulate (Asphyxiate) | 58 | not named | a caster out of reach (expert) | PvP only |
| Chains of Ice | 58 | not named | a fleeing mob (expert) | PvP only |
| Horn of Winter | 65 | upkeep | out of combat and in combat when missing | yes |

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Obliterate arrives at 58, Death and Decay at 60, Killing Machine at 63, Horn of Winter at 65, **Pillar of
  Frost at 68**, **Rime at 70**, Empower Rune Weapon at 76, Army of the Dead at 80, Outbreak at 81, Soul Reaper at 87. Below those
  levels the lists start without them. The server opens three talent tiers at 55 and the others at 60, 75 and 90.
- **Frost Presence's Runic Power bonus does nothing in this core** (aura 329 is listed as not implemented); the presence is still
  right for Frost Strike (-15 RP from 65), the Death Strike record is zero outside Blood Presence in any case, and nothing else
  asks for another presence. The guides say "stay in Frost Presence" without a reason.
- Plague Leech is a rune refund only with Outbreak (81); the guides rank it for level 90.
- **Blood Tap fires from 5 charges** in the core; the guides' 9 and 10 are not a game rule.
- Frost Strike costs 35 Runic Power until 65 and 20 from 65 (data); the guides' "spend at 90 or more" assumes the cap is the reason.
- Death Strike is a damage button at 56 and 57 and a 7 percent heal in Frost Presence; the guides use it as neither.
- Method's Hit and Expertise caps (7.5 percent) are level 90 numbers; the module's calculator uses 8 and 26.
- wowtbc's "Rune of the Fallen Crusader" (a runeforged weapon enchant, spell 53344 learned at 70) is not a spell rotation line; the
  bots have no enchant rule for it. Not proposed.
- Overgear, Icy Veins and Wowhead were not readable, so two guides were compared.
- Method's and wowtbc's consumables, gear lists and Engineering notes have no counterpart in the bots (design decision 0016 to 0021).

## Spell names the Frost strategies ask for that a Frost death knight of level 55 to 70 does not know

`SpellIdValue` logs `[SID] ... qualifier='<name>' resolved=0` whenever a name matches no known, non-passive spell
(`strategy/value/SpellIdValue.cpp:147`) and caches the answer 20 s per name and bot (`:24`). From the code (inference, not checked
against the log), the names a Frost death knight asks for and does not know. Aura-name triggers (Freezing Fog, Killing Machine) do
not go through this value.

| Name | Known from (Frost) | Asked from |
|------|--------------------|------------|
| `obliterate` | 58 | default list, 55 to 57 (`FS:88`) |
| `death strike` | 56 | default list, edge at 55 (`FS:92`, `:121-122`) |
| `soul reaper` | 87 | default list, 55 to 86 (`FS:91`) |
| `horn of winter` | 65 | default list and edges, 55 to 64 (`FS:94`, `GS:178-179`) |
| `pillar of frost` | 68 | edge, 55 to 67 (`FS:105-106`) |
| `raise dead` | 56 | edge, 55 (`FS:124-125`) |
| `army of the dead` | 80 | `boost` edge, 55 to 79 (`FS:130-131`) |
| `icebound fortitude` | 62 | `dk health 40`, 55 to 61 (`GS:189-192`) |
| `rune tap` | **never** (Blood only, 64) | the same edge, every check (`GS:189-192`) |
| `death pact` | talent 60 | critical health, 55 to 59 and with another tier 4 pick (`GS:182-184`) |
| `mind freeze` | 57 | target and healer edges, 55 and 56 (`GS:173-177`) |
| `death and decay`, `pestilence`, `blood boil` | 60, 56, 56 | the area edge, 55 to 59 (`GS:212-215`) |
| `empower rune weapon` | 76 | `no rune` edge, 55 to 75 (`GS:226-227`) |
| `raise ally` | 72 | `combat party member dead` edge, 55 to 71 (`GS:197-198`) |
| `anti-magic shell`, `strangulate`, `chains of ice` | 68, 58, 58 | PvP edges only (`GS:204-211`) |
| `blood tap` | talent 75 | no Frost line today (a line would ask it from 55) |

Fix direction: a level and talent test on the default list entries and on each edge (the same fix the paladin and monk sheets
propose), drop `rune tap` from the Frost and Unholy `dk health 40` edge, and the `resolved=0` count per bot should fall to near zero.

## Gap list (ordered)

Ranked by value for levels 55 to 70 and the first dungeons. Config items can go at once; rotation and strategy items are class fixes
and wait for the owner's in-game test.

1. **Rotation, Death Strike displaces Obliterate** (`FrostDKStrategy.cpp:121-122`, `:88`): the `dk health 90` edge is at 5.8, above
   Obliterate at 5.72, and both cost one unholy and one frost rune. Death Strike deals 185 percent weapon damage against 275 for
   Obliterate with two diseases, loses the Rime roll and the Killing Machine crit, and heals only the 7 percent minimum outside Blood
   Presence (`spell_dk.cpp:812-840`, `:874-882`). Fix: lower the edge to health under 50 percent (or use the Dark Succor aura, see
   gap 14); keep the default 5.7 as the 56 to 57 attack. Inference from the relevance numbers; confirm in an action log.
2. **Rotation, unknown-name spam** (table above): level and talent gates on the default list entries and on each edge; delete `rune tap`
   from the Frost and Unholy health edge (`GenericDKStrategy.cpp:189-192`). Verify with the debug log: `resolved=0` lines per bot fall to near zero.
3. **Rotation, Rime Howling Blast above Obliterate** (`FrostDKStrategy.cpp:111-112`): raise the Freezing Fog edge to 5.95 (both
   guides rank it above Obliterate).
4. **Rotation, Raise Dead stamp** (`DKActions.cpp:33-44`): `bot->AddSpellCooldown(spellId, 0, 3 * 60 * 1000)` passes 180000 as the end
   time, but the third argument is an absolute time in seconds (`Player.h:2206`, `compat/SpellHistory.h:52-55` converts milliseconds
   to seconds from now), so the stamp sits in 1970 and **replaces the real 120 s cooldown with an expired one**. With the `raise dead`
   edge at 11 (`BuffTrigger`, no aura named "raise dead" on the bot) Frost may recast Raise Dead on every free global cooldown in
   a fight. **To check live** (action log: casts of 46584 per fight); fix: `BotSpellHistory::AddCooldown` or `time(nullptr) + 180`.
   The same call shape appears at `GenericSpellActions.cpp:95` (750) for failed casts (not a Frost line; mention to the maintainer).
5. **Strategy, presence out of combat for a damage dealer** (`GenericDKNonCombatStrategy.cpp:286-289`, `DKTriggers.cpp:163-177`): the
   tank edges are Blood only. Add a "frost presence" edge for Frost (and Unholy Presence for Unholy from 64) out of combat in a
   group and not mounted, so the first pull and the fight after a resurrection do not start without it; Pillar, Plague Strike and
   Death Strike carry no presence prerequisite today.
6. **Rotation, Death Runes unread** (`DKTriggers.cpp:43-47`, `FrostDKStrategy.cpp:117-118`, `:127-128`): `high frost rune` reads the
   frost slots only; the two Death Runes are never named (the `high blood rune` edge casts Frost Strike after the spec swap, not a
   rune spender). Fix: let the Howling Blast edge count frost and death runes (the core pays a frost cost with a death rune), and
   drop the blood-slot edge.
7. **Strategy, Blood Tap line** (`FrostDKStrategy.cpp`, model `BloodDKStrategy.cpp:115-116`, `CastBloodTapAction` `DKActions.h:402-408`): one
   `blood tap` trigger, if the tier 5 pick stays (question 4). Level 75 and above, so out of the detailed band.
8. **Strategy, Outbreak and Plague Leech at 81** (`FrostDKStrategy.cpp`, model `UnholyDKStrategy.cpp:175-176`): add `dk outbreak` for
   Frost; Plague Leech needs a line only if the tier 1 pick stays. Out of band.
9. **Strategy, Mind Freeze on a pack caster for a damage dealer** (`FrostDKStrategy.cpp`, model `BloodDKStrategy.cpp:137-141`, `DKActions.cpp:65-68`):
   the BT-134 action is Blood only; add the same `enemy caster in range` edge for Frost ("good" tier).
10. **Rotation, Soul Reaper only below 35 percent** (`FrostDKStrategy.cpp:91`, 87 and above): default 5.75 on any target; add a
    health test (the Blood sheet has the same item).
11. **Strategy, Anti-Magic Shell in PvE** (`GenericDKStrategy.cpp:204-208`): PvP only today; the glyph in the premade is idle until a
    PvE line exists (against an enemy caster that hits the group, Mind Freeze on cooldown). Shared with the Blood sheet.
12. **Config, stat weights** (`Mgr/Item/StatsWeightCalculator.cpp:465-477`): Mastery 0 below 80 (2.4 from 80), drop Armor Penetration
    2.7 (no such stat in 5.4.8), Strength above Hit and Expertise, Hit and Expertise 1.9; check the overflow values
    (`StatsWeightCalculator.h:25-28`, 8 and 26).
13. **Config, talents** `AiPlayerbot.PremadeSpecLink.6.1` (`conf/playerbots.conf.dist:2798`) from `112001` to **`020001`** (Roiling Blood,
    Purgatory, Death's Advance; tiers 4 to 6 unchanged); comment at `:2796-2797`. Questions 1 to 3.
14. **Config, glyphs** `PremadeSpecGlyph.6.1` (`:2799`) from `43541,45806,43548,43550,104101,43533` to **`43548,43550,68793,45806,104101,43533`**
    (Dark Succor for the inert Death Grip; Army of the Dead first). Question 6.
15. **Rotation, cooldown timing** (`FrostDKStrategy.cpp:105-106`, `:130-131`): Pillar of Frost, Raise Dead and Army of the Dead start
    in any fight; use the `boost` test ("hard fight") for Pillar, keep Army of the Dead as an expert line before a hard pull.
16. **Weapons** (`StatsWeightCalculator.cpp:821`, `:1157-1162`): dual wield forced for Frost; question 5. Out of the numbers until
    answered.
17. **Skill tiers**: no gating. Lines marked good and expert above (Death Strike heal, Mind Freeze on any caster, Chains of Ice,
    Pillar timing, Army of the Dead) are the ones a casual bot should drop.
18. **To check live:** whether the two Death Runes exist (Blood of the North, `spell_dk.cpp:1305-1370` converts spent blood runes;
    `HandleAuraConvertRune` for the aura at `SpellAuraMisc.cpp:500`); the real rune cooldown of a Frost death knight (Icy Talons);
    Rime and Killing Machine proc rates in play; Purgatory with the bots' healers; Horn of Winter against another attack power
    buff; the order of two actions with the same relevance (Rime Howling Blast and Obliterate).

Owner questions:

1. Tier 1 (level 15, open at 55): Roiling Blood (derived bot-friendly: passive, matches the Blood Boil lines already there) or Plague
   Leech (Method default and the premade; useful only with Outbreak at 81, no line).
2. Tier 2: Purgatory (Method default, passive) or Anti-Magic Zone (wowtbc "Raid CD" and the premade; no line, a 3 s window).
3. Tier 3: Death's Advance (derived, passive speed), Chilblains (passive slows), or Asphyxiate (premade, PvP stun only).
4. Tier 5 (level 75): Blood Tap (both guides, one new trigger line like the Blood spec) or Runic Corruption (Method's easier pick,
   passive, no line).
5. Weapons: keep dual wield one-handers (today, the calculator) or allow a two-hander before level 74 (data does not decide).
6. Glyphs: Dark Succor in place of Death Grip (derived; works with today's Death Strike lines), or keep wowtbc's set.
7. Death Strike for a damage dealer: at 50 percent health (proposal) or under 90 (today, displaces Obliterate).
8. Army of the Dead: keep as a hard fight opener (today), pre-pull only (Method, expert tier), or never in groups (as the Blood sheet).
9. Killing Machine: Frost Strike (wowtbc, today) or Obliterate on the proc (the usual two-hander use; Method does not say).

## PvP play (player guides, 5.x, read 2026-11-19)

- **Sources:** five player guides for Frost: four by one 5.4.7 and 5.4.8 player (talents and glyphs, a basic rotation, a burst, one dual wield battleground experiment), one for 5.1 to 5.2 (talents, glyphs, necrotic stacking). No coach course. Facts only, nothing copied; data checked against this server's 5.4.8 tables.
- **Coverage:** talents, glyphs and the burst rotation; thin on openers, positioning, interrupts, matchups and defensive timing. Every clip is a duel or battleground, no arena game. No Frost Death Knight was in the 2026-11-18 ladder sample, and the premade PvP set is a copy of the Unholy one.

**The plan.** Pressure by burst on one target: hold the target with Chilblains plus Chains of Ice (Howling Blast spreads the Frost Fever slow), then spend runes and runic power in a window. Obliterate with a Killing Machine proc is what kills; Frost Strike "does decent damage" but is the second choice. Keep a spare Unholy rune so a proc becomes an Obliterate at once instead of waiting.

**The burst.** Dots (Outbreak), two Obliterates, wait for a proc (trinket strength, Killing Machine, Rime), then Pillar of Frost and the on-use trinket (the ghoul lasts a minute and is a cooldown too). Have about 80 runic power before the cooldowns. Order: Obliterate first, Frost Strike second, Howling Blast on a Rime proc. When runic power runs out, Empower Rune Weapon (or on a kill). The 5.1 guide adds Necrotic Strike stacked on Death Runes with Pillar and the trinket up (about 100k damage and 62k absorbed healing each; Death Runes only, so Obliterate keeps its runes). Without a healer to absorb, Necrotic Strike is not needed.

**Defensives and control.** Lichborne out of fears; Anti-Magic Shell with its absorb glyph so casters get no leeway; Death Pact (big heal) or Conversion with Blood Presence (a slow kite loop); Blood Presence when tunneled, with Glyph of Shifting Presences keeping 70 percent of runic power so the swap back keeps its Frost Strikes; the Icebound Fortitude glyph (cooldown cut, duration cut to about 3 s) as a quick break from a Deep Freeze; Desecrated Ground as the second break (immune to loss of control while standing in it) against mages and double damage dealers; Remorseless Winter against melee comps (its stun sticks better with Chilblains); Gorefiend's Grasp in battlegrounds. Mages are named as the hardest matchup, melee cleaves as survivable. Icy Touch with its glyph removes a mage's shield or a bubble.

**Talents and glyphs, guides against the set** (`PremadeSpecLinkPvp.6.1 = 001002`, glyphs `43546,43539,43533,43672,104101,45800`, both the Unholy set):

| Tier | Guides | Set |
|---|---|---|
| 15 | Unholy Blight (burst, areas); Plague Leech and Roiling Blood rejected | Roiling Blood |
| 30 | Lichborne | Lichborne |
| 45 | Chilblains | Chilblains |
| 60 | Death Pact or Conversion | Death Pact |
| 75 | Runic Empowerment (damage) or Runic Corruption (rune regeneration); Blood Tap for necrotic stacking in arena | Blood Tap |
| 90 | Desecrated Ground (arena, double damage dealer or spell cleave), Remorseless Winter (melee, healer kill), Gorefiend's Grasp (battlegrounds) | Desecrated Ground |

Glyphs the guides want: Anti-Magic Shell, Shifting Presences, Icy Touch; alternatives Chains of Ice, Death and Decay, Dark Succor, Icebound Fortitude. Weapon: two-hander in arena guides; dual wield only as a battleground experiment. Stat order (steers rolls only): haste, mastery, crit with strength everywhere (one guide has mastery before haste); crit last.

**Differences from the guides (5.4.8).** Necrotic Strike (level 83) is not in the Frost or Unholy specialisation lists read from the data, so whether a Frost bot can learn it is open. Glyph of Icebound Fortitude cuts the cooldown by 50 percent and the duration by 75 percent (3 s of 12, as the guide says). Glyph of Pillar of Frost (58635) makes Pillar immune to all loss of control at a movement speed cost; no guide mentions it. Unholy Blight (115989) stings every second and infects with both diseases; the 12.5 percent Obliterate figure is the guide's own. Level gates: Frost Strike and Howling Blast 55, Obliterate 58, Pillar of Frost 68, Soul Reaper 87, Strangulate 58, Mind Freeze 57, Anti-Magic Shell 68, Icebound Fortitude 62, Death Grip 55, Empower Rune Weapon 76, Outbreak 81.

**Bots today (read from `src/strategy/Classes/deathknight/`, ordered by value).**
1. Pillar of Frost is cast only on the "hard fight" trigger (an elite, a boss, or three attackers: `FrostDKStrategy.cpp:105`, `DKTriggers.cpp:257-262`), so against one player it is never pressed (inference, to check live).
2. Frost Strike wins over Obliterate on a Killing Machine proc (`DKTriggers.cpp:140-143`, edge at +0.95 against Obliterate at +0.72 in `FrostDKStrategy.cpp`), the reverse of what the PvP guides want.
3. No runic power floor before cooldowns; Empower Rune Weapon only on a "no rune" trigger (`GenericDKStrategy.cpp:223-226`).
4. No Necrotic Strike, Desecrated Ground, Remorseless Winter or Gorefiend's Grasp (the tier 90 pick is idle).
5. Exists: Outbreak, Howling Blast and Plague Strike for diseases, Chains of Ice on a player 6 yd away or more, Mind Freeze, Strangulate and Anti-Magic Shell on casters, Icebound Fortitude at 40 percent, Death Pact (`GenericDKStrategy.cpp:186-210`, `FrostDKStrategy.cpp:127-138`); Lichborne, Icebound Fortitude and Anti-Magic Shell are in the shared defensive and control-break catalogs.
6. Icy Touch is only a disease fallback (`FrostDKStrategy.cpp:135-138`); the Icy Touch glyph is not used as a purge.

**Research update (public patch notes 5.1 to 5.4.7 and spell data, 2026-11-19; lines marked derived come from game knowledge and the 5.4.8 data).** Necrotic Strike's absorb is 225 percent of attack power since 5.3; Icebound Fortitude costs no runic power since 5.4.0; area knockbacks share a category with 10 s immunity; Death Strike's heal ignores damage from players since 5.1. Data: Pillar of Frost 1 min, 20 s, +20 percent strength; Desecrated Ground 2 min (so it is a once a fight second break, not an opener); Remorseless Winter 1 min, 8 s; Lichborne 2 min, 10 s; Death Grip 25 s, 30 yd; Strangulate 60 s, 30 yd, 5 s; Icebound Fortitude 3 min, 12 s (90 s and 3 s with the glyph); Chains of Ice 30 yd, 6 s. Derived: Death Grip a healer out of cover then Strangulate; with the partner's stun on the target start the burst (two Obliterates, Pillar, cooldowns); use the Icebound Fortitude glyph first and Desecrated Ground second against Deep Freeze; Remorseless Winter's stun on the fifth pulse is not guaranteed and diminishes with other stuns. Extra gap lines: Death Grip has no healer use in arenas (`GenericDKStrategy.cpp:45,62` is a pull); no code uses Desecrated Ground or Remorseless Winter.

**Proposals.** Config (no code): tier 15 Unholy Blight, tier 75 Runic Corruption or Empowerment per the guides (the set copies Unholy's Roiling Blood and Blood Tap, which the Frost guides reject for rune cost); glyphs Anti-Magic Shell, Shifting Presences, Icy Touch (the set holds the Unholy list); add the Pillar of Frost glyph only after a break rule exists. Rotation (waits for the owner's in-game test): Obliterate on Killing Machine; Pillar of Frost and cooldowns for a player burst with a runic power floor; Desecrated Ground and Icebound Fortitude as breaks from a stun.

**Open questions.** (1) Two-hander or dual wield for a Frost PvP bot (the guides use a two-hander in arena). (2) Whether Frost can learn Necrotic Strike. (3) Glyph of Pillar of Frost against the Desecrated Ground talent.
