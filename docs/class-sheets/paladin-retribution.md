# Paladin: Retribution (damage, melee)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). The mana plan is left
  for later (it needs a live measurement). The "bots today" columns below describe the code before the 2026-11-11
  implementation.
- **Sources:**
  - Method, "Retribution Paladin Class Guide, MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 23 July 2025), read
    2026-11-11 through a scripted fetch (the talent builds are images; the text names the picks and gives a raid and a dungeon glyph set).
  - wowtbc.gg, "Retribution Paladin" MoP Classic PvE guide (no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent, GlyphProperties,
    SpellCooldowns, SpellPower, SpellEffect, SpellMisc, SpellDuration, SpellRadius, SpellCategories, SpellAuraRestrictions,
    SpellEquippedItems; `data/db2/Item-sparse.db2` for glyph item ids) through `spec_data.py` and short read-only scripts; the
    core source (the SkyFire core: `scripts/Spells/spell_paladin.cpp`, `PaladinSpellCalculations.h`,
    `Entities/Unit/Unit.cpp`, `Entities/Player/Player.cpp`) for the spell scripts and the Holy Power cap.
  - Not readable: Overgear and Icy Veins (HTTP 403, as noted in the skill), Wowhead (needs a browser). Two guides were compared,
    not three.
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the tank's
  marks), 61 to 90 briefly. Facts shared with the other paladin sheets (`paladin-protection.md`, `paladin-holy.md`: talent
  notes, the pull, the blessing triggers) are not repeated.

Which source was richer: **the stat order with the caps, the dungeon glyph set, the talent picks with reasons, the Holy Avenger
versus Sanctified Wrath note** Method; **the option list of all 18 talents, the 12-line opener, the Hammer of Wrath and
Execution Sentence rules, the Inquisition refresh rule, the 2 to 3 and 4+ target split, the defensive list** wowtbc. Neither guide
has levelling advice (wowtbc has one line: Glyph of Inquisition for leveling), a mana plan, an interrupt rule, a blessing rule or a
word on what a Retribution paladin does before Templar's Verdict (10), Divine Storm (34), Exorcism (46) or Inquisition (81), so
those lines come from the 5.4.8 data and are marked "derived". The guides disagree on these points:

- **Holy Power cap.** Both guides say "never sit at 5". In this server the cap is **3** until level 85: `Unit::GetCreatePowers`
  returns 3 for Holy Power (`Unit.cpp:5669`), and the passive Boundless Conviction (115675, level 85) stores "an additional 2
  beyond the maximum of 3" (spell text); no ability spends more than 3 (`MaxSpentHolyPower`, `PaladinSpellCalculations.h:19`).
  So below 85 Templar's Verdict at 3 is the first spender, never "wait for 5". Data wins (derived).
- **Divide between Divine Storm / Hammer of the Righteous and the single-target buttons.** Method: more than 1 target; wowtbc: 2 to
  3 targets. The effect data says weapon damage 275 percent for Templar's Verdict against 100 percent per target for Divine Storm,
  and 125 percent for Crusader Strike against 20 plus 35 per target for Hammer of the Righteous, so the break-even is 3 targets
  (Divine Storm) and 3 to 4 (Hammer, equal at 3). Keep the bots' 3 (derived, question 5).
- **Tier 15, 45, 75 picks.** Method: Pursuit of Justice, Selfless Healer, Holy Avenger first then Sanctified Wrath; wowtbc marks
  none. The premade takes Speed of Light, Eternal Flame and Sanctified Wrath. Questions 1 to 3.
- **Glyphs.** Method raid: Hand of Sacrifice, Templar's Verdict, Inquisition; Method dungeon: Mass Exorcism, Double Jeopardy,
  Inquisition; wowtbc: Templar's Verdict, Double Jeopardy, Hand of Sacrifice, with Mass Exorcism for AoE. The dungeon set is used
  here (a damage dealer in a group), then adjusted for what a bot can use.
- **Inquisition.** Method: keep it up; wowtbc: refresh under 2 seconds. wowtbc's rule is used (the core allows a recast within 2 s
  and adds the remaining time, `spell_paladin.cpp:1370-1378`). Level 81 only.
- **Spending Holy Power during Avenging Wrath.** wowtbc spends at 3 during the buff before Judgment; Method does not say. With a
  cap of 3 this is the normal rule below 85.
- **Opener.** Method: Judgment and Exorcism while closing, then Crusader Strike, Inquisition, Guardian, Avenging Wrath, Execution
  Sentence; wowtbc: Judgment while running in, then Crusader Strike, Inquisition, cooldowns, then a Hammer of Wrath chain. All of it
  is level 75 or higher except the first two buttons.

## Talents

Tiers and columns are from Talent.dbc (shared by the three paladin specialisations). "Bots today" is
`AiPlayerbot.PremadeSpecLink.2.2 = 001112` (`conf/playerbots.conf.dist:2696`; one digit per tier, column counted from 0): Speed of
Light, Fist of Justice, Eternal Flame, Unbreakable Spirit, Sanctified Wrath, Execution Sentence. The talent-override swap
(`BotOverrideSpell`, `AI/PlayerbotAI.cpp:3731-3790`) means the name `hammer of justice` casts Fist of Justice (105593) and the name
`word of glory` casts Eternal Flame (114163) when those talents are picked (Talent.dbc override column: 853 and 85673).

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (001112) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Pursuit of Justice (Method default; a passive: speed at all times plus more per Holy Power up to 3) | Speed of Light (an active every 45 s), Long Arm of the Law (Judgment gives speed; wowtbc lists) | 26023 / 85499 / 87172 | **Speed of Light, differs and inert**: no line casts it (`speed of light` appears nowhere in `src/`) |
| 2 (30) | Fist of Justice (Method default) | Repentance, Evil is a Point of View (by encounter) | 105593 / 20066 / 110301 | Fist of Justice, same; the three Hammer of Justice edges work through the override |
| 3 (45) | Selfless Healer (Method) | Eternal Flame (Method: not recommended), Sacred Shield (heavy damage only) | 85804 / 114163 / 20925 | **Eternal Flame, differs**; reached only through the name `word of glory` (no Retribution Eternal Flame line) |
| 4 (60) | Unbreakable Spirit (Method: easier; or Clemency) | Hand of Purity (wowtbc lists), Clemency with group utility | 114154 / 105622 / 114039 | Unbreakable Spirit, same; the bot presses all three buttons it shortens (Divine Shield, Divine Protection, Lay on Hands) |
| 5 (75) | Holy Avenger first, Sanctified Wrath with better gear (Method) | Divine Purpose (wowtbc: tied to the tier 16 set) | 105809 / 53376 / 86172 | Sanctified Wrath; a passive, no line needed; the Holy Avenger line exists (`DPS:75`) |
| 6 (90) | Execution Sentence (single target) | Light's Hammer (area), Holy Prism | 114157 / 114158 / 114165 | Execution Sentence, same; the line exists (`DPS:60`) |

