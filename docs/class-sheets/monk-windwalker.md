# Monk: Windwalker (damage, melee)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). Gap item 9, second half
  (no Legacy of the White Tiger on a member who has Leader of the Pack, Arcane or Dalaran Brilliance or a hunter pet's
  critical strike buff) is done. The "bots today" columns below describe the code before the 2026-11-11 implementation.
- **Sources:**
  - Method, "Windwalker Monk Class Guide - MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 23 July 2025), read
    2026-11-11 through a scripted fetch (the talent builds are images; the text names the picks and gives one glyph set for raid and
    dungeon).
  - wowtbc.gg, "Windwalker Monk Rotation & Talent Build - MoP Classic" (no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent, GlyphProperties,
    SpellCooldowns, SpellPower, SpellEffect, SpellMisc, SpellDuration, SpellRange, SpellInterrupts, ChrClassesXPowerTypes;
    `data/db2/Item-sparse.db2` for glyph item ids) through `spec_data.py` and short read-only scripts; the core source
    (the SkyFire core: `scripts/Spells/spell_monk.cpp`, `Entities/Unit/Unit.cpp`, `Entities/Player/Player.cpp`) for
    the Chi cap, the Chi decay, the Energy regeneration, the Tigereye Brew counting and the Touch of Death check; the core's
    `spell_group` SQL for the stat buffs.
  - Not readable: Overgear and Icy Veins (HTTP 403, as noted in the skill), Wowhead (needs a browser). Two guides were compared,
    not three.
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the tank's
  marks), 61 to 90 briefly.

Which source was richer: **talents** Method for the picks and the swaps (it names one pick per row, wowtbc lists the options of
every row without a mark); **glyphs** equal (both list Spinning Crane Kick, Touch of Death, Touch of Karma and the minors Blackout
Kick, Spirit Roll, Water Roll; Method adds two major alternatives); **stat order and caps** Method (wowtbc has none); **opener**
wowtbc (a 13-step line with the Chi Brew and Tigereye timing; Method has five steps); **single target priority** wowtbc (14 lines
with the Rising Sun Kick debuff, the Chi Brew rule and the Fists of Fury condition; Method has 11 and leaves out the debuff);
**area** wowtbc (two and three-plus targets separated; Method says only "Spinning Crane Kick in place of Jab"); **defensives**
wowtbc (an ordered list of four; Method has no section); **dungeon talent notes** Method (Chi Burst over Chi Wave, Rushing Jade Wind
only with high cleave uptime, Invoke Xuen when there is downtime). **Levels 15 to 60, the Chi budget, what to do before Rising Sun
Kick (56) or Tigereye Brew (56), thresholds, interrupts, the Combo Breaker timing, the buffs, duels and PvP**: neither guide says
anything, so those lines come from the 5.4.8 data and are marked "derived".

Where the guides disagree, and the pick:

- **Order of Rising Sun Kick, Fists of Fury and Blackout Kick.** Method: Rising Sun Kick, Fists of Fury, Blackout Kick on Combo
  Breaker, Chi Wave, then Jab and Blackout Kick as filler. wowtbc: the debuff upkeep first, Fists of Fury "to spend Chi at low
  Energy if Tiger Power lasts the whole channel", Chi Wave, Blackout Kick on a proc, and Blackout Kick as a plain Chi spender only
  "if it does not delay Rising Sun Kick". Both rank Blackout Kick last; wowtbc's condition is the useful one, and the data agrees (the
  spell text coefficients per Chi are Fists of Fury about 12, Rising Sun Kick 7.2, Blackout Kick 3.6, Tiger Palm 3; see "Chi
  budget"). Pick: wowtbc's condition, with Method's ranking. The bots do the opposite (Blackout Kick takes every 2 Chi).
- **Tigereye Brew.** Both: at 10 or more charges. The bots spend at any charge count (gap 4).
- **Chi Brew.** Method: the best choice of its row, no rule; wowtbc: at 1 or more charges and 2 Chi or less. wowtbc's rule is used.
- **Talents, tier 1, 2, 6.** Method: Tiger's Lust, Chi Wave (Chi Burst for area and slightly better in dungeons), Invoke Xuen (Rushing
  Jade Wind only with very high cleave uptime). The premade takes Celerity, Chi Wave and Rushing Jade Wind. Questions 1 and 2.
- **Glyph of Touch of Death.** Method: "raids only, not open world". The data has no such limit (the glyph removes the Chi cost and adds
  2 minutes of cooldown, effects of 123391); the restriction is the guide's advice, not the game's. Question 4.
- **Hit and Expertise.** Method: both to 7.5 percent. The caps are level 90 numbers; the module's stat calculator uses 8 percent for
  melee hit and **26** for expertise (`Mgr/Item/StatsWeightCalculator.h:26-28`), a WotLK number that no 5.4.8 character reaches (gap 12).

## Talents

