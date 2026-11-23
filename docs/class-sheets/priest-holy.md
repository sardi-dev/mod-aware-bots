# Priest: Holy (healer)

- **Status:** checked against 5.4.8 (2026-11-10). Implemented, compile and live check pending: gap list items 1 to 4
  (owner approved 2026-11-10, branch `feat/class-sheet-priest-paladin`); the "bots today" columns below describe the
  state before them. Every "bots today" claim below was checked by reading the call sites unless it says "to check".
- **Sources:** Method, "Holy Priest Class Guide - MoP Classic" (Devorta, 23 July 2025, Phase 1, level 90 raids and
  dungeons), read 2026-11-10; wowtbc.gg, "Holy Priest PvE Guide - MoP Classic" (no patch line, the page the module's
  premade comments cite), read 2026-11-10; the server's own data files (Spell.dbc with its tooltip text,
  SpellLevels.dbc, SpecializationSpells.dbc, SkillLineAbility.dbc, SpellPower.dbc, SpellCooldowns.dbc, Talent.dbc,
  GlyphProperties.dbc, Item-sparse.db2 of build 18414) for every spell id, learn level, cost, cooldown, talent tier and
  glyph. Overgear, Icy Veins and Wowhead are not readable with a plain fetch, and a web archive copy and Noxxic (a
  retail page) gave nothing, so there are two guides. Neither has a levelling section, a dispel section, buff advice,
  resurrection, mana thresholds or triage thresholds: those lines are derived (marked "derived").
- **Levels covered:** 15 to 90, with the detail on 15 to 60 (dungeons with five players).

## Which strategy a Holy priest runs

Read from the factory, not assumed:

- A Holy priest in a group, which is every dungeon, runs **`HolyHealPriestStrategy`** ("holy heal",
  `HolyPriestStrategy.cpp:61-158`), added at `AiFactory.cpp:343`. It has no `HealPriestStrategy` base.
- `HealPriestStrategy` ("heal", `HealPriestStrategy.cpp`) is the **Discipline** strategy (`AiFactory.cpp:337-340`). A Holy
  priest never runs it in a group.
- `HolyPriestStrategy` ("holy dps", `HolyPriestStrategy.cpp:10-59`, a subclass of `HealPriestStrategy`) is added on top
  only when the bot has **no group** (`AiFactory.cpp:597-600`), with the shadow debuff and area strategies. It is the
  solo and leveling set (Holy Fire, Shadowfiend, Chakra, Lightwell and Smite as the default action). Whether a solo bot
  that joins a group drops it is to check (a reset rebuilds the set, `AI/PlayerbotAI.cpp:2093`).
- Both sets also get "dps assist" and "cure" (`AiFactory.cpp:350`), "save mana" (`:577`) and, on maps outside the
  restricted list, "healer dps" (`:581`). The default restricted list (`AiFactory.cpp:75-78`, config `AiPlayerbot.RestrictedHealerDPSMaps`) holds the classic,
  Burning Crusade, Wrath, Cataclysm and Mists dungeon map ids (not every id was checked one by one), so a healer in
  those dungeons casts no damage spell.
- How an action is chosen (read, `Engine.cpp:150-260`): every active trigger pushes its actions with their numbers; the
  engine runs the highest-numbered action that is useful and possible, once per tick. A failed cast pushes the node's
  alternative. Heal numbers are 10 to 19 (light), 20 to 29 (medium), 30 to 39 (critical); dispels are 30 to 41; mana potion
  90. So the "priority list" of the bot is the sorted list of all numbers whose trigger is on.
- The heal target is the group member with the lowest health percent, a distance penalty added
  (`PartyMemberToHealValue.cpp:33-99`); the tank is not preferred and nothing weighs a member by role. The trigger bands
  are critical below 25, low below 45, medium below 65, almost full below 85 (conf `AiPlayerbot.CriticalHealth` and
  the three after it).

## Talents

Tiers and columns are from Talent.dbc (the guides show builds as images or option lists). "Bots today" is
`AiPlayerbot.PremadeSpecLink.5.1 = 000001` (one digit per tier, the column counted from 0).

