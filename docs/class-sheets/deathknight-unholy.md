# Death Knight: Unholy (damage, melee with a permanent ghoul)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). A pre-pull Army of the
  Dead and the remaining level gates are left for later. The "bots today" columns below describe the code before the
  2026-11-11 implementation.
- **Sources:**
  - Method, "Unholy Death Knight Class Guide, MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 22 July 2025), read
    2026-11-11 through a scripted fetch (the talent builds are images; the text names swaps and gives a raid and a dungeon glyph set).
  - wowtbc.gg, "Unholy Death Knight PvE Guide, MoP Classic" (no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent, GlyphProperties,
    SpellEffect, SpellCooldowns, SpellPower, SpellRuneCost, SpellMisc, SpellDuration, SpellAuraOptions, SpellAuraRestrictions;
    `data/db2/Item-sparse.db2` for glyph item ids) through `spec_data.py` and short read-only scripts; the core source
    (the SkyFire core: `scripts/Spells/spell_dk.cpp`, `Entities/Player/Player.cpp`, `Entities/Unit/StatSystem.cpp`,
    `Spells/Auras/SpellAuraEffects.cpp`) for the rune mechanics, Shadow Infusion, Dark Transformation, Festering Strike, Unholy
    Frenzy, and the cooldown units of `Player::GetSpellCooldownDelay`.
  - Not readable: Overgear and Icy Veins (HTTP 403, as noted in the skill), Wowhead (needs a browser). Two guides were compared,
    not three.