Notes on the picks (data, not guide):

- Pursuit of Justice (26023) is passive and helps a bot that follows a leader; Speed of Light (85499) needs a cast no line makes.
- Selfless Healer (85804, passive): each successful Judgment gives a stack (up to 3) that takes 35 percent off the cast time and
  the mana cost of the next Flash of Light and adds 20 percent to its heal on others (effects of 114250; core script
  `spell_paladin.cpp:223-277` and `:438-476`). Three stacks make Flash of Light instant and free, which the bots' existing Flash of
  Light lines (`DPS:78`, `GPS:52-54`, `GPS:64-66`) use without any new code. Eternal Flame replaces Word of Glory (spell text) and
  spends all the Holy Power present (1 to 3) as a heal over time; for a damage dealer that spend competes with Templar's Verdict.
- Execution Sentence (114157) is a hammer that falls after a delay; it snapshots Avenging Wrath and Guardian (wowtbc), which the
  bots' order (Avenging Wrath and Guardian above the default list) already gives.
- Holy Avenger (105809): 18 s, Holy Power generators deal extra damage and give 3 Holy Power (spell text); with a cap of 3 each
  generator refills the bar, so it is a burst button (Judgment gives 3, `spell_paladin.cpp:390-391`). Sanctified Wrath makes
  Avenging Wrath last longer and cuts the cooldown of Hammer of Wrath (spell text).
- Fist of Justice replaces Hammer of Justice (spell text), so the PvP stun and interrupt edges keep working through the override.

Proposed link (derived, from Method's picks and the lines that exist): **`200112`** = Pursuit of Justice, Fist of Justice,
Selfless Healer, Unbreakable Spirit, Sanctified Wrath, Execution Sentence. Only the tier 15 and tier 45 digits change. Owner
questions 1 and 2.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`, effect text from Spell.dbc. Slots unlock in
pairs at 25, 50 and 75 (config comment `conf/playerbots.conf.dist:2631-2636`). The config order is major, minor, major, minor,
minor, major. "Bots today" is `AiPlayerbot.PremadeSpecGlyph.2.2 = 41097,43365,41092,43340,43369,104051` (`conf.dist:2697`).

| Type | Glyph | Item id | Why (one clause) | Bots today |
|------|-------|--------:|------------------|------------|
| Major | Templar's Verdict | 41097 | both guides; 10 percent less damage taken for 6 s after Templar's Verdict or Exorcism (54926, 115668) | yes, slot 1 (level 25) |
| Major | Mass Exorcism | 83107 | Method dungeon set, wowtbc AoE; Exorcism at melee range, 25 percent damage to all enemies within the radius of the target (122028, 122032) | no |
| Major | Double Jeopardy | 41092 | both guides; the next Judgment deals more, but **only when the previous Judgment hit a different target** (54922; core `spell_pal_judgment`, `HandleDoubleJeopardy`) | yes, slot 3 (level 50); **pays nothing** while the bots Judgment the one current target |
| Major | Divine Protection | 41096 | both guides (physical damage reduction); Divine Protection becomes 20 percent magic plus a physical share instead of magic only (54924); the bot casts it at 50 percent health (`DPS:77`) and the spell is learned at 26 | no |
| Major | Inquisition | 45747 | Method dungeon and raid, wowtbc leveling; a killing blow adds time to Inquisition (63225); **dead below 81** (Inquisition is learned at 81) | no |
| Major | Hand of Sacrifice | 104051 | both guides; the spell no longer redirects damage to the paladin (146957); **dead until a line casts Hand of Sacrifice** (learned at 80; no Retribution line exists) | yes, slot 6 (level 75), inert |
| Major, other | Harsh Words | 41107 | Method other; Word of Glory can hurt an enemy, but not with Eternal Flame (54938) | no |
| Minor | Contemplation, the Mounted King, Fire From the Heavens | 43365, 43340, 43369 | cosmetic or harmless (both guides: minors give no damage) | yes, slots 2, 4, 5 |
| Minor, PvP | Righteous Retreat | 85716 or 80585 | Method: world PvP only | no |

Disagreement: see above. **Proposed (derived): `41097,43365,83107,43340,43369,41096`**: Templar's Verdict, Mass Exorcism instead of
Double Jeopardy (the bots never alternate Judgment targets, and a dungeon pack of 3 or more is exactly what Method's dungeon set
is for), Divine Protection instead of Hand of Sacrifice at 75 (works from the day the slot opens; Inquisition would be dead for six
more levels). Needs a live check that a glyphed Exorcism still casts at melee range for the bot (question 3).

## Stat order

Method: Strength, then Haste, Mastery, Critical Strike; Hit and Expertise to about 7.5 percent each (the caps come first). wowtbc
has no stat order. The caps are level 90 numbers and cannot be reached with the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **A two-handed weapon is mandatory.** Sword of Light (53503, level 10) carries "Sword of Light part two" (20113): +30 percent
  damage with a two-handed weapon (the equipped-item restriction of 20113 is the mask of the two-handed axe, mace, polearm and
  sword subclasses). Crusader Strike, Templar's Verdict and Divine Storm need a melee weapon of either grip. A slow two-handed
  weapon in the main hand beats any one-hand and shield; the bots' calculator already does this (`Mgr/Item/StatsWeightCalculator.cpp:840`
  multiplies the weight of a non-two-handed weapon by 0.1 and `:1136-1140` boosts a 3.4 s or slower two-hander).
- **Strength first** (Method; Strength is the attack power stat of the paladin, Plate Specialization at 50 adds 5 percent Strength
  in all-plate, 86539). Agility gives a Retribution paladin almost nothing (no attack power, spell text of Sword of Light and class
  data), Intellect only mana.
- **Haste is the best secondary from level 58**: Sanctity of Battle (25956, level 58) lets melee haste shorten the cooldown and the
  global cooldown of Judgment, Crusader Strike, Hammer of the Righteous, Exorcism and Hammer of Wrath (spell text). Below 58 Haste
  is only swing speed, close to Critical Strike.
- **Hit and Expertise before the other secondary stats** until the caps (Method); unreachable below 90.
- **Mastery is worth nothing below level 80**: Mastery: Hand of Light (76672) is learned at 80 (SpecializationSpells). From 80
  Method ranks it above Critical Strike.
- **Spell Power is worth nothing**: Sword of Light sets spell power to 50 percent of attack power and "you no longer benefit from
  other sources of spell power" (53503 text and effect). Mana comes from Sword of Light too (below).
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:495-505`): Strength 2.5, Agility 0.5, Attack Power 0.5, Armor Penetration 1.5
  (the stat no longer exists on 5.4.8 gear; harmless), Hit 1.9, Critical 1.7, Haste 1.6, **Mastery 1.75 at every level** (dead
  below 80, and above Haste), Expertise 2.0, Melee DPS 9.0. Spell Power and Intellect are not set (0). Proposed (derived):
  **Mastery 0 below level 80 and 1.75 from 80; Haste 1.8 from level 58 (1.4 below); Agility 0.2; Critical 1.6**; keep Strength
  2.5, Hit 1.9, Expertise 2.0, Attack Power 0.5. Whether the hit cap used for the overflow equals Method's 7.5 percent is to check.