| Tier (level) | Pick | Alternative and when | Spell id | Bots today |
|--------------|------|----------------------|----------|------------|
| 1 (15) | Void Tendrils (Method: all three interchangeable) | Psyfiend; Dominate Mind (Method: levelling and dungeons). A Holy bot has no edge for any of the three (Void Tendrils is cast only by the shadow set), so the pick changes nothing | 108920 | Void Tendrils, same |
| 2 (30) | Body and Soul (Method, raid) | Angelic Feather (Method: the dungeon option, no edge exists); Phantasm. Body and Soul is passive and costs nothing | 64129 | Body and Soul, same |
| 3 (45) | From Darkness, Comes Light (both guides name it with Mindbender) | Mindbender (Method: more reliable mana). The module casts "shadowfiend" by name and its trigger reads the cooldown of 34433; whether Mindbender (which replaces it) is castable that way is unverified, so From Darkness, Comes Light stays; the Surge of Light edge exists (`HolyPriestStrategy.cpp:107-108`) | 109186 (proc 114255) | From Darkness, Comes Light, same |
| 4 (60) | Desperate Prayer (Method: active) | Angelic Bulwark (Method: passive); Spectral Guise (wowtbc only). The bot casts Desperate Prayer at critical health (`GenericPriestStrategy.cpp:54`) | 19236 | Desperate Prayer, same |
| 5 (75) | **Divine Insight** (Method: "best option") | Twist of Fate; Power Infusion (wowtbc only) | 109175 | **Twist of Fate (109142), differs** |
| 6 (90) | **Halo** (derived, see below) | Cascade (Method: simplest), Divine Star (Method: best in a line) | 120517 | **Divine Star (110744), differs: no Holy edge ever casts it** |

Why Halo at 90 (derived): both guides accept all three; of the three, only Halo has an edge for a Holy bot
(`GenericPriestStrategy.cpp:52-53`, three members below 65% in a full group). Cascade is cast only by the shadow set
(`ShadowPriestStrategy.cpp:19`), Divine Star by nobody. Why Divine Insight at 75: Method names it best, and the bot
casts Greater Heal and Prayer of Mending, the two spells it works with; the module has no edge that uses Twist of Fate
either way (both are passive, so this is a choice between two passives).

Which guide was richer: Method names a default for tiers 3 to 6 and gives reasons; wowtbc lists three options per tier
without a marked pick. They agree on every option except that wowtbc also lists Phantasm, Solace and Insanity, Spectral
Guise and Power Infusion. The premade's picks for tiers 5 and 6 are therefore not "wowtbc's picks": the page names none.

## Glyphs

Majors unlock at 25, 50 and 75, minors in the same pairs (`BotFactory.cpp:1338-1345`): a configured glyph goes into the first
free slot of its type, in config order, so the order of the majors decides what is worn at 25 to 49, 50 to 74 and 75 and
up. Effects read from GlyphProperties and Spell.dbc text.

| Type | Glyph | Item id | Why | Bots today (PremadeSpecGlyph.5.1 = 42396,87276,42403,79538,86541,42409) |
|------|-------|--------:|-----|------------|
| Major | Renew | 42411 | Method: small group content; Renew heals more per tick with 3 s less duration; works from 26 (Renew is level 26), the only guide glyph useful at 25 to 49 | not in the build |
| Major | Circle of Healing | 42396 | both guides (Method: if mana is no issue). **Derived: Circle of Healing already hits up to 5 members, so the extra target (and its 35% higher cost) gains nothing in a 5-player group**; needs Circle of Healing (level 50) | **first major, worn from 25, does nothing until 50 and is useless in a party of five** |
| Major | Deep Wells | 42403 | both guides; +2 Lightwell charges; needs Lightwell (level 36) | second major, worn from 50, works |
| Major | Spirit of Redemption | 42409 | both guides; +10 s of free healing after death; Spirit of Redemption is level 30 | third major, worn from 75 |
| Major | Fade | 42398 | Method first, wowtbc personal cooldown; Fade also cuts damage taken; the bot casts Fade only at medium threat (`GenericPriestStrategy.cpp:33`) | not in the build |
| Major | Inner Sanctum or Inner Fire | 42400, 42402 | both guides, by damage type; no data on which a dungeon wants | not in the build |
| Minor | Holy Resurrection, the Heavens, Confession | 87276, 79538, 86541 | all cosmetic (Method: no minor is strong, wowtbc lists none) | yes, all three |

## Stat order