Tiers and columns are from Talent.dbc (the monk talents are shared by the three specialisations). "Bots today" is
`AiPlayerbot.PremadeSpecLink.10.2 = 002220` (`conf/playerbots.conf.dist:2910`; one digit per tier, column counted from 0):
Celerity, Chi Wave, Chi Brew, Leg Sweep, Diffuse Magic, Rushing Jade Wind. The talent-override swap (`BotOverrideSpell`,
`AI/PlayerbotAI.cpp:3731-3790`, Talent.dbc override column) matters for two monk talents: Rushing Jade Wind replaces Spinning Crane
Kick (101546) and Chi Torpedo replaces Roll (109132); with the premade, the name `spinning crane kick` casts Rushing Jade Wind at 90.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (002220) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Tiger's Lust (Method; wowtbc lists all three) | Momentum when there is much movement (Method); Celerity (wowtbc lists only) | 116841 / 115174 / 115173 | **Celerity, differs**: a passive, it adds a Roll charge and cuts the cooldown by 5 s (spell text), and the Roll gap-closer line exists (`MonkActions.cpp:361`), so Celerity has an effect, Tiger's Lust has no line |
| 2 (30) | Chi Wave (Method: single target) | Chi Burst (Method: area, slightly better in dungeons; wowtbc: AoE), Zen Sphere | 115098 / 123986 / 124081 | Chi Wave, same; the default list has the line (`MonkStrategies.cpp:55`) |
| 3 (45) | Chi Brew (Method: best of its row; wowtbc's opener uses it) | Ascension (derived: +1 maximum Chi, +15 percent Energy regeneration), Power Strikes | 115399 / 115396 / 121817 | Chi Brew, same; the line exists (`MonkStrategies.cpp:173`, `MonkActions.cpp:316`) |
| 4 (60) | Leg Sweep (Method: most uses) | Ring of Peace by fight (Method), Charging Ox Wave | 119381 / 116844 / 119392 | Leg Sweep, same; **no line casts any of the three** (no `cc` strategy for the monk, `Factory/AiFactory.cpp:531-543`) |
| 5 (75) | Diffuse Magic (Method: best mitigation of its row) | Dampen Harm (wowtbc: burst reduction), Healing Elixirs | 122783 / 122278 / 122280 | Diffuse Magic, same as Method; **no line** (the Dampen Harm edge is Brewmaster only, `MonkStrategies.cpp:232`) |
| 6 (90) | Invoke Xuen (Method: best single target; dungeon: when there is downtime) | Rushing Jade Wind (area; Method dungeon: only with very high cleave uptime), Chi Torpedo | 123904 / 116847 / 115008 | **Rushing Jade Wind, differs**; the line exists in the area edge (`MonkStrategies.cpp:122`); the Invoke Xuen line also exists (`:170`) and would fire at every pull |

Notes on the picks (data, not guide):

- Chi Brew (115399): restores 2 Chi and gives 2 Tigereye Brew charges for a Windwalker (effects of 145640), two charges, so it is a
  burst of Chi and of Tigereye at once. Ascension (115396) is +1 maximum Chi, +15 percent Energy regeneration and +15 percent mana. In an
  Energy-limited rotation (see "Chi budget") +15 percent regeneration is about +0.6 Chi per 8 s, which over 45 s (one Chi Brew
  recharge) is 3.4 Chi against Chi Brew's 2 Chi plus 2 charges. **Derived estimate, not a measurement**; Method's pick stays.
- Tiger's Lust (116841, 30 s): clears roots and snares and gives 70 percent speed. Against a Frost Nova or a snared flee it is the
  best answer; no line exists. Celerity is the bot-friendly pick until one does (question 1).
- Chi Wave (115098, 15 s cooldown in the data, not 8 s) is a 40 yd bouncing bolt. Chi Burst (123986, 30 s) is stronger in a pack but has
  no line.
- Rushing Jade Wind (116847, 6 s cooldown, 40 Energy, 6 s of ticks every 0.75 s, +1 Chi at 3 or more targets) takes 40 Energy per 6 s
  of the 10 per second the monk regenerates. Method keeps it for very high cleave uptime for that reason (derived).
- Invoke Xuen (123904, 3 min): an effigy that attacks the target and hits 3 enemies near it. It has an always-on-cooldown line.
- Dampen Harm and Diffuse Magic have no Windwalker line: the premade's tier 5 pick is inert either way.

Proposed link (derived): **keep `002220`** for the band 15 to 60 (tiers 1 to 4 are what a bot below 75 has; Celerity is the only tier 1
pick that has an effect). From 75 and 90 the picks follow questions 1 and 2. No config change is needed for the band.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`, effect text from Spell.dbc. Slots unlock in
pairs at 25, 50 and 75 (config comment `conf/playerbots.conf.dist:2631-2636`). The config order is major, minor, major, minor,
minor, major. "Bots today" is `AiPlayerbot.PremadeSpecGlyph.10.2 = 85700,90715,87900,87887,87889,85697` (`conf.dist:2911`).

| Type | Glyph | Item id | Why (one clause) | Bots today |
|------|-------|--------:|------------------|------------|
| Major | Touch of Death | 85700 | both guides' first pick; no Chi cost but +2 minutes of cooldown (123391); the spell is learned at 22 | yes, slot 1 (level 25) |
| Major | Touch of Karma | 87900 | both guides; range 20 yd instead of melee (125678); **no value for a bot that stays in melee** (derived) | yes, slot 3 (level 50) |
| Major | Spinning Crane Kick | 85697 | both guides; full movement speed while channelling (120479); **a bot never walks inside the channel** (follow, reach and formation refuse while channelling, `ReachTargetActions.cpp:34`, `FollowActions.cpp:1010`, `MovementActions.cpp:1727-1747`), so inert (derived) | yes, slot 6 (level 75) |
| Major | Fists of Fury | 87892 | not in either guide; parry chance +100 percent while channelling (125671), a real effect for a bot that stands still in melee for 4 s | no |
| Major | Fortifying Brew | 87893 | not in either guide; more damage reduction, less bonus health | no |
| Major | Detox, Paralysis, Rapid Rolling, Targeted Expulsion, Nimble Brew | 87899, 87897, 82345, 87901, 87880 | Detox heals the target when it removes an effect; Paralysis also clears damage over time (no Paralysis line); the others need lines that do not exist | no |
| Major | Zen Meditation | 85695 | wowtbc "quality of life"; the spell is learned at 82 | no |
| Minor | Blackout Kick | 90715 | both guides; the Blackout Kick damage over time works from any position, the self heal is lost (132005); the bots take the "behind" position anyway (`AiFactory.cpp:567`) | yes, slot 2 (level 25) |
| Minor | Spirit Roll, Water Roll | 87887, 87889 | both guides: the only good minors | yes, slots 4 and 5 |
| Minor | Zen Flight, Jab, Honor, Crackling Tiger Lightning, Rising Tiger Kick | 87890, 87884, 87883, 87881, 87885 | cosmetic or flight teaching | no |

Disagreement: none between the guides on the set. **Proposed (derived): `85700,90715,87892,87887,87889,87900`**: Fists of Fury in the
level 50 slot (a real defensive effect from level 50) instead of Touch of Karma, which moves to the level 75 slot in place of Spinning
Crane Kick. Question 4.

## Stat order

Method: Hit and Expertise to about 7.5 percent each, then Agility, Haste (a target of 10,000 rating at 90), Critical Strike, Mastery;
an optional hit of 26.5 percent makes the off-hand hit too, which Method calls often not worth it. wowtbc has no stat order. The caps
are level 90 numbers.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **Agility first.** It is the Windwalker's attack power stat; Strength and Intellect give nothing to the specialisation (no line of
  the sheet uses mana). Leather Specialization at 50 adds 5 percent Agility in all-leather (120227), so keep a full leather set from
  50.
- **Critical Strike and Haste next, close together** (derived). Method ranks Haste second because in the game Haste raises Energy
  regeneration. Here it does not: the Energy formula adds only 0.01 times the haste multiplier (about 1.0) per update unit
  (`Player.cpp:2646-2650`), and the 1.0 s global cooldown of the monk is already the minimum (`Spell.cpp:8331-8341` clamps at
  `MIN_GCD`), so Haste is auto-attack speed and Tiger Strikes only. Critical Strike is as good as Haste from level 15 (inference from
  reading the code; a live Energy trace with a hasted gear piece would settle it).
- **Hit and Expertise before the other secondaries only up to their caps** (Method). Whether the caps are reachable below 90 is **to
  check** against the calculator's own overflow values (8 percent hit, 26 expertise, `StatsWeightCalculator.h:26-28`).
- **Mastery is worth nothing before level 80**: Mastery: Bottled Fury (115636) is learned at 80 (SpecializationSpells) and adds a chance
  of an extra Tigereye Brew charge, which Tigereye Brew itself (56) already needs.
- **Two one-handed weapons** over a two-hander: the monk dual-wields from 10 (Dual Wield, 124146) and Way of the Monk gives +40 percent
  auto-attack damage for dual-wielded one-handers or +40 percent melee speed for a two-hander (spell text of 120277), so the pair wins
  once there is a second weapon. The calculator scales a two-hander by 0.05 for the Windwalker (`StatsWeightCalculator.cpp:819-830`);
  with only one one-hander found, a staff or polearm is better than an empty off-hand (to check how the calculator treats an empty
  slot).
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:678-687`): Agility 2.0, Attack Power 1.0, Hit 2.1, Critical 1.9, Haste 2.0, **Mastery
  1.3 at every level**, Expertise 2.0, Melee DPS 7.0. Proposed (derived): **Agility 2.5, Critical 2.0, Haste 1.8, Hit 1.8, Expertise 1.7,
  Mastery 0 below level 80 and 1.3 from 80**; keep Attack Power 1.0 and Melee DPS 7.0. Hit and Expertise above Agility (today) lets a hit
  piece beat an Agility piece where the caps are not reached.