## What the specialisation knows, by level (5.4.8 data)

A Retribution paladin knows the Paladin class line (SkillLineAbility 800) plus the Retribution rows of SpecializationSpells (spec
70). Ids are the spells the player casts; the data holds old rows of the same name. Costs are percent of base mana (SpellPower).
The global cooldown is 1.5 s; seals 1.0 s. **Holy Power cap: 3 until level 85** (see above); it decays by 1 every 10 s out of
combat (`Player.cpp:2662-2666`).

Retribution does **not** know Consecration (26573 is a Protection row, level 34), Holy Wrath (119072, Protection), Divine Plea
(54428, Holy), Holy Shock, Holy Light, Divine Light, Holy Radiance, Beacon of Light, Reckoning, Avenger's Shield or Shield of the
Righteous (Holy and Protection rows). Retribution has Hammer of the Righteous (53595, spec 66 and 70 both).

| Level | Spell (id) |
|------:|------------|
| 1 | Crusader Strike (35395, weapon 125 percent, 4.5 s, 15.0 percent mana, **grants 1 Holy Power** by its energize effect from level 1; shows Weakened Blows), Seal of Righteousness row 101423 (the casting spell 20154 is level 42) |
| 3 | Seal of Command (105361, +10 percent weapon damage) |
| 5 | Judgment (20271, 6 s, 12.0 percent mana, magic attack, **needs a seal up**; grants Holy Power only from 28) |
| 7 | Hammer of Justice (853, 60 s, 3.5 percent) |
| 9 | Word of Glory (85673, 1 to 3 Holy Power, 1.5 s category cooldown) |
| 10 | Sword of Light (53503: +30 percent two-handed damage, spell power from attack power, periodic mana, Word of Glory and Flash of Light heal more); **Templar's Verdict** (85256, 3 Holy Power, 275 percent weapon); the specialisation is chosen here |
| 12 | Righteous Fury (25780; a tank spell, not for this specialisation) |
| 13 | Redemption (7328) |
| 14 | Flash of Light (19750, 37.8 percent mana) |
| 15 | tier 1 talent |
| 16 | Lay on Hands (633, 10 min) |
| 18 | Divine Shield (642, 5 min, 8 s) |
| 20 | **Hammer of the Righteous** (53595: 20 percent weapon on the target plus a 35 percent wave to everything within 8 yd, shares the 4.5 s cooldown with Crusader Strike, 1 Holy Power); Cleanse (4987, poison and disease for this specialisation); Warhorse |
| 24 | **Seal of Truth** (31801, +12 percent weapon damage and Censure; replaces Seal of Command) |
| 26 | Divine Protection (498, 60 s, 10 s) |
| 28 | **Judgments of the Bold** (111529: Judgment grants 1 Holy Power and applies Physical Vulnerability) |
| 30 | Blessing of Kings (20217, 5.0 percent mana), tier 2 talent |
| 32 | Seal of Insight (20165: melee hits heal the paladin) |
| 34 | **Divine Storm** (53385, 3 Holy Power, 100 percent weapon to everything within 8 yd) |
| 36 | **Rebuke** (96231, melee range, 15 s, 4 s lockout, 11.7 percent mana) |
| 38 | **Hammer of Wrath** (24275, 6 s, 3.0 percent mana, only on targets at 20 percent health or less or during Avenging Wrath; grants 1 Holy Power through Sword of Light) |
| 42 | Seal of Righteousness (20154, 9 percent weapon damage to all within 8 yd) |
| 44 | Seal of Truth row 42463; Heart of the Crusader (mount speed) |
| 45 | tier 3 talent |
| 46 | **Exorcism** (879, 15 s category cooldown, 4.0 percent mana, grants 1 Holy Power); Turn Evil (10326) |
| 48 | Hand of Protection (1022, 5 min, 7.0 percent mana) |
| 50 | **The Art of War** (87138: auto attacks can reset Exorcism); Plate Specialization (86539, +5 percent Strength in all-plate) |
| 52 | Hand of Freedom (1044, 25 s, 7.0 percent mana) |
| 54 | Emancipate (121783: frees one movement impairing effect) |
| 58 | Sanctity of Battle (25956: haste shortens cooldowns and the global cooldown) |
| 60 | Devotion Aura (31821, 3 min cooldown, 6 s, immunity to silence and interrupt plus less magic damage for the raid); tier 4 talent |
| 64 | Censure (31803) |
| 66 | Hand of Salvation (1038) |
| 70 | Seal of Justice (20164) |
| 72 | **Avenging Wrath** (31884, 3 min, 20 s) |
| 75 | **Guardian of Ancient Kings** (86698, 3 min, 30 s, a guardian that attacks your target); tier 5 talent |
| 80 | Hand of Sacrifice (6940); Mastery: Hand of Light (76672); Absolve (140333) |
| 81 | **Inquisition** (84963, 1 to 3 Holy Power, 20 s per charge); Blessing of Might (19740) |
| 85 | Boundless Conviction (115675: Holy Power cap 5, no spender uses more than 3) |
| 87 | Blinding Light (115750) |
| 90 | tier 6 talent |