Method: Mastery, then Spirit, then Critical Strike, then Haste (secondary stats, level 90, no breakpoints); wowtbc gives none.
For levels 15 to 79 (derived: Mastery: Echo of Light is learned at level 80, so Mastery rating does nothing before it):
**Intellect, then Spirit, then Critical Strike, then Haste, and Mastery last**; from 80 follow Method (Intellect, Mastery,
Spirit, Critical Strike, Haste). Spirit matters more than for most casters because Meditation (level 10) lets part of
Spirit regeneration run in combat. For loot rolls and quest rewards: cloth with Intellect and Spirit; a piece with Spirit
beats the same piece without; Mastery is worth nothing below 80.

## What the specialisation knows, by level (5.4.8 data)

Spec spells are from SpecializationSpells.dbc; class spells (every priest) are the Priest skill line rows in
SkillLineAbility.dbc with their level from SpellLevels.dbc. Cost is percent of base mana (SpellPower.dbc), cast time
from SpellMisc and SpellCastTimes (column inferred from known spells: Resurrection 10 s, Flash Heal 1.5 s).

| Level | Spell (id), cost, cast |
|------:|------------|
| 1 | Smite (585) |
| 3 | Shadow Word: Pain (589) |
| 5 | Power Word: Shield (17), 6.1%, instant, 6 s cooldown, leaves Weakened Soul |
| 7 | Flash Heal (2061), 5.9%, 1.5 s |
| 9 | Inner Fire (588) |
| 10 | Spiritual Healing (87336, passive, more healing from Flash Heal, Binding Heal, Renew, Prayer of Mending), Meditation (95861, passive), Holy Word: Chastise (88625, 30 s cooldown) |
| 12 | Psychic Scream (8122) |
| 16 | Divine Fury (122098, passive, hit chance) |
| 18 | Holy Fire (14914), Resurrection (2006, 4.0%, 10 s cast, out of combat only) |
| 22 | Purify (527, 2.6%, 8 s cooldown, removes Magic and Disease from a friend), Power Word: Fortitude (21562, 1.0%, buffs the whole group) |
| 24 | Fade (586, 30 s cooldown) |
| 26 | Renew (139, 2.6%, instant), Dispel Magic (528, hostile target only) |
| 28 | Heal (2050, 1.9%, 2.5 s) |
| 30 | Spirit of Redemption (20711, passive), Focused Will (45243) |
| 34 | Greater Heal (2060, 5.9%, 2.5 s), Serendipity (63733, passive: two Flash Heals or Binding Heals make Greater Heal and Prayer of Healing faster and cheaper), Levitate (1706) |
| 36 | Lightwell (126135, 5.4%, 3 minute cooldown) |
| 42 | Shadowfiend (34433, 3 minute cooldown) |
| 44 | Evangelism (81662, passive) |
| 46 | Prayer of Healing (596, 4.5%, 2.5 s) |
| 48 | Binding Heal (32546, 5.4%, 1.5 s) |
| 50 | Circle of Healing (34861, 3.2%, instant, 10 s cooldown) |
| 54 | Fear Ward (6346, 0.9%, 3 minute cooldown) |
| 56 | Chakra: Sanctuary (81206), Chakra: Serenity (81208), Chakra: Chastise (81209), each 30 s cooldown; they turn Holy Word: Chastise into Holy Word: Sanctuary (88685, 3.8%, 40 s cooldown) or Holy Word: Serenity (88684, 2.0%, instant, 10 s cooldown in the data, the stance tooltip says 15) |
| 60 | tier 4 talent |
| 64 | Rapid Renewal (95649, passive: Renew heals more, has an instant part and a shorter global cooldown) |
| 66 | Hymn of Hope (64901, 6 minute cooldown, channel) |
| 68 | Prayer of Mending (33076, 3.5%, instant, 10 s cooldown) |
| 70 | Guardian Spirit (47788, 0.9%, 3 minute cooldown) |
| 72 | Mass Dispel (32375, 13%) |
| 75 | tier 5 talent |
| 78 | Divine Hymn (64843, 6.3%, 3 minute cooldown, channel) |
| 80 | Mastery: Echo of Light (77485), Inner Will (73413) |
| 87 | Void Shift (108968) |
| 90 | tier 6 talent |

Chakra: Serenity adds 25% to single-target healing and makes those heals refresh Renew on the target; Chakra:
Sanctuary adds 25% to area healing and cuts the Circle of Healing cooldown by 2 s (SpellEffect.dbc). Before level 56
there are no stances and no Holy Words.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only).

