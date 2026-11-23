# Paladin: Holy (healer)

- **Status:** draft, checked against 5.4.8 data and against the bots' code by reading (2026-11-10). Config gap items 1
  and 2 and rotation gap items 1 to 5 and 8 are implemented on branch `feat/class-sheet-hpal-brewmaster` (BT-132; compile
  pending, the owner's in-game dungeon test pending); rotation items 6, 7 and 9 to 12 are open. File and line numbers in
  the tables are those of the code before that change. Every "bots today" claim was checked by reading the call sites unless
  it says "to check". A repair of the Hand of Protection value (BT-129, row 21) was in progress and uncommitted when the
  sheet was written, and is described where it matters.
- **Sources:**
  - Method, "Holy Paladin Class Guide, MoP Classic" (Devorta, 23 July 2025, Phase 1, level 90 raid and dungeon), read
    2026-11-10.
  - wowtbc.gg, "Holy Paladin PvE Guide, MoP Classic" (no patch line and no date on the page; the page the module's
    premade comment cites), read 2026-11-10.
  - The server's own data files (Spell.dbc with its tooltip text, SpellLevels.dbc, SpecializationSpells.dbc,
    SpellEffect.dbc, SpellPower.dbc, SpellCooldowns.dbc, SpellMisc.dbc, SpellCastTimes.dbc, SpellDuration.dbc,
    SpellRadius.dbc, SpellRange.dbc, Talent.dbc, GlyphProperties.dbc, Item-sparse.db2 of build 18414) through the data
    reader and short read-only scripts, for every spell id, learn level, cost, cooldown, duration, talent tier and glyph.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a plain fetch), Wowhead (script-built page), one guessed third-party
    URL (404). Two guides were read, not three. Neither has a levelling section, a mana section, triage thresholds, a dispel
    section, resurrection, buff advice beyond one line, or a range habit beyond "melee healer": those lines are derived
    (marked "derived", with the reason).
- **Levels covered:** 15 to 60 in detail (5-player dungeons, a human leader, a main tank), 61 to 90 briefly.

Which source was richer: **talents** wowtbc (all three options per tier; Method names a default for rows 1 and 3 to 6 and a
reason, so both used); **glyphs** Method (main set of three with named alternatives, a separate dungeon line; wowtbc lists
three and a situational one); **stat order** Method (wowtbc has none); **priority list** Method (one numbered list of eleven
lines with Holy Power spending split into single target and area; wowtbc has two shorter lists, one per talent build, and
names the Beacon glyph effect); **cooldowns and defensives** wowtbc (an ordered list of twelve; Method names four and no
order); **dispels, resurrection, mana, triage, melee habits, levels 15 to 60, what to do before a spell is learned** neither
guide, so those lines are derived from the 5.4.8 data and marked.

## Which strategy a Holy paladin runs

Read from the factory, not assumed:

- In a group (every dungeon) a Holy paladin's combat set is `heal`, `dps assist`, `cure` and `bcast`
  (`Factory/AiFactory.cpp:405-406`). `heal` is `HealPaladinStrategy` (`HealPaladinStrategy.cpp`, type heal and ranged,
  `HealPaladinStrategy.h:21`), a subclass of `GenericPaladinStrategy` (`GenericPaladinStrategy.cpp:16-62`, which adds
  Lay on Hands, Divine Shield, Hand of Freedom, Hand of Protection, Rebuke, Hammer of Justice and Divine Plea lines).
  **`bcast` is not a registered strategy name any more** (the empty strategy was removed in BT-080; the paladin buff
  factory has only `bhealth`, `bdps`, `bstats`, `bmight`, `bkings`, `PaladinAiObjectContext.cpp:69-78`), and
  `Engine::addStrategy` ignores an unknown name silently (`strategy/Engine.cpp:367-381`), so it is a no-op.
- Out of combat: `dps assist`, `bmight`, `bcast` (no-op), `cure` (`AiFactory.cpp:704-705`, `:709`) and the generic paladin
  `nc` set (`GenericPaladinNonCombatStrategy.cpp:23-47`: Redemption and Flash of Light top-ups).
- Both sets also get `save mana` (`AiFactory.cpp:577`) and, on maps outside the restricted list, `healer dps` (`:581`). The
  default restricted list (`AiFactory.cpp:70-78`, config `AiPlayerbot.RestrictedHealerDPSMaps`) holds the dungeon and raid
  map ids, so a healer in a dungeon casts none of the healer damage lines. The default action `Judgment`
  (`HealPaladinStrategy.cpp:19`) is separate from that strategy and is not blocked (see "Nothing to heal").
- A Holy paladin with **no group** additionally gets `dps`, `dps assist` and `baoe` (`AiFactory.cpp:625-627`): the
  Retribution rotation on top of the heal set, for solo questing. Not covered here; whether it drops when the bot joins a
  group is to check live.
- How an action is chosen (read, `strategy/Engine.cpp:150-260`): every active trigger pushes its actions with their numbers;
  the engine runs the highest-numbered action that is useful and possible, once per tick. Relevance scale
  (`strategy/Strategy.h:37-51`): default 5, normal 10, high 20, interrupt 40, dispel 50, emergency 90; light heals 10 to 19,
  medium 20 to 29, critical 30 to 39.
- The heal target is the group member with the lowest health percent plus a distance penalty
  (`value/PartyMemberToHealValue.cpp:33-99`); the tank is not preferred and nothing weighs a member by role. The health
  bands are **cumulative**, not exclusive (every "party member ... health" trigger has minimum 0,
  `triggers/HealthTriggers.h:82-122`): critical below 25, low below 45, medium below 65, almost full below 85
  (conf `AiPlayerbot.CriticalHealth` and the three after it, `conf/playerbots.conf.dist:667-670`). So a member at 20% makes
  all four triggers fire and the highest number among all their actions wins.

## Talents

Tiers and columns are from Talent.dbc (the guides show builds as images or option lists). "Bots today" is
`AiPlayerbot.PremadeSpecLink.2.0 = 001101` (`conf/playerbots.conf.dist:2674`; one digit per tier, the column counted from
0). For a character below 75 only the first four digits are in effect.