## What the specialisation knows, by level (5.4.8 data)

A Windwalker knows the Monk class line (SkillLineAbility 829) plus the Windwalker rows of SpecializationSpells (spec 269). Ids are the
spells the player casts. Costs are Energy (Stance of the Fierce Tiger; the other two stances cost mana percentages) or Chi. The global
cooldown is **1.0 s**. **Chi: cap 4 from level 1** (`Unit::GetCreatePowers` returns 4 for `POWER_CHI`, `Unit.cpp:5674`) **and 5 with
Ascension** (effect of 115396, a maximum-power aura); it does not change with the level. Chi decays 1 per 10 s out of combat and not in
combat (`Player.cpp:2672-2677`). **Energy: cap 100 at every level**, regeneration 10 per second; the haste term of the formula adds 0.01 times the haste multiplier (about 1.0) per update unit, so
**melee haste adds almost nothing to Energy in this core** (`Player.cpp:2646-2650`, the multiplier is initialised to 1.0 at `:3430`; inference from reading the formula). A Windwalker needs no mana.

| Level | Spell (id, cost, cooldown) |
|------:|----------------------------|
| 1 | Jab (100780, 40 Energy, **+1 Chi**; weapon coefficient 1.5); **Stance of the Fierce Tiger** (103985: +10 percent movement speed, +10 percent damage, **+1 Chi from Jab and Expel Harm**, so Jab gives **2 Chi**); the stance needs no talent |
| 3 | Tiger Palm (100787, 1 Chi, coefficient 3; grants Tiger Power 125359: 30 percent of the target's armor ignored, **20 s, does not stack**) |
| 5 | Roll (109132) |
| 7 | Blackout Kick (100784, 2 Chi, coefficient 7.12) |
| 10 | **Fists of Fury** (113656, 3 Chi, 25 s, **4 s channel**, damage 7.5 each second and at the start, split between the targets in front, stuns them; any movement interrupts it); Tiger Strikes (120272, a chance for faster attacks with extra swings); Dual Wield (124146); the specialisation is chosen here |
| 14 | Provoke (not for this specialisation) |
| 15 | **Combo Breaker** (137384: 12 percent per Jab that the next Blackout Kick or Tiger Palm costs no Chi, 15 s, one proc each); tier 1 talent |
| 18 | Flying Serpent Kick (101545, 25 s); Resuscitate (115178) |
| 20 | Detox (115450, 20 Energy, 8 s, poison and disease for this specialisation); Combat Conditioning (128595: Blackout Kick adds 20 percent damage over time from behind or heals from the front); Zen Pilgrimage |
| 22 | **Touch of Karma** (122470, 90 s, 10 s absorb up to the monk's maximum health, redirected to the enemy as Nature damage); **Touch of Death** (115080, 3 Chi, 90 s, kills a non-player target whose health is **at or below the monk's**, `spell_monk.cpp:830-865`); Legacy of the Emperor (115921, 20 Energy) |
| 24 | Fortifying Brew (115203, 3 min, 20 s, 20 percent less damage, +20 percent health) |
| 26 | **Expel Harm** (115072, 40 Energy, 15 s, heals the monk, **+2 Chi** in the stance); Afterlife (116092) |
| 28 | Disable (116095, 15 Energy, no cooldown, -50 percent speed; cast again on a snared target it roots) |
| 30 | tier 2 talent; Nimble Brew (137562, 2 min, clears root, stun, fear) |
| 32 | **Spear Hand Strike** (116705, 15 s, melee range, interrupts and locks the school 4 s; silences 2 s more if the enemy faces the monk) |
| 36 | **Energizing Brew** (115288, 60 s, 60 Energy over 6 s, **combat only**) |
| 42 | Sparring (116023, parry) |
| 44 | Paralysis (115078, 20 Energy, 15 s, incapacitate 40 s, breaks on damage) |
| 45 | tier 3 talent; Adaptation (126046) |
| 46 | **Spinning Crane Kick** (101546, 40 Energy, **2.25 s channel**, 3 ticks of coefficient 1.59, **+1 Chi if it hits 3 or more targets**, -30 percent movement speed) |
| 48 | Spinning Fire Blossom (115073, 1 Chi, ranged fire, coefficient 2.1) |
| 50 | Leather Specialization (120227) |
| 54 | Crackling Jade Lightning (117952, a 40 yd channel, no cost, a chance of 1 Chi) |
| 56 | **Rising Sun Kick** (107428, 2 Chi, **8 s**, coefficient 14.4; all targets within the radius take 20 percent more damage from the monk for 15 s, 130320); **Tigereye Brew** (116740 with the counting aura 123980 and the stack aura 125195: 1 charge per 4 Chi spent, up to 20 charges, a use consumes up to 10 for +6 percent damage and healing each, 15 s, 5 s cooldown) |
| 60 | tier 4 talent |
| 68 | Grapple Weapon (117368, disarm, 60 s) |
| 75 | **Storm, Earth, and Fire** (137639, 10 Energy: up to two spirits mirror the attacks, the monk's own damage falls 30 percent per spirit); tier 5 talent |
| 80 | Mastery: Bottled Fury (115636) |
| 81 | Legacy of the White Tiger (116781, 20 Energy, +5 percent critical strike for the group) |
| 82 | Zen Meditation (115176) |
| 87 | Transcendence and Transfer |
| 90 | tier 6 talent |

**Chi budget (derived, arithmetic on the rows above).** Sustained income is Energy-limited: 10 Energy per second buys one Jab (2 Chi) per
4 s, so about **4 Chi per 8 s**, plus Combo Breaker (a free spender on 12 percent of Jabs), Chi Brew (+2 each 45 s from 45) and Energizing
Brew (+60 Energy per 60 s from 36). Needs per 8 s at 56 and above: Rising Sun Kick 2 (it is on an 8 s cooldown), Tiger Palm about 0.5 (one
per 16 s of Tiger Power), Fists of Fury about 1 (3 per 25 s), Touch of Death about 0.3 (3 per 90 s): **about 3.8 of the 4**, so Blackout Kick is
almost never affordable except from a Combo Breaker proc or at the Chi cap. Before 56 Rising Sun Kick does not exist: needs are about 1.8
per 8 s and about 2.2 are spare, one Blackout Kick per 8 s. Value per Chi from the tooltip coefficients (all multiply the same weapon
damage variable): Fists of Fury about 12 (37.5 for 3 Chi, split over the targets), Rising Sun Kick 7.2, Blackout Kick 3.6, Tiger Palm 3.
A Jab at 3 Chi or more wastes Chi at the cap of 4.

## How the bots run this specialisation (read)

- A Windwalker gets the `melee` combat strategy (`WindwalkerMonkStrategy`, `AiFactory.cpp:540`) and `melee aoe`, plus `dps assist` and
  `cure` (`:542`), the generic list with `boost` (`:316`), `behind` for melee damage dealers (`:567-568`), and out of combat `nc`, `dps assist`
  and `cure` (`:784-790`). The `cc` strategy (Paralysis, `MonkStrategies.cpp:446`) is added for the mage and the balance and feral druid
  (`AiFactory.cpp:370`, `:427`, `:444`) but **not** for the monk (the monk case at `:531-543` has no `cc`): Paralysis is never cast. The
  `pvp kite` strategy does not apply (a monk is melee, `:570-571`).
- Rank of an action (`strategy/Strategy.h`): ACTION_DEFAULT 5, NORMAL 10, HIGH 20, INTERRUPT 40, EMERGENCY 90. The engine takes the highest
  action that is useful and possible and **tries the next one when a cast fails in the same tick** (`Engine.cpp:207-246`, per the paladin
  sheet), so an unaffordable Chi spender costs no time. An action whose spell is unknown is skipped; a Chi or Energy shortfall is refused by
  the core's power check (the wave 13 comment at `MonkStrategies.cpp:45-48`).
- Default list (`MonkStrategies.cpp:49-58`): Rising Sun Kick 5.6, Tiger Palm 5.55 (only for Tiger Power upkeep: missing or under 4 s,
  `MonkActions.cpp:196-206`), Fists of Fury 5.5, Blackout Kick 5.4, Chi Wave 5.3, Jab 5.1, melee 5.
- Edges: Stance of the Fierce Tiger 29 (`:194-195`), Energizing Brew 22 at Energy 75 or less (`:204-206`, `MonkActions.cpp:328`), Roll gap-closer
  22 at 11 to 30 yd, **battlegrounds, arenas and a running duel only** (`:209`, `MonkActions.cpp:361`, `PvpRotationValues.cpp:189-195`),
  reach melee 21 (`:188`), Touch of Karma 22 whenever castable (`:200`), Elusive Brew 23 (`:202`), the shared edges (`:152-181`: Spear Hand
  Strike on the target 42, on an enemy healer 41, on any casting enemy 40; Fortifying Brew at low health 25; Invoke Xuen 20; Chi Brew 21 at
  Chi 2 or less or Energy 30 or less; Zen Sphere; Expel Harm at 10 for health between 65 and 85 percent), the area edge "medium aoe"
  (3 enemies within 8 yd) with Rushing Jade Wind 25, Spinning Crane Kick 24, Breath of Fire 23 (`:122-124`), the `boost` strategy with
  Tigereye Brew 22 and Touch of Death 23 (`:440-442`), Resuscitate (`:377`), the spec's stance out of combat 22 (`:382-388`), Legacy of the
  Emperor 21 and 20 and Legacy of the White Tiger 19 and 18 (`:396-405`), Detox on poison and disease for self and party (`:415-423`).
- Skill tiers: no Windwalker line is gated by `BotSkill` (it appears only in the Brewmaster Chi Wave and the Mistweaver files).

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked "derived" comes
from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to `mod-aware-bots-548/src/strategy/Classes/monk/`
at commit `1a8d404`. `MS` is `MonkStrategies.cpp`, `MA` is `MonkActions.cpp`, `MH` is `MonkActions.h`.

### Levels 15 to 29

Known: Jab, Tiger Palm, Blackout Kick, Fists of Fury (10), Roll (5), Combo Breaker and the tier 1 talent (15), Flying Serpent Kick and
Resuscitate (18), Detox (20), Touch of Karma, Touch of Death and Legacy of the Emperor (22), Fortifying Brew (24), Expel Harm (26),
Disable (28). **No Rising Sun Kick, no Tigereye Brew, no Spear Hand Strike, no Energizing Brew, no area attack** (46). The Chi spenders
are Tiger Palm, Fists of Fury and Blackout Kick; Jab gives 2 Chi.

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | always | Stance of the Fierce Tiger (103985, 1) | yes: `MS:194-195` (29), out of combat `MS:382-388` (22) |
| 2 | core | Tiger Power missing or under 4 s, Chi 1 or more | Tiger Palm (100787, 3): guide rank 2 | yes: 5.55 (`MS:52`, `MA:196-206`) |
| 3 | core | off cooldown, Chi 3 or more, Tiger Power at least 4 s (wowtbc) | Fists of Fury (113656, 10): the best Chi value, a 4 s stun | yes: 5.5 (`MS:53`); the cast fails without 3 Chi and the engine moves on. Blackout Kick (row 5) usually takes the Chi first (gap 3) |
| 4 | good | a Combo Breaker proc (aura 116768 for the kick, 118864 for the palm) | Blackout Kick or Tiger Palm at no cost (137384, 15) | **missing:** no line reads the proc (search of `src/` for 116768 and 118864 finds nothing) |
| 5 | core | Chi 4 (at the cap), or Chi 2 or more while Fists of Fury has more than about 6 s of cooldown left (derived: the budget above leaves one Blackout Kick per 8 s before 56) | Blackout Kick (100784, 7) as the Chi dump | **wrong order:** 5.4 at every 2 Chi, ahead of Chi Wave and Jab but behind Fists of Fury, so Fists of Fury waits for 3 Chi that Blackout Kick keeps spending (gap 3) |
| 6 | core | Chi 2 or less, Energy 40 or more (derived: a Jab at 3 Chi or more wastes Chi at the cap of 4) | Jab (100780, 1): +2 Chi in the stance | yes: 5.1 (`MS:56`), reached only when Chi is under 2 today |
| 7 | good | target health at or below the monk's and at least half of the monk's health (derived: a kill worth 3 Chi), 3 Chi, 90 s | Touch of Death (115080, 22) | partly: `boost` 23 (`MS:442`), `MA:276-292`; fires at **any** target at or below the monk's health, so on a mob that dies in two hits (gap 8) |
| 8 | good | health under 70 percent, Energy 40 or more | Expel Harm (115072, 26): heal and +2 Chi | **wrong band:** the shared edge covers 65 to 85 percent only (`MS:181`, `HealthTriggers.h:72-79`); under 65 percent nothing (gap 7) |
| 9 | good | health under 45 percent | Fortifying Brew (115203, 24) | yes: `low health` 25 (`MS:160`) |
| 10 | good | health under 60 percent and an enemy on the monk, or three enemies on the monk (derived) | Touch of Karma (122470, 22): a 10 s absorb | **wrong trigger:** "whenever castable" 22 (`MS:200`) spends the 90 s cooldown on the first pull (gap 6) |
| 11 | good | a curse, poison or disease on the monk or a member | Detox (115450, 20) | yes: `MS:415-423` |
| 12 | good | out of combat, a member without a stat buff from anyone | Legacy of the Emperor (115921, 22) | partly: `MS:396-399` checks only the name "Legacy of the Emperor" (gap 9) |
| 13 | expert | the target is more than 11 yd away in a duel | Roll (109132, 5) | yes, duel and PvP maps only (`MS:209`) |
| 14 | expert | derived: a fleeing or kiting enemy | Disable (116095, 28): snare, then root | missing |

Before the spells are learned (derived): 15 to 21 the buttons are Jab, Tiger Palm, Fists of Fury and Blackout Kick; Combo Breaker procs
free the last two.

### Levels 30 to 44

As above, plus the tier 2 talent and Nimble Brew (30), Spear Hand Strike (32), Energizing Brew (36), Sparring (42), Paralysis (44).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 15 | good | off cooldown (15 s), a free global cooldown | Chi Wave (115098, talent 30): Method and wowtbc both list it above the filler | yes: 5.3 (`MS:55`) |
| 16 | core | an enemy casts | Spear Hand Strike (116705, 32): melee range, 15 s | yes: three edges 42, 41, 40 (`MS:152-155`, `:167`; the on-caster action tests melee range, `MA:354-359`) |
| 17 | good | Energy 40 or less, in combat (derived: 60 Energy over 6 s with 10 per second regeneration overflows above that) | Energizing Brew (115288, 36), 60 s | **threshold wide:** Energy 75 or less (`MA:328-336`); low value (gap 10) |
| 18 | expert | derived: an enemy caster with no one to interrupt it | Paralysis (115078, 44) on a second caster | missing (no `cc` strategy for the monk) |

### Levels 45 to 59

As above, plus the tier 3 talent (45), Spinning Crane Kick (46), Spinning Fire Blossom (48), Crackling Jade Lightning (54), Rising Sun
Kick and Tigereye Brew (56).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 19 | core | Chi 2 or more, off cooldown (8 s); keep the 15 s debuff | Rising Sun Kick (107428, 56): rank 1 in both guides after the Tiger Power upkeep | yes: 5.6 (`MS:51`); unknown below 56 (see the unknown-name table) |
| 20 | good | Chi 2 or less and one charge (wowtbc) | Chi Brew (115399, talent 45): +2 Chi, +2 Tigereye charges | yes: `MS:173` (21), `MA:316-326`; the Energy clause (30 or less) casts it even at 3 or 4 Chi and wastes Chi at the cap (low value) |
| 21 | core | 10 or more charges (both guides), the buff not running; spend at 15 or more even without a hard fight (a cap of 20) | Tigereye Brew (116740, 56) | **wrong threshold:** any charge with a hard fight ("balance" 50 or less, which is also true when the bot is alone, `AttackerCountValues.cpp:55-105`); one charge is 6 percent for 15 s (`MA:259-274`, gap 5) |
| 22 | core | three or more enemies within 8 yd of the target, Energy 40 or more, Chi 2 or less | Spinning Crane Kick (101546, 46) **in place of Jab** (Method), +1 Chi | **wrong order:** the area edge puts it at 24 above Rising Sun Kick, Fists of Fury and Tiger Palm; the 2.25 s channel then blocks every other cast (`MS:122-124`; `AI/PlayerbotAI.cpp:3874`; gap 2) |
| 23 | good | Rising Sun Kick and Fists of Fury before the area attack | all targets in 8 yd take 20 percent more; Fists of Fury hits the cone | the same order gap |
| 24 | good | the target is out of reach and the monk is far | Spinning Fire Blossom (115073, 48), Crackling Jade Lightning (117952, 54) | missing, low value |

### Levels 60 to 74

As above, plus the tier 4 talent (60), Grapple Weapon (68). No new damage button.

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 25 | expert | a stun on one big enemy while the group kills the rest (derived) | Leg Sweep (119381, talent 60), 45 s | idle (no line) |
| 26 | good | a hard fight | Touch of Karma, Fortifying Brew, as rows 9 and 10 | as above |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 75 | Storm, Earth, and Fire on two extra targets (both guides), tier 5 talent | no line for Storm, Earth, and Fire; the tier 5 premade pick (Diffuse Magic) is inert |
| 80 | Mastery: Bottled Fury | passive |
| 81 | Legacy of the White Tiger for the group | lines exist (`MS:401-405`); the duplicate-buff check applies (gap 9) |
| 90 | Method: stance, Tiger Power, Touch of Death, Tigereye Brew at 10, Invoke Xuen, Energizing Brew or Chi Brew, Rising Sun Kick, Fists of Fury, Blackout Kick on Combo Breaker, Chi Wave, Jab and Blackout Kick; area: Storm, Earth, and Fire and Spinning Crane Kick or Rushing Jade Wind | all the buttons exist except Storm, Earth, and Fire; the area edge and Blackout Kick orders are the gaps above |

## Cooldowns and defensives

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Touch of Death | 22 | Method: as soon as possible; wowtbc: on targets below your health | a target with at most the monk's health and at least half of it; 3 Chi free | yes, `boost` (no health floor) |
| Tigereye Brew | 56 | at 10 or more charges | 10 or more charges, or 15 or more at any time | any charge on a hard fight |
| Energizing Brew | 36 | at low Energy | Energy 40 or less, in combat | Energy 75 or less |
| Chi Brew | talent 45 | at 1+ charges and 2 Chi or less (wowtbc) | same | Chi 2 or less (and Energy 30 or less) |
| Invoke Xuen | talent 90 | on cooldown | boss, elite pack or downtime (Method dungeon) | line fires at every pull; the premade does not take it |
| Storm, Earth, and Fire | 75 | area, two extra targets | three or more enemies (derived) | no line |
| Fortifying Brew | 24 | wowtbc defensive 1 | health under 45 percent | yes, `low health` 25 |
| Touch of Karma | 22 | wowtbc defensive 2 | health under 60 percent with an enemy on the monk | on cooldown, "always" |
| Zen Meditation | 82 | wowtbc defensive 3 | rarely: a melee hit breaks it | no line |
| Diffuse Magic / Dampen Harm | talent 75 | wowtbc defensive 4 | a spell-heavy pull / a hard hit | no Windwalker line |
| Expel Harm | 26 | opener (both guides) | health under 70 percent, and as a Chi builder when the Chi is 0 and Jab is not affordable | 65 to 85 percent only |
| Spear Hand Strike | 32 | not named | every interruptible cast of the pull target | yes, three edges |
| Detox | 20 | not named | poison and disease on the monk or a member | yes |
| Nimble Brew | 30 | not named | stunned or rooted in a duel (derived) | no line |

Group buffs (who casts which, derived from the data and the core): Legacy of the Emperor (22) is the same stats buff as Blessing of Kings and Mark of
the Wild; the core makes the three **mutually exclusive** (the core's `spell_group` SQL, `2026_07_23_world_00.sql`: group 1118,
exclusive same effect), so in a group with a paladin or a druid the monk's cast adds nothing and its 20 Energy is wasted. The
party check (`PartyMemberWithoutAuraValue`) tests the aura name from the trigger only; the paladin's Kings trigger already lists two
names (`PaladinTriggers.h:87`). Legacy of the White Tiger (81, Windwalker only) is a critical strike buff of the same group as the other
critical strike raid buffs (`2026_10_25_world_10.sql`, group 1141), so a mage's or druid's version cancels it. The Windwalker is the only
monk that casts Legacy of the White Tiger; all three specialisations cast Legacy of the Emperor, and the first to look casts it.

## Duel and PvP basics (derived, short; the PvP pack is future work)

The monk is melee with no ranged attack before 48, so a kiting caster is the hard case (the owner watched a mage bot duel a monk bot on
2026-11-10). The tools by level: Roll (5, gap closer, exists as `roll gap` for duels and battlegrounds, `MA:361`), Flying Serpent Kick (18,
25 s, a long leap; no line), Disable (28, snare then root, no cooldown; no line), Spear Hand Strike (32, silences a caster in front;
exists), Leg Sweep (talent 60, 5 s stun, 45 s), Paralysis (44, an incapacitate, never cast), Touch of Karma (22, defensive against a
burst; exists as "whenever castable"), Fortifying Brew (24), Tiger's Lust (talent 15, clears a root; no line), Nimble Brew (30, clears
stun and root, 2 min). The bots' duel does close the gap with Roll from 11 yd; the rest is missing (gap 11).

## Spell names the Windwalker strategies ask for that a Windwalker of level 15 to 59 does not know

`SpellIdValue` logs `[SID] ... qualifier='<name>' resolved=0` whenever a name matches no known, non-passive spell
(`strategy/value/SpellIdValue.cpp`) and caches the answer 20 s per name and bot. From the code (inference, not checked against the log),
the names a Windwalker asks for and does not know:

| Name | Known from (Windwalker) | Asked from |
|------|-------------------------|------------|
| `rising sun kick` | 56 | default list, first entry (`MS:51`), 15 to 55 |
| `elusive brew` | **never** (Brewmaster row, 36; no Windwalker row in SpecializationSpells) | `elusive brew` edge (`MS:202`), every check; `ElusiveBrewTrigger` reads aura 128939 which a Windwalker never has |
| `breath of fire` | **never** (Brewmaster row, 18) | "medium aoe" edge (`MS:124`) |
| `rushing jade wind` | talent 90 (the premade's pick), nothing before | "medium aoe" edge (`MS:122`), 15 to 89 |
| `spinning crane kick` | 46 | "medium aoe" edge (`MS:123`), 15 to 45 |
| `invoke xuen, the white tiger` | talent 90, not in the premade | shared edge (`MS:170`), every check |
| `zen sphere` | talent 30, **not in the premade** | shared edge (`MS:178`), every check |
| `chi wave` | talent 30 (premade) | default list (`MS:55`), 15 to 29 |
| `chi brew` | talent 45 (premade) | shared edge (`MS:173`), 15 to 44 |
| `spear hand strike` | 32 | three edges (`MS:152-167`), 15 to 31, when the target casts |
| `fortifying brew` | 24 | `low health` (`MS:160`), 15 to 23 |
| `expel harm` | 26 | `almost full health` (`MS:181`), 15 to 25 |
| `touch of karma`, `touch of death`, `legacy of the emperor` | 22 | `MS:200`, `:442`, `:396-399` |
| `energizing brew` | 36 | `MS:206`, 15 to 35 |
| `tigereye brew` | 56 | `boost` (`MS:440`), 15 to 55 |
| `detox`, `resuscitate` | 20, 18 | cure edges (`MS:415-431`), dead-member edge (`MS:377`) |
| `legacy of the white tiger` | 81 | out of combat (`MS:401-405`), 15 to 80 |

Fix direction: a level and talent test on the default list entries and on each edge (as the paladin sheet proposes), drop `elusive brew`
and `breath of fire` from the Windwalker lists, and the `resolved=0` count per bot should fall to near zero.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Fists of Fury arrives at 10, Combo Breaker at 15, Touch of Karma and Touch of Death at 22, Expel Harm at 26,
  Spear Hand Strike at 32, Energizing Brew at 36, Spinning Crane Kick at 46, **Rising Sun Kick and Tigereye Brew at 56**, Storm, Earth, and
  Fire at 75, Mastery at 80, Legacy of the White Tiger at 81. Below those levels the lists start without them.
- Chi cap 4 (5 with Ascension) from level 1; Jab gives **2** Chi in the stance (the guides say "Jab to generate Chi").
- Blackout Kick is a dump, not a rotation button (both guides rank it last; the data's coefficients per Chi say the same). The bots
  rank it above Chi Wave and Jab.
- Touch of Karma is a defensive in wowtbc (second) and absent from Method's priority list; the bots cast it on cooldown at the pull.
- Tiger Power does not stack and lasts 20 s (the module's own comment at `MA:198-202`); the guides say only "maintain".
- Elusive Brew and Breath of Fire are Brewmaster spells; the Windwalker lists name both.
- Method's 7.5 percent hit and expertise are level 90 numbers; the module's calculator uses 8 and 26.
- The glyph of Touch of Death's "raids only" is a guide recommendation, not a game rule.
- Overgear, Icy Veins and Wowhead were not readable, so two guides were compared.
- Method's and wowtbc's consumables, gear lists and Engineering notes have no counterpart in the bots (design decision 0016 to 0021).

## Gap list (ordered)

Config (can go at once):

1. Talents `AiPlayerbot.PremadeSpecLink.10.2` (`conf/playerbots.conf.dist:2910`): **keep `002220`**. No change is needed for the band
   (Celerity is the only tier 1 pick with an effect today). From 75 and 90 see questions 1 and 2.
2. Glyphs `PremadeSpecGlyph.10.2` (`:2911`) from `85700,90715,87900,87887,87889,85697` to **`85700,90715,87892,87887,87889,87900`**:
   Fists of Fury (parry while the bot stands in a 4 s channel) in the level 50 slot instead of Touch of Karma's range, which an in-melee bot
   cannot use, and Touch of Karma in the level 75 slot instead of Spinning Crane Kick (inert, the bots never move in a channel). The
   comment at `:2908` needs the same change. Question 4.
3. Stat weights for the Windwalker (`Mgr/Item/StatsWeightCalculator.cpp:678-687`): **Mastery 0 below level 80** (1.3 from 80), Agility 2.5,
   Critical 2.0, Haste 1.8, Hit 1.8, Expertise 1.7. Check the hit and expertise overflow values (`StatsWeightCalculator.h:26-28`,
   8 and 26; Method says 7.5 percent each).

Strategy (class fixes, wait for the owner's in-game test), ranked by value for levels 15 to 60:

4. **Unknown-name spam** (the table above): level and talent gates on the default list and each edge, delete `elusive brew` (`MS:202`) and
   `breath of fire` (`MS:124`) from the Windwalker, delete `zen sphere` unless the talent is taken. Verify with the debug log: `resolved=0`
   lines per bot fall to near zero.
5. **Area attacks outrank the single-target buttons** (`MS:121-124`): the "medium aoe" edge puts Rushing Jade Wind 25, Spinning Crane
   Kick 24 and Breath of Fire 23 above every default action, and the 2.25 s Spinning Crane Kick channel blocks every other cast. The
   Brewmaster got the same fix (BT-133: default band under the upkeep lines). For the Windwalker: Rising Sun Kick and Fists of Fury stay
   first, Spinning Crane Kick replaces Jab (Method) for three or more targets and Chi 2 or less, Rushing Jade Wind stays at 90 only.
   Inference from the numbers; confirm in an action log.
6. **Blackout Kick and the Chi budget** (`MS:49-58`): make Blackout Kick the dump (Chi 4, or before 56 while Fists of Fury is on cooldown with
   more than about 6 s left), add a **Combo Breaker line** (aura 116768: Blackout Kick; 118864: Tiger Palm) at about 5.45, and let Jab fire
   only at Chi 2 or less (the cap is 4). The largest rotation gain for 15 to 60 (Fists of Fury at 3 Chi, Rising Sun Kick on time).
7. **Tigereye Brew at 10 or more charges** (`MA:259-274`): require 10 charges (both guides), or 15 or more at any time; today any
   charge with a hard fight, and "hard" is also true when the bot is alone.
8. **Touch of Karma as a defensive** (`MS:200`): health under 60 percent with an enemy on the monk, or three or more enemies on the
   monk; today it is cast at the first pull and unavailable for the next 90 s.
9. **Expel Harm below 70 percent** (`MS:181`, `HealthTriggers.h:72-79`): add the Brewmaster's trigger (BT-133 "brewmaster expel harm") to the
   Windwalker; under 65 percent today nothing casts it.
10. **Touch of Death target filter** (`MA:276-292`): require the target to have at least half of the monk's health (a kill worth 3 Chi) or
    to be an elite; today a mob that dies in two hits takes it.
11. **Legacy of the Emperor and the other stat buffs** (`MH:373-376`, `MS:396-399`): list Blessing of Kings and Mark of the Wild in the
    party check qualifier (`"legacy of the emperor,blessing of kings,mark of the wild"`, as `PaladinTriggers.h:87` does for two names), so
    a group with a paladin or a druid does not cost 20 Energy a cast. Same for Legacy of the White Tiger at 81 with the other critical
    buffs. To check live how the core treats the second cast.
12. **Energizing Brew at Energy 40 or less** (`MA:328-336`): low value; today Energy 75 or less.
13. **Duel and PvP gap closers** (expert): Disable on a fleeing or kiting enemy (28), Flying Serpent Kick (18), Nimble Brew (30) when
    rooted or stunned in a duel, Paralysis on a second caster (add `cc` for the monk, `AiFactory.cpp:531-543`); Tiger's Lust if question 1
    is answered with it.
14. **Skill tiers**: no gating. Lines marked good and expert above (Combo Breaker, Expel Harm band, Touch of Karma, Chi Wave, Touch of
    Death filter, Energizing Brew, Disable, Nimble Brew, Paralysis, Spinning Crane Kick below 3 targets) are the ones a casual bot should
    drop.
15. **Storm, Earth, and Fire** (75): a line for three or more enemies; no line today. Out of the band.
16. **To check live:** Fists of Fury finishes its 4 s channel (no follow, formation or avoid-area move inside it; the channel has the
    movement interrupt flag, `SpellInterrupts` row 26041); the Energy and Chi trace of one fight; whether `behind` keeps the bot behind
    the target so the Blackout Kick damage over time applies; the second Legacy of the Emperor cast in a group with Mark of the Wild;
    Touch of Death on elites that exceed the monk's health (the cast refuses); Tigereye Brew counts only non-triggered casts with a Chi
    cost (`spell_monk.cpp:5429-5435`), so a free Combo Breaker cast adds none.

Owner questions:

1. Tier 1 (level 15): Celerity (today, the Roll gap-closer line gets a charge) or Tiger's Lust (Method, clears a root, but needs a new line).
2. Tier 6 (level 90): Rushing Jade Wind (today) or Invoke Xuen (Method's single target pick; a line fires it at every pull, which for a
   group fight means the boss and the trash alike).
3. Blackout Kick: keep "every 2 Chi" (today, simple) or the guides' "dump at the cap or on a proc" (needs the Chi arithmetic above and
   a live Chi trace).
4. Glyphs: Fists of Fury in the level 50 slot and Touch of Karma at 75 (proposal), or keep the guide set unchanged (Touch of Karma at 50,
   Spinning Crane Kick at 75). Touch of Death glyph in slot 1: keep (both guides) or drop for the shorter cooldown while leveling?
5. Touch of Karma: on cooldown at the pull (today) or as a defensive only (proposal).
6. Touch of Death: any target at or below the monk's health (today) or only targets worth at least half of it (proposal).
7. Legacy of the Emperor: always when the name is missing (today) or skipped when Kings or Mark of the Wild is on the member
   (proposal; avoids the repeated cast).
8. Duels: should a Windwalker use Disable, Flying Serpent Kick and Nimble Brew on its own (expert tier only), or wait for the PvP pack?

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 3 players of this specialisation (rating 2444 to 2665). 2v2: 3 players (rating 2656 to 2701). Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** pooled (6 players)
- **BT-324 result:** verified (BT-453, pooled 6 players)
- **Set now:** `PremadeSpecLinkPvp.10.2 = 102200`, `PremadeSpecGlyphPvp.10.2 = 85700,90715,87900,87889,87890,87897`

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Tiger's Lust 100% / 100% / 6 of 6 | Tiger's Lust | Tiger's Lust | matches |
| 2 (30) | Chi Wave 100% / 100% / 6 of 6 | Chi Wave | Chi Wave | matches |
| 3 (45) | Chi Brew 100% / 100% / 6 of 6 | Chi Brew | Chi Brew | matches |
| 4 (60) | Leg Sweep 100% / 100% / 6 of 6 | Leg Sweep | Leg Sweep | matches |
| 5 (75) | Healing Elixirs 100% / 100% / 6 of 6 | Healing Elixirs | Healing Elixirs | matches |
| 6 (90) | Rushing Jade Wind 100% / 100% / 6 of 6 | Rushing Jade Wind | Rushing Jade Wind | matches |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Touch of Death (85700) | 100% | 100% | 6 of 6 | yes | yes |
| major | Touch of Karma (87900) | 100% | 100% | 6 of 6 | yes | yes |
| major | Paralysis (87897) | 67% | 67% | 4 of 6 | yes | yes |
| major | Sparring (87898) | 33% | 33% | 2 of 6 | no | no |
| minor | Zen Flight (87890) | 67% | 67% | 4 of 6 | yes | yes |
| minor | Blackout Kick (90715) | 67% | 67% | 4 of 6 | yes | yes |
| minor | Jab (87884) | 33% | 67% | 3 of 6 | no | no |
| minor | Rising Tiger Kick (87885) | 100% | 0% | 3 of 6 | no | no |
| minor | Water Roll (87889) | 0% | 0% | 0 of 6 | yes | yes |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **What stays (pooled 3 + 3 = 6):** all six tiers match, 6 of 6. Majors Touch of Death and Touch of Karma 6 of 6 and Paralysis 4 of 6 are held; minors Zen Flight and Blackout Kick 4 of 6 are held. Water Roll sits in the third minor slot and is on no ladder build; no minor reaches 60 percent to replace it (Jab and Rising Tiger Kick, the cosmetic one, 3 of 6 each), so it stays.