### Levels 15 to 29
Known: Flash Heal, Power Word: Shield, Inner Fire, Resurrection (18), Purify and Fortitude (22), Fade (24), Renew (26),
Heal (28). Only Flash Heal heals until 26, and at 5.9% it is dear: keep the fight short, drink after each pull.

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | out of combat, Inner Fire missing | Inner Fire (588, 9) | yes: `PriestNonCombatStrategy.cpp:28-29` (10) |
| 2 | core | out of combat, a group member lacks it, from 22 | Power Word: Fortitude (21562, 22) | yes: `PriestNonCombatStrategy.cpp:83-85`, set added by "buff" (`AiFactory.cpp:797`) |
| 3 | core | a member is dead, out of combat, from 18 | Resurrection (2006, 18) | yes: `PriestNonCombatStrategy.cpp:41-43`; guarded in the Spirit of Redemption window (`PriestActions.h:87`) |
| 4 | core | a member below 25% | Power Word: Shield (5), then Flash Heal (7) | yes: `HolyPriestStrategy.cpp:117-124` (shield 35, Flash Heal 31). The shield is not cast on a member who has Weakened Soul (to check) |
| 5 | core | a member below 65% | Flash Heal (7); from 28 Heal (28) | yes: Flash Heal `:144` (14), `:134` (21); from 28 Heal is cast only as the alternative of an unknown Greater Heal (`GenericPriestStrategyActionNodeFactory.h`, node "greater heal on party"), so it is used from 28 to 33 and never after (see line 11) |
| 6 | core | tank upkeep: pull starts, from 26 | Renew (139, 26) on the tank (derived: instant, 2.6%, both guides keep Renew on the tank) | **gap:** no tank-aimed edge; Renew is cast only in the "almost full" band on the lowest member (`HolyPriestStrategy.cpp:148-153`, 12) |
| 7 | good | a harmful Magic or Disease effect on a member, from 22 | Purify (527, 22) | partly: edges exist (`GenericPriestStrategy.cpp:95-103`, 41, 40, 31, 30) but see Dispels below: they sit above every heal |
| 8 | good | mana below 50%, nobody of the group in combat | drink | yes: `UseFoodStrategy.cpp:49`, `GenericTriggers.cpp:51` |
| 9 | good | a mob is on the healer | Fade (586, 24) | partly: Fade at "medium threat" (`GenericPriestStrategy.cpp:33`, 55), above every heal |
| 10 | expert | nobody hurt, in a dungeon | nothing (derived: Flash Heal is 5.9% of base mana, no damage spell is worth the mana below 28; guides give no damage rotation) | yes: healer dps is off on dungeon maps (`AiFactory.cpp:581`); default action "shoot" (`HolyPriestStrategy.cpp:66-69`) is a dead action in 5.4.8 (BT-038), to check it only fails quietly |

### Levels 30 to 44
As above, plus Heal as the low-damage filler, Greater Heal (34), Lightwell (36), Spirit of Redemption (30).

| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 11 | core | tank or member below 45%, deficit large (derived: Greater Heal 5.9% for big deficits, Heal 1.9% for small ones, Method's rule) | Greater Heal (34), Heal (28) | partly: Greater Heal at 25 beats Flash Heal at 14 for everyone below 65% (`HolyPriestStrategy.cpp:143-144`) and Heal is never chosen once Greater Heal is known: **gap, mana-inefficient** for 65 to 85% deficits (the save-mana rule blocks Greater Heal above 50% only when mana is under 60%, `ConserveManaStrategy.cpp:49-53`) |
| 12 | core | below 25% | Flash Heal (7), fast; Greater Heal is the alternative | yes: `HolyPriestStrategy.cpp:122-123` |
| 13 | good | Flash Heal (or Binding Heal) just cast twice | Greater Heal next (Serendipity, 34) | **missing:** no Serendipity edge (expert) |
| 14 | good | a fight begins, from 36 | Lightwell (126135, 36) | yes but crude: cast on the first tick of every fight, 3 minute cooldown (`HolyPriestStrategy.cpp:82-83`, `PriestTriggers.cpp:74-78`); fine for dungeon trash spacing, to check the duration |
| 15 | core | mana below 40%, from 42 | Shadowfiend (34433, 42), mana potion | partly: `GenericPriestStrategy.cpp:66-68` (22) casts on "current target", so a healer with no target casts nothing, to check live; potion at 90 (`UsePotionsStrategy.cpp:9-10`) |
| 16 | good | the bot is the target of an attacker | Power Word: Shield on self | yes but spendy: `GenericPriestStrategy.cpp:38-39` (21), `:59-63`, 6.1% each |

### Levels 45 to 59
As above, plus Prayer of Healing (46), Binding Heal (48), Circle of Healing (50), Fear Ward (54), Chakra (56).

| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 17 | core | three or more members below 85% (derived: Circle of Healing reaches all five in a party) | Circle of Healing (34861, 50), on cooldown; Prayer of Healing (46) before 50 | partly: `HolyPriestStrategy.cpp:85-96`; **the trigger needs five group members near** (`HealthTriggers.cpp:29-33`), so in a group of 2 to 4 there is no area heal at all |
| 18 | good | the bot and a member both hurt | Binding Heal (32546, 48) | yes: `HolyPriestStrategy.cpp:111-112` (25), trigger `PriestTriggers.cpp:109-113` |
| 19 | good | Fear Ward on the tank before a pull, from 54 | Fear Ward (6346, 54) | yes: `GenericPriestStrategy.cpp:35-36` (23) |
| 20 | core | from 56, default stance | Chakra: Serenity (81208, 56) | yes: `HolyPriestStrategy.cpp:79-80` (38), trigger `PriestTriggers.cpp:60-64` |
| 21 | core | from 56, in Serenity, a member below 65% | Holy Word: Serenity (88684), instant, 2.0%, 10 s; cheaper than Flash Heal and a shield | yes: `HolyPriestStrategy.cpp:120,130,140` (34, 25, 26) |
| 22 | good | from 56, three members below 80% within 30 yards | Chakra: Sanctuary, then Holy Word: Sanctuary (88685), on cooldown | yes: `PriestTriggers.cpp:45-70` (works from groups of three), `HolyPriestStrategy.cpp:101-104` (38; Holy Word: Sanctuary needs the 5-member trigger, three below 65%) |
| 23 | good | the 30 s Chakra cooldown (data) | do not flip: stay in Serenity (derived: dungeon damage is mostly on the tank), Sanctuary only for area pulls | **gap:** the out-of-combat edge casts Sanctuary whenever Sanctuary is missing (`PriestNonCombatStrategy.cpp:33-34`, 11), then the first combat tick wants Serenity: within 30 s of the rest the swap may fail on cooldown. To check live |
| 24 | core | save mana below 60% | skip top-ups | yes (shared): `ConserveManaStrategy.cpp:14-56`, added at `AiFactory.cpp:577` |

### Levels 60 to 74
As above, plus tier 4 talent (60), Rapid Renewal (64), Hymn of Hope (66), Prayer of Mending (68), Guardian Spirit (70), Mass Dispel (72).

| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 25 | core | pull starts | Prayer of Mending (33076, 68) on the tank, on cooldown (both guides) | partly: cast on cooldown on the lowest member (`HolyPriestStrategy.cpp:88,94,121,132,142,152`), **not on the tank**, and the 5-member gate applies to the group lines |
| 26 | good | a member below 25% | Guardian Spirit (47788, 70), 3 minute cooldown | yes: `HolyPriestStrategy.cpp:117` (36); aimed at the lowest member, any role |
| 27 | good | the bot below 25% | Desperate Prayer (19236, talent 60) | yes: `GenericPriestStrategy.cpp:54` (25) |
| 28 | good | mana below 15% | Hymn of Hope (64901, 66) | partly: `GenericPriestStrategy.cpp:72` (20); at 15% a channel the group must wait out is late (derived), to check |

### Levels 75 to 89 and 90
| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 29 | good | three members below 65% in a full group | Divine Hymn (64843, 78), Halo (90) | yes: `HolyPriestStrategy.cpp:93` (37), `GenericPriestStrategy.cpp:53` (33) |
| 30 | expert | the bot at high health and a member near death (Method: use it when the priest is high on health) | Void Shift (108968, 87) | missing: no Holy edge |
| 31 | expert | magic debuff on a group of members at once | Mass Dispel (32375, 72) | missing: the action exists (`PriestActions.h:27`), no Holy edge |

## Cooldowns and defensives

- **Emergency buttons by level** (derived order, instant first): Power Word: Shield (5), Flash Heal (7), Holy Word: Serenity
  (56), Guardian Spirit (70, tank save, 3 minutes), Desperate Prayer (60 talent, self), Spirit of Redemption (30, passive).
  Bots: all of them exist as edges; the order differs (see the gap list, item 5).
- **Lightwell** (36), **Divine Hymn** (78), **Hymn of Hope** (66), **Shadowfiend** (42): see the lines above. Both guides rank
  the raid cooldowns Divine Hymn, Guardian Spirit, Lightwell, Void Shift, Hymn of Hope, Shadowfiend (Method puts Void
  Shift third, wowtbc fourth; wowtbc adds Fade).
- **Spirit of Redemption** (30, baseline for Holy in 5.4.8): on death the priest can cast healing spells free of cost
  for the spirit window (about 15 s, the glyph adds 10). The module guards Inner Fire and Resurrection in that window
  (`PriestTriggers.cpp:17-29`, `PriestActions.h:87`), and its comments show the bot's AI keeps ticking then. That the
  combat heal edges fire in the window is to check live.
- **Fade** (24): at medium threat only (`GenericPriestStrategy.cpp:33`), and only in a group against non-players
  (`PriestActions.cpp:11-23`).

## Healer triage and mana

- **Who first (derived from both guides' logic and the thresholds):** 1 a member below 25%: an instant first (Holy Word:
  Serenity from 56, Power Word: Shield, Guardian Spirit from 70 on the tank), then Flash Heal; 2 the tank below 45%:
  Greater Heal (34) or Heal; 3 the tank's upkeep: Renew always (26), Prayer of Mending (68) on cooldown, Fear Ward on the
  tank before a fear caster (54); 4 anyone else, lowest percent first; 5 area: Circle of Healing on cooldown (50), Prayer
  of Healing before that (46), Chakra: Sanctuary with Holy Word: Sanctuary (56), Divine Hymn (78) when three are below
  65%; 6 top-ups: Renew, Heal. Today: lowest percent first for everyone, no tank upkeep.
- **Single target ladder (Method, kept):** Holy Word: Serenity, Greater Heal for high damage, Heal for low damage, Flash
  Heal for an emergency. **Area ladder (Method, kept):** Holy Word: Sanctuary, Cascade (not in the bots), Circle of Healing,
  Prayer of Healing as filler. wowtbc has one merged list (Inner Fire, Chakra, Holy Word, Prayer of Mending, Divine Star,
  Circle of Healing, Prayer of Healing, Flash Heal or Binding Heal) and no thresholds; Method is richer here.
- **Dispels:** Purify (22) takes Magic and Disease from a friend; a priest has no poison or curse dispel. Dispel Magic
  (26) is hostile-target only and no edge casts it. The edges run at 41 (self, Magic), 40 (member, Magic), 31 and 30
  (Disease) (`GenericPriestStrategy.cpp:95-103`), above every heal (30 to 38), and `HasAuraToDispel` accepts any
  harmful aura of the type with 0.7 s or more left (`AI/PlayerbotAI.cpp:3380-3446`, conf `DispelAuraDuration` 700, the value the server runs; BT-130). So a harmless
  Magic debuff can delay a Guardian Spirit. Derived rule: dispel only when nobody is below 45%.
- **Shield and Renew habits:** Method and wowtbc name Renew for the tank and none for Power Word: Shield (a Holy priest
  has no Divine Aegis or Spirit Shell). Derived: Power Word: Shield (6.1%) is the instant of last resort and Holy Word:
  Serenity (2.0%) is the cheaper instant from 56. The bot orders them the other way round (`HolyPriestStrategy.cpp:118,120`:
  shield 35, Holy Word 34) and also shields itself whenever an attacker targets it (`GenericPriestStrategy.cpp:38-39`).
- **Mana, in a fight:** below 60% the shared save-mana rule blocks Flash Heal on members at 25% and above and Greater
  Heal above 50% (`ConserveManaStrategy.cpp:14-56`); Shadowfiend and Inner Focus at 40% (`GenericPriestStrategy.cpp:66-68`; Inner Focus is
  Discipline only and a no-op); a mana potion at 40% (90); Hymn of Hope at 15%. Out of a fight: drink below 50% in a
  dungeon (`DungeonDrinkMana`), 15% elsewhere. Cost table (percent of base mana, data): Heal 1.9, Renew 2.6, Holy Word:
  Serenity 2.0, Circle of Healing 3.2, Prayer of Mending 3.5, Prayer of Healing 4.5, Binding Heal 5.4, Flash Heal 5.9,
  Greater Heal 5.9, Power Word: Shield 6.1, Halo 13.5. Derived: the cheap casts first for small deficits, Flash Heal only
  for emergencies. Heal is reached only as the alternative of an unknown Greater Heal (levels 28 to 33), and Renew only on the lowest member below 85%.
- **Out of combat top-ups:** Flash Heal, Renew, Greater Heal and Circle of Healing on hurt members
  (`PriestNonCombatStrategy.cpp:48-71`; "penance on party" in it is a Discipline spell and a no-op for Holy). Derived:
  Renew and Heal only, then drink; a Flash Heal at 5.9% before a drink spends the mana the drink must give back.
- **Buffs:** Inner Fire (9) always; Inner Will (80) is never used and does not need to be (it replaces armor and spell
  power with movement); Power Word: Fortitude (22) on the group out of combat, as above. Both guides name Inner Fire only
  (Method: "maintain"; wowtbc: first in its list).
- **Resurrection:** Resurrection (18) out of combat only, 10 s cast, from `PriestNonCombatStrategy.cpp:41-43`; a priest has
  no combat resurrection in 5.4.8 (Mass Resurrection is a guild perk, not a priest spell).
- **Nothing to heal:** in a dungeon nothing (healer dps is off on dungeon maps); in the open world "healer dps" casts
  Shadow Word: Pain, Holy Fire and Smite when every member is at 85% or more and mana is above a threshold
  (`GenericTriggers.cpp:434-461`, `GenericPriestStrategy.cpp:119-139`). Mind Blast is a Shadow spell and is not known to Holy
  (it is level 21 for spec 258 only).
- **Which Chakra for a dungeon (derived):** Serenity as the default (both guides: Serenity for single target; wowtbc
  says "usually Sanctuary" for a raid), Sanctuary when three or more are hurt, never swapping within the 30 s
  cooldown. The bots' trigger matches this (`PriestTriggers.cpp:45-70`); only the out-of-combat Sanctuary edge disturbs it.

## Differences from the guide (5.4.8 against MoP Classic)

- Both guides are level 90. Heal is level 28, Greater Heal 34, Renew 26, Prayer of Healing 46, Binding Heal 48, Circle of
  Healing 50, Prayer of Mending 68, Guardian Spirit 70, Divine Hymn 78. Below those the bot must make do with what it has.
- Holy Word: Serenity and Holy Word: Sanctuary are not learned spells here: Chakra (level 56) turns Holy Word: Chastise
  (level 10) into them. Neither guide says that, so a bot below 56 has no Holy Words and no stances.
- The stance tooltip says Holy Word: Serenity has a 15 s cooldown; its own spell data says 10 s.
- Mastery: Echo of Light is level 80: the guides' Mastery-first stat order does not apply below it.
- Neither guide covers Resurrection (18), Fortitude (22), Fear Ward (54), Purify (22), Binding Heal, Heal: those lines are derived.
- Divine Star and Cascade exist as talents; no Holy edge casts either. Power Word: Solace needs the Solace and Insanity talent.
- Mass Resurrection is a guild perk and not a priest spell (it appears at level 28 in SpellLevels but belongs to another skill line).
- wowtbc's Spectral Guise (112833), Phantasm (108942), Power Infusion (10060) and Twist of Fate (109142) exist as talents.

## Gap list (ordered by value for levels 15 to 60 in dungeons)

1. Renew on the tank from 26 (core, both guides, derived for the band): no tank-aimed edge; Renew only in the "almost full" band on the lowest member.
   **Implemented:** trigger and action "renew on main tank" (priority 21.9, mana above the save-mana threshold), and
   "prayer of mending on main tank" (21.8, level 68, "good" tier, off cooldown, the bot's charge missing; this is also
   item 7). Both sit under the critical and low health chains (`HolyPriestStrategy.cpp`).
2. Group heals need five members near (`HealthTriggers.cpp:29-33`): in a group of 2 to 4 there is no Circle of Healing, Prayer of Healing, Divine Hymn or Halo (good; affects the owner's smaller groups).
   **Implemented:** triggers "holy group heal setting" and "holy medium group heal setting" (`HolyGroupHealTrigger`):
   three hurt members are enough in a party of up to four, five or more near keep the shared rule. The shared trigger is
   unchanged; Divine Hymn and Holy Word: Sanctuary stay on it.
3. Heal (28) is never chosen after Greater Heal (34): Greater Heal wins every band below 65% (good, Method's low and high damage rule; mana).
   **Implemented:** Heal in the medium band (21) and as the last line of the almost full band (10); Greater Heal only in
   the low band (22) and the critical chain; Flash Heal at critical health (31) and, below Heal in the medium band (14),
   as the heal at levels 7 to 27. The shared save-mana rule is unchanged.
4. Dispels (41, 40) outrank every heal, including emergencies, and fire on any harmful Magic aura with 0.7 s or more left (good; a harmless debuff can delay a save).
   **Implemented:** the four Purify edges (self and party, Magic and Disease) are silent while the lowest group member is
   below the low health threshold (45). The aura time rule (0.7 s) is unchanged.
5. Power Word: Shield (35) before Holy Word: Serenity (34) from 56, and a self shield whenever attacked (good, derived from cost data).
6. Out-of-combat Chakra: Sanctuary edge (`PriestNonCombatStrategy.cpp:33-34`) fights the combat Serenity default under the 30 s cooldown (good; verify live which stance a pull starts in).
7. Prayer of Mending goes to the lowest member, not the tank (good). **Implemented** with item 1 ("prayer of mending on main tank"); the group lines still aim it at the lowest member.
8. Out-of-combat Flash Heal at critical health before drinking (`PriestNonCombatStrategy.cpp:49-53`): Renew or Heal instead (good).
9. Mana: Shadowfiend needs a current target (to check live), Hymn of Hope at 15% is late (good, to check).
10. Premade tier 5 Divine Insight and tier 6 Halo (config, derived reasons above); premade glyph order Renew, Deep Wells, Spirit of Redemption instead of Circle of Healing, Deep Wells, Spirit of Redemption (config).
11. Serendipity (34) not used; Void Shift (87) and Mass Dispel (72) have no Holy edge (expert, later).
12. Spirit of Redemption window: whether combat heals fire in it; a solo Holy bot's "holy dps" set after joining a group; the dead default action "shoot" (all to check live).

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 1 player of this specialisation (rating 2618 to 2618). 2v2: 3 players (rating 2713 to 2817). Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** none, only 4 players in all (noted, not changed)
- **BT-324 result:** open (pooled 4 players)
- **Set now:** `PremadeSpecLinkPvp.5.1 = 111111`, `PremadeSpecGlyphPvp.5.1 = 42417,102153,42411,87276,79538,79514`

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Dominate Mind 0% / 100% / 3 of 4, Psyfiend 100% / 0% / 1 of 4 | Psyfiend | Psyfiend | in the ladder, split; 2v2 prefers Dominate Mind (3 of 3) |
| 2 (30) | Angelic Feather 100% / 100% / 4 of 4 | Angelic Feather | Angelic Feather | matches |
| 3 (45) | From Darkness, Comes Light 100% / 100% / 4 of 4 | Mindbender | Mindbender | differs, not changed; 2v2 prefers From Darkness, Comes Light (3 of 3) |
| 4 (60) | Spectral Guise 100% / 100% / 4 of 4 | Spectral Guise | Spectral Guise | matches |
| 5 (75) | Twist of Fate 100% / 100% / 4 of 4 | Power Infusion | Power Infusion | differs, not changed; 2v2 prefers Twist of Fate (3 of 3) |
| 6 (90) | Divine Star 100% / 100% / 4 of 4 | Divine Star | Divine Star | matches |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Renew (42411) | 100% | 100% | 4 of 4 | yes | yes |
| major | Prayer of Mending (42417) | 100% | 100% | 4 of 4 | yes | yes |
| major | Shadow Word: Death (79514) | 0% | 100% | 3 of 4 | yes | yes |
| major | Inner Fire (42402) | 100% | 0% | 1 of 4 | no | no |
| minor | Shackle Undead (43373) | 0% | 33% | 1 of 4 | no | no |
| minor | Holy Resurrection (87276) | 0% | 33% | 1 of 4 | yes | yes |
| minor | Angels (102153) | 100% | 0% | 1 of 4 | yes | yes |
| minor | the Heavens (79538) | 0% | 0% | 0 of 4 | yes | yes |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **Open (BT-451):** four players pooled (1 in 3v3, 3 in 2v2), one short of the minimum. The three 2v2 priests agree on three tiers the set does not hold: tier 15 Dominate Mind (set: Psyfiend), tier 45 From Darkness, Comes Light (set: Mindbender) and tier 75 Twist of Fate (set: Power Infusion); tiers 30, 60 and 90 match. Glyphs: Prayer of Mending, Renew and Shadow Word: Death (3 of 3 in 2v2) are held. One more holy priest on the ladder would settle the three tiers.