| Tier (level) | Pick | Alternative and when | Spell id | Bots today (001101) |
|--------------|------|----------------------|----------|---------------------|
| 1 (15) | Speed of Light (Method default, wowtbc lists first); **derived: Pursuit of Justice** | Long Arm of the Law. Speed of Light is an active spell (3.5% mana) that no Holy edge casts; Pursuit of Justice is a passive movement bonus for a healer that must keep up with the group | 85499 / 26023 | Speed of Light, same as the guide, but a dead talent in the bots' hands |
| 2 (30) | Fist of Justice (Method: more versatile) | Repentance, Evil is a Point of View (Method: by encounter). Fist of Justice "replaces Hammer of Justice" (spell text); whether the generic "hammer of justice" lines (`GenericPaladinStrategy.cpp:35-44`) still resolve is **to check live** | 105593 | Fist of Justice, same |
| 3 (45) | Eternal Flame (Method default; wowtbc lists it; Holy edges exist) | Selfless Healer (Method: when mana is the problem; wowtbc's area build); Sacred Shield (wowtbc lists it, the bots have an edge for it, `HealPaladinStrategy.cpp:81-83`, but its value is lower: a 30 s absorb on one target). Eternal Flame "replaces Word of Glory" (spell text) | 114163 | Eternal Flame, same |
| 4 (60) | Unbreakable Spirit (wowtbc: "Personal CD"; Method: more personal cooldowns) | Clemency (Method), Hand of Purity (wowtbc). Unbreakable Spirit cuts the cooldown of Divine Shield, Divine Protection and Lay on Hands (spell text), two of which the bot presses | 114154 | Unbreakable Spirit, same |
| 5 (75) | **Divine Purpose** (derived, see below) | Holy Avenger (Method: when mana is no issue), Sanctified Wrath | 86172 | **Holy Avenger (105809), differs: no Holy edge casts it** |
| 6 (90) | **Execution Sentence** (derived, see below) | Light's Hammer (Method default for raid), Holy Prism (Method: widely spread raid) | 114157 | **Light's Hammer (114158), differs: no Holy edge casts it** |

Why Divine Purpose at 75 (derived): both guides accept Holy Avenger and Divine Purpose (Method names Divine Purpose for mana
problems). `HolyAvengerTrigger` exists but only the Retribution and Protection sets use it (`DpsPaladinStrategy.cpp:75`,
`PaladinTriggers.h:303-307`); the Holy set has no Holy Avenger line. Divine Purpose needs no edge: the Eternal Flame gate
accepts its proc (`PaladinActions.cpp:117-128`, aura 90174), so a proc makes the next Eternal Flame free and it is cast as
3 Holy Power (spell text of 90174).

Why Execution Sentence at 90 (derived): Light's Hammer and Holy Prism have no action at all in the module
(`PaladinAiObjectContext.cpp`: no registration for either); Execution Sentence has one on the tank at 60% health or less
(`HealPaladinStrategy.cpp:66`, `PaladinActions.cpp:132-139`, relevance 21) and its spell text says it heals a friendly target
over time. Both guides list it as an option (wowtbc, standard row 6; Method does not name it).

Proposed link `001122` (Speed of Light / Fist of Justice / Eternal Flame / Unbreakable Spirit / Divine Purpose / Execution
Sentence; tier 5 column 2 and tier 6 column 2 verified with the data reader), or `201122` for Pursuit of Justice.

## Glyphs

Types from GlyphProperties.dbc (0 major, 1 minor), item ids from `Item-sparse.db2`. Slots unlock in pairs at 25, 50 and 75
(`BotFactory.cpp:1338-1345`): a configured glyph goes into the first free slot of its type, in config order, so the order of
the majors decides what is worn at 25 to 49, 50 to 74, and 75 and up. Config order is major, minor, major, minor, minor,
major (`conf/playerbots.conf.dist:2624`).

| Type | Glyph | Item id | Why (one clause) | Bots today (`45741,43365,104051,43340,43369,43367`) |
|------|-------|--------:|------------------|------------------------|
| Major | Flash of Light | 43367 | wowtbc standard, Method alternative; the next heal on that target gets a bonus; Flash of Light (14) is the main heal until Holy Light (34), so it works at 25 | slot 3, worn only from 75 (**derived: should be first**) |
| Major | Beacon of Light | 45741 | wowtbc standard, Method alternative; removes the global cooldown on Beacon of Light (spell text); needs Beacon (39) | slot 1, worn from 25 but **does nothing until 39** |
| Major | Divinity | 41108 | Method main set; Lay on Hands cooldown up, but it returns mana to the paladin (spell text); the bot casts Lay on Hands | not in the build |
| Major | Hand of Sacrifice | 104051 | both guides (Method main set, wowtbc standard); Hand of Sacrifice no longer redirects damage to the paladin; **learned at 80 and no Holy edge casts it** | slot 2, worn from 50, **dead until 80 and then dead without an edge** |
| Major | Protector of the Innocent | 66918 | Method main set; Word of Glory on another target also heals the paladin; with Eternal Flame (which replaces Word of Glory) whether it still applies is unknown | not in the build |
| Major, situational | Divine Protection | 41096 | both guides (wowtbc: physical reduction); no Holy edge casts Divine Protection | not in the build |
| Minor | Contemplation, the Mounted King, Fire From the Heavens | 43365, 43340, 43369 | cosmetic; the Mounted King casts Blessing of Kings on the paladin when it mounts (spell text), a small gain because the bot never casts Kings (see Buffs). Method names Righteous Retreat and Focused Wrath, both cosmetic | yes, all three, harmless |

Disagreement: Method's main set is Hand of Sacrifice, Protector of the Innocent, Divinity; wowtbc's standard set is Beacon
of Light, Hand of Sacrifice, Flash of Light. Picked per level (derived): Flash of Light first (works from 25, used all the
time), Beacon of Light second (worn from 50, useful from 39), Divinity third (works with a button the bots press) until a
Hand of Sacrifice edge exists, then Hand of Sacrifice (both guides, dungeon main). Proposed
`43367,43365,45741,43340,43369,41108`.

## Stat order

Method: with Eternal Flame Haste first (soft caps at 3506, 5335 and 7170 rating at level 90, the last with a haste buff);
without Eternal Flame Mastery, then Spirit; Critical Strike least valuable; Hit and Expertise not needed. wowtbc gives none.
The text does not give a full ordered list, and the caps are level 90 numbers.

For levels 15 to 79 (derived: Mastery: Illuminated Healing is learned at 80, so Mastery rating does nothing before it;
Holy Insight, level 10, lets 50% of Spirit regeneration continue in combat, and Divine Plea is 135% of Spirit per tick):
**Intellect, then Spirit, then Haste, then Critical Strike, Mastery last**; from 80 follow Method (Haste with Eternal Flame,
then Mastery, then Spirit). Plate Specialization (50) adds Intellect only while wearing nothing but plate. For loot rolls
and quest rewards: plate with Intellect and Spirit; a piece with Spirit beats the same piece without; a piece with Strength
or Hit or Expertise is worth nothing to a healer; Mastery is worth nothing below 80.

## What the specialisation knows, by level (5.4.8 data)

Spec spells are from SpecializationSpells.dbc (spec 65); class spells (every paladin) are the paladin rows with their level
from SpellLevels.dbc. Cost is percent of base mana (SpellPower.dbc); cast time from SpellMisc and SpellCastTimes; cooldown
from SpellCooldowns. Word of Glory, Eternal Flame and Light of Dawn cost Holy Power, not mana.

| Level | Spell (id), cost, cast |
|------:|------------|
| 1 | Crusader Strike (35395, 15%, melee, 4.5 s, grants Holy Power once Word of Glory is known), Judgment row 43838 (the casting spell 20271 is level 5) |
| 3 | Seal of Command (105361) |
| 5 | Judgment (20271, 12%, instant, 30 yd) |
| 7 | Hammer of Justice (853, 60 s) |
| 9 | Word of Glory (85673, 1 to 3 Holy Power, heals per charge) |
| 10 | **Holy Shock (20473, 8%, instant, 6 s cooldown, heals an ally or hurts an enemy, +1 Holy Power)**, **Holy Insight (112859, passive: healing up, Word of Glory, Eternal Flame and Light of Dawn up, 50% of Spirit regeneration in combat, more hit for Holy Shock and Judgment)**; the specialisation is chosen here |
| 12 | Righteous Fury (25780, not for a healer) |
| 13 | Redemption (7328, 75%, 10 s cast, out of combat only) |
| 14 | Flash of Light (19750, 37.8%, 1.5 s) |
| 15 | tier 1 talent |
| 16 | Lay on Hands (633, no cost, 10 min cooldown, heals the target for the paladin's maximum health, Forbearance) |
| 18 | Divine Shield (642, 5 min, 8 s, Forbearance) |
| 20 | Cleanse (4987, 16.4%, 8 s cooldown) and Sacred Cleansing (53551, passive: Cleanse also removes Magic), Denounce (2812, 9.4%, 1.5 s), Seal of Truth row (the casting spell 31801 is level 24), Holy Wrath (119072) |
| 24 | Seal of Truth (31801) |
| 26 | Divine Protection (498, 3.5%, 60 s, 10 s) |
| 28 | Holy Radiance (82327, 36%, 2.5 s, heals the target and others within 10 yd for a share, +1 Holy Power), Judgments of the Wise (105424, passive: Judgment grants Holy Power) |
| 30 | Blessing of Kings (20217), tier 2 talent |
| 32 | Seal of Insight (20165, 16.4%, +casting speed, +healing, melee hits heal) |
| 34 | Holy Light (635, 12.6%, 2.5 s) |
| 36 | Rebuke (96231, melee) |
| 38 | Hammer of Wrath (24275, 30 yd, target at 20% health or less) |
| 39 | Beacon of Light (53563, 3 s cooldown, infinite duration, 60 yd) |
| 45 | tier 3 talent (Eternal Flame: 3 Holy Power, instant, a heal plus a 30 s heal over time) |
| 46 | Divine Plea (54428, 2 min cooldown, 9 s, mana equal to 135% of Spirit every 3 s, never less than 12% of maximum mana in total), Turn Evil (10326) |
| 48 | Hand of Protection (1022, 7%, 5 min cooldown, 10 s) |
| 50 | Infusion of Light (53576, passive: Holy Shock crits shorten the next Holy Light, Divine Light or Holy Radiance), Plate Specialization (86103, passive) |
| 52 | Hand of Freedom (1044, 25 s cooldown) |
| 54 | Divine Light (82326, 36%, 2.5 s) |
| 56 | Daybreak (88821, passive: after Holy Radiance the next Holy Shock also heals others near the target) |
| 60 | tier 4 talent, Devotion Aura (31821, 3 min cooldown, 6 s, immunity to Silence and Interrupt, 20% less magic damage for the group) |
| 62 | Divine Favor (31842, 3 min cooldown, 20 s, +20% haste, +20% critical chance) |
| 64 | Tower of Radiance (85512, passive: Flash of Light or Divine Light on the Beacon target may give Holy Power) |
| 66 | Hand of Salvation (1038, 2 min) |
| 70 | Light of Dawn (85222, 3 Holy Power, heals up to 6 injured allies within 30 yd) |
| 72 | Avenging Wrath (31884, 3 min cooldown, 20 s, damage and healing up) |
| 75 | Guardian of Ancient Kings (86669, 3 min cooldown, 15 s: the guardian heals the targets of your heals for an extra 100% of the amount and you gain haste), tier 5 talent |
| 80 | Hand of Sacrifice (6940, 7%, 2 min cooldown, 12 s, 30% of the target's damage goes to the paladin), Mastery: Illuminated Healing (76669, healing places an absorb on the target) |
| 81 | Blessing of Might (19740) |
| 87 | Blinding Light (115750) |
| 90 | tier 6 talent |

Facts the priority lists rest on (spell texts and tables): Beacon of Light heals the Beacon target for 100% of what Holy
Light heals on someone else, 15% of Holy Radiance, Light of Dawn, Light's Hammer and Holy Prism, and 50% of every other heal
(effect base points 50, spell text); healing the Beacon target itself does not fire Beacon (it fires on heals of other
members), so a direct tank heal is "just a heal" and a heal of anyone else is a double heal on the tank. Holy Power builders
for a healer: Holy Shock (8% mana, heals), Holy Radiance (36%), Judgment (12%, needs Judgments of the Wise at 28), Crusader
Strike (15%, melee); spenders: Word of Glory (until Eternal Flame replaces it), Eternal Flame, Light of Dawn (70). Costs in
percent of base mana, cheapest per cast first: **Holy Shock 8.0, Judgment 12.0, Holy Light 12.6, Crusader Strike 15.0, Seal
of Insight 16.4, Cleanse 16.4, Divine Light 36.0, Holy Radiance 36.0, Flash of Light 37.8**, Redemption 75.0. Flash of Light
costs three times Holy Light; the guides' "Flash of Light in emergencies" is therefore the whole reason to cast it. Selfless
Healer (talent 45): each successful Judgment adds a stack (max 3) that makes the next Flash of Light, Divine Light or Holy
Radiance 35% faster, 35% cheaper and 20% stronger on others per stack (spell 114250). Method says Holy Power stacks to 5;
the data files were not read for the cap.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked
"derived" comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to
`mod-aware-bots-548/src/` (module repository).

### Levels 15 to 29

Known: Holy Shock (10), Word of Glory (9), Redemption (13), Flash of Light (14), Lay on Hands (16), Divine Shield (18),
Cleanse with Sacred Cleansing (20), Divine Protection (26), Holy Radiance (28). **No Holy Light until 34, no Seal of Insight
until 32, no blessing until 30, no Beacon until 39.** Flash of Light at 37.8% is the only mana heal, so the cheap tools
decide the fight: Holy Shock on cooldown, Word of Glory at 3 Holy Power, and drinking after every pull.

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | out of combat, a member is dead (from 13) | Redemption (7328, 13) | yes: `GenericPaladinNonCombatStrategy.cpp:30` (40); resurrection in combat does not exist in 5.4.8 (no Intercession row in the data) |
| 2 | core | a member below 25% | Lay on Hands (633, 16) on anyone, then Holy Shock (instant), then Flash of Light | yes: Lay on Hands on the member `GenericPaladinStrategy.cpp:47-48` (91, no role rule), Holy Shock `HealPaladinStrategy.cpp:51` (36), Flash of Light `:54` (34.5) |
| 3 | core | a member below 85% and Holy Shock ready | Holy Shock (20473, 10) on the lowest member (derived: 8% mana, the cheapest heal, and it builds Holy Power) | **gap: Holy Shock only at critical health** (`HealPaladinStrategy.cpp:51`); no edge in the low, medium or almost full bands |
| 4 | core | 3 Holy Power, a member below 65% | Word of Glory (85673, 9) (derived: heals per charge, so three charges for one global cooldown, no mana) | **gap:** spent at 1 or more Holy Power below 45% (`GenericPaladinStrategy.cpp:22-23`, 32) and out of combat at 1 or more (`GenericPaladinNonCombatStrategy.cpp:37-38`, 27 and 28) |
| 5 | core | a deficit that needs a cast and Holy Shock is on cooldown | Flash of Light (19750, 14), kept for deficits of 35% or more until Holy Light is learned (derived) | **gap, mana:** Flash of Light is used in every band, the almost full one included (`HealPaladinStrategy.cpp:70`, `:75` at 13, `GenericPaladinStrategy.cpp:24-25` at 31) |
| 6 | good | a member with a Poison, Disease or Magic effect, from 20; nobody below 45% (derived) | Cleanse (4987, 20), Magic only because of Sacred Cleansing (53551, 20) | partly: `GenericPaladinStrategy.cpp:64-79` (self 52, party 51), above every heal except Lay on Hands (91); see Dispels |
| 7 | good | the bot at 25% or less and Lay on Hands on cooldown or on Forbearance | Divine Shield (642, 18), then Divine Protection (498, 26) | Divine Shield yes (`GenericPaladinStrategy.cpp:26-27`, 25, behind Lay on Hands 90); Divine Protection has no Holy edge |
| 8 | good | three members below 65%, mana above 50% (derived: 36% of base mana for a 2.5 s cast) | Holy Radiance (82327, 28) | partly: needs three below 50% within 10 yd of a hurt member (`HealPaladinStrategy.cpp:40-41`, 34.8; `PaladinTriggers.cpp:69-97`); no mana rule |
| 9 | good | nobody needs healing, mana above 70% (derived) | Judgment (20271, 5): Holy Power from 28 | partly: default action with no mana rule (`HealPaladinStrategy.cpp:19`, relevance 5), fires whenever no heal trigger is on |
| 10 | core | mana below 50%, group out of combat | drink | yes: `triggers/GenericTriggers.cpp:50-78` (conf `AiPlayerbot.DungeonDrinkMana = 50`, `conf/playerbots.conf.dist:2269`) |

### Levels 30 to 44

As above, plus Blessing of Kings (30), Seal of Insight (32), Holy Light (34), Beacon of Light (39).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 11 | core | out of combat, a member lacks it | Blessing of Kings (20217, 30) (derived: stats; both guides name no blessing) | **missing:** a Holy paladin gets `bmight` only (`AiFactory.cpp:705`), whose spell is level 81, and `bcast` is a no-op; Kings is never cast below 81 and Might only from 81 |
| 12 | core | no seal, in a fight, from 32 | Seal of Insight (20165, 32): both guides keep it up | partly: in combat only (`HealPaladinStrategy.cpp:26`, 20; `SealTrigger`, `PaladinTriggers.cpp:17-26`); none out of combat, so the first fight pays 16.4%; below 32 the cast simply fails |
| 13 | core | the main tank is in the group, from 39 | Beacon of Light (53563, 39) on the tank; recast when the tank changes (both guides) | yes: `HealPaladinStrategy.cpp:77-79` (37), trigger `BuffOnMainTankTrigger` (`triggers/GenericTriggers.h:973-983`, own aura, `BeaconOfLightOnMainTankTrigger`, `PaladinTriggers.h:222-227`); main tank is the flagged member or the first living tank spec (`AI/PlayerbotSpec.cpp:191-221`) |
| 14 | core | a member between 25 and 85% with a deficit | Holy Light (635, 34), 12.6%: the efficient heal (derived from cost; Method: "outside emergencies") | partly: Holy Light is in the medium band only (`HealPaladinStrategy.cpp:69`, 19); in the low band Flash of Light (31) and Word of Glory (32) outrank it (25, `:62`); in the almost full band it is absent |
| 15 | core | a member below 45% and a fast heal is needed | Flash of Light (14) | yes (see 5) |
| 16 | good | Holy Shock crit makes the next Holy Light faster (Infusion of Light, 50) | cast Holy Light next | no edge (passive; effect applies whenever Holy Light is cast) |

### Levels 45 to 59

As above, plus tier 3 (45), Divine Plea (46), Hand of Protection (48), Infusion of Light (50), Hand of Freedom (52), Divine
Light (54), Daybreak (56).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 17 | core | 3 Holy Power (or a Divine Purpose proc), the target lacks the heal over time | Eternal Flame (114163, talent 45) on the tank first (both guides; wowtbc: "if the target lacks it") | partly: edges on the lowest member, not the tank (`HealPaladinStrategy.cpp:53` 35, `:60` 26, `:67` 20, `:74` 14), skipped when the bot's own flame is on the target (`GenericSpellActions.cpp:197-206`); in the low band Flash of Light (31) and a one-charge Word of Glory (32) outrank it (26). Whether "word of glory" still resolves once Eternal Flame replaces it is **to check live** |
| 18 | core | mana below 65% | Divine Plea (54428, 46): at least 12% of maximum mana over 9 s, 2 min cooldown, no cost on the data | yes: `GenericPaladinStrategy.cpp:61` (20) through the trigger named "high mana", which is active below 65% (`triggers/GenericTriggers.cpp:121-124`), and `HealPaladinStrategy.cpp:30` (22) below 15% |
| 19 | good | a member would take a large hit or is below 45%, from 54 | Divine Light (82326, 54), 36%: big deficits only (derived) | **wrong order:** Divine Light (19.5) before Holy Light (19) in the medium band (`HealPaladinStrategy.cpp:68-69`) and in the low band (25.5 against 25, shadowed by Flash of Light 31); the cheaper heal comes second |
| 20 | good | after Holy Radiance, from 56 | Holy Shock (heals others near the target, Daybreak) | no edge (Holy Shock only at critical) |
| 21 | good | a member about to die to melee damage, from 48; **never the tank** and never someone who must attack or taunt (Hand of Protection stops physical abilities, spell text) | Hand of Protection (1022, 48) | **never fires in the committed code:** `PartyMemberToProtect::Calculate` starts with `return nullptr;` (`value/PartyMemberToHealValue.cpp:141-143`, BT-129); the dead code below it skips a tank only above 10% health (`:168-171`), so a tank at 10% or less would be chosen once the line is gone. The BT-129 repair in progress revives the value and adds a refusal for a tank target in the Hand of Protection action (`PaladinActions.cpp`, uncommitted), which is the rule this row wants |
| 22 | good | a member is snared or rooted, from 52 | Hand of Freedom (1044, 52) | yes: `GenericPaladinStrategy.cpp:53-55` (24) |
| 23 | good | the tank's heal, from 64 | Flash of Light or Divine Light on the Beacon target (Tower of Radiance, 64, may give Holy Power); other members get Holy Light so Beacon doubles it (derived from the Beacon rates above) | no edge (target choice is lowest health percent, no role rule) |

### Levels 60 to 74

As above, plus tier 4 (60), Devotion Aura (60), Divine Favor (62), Tower of Radiance (64), Hand of Salvation (66), Light of
Dawn (70), Avenging Wrath (72).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 24 | core | 3 Holy Power, two or more members below 85% within 30 yd (Method: area spend) | Light of Dawn (85222, 70) | **missing:** no action for it (`PaladinAiObjectContext.cpp`, none registered) |
| 25 | good | three members below 50%, spread out in time (Method: "avoid all at once") | Avenging Wrath (72), Guardian of Ancient Kings (75), Divine Favor (62), Devotion Aura (60, derived: 20% less magic damage for the group and immunity to interrupts, so for a caster pack) | partly: `HealPaladinStrategy.cpp:34-37` fires Guardian of Ancient Kings (24), Avenging Wrath (23), Divine Favor (22) on three consecutive ticks; Devotion Aura missing (its edge was removed as a keep-up aura, `PaladinBuffStrategies.cpp:28-35`) |
| 26 | good | mana is low | Divine Plea, potion, drink (not Divine Favor: it is haste and critical, spell text) | **wrong:** `HealPaladinStrategy.cpp:31` (21) casts Divine Favor at 15% mana, where haste only drains mana faster |
| 27 | good | a member below 25%, from 60 with Unbreakable Spirit | Lay on Hands (633) faster again | yes through row 2 |

### Levels 75 to 89 and 90

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 28 | core | three members below 50% | Guardian of Ancient Kings (86669, 75) | yes (row 25) |
| 29 | good | the tank below 50% and the bot above 70% health (derived: the paladin takes 30% of the damage, 12 s) | Hand of Sacrifice (6940, 80) (wowtbc order: sixth) | **missing:** no action registered |
| 30 | good | the party lacks a blessing from 81 | Blessing of Might (19740, 81) as the second blessing or when Kings is covered by another source (derived) | only `bmight` exists, see row 11 |
| 31 | expert | a Divine Purpose proc | Eternal Flame free, cast at 3 charges | works through the gate (`PaladinActions.cpp:117-128`) |
| 32 | expert | the tank at 60% or less, from 90 with the talent | Execution Sentence (114157) | yes: `HealPaladinStrategy.cpp:66` (21), tank only (`PaladinActions.cpp:132-139`) |
| 33 | expert | on cooldown with a pull, from 75 with the talent | Holy Avenger (105809) | missing in the Holy set |
| 34 | expert | level 90, in melee reach of an enemy | Crusader Strike on cooldown (Method), Light's Hammer on cooldown | missing, and derived to stay missing for dungeons (see Melee range) |

## Cooldowns and defensives

Order from wowtbc (raid and defensive): Divine Favor, Avenging Wrath, Holy Avenger, Guardian of Ancient Kings, Lay on Hands,
Hand of Sacrifice, Hand of Protection, Devotion Aura, Light's Hammer, Divine Plea, Divine Protection, Divine Shield. Method
names Divine Favor, Avenging Wrath, Holy Avenger and Guardian of Ancient Kings for high damage and says not to use them all
at once; it gives no order for the rest.

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Lay on Hands | 16 | wowtbc fifth | any member below 25% (10 min; Unbreakable Spirit shortens it); on the tank first, then on the healer last | yes, on the lowest member, any role (`GenericPaladinStrategy.cpp:47-48`, 91) and on itself (`:44-45`, 90) |
| Holy Shock | 10 | on cooldown | below 85% on the lowest member (cheapest heal) | only below 25% |
| Divine Shield | 18 | wowtbc last | the bot below 25% after Lay on Hands | yes (`GenericPaladinStrategy.cpp:26-27`, 25) |
| Divine Protection | 26 | wowtbc eleventh | the bot below 50% with an attacker on it | missing (no Holy edge) |
| Hand of Protection | 48 | wowtbc seventh | a hurt DPS under melee attack; never the tank | never fires in the committed code (BT-129, repair in progress) |
| Hand of Freedom | 52 | not listed | a snared member | yes |
| Divine Plea | 46 | wowtbc tenth | mana below 65% | yes (`GenericPaladinStrategy.cpp:61`) |
| Devotion Aura | 60 | wowtbc eighth | the start of a caster pack or a magic burst | missing |
| Divine Favor | 62 | wowtbc first | with a heavy-damage pull, not at low mana | at 15% mana (wrong) and in the cluster |
| Avenging Wrath | 72 | wowtbc second | a hard pull | in the cluster (`HealPaladinStrategy.cpp:36`, 23) and at the "medium group heal" trigger (`:45-47`, 24) |
| Guardian of Ancient Kings | 75 | wowtbc fourth | a hard pull | in the cluster (`:35`, 24) |
| Hand of Sacrifice | 80 | wowtbc sixth | the tank below 50%, the bot above 70% | missing |
| Holy Avenger | talent 75 | wowtbc third | with Avenging Wrath | missing in the Holy set |

## Healer triage and mana

- **Who first (derived from the guides' logic and the thresholds):** 1 any member below 25%: Lay on Hands (16), then the
  instant (Holy Shock 10), then a fast heal (Flash of Light 14); from 80 Hand of Sacrifice on the tank when the healer is
  healthy; Hand of Protection (48) on a non-tank under melee attack, never on the tank; 2 the tank below 45%: Eternal Flame at
  3 Holy Power (45), Divine Light (54) or Holy Light (34), Word of Glory before 45; 3 tank upkeep: Beacon of Light (39) once,
  Eternal Flame when its heal over time is off (45), Judgment and Holy Shock for Holy Power in the gaps; 4 anyone else, lowest
  percent first, with Holy Light (34) so Beacon doubles it; 5 area: Holy Radiance (28) when three are hurt, Light of Dawn
  (70) at 3 Holy Power, Guardian of Ancient Kings, Avenging Wrath and Divine Favor staggered; 6 top-ups below 85%: Holy Shock
  (8%), then Holy Light (12.6%), never Flash of Light (37.8%). Today: lowest percent first for everyone with no tank
  upkeep except Beacon, and Flash of Light is the top-up.
- **Single target and area ladders (Method, kept):** one list for both: Beacon on the tank; spend Holy Power (Word of Glory
  or Eternal Flame single target, Light of Dawn area); Judgment, Holy Shock, Crusader Strike on cooldown; Holy Radiance for
  area; Divine Light on the Beacon target; Holy Light outside emergencies, Flash of Light in emergencies. wowtbc has two
  lists, by talent, with Eternal Flame first when the target lacks it. Method is richer here.
- **Dispels:** Cleanse (20) removes Poison and Disease from a friend, and Magic too because of Sacred Cleansing (20, Holy
  only, spell text); it costs 16.4% with an 8 s cooldown, so it is not free. The edges run at 52 (self) and 51 (member),
  above every heal (30 to 37) and below Lay on Hands (90 to 92) (`GenericPaladinStrategy.cpp:64-79`); `CastCureSpellAction` is
  not a healing action, so the save-mana rule does not apply to it. `HasAuraToDispel` accepts any harmful aura whose
  remaining duration is at least `AiPlayerbot.DispelAuraDuration` (`AI/PlayerbotAI.cpp:3416-3448`); the shipped config says
  700 (0.7 s, `conf/playerbots.conf.dist:601`) while the code default is 7000 (`Utils/PlayerbotAIConfig.cpp:88`), which is
  live is **to check** (BT-130). Derived rule: dispel only when nobody is below 45% and mana is above 30%, except effects that
  stun, silence or deal heavy damage over time.
- **Seal choice (both guides):** Seal of Insight from 32, always. Its text (casting speed, healing spells up, melee hits
  heal) works without meleeing. Below 32 a healer needs no seal (Judgment works without one).
- **Mana, in a fight:** below 60% the shared save-mana rule (`strategy/generic/ConserveManaStrategy.cpp:14-56`) blocks heals
  by their estimate and efficiency label: Holy Shock and Holy Radiance (label low) are blocked unless the target is below 25%,
  Holy Light and Divine Light (label medium, estimate 50) unless the target is at 50% or lower, while Flash of Light, Eternal
  Flame and Word of Glory (label high) pass at any deficit of 15 or more (`PaladinActions.h:141-190, 433-460`). **The labels
  are inverted against the cost data:** Flash of Light (37.8%) is the cheapest to let through, Holy Light (12.6%) and Holy
  Shock (8%) the first to be blocked. Divine Plea below 65% (and at 15%), a mana potion at 40% (`UsePotionsStrategy.cpp:9-10`,
  shared), drinking out of combat below 50% in a dungeon. Divine Favor is not a mana tool.
- **Out of combat top-ups:** Flash of Light on every member below 85% at 25 to 26 and Word of Glory at 27 and 28
  (`GenericPaladinNonCombatStrategy.cpp:31-38`). Derived: Holy Shock (8%) or Word of Glory only, then drink; a Flash of Light
  at 37.8% before a drink spends the mana the drink must give back.
- **Buffs:** Blessing of Kings (30) on the group, Blessing of Might (81) as the mastery blessing; neither is cast by a Holy
  paladin today (row 11). The module treats either blessing as enough on a target (`PaladinTriggers.cpp:27-31`). Righteous Fury
  is a tank spell and a healer must not carry it. No aura keeps up: Devotion Aura is a 3 minute cooldown here.
- **Resurrection:** Redemption (13), 75% mana, 10 s cast, out of combat only, from `GenericPaladinNonCombatStrategy.cpp:30`
  (40); a paladin has no combat resurrection in 5.4.8.
- **Melee range (derived, with Method's "melee healer" stated):** a Holy paladin heals from 40 yd range (Holy Shock, Holy
  Light, Flash of Light, Beacon 60 yd) and the module's Holy set is type ranged (`HealPaladinStrategy.h:21`) with no Crusader
  Strike line. Method keeps Crusader Strike on cooldown at 90 for Holy Power; in the 15 to 60 band Crusader Strike costs 15% of
  base mana against Holy Shock's 8% and puts a healer in front of the pack, so the bot stays at range (expert tier may use it at
  90).
- **Nothing to heal:** in a dungeon the `healer dps` lines are off (`AiFactory.cpp:581`), but the default action Judgment
  stays (`HealPaladinStrategy.cpp:19`): with a dps-assist target (`strategy/generic/DpsAssistStrategy.cpp:5-9`) the healer
  judges whenever no heal trigger is on, at 12% mana per cast and no mana rule. Derived: judge only with mana above 70%
  (Judgment gives Holy Power from 28 and, with Selfless Healer, three stacks make the next Flash of Light, Divine Light or
  Holy Radiance 35% cheaper per stack; it does not give mana). Holy Shock on an enemy belongs to the open-world `healer dps`
  set only (`GenericPaladinStrategy.cpp:96-108`). Whether a healer holds a target and chases it for range is to check live.

## Differences from the guide (5.4.8 against MoP Classic)

- Both guides are level 90. In this data Holy Light is level 34 (before it Flash of Light is the only mana heal), Divine
  Light 54, Beacon of Light 39, Eternal Flame (talent) 45, Divine Plea 46, Light of Dawn 70, Divine Favor 62, Guardian of
  Ancient Kings 75, Hand of Sacrifice 80, Mastery 80. The lists below those levels leave them out.
- Neither guide gives costs. In the data Flash of Light costs 37.8% of base mana, Divine Light 36.0% and Holy Light 12.6%;
  "Holy Light outside emergencies" (Method) is the cost-efficient rule, and for levels 34 to 53 it is also the only big heal.
- Method's "Judgment grants Holy Power" needs Judgments of the Wise (28) here; Crusader Strike grants it once Word of Glory is
  known (spell text).
- Eternal Flame replaces Word of Glory and Fist of Justice replaces Hammer of Justice (spell texts); the module casts both by
  the old names (to check live).
- Sacred Cleansing (Holy, level 20) is what gives Cleanse its Magic part; neither guide says it, and a bot below 20 has no
  Cleanse at all.
- "Aura Mastery" does not exist in 5.4.8 data (no row); Devotion Aura is level 60 and a 3 minute cooldown, not a keep-up aura.
- wowtbc's "Beacon off the global cooldown with the glyph" matches the glyph text (Glyph of Beacon of Light).
- Divine Plea has no healing penalty in the data text (an older version had one).
- Light of Dawn heals up to 6 (4 with its glyph), as Method says; Daybreak (56) is a Holy spec passive, so wowtbc's "Holy
  Radiance feeds Daybreak" is available from 56.
- Method's Holy Power cap of 5 and its haste soft caps (3506, 5335, 7170) were not checked in the data.
- Hand of Sacrifice is a level 80 spell: the guides' dungeon glyph set cannot be used before it.
- The premade comment cites wowtbc for the build; that page marks no pick except the labels "Personal CD" (tier 4) and "Raid
  Spread" (tier 6), so the premade's tier 5 (Holy Avenger) and tier 6 (Light's Hammer) are Method's raid defaults, not
  wowtbc's picks.

## Gap list (ordered by value for levels 15 to 60 in dungeons)

Config (can go at once):

1. **Done (BT-132).** Tier 5 Divine Purpose instead of Holy Avenger, tier 6 Execution Sentence instead of Light's Hammer: nothing in the Holy set
   casts Holy Avenger, Light's Hammer or Holy Prism; Divine Purpose works through the Eternal Flame gate and Execution Sentence
   has an edge. Proposed `PremadeSpecLink.2.0 = 001122` (optional first digit 2 for Pursuit of Justice).
2. **Done (BT-132).** Glyph order: Flash of Light first (works from 25), Beacon of Light second, Divinity third; Hand of Sacrifice (dead until 80
   and no edge) out until a Hand of Sacrifice line exists. Proposed `PremadeSpecGlyph.2.0 = 43367,43365,45741,43340,43369,41108`.

Rotation (class fixes, wait for the owner's in-game test):

1. **Done (BT-132).** **Holy Shock only at critical health** (`HealPaladinStrategy.cpp:51`): it is the cheapest heal (8%) and a Holy Power
   builder; use it on cooldown on the lowest member below 85%, and for Holy Power in the gaps.
2. **Done (BT-132).** **Mana: Flash of Light is the top-up and the labels are inverted** (`HealPaladinStrategy.cpp:70,75`,
   `GenericPaladinStrategy.cpp:24-25`, `GenericPaladinNonCombatStrategy.cpp:31-34`, `PaladinActions.h:141-190`): Flash of
   Light 37.8% against Holy Light 12.6%; Holy Light is absent from the almost full band and loses to Flash of Light in the low
   band, and the save-mana rule lets Flash of Light through while blocking Holy Light and Holy Shock. Order the single
   target ladder by cost: Holy Shock, Word of Glory or Eternal Flame, Holy Light (34), Divine Light (54, big deficits),
   Flash of Light only for emergencies; fix the efficiency labels; out of combat only Holy Shock or Word of Glory.
3. **Done (BT-132).** **No blessing for a Holy paladin before 81** (`AiFactory.cpp:705` names `bmight` and the removed `bcast`): add
   Blessing of Kings from 30 (the Kings strategy exists for tanks, `AiFactory.cpp` `bstats`/`bkings`), Might as second.
4. **Done (BT-132).** **Word of Glory at one Holy Power** (`GenericPaladinStrategy.cpp:22-23`, `GenericPaladinNonCombatStrategy.cpp:37-38`):
   spend at three charges (one below 25% health); and whether "word of glory" still resolves once Eternal Flame replaces it
   (the generic line at 32 would pre-empt Eternal Flame at 26).
5. **Done (BT-132), for every paladin spec; the mana floor is not added.** **Dispels outrank every heal but Lay on Hands** (`GenericPaladinStrategy.cpp:64-79`, 51 and 52), with a 16.4% cost the
   save-mana rule ignores, and fire on any harmful aura of at least the configured duration (BT-130): silent while the lowest
   member is below 45%, a mana floor, and (priest and druid sheets did the same).
6. **Light of Dawn (70) and Hand of Sacrifice (80) have no action**; Hand of Protection never fires in the committed code (BT-129, value stubbed at
   `value/PartyMemberToHealValue.cpp:141-143`; the repair under way also stops it landing on a tank); Holy Avenger,
   Light's Hammer, Holy Prism and Devotion Aura (60) have no Holy line.
7. **Cooldown cluster** (`HealPaladinStrategy.cpp:34-37`): Guardian of Ancient Kings, Avenging Wrath and Divine Favor on
   three consecutive ticks at three members below 50%; stagger (Method: not all at once). **Divine Favor at 15% mana**
   (`:31`) is wrong; remove it from the low mana edge.
8. **Done (BT-132).** **Divine Light before Holy Light** in the medium and low bands (`HealPaladinStrategy.cpp:61,68-69`): reverse by cost, Divine
   Light only for big deficits.
9. **Seal of Insight in combat only** (`HealPaladinStrategy.cpp:26`): no seal out of combat, so the first fight pays 16.4%.
10. **Tank upkeep**: Eternal Flame (45) and the Holy Light for others (Beacon doubles it) have no tank-aimed edge; target
    choice is lowest percent with no role rule (`PartyMemberToHealValue.cpp:33-99`).
11. **Judgment as the default action has no mana rule** (`HealPaladinStrategy.cpp:19`): require mana above 70% in a dungeon.
12. To check live: that a Holy bot below 34 uses Flash of Light only and drinks between pulls; the Fist of Justice and Eternal
    Flame name resolution; the solo Holy paladin's `dps` set after joining a group (`AiFactory.cpp:625-627`); the live
    `DispelAuraDuration`; whether the repeating Sacred Shield and Beacon triggers (a `BuffTrigger` does not test whether the
    spell is known, `triggers/GenericTriggers.cpp:212-225`) cost anything while the spell is unknown (the engine skips an
    impossible action, so probably nothing).

## Implemented on `feat/class-sheet-hpal-brewmaster` (BT-132, compile pending)

What the code does now; the tables above describe the code before it.

- **Heal chain** (`HealPaladinStrategy.cpp`): the shared Word of Glory (32) and Flash of Light (31) lines of
  `GenericPaladinStrategy` are replaced for the holy strategy by its own chain (new hook `InitPartyHealTriggers`, empty for
  holy; Protection and Retribution keep the shared lines). Highest number wins, bands are cumulative: Eternal Flame (26.2)
  and Word of Glory (26.1) at 3 Holy Power for the lowest member under 65 percent (Word of Glory at 1 or 2 Holy Power only
  under the critical threshold, 34.9); Holy Shock on cooldown (25.9) under 85 percent and at critical health (36); Divine
  Light only under 45 percent (25.5); Holy Light in every band (25 low, 19 medium, 13 almost full); Flash of Light at
  critical health (34.5) and as the last line of each band, which only runs where Holy Light is not learned (below 34).
- **Labels** (`PaladinActions.h`): Holy Shock and Holy Light "on party" now carry the high efficiency label, Flash of Light
  "on party" the low one. They are shared with Retribution (`DpsPaladinStrategy.cpp:164-173`) and with the shared lines of
  Protection and Retribution, so those specs also let the cheap heals through and hold Flash of Light back at low mana.
- **Out of combat** (`GenericPaladinNonCombatStrategy.cpp`): a holy paladin tops up with Holy Shock (26) and Holy Light (25)
  and spends Word of Glory at 3 Holy Power (27) or at 1 under the critical threshold (28); no Flash of Light. Other specs
  unchanged.
- **Blessings** (`AiFactory.cpp`, `PaladinBuffStrategies.*`, `PaladinTriggers.*`): the holy set takes `bkings` (Kings from 30)
  and the new `bmight2`, whose trigger fires only for a member who carries Kings from another paladin and only once Might is
  learned (81). `bmight` and the removed `bcast` are gone from the holy set.
- **Dispels** (`PaladinTriggers.*`): the six Cleanse triggers (self and party, Poison, Disease, Magic) are silent while the
  lowest group member is under the low health threshold (45), for every paladin spec.
- **Config**: `PremadeSpecLink.2.0 = 001122`, `PremadeSpecGlyph.2.0 = 43367,43365,45741,43340,43369,41108`.
- Not changed, still open: Light of Dawn, Hand of Sacrifice, Holy Avenger, Devotion Aura, the cooldown cluster, Seal of
  Insight out of combat, tank upkeep, the Judgment mana rule and the to-check items.

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 10 players of this specialisation (rating 2455 to 2662). 2v2: 1 player (rating 2799 to 2799). Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** 3v3 (10 players)
- **BT-324 result:** verified (BT-450); Denounce glyph open (BT-452)
- **Set now:** `PremadeSpecLinkPvp.2.0 = 001220`, `PremadeSpecGlyphPvp.2.0 = 43867,43340,43367,43365,43369,45741` (Denounce in place of Blessed Life, 2026-11-19, BT-683)

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Speed of Light 90% / 0% / 9 of 11, Pursuit of Justice 10% / 100% / 2 of 11 | Speed of Light | Speed of Light | matches |
| 2 (30) | Fist of Justice 90% / 100% / 10 of 11, Evil is a Point of View 10% / 0% / 1 of 11 | Fist of Justice | Fist of Justice | matches |
| 3 (45) | Eternal Flame 100% / 100% / 11 of 11 | Eternal Flame | Eternal Flame | matches |
| 4 (60) | Clemency 80% / 100% / 9 of 11, Unbreakable Spirit 20% / 0% / 2 of 11 | Clemency | Clemency | matches |
| 5 (75) | Divine Purpose 70% / 100% / 8 of 11, Holy Avenger 30% / 0% / 3 of 11 | Divine Purpose | Divine Purpose | matches |
| 6 (90) | Holy Prism 90% / 100% / 10 of 11, Execution Sentence 10% / 0% / 1 of 11 | Holy Prism | Holy Prism | matches |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Flash of Light (43367) | 80% | 0% | 8 of 11 | yes | yes |
| major | Denounce (43867) | 70% | 100% | 8 of 11 | no | yes (BT-683) |
| major | Beacon of Light (45741) | 50% | 100% | 6 of 11 | yes | yes |
| major | Divine Protection (41096) | 30% | 100% | 4 of 11 | no | no |
| major | Blessed Life (41110) | 40% | 0% | 4 of 11 | yes | no (BT-683) |
| major | Devotion Aura (41094) | 30% | 0% | 3 of 11 | no | no |
| minor | Winged Vengeance (43366) | 50% | 0% | 5 of 11 | no | no |
| minor | the Mounted King (43340) | 40% | 0% | 4 of 11 | yes | yes |
| minor | Fire From the Heavens (43369) | 40% | 0% | 4 of 11 | yes | yes |
| minor | Contemplation (43365) | 20% | 0% | 2 of 11 | yes | yes |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **What stays:** all six tiers match in 3v3 (10 players); the one 2v2 player holds the same tiers except tier 15 (Pursuit of Justice). The ladder's blank names are spell 26023 (Pursuit of Justice), 110301 (named "Evil is a Point of View" in the 5.4.8 spell data, tier 30 third column) and 114157 (Execution Sentence).
- **Open (BT-452):** Denounce is a 70 percent major glyph in 3v3 (pooled 8 of 11), but no bot rule casts Denounce (grep of `src`, 2026-11-18), so the glyph would sit idle; Blessed Life (40 percent) stays. A Denounce rule (a Holy Shock into a faster Denounce) would make it worth taking. Minors split (Winged Vengeance 50).

## PvP class package: bots today (BT-683 to BT-687, 2026-11-19)

Status of the digest's section 12 rules after the Holy package (see `docs/pvp/class-packages.md`; all wait for the owner's in-game test). BT-452 is closed: the Denounce rule exists and the glyph is in the set.

| Digest rule | Bots today |
|---|---|
| 1 Beacon on the kill target, Word of Glory at 5 Holy Power | Beacon follows the focused ally (BT-687); the Holy Power spender is as before |
| 2 Holy Shock on cooldown, spend at 5 | as before |
| 3 Divine Protection when stunned | the shared defensive layer (catalog tempo row, BT-522) |
| 4 Hand of Freedom on a rooted ally | as before (`PaladinTriggers.cpp:186`) |
| 5 Hand of Protection on a stunned or burst ally | done (BT-685) |
| 6 Hand of Sacrifice before a control | done (BT-684) |
| 7 Divine Shield against the medallion | the shared defensive layer |
| 8 Devotion Aura when silenced | done (BT-686) |
| 9 Divine Favor, Avenging Wrath, Guardian timing | partly, as before (group heal edges) |
| 10 Fist of Justice, Blinding Light on the healer | Hammer of Justice is a kit row; Blinding Light is not built |
| 11 pillar and instants only after a swap | not built |
| 12 Denounce in the downtime | done (BT-683), with the glyph |