Mana: Retribution presses Crusader Strike (15.0 percent) and Judgment (12.0 percent) on cooldown. Sword of Light restores mana as a
periodic aura (effect period 2 s, base value 6; read as 6 percent of maximum mana every 2 s, **not verified live**). By the data a
bot that presses both on cooldown spends about 5 percent of base mana per second against about 3 percent restored, so a long
dungeon fight may end with the mana low; Templar's Verdict, Hammer of Wrath (3.0) and Exorcism (4.0) are cheap. This is a
**to check live** item (gap 14), not a finding.

## How the bots run this specialisation (read)

- A Retribution paladin (and a paladin with no specialisation at levels 1 to 9, the `else` branch) gets the `dps` combat strategy
  (`DpsPaladinStrategy`, `Factory/AiFactory.cpp:408`), plus `dps assist`, `cure`, and the shared `racials`, `chat`, `default`,
  `cast time`, `potions`, `duel`, `boost` (`AiFactory.cpp:316`). Out of combat: `nc` (`GenericPaladinNonCombatStrategy`),
  `dps assist`, `bmight`, `cure` (`AiFactory.cpp:711`; `baoe` is not registered any more and is dropped silently), food, follow,
  loot. The `offheal` Retribution strategy exists (`DPS:116-175`) but the factory never adds it.
- Rank of an action (`strategy/Strategy.h`): ACTION_DEFAULT 5, NORMAL 10, HIGH 20, INTERRUPT 40, EMERGENCY 90; the engine takes the
  highest action that is useful and possible and **tries the next one when a cast fails in the same tick** (`Engine.cpp:207-246`),
  so a Divine Storm refused for lack of Holy Power costs no time. An action whose spell is unknown is skipped. Health thresholds
  (`conf.dist:670-676`): critical 25, low 45, medium 65, almost full 85; mana: low 15, medium 40, high 65.
- Holy Power spenders are gated by triggers, because the cast check ignores the power cost (`PaladinActions.h:30-35`): Templar's
  Verdict from "holy power 3 available" or the Divine Purpose proc, at 5.65 (`DPS:108-109`), above every default generator.
- Default list (`DPS:60-68`): Execution Sentence 5.62, Hammer of Wrath 5.6, Judgment 5.57, Exorcism 5.56, Crusader Strike 5.55, Divine
  Storm 5.3, Holy Wrath 5.15, Consecration 5.1, melee 5. Judgment and Exorcism sit above Crusader Strike on purpose (comment at
  `DPS:44-52`: a bot gets an open global cooldown only every third engine tick and the long cooldown spells starved).
- Edges: seal missing 20 (`DPS:89`), Avenging Wrath `boost` 22 (`DPS:94`), "medium aoe" (3 attackers within 8 yd of the target)
  Divine Storm 24, Consecration 23, Hammer of the Righteous 22 (`DPS:96-101`), Inquisition (missing, 1 or more Holy Power) 21
  (`DPS:106`), Holy Avenger `boost` 22.7 and Guardian `boost` 22.6 (`DPS:75-76`), enemy out of melee: reach melee 21 (`DPS:110`),
  Divine Protection at 50 percent health 26 (`DPS:77`), Flash of Light at 25 percent health (`DPS:78`).
- Shared edges (`GPS`): Divine Shield at critical health 25 (`:21`), Rebuke on the current target 41 and on any casting enemy 40.5
  (`:26-29`), Hammer of Justice as an interrupt, on an enemy healer and on a snare target 40 (`:31-38`), Lay on Hands at critical health 90
  and on a critical member 91 (`:40-43`), Hand of Protection through "protect party member" 92 (`:44-46`; the value was rewritten
  under BT-129), Hand of Freedom on a snared member or self 24 (`:49-50`), Flash of Light after Divine Shield 93 (`:52-54`), Divine
  Plea at "high mana" 20 (`:56`), Word of Glory on a party member below 45 percent with 1 or more Holy Power 32 and Flash of Light
  31 (`:64-66`). The party target includes the bot, so a solo bot heals itself the same way.
- The shared Hand of Freedom action also frees the bot itself when it is slowed or rooted and has no group
  (`PaladinActions.cpp:50-87`); Hand of Protection refuses a tank (`:32-39`); Crusader Strike is skipped only while the target carries
  the bot's Weakened Blows and the bot holds 3 Holy Power (`:86-97`, BT-051), which is the right rule for a Retribution paladin
  because Templar's Verdict (5.65) takes the 3 Holy Power first.
- The seal edge (`PaladinTriggers.cpp:31-38`) counts any of Justice, Command, Righteousness or Truth as "a seal is up"; Insight
  counts only at 70 percent mana or below, and for a tank always. The action is Seal of Truth with Seal of Command as the
  alternative below level 24 (`DPS:28`). Nothing recasts or swaps a seal out of combat or for a pack.
- Blessings: `bmight` (`PaladinBuffStrategies.cpp:20-24`) casts Blessing of Might on a member who lacks it, **and Might is learned
  at 81**. Blessing of Kings (30) is never cast by a Retribution paladin; the holy paladin runs `bkings` plus `bmight2`, the
  protection paladin `bstats` (`AiFactory.cpp:689-709`).
- Skill tiers: no Retribution line is gated by `BotSkill` (nothing in the paladin files uses it).

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked "derived"
comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to `mod-aware-bots-548/src/strategy/
Classes/paladin/` at commit `b5ed934`. `DPS` is `DpsPaladinStrategy.cpp`, `GPS` is `GenericPaladinStrategy.cpp`, `NC` is
`GenericPaladinNonCombatStrategy.cpp`, `PA` is `PaladinActions.cpp`, `PT` is `PaladinTriggers.cpp`, `AF` is
`Factory/AiFactory.cpp`.

### Levels 15 to 29

