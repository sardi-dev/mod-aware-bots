# Paladin: Protection (tank)

- **Status:** checked against 5.4.8 data and against the bots' code by reading (2026-11-10). Implemented, compile and live
  check pending: config gap items 1 and 2 and rotation gap items 1 to 3 (owner approved 2026-11-10, branch
  `feat/class-sheet-priest-paladin`); the "bots today" columns below describe the state before them. Nothing verified live.
- **Sources:**
  - Method, "Protection Paladin Class Guide, MoP Classic" (Phase 1, level 90 raid and dungeon, 23 July 2025), read
    2026-11-10.
  - wowtbc.gg, "Protection Paladin Rotation & Talent Build, MoP Classic" (no patch line on the page), read 2026-11-10.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent,
    GlyphProperties, SpellCooldowns, SpellPower, SpellRange, SpellMisc, SpellDuration, SpellEquippedItems;
    `data/db2/Item-sparse.db2` for glyph item ids) through the data reader and short read-only scripts.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch), Wowhead (script-built page); two guessed third-party
    URLs were 404. Two guides were read, not three.
- **Levels covered:** 15 to 60 in detail (5-player dungeons, a human leader who marks targets), 61 to 90 briefly.

Which source was richer: **talents** wowtbc (all options per tier; Method gives picks and reasons, so both used); **glyphs**
equal, wowtbc has a dungeon set that Method lacks; **stat order** Method (wowtbc has none); **opener, single target
priority** wowtbc (it names Sacred Shield, Taunt, the off-GCD Holy Power spends and a Grand Crusader note; Method's list
is the same order with fewer lines); **area priority** wowtbc (Method says only "Hammer of the Righteous for Crusader
Strike"); **defensives** wowtbc (an ordered list of eight; Method names three); **interrupts, taunts, the pull, levels 15
to 60, what to do before a spell is learned** neither guide says anything (wowtbc has Taunt only in the opener), so those
lines are derived from the 5.4.8 data and marked "derived".

## Talents

Tiers and columns are from Talent.dbc. "Bots today" is `AiPlayerbot.PremadeSpecLink.2.1 = 002101`
(`conf/playerbots.conf.dist:2679`; one digit per tier, the column counted from 0). For a character below 75 only the
first four digits are in effect.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (002101) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Speed of Light (both) | Pursuit of Justice (Method), Long Arm of the Law (wowtbc lists only) | 85499 | Speed of Light, same |
| 2 (30) | Fist of Justice (Method default) | Repentance, Evil is a Point of View (by encounter) | 105593 | Fist of Justice, same |
| 3 (45) | Eternal Flame (Method default) | Sacred Shield (Method alternative; wowtbc's opener and priority keep it up); Selfless Healer | 114163 | **Sacred Shield (20925), differs** |
| 4 (60) | Unbreakable Spirit (Method: or Clemency) | Hand of Purity (wowtbc lists only) | 114154 | Unbreakable Spirit, same |
| 5 (75) | Holy Avenger (both; Method: or Divine Purpose) | Divine Purpose (pairs with Eternal Flame), Sanctified Wrath | 105809 / 86172 | Holy Avenger, same as the guide |
| 6 (90) | Execution Sentence (Method single target) | Light's Hammer (area, if the group can stay in it), Holy Prism | 114157 / 114158 / 114165 | Light's Hammer, Method's area pick |

Notes on the picks (data, not guide):

- Eternal Flame replaces Word of Glory (spell text): the three Holy Power heal. Sacred Shield is a 30 s absorb on one
  target. The tank strategy has triggers for Eternal Flame (`TankPaladinStrategy.cpp:117-120`, health at or below 70 and
  3 stacks of Bastion of Glory) and none for Sacred Shield (the only user is the Holy strategy, `HealPaladinStrategy.cpp:82`),
  so today the tier 3 pick is never cast by the tank.
- Unbreakable Spirit cuts the cooldown of Divine Shield, Divine Protection and Lay on Hands (spell text), the three
  buttons the bot presses.
- Holy Avenger (2 min cooldown, 18 s): Holy Power generators deal extra damage and give 3 charges of Holy Power. The
  tank strategy has no Holy Avenger line (see the gap list); Divine Purpose (a chance that a Holy Power spender is free
  and acts as three) has its proc trigger in place (`TankPaladinStrategy.cpp:83-84`, `PaladinTriggers.cpp:53`).
- Light's Hammer, Execution Sentence and Holy Prism have no tank line either (the Execution Sentence action exists for
  Retribution and Holy only, `PaladinActions.h:410`); level 90 only.
- Fist of Justice "replaces Hammer of Justice" (spell text). The generic strategy casts a spell named "hammer of
  justice" (`GenericPaladinStrategy.cpp:36-43`); whether that name still resolves after the talent is **to check live**.

Proposed link by the guide, matching the code that exists (dungeon): `001121` (Speed of Light / Fist of Justice / Eternal
Flame / Unbreakable Spirit / Divine Purpose / Light's Hammer). Back to Holy Avenger (`001101`) once the tank strategy casts it.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and
75 (config comment, `conf/playerbots.conf.dist:2626`; the unlock levels are core-side, not in the data files). Config
order is major, minor, major, minor, minor, major (`conf/playerbots.conf.dist:2624`).

| Type | Glyph | Item id | Why (one clause) | Bots today (`45744,43365,41101,43340,43369,41096`) |
|------|-------|--------:|------------------|------------------------|
| Major | Alabaster Shield | 45744 | each block makes the next Shield of the Righteous hit harder (stacking) | yes (slot 1, level 25) |
| Major | Focused Shield | 41101 | Avenger's Shield hits fewer targets for more damage | yes (slot 3, level 50); see the dungeon note below |
| Major | Divine Protection | 41096 | Divine Protection gives 20 percent magic and extra physical reduction instead of magic only | yes (slot 6, level 75) |
| Major, dungeon | Final Wrath | 41104 | Holy Wrath does more to targets under 20 percent health (wowtbc's dungeon set takes it over Focused Shield; Method's main three include it) | no |
| Major, situational | Word of Glory, Hand of Sacrifice | 41105, 104051 | damage after healing; Hand of Sacrifice stops redirecting damage to the paladin (Method alternatives) | no |
| Minor | Contemplation, the Mounted King, Fire From the Heavens | 43365, 43340, 43369 | no tanking effect (Method and wowtbc: minors are weak; Righteous Retreat, Focused Wrath are Method's names) | yes (slots 2, 4, 5), harmless |

Disagreement: Method's main three are Focused Shield, Alabaster Shield, Final Wrath; wowtbc's standard raid set is
Alabaster Shield, Focused Shield, Divine Protection, and its dungeon set is Alabaster Shield, Final Wrath, Divine
Protection. For a dungeon tank the wowtbc dungeon set is used: spell 54930's text shows that Avenger's Shield does not jump
to further enemies with Focused Shield, which is its pack threat tool (derived: threat on two more mobs of a pack of three
or more). The same property helps the pull (see the pull section), so this is an owner decision after a live test.

## Stat order

Method: Hit to 7.5 percent, Expertise to about 15 percent, Strength, Haste, Mastery, Critical Strike (wowtbc gives none).
Those caps are level 90 numbers and cannot be reached with the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason): a **shield is mandatory** (Avenger's Shield and
Shield of the Righteous require an equipped shield in SpellEquippedItems; Crusader Strike and Hammer of the Righteous need
a melee weapon), so a shield beats any other upgrade when the bot has none; then plate (Plate Specialization, level 50,
gives Stamina only in all-plate), Strength, Stamina (not in the guide: the survival stat while the 90 caps are out of
reach), Haste (more Holy Power generators), Critical Strike; **Mastery is worth nothing below level 80** (Mastery: Divine
Bulwark is learned at 80, so earlier Mastery rating has no effect); Hit and Expertise only near level 90.

## What the specialisation knows, by level (5.4.8 data)

Ids are the spells the player actually casts (the paladin class skill line 800 and the specialisation's own spells); the
data holds several old rows of the same name. Talent spells are listed by tier.

| Level | Spell (id) |
|------:|------------|
| 1 | Crusader Strike (35395, weapon strike, 4.5 s, shows Weakened Blows; grants Holy Power once Word of Glory is known), Seal of Righteousness row 101423 (the casting spell 20154 is level 42), Block, Parry |
| 3 | Seal of Command (105361) |
| 5 | Judgment (20271, 30 yd, 6 s) |
| 6 | Consecration damage helper 81297 (the castable Consecration, 26573, is a Protection spell at 34) |
| 7 | Hammer of Justice (853, 60 s) |
| 9 | Word of Glory (85673, 1 to 3 Holy Power) |
| 10 | Avenger's Shield (31935, 30 yd, 15 s), Guarded by the Light (53592, passive), Vengeance (84839, passive); the specialisation is chosen here |
| 12 | Righteous Fury (25780) |
| 13 | Redemption (7328) |
| 14 | Flash of Light (19750) |
| 15 | Reckoning (62124, taunt, 30 yd, 8 s), tier 1 talent |
| 16 | Lay on Hands (633, 10 min) |
| 18 | Divine Shield (642, 5 min, 8 s) |
| 20 | Hammer of the Righteous (53595, melee plus 8 yd wave, 4.5 s, Holy Power), Holy Wrath (119072, 9 s, also stuns demons and undead), Cleanse, Warhorse |
| 24 | Seal of Truth (31801, replaces Seal of Command) |
| 26 | Divine Protection (498, 60 s, 10 s) |
| 28 | Judgments of the Wise (105424, passive: Judgment grants Holy Power) |
| 30 | Blessing of Kings (20217), tier 2 talent |
| 32 | Seal of Insight (20165) |
| 34 | Consecration (26573, 9 s) |
| 36 | Rebuke (96231, melee range, 15 s) |
| 38 | Hammer of Wrath (24275, 30 yd, 6 s, target at 20 percent health or less, or during Avenging Wrath) |
| 40 | Shield of the Righteous (53600, 3 Holy Power, 1.5 s, off the global cooldown), Bastion of Glory (114637, passive) |
| 42 | Seal of Righteousness (20154) |
| 45 | tier 3 talent |
| 46 | Turn Evil (10326) |
| 48 | Hand of Protection (1022, 5 min) |
| 50 | Grand Crusader (85043, passive), Plate Specialization (86102, passive) |
| 52 | Hand of Freedom (1044) |
| 60 | Devotion Aura (31821, 3 min cooldown, 6 s), tier 4 talent |
| 64 | Sanctuary (105805, passive) |
| 66 | Hand of Salvation (1038, 2 min) |
| 70 | Ardent Defender (31850, 3 min, 10 s) |
| 72 | Avenging Wrath (31884, 3 min, 20 s) |
| 75 | Guardian of Ancient Kings (86659, 3 min, 12 s), tier 5 talent |
| 80 | Hand of Sacrifice (6940, 2 min), Mastery: Divine Bulwark (76671) |
| 81 | Blessing of Might (19740) |
| 87 | Blinding Light (115750) |
| 90 | tier 6 talent |

Facts the priority lists rest on (spell texts and tables of the data): Weakened Blows (30 s) is applied by Crusader Strike
and Hammer of the Righteous; Judgment gives Holy Power only with Judgments of the Wise (28); Hammer of Wrath gives Holy
Power only with the Retribution passive, so not for Protection; Avenger's Shield also silences and interrupts the target;
Shield of the Righteous reduces physical damage taken (30 to 80 percent, short duration) and builds Bastion of Glory, which
raises a self cast of Word of Glory or Eternal Flame; Guarded by the Light makes Word of Glory free of the global
cooldown and sets spell power to attack power; Grand Crusader (50): a dodge or parry can reset Avenger's Shield and make
its next cast give Holy Power; Reckoning also raises threat against that target for a while; there is **no area taunt**
in the paladin skill line for 5.4.8 (Righteous Defense is not in the class line; the code comment says it was removed in
5.0).

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked
"derived" comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to
`mod-aware-bots-548/src/` (module repository). Relevance numbers: default actions sit at 5.0 plus a fraction, high 20,
interrupt 40, emergency 90 (`strategy/Strategy.h:37-51`).

### The pull (all bands; a human leader marks, the tank pulls marks only, the group waits)

Setup verified by reading: the paladin tank carries `tank`, `tank assist`, `pull`, `pull back`, `bthreat`, `barmor`,
`cure` in combat (`Factory/AiFactory.cpp:404`) and out of combat `bthreat`, `tank assist`, `pull`, `barmor`, plus a blessing
strategy (`AiFactory.cpp:688-701`); the tank also gets `mark pull` (`AiFactory.cpp:886`). The trigger fires when a mark
exists, is alive, not in combat, within 40 yd, and every member is alive, out of combat, above 60 percent health and (mana
users) 50 percent mana (`strategy/triggers/MarkTriggers.cpp:49-84`, `:32-45`); then `pull rti target`.

**Does the paladin have the warrior's problem (a tank without its ranged pull spell refusing or stalling every pull)?
No, with a narrow exception.** Verified by reading: the paladin is exempt from the "shoot needs a ranged weapon" refusal
(`strategy/generic/PullStrategy.cpp:150-152`), and its pull action is never "shoot": Avenger's Shield if it can be cast,
else Reckoning, else the base action "judgement" (`PaladinPullStrategy.cpp:14-29`, base action set at
`PaladinPullStrategy.h:16`), which is the real spell Judgment (level 5, 30 yd). Judgment is returned without a "can cast"
check, so the pull stalls only when Avenger's Shield (15 s) and Reckoning (8 s, from level 15) are on cooldown and Judgment
(6 s) is too; the pull then waits until the 15 s limit (`PullTriggers.cpp:42`, `PullStrategy.h:41`). The window is real
only at levels 10 to 14 (no Reckoning) after a short fight; **to check live**.

| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| P1 | core | levels 10 to 14, mark within 30 yd (derived: Avenger's Shield is the only ranged tool before Reckoning) | Avenger's Shield (31935, 10) | yes (`PaladinPullStrategy.cpp:21-22`) |
| P2 | core | level 15 and up, Avenger's Shield not ready or the mark has neighbours (derived) | Reckoning (62124, 15), single target, 30 yd, 8 s | yes as the second choice (`PaladinPullStrategy.cpp:25-26`); never chosen for neighbours (see P4) |
| P3 | good | both on cooldown | Judgment (20271, 5) | yes, the base action (`PaladinPullStrategy.h:16`); not checked for cooldown (stall above) |
| P4 | good | the marked mob stands within about 10 yd of unmarked mobs (derived: Avenger's Shield jumps to up to two more enemies unless Focused Shield is taken, and the leader's rule is marks only) | prefer Reckoning (single target) over Avenger's Shield | **gap: Avenger's Shield is always tried first (`PaladinPullStrategy.cpp:21`); the jump radius and whether it pulls a neighbour are to check live** |
| P5 | good | before the pull | Righteous Fury up (from 12), a seal up | Righteous Fury yes (`PaladinBuffStrategies.cpp:37-41`, non-combat); **no seal: the pull's pre-action is empty for a tank (`PaladinPullStrategy.cpp:31-37`) and the seal trigger exists only in the combat strategy (`TankPaladinStrategy.cpp:71-72`)**, so the tank starts the fight without one; low impact |
| P6 | good | the pull target is hit; mobs come to the tank | stay at the pull position | yes: `pull back` (`PullStrategy.cpp:262-268`) |

Range detail (verified): the pull range is read from the spell named by the action; for "hand of reckoning" and
"judgement" no 5.4.8 spell has that name, so the generic spell distance 28.5 yd applies (`PullStrategy.cpp:121-133`,
`conf/playerbots.conf.dist:636`) instead of 30 yd: harmless.

### Levels 15 to 29

Holy Power builders: Crusader Strike (from 9), Hammer of the Righteous (20); Judgment only from 28. The only spender is
Word of Glory (9) until Shield of the Righteous at 40.

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | before the pull and when it drops | Righteous Fury (25780, 12) | yes (`TankPaladinStrategy.cpp:109-110`, relevance 28; `PaladinBuffStrategies.cpp:37-41` out of combat) |
| 2 | core | no seal on the paladin, in a fight | Seal of Command (3) then Seal of Truth (24) (derived: they are the only seals; the Truth text says it "replaces Seal of Command") | yes (`TankPaladinStrategy.cpp:71-72`, chain `:21`, `:35`, `GenericPaladinStrategyActionNodeFactory.h` seal of command to righteousness) |
| 3 | core | an enemy attacks someone else (healer, DPS) | Reckoning (62124, 15), 30 yd | yes: `lose aggro` on the current target (`TankPaladinStrategy.cpp:95-96`, relevance 27; `GenericTriggers.cpp:155-161`, never on a player); the tank target picks a mob without aggro first (`TankTargetValue.cpp:74-111`); the action is a ranged cast, so no walk into melee (`PaladinActions.h:237-245`) |
| 4 | core | Crusader Strike ready | Crusader Strike (35395, 1) | **wrong before level 40: skipped while the bot's own Weakened Blows is on the target and Holy Power is 3 or more (`PaladinActions.cpp:60-66`; the debuff lasts 30 s). Below 40 nothing spends the Holy Power except Word of Glory at 45 percent party health (`GenericPaladinStrategy.cpp:22-25`), so after the first cast the builder fires about once per 30 s per target** |
| 5 | core | Avenger's Shield ready (15 s) | Avenger's Shield (31935, 10) | yes (`TankPaladinStrategy.cpp:54`, relevance 5.45; at two or more enemies within 8 yd `:85-86`, relevance 25) |
| 6 | core | after the two above | Judgment (20271, 5; Holy Power from 28) | yes (`TankPaladinStrategy.cpp:58`, 5.35, below Hammer of Wrath) |
| 7 | core | Holy Power at least 1 and a party member (the tank itself too) at or below 45 percent | Word of Glory (85673, 9) | yes (`GenericPaladinStrategy.cpp:22-23`, relevance 32; whether the tank counts itself as the party member is **to check live**) |
| 8 | good | three or more enemies within 8 yd, from 20 | Hammer of the Righteous (53595, 20) instead of Crusader Strike; Holy Wrath (119072, 20) as a filler | yes (`TankPaladinStrategy.cpp:87-92`, relevance 25; Holy Wrath `:60`, 5.1) |
| 9 | core | health at or below 25 percent (derived timing: see Cooldowns) | Lay on Hands (633, 16), Divine Shield (642, 18) | yes (`GenericPaladinStrategy.cpp:26-27`, `:44-45`, relevance 90 and 25) |
| 10 | core | from 26, health at or below half | Divine Protection (498, 26) | yes (`TankPaladinStrategy.cpp:112-114`, relevance 26) |
| 11 | good | a caster in range, from 36 | Rebuke (96231, 36) | yes, current target and any casting enemy (`GenericPaladinStrategy.cpp:31-34`, relevance 41 and 40.5; melee range only, `PaladinActions.h:390-399`) |

What a tank does where a key spell is missing (derived): levels 10 to 14 have no Righteous Fury before 12 and no taunt
before 15, so a loose mob stays loose; no Lay on Hands before 16; no Divine Shield before 18; no Divine Protection before
26; the only Holy Power spend is the heal Word of Glory. The bots tolerate these because an unknown spell is simply not cast
(inference from the way the call sites test spells; **to check live** at levels 10 to 14).

### Levels 30 to 44

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 12 | good | no blessing on the party | Blessing of Kings (20217, 30) | yes (`PaladinBuffStrategies.cpp:43-50`, relevance 11, strategy chosen at `AiFactory.cpp:692-694` from level 20; below 30 the spell is unknown, so nothing) |
| 13 | core | seal, from 32 | Seal of Insight (20165, 32): both guides keep it up (its melee hits heal) | **gap: the tank always uses Seal of Truth (`TankPaladinStrategy.cpp:71-72`); the seal trigger would also recast a bot on Insight whenever mana is at or below 70 percent (`PaladinTriggers.cpp:14-20`)** |
| 14 | core | stay in Consecration, from 34 | Consecration (26573, 34), 9 s | partly: cast whenever mana is above 65 percent regardless of enemy count (`TankPaladinStrategy.cpp:123-124`, relevance 24, above all generators) and with three or more enemies (`:87-88`); melee action, needs the target in reach (`PaladinActions.h:57`) |
| 15 | good | interrupt, from 36 | Rebuke (96231, 36) | yes (row 11) |
| 16 | good | target at 20 percent health or less, from 38 | Hammer of Wrath (24275, 38) | yes but wrong place: at 20 percent target health it outranks Righteous Fury and every defensive (`TankPaladinStrategy.cpp:106-108`, relevance 30) and in the default list it sits above Judgment (`:55`, 5.4); both guides put it near the end, and for Protection it gives no Holy Power |
| 17 | core | Holy Power at 3, from 40 | Shield of the Righteous (53600, 40), off the global cooldown | yes (`TankPaladinStrategy.cpp:81-84`, relevance 5.6, below every high-relevance line, above Crusader Strike 5.5) |
| 18 | good | seal for a pack, from 42 | Seal of Righteousness (20154, 42) hits every enemy within 8 yd (derived: area threat; neither guide says it) | missing (the chain `TankPaladinStrategy.cpp:21`, `:35` reaches it only when Truth and Command are unknown) |
| 19 | good | tier 2 talent | Fist of Justice (105593) on a casting pack | partly: the generic Hammer of Justice lines (`GenericPaladinStrategy.cpp:36-43`) may not resolve after the talent, **to check live** |

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 20 | good | health at or below 70 percent, or 3 stacks of Bastion of Glory, 3 Holy Power | Eternal Flame (114163, talent tier 3) instead of Shield of the Righteous | code yes (`TankPaladinStrategy.cpp:117-120`, relevance 24.5 and 24.4, `PaladinActions.cpp:83-89`), **but the premade takes Sacred Shield, so it never fires** |
| 21 | good | a party member is about to die to physical damage, from 48 | Hand of Protection (1022, 48) | yes (`GenericPaladinStrategy.cpp:49-51`, relevance 92) |
| 22 | good | a party member is snared, from 52 | Hand of Freedom (1044, 52) | yes (`GenericPaladinStrategy.cpp:53-55`) |
| 23 | good | Grand Crusader proc, from 50 | Avenger's Shield at once (85043, 50) | partly: Avenger's Shield is cast whenever off cooldown, ahead of nothing but Crusader Strike (`TankPaladinStrategy.cpp:53-54`); no line reads the proc, which is acceptable because the reset makes it ready anyway |
| 24 | good | Turn Evil on an undead or demon not being tanked, from 46 | Turn Evil (10326, 46) | the generic crowd control strategy only (`GenericPaladinStrategy.cpp:90-94`), not in the tank engine (to check) |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 25 | good | tier 4 talent at 60 | Unbreakable Spirit (114154): shorter Lay on Hands, Divine Shield and Divine Protection | taken by the premade; nothing to cast |
| 26 | good | a caster pack at the start, from 60 | Devotion Aura (31821, 60), 3 min cooldown: immunity to silence and interrupt, less magic damage for the party | **missing: the edge was removed as a keep-up aura (`PaladinBuffStrategies.cpp:28-35`) and nothing casts it as a cooldown** |
| 27 | good | health at or below 20 percent, from 70 | Ardent Defender (31850, 70) | yes but late and behind Lay on Hands (`TankPaladinStrategy.cpp:115-116`, relevance 89, below Lay on Hands 90 at 25 percent) |
| 28 | core | a hard pull, from 72 | Avenging Wrath (31884, 72), 20 s, damage and healing up | yes on the group strength balance (`TankPaladinStrategy.cpp:104-105`) and wrongly again at 50 percent health (`:112-114`): the 3 min cooldown is spent as a defensive that it is not for Protection |
| 29 | core | on cooldown with Avenging Wrath | Holy Avenger (105809, talent tier 5) | **missing in the tank strategy (the Retribution strategy has it, `DpsPaladinStrategy.cpp:75`)** |

### Levels 75 to 89

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 30 | core | a hard pull or health low, from 75 | Guardian of Ancient Kings (86659, 75), 12 s | yes but only on the group strength balance (`TankPaladinStrategy.cpp:121-122`, `PaladinTriggers.h:309-313`); no health rule |
| 31 | good | a party member (healer) pressed, from 80 | Hand of Sacrifice (6940, 80) | missing (no tank line) |
| 32 | good | the party lacks the mastery blessing, from 81 | Blessing of Might (19740, 81) | missing: the tank carries Kings only (`PaladinBuffStrategies.cpp:43-50`); also the "bmight" strategy it gets below level 20 (`AiFactory.cpp:701`) names a spell that is level 81, so it is idle at those levels |
| 33 | expert | Divine Purpose proc (tier 5 talent), or 5 Holy Power | spend at once | proc trigger yes (`TankPaladinStrategy.cpp:83-84`) |

### Level 90

Guide (Method, wowtbc): Righteous Fury, Seal of Insight, Avenging Wrath and Holy Avenger on cooldown, Shield of the
Righteous or Eternal Flame at 3 Holy Power, Execution Sentence or Light's Hammer on cooldown, Crusader Strike, Judgment,
Avenger's Shield, Consecration, Hammer of Wrath (target under 20 percent), Holy Wrath; Hammer of the Righteous and
Consecration first at four or more targets. The bots have none of the tier 5 and tier 6 abilities in the tank strategy.

## Cooldowns and defensives

The guides give an order or "big spikes"; the thresholds are derived because dungeon trash needs numbers. Thresholds are
the bot's own health percent.

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Righteous Fury | 12 | always | always, before the pull | yes |
| Shield of the Righteous | 40 | main spender | at 3 Holy Power, in melee | yes (relevance 5.6) |
| Eternal Flame / Word of Glory | talent / 9 | magic damage, allies in danger | below 70 percent with 3 Holy Power; Word of Glory on anyone under 45 percent | yes, if the talent is taken |
| Divine Protection | 26 | big spikes | below 65 percent, or on a pull of three or more elites at once | yes at 50 percent |
| Ardent Defender | 70 | big spikes | below 40 percent (it also saves a killing blow) | late: 20 percent, behind Lay on Hands |
| Guardian of Ancient Kings | 75 | big spikes | below 50 percent with three or more enemies on the tank | only on the strength balance |
| Lay on Hands | 16 | emergency (wowtbc order: fifth) | below 20 percent after Ardent Defender or Divine Protection are used, or on a party member under 20 percent | fires at 25 percent, before Ardent Defender |
| Divine Shield | 18 | last in wowtbc's order | when Lay on Hands is unavailable and health is under 25 percent | yes at critical health (25), after Lay on Hands |
| Hand of Protection | 48 | wowtbc order: seventh | on a hurt DPS or healer taking physical damage | yes |
| Hand of Sacrifice | 80 | wowtbc order: sixth | on the healer or the most hurt ally | missing |
| Devotion Aura | 60 | wowtbc order: fourth | at the start of a caster pack | missing |
| Avenging Wrath / Holy Avenger | 72 / talent | on cooldown | at the start of a hard pull | Avenging Wrath yes (and wrongly at 50 percent health); Holy Avenger missing |

## Healer triage and mana (healers only)

Not applicable to a tank. The resource is Holy Power (cap 5 by Method, not checked in the data): Crusader Strike, Hammer of
the Righteous, Judgment (28 and up) build it, Shield of the Righteous (40) and Word of Glory or Eternal Flame spend it.
Guarded by the Light returns a share of maximum mana at intervals (spell text), so a Protection paladin is not expected
to drink between pulls. Divine Plea is a Holy spell, not Protection's.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Seal of Insight (32), Consecration (34), Rebuke (36), Shield of the Righteous (40), Eternal
  Flame (talent 45), Ardent Defender (70), Avenging Wrath (72), Guardian of Ancient Kings (75), Hand of Sacrifice (80) and
  the tier 5 and 6 talents are not available in the 15 to 60 band; the lists below those levels leave them out.
- "Judgment always grants Holy Power" (Method) is level 90; here it needs Judgments of the Wise (28).
- Hammer of Wrath is learned at 38 in this data and gives Holy Power only for Retribution; the guides list it as a normal
  generator step.
- Consecration is a Protection spell learned at 34 (the level 6 row in the class line is its damage helper).
- Holy Power use before level 40 has no guide line: Shield of the Righteous does not exist yet (derived lines 4 and 7).
- Both guides recommend Seal of Insight; Seal of Command (3) and Seal of Truth (24) are the only seals before 32.
- wowtbc's opener step "Taunt" is Reckoning here (learned at 15); Righteous Defense (the area taunt of older versions) does
  not exist in 5.4.8 (no row in the paladin class skill line).
- Method says Holy Power stacks to 5; the data files were not read for the cap.
- Method's Blessing of Might is level 81 (19740); below that the paladin has only Blessing of Kings (30).
- The module config comment cites wowtbc for the premade, whose page marks no pick in its text; the choice of Sacred
  Shield matches the opener and priority lines, not Method's default (Eternal Flame).

## Gap list (ordered by value for levels 15 to 60)

Config (can go at once):

1. Tier 3 talent: Eternal Flame instead of Sacred Shield: the tank has Eternal Flame lines and no Sacred Shield line
   (`TankPaladinStrategy.cpp:117-120`). Proposed link `001121` (also Divine Purpose at 75, see 2). **Done:**
   `PremadeSpecLink.2.1 = 001121` (tier 3 column 1 and tier 5 column 2 verified with the data reader).
2. Tier 5 talent: Divine Purpose instead of Holy Avenger until the tank strategy casts Holy Avenger (its proc trigger
   exists, `TankPaladinStrategy.cpp:83-84`); back to `001101` after rotation gap 4. **Done** with item 1.
3. Third major glyph (slot 3, level 50): Final Wrath (41104) instead of Focused Shield (41101) for dungeons (wowtbc's
   dungeon set; Avenger's Shield keeps its jump to extra enemies). Proposed `45744,43365,41104,43340,43369,41096`. Owner
   decision after a live test of the pull.

Rotation (class fixes, wait for the owner's in-game test):

1. **Crusader Strike gate below level 40** (`PaladinActions.cpp:60-66`): the Weakened Blows plus 3 Holy Power skip assumes a
   spender exists; Protection has none before 40, so Crusader Strike (the main builder and threat attack) fires about once
   per 30 s per target. Remove the gate for the tank, or keep it only from level 40. **Implemented:** the gate is removed for
   a tank (`CastCrusaderStrikeAction::isUseful`, `PaladinActions.cpp`); Retribution keeps it (BT-051).
2. **Seal choice**: Seal of Insight from 32 for the tank (both guides), Truth below 32, Righteousness for packs from 42
   (derived); the seal trigger's mana rule for Insight (`PaladinTriggers.cpp:19`) must not recast it for a tank. Also a seal
   before the pull (`PaladinPullStrategy.cpp:31-37` and no non-combat seal trigger). **Implemented:** Seal of Insight, then
   Seal of Truth, then Seal of Command (the engine falls down the list while a seal is not learned); `SealTrigger` counts
   Insight as up for a tank at any mana. Not done: Righteousness for packs from 42, a seal before the pull.
3. **Defensive order and thresholds**: Ardent Defender before Lay on Hands (`TankPaladinStrategy.cpp:115-116` against
   `GenericPaladinStrategy.cpp:44-45`), Divine Protection earlier than 50, Guardian of Ancient Kings by health as well as by
   the balance value, Avenging Wrath removed from the 50 percent line (`TankPaladinStrategy.cpp:114`). **Implemented:**
   Divine Protection at 65 percent, Ardent Defender at 40, Lay on Hands on itself at 20 only when Ardent Defender is not
   learned or on cooldown (both Lay on Hands actions; other party members keep the old rule), Avenging Wrath off the
   defensive line (its offensive edge, `TankPaladinStrategy.cpp` "avenging wrath", stays). Constants
   `TANK_*_HEALTH_PCT` in `PaladinActions.h`. Not done: Guardian of Ancient Kings by health.
4. **Holy Avenger** (talent 75) and **Avenging Wrath** on cooldown at the start of a pull; **Light's Hammer, Execution
   Sentence, Holy Prism** (level 90) have no tank line.
5. **Hammer of Wrath ordering**: not at relevance 30 on target health under 20 percent (`TankPaladinStrategy.cpp:106-108`)
   and below Judgment in the default list (`:55`).
6. **Pull**: prefer Reckoning over Avenger's Shield when the mark has neighbours (`PaladinPullStrategy.cpp:21`), check the
   cooldown of the Judgment fallback (levels 10 to 14), and the unresolved spell names in the range lookup.
7. **Devotion Aura** as a cooldown at the start of a caster pack (level 60), **Hand of Sacrifice** (80) on the healer.
8. **Blessing of Might** at 81 and the idle "bmight" strategy below level 20 (`AiFactory.cpp:701`).
9. Consecration cast at any enemy count whenever mana is above 65 percent, above every generator
   (`TankPaladinStrategy.cpp:123-124`): fine in the guide's terms ("stay in Consecration"), to check against mana use live.
10. Taunting only: Reckoning has no area counterpart in 5.4.8; nothing to add.