- **Levels covered:** 55 to 70 in detail (a death knight starts at 55; leveling and 5-player dungeons, a damage dealer that
  follows the leader and the tank's marks), 71 to 90 as brief notes at the end of the priority lists.

Which source was richer: **talents** wowtbc for the options (three per tier, tags "Standard" and "Festerblight"), Method for the
swaps and the dungeon differences (Roiling Blood over Plague Leech, Runic Corruption for most players); **glyphs** Method (separate
raid and dungeon sets) against wowtbc (a standard set with the minors); they differ, see Glyphs; **stat order** Method only (wowtbc
promises one in its intro and has none); **opener** wowtbc (eleven steps with the Shadow Infusion count; Method has seven and one
line that puts Unholy Frenzy "on the ghoul"); **single target priority** wowtbc (19 lines, with the Blood Tap and rune rules and
the Festering Strike condition) against Method (17 lines, with the Death Coil rules for Dark Transformation); neither is complete
alone, the list below merges them; **area** wowtbc (two to three targets and four or more) against Method (a five-line list that
names Festering Strike for Death Runes first); **cooldowns** Method; **defensives** wowtbc (four, ordered). **Levels 55 to 89, what to
do before a spell is learned, thresholds, interrupts, the pull, self healing and which procs work in this core**: neither guide says
anything, so those lines come from the 5.4.8 data and are marked "derived".

Where the guides disagree, and the pick:

- **Death Coil.** Method: spend Runic Power with Death Coil when Dark Transformation is **not** active (step 12), on Sudden Doom, and
  above 90 Runic Power; wowtbc: Death Coil to reach five Shadow Infusion stacks while Dark Transformation is inactive, on Sudden
  Doom, and at 90 or more. Same rule in two forms. The data agrees: stacks are built only by Death Coils and cannot build while the
  ghoul is transformed (`spell_dk.cpp:1648-1672`), so the Death Coils during the transformation are the ones to hold. Pick: both.
- **Festering Strike.** Method: "when nothing else is available" (last), wowtbc: with Frost and Blood runes up (step 15), then again at
  the end. wowtbc's condition is the useful one: the strike costs a blood and a frost rune and Reaping turns both into Death Runes that
  Scourge Strike and Death and Decay can then use (derived from the spell text and `spell_dk.cpp:1305-1370`).
- **Unholy Frenzy.** Method: "on the ghoul" in the opener; wowtbc: a cooldown; the data: the target type is **a raid member** (target
  57, `TARGET_UNIT_TARGET_RAID`), the effect is +20 percent haste for 30 s and 2 percent of maximum health every 3 s, and a glyph
  removes the health loss (58616). The bots cast it on themselves. The "on the ghoul" line looks like an error in Method's opener
  (the ghoul is not a raid member); to check live if a pet cast is accepted.
- **Plague Leech.** Method and wowtbc both pair it with Outbreak (learned at 81); below 81 it is not a rune gain (derived), see Talents.
- **Blood Tap charges.** Method 10, wowtbc 9; the core allows it from 5 (`spell_dk.cpp:131`, `:1253`).
- **Talents.** The module's premade follows wowtbc's page (Roiling Blood, Anti-Magic Zone, Death's Advance, Death Pact, Blood Tap,
  Gorefiend's Grasp); Method's dungeon build is Roiling Blood with Runic Corruption or Blood Tap, Purgatory by default. Questions 1 to 4.
- **Hit and Expertise.** Method: both to about 7.5 percent. The caps are level 90 numbers; the module's calculator reads Unholy hit
  against the **spell** overflow (14 percent) and expertise against 26 (`StatsWeightCalculator.cpp:129-130`,
  `StatsWeightCalculator.h:25-28`), neither of which is Method's 7.5 (gap 13).

## Talents

Tiers and columns are from Talent.dbc. This server opens tier N at level 15 x (N + 1) (`Player::CalculateTalentsPoints` is
floor(level / 15); `BotFactory::InitTalentTiers`, `Factory/BotFactory.cpp:1272-1285`), so a level 55 death knight has **tiers 1 to 3
at once**, the fourth at 60, then 75 and 90. "Bots today" is `AiPlayerbot.PremadeSpecLink.6.2 = 010000` (`conf/playerbots.conf.dist:2803`):
Roiling Blood, Anti-Magic Zone, Death's Advance, Death Pact, Blood Tap, Gorefiend's Grasp.

| Tier (opens at) | Guide pick | Alternative and when | Spell id | Bots today (010000) |
|-----------------|------------|----------------------|----------|---------------------|
| 1 (55) | Plague Leech (Method raid default, wowtbc "Standard"); **Roiling Blood in dungeons (Method)** | Roiling Blood for AoE (Method raid); Unholy Blight (wowtbc "Festerblight") | 123693 / 108170 / 115989 | **Roiling Blood (column 0)**, the Method dungeon pick; no new line needed |
| 2 (55) | Purgatory (Method default) | Anti-Magic Zone for a raid-wide defensive (Method; wowtbc "Raid CD"); Lichborne | 114556 / 51052 / 49039 | **Anti-Magic Zone (column 1)**; the action exists (`DKActions.h:173`) but **no trigger casts it** (search of `src/`), so the pick is inert |
| 3 (55) | not named | Death's Advance, Chilblains, Asphyxiate | 96268 / 50041 / 108194 | Death's Advance (column 0), passive speed |
| 4 (60) | not named by Method; Death Pact is in wowtbc's defensive list | Death Siphon, Conversion | 48743 / 108196 / 119975 | Death Pact (column 0); the line exists (`GenericDKStrategy.cpp:182-184`) and the permanent ghoul feeds it |
| 5 (75) | Blood Tap (Method: best damage, harder; wowtbc "Standard") | **Runic Corruption (Method: recommended for most players in dungeons)**, Runic Empowerment (wowtbc "Festerblight") | 45529 / 51462 / 81229 | Blood Tap (column 0), same as wowtbc; **no Unholy line casts it** (Blood has one, `BloodDKStrategy.cpp:115-116`; gap 8) |
| 6 (90) | Gorefiend's Grasp (Method raid default) | Remorseless Winter if Gorefiend's is not needed (Method), Desecrated Ground | 108199 / 108200 / 108201 | Gorefiend's Grasp (column 0), same as Method; no bot line casts any of the three |

Notes on the picks (data and code, not guide):

- **Roiling Blood** (passive): Blood Boil also casts Pestilence on a diseased target. The bots already cast Pestilence and Blood Boil
  with three or more enemies (`GenericDKStrategy.cpp:212-215`). Method's dungeon pick for Unholy, and the premade's.
- **Plague Leech** (25 s, 30 yd): consumes both diseases and turns two depleted runes into Death Runes; Method and wowtbc both chain it
  into Outbreak (81). Below 81 the diseases return through Icy Touch (a frost rune) and Plague Strike (an unholy rune), two runes for
  two (derived), so it buys nothing before 81 and the bots have no line. **Unholy Blight** (115989, 90 s, 10 s): infects every enemy
  around the death knight with both diseases every second, free, learned with the talent; a pack opener for 55 to 80 (derived), no line.
- **Purgatory** (passive): a fatal hit becomes a Shroud of Purgatory that absorbs healing equal to the damage prevented; the death knight
  dies if the absorb is left when it ends (lockout 123981). Passive, so no line needed; healers' behaviour **to check live** (open item
  of the Blood sheet). **Anti-Magic Zone** (2 min, 3 s in this data, 40 percent less spell damage in the zone, spell 145629): a human
  times it; no bot line.
- **Death Pact** (120 s): heals for 50 percent of maximum health from an undead minion that loses 50 percent of its own. The Unholy
  ghoul is permanent (Master of Ghouls, 52143), so the pick works without the Raise Dead step the other specs need.
- **Runic Corruption** (passive, 45 percent proc on a Death Coil, Frost Strike or Rune Strike hit: +100 percent rune regeneration for
  3 s, spell 51460): Unholy casts Death Coils all the time, so it fires often. **Blood Tap** (5 charges, cap 12) needs one trigger.
- **Gorefiend's Grasp** pulls every enemy around a target to it; a human leader's pull plan does not expect a damage dealer that
  drags a room (derived), so the talent stays idle even if a line existed.

Proposed link (the guides' dungeon picks with two derived changes): **`020000`** (Roiling Blood / Purgatory / Death's Advance /
Death Pact / Blood Tap / Gorefiend's Grasp). Only tier 2 changes; tier 5 follows question 4 (`020020` with Runic Corruption).

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`, effect text from Spell.dbc. Slots unlock in pairs
at 25, 50 and 75; each configured glyph goes to the first free slot of its type, so a death knight of 55 to 74 has **four slots**
(two major, two minor). "Bots today" is `PremadeSpecGlyph.6.2 = 43548,45806,43533,43550,104101,104049` (`conf.dist:2804`): Pestilence,
Tranquil Grip, Anti-Magic Shell, Army of the Dead, the Long Winter, Festering Blood.

| Type | Glyph | Item id | Why (one clause) | Bots today |
|------|-------|--------:|------------------|------------|
| Major | Pestilence | 43548 | wider Pestilence radius; Method's dungeon core, wowtbc standard; the bots cast Pestilence with three or more enemies | yes (slot 1, level 25) |
| Major | Dark Succor | 68793 | derived: after a kill that yields experience, in Frost or Unholy Presence, the next Death Strike within 15 s is free and heals at least 20 percent of maximum health (spell 101568) | no |
| Major | Anti-Magic Shell | 43533 | Method core in both sets, wowtbc standard (absorbs all magic damage up to the limit); **the bots cast the Shell in PvP only**, so it is idle in PvE (gap 11) | yes (slot 3, level 50) |
| Major | Unholy Frenzy | 43534 | derived: Unholy Frenzy no longer costs health (58616); the bots cast it on themselves, 2 percent of maximum health every 3 s for 30 s | no |
| Major | Festering Blood | 104049 | wowtbc standard, Method AoE: Blood Boil treats every target as diseased | yes (slot 6, level 75) |
| Major | Regenerative Magic, Loud Horn | 104048, 104047 | Method's third core glyph in both sets (needs the Shell line), a Horn of Winter that gives more Runic Power | no |
| Major | Icebound Fortitude, Outbreak, Dark Simulacrum | 43536, 43826, 45800 | wowtbc burst damage reduction; Method alternatives | no |
| Major | Mind Freeze, Death and Decay, Death Coil, Death Grip, Unholy Command | 43549, 43542, 45804, 43541, 43825 | not in either guide (cooldown for Runic Power; a slow; allies; range; Death Grip reset on a kill) | no |
| Minor | Army of the Dead | 43550 | wowtbc standard; the ghouls no longer taunt, and the bots cast Army of the Dead in hard fights | yes (slot 4, level 50) |
| Minor | Tranquil Grip | 45806 | wowtbc standard; Death Grip no longer taunts (only matters if a grip line exists) | yes (slot 2, level 25) |
| Minor | the Long Winter | 104101 | wowtbc standard; Horn of Winter lasts an hour (5 minutes otherwise) | yes (slot 5, level 75) |
| Minor | **the Geist, the Skeleton** | 43535, 104099 | change Raise Dead into a geist or a skeleton: **never for Unholy** (Dark Transformation and Shadow Infusion need the ghoul) | no |
| Minor | Path of Frost, Death Gate, Resilient Grip, Horn of Winter, Death's Embrace, Foul Menagerie, Corpse Explosion | 43671, 43673, 43672, 43544, 43539, 43551, 43827 | cosmetic, travel or teaching | no |

Disagreement: Method's raid set is Loud Horn, Anti-Magic Shell, Regenerative Magic; its dungeon set Pestilence, Anti-Magic Shell,
Regenerative Magic; wowtbc's standard Pestilence, Anti-Magic Shell, Festering Blood. The premade is wowtbc's. **Proposed (derived):
`43548,43550,68793,45806,104101,43534`**: Pestilence kept; Army of the Dead moved to the level 25 minor slot; Dark Succor in place of
the idle Shell; Unholy Frenzy (the self-cast with no health cost) at 75 in place of Festering Blood. Question 6.

## Stat order

Method: Hit 7.5 percent and Expertise about 7.5 percent (caps), then Strength, then Critical Strike, Mastery, Haste. wowtbc has none.
The caps are level 90 numbers.

For loot rolls and quest rewards from 55 to 89 (derived, with the reason):

- **Plate** (Plate Specialization, level 55, adds Stamina only in all-plate, so keep a full set) and **Strength first**: Unholy Might
  (91107, level 55) adds a Strength percentage, and no Agility or Intellect does anything for the specialisation.
- **A slow two-handed weapon** in the main hand (the calculator already assumes it, `StatsWeightCalculator.cpp:1152-1156`): Unholy has
  no dual wield passive and no Threat of Thassarian (that is Frost's).
- **Hit and Expertise only up to the caps and only near level 90** (Method); the module's constants are not the 5.4.8 numbers.
- **Mastery is worth nothing before level 80**: Mastery: Dreadblade (77515, +shadow damage) is learned at 80 (SpecializationSpells).
- **Haste is better than Method's last place** in this core (derived from reading the formula): the rune recovery time is 10 s times
  (1 minus the haste percent) (`Player.cpp:21210-21231`), and an Unholy death knight has no Icy Talons, so its runes are the slowest
  of the three specs (about 0.6 runes per second against one global cooldown per second). Unholy is rune-limited, and Haste is the
  only stat that speeds the runes. Critical Strike is next (Method puts it second after Strength). Treat Haste and Critical
  Strike as equals until a live trace says otherwise.

Bots today (`Mgr/Item/StatsWeightCalculator.cpp:479-491`): Strength 2.5, Agility 0.5, Attack Power 0.5, **Armor Penetration 1.3**, Hit 2.2
(against the spell overflow, `:129-130`), Critical 1.7, Haste 1.8, **Mastery 1.4 at every level**, Expertise 1.5, Spell Power -1.0, Melee DPS
5.0. Gaps: armor penetration rating does not exist in 5.4.8 (derived; the weight is dead), Mastery counts before level 80 (the other
specs' blocks already use `lvl >= 80 ? x : 0`, e.g. lines 380, 417, 518), Hit above Strength. Proposed (derived): Strength 2.5, Haste
2.0, Critical 1.9, Hit 2.0, Expertise 1.7, **Mastery 0 below 80 and 1.4 from 80**, drop Armor Penetration; keep Attack Power 0.5 and
Melee DPS 5.0.

## What the specialisation knows, by level (5.4.8 data)

An Unholy death knight knows the death knight class line (SkillLineAbility 796) plus the Unholy rows of SpecializationSpells (spec 252).
Ids are the spells the player casts. Rune costs are from SpellRuneCost in the data's order (blood, unholy, frost, death; the core's
`RUNE_BLOOD 0, RUNE_UNHOLY 1, RUNE_FROST 2, RUNE_DEATH 3`); Runic Power costs are tenths in SpellPower (400 is 40). The global
cooldown is **1.0 s**. Unholy has **no Obliterate, Howling Blast, Frost Strike or Pillar of Frost** (Frost rows) and **no Rune Tap, Heart
Strike or Vampiric Blood** (Blood rows).

| Level | Spell (id, cost, cooldown) |
|------:|----------------------------|
| 55 | **Frost Presence** (48266: the only presence until 57; Runic Power gain +20 percent is aura 329, **not implemented in this core**; crowd control duration reduced); Plague Strike (45462, unholy rune, melee, Blood Plague); Icy Touch (45477, frost rune, 30 yd, Frost Fever); Blood Strike (45902, blood rune); Death Coil (47541, 40 RP, 40 yd); Death Grip (49576, 25 s); Death Gate (50977); passives **Master of Ghouls** (52143: the Raise Dead ghoul has no time limit, cooldown 60 s shorter), **Reaping** (56835: the runes spent by Blood Strike, Pestilence, Festering Strike, Icy Touch and Blood Boil become Death Runes), **Unholy Might** (91107, Strength), Plate Specialization (86536); tiers 1 to 3 open |
| 56 | Death Strike (49998, unholy and frost rune, 185 percent weapon damage, +20 RP; heals the 7 percent minimum outside Blood Presence); Blood Boil (48721, blood rune, area); Pestilence (50842, blood rune); **Raise Dead** (46584, free, 60 s cooldown with Master of Ghouls: the permanent ghoul) |
| 57 | Blood Presence (48263); **Mind Freeze** (47528, free, 15 s, melee range, 4 s school lock, no global cooldown) |
| 58 | **Scourge Strike** (55090, 1 unholy rune, **135 percent weapon damage plus 25 percent of that as Shadow per own disease**, +10 RP); Chains of Ice (45524); Strangulate (47476, blood rune) |
| 60 | **Death and Decay** (43265, unholy rune, 30 s, 10 yd ground, 10 s); **Shadow Infusion** (49572: a successful Death Coil gives the ghoul a stack of +10 percent damage for 30 s, up to 5); Unholy Aura (55610, party attack speed aura); tier 4 talent |
| 62 | **Festering Strike** (85948, blood and frost rune, 200 percent weapon damage, +20 RP, **extends Blood Plague, Frost Fever and Chains of Ice by 6 s**); Icebound Fortitude (48792, 180 s, 12 s, 20 percent less damage, stun immunity) |
| 64 | **Unholy Presence** (48265: haste +10 percent, movement +15 percent); **Sudden Doom** (49530 with the proc 81340: **in Unholy Presence** main-hand autoattacks sometimes make the next Death Coil free of Runic Power, 10 s) |
| 65 | **Horn of Winter** (57330, 20 s, +10 RP, attack power +10 percent for the party within 100 yd, 5 min) |
| 66 | **Unholy Frenzy** (49016, 180 s, no global cooldown, 30 s: +20 percent haste on a raid member and 2 percent of maximum health lost every 3 s); Path of Frost (3714) |
| 68 | **Ebon Plaguebringer** (51160: Plague Strike also applies Frost Fever, Blood Plague applies physical vulnerability, diseases +60 percent damage in the spell text); Anti-Magic Shell (48707, 45 s, 5 s, absorbs 75 percent of spell damage up to 50 percent of maximum health) |
| 70 | **Dark Transformation** (63560, 1 unholy rune, no cooldown, **needs 5 Shadow Infusion stacks**, ghoul transformed for 30 s, damage +100 percent in the aura data) |
| 72 | Raise Ally (61999, 30 RP, 40 yd, 600 s) |
| 74 | **Summon Gargoyle** (49206, free, 180 s, 40 s, damage that scales with attack power) |
| 75 | Improved Unholy Presence (haste +10 percent more); tier 5 talent |
| 76 | Empower Rune Weapon (47568, 300 s, all runes ready, +25 RP) |
| 80 | Mastery: Dreadblade (77515); Army of the Dead (42650, one rune of each type, 600 s, 4 s channel) |
| 81 | Outbreak (77575, 60 s, 30 yd, free, both diseases) |
| 83 | Necrotic Strike (73975, all specs) |
| 87 | Soul Reaper, Unholy (130736, unholy rune, 6 s, extra shadow damage after 5 s on a target below 35 percent health) |
| 90 | tier 6 talent |

**Resources and the ghoul (derived, arithmetic on the rows above).** Six runes: two blood, two unholy, two frost; base recovery 10 s
each, shortened by haste only (no Icy Talons), so **about 0.6 runes per second** against one global cooldown per second, and Reaping
turns the blood and frost runes spent by Festering Strike, Icy Touch, Blood Strike and Blood Boil into Death Runes that any
rune spell can use. Each rune spent gives 10 Runic Power (20 for a two-rune spell), so the income is about 6 Runic Power per second
plus Horn of Winter, and the only sink is **Death Coil at 40**. Five Death Coils (200 Runic Power) make one Dark Transformation,
which lasts 30 s, and the stacks cannot build during it; so the cycle is about 30 s of transformation and about 30 s of
rebuilding, which is Method's "Dark Transformation as often as possible" (derived estimate, not a measurement). Rune spells by
value per rune (weapon percent from the spell effects, not a simulation): Scourge Strike about 200 with two diseases (135 plus half
again as Shadow) for **one** rune, Festering Strike 200 for two runes (about 100 each, plus the disease extension and two Death
Runes), Plague Strike 100 for one rune plus the disease, Death Strike 185 for two runes. So **Scourge Strike is the best rune spender,
Festering Strike the rune converter, Death Coil the Runic Power sink and Shadow Infusion feeder** (derived).

## How the bots run this specialisation (read)

- An Unholy death knight gets the `unholy` combat strategy (`UnholyDKStrategy`, `Factory/AiFactory.cpp:528`) with `unholy aoe` and
  `dps assist`, the generic list with `boost` (`:316`), `behind` (`:569`), `threat` (`:559`), and out of combat `nc` and `dps assist`
  (`:777-781`). No `cc`, `cure` or `pull` for a damage dealer; Death Grip is never cast (the Blood pull only).
- Rank of an action (`strategy/Strategy.h`): ACTION_DEFAULT 5, NORMAL 10, HIGH 20, INTERRUPT 40, EMERGENCY 90. The engine takes the
  highest action that is useful and possible and tries the next one when a cast fails in the same tick.
- Default list (`UnholyDKStrategy.cpp:81-95`): Death and Decay **25**, Summon Gargoyle 6.0, Horn of Winter 5.2, Death Coil 5.1, melee 5.0.
  Edges: **the "death and decay cooldown" group** (`:117-133`): Dark Transformation ("ghoul frenzy") 5.9, Scourge Strike 5.8, Festering
  Strike 5.75, Soul Reaper 5.74, Icy Touch 5.7, Blood Strike 5.6, Plague Strike 5.5; Death Coil at 90 Runic Power or on Sudden Doom
  5.85 (`:181-182`, `DKTriggers.cpp:127-130`); Blood Strike 5.35 (`:138-139`, which uses the same gate through `dd cd and no desolation`, so it is dead from 60 too); rune dumps Icy Touch 5.37, Blood Strike 5.36, Plague Strike
  5.355 when both runes of a type are ready within 2 s (`:146-160`, `DKTriggers.cpp:43-47`); Plague Strike and Icy Touch when a disease
  has under 3 s (21 and 22, `:162-167`); Outbreak when a disease is missing (23, `:175-176`, 81 and above); Festering Strike when a disease
  has under 2 s (20.5, `:178-179`); Unholy Frenzy (24, `:184-185`); Army of the Dead (26, `boost`, `:188-189`); Death Strike under 40
  percent health (23, `:102-103`); Raise Dead when there is no pet (15, `:107-108`); pet toggle and stance (`:109-115`). Shared
  (`GenericDKStrategy.cpp`): as in the Frost sheet (Mind Freeze on the target and the healer, Horn of Winter, Raise Dead and Death
  Pact at critical health, Icebound Fortitude and Rune Tap under 40 percent, Raise Ally, area edges, Empower Rune Weapon, PvP edges).
- **Presences** are applied only as a prerequisite of single actions: Death Strike, Dark Transformation, Corpse Explosion, Scourge
  Strike and Icy Touch carry "unholy presence" (`UnholyDKStrategy.cpp:38-73`); Plague Strike, Death Coil, Festering Strike, Soul
  Reaper and Death and Decay carry none. Unholy Presence is learned at **64**, so before 64 the prerequisite is skipped and **no
  presence is ever cast** (no Frost Presence node for Unholy). Nothing keeps a presence up out of combat for a damage dealer.
- Skill tiers: no Unholy line is gated by `BotSkill`.

### The "death and decay cooldown" trigger never fires from level 60 (read, not yet seen in a log)

`DeathAndDecayCooldownTrigger::IsActive` (`DKTriggers.cpp:15-22`) returns true when Death and Decay is unknown (spell id 0, so
levels 55 to 59) and otherwise `bot->GetSpellCooldownDelay(spellId) >= 2000`. In this core `Player::GetSpellCooldownDelay` returns
**seconds** (`Player.cpp:17860-17865`: the stored end time is in whole seconds, `compat/SpellHistory.h:52-55`), and Death and Decay has a
30 s cooldown, so the test is never true. From level 60 the whole group behind the trigger (`UnholyDKStrategy.cpp:117-133`) is
silent: **Dark Transformation, Scourge Strike, Festering Strike, Soul Reaper and the Icy Touch, Blood Strike and Plague Strike fillers
are never cast**. What an Unholy death knight of 60 to 90 casts instead: Death and Decay (25), Summon Gargoyle, Death Coil, the rune
dumps at 5.355 to 5.37, the disease refreshes, Festering Strike under 2 s, Outbreak, Unholy Frenzy, Army of the Dead. At 55 to 59 the
group works (Death and Decay unknown). This is the largest gap of the sheet (gap 1); it is **inference from reading the code and the core
function**, to confirm with one action log of a level 60 or higher Unholy bot.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked "derived" comes
from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to `mod-aware-bots-548/src/strategy/Classes/deathknight/`
at commit `06b4020`. `US` is `UnholyDKStrategy.cpp`, `GS` is `GenericDKStrategy.cpp`, `TR` is `DKTriggers.cpp`, `AC` is `DKActions.h`.

### Levels 55 to 57

Known: Frost Presence, Plague Strike, Icy Touch, Blood Strike, Death Coil, Death Gate, Master of Ghouls, Reaping, Unholy Might; 56 Death
Strike, Blood Boil, Pestilence, Raise Dead; 57 Blood Presence, Mind Freeze. Three talent tiers open at 55. **No Scourge Strike (58), no
Shadow Infusion or Death and Decay (60), no Unholy Presence (64).**

| # | Tier | Condition | Spell (id, learned at) | Bots today (file:line or gap) |
|--:|------|-----------|------------------------|-------------------------------|
| 1 | core | before the first pull and whenever the ghoul is dead | Raise Dead (46584, 56): the permanent ghoul | partly: the `no pet` edge at 15, in combat only (`US:107-108`); no out-of-combat raise (the loop of #128, `GenericDKNonCombatStrategy.cpp:276-279`); unknown at 55; the action's stamp is wrong (gap 5) |
| 2 | core | never in Blood Presence (threat) (derived) | Frost Presence (48266, 55) | **missing:** no presence is ever cast below 64 (`US:38-73` carry "unholy presence", unknown); Blood Presence would add threat on top of the tank's |
| 3 | core | Frost Fever and Blood Plague missing, 30 s each, before 68 both buttons are needed | Icy Touch (45477, 55) and Plague Strike (45462, 55) | yes: refresh under 3 s, 22 and 21 (`US:162-167`); at 55 to 59 also the fillers 5.7 and 5.5 (`US:117-133`) |
| 4 | core | a blood rune ready | Blood Strike (45902, 55): the Death Rune maker | yes: 5.6 in the group (`US:131`) and the 5.36 rune dump (`US:155-156`); the 5.35 edge shares the gate (`US:138-139`) |
| 5 | core | Runic Power 40 or more | Death Coil (47541, 55): the Runic Power sink | yes: default 5.1 (`US:93`) |
| 6 | good | health under 40 percent, a rune pair ready, or after a kill with the Dark Succor glyph | Death Strike (49998, 56) | yes: 23 (`US:102-103`) |
| 7 | good | an enemy casts, 57 | Mind Freeze (47528, 57) | partly: the target and an enemy healer (`GS:173-177`); other casters are ignored in PvE (gap 9) |
| 8 | good | three or more enemies within 8 yd | Pestilence (50842, 56), then Blood Boil (48721, 56); Death and Decay from 60 | yes (`GS:212-215`) |

### Levels 58 to 59

As above, plus Scourge Strike, Chains of Ice and Strangulate (58).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 9 | core | an unholy or death rune ready, diseases on the target | **Scourge Strike** (55090, 58): the best rune spender | yes: 5.8 in the group (`US:120`), working only because Death and Decay is unknown |
| 10 | core | Death Coil as row 5 | Death Coil | yes |

### Levels 60 to 63

As above, plus Shadow Infusion, Death and Decay, Unholy Aura and the tier 4 talent (60), Festering Strike and Icebound Fortitude (62).
Still **no Unholy Presence and no Sudden Doom (64)**.

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 11 | core | diseases on the target, an unholy or death rune ready | **Scourge Strike** (55090, 58) | **missing since 60:** the group behind "death and decay cooldown" never fires (read above, `DKTriggers.cpp:15-22`, `US:117-133`) |
| 12 | core | blood and frost runes ready, Scourge Strike not castable (wowtbc: "with Frost and Blood runes up"), or a disease under 3 s | **Festering Strike** (85948, 62): +6 s on both diseases, two Death Runes | partly: only the disease edge under 2 s (20.5, `US:178-179`, `TR:98-108`); the main edge is in the dead group |
| 13 | core | off cooldown (30 s), a pack or a boss (Method: on cooldown) | Death and Decay (43265, 60), unholy rune | yes: default 25 (`US:87`), area 29 (`GS:212-215`) and 13 (`US:200-201`); **it outranks every disease and strike by design** and is asked from level 55 (unknown, gap 2) |
| 14 | core | after a kill or on cooldown | Death Coil to stack Shadow Infusion (60): each hit puts a +10 percent stack on the ghoul | yes: default 5.1 (`US:93`) |
| 15 | good | health under 50 percent and the group heals badly (derived) | Icebound Fortitude (48792, 62) | yes: 25 under 40 percent (`GS:189-192`) |
| 16 | core | two runes ready of a type and Blood or Frost runes idle | Icy Touch, Blood Strike, Plague Strike as dumps | yes: 5.355 to 5.37 (`US:146-160`) |

### Levels 64 to 67

As above, plus Unholy Presence and Sudden Doom (64), Horn of Winter (65), Unholy Frenzy and Path of Frost (66).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 17 | core | before the first attack and after every resurrection | **Unholy Presence** (48265, 64): haste +10 percent and the condition of Sudden Doom | partly: prerequisite of Death Strike, Dark Transformation, Corpse Explosion, Scourge Strike, Icy Touch (`US:38-73`); with the dead group (row 11) the first Icy Touch dump sets it; no out-of-combat line (gap 4) |
| 18 | core | Sudden Doom up (81340) or Runic Power 90 or more | Death Coil, free on the proc | yes: 5.85 (`US:181-182`, `TR:127-130`) |
| 19 | core | the buff is missing | Horn of Winter (57330, 65) | yes: out of combat 21 (`GenericDKNonCombatStrategy.cpp:278-279`), in combat 11, default 5.2 |
| 20 | good | off cooldown, a fight that will last, health 70 percent or more (derived: 20 percent of maximum health over 30 s unless the glyph is on) | Unholy Frenzy (49016, 66), self | **wrong trigger:** 24 at the first tick of any fight (`US:184-185`, `DKTriggers.h:238`), with no health condition; unknown 55 to 65 |
| 21 | core | Scourge Strike and Festering Strike rows as 11 and 12 | | missing (dead group) |

### Levels 68 to 70

As above, plus Ebon Plaguebringer and Anti-Magic Shell (68), Dark Transformation (70).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 22 | core | Blood Plague missing | **Plague Strike** (45462) now applies Frost Fever too (Ebon Plaguebringer, 68): one button, one unholy rune, and the frost rune stays free for Festering Strike or Death Strike | partly: the Icy Touch refresh still fires separately (`US:166-167`), a frost rune for nothing (derived) |
| 23 | core | 5 Shadow Infusion stacks on the ghoul | **Dark Transformation** (63560, 70): 30 s of double ghoul damage | **missing since 60:** it sits in the dead group (5.9, `US:119`, action `AC:151-158`, "pet target") |
| 24 | core | Dark Transformation not active, Runic Power 40 or more | Death Coil to build the 5 stacks (200 Runic Power) | yes: default 5.1, with no hold during the transformation (derived: the Death Coils then fill no stacks; the rest of the Runic Power waits for 90) |
| 25 | good | casters hit the group with spells | Anti-Magic Shell (48707, 68) | PvP only (`GS:204-208`, gap 11) |

### Levels 71 to 90, brief

| Level | Guide and data | Bots today |
|------:|----------------|------------|
| 72 | Raise Ally (61999) for a dead member | yes (`GS:197-198`, BT-134) |
| 74 | Summon Gargoyle on cooldown (both guides) | yes: default 6.0 (`US:90`), in any fight; unknown 55 to 73 (gap 2) |
| 75 | Improved Unholy Presence (+10 percent haste more); tier 5 talent: Blood Tap | passive; **no Unholy Blood Tap line** (gap 8) |
| 76 | Empower Rune Weapon with every rune on cooldown and Runic Power not capping (wowtbc) | yes (`GS:226-227`), no Runic Power check |
| 80 | Mastery: Dreadblade; Army of the Dead only before the pull (Method) | passive; Army of the Dead at 26 in a hard fight (`US:188-189`) |
| 81 | Outbreak for the diseases (both guides, step 3); Plague Leech into Outbreak | Outbreak yes (23, `US:175-176`); no Plague Leech line |
| 87 | Soul Reaper below 35 percent health (wowtbc) | in the dead group, on any target (`US:130`) when the trigger is fixed (gap 10) |
| 90 | Method: Unholy Presence, Horn, diseases, Raise Dead, Gargoyle, Frenzy, Empower Rune Weapon, Dark Transformation, Death Coil rules, Death and Decay, Scourge Strike, Festering Strike | the skeleton is there; the dead group and the Frenzy trigger are the gaps above |

## Cooldowns and defensives

The guides give an order for the defensives (wowtbc: Icebound Fortitude, Anti-Magic Shell, Death Pact, Anti-Magic Zone); the
thresholds are derived because dungeon trash needs numbers. Thresholds are the death knight's own health percent.

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Dark Transformation | 70 | as often as possible (Method) | at 5 stacks, in a fight that will last | in the dead group (never from 60) |
| Summon Gargoyle | 74 | on cooldown | on cooldown when the pull is an elite or a pack (the same test as Frost's Pillar of Frost) | on cooldown in any fight |
| Unholy Frenzy | 66 | on cooldown | on cooldown at 70 percent health or more, or with the glyph | at the first tick of any fight, no health test |
| Raise Dead | 56 | on cooldown (Method) | whenever the ghoul is dead; 60 s cooldown | `no pet` edge in combat; the stamp bug (gap 5) |
| Empower Rune Weapon | 76 | all runes on cooldown and Runic Power not capping | the same | all runes on cooldown only |
| Army of the Dead | 80 | only before the pull | before a hard pull (expert) | `boost` edge 26: a 4 s channel in a hard fight |
| Blood Tap | 75 talent | 9 or 10 or more charges, a depleted rune | 5 or more charges and a depleted rune | no Unholy line |
| Soul Reaper | 87 | below 35 percent (wowtbc) | target below 35 percent health and an unholy rune free | on any target |
| Icebound Fortitude | 62 | wowtbc defensive 1 | health under 50 percent, or a boss hit | under 40 percent |
| Anti-Magic Shell | 68 | wowtbc defensive 2 | an enemy caster hits the group, Mind Freeze used | PvP only |
| Death Pact | 60 talent | wowtbc defensive 3 | critical health, the ghoul alive | yes: Raise Dead then Death Pact (`GS:182-184`) |
| Death Strike | 56 | not named | health under 40 percent, or after a kill with Dark Succor | health under 40 percent |
| Mind Freeze | 57 | not named | any casting enemy in reach | the target and an enemy healer |
| Horn of Winter | 65 | upkeep | out of combat and in combat when missing | yes |

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Scourge Strike arrives at 58, Shadow Infusion and Death and Decay at 60, Festering Strike at 62, **Unholy
  Presence and Sudden Doom at 64**, Unholy Frenzy at 66, Ebon Plaguebringer at 68, **Dark Transformation at 70**, Summon Gargoyle at 74,
  Empower Rune Weapon at 76, Army of the Dead at 80, Outbreak at 81, Soul Reaper at 87. Below those levels the lists start without
  them. The server opens three talent tiers at 55 and the others at 60, 75 and 90.
- **Sudden Doom works only in Unholy Presence** (spell text), so an Unholy death knight in another presence never gets the free Death
  Coil; the guides say "stay in Unholy Presence" without the reason.
- **Frost Presence's Runic Power bonus does nothing in this core** (aura 329 not implemented); before 64 no presence matters for damage.
- Plague Leech is a rune refund only with Outbreak (81). Before 68 both disease buttons are needed (Plague Strike applies Frost Fever
  only with Ebon Plaguebringer, 68).
- Unholy Frenzy's target is a raid member (the guide's "on the ghoul" is not supported by the data) and it costs 20 percent of maximum
  health over 30 s unless the glyph is on.
- **Blood Tap fires from 5 charges** in the core; the guides' 9 and 10 are not a game rule.
- Death Strike heals the 7 percent minimum in Frost or Unholy Presence (the 5 s damage record counts only in Blood Presence); the
  guides do not use it for Unholy at all, the bots use it under 40 percent.
- Method's Hit and Expertise caps (7.5 percent) are level 90 numbers; the module's calculator uses a spell overflow of 14 for Unholy
  hit and 26 for expertise.
- wowtbc's "Rune of the Fallen Crusader" (a runeforged weapon enchant, learned at 70) is not a spell rotation line; not proposed.
- Overgear, Icy Veins and Wowhead were not readable, so two guides were compared.
- Method's and wowtbc's consumables, gear lists and Engineering notes have no counterpart in the bots (design decision 0016 to 0021).

## Spell names the Unholy strategies ask for that an Unholy death knight of level 55 to 70 does not know

`SpellIdValue` logs `[SID] ... qualifier='<name>' resolved=0` whenever a name matches no known, non-passive spell
(`strategy/value/SpellIdValue.cpp:147`) and caches the answer 20 s per name and bot (`:24`). From the code (inference, not checked
against the log), the names an Unholy death knight asks for and does not know:

| Name | Known from (Unholy) | Asked from |
|------|---------------------|------------|
| **`death and decay`** | 60 | default list at 25, **every tick from 55 to 59** (`US:87`); the cooldown trigger asks it too (`TR:17`) |
| `summon gargoyle` | 74 | default list at 6.0, every tick 55 to 73 (`US:90`) |
| `horn of winter` | 65 | default list and edges, 55 to 64 (`US:92`, `GS:178-179`) |
| `scourge strike` | 58 | the group, 55 to 57 (`US:120`) |
| `festering strike` | 62 | the group, 55 to 61 (`US:129`; the `dk festering` trigger tests `HasSpell` first, `TR:100`) |
| `ghoul frenzy` (Dark Transformation) | 70 | the group, 55 to 69 (`US:119`, `AC:157`) |
| `soul reaper` | 87 | the group, 55 to 86 (`US:130`) |
| `unholy presence` | 64 | prerequisite of five actions, 55 to 63 (`US:38-73`) |
| `unholy frenzy` | 66 | edge, 55 to 65 (`US:184-185`) |
| `raise dead`, `death strike` | 56 | `no pet` and health edges, level 55 (`US:102-108`) |
| `army of the dead` | 80 | `boost` edge, 55 to 79 (`US:188-189`) |
| `icebound fortitude` | 62 | `dk health 40`, 55 to 61 (`GS:189-192`) |
| `rune tap` | **never** (Blood only, 64) | the same edge, every check (`GS:189-192`) |
| `death pact` | talent 60 | critical health, 55 to 59 (`GS:182-184`) |
| `mind freeze` | 57 | target and healer edges, 55 and 56 (`GS:173-177`) |
| `pestilence`, `blood boil`, `death and decay` | 56, 56, 60 | the area edge, 55 (to 59 for Death and Decay) (`GS:212-215`) |
| `empower rune weapon` | 76 | `no rune` edge, 55 to 75 (`GS:226-227`) |
| `raise ally` | 72 | `combat party member dead` edge, 55 to 71 (`GS:197-198`) |
| `corpse explosion` | **never** (not learnable) | only as an action node, not asked in a list |

Fix direction: a level and talent test on the default list entries and on each edge, drop `rune tap`, and the `resolved=0` count per
bot should fall to near zero. The first two rows are asked on every tick of the default list and are the largest share.

## Gap list (ordered)

Ranked by value for levels 55 to 70 and the first dungeons. Config items can go at once; rotation and strategy items are class fixes
and wait for the owner's in-game test.

1. **Rotation, the "death and decay cooldown" trigger** (`DKTriggers.cpp:15-22`): `GetSpellCooldownDelay(spellId) >= 2000` compares
   seconds with milliseconds, so from level 60 the trigger never fires and Dark Transformation, Scourge Strike, Festering Strike,
   Soul Reaper and the three fillers (`UnholyDKStrategy.cpp:117-133`) are never cast. Fix: `>= 2` seconds, or drop the gate and let the
   relevance numbers order the group (Death and Decay already has its own default at 25 and a 30 s cooldown, so it cannot starve the
   group). This is the main fix for the specialisation. Inference from reading; confirm with one action log of a bot at level 60 or higher.
2. **Rotation, unknown-name spam** (table above): level and talent gates on the default list entries and on each edge, above all
   `death and decay` (asked every tick at 55 to 59) and `summon gargoyle` (55 to 73); delete `rune tap` from the shared health edge
   (`GenericDKStrategy.cpp:189-192`). Verify with the debug log.
3. **Rotation, Death Coil during the transformation** (`UnholyDKStrategy.cpp:93`, `:181-182`; derived from `spell_dk.cpp:1648-1672`): once
   Dark Transformation is back, the default Death Coil (5.1) keeps spending Runic Power while the ghoul is transformed, when the
   stacks cannot build; hold it above 90 Runic Power or on Sudden Doom (Method steps 12, 13, 17). Low value until row 23 works.
4. **Strategy, presence for a damage dealer** (`UnholyDKStrategy.cpp:38-73`, `GenericDKNonCombatStrategy.cpp:286-289`, `DKTriggers.cpp:163-177`):
   Frost Presence at 55 to 63 and Unholy Presence from 64, out of combat in a group and not mounted (the tank edges are Blood only);
   add the prerequisite to Plague Strike, Festering Strike, Death Coil and Death and Decay. Sudden Doom needs Unholy Presence.
5. **Rotation, Raise Dead stamp** (`DKActions.cpp:33-44`): `AddSpellCooldown(spellId, 0, 3 * 60 * 1000)` passes 180000 as an absolute end
   time in seconds (`Player.h:2206`), so the stamp sits in 1970 and replaces the real cooldown with an expired one. For Unholy the
   `no pet` edge limits the harm; the critical health edge (`GenericDKStrategy.cpp:182-184`) can recast Raise Dead on a living ghoul.
   **To check live**; fix `time(nullptr) + 180`.
6. **Rotation, Unholy Frenzy** (`UnholyDKStrategy.cpp:184-185`, `CastUnholyFrenzyAction`): add a health test (70 percent or more) unless
   the glyph is on, and the `boost` test so it is not spent on a trash mob.
7. **Rotation, Plague Strike alone from 68** (`UnholyDKStrategy.cpp:166-167`): Ebon Plaguebringer applies both diseases, so the
   Icy Touch refresh for Frost Fever is a frost rune spent for nothing; make the disease refresh Plague Strike only from 68.
8. **Strategy, Blood Tap line** (`UnholyDKStrategy.cpp`, model `BloodDKStrategy.cpp:115-116`, `CastBloodTapAction` `DKActions.h:402-408`):
   one `blood tap` trigger if the tier 5 pick stays (question 4); level 75 and above.
9. **Strategy, Mind Freeze on a pack caster for a damage dealer** (`UnholyDKStrategy.cpp`, model `BloodDKStrategy.cpp:137-141`,
   `DKActions.cpp:65-68`): the BT-134 action is Blood only; add the same `enemy caster in range` edge ("good" tier).
10. **Rotation, Soul Reaper only below 35 percent** (`UnholyDKStrategy.cpp:130`, 87 and above): add a health test (the Blood sheet has the
    same item).
11. **Strategy, Anti-Magic Shell in PvE** (`GenericDKStrategy.cpp:204-208`): PvP only; the premade's glyph (and the proposed one) is idle until
    a PvE line exists. Shared with the Blood and Frost sheets.
12. **Config, talents** `AiPlayerbot.PremadeSpecLink.6.2` (`conf/playerbots.conf.dist:2803`) from `010000` to **`020000`** (Purgatory in
    place of the inert Anti-Magic Zone); comment at `:2801-2802`. Questions 1 to 4.
13. **Config, stat weights** (`Mgr/Item/StatsWeightCalculator.cpp:479-491`, `:129-130`): Mastery 0 below 80 (1.4 from 80), drop Armor
    Penetration 1.3, Haste 2.0, Hit 2.0 against the melee overflow unless a live check says Unholy needs the spell one, Expertise 1.7;
    check the overflow constants (`StatsWeightCalculator.h:25-28`).
14. **Config, glyphs** `PremadeSpecGlyph.6.2` (`:2804`) from `43548,45806,43533,43550,104101,104049` to **`43548,43550,68793,45806,104101,43534`**
    (Dark Succor in place of the idle Shell, Unholy Frenzy without the health loss at 75). Question 6.
15. **Rotation, area list** (`GenericDKStrategy.cpp:212-215`, `UnholyDKStrategy.cpp:195-202`): Method's area list puts Festering Strike
    before Blood Boil for Death Runes; with the group fixed (row 1) Blood Boil at 13 outranks Festering Strike at 5.75. Low value.
16. **Rotation, cooldown timing** (`UnholyDKStrategy.cpp:90`, `:188-189`): Summon Gargoyle and Army of the Dead start in any fight; use the
    `boost` test for the Gargoyle and keep Army of the Dead an expert line before a hard pull.
17. **Skill tiers**: no gating. Lines marked good and expert above (Death Strike heal, Mind Freeze on any caster, Unholy Frenzy,
    Festering Strike as a rune converter, Army of the Dead) are the ones a casual bot should drop.
18. **To check live:** the action log of a level 60 or higher bot for gap 1; Shadow Infusion stack counts on the ghoul (data: a hit of
    Death Coil gives a stack; the aura text says "a chance"); whether Unholy Frenzy is accepted on the ghoul; whether Sudden Doom
    procs at all in Unholy Presence in this core; Reaping's Death Runes (`spell_dk.cpp:1305-1370`); Purgatory with the bots' healers; the
    action order of equal relevance numbers.

Owner questions:

1. Tier 1 (open at 55): Roiling Blood (today, the Method dungeon pick) or Plague Leech (Method raid default, wowtbc "Standard", useful
   only with Outbreak at 81, no line) or Unholy Blight (a pack opener, one new line).
2. Tier 2: Purgatory (Method default, passive, proposal) or Anti-Magic Zone (today, wowtbc "Raid CD", no line, a 3 s window).
3. Tier 3: Death's Advance (today, passive speed), Chilblains or Asphyxiate.
4. Tier 5 (level 75): Blood Tap (today and both guides, one new trigger line) or Runic Corruption (Method's recommendation for dungeons,
   passive, no line).
5. Presence 55 to 63: Frost Presence (derived, no threat penalty) or nothing (today).
6. Glyphs: Dark Succor and Unholy Frenzy in place of the Shell and Festering Blood (derived), or keep wowtbc's set.
7. Unholy Frenzy: self-cast at 70 percent health or more (proposal), always at the first tick (today), or not at all without the glyph.
8. Army of the Dead and Summon Gargoyle: any hard fight (today), pull-bound with `boost` (proposal), or Army of the Dead pre-pull only.
9. Fix the cooldown trigger by units (smallest change) or by removing the gate (the relevance numbers already order the group).

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 1 player of this specialisation (rating 2500 to 2500). 2v2: no player of this specialisation. Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** none, only 1 player in all (noted, not changed)
- **BT-324 result:** open (1 player)
- **Set now:** `PremadeSpecLinkPvp.6.2 = 001002`, `PremadeSpecGlyphPvp.6.2 = 43546,43539,43533,43672,104101,45800`

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Roiling Blood 100% / - / 1 of 1 | Roiling Blood | Roiling Blood | matches |
| 2 (30) | Lichborne 100% / - / 1 of 1 | Lichborne | Lichborne | matches |
| 3 (45) | Chilblains 100% / - / 1 of 1 | Chilblains | Chilblains | matches |
| 4 (60) | Death Pact 100% / - / 1 of 1 | Death Pact | Death Pact | matches |
| 5 (75) | Blood Tap 100% / - / 1 of 1 | Blood Tap | Blood Tap | matches |
| 6 (90) | Desecrated Ground 100% / - / 1 of 1 | Desecrated Ground | Desecrated Ground | matches |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Anti-Magic Shell (43533) | 100% | - | 1 of 1 | yes | yes |
| major | Icy Touch (43546) | 100% | - | 1 of 1 | yes | yes |
| major | Regenerative Magic (104048) | 100% | - | 1 of 1 | no | no |
| major | Dark Simulacrum (45800) | 0% | - | 0 of 1 | yes | yes |
| minor | Army of the Dead (43550) | 100% | - | 1 of 1 | no | no |
| minor | Resilient Grip (43672) | 100% | - | 1 of 1 | yes | yes |
| minor | the Long Winter (104101) | 100% | - | 1 of 1 | yes | yes |
| minor | Death's Embrace (43539) | 0% | - | 0 of 1 | yes | yes |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **Open (BT-451):** one player in 3v3, none in 2v2: all six tiers match the set; the glyphs Icy Touch, Anti-Magic Shell, Resilient Grip and the Long Winter match, Regenerative Magic (major) and Army of the Dead (minor) are not in the set.