Known: Crusader Strike, Seal of Command (Seal of Truth from 24), Judgment, Hammer of Justice, Word of Glory, Templar's Verdict,
Redemption, Flash of Light, tier 1 talent (15), Lay on Hands (16), Divine Shield (18), Hammer of the Righteous and Cleanse (20),
Divine Protection (26), Judgments of the Bold (28). The only Holy Power source until 28 is Crusader Strike (Judgment gives none),
so Templar's Verdict comes about every third Crusader Strike (13.5 s). Hammer of the Righteous shares the cooldown, so it
replaces Crusader Strike rather than adding to it.

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | a seal is not up | Seal of Command (105361, 3), then Seal of Truth (31801, 24) in its place (derived: they are the only seals; Judgment needs a seal up, aura state 5 in `SpellAuraRestrictions`) | yes: `DPS:89`, rank 20, node falls back to command (`DPS:28`) |
| 2 | core | 3 Holy Power (or a Divine Purpose proc) | Templar's Verdict (85256, 10): the first spender, never "wait for 5" (cap 3, derived) | yes: 5.65 (`DPS:108-109`) |
| 3 | core | off cooldown | Crusader Strike (35395, 1): the only Holy Power builder below 28; not cast at 3 Holy Power | yes: 5.55 (`DPS:64`), skipped at 3 Holy Power with Weakened Blows on the target (`PA:86-97`), which is right |
| 4 | core | off cooldown, a seal up | Judgment (20271, 5): 12.0 percent mana; before 28 it only does damage, still worth it on a pull (Physical Vulnerability and Holy Power from 28) | yes: 5.57 (`DPS:62`), above Crusader Strike (the guides put it below; kept, see the default-list note) |
| 5 | good | 3 or more enemies within 8 yd of the target | Hammer of the Righteous (53595, 20) instead of Crusader Strike (derived break-even: 20 plus 35 per target against 125) | yes: "medium aoe" 22 (`DPS:100`) |
| 6 | good | the target is at 20 percent health or less | Hammer of Wrath (24275, 38): see 30 to 44 | n/a below 38 |
| 7 | good | health under 45 percent | Word of Glory on self (85673, 9) with 1 or more Holy Power, else Flash of Light (19750, 14) | yes: `GPS:64-66` (32 and 31); a Retribution paladin also spends its damage Holy Power this way (gap 6) |
| 8 | good | health under 25 percent | Lay on Hands (633, 16), then Divine Shield (642, 18) | yes: `GPS:40` at 90, Divine Shield at 25 (`GPS:21`): Lay on Hands always goes first and puts Forbearance on, so Divine Shield only follows when Lay on Hands is on cooldown |
| 9 | good | health under 50 percent | Divine Protection (498, 26) | yes: `DPS:77` rank 26 |
| 10 | good | a curse or poison or disease on a member | Cleanse (4987, 20): poison and disease only for this specialisation (Sacred Cleansing 53551 is a Holy row) | **to check:** the magic edges (`GPS:69-86`) still ask Cleanse for magic effects of a Retribution paladin |
| 11 | expert | derived: the target is out of melee range | Judgment (30 yd) while closing in | **missing:** "reach melee" 21 (`DPS:110`) outranks the default list, so the bot walks first and never casts on the way |

Before the spells are learned (derived): 15 to 19 the buttons are Crusader Strike, Judgment and Templar's Verdict; Hammer of the
Righteous joins at 20 for packs and Seal of Truth at 24.

### Levels 30 to 44

As above, plus Blessing of Kings and the tier 2 talent (30), Seal of Insight (32), Divine Storm (34), Rebuke (36), Hammer of Wrath
(38), Seal of Righteousness (42). Judgment now gives Holy Power (28), so two builders feed one spender.

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 12 | core | a member lacks Kings and no other paladin cast it | Blessing of Kings (20217, 30): the only blessing until 81 | **missing:** the Retribution out-of-combat set is `bmight` (`AF:711`) and Might is level 81, so nothing is cast between 30 and 80 |
| 13 | core | 3 Holy Power | Templar's Verdict (cap 3, spend at once) | yes (`DPS:108`) |
| 14 | good | 3 or more enemies within 8 yd of the target, 3 Holy Power | Divine Storm (53385, 34) instead of Templar's Verdict (derived: 100 percent per target against 275, so 3 targets; guides say 2) | yes: "medium aoe" 24 (`DPS:97`); fails without Holy Power and the engine moves on (`Engine.cpp:234-239`) |
| 15 | core | the target is at 20 percent health or less | Hammer of Wrath (24275, 38): Holy Power builder, above Crusader Strike (both guides) | yes: 5.6 (`DPS:61`) |
| 16 | core | an enemy casts | Rebuke (96231, 36): the interrupt; Hammer of Justice (853, or Fist of Justice) as the second one | yes: `GPS:26-29`; the "on enemy caster" action targets casters up to 30 yd while Rebuke has melee range (to check, harmless if it just fails) |
| 17 | good | 4 or more enemies within 8 yd of the target | Seal of Righteousness (20154, 42) instead of Seal of Truth, back at 2 or fewer (Method and wowtbc: 4 and more) | **missing:** only Seal of Truth exists in the Retribution edges; the Righteousness action is registered (`PaladinActions.h:15`) but no edge casts it |
| 18 | good | out of combat | Hand of Freedom on a snared member (52, see 45 to 59) | yes: `GPS:49-50` |
| 19 | expert | derived: low mana and a long fight | spend on Templar's Verdict and Hammer of Wrath first, press Judgment and Crusader Strike only while mana is above the low threshold | the cast fails by itself at low mana; no plan (gap 14) |

### Levels 45 to 59

As above, plus the tier 3 talent (45), Exorcism and Turn Evil (46), Hand of Protection (48), The Art of War and Plate Specialization
(50), Hand of Freedom (52), Emancipate (54), Sanctity of Battle (58).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 20 | core | off cooldown | Exorcism (879, 46): after Judgment (both guides); an Art of War proc (50) resets it | yes: 5.56 (`DPS:63`); the Art of War edge (`DPS:87`) only repeats the default entry at a lower rank |
| 21 | core | order of the generators | Hammer of Wrath, Crusader Strike, Judgment, Exorcism (both guides) | **wrong order, deliberate:** Judgment 5.57 and Exorcism 5.56 above Crusader Strike 5.55 (`DPS:62-64`); with about 36 uses in 60 s against 40 open global cooldowns the order only decides which button waits, so low value (gap 12) |
| 22 | good | a member is under 30 percent and not a tank | Hand of Protection (1022, 48) | yes: `GPS:44-46` |
| 23 | good | the bot or a member is rooted or slowed | Hand of Freedom (1044, 52); Emancipate (121783, 54) for the bot itself | Hand of Freedom yes (`GPS:49-50`, `PA:50-87`); Emancipate missing, low value |
| 24 | good | health under 70 percent, Selfless Healer stacks, alone (derived) | Flash of Light is free and instant at 3 stacks | automatic when the Selfless Healer talent is picked (the premade takes Eternal Flame) |
| 25 | good | a pack of 3 or more | Mass Exorcism glyph: Exorcism hits the pack | glyph not in the premade |
| 26 | expert | a fear or silence caster in the pack | Turn Evil (10326, 46) on one | missing (`cc` strategy not added to a Retribution paladin) |

### Levels 60 to 74

As above, plus Devotion Aura and the tier 4 talent (60), Censure (64), Hand of Salvation (66), Seal of Justice (70), Avenging Wrath
(72). No new damage button before 72.

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 27 | core | a hard fight or the bot is alone | Avenging Wrath (31884, 72): on cooldown (both guides) | yes: `boost` 22 (`DPS:94`) |
| 28 | good | 3 or more members under 50 percent, or a boss | Devotion Aura (31821, 60) | **missing** (the edge was removed, `PaladinBuffStrategies.cpp:30-37`) |
| 29 | expert | the bot has the threat | Hand of Salvation (1038, 66) on itself | missing |
| 30 | good | Hammer of Wrath during Avenging Wrath on any target | Hammer of Wrath is castable on every target while the buff lasts (spell text) | automatic through the cast check |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 75 | Guardian of Ancient Kings on cooldown with Avenging Wrath; tier 5 talent | yes: `boost` 22.6 (`DPS:76`); Holy Avenger line 22.7 for that pick |
| 80 | Mastery: Hand of Light; Hand of Sacrifice for the tank (glyphed, wowtbc) | passive; no Retribution Hand of Sacrifice line (the glyph in the premade is inert) |
| 81 | Inquisition up and refreshed under 2 s (wowtbc), spend 3 Holy Power so it lasts 60 s; Blessing of Might | line exists: Inquisition when missing and 1 or more Holy Power, 21 (`DPS:106`) wastes nothing (1 Holy Power gives 20 s, the same rate) but adds global cooldowns and leaves a gap each cycle because nothing refreshes within 2 s; Might through `bmight` |
| 85 | Boundless Conviction: the cap is 5; Method and wowtbc: Templar's Verdict at 5, generators first at 3 and 4 | Templar's Verdict at 3 stays above every generator, so the bar never fills; fix is a separate "holy power 5" line (gap 11) |
| 87 | Blinding Light | no line |
| 90 | Method single target: seal, Inquisition, Avenging Wrath, Guardian, Execution Sentence, Templar's Verdict at 5, Hammer of Wrath, Crusader Strike, Judgment, Exorcism, Templar's Verdict at 3 | all there except the cap-5 rule; Execution Sentence in the default list at 5.62 sits below Avenging Wrath and Guardian |

## Cooldowns and defensives

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Avenging Wrath | 72 | on cooldown with Guardian | boss, elite or hard pack | yes, `boost` (alone: every pull) |
| Guardian of Ancient Kings | 75 | on cooldown | same | yes, `boost` |
| Holy Avenger | talent 75 | with Avenging Wrath (Method) | same | line exists; the premade takes Sanctified Wrath |
| Execution Sentence | talent 90 | on cooldown, after the two above | same | yes, default list |
| Divine Shield | 18 | wowtbc defensive 1 | health under 25 percent after Lay on Hands is on cooldown | yes (second) |
| Lay on Hands | 16 | wowtbc defensive 4 | health under 25 percent, or a member critical | yes, first (`GPS:40-43`) |
| Divine Protection | 26 | wowtbc defensive 2 | health under 50 percent | yes, `DPS:77` |
| Devotion Aura | 60 | wowtbc defensive 3 | the group takes magic damage or 3 hurt | missing |
| Hand of Sacrifice | 80 | wowtbc defensive 5 | a member is dying and the tank is not | no line for Retribution |
| Hand of Protection | 48 | wowtbc defensive 6 | a non-tank member under 30 percent | yes, `GPS:44-46` |
| Hand of Freedom | 52 | not named | a member or the bot is slowed or rooted | yes, `GPS:49-50` |
| Rebuke | 36 | not named | every interruptible cast of the pull target | yes, `GPS:26-29` |
| Hammer of Justice / Fist of Justice | 7 / talent 30 | not PvE | the second interrupt, PvP stun | yes, `GPS:31-38` |
| Word of Glory / Eternal Flame | 9 / talent 45 | not named | self heal at under 45 percent | yes, `GPS:64-66` (spends the damage Holy Power) |

Blessings (who casts which in a group, derived from `PT:44-52` and the strategies): Kings (30) is the better one for everybody;
Might (81) adds mastery. The target test is "no Kings aura from anybody" (`PartyMemberWithoutAuraValue.cpp`), so with several
paladins the first one to look casts it and the others see it. The holy paladin already runs "Kings, and Might only on members whose
Kings came from another paladin" (`bkings` plus `bmight2`, BT-132); the Retribution paladin should run the same pair.

## Healer triage and mana (not a healer)

Healing that exists: Word of Glory and Flash of Light on a party member under 45 percent in combat (`GPS:64-66`), the same plus
Redemption out of combat (`NC:30`, `:47-54`). Out of combat a non-Holy paladin casts Flash of Light (37.8 percent mana) on any
member under 85 percent (`NC:47`); for Retribution that is an expensive top-up (gap 15).

## Spell names the Retribution strategies ask for that a Retribution paladin does not know

`SpellIdValue` logs `[SID] ... qualifier='<name>' resolved=0` whenever a name matches no known, non-passive spell
(`strategy/value/SpellIdValue.cpp`) and caches the answer 20 s per name and bot. From the code (inference, not checked against the
log), the names a Retribution paladin of level 15 to 59 asks for and does not know:

| Name | Known from (Retribution) | Asked from |
|------|--------------------------|------------|
| `execution sentence` | talent 90, only that pick | default list, first entry, every tick (`DPS:60`) |
| `hammer of wrath` | 38 | default list, 15 to 37 (`DPS:61`) |
| `exorcism` | 46 | default list, 15 to 45 (`DPS:63`) |
| `divine storm` | 34 | default list (`DPS:65`) and "medium aoe" (`DPS:97`), 15 to 33 |
| `holy wrath` | **never** (Protection row) | default list, every tick, all levels (`DPS:66`) |
| `consecration` | **never** (Protection row, level 34); to check whether a Retribution paladin knows the helper 81297 | default list and "medium aoe" (`DPS:67`, `:98`) |
| `hammer of the righteous` | 20 | "medium aoe" (`DPS:100`), 15 to 19 |
| `divine plea` | **never** (Holy row) | "high mana", whenever mana is under 65 percent (`GPS:56`), all levels |
| `inquisition` | 81 | "inquisition and holy power 1 available" (`DPS:106`), whenever Holy Power is 1 or more, 15 to 80 |
| `avenging wrath`, `guardian of ancient kings`, `holy avenger` | 72, 75, talent 75 (not the premade's pick) | `boost` edges (`DPS:75-76`, `:94`), every pull when alone |
| `divine protection` | 26 | "paladin health 50" (`DPS:77`), 15 to 25 |
| `rebuke` | 36 | when an enemy casts (`GPS:26-29`), 15 to 35 |
| `hand of protection`, `hand of freedom` | 48, 52 | "protect party member", "hand of freedom on party" (`GPS:44-50`) while a member is hurt or slowed |
| `blessing of might` | 81 | `bmight` (`AF:711`): a member without Might, every check, 15 to 80 |
| `divine shield`, `lay on hands` | 18, 16 | critical health (`GPS:21`, `:40`), 15 to 17 |
| `cleanse` | 20 | the `cure` edges (`GPS:69-86`) when somebody carries a debuff, 15 to 19 |

Fix direction: a level and talent test on the default list entries and the `boost`, `inquisition` and blessing edges, drop `holy
wrath`, `consecration` and `divine plea` from the Retribution lists (they are never learned), and the `resolved=0` count per bot
should fall to near zero.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Templar's Verdict arrives at 10, Hammer of the Righteous at 20, Seal of Truth at 24, Judgment gives
  Holy Power at 28, Divine Storm at 34, Rebuke at 36, Hammer of Wrath at 38, Exorcism at 46, Avenging Wrath at 72, Guardian at 75,
  Inquisition and Blessing of Might at 81; below those levels the lists above start without them.
- Holy Power: cap 3 until level 85 (Boundless Conviction), so "never sit at 5" becomes "spend at 3".
- Consecration, Holy Wrath and Divine Plea are not Retribution spells here (the bots' lists name all three).
- Judgment needs a seal up and grants no Holy Power below 28.
- The effect data puts the Divine Storm and Hammer of the Righteous break-even at 3 targets; both guides say 2.
- Mastery: Hand of Light starts at 80 (Method ranks Mastery above Critical Strike for level 90).
- Overgear, Icy Veins and Wowhead were not readable, so two guides were compared.
- Method's talent picks (Pursuit of Justice, Selfless Healer) are passives that need no bot code; the premade's (Speed of Light,
  Eternal Flame) were chosen from wowtbc's lists.

## Gap list (ordered)

Config (can go at once):

1. Talent link `AiPlayerbot.PremadeSpecLink.2.2` (`conf/playerbots.conf.dist:2696`) from `001112` to **`200112`** (Pursuit of Justice at
   15, Selfless Healer at 45). The comment at `:2693` needs the same change. Reason: Speed of Light is an active no line casts;
   Selfless Healer gives free Flash of Light on the lines that already exist and keeps the Holy Power for Templar's Verdict.
   Questions 1 and 2.
2. Glyphs `PremadeSpecGlyph.2.2` (`:2697`) from `41097,43365,41092,43340,43369,104051` to **`41097,43365,83107,43340,43369,41096`**
   (Mass Exorcism for Double Jeopardy, Divine Protection for the inert Hand of Sacrifice). The comment at `:2694` needs the same
   change. Question 3.
3. Stat weights for the Retribution (`Mgr/Item/StatsWeightCalculator.cpp:495-505`): Mastery 0 below level 80, Haste 1.8 from level 58
   (1.4 below), Agility 0.2, Critical 1.6; keep Strength 2.5, Hit 1.9, Expertise 2.0, Attack Power 0.5. Check the hit cap used for
   the overflow.

Strategy (class fixes, wait for the owner's in-game test), ranked by value for levels 15 to 60:

4. **Blessing of Kings for Retribution** (`Factory/AiFactory.cpp:711`): replace `"bmight", "baoe"` by `"bkings", "bmight2"`, as the
   holy paladin does (`:709`). Today a Retribution paladin casts no blessing from 30 to 80 and Might alone from 81, and a group
   with a Retribution paladin and no other loses Kings. One line; the strategies exist.
5. **Unknown-name spam** (the table above): level and talent gates on the default list (`DPS:60-68`: `hammer of wrath` from 38,
   `exorcism` from 46, `divine storm` from 34, `execution sentence` with the talent), delete `holy wrath` and `consecration` from
   the Retribution default list and the "medium aoe" edge (`DPS:66-67`, `:98`), delete `divine plea` for Retribution
   (`GPS:56`, also reached by the protection paladin), known-spell tests on the `boost`, Inquisition and Rebuke edges. Verify with the
   debug log: `resolved=0` lines per bot fall to near zero.
6. **Holy Power spent on heals** (`GPS:63-66`, `NC:53-54`): a Retribution paladin spends 1 to 3 Holy Power (its Templar's Verdict
   damage) on a member under 45 percent even when a healer is in the group. Derived rule: with a healer in the group, spend only
   below the critical threshold (25 percent) or at 3 Holy Power; alone, as today. Needs the group-has-healer test the other
   sheets use.
7. **Seal of Righteousness at 4 or more enemies** (level 42 and up; derived thresholds from both guides): a "high aoe" (4 attackers
   within 8 yd, exists as `HighAoeTrigger`) edge that casts the Righteousness action (`PaladinActions.h:15`), plus a return edge
   to Seal of Truth when the pack is down to 2 and `SealTrigger` (`PT:31-38`) treating Righteousness as "not up" for a Retribution
   paladin outside a pack. Expert tier; moderate work.
8. **Inquisition** (level 81 and up): cast at 3 Holy Power and refresh under 2 s (`BuffTrigger` with `beforeDuration` about 2 s,
   `PaladinTriggers.h:17` and the two-trigger at `DPS:106`), instead of at 1 Holy Power when missing. Not in the 15 to 60 band.
9. **Cast Judgment on the way in** (expert, derived): let Judgment outrank "reach melee" for a target out of melee range but within 30
   yd, `DPS:110` (relevance 21) against the default list (5.57).
10. **Devotion Aura** (60 and up): an edge on `paladin 3 hurt group` (exists, `PT:127-151`) or a boss `boost` that casts Devotion
    Aura, with its 3 min cooldown (`PaladinBuffStrategies.cpp:30-37` records why the keep-up edge was removed).
11. **Holy Power 5 from level 85** (Boundless Conviction): Templar's Verdict at 5 above the generators, at 3 or 4 below them
    (`DPS:108`, `PT:HolyPowerAvailableTrigger`).
12. **Judgment and Exorcism above Crusader Strike** (`DPS:62-64`): the guides put Crusader Strike first; the comment at `DPS:44-52`
    says it was moved for tick starvation. Keep, and measure casts per minute in a live run before touching it.
13. **Hammer of the Righteous and Divine Storm thresholds**: keep 3 enemies; the data says 3 is the break-even, the guides' 2 is a
    level 90 number with gear. Question 5.
14. **Mana plan** (to check live first): log mana across one dungeon pull; if it runs dry, drop Judgment below the medium mana
    threshold and keep Crusader Strike and the free spender.
15. **Out-of-combat top-ups** (`NC:47`): for non-Holy paladins use Flash of Light only below the medium threshold (65 percent); the
    almost full edge (85 percent) costs 37.8 percent mana.
16. **Skill tiers**: no gating. Lines marked good and expert above (Seal of Righteousness, Judgment on approach, Devotion Aura, the
    Holy Power heal rule, Hand of Freedom, Hand of Protection, Emancipate, Turn Evil) are the ones a casual bot should drop.
17. **To check live:** whether the Retribution paladin's Cleanse tries magic effects (`GPS:69-86`); whether `consecration` resolves to
    the helper 81297 for a Retribution bot; a glyphed Exorcism at melee range; Crusader Strike grants Holy Power at levels 10 to 27
    (energize effect says yes); Eternal Flame through the name `word of glory`; the Rebuke "on enemy caster" action against a
    caster at 20 yd; the periodic mana of Sword of Light; the `[SID]` counts per bot; Selfless Healer stacks counted from Judgment
    (core script at `spell_paladin.cpp:438-476`).

Owner questions:

1. Tier 15: Pursuit of Justice (Method's default, a passive with a bonus per Holy Power) or Speed of Light (today, inert).
2. Tier 45: Selfless Healer (Method, a passive; free Flash of Light after three Judgments) or Eternal Flame (today; a Holy Power
   heal over time that competes with Templar's Verdict). Tier 75: Sanctified Wrath (today, a passive) or Holy Avenger (Method's
   first pick, a line exists).
3. Glyphs: Mass Exorcism (Method dungeon set, works for a bot) or Double Jeopardy (both guides, pays nothing unless Judgment
   alternates targets); Divine Protection or Inquisition at level 75 (Inquisition is dead until 81).
4. Holy Power on heals: should a Retribution paladin ever spend it on a member while a healer is in the group, or only at the
   critical threshold?
5. Area threshold: 3 enemies within 8 yd for Divine Storm and Hammer of the Righteous (data and today) or 2 (both guides).
6. Seal of Righteousness for packs of 4 or more: worth an edge and a swap back, or keep Seal of Truth always (simpler, bot-friendly)?

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 1 player of this specialisation (rating 2466 to 2466). 2v2: no player of this specialisation. Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** none, only 1 player in all (noted, not changed)
- **BT-324 result:** open (1 player)
- **Set now:** `PremadeSpecLinkPvp.2.2 = 100202`, `PremadeSpecGlyphPvp.2.2 = 41102,43340,41097,43365,43369,41105`

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Long Arm of the Law 100% / - / 1 of 1 | Long Arm of the Law | Long Arm of the Law | matches |
| 2 (30) | Fist of Justice 100% / - / 1 of 1 | Fist of Justice | Fist of Justice | matches |
| 3 (45) | Selfless Healer 100% / - / 1 of 1 | Selfless Healer | Selfless Healer | matches |
| 4 (60) | Unbreakable Spirit 100% / - / 1 of 1 | Clemency | Clemency | differs, not changed |
| 5 (75) | Sanctified Wrath 100% / - / 1 of 1 | Holy Avenger | Holy Avenger | differs, not changed |
| 6 (90) | Execution Sentence 100% / - / 1 of 1 | Execution Sentence | Execution Sentence | matches |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Templar's Verdict (41097) | 100% | - | 1 of 1 | yes | yes |
| major | Inquisition (45747) | 100% | - | 1 of 1 | no | no |
| major | Hand of Sacrifice (104051) | 100% | - | 1 of 1 | no | no |
| major | Burden of Guilt (41102) | 0% | - | 0 of 1 | yes | yes |
| major | Word of Glory (41105) | 0% | - | 0 of 1 | yes | yes |
| minor | Winged Vengeance (43366) | 100% | - | 1 of 1 | no | no |
| minor | Seal of Blood (43368) | 100% | - | 1 of 1 | no | no |
| minor | Fire From the Heavens (43369) | 100% | - | 1 of 1 | yes | yes |
| minor | the Mounted King (43340) | 0% | - | 0 of 1 | yes | yes |
| minor | Contemplation (43365) | 0% | - | 0 of 1 | yes | yes |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **Open (BT-451):** one player in 3v3, none in 2v2: tiers 15, 30 and 90 match; tier 60 (Unbreakable Spirit) and tier 75 (Sanctified Wrath) differ from the set (Clemency, Holy Avenger); the glyphs Templar's Verdict and Fire From the Heavens match.
