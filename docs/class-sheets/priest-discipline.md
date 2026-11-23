# Priest: Discipline (healer)

- **Status:** draft, checked against 5.4.8 (2026-11-10). Config items 1 and 2 and rotation items 3, 5, 6 and 7 of the gap
  list are implemented on `feat/class-sheet-bear-disc-protect` (BT-128, compile pending, owner tests them in his next
  dungeon run); item 4 (the "protect party member" value, BT-129) and item 11 (the dispel timing key, BT-130) are repaired in the
  same branch; the rest is open. Every "bots today" claim
  below was checked by reading the call sites unless it says "to check", and describes the code at commit 27f004b (the holy
  priest fixes of 2026-11-10 are in it; they touch the Holy strategy and the shared Purify triggers), before that change.
- **Sources:** Method, "Discipline Priest Class Guide, MoP Classic (Phase 1)" (Devorta, 23 July 2025, level 90 raids
  and dungeons), read 2026-11-10; wowtbc.gg, "Discipline Priest PvE Guide - MoP Classic" (no patch line, the page the
  module's premade comments cite), read 2026-11-10; the server's own data files (Spell.dbc with its tooltip text,
  SpellLevels.dbc, SpecializationSpells.dbc, SkillLineAbility.dbc, SpellPower.dbc, SpellCooldowns.dbc, SpellMisc.dbc,
  SpellEffect.dbc, SpellAuraOptions.dbc, Talent.dbc, GlyphProperties.dbc, Item-sparse.db2 of build 18414) for every spell
  id, learn level, cost, cooldown, duration, talent tier and glyph. Overgear, Icy Veins and Wowhead are not readable with
  a plain fetch, so there are two guides. Neither has a levelling section, a dispel section, resurrection, buff advice,
  triage thresholds or a dungeon rule for Atonement: those lines are derived (marked "derived").
- **Levels covered:** 15 to 90, with the detail on 15 to 60 (dungeons with five players).

## Which strategy a Discipline priest runs

Read from the factory, not assumed:

- A Discipline priest in a group, which is every dungeon, runs **`HealPriestStrategy`** ("heal",
  `HealPriestStrategy.cpp`, added at `AiFactory.cpp:337-339`). The Holy priest runs `HolyHealPriestStrategy` ("holy
  heal", `AiFactory.cpp:343`), a different class with its own chains; this sheet does not use the Holy sheet's numbers.
- Added on top for every priest: "dps assist" and "cure" (`AiFactory.cpp:350`); for every healer by specialisation
  (`PlayerBotSpec::IsHeal`, `AI/PlayerbotSpec.cpp:146-158` lists Discipline): "save mana" (`AiFactory.cpp:577`, config
  `AiPlayerbot.AutoSaveMana = 1`, threshold 60) and, on maps outside the restricted list, "healer dps" (`AiFactory.cpp:581`).
  The default restricted list (`AiFactory.cpp:75-78`, `conf.dist:2329`) holds the classic, Burning Crusade, Wrath,
  Cataclysm and Mists dungeon map ids (not every id checked one by one), so a healer in those dungeons casts no damage
  spell. `AiFactoryEvaluateHealerDps` (`AiFactory.cpp:104-113`) flips it when a grouped bot changes map.
- Without a group (`AiFactory.cpp:595-600`) a Discipline bot also gets "holy dps" (`HolyPriestStrategy`, a subclass of
  `HealPriestStrategy`), "shadow debuff", "shadow aoe" and "boost": a solo bot smites and shadowfiends. Its Chakra and
  Lightwell edges are no-ops for Discipline (the spells are Holy only).
- How an action is chosen (read, `Engine.cpp:150-260`): every active trigger pushes its actions with their numbers; the
  engine runs the highest-numbered action that is useful and possible, once per tick. An action that is "IMPOSSIBLE"
  (for example Power Word: Shield on a member who carries Weakened Soul, see below) is dropped and the next one in the
  queue runs in the same tick; a failed cast pushes the node's alternative. Heal numbers are 10 to 19 (light), 20 to 29
  (medium), 30 to 39 (critical); dispels 30 to 41; mana potion and Pain Suppression 90.
- The heal target is the group member with the lowest health percent, plus a distance penalty
  (`PartyMemberToHealValue.cpp:33-128`); a member at 65% or more who already has a heal incoming is skipped in a
  non-raid group. The tank is not preferred and nothing weighs a member by role. The trigger bands are critical below 25,
  low below 45, medium below 65, almost full below 85 (`conf.dist:667-670`; the code default for critical is 20). The
  bands overlap: a member at 20% turns on all four triggers at once, so the bot's list for him is the union, sorted by
  number.

## Talents

Tiers and columns are from Talent.dbc (the guides show builds as images or option lists). "Bots today" is
`AiPlayerbot.PremadeSpecLink.5.0 = 001001` (one digit per tier, the column counted from 0; the config comment names
the same six talents).

| Tier (level) | Pick | Alternative and when | Spell id | Bots today |
|--------------|------|----------------------|----------|------------|
| 1 (15) | Void Tendrils (Method default) | Psyfiend; Dominate Mind. No Discipline edge casts any of the three (`VoidTendrilsTargetCloseTrigger` is in the shadow set), so the pick changes nothing | 108920 | Void Tendrils, same |
| 2 (30) | Body and Soul (Method default) | Angelic Feather (Method: better on paper, needs placing); Phantasm. Body and Soul speeds the target of every Power Word: Shield, and the bot casts it constantly | 64129 | Body and Soul, same |
| 3 (45) | **From Darkness, Comes Light** (derived, see below) | Mindbender (Method default, wowtbc lists it). Method: From Darkness, Comes Light when extra mana is not needed | 109186 (proc 114255) | **Mindbender (123040), differs** |
| 4 (60) | Desperate Prayer (Method: usually over Angelic Bulwark) | Angelic Bulwark (passive shield below 50% health); Spectral Guise (wowtbc only). The bot casts Desperate Prayer at critical health (`GenericPriestStrategy.cpp:54-55`) | 19236 | Desperate Prayer, same |
| 5 (75) | Twist of Fate (Method: better; dungeon: gains more) | Power Infusion (Method: for a strong cooldown; wowtbc lists it); Divine Insight (wowtbc lists it). Passive pick, no edge either way | 109142 | Twist of Fate, same |
| 6 (90) | **Halo** (derived, see below) | Divine Star (Method: the safer dungeon pick, raid in a line); Cascade | 120517 | **Divine Star (110744), differs: no Discipline edge ever casts it** |

Why From Darkness, Comes Light at 45 (derived): Method names Mindbender the default and calls From Darkness, Comes Light
the pick when mana is fine; wowtbc names none. The talent data says From Darkness, Comes Light gives a chance, on Smite or
any healing spell, that the next Flash Heal is instant and free (two charges), and the bot has an edge for exactly that
proc (`HealPriestStrategy.cpp:102-103`, trigger on aura 114255, `PriestTriggers.cpp:SurgeOfLightProcTrigger`). Mindbender
is its own spell (123040, 60 s cooldown, 15 s; the server script is shared with Shadowfiend) and the bot's mana edge
casts "shadowfiend" by name (`GenericPriestStrategy.cpp:65-69`); whether that name still resolves to a known spell
after Mindbender is learned, or reaches Mindbender, is not verified (to check live). Until that is checked, a Mindbender pick may leave the bot with
no mana cooldown from level 45 on.

Why Halo at 90 (derived): both guides accept all three. The dungeon note of Method prefers Divine Star because Halo does
not reach full strength within 25 yards, but of the three only Halo has a Discipline edge (`GenericPriestStrategy.cpp:52-53`,
three members below 65% in a full party); Divine Star is cast by nobody and Cascade only by the shadow set. A talent the
rotation never presses is worth nothing. Halo costs 13.5% of base mana and has a 40 s cooldown, so it stays an area button.

Why Twist of Fate stays: Method names it better, more so in dungeons (more targets under 35%), and it is passive.

Which guide was richer: Method names a default for every tier and gives reasons, including a separate dungeon note;
wowtbc lists three options per tier with no marked pick. They agree on every option except that wowtbc also lists Phantasm,
Solace and Insanity, Spectral Guise and Divine Insight as options. The premade comment cites wowtbc, which names no picks;
the premade's tier 3 and tier 6 are therefore not "wowtbc's picks".

## Glyphs

Majors unlock at 25, 50 and 75, minors in the same pairs (`BotFactory.cpp:1338-1345`): a configured glyph goes into the first
free slot of its type, in config order. Effects are read from GlyphProperties and Spell.dbc (SpellEffect values in
brackets). "Bots today" is `PremadeSpecGlyph.5.0 = 45756,43371,42406,87276,79538,42416`: majors Penance, Holy Fire, Smite;
minors Borrowed Time, Holy Resurrection, the Heavens.

| Type | Glyph | Item id | Effect (5.4.8 data) and why | Bots today |
|------|-------|--------:|-----------------------------|------------|
| Major | Power Word: Shield | 42408 | **Derived, not in either guide:** 20% of the absorb also heals; the bot casts a shield in every health band | not in the build |
| Major | Weakened Soul | 42407 | Method: frequent shielding. Weakened Soul lasts 2 s less (15 s to 13 s) | not in the build |
| Major | Inner Sanctum | 42400 | both guides, by damage type. 6% less spell damage taken while Inner Fire is up; the bot always keeps Inner Fire up, so it is a passive gain | not in the build |
| Major | Inner Fire | 42402 | both guides, the physical alternative: +50% to the armor Inner Fire gives | not in the build |
| Major | Fade | 42398 | Method first for a personal cooldown, wowtbc too: Fade also cuts damage taken by 10%; the bot casts Fade only at medium threat (`GenericPriestStrategy.cpp:33`) | not in the build |
| Major | Penance | 45756 | both guides. **Derived: of no use to a bot.** +20% mana cost, and Penance can be cast while moving; the module refuses any channel or cast with a cast time while the bot is moving (`AI/PlayerbotAI.cpp:3828-3830`), so the glyph only raises the cost | **first major, worn from 25, a net loss** |
| Major | Holy Fire | 42406 | both guides. +10 yards of range on Holy Fire, Smite and Solace: damage spells only, and a dungeon healer casts none | second major, worn from 50, no effect in dungeons |
| Major | Smite | 42416 | both guides. +20% Smite damage on a target with Holy Fire; damage only | third major, worn from 75, no effect in dungeons |
| Minor | Borrowed Time, Holy Resurrection, the Heavens | 43371, 87276, 79538 | all cosmetic (Method: none is game-changing; wowtbc lists no minors) | yes, all three |

Order of majors proposed: Power Word: Shield (25), Weakened Soul (50), Inner Sanctum (75). Each is usable from the level
it is worn.

## Stat order

Method: Critical Strike, then Spirit, then Mastery, then Haste (secondary stats, level 90, Critical works well with
Atonement); wowtbc gives none. For levels 15 to 79 (derived: Mastery: Shield Discipline is learned at level 80, so
Mastery rating does nothing before it): **Intellect, then Spirit, then Critical Strike, then Haste, and Mastery last**;
from 80 follow Method (Intellect, Critical Strike, Spirit, Mastery, Haste). Spirit matters because Meditation (level 10)
lets half of Spirit regeneration run in combat, and Critical Strike feeds Divine Aegis (level 24: a critical heal makes a
shield). For loot rolls and quest rewards: cloth with Intellect and Spirit; a piece with Spirit beats the same piece without;
Mastery is worth nothing below 80.

## What the specialisation knows, by level (5.4.8 data)

Spec spells are from SpecializationSpells.dbc (spec 256); class spells (every priest) are the Priest skill line rows
(804) in SkillLineAbility.dbc with their level from SpellLevels.dbc. Cost is percent of base mana (SpellPower.dbc),
cast time from SpellMisc and SpellCastTimes, durations from SpellDuration, values from SpellEffect.

| Level | Spell (id), cost, cast, cooldown |
|------:|------------|
| 1 | Smite (585, 2.7%, 1.5 s) |
| 3 | Shadow Word: Pain (589, 4.4%) |
| 5 | Power Word: Shield (17, 6.1%, instant, 15 s absorb, leaves Weakened Soul for 15 s; refused on a target that has it) |
| 7 | Flash Heal (2061, 5.9%, 1.5 s) |
| 9 | Inner Fire (588) |
| 10 | Penance (47540, 3.1%, channel 2 s, 9 s cooldown, damages an enemy or heals an ally), Rapture (47536, passive: Power Word: Shield has no cooldown, costs 25% less, and refunds mana when fully absorbed, once per 12 s), Spiritual Healing (87336, passive: Flash Heal, Binding Heal, Renew and Prayer of Mending heal 25% more; Heal, Greater Heal, Penance and Prayer of Healing do not), Meditation (95860, passive: 50% of Spirit regeneration in combat) |
| 12 | Psychic Scream (8122) |
| 16 | Divine Fury (122098, passive) |
| 18 | Holy Fire (14914, 1.8%, instant, 10 s cooldown), Resurrection (2006, 4.0%, 10 s cast, out of combat only) |
| 22 | Purify (527, 2.6%, 8 s cooldown, removes Magic and Disease from a friend), Power Word: Fortitude (21562, 1.0%, buffs the whole group) |
| 24 | Fade (586, 3.5%, 30 s cooldown), Divine Aegis (47515, passive) |
| 26 | Renew (139, 2.6%, instant, 12 s), Dispel Magic (528, hostile target only) |
| 28 | Heal (2050, 1.9%, 2.5 s), Spirit Shell (109964, no cost, 60 s cooldown, 10 s: Heal, Flash Heal, Greater Heal and Prayer of Healing make 15 s absorbs instead of healing) |
| 30 | Focused Will (45243, passive), talent tier 2 |
| 32 | Shackle Undead (9484) |
| 34 | Greater Heal (2060, 5.9%, 2.5 s), Levitate (1706) |
| 36 | Inner Focus (89485, 45 s cooldown: next Flash Heal, Greater Heal or Prayer of Healing costs 25% less and crits for sure; also immune to interrupts) |
| 38 | Atonement (81749, passive: Smite, Holy Fire and Penance damage heals a nearby low-health friend for 90% of the damage dealt, halved if that friend is the priest) |
| 42 | Shadowfiend (34433, 3 minute cooldown, 12 s, returns mana) |
| 44 | Evangelism (81662, passive: damage with Penance, Smite or Holy Fire gives a stack, up to 5, 20 s; each stack adds 4% damage and healing and cuts 6% mana cost of those spells) |
| 45 | Grace (47517, passive: Flash Heal, Greater Heal, Heal and Penance put Grace on the target, 3 stacks, 15 s, each adds 10% to healing the target gets from the priest), talent tier 3 |
| 46 | Prayer of Healing (596, 4.5%, 2.5 s), Shadow Word: Death (32379) |
| 48 | Binding Heal (32546, 5.4%, 1.5 s) |
| 50 | Archangel (81700, no cost, 30 s cooldown: spends Evangelism, 5% healing per stack for 18 s) |
| 52 | Strength of Soul (89488, passive: Heal, Greater Heal and Flash Heal shorten the target's Weakened Soul by 2 s) |
| 54 | Fear Ward (6346, 0.9%, 3 minute cooldown) |
| 58 | Pain Suppression (33206, 1.6%, instant, 3 minute cooldown, 8 s, 40% less damage taken, castable while stunned) |
| 60 | talent tier 4 |
| 62 | Borrowed Time (52798, passive: after Power Word: Shield the next spell within 6 s casts 15% faster), Holy Nova (23455) |
| 66 | Hymn of Hope (64901, 6 minute cooldown, 8 s channel) |
| 68 | Prayer of Mending (33076, 3.5%, instant, 10 s cooldown) |
| 70 | Power Word: Barrier (62618, 6.3%, 3 minute cooldown, 10 s at a place, 25% less damage to friends inside) |
| 72 | Mass Dispel (32375, 13%, 1.5 s cast) |
| 75 | talent tier 5 |
| 78 | Train of Thought (92297, passive: Greater Heal cuts Inner Focus cooldown, Smite cuts Penance cooldown) |
| 80 | Mastery: Shield Discipline (77484), Inner Will (73413) |
| 84 | Leap of Faith (73325) |
| 87 | Void Shift (108968, 5 minute cooldown: swap health percent with a party member) |
| 90 | talent tier 6 |

A Discipline priest does not know Circle of Healing, Divine Hymn, Guardian Spirit, Lightwell or the Chakras (Holy), and has
no Mind Blast (shadow). Power Word: Shield is the only spell on a hard shared rule: Weakened Soul (15 s) forbids a second
shield on the same target, so the bot can shield each member at most once per 15 s, and Rapture removes the cooldown, so
Weakened Soul is the only limit.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only).

### Levels 15 to 29
Known: Power Word: Shield, Flash Heal, Penance, Inner Fire, Holy Fire, Resurrection (18), Purify and Fortitude (22), Fade
(24), Divine Aegis (24), Renew (26), Heal (28), Spirit Shell (28). The cheap strong buttons are Penance (3.1%) and the
shield with Rapture (about 4.6%); Flash Heal at 5.9% is the emergency button, so keep fights short and drink after each pull.

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | out of combat, Inner Fire missing | Inner Fire (588, 9) | yes: `PriestNonCombatStrategy.cpp:28-29` (10) |
| 2 | core | out of combat, a member lacks it, from 22 | Power Word: Fortitude (21562, 22) | yes: `PriestNonCombatStrategy.cpp:83-85`, set added by "buff" (`AiFactory.cpp:797`) |
| 3 | core | a member is dead, out of combat, from 18 | Resurrection (2006, 18) | yes: `PriestNonCombatStrategy.cpp:41-43` (40) |
| 4 | core | a member below 25% | Power Word: Shield (5) if he has no Weakened Soul, then Penance (10), then Flash Heal (7) | yes: `HealPriestStrategy.cpp:51-57` (35, 34, 32); the shield is refused by the server on a Weakened Soul carrier and the next line runs the same tick (`Engine.cpp:241-246`, `AI/PlayerbotAI.cpp:3850`, aura restriction 6788 on spell 17, data) |
| 5 | core | a member below 45% or 65% | Penance (10), then Flash Heal (7); from 28 Heal (28) for the small deficits | partly: Penance and Flash Heal at `HealPriestStrategy.cpp:59-73` (22, 20; 16, 15), preceded by the shield (24, 19); **Heal is never chosen** (see line 11) |
| 6 | core | the pull starts, tank first | Power Word: Shield on the tank (5), Renew on the tank (derived: 2.6%, Spiritual Healing adds 25%, both guides keep upkeep on the tank) from 26 | **gap:** no tank-aimed edge; the shield goes to the lowest member below 85% (`HealPriestStrategy.cpp:75-83`, 13), Renew is cast in that band only after the shield and Prayer of Mending (11) |
| 7 | good | a harmful Magic or Disease effect on a member, from 22, nobody below 45% | Purify (527, 22) | yes: edges `GenericPriestStrategy.cpp:94-103` (41, 40, 31, 30), silent below 45% since the holy priest fixes (`PriestTriggers.cpp:21-24, 56-70`) |
| 8 | good | mana below 50%, nobody of the group in combat | drink | yes: `UseFoodStrategy.cpp:49`, `GenericTriggers.cpp:50-` (`DungeonDrinkMana` 50, `conf.dist:2269`) |
| 9 | good | a mob is on the healer | Fade (586, 24) | partly: Fade at "medium threat" (`GenericPriestStrategy.cpp:33`, 55), above every heal |
| 10 | good | Spirit Shell before a big pull on the tank, from 28 (derived: its heals become shields, so they cannot overheal) | Spirit Shell (109964, 28) | **missing:** no action for it (expert) |
| 11 | expert | nobody hurt in a dungeon | nothing (the rule for damage is under "Atonement") | yes: healer dps is off on dungeon maps (`AiFactory.cpp:581`); the default action "shoot" (`HealPriestStrategy.cpp:16-19`) is a dead action in 5.4.8 (BT-038), to check that it fails quietly |

### Levels 30 to 44
As above, plus Greater Heal (34), Inner Focus (36), Atonement (38), Shadowfiend (42), Evangelism (44). Talent tier 2 (Body and Soul).

| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 12 | core | tank or member below 45%, large deficit (derived from Method's single target rule: Greater Heal for big deficits, Heal for small ones) | Greater Heal (34), Heal (28) | **missing:** neither has an edge in the Discipline set; both appear only as the alternatives of Flash Heal and Heal is the alternative of an alternative (`GenericPriestStrategyActionNodeFactory.h`, nodes "flash heal on party" and "greater heal on party"), reached only when Flash Heal fails or is impossible |
| 13 | core | mana below 60%, a member between 25% and 65% | Penance, Greater Heal | **gap, a stall:** the save-mana rule (`ConserveManaStrategy.cpp:14-56`, threshold 60) blocks Flash Heal for every non-tank member who is not below 25% and for the tank at 45% or more, and Greater Heal has no edge: at 60% mana and below the Discipline priest has only Power Word: Shield, Penance and Prayer of Mending for such a member |
| 14 | good | a member below 25% and Pain Suppression is up, from 58 (see Cooldowns) | Pain Suppression | see the cooldown section |
| 15 | good | mana below 40%, big heal coming | Inner Focus (36), then Greater Heal | partly: cast at "medium mana" with Shadowfiend (`GenericPriestStrategy.cpp:65-69`, 21), with no Greater Heal behind it |
| 16 | core | mana below 40%, from 42 | Shadowfiend (34433, 42), mana potion | partly: `GenericPriestStrategy.cpp:65-69` (22) casts on "current target" (`PriestActions.h:247`), so a healer with no target casts nothing (to check live); potion at 90 (`UsePotionsStrategy.cpp:10`) |
| 17 | good | the bot is the target of an attacker | Power Word: Shield on self | yes: `GenericPriestStrategy.cpp:38-39` (21), `:59-63`; with Rapture (25% cheaper, mana back when the shield is fully absorbed) this is cheap for Discipline |
| 18 | good | Atonement rule below | Smite, Holy Fire, Penance on the tank's target | **missing in dungeons** (healer dps is off there) |

### Levels 45 to 59
As above, plus Grace (45), Prayer of Healing (46), Binding Heal (48), Archangel (50), Strength of Soul (52), Fear Ward (54), Pain Suppression (58). Talent tier 3 (From Darkness, Comes Light).

| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 19 | core | three or more members below 85% (derived: Prayer of Healing reaches the party; Method's area rule) | Prayer of Healing (46) | partly: `HealPriestStrategy.cpp:43-49` (34); **the trigger needs five group members near** (`HealthTriggers.cpp:29-40`), so a party of two to four has no area heal at all (Prayer of Mending 36, Halo 33 and Divine Hymn go with it) |
| 20 | good | the tank, whenever Grace is under 3 stacks | Heal (28) or Penance (10), cheap single target heals (derived: Grace is put by Flash Heal, Greater Heal, Heal and Penance and adds 10% per stack; both guides keep 3 stacks on the tank) | **missing:** nothing tracks Grace or aims a heal at the tank (expert) |
| 21 | good | the bot and a member both hurt | Binding Heal (32546, 48) | yes: `HealPriestStrategy.cpp:106-107` (25), trigger `PriestTriggers.cpp:BindingHealTrigger` |
| 22 | good | a fear caster, Fear Ward on the tank before the pull, from 54 | Fear Ward (6346, 54) | yes: `GenericPriestStrategy.cpp:35-36` (23) |
| 23 | good | a member below 25%, tank or not, from 58 | Pain Suppression (33206, 58) | **only on the bot itself:** `HealPriestStrategy.cpp:109-110` (91); the edge for others (`:111-113`, "protect party member") never fires because the value it reads returns nothing at its first line (`PartyMemberToHealValue.cpp:141-143`) |
| 24 | expert | five Evangelism stacks, damage incoming | Archangel (81700, 50) | **missing:** no action, and nothing builds Evangelism in a dungeon (healer dps off) |
| 25 | core | save mana below 60% | skip top-ups | yes (shared): `ConserveManaStrategy.cpp:14-56`, `AiFactory.cpp:577`; it also blocks line 12's only available slow heal, see line 13 |

### Levels 60 to 74
As above, plus tier 4 (Desperate Prayer), Borrowed Time (62), Hymn of Hope (66), Prayer of Mending (68), Power Word: Barrier (70), Mass Dispel (72).

| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 26 | core | pull starts | Prayer of Mending (33076, 68) on the tank, on cooldown (Method) | partly: cast on the lowest member in every chain (`HealPriestStrategy.cpp:36-83`: 28, 36, 33, 23, 17, 12), **not on the tank**, and the group lines carry the five-member gate |
| 27 | good | the bot below 25% | Desperate Prayer (19236, talent 60) | yes: `GenericPriestStrategy.cpp:54-55` (25) |
| 28 | good | mana below 15% | Hymn of Hope (64901, 66) | partly: `GenericPriestStrategy.cpp:71-72` (20); at 15% a channel the group must wait out is late (derived), to check |
| 29 | expert | a big pack on the tank, three members below 85% | Power Word: Barrier (62618, 70) at the tank's feet | **missing:** no action (expert) |

### Levels 75 to 89 and 90
| # | Tier | Condition | Spell | Bots today |
|--:|------|-----------|-------|------------|
| 30 | good | three members below 65% in a full party | Halo (90 talent) | yes if the talent is Halo: `GenericPriestStrategy.cpp:52-53` (33); the premade takes Divine Star, which nothing casts |
| 31 | expert | the bot at high health and a member near death (Method) | Void Shift (108968, 87) | missing: no edge |
| 32 | expert | harmful Magic on several members at once | Mass Dispel (32375, 72) | missing: the action exists (`PriestActions.h:27`), no Discipline edge |

## Atonement and when a healer may deal damage in a dungeon

Both guides build the spec on Atonement: in light damage Holy Fire, Penance and Smite take the place of direct heals, and
Evangelism and Archangel turn the same damage into more healing. The bots today use none of it in a dungeon: the factory
turns "healer dps" off on dungeon maps (`AiFactory.cpp:581`), the damage list lives only in that strategy
(`GenericPriestStrategy.cpp:119-139`: Shadow Word: Pain 5.5, Penance 5.45, Holy Fire 5.4, Smite 5.3, Mind Blast, Shoot),
and nothing casts Archangel or tracks Evangelism. In the open world a grouped Discipline bot does cast them when every
member is at 85% or more and mana is above the threshold (`GenericTriggers.cpp:HealerShouldAttackTrigger`), so Atonement
heals happen there by accident. A bot never casts Shadow Word: Pain to heal: it makes no Atonement heal and no Evangelism
stack (derived from the Evangelism and Atonement tooltips).

Derived rule for a dungeon (the guides give none; Atonement is learned at 38, so none below it):

1. Only from level 38, only in a group, and only at the tank's current target (never an unengaged enemy, so the healer
   never pulls).
2. Only while no member is below 65% (below it the direct heals of the lists above come first) and mana is 60% or more
   (the save-mana threshold).
3. Order Holy Fire (instant, 1.8%), Penance on the enemy (3.1%, only if no member is below 85%, as it is the best direct
   heal), Smite (1.5 s, 2.7%). No Shadow Word: Pain.
4. Why it can pay: Smite costs 2.7% base mana and heals a nearby hurt friend for 90% of its damage, against 5.9% for
   Flash Heal; Evangelism then cuts the cost of those three spells by 6% a stack.
5. Archangel (expert, from 50) when five Evangelism stacks and a member below 65%.

The owner's restriction exists to keep healers off the enemy in dungeons; this rule keeps that for every case where a
heal is needed and only adds the cheap healing-by-damage when nobody is hurt badly. It needs an Atonement-only variant of
"healer dps" for dungeon maps, not a removal of the restriction.

## Cooldowns and defensives

- **Emergency buttons by level** (derived order, instant first): Power Word: Shield (5), Penance (10), Flash Heal (7),
  Pain Suppression (58), Desperate Prayer (60 talent, self). Bots: shield, Penance and Flash Heal exist in that order
  (`HealPriestStrategy.cpp:51-57`); Pain Suppression exists for the bot itself only (see line 23).
- **Pain Suppression** (58): 1.6% mana, 3 minutes, 40% less damage for 8 s, instant: the first defensive both guides name
  (Method puts it seventh on the heavy damage list, wowtbc first in its cooldown list). The bot's emergency edge for a
  group member never fires, so a tank at 10% gets shield, Penance and Flash Heal only.
- **Spirit Shell** (28): both guides list it as a raid absorb cooldown; here it exists from 28 with a 60 s cooldown and a
  10 s window. Method: use it, then spam Prayer of Healing, on heavy damage. Not cast by the bots.
- **Power Word: Barrier** (70), **Hymn of Hope** (66), **Mindbender or Shadowfiend** (45 or 42), **Void Shift** (87): see
  the lines above. The guides rank the raid cooldowns Pain Suppression, Barrier, Spirit Shell, Void Shift, Hymn of Hope,
  Mindbender (wowtbc) and Hymn of Hope, Mindbender, Archangel, Spirit Shell, Barrier, Pain Suppression, Void Shift (Method,
  heavy damage); the orders differ because wowtbc ranks defence first and Method ranks mana and throughput first.
- **Power Infusion** is not a talent pick here; the bot casts it only through the solo "boost" strategy
  (`GenericPriestStrategy.cpp:106-111`).
- **Fade** (24): at medium threat only (`GenericPriestStrategy.cpp:33`), and only in a group against non-players
  (`PriestActions.cpp:11-23`).

## Healer triage and mana

- **Who first (derived from both guides' logic and the thresholds):** 1 a member below 25%: Pain Suppression (58) if
  it is the tank or if a hit could kill him, an instant first (Power Word: Shield unless Weakened Soul, then Penance),
  then Flash Heal; 2 the tank's upkeep: Power Word: Shield whenever Weakened Soul has gone (15 s), Renew (26), Grace with
  Heal or Penance (45), Fear Ward before a fear caster (54), Prayer of Mending (68); 3 anyone below 45%: Penance, then
  Greater Heal (34) or Flash Heal; 4 below 65%: Penance, Heal (28), Renew; 5 area: Prayer of Healing (46), Spirit Shell
  before it for a spike (28), Halo with the talent (90), Barrier at the tank's feet (70); 6 light damage 65 to 85%: the
  Atonement rule above, else Renew and Heal on the tank only. Today: lowest percent first for everyone, no role; the
  shield first in every chain; no Greater Heal or Heal; no Pain Suppression on others.
- **Single target ladder (Method, kept):** Penance on cooldown, Greater Heal for big or Heal for small deficits, Flash Heal
  as the emergency (both guides put Flash Heal last). **Area ladder:** Spirit Shell, then Prayer of Healing (Method,
  heavy damage), Halo or Divine Star, Prayer of Mending. wowtbc has one merged list and no thresholds; Method is richer
  here (it splits low damage, which is Atonement, from heavy damage).
- **Where the bots differ from the ladder:** the shield and Prayer of Mending lead every chain at `HealPriestStrategy.cpp:36-83`
  (shield 35, 24, 19, 13), Penance follows, Flash Heal is last and is the only slow heal; the guides put Flash Heal last
  too, but they have Greater Heal and Prayer of Healing ahead of it.
- **Dispels:** Purify (22) takes Magic and Disease from a friend (Method: it cleanses more and has a cooldown, 8 s in the
  data); a priest has no poison or curse dispel. Dispel Magic (26) is hostile-target only and no edge casts it. The edges run
  at 41 (self, Magic), 40 (member, Magic), 31 and 30 (Disease), above every heal, and since the holy priest fixes stay
  silent while any member is below 45% (`PriestTriggers.cpp:21-24, 56-70`). `HasAuraToDispel` accepts any harmful aura of
  the type whose remaining time is at least `DispelAuraDuration` (`AI/PlayerbotAI.cpp:3431-3433`); the shipped config
  sets it to 700 (0.7 s, `conf.dist:601`), the code default is 7000, so almost any harmful Magic aura qualifies (to check
  which the live config holds). Derived rule: dispel only when nobody is below 45% (done).
- **Shield and Weakened Soul rules (derived):** one shield per member per 15 s; the tank first, others only in an
  emergency (6.1% base, about 4.6% with Rapture; Rapture's mana refund when a shield is fully absorbed is once per 12 s). Weakened Soul is a hard refusal of the server, so the bot loses nothing by trying (line 4). Today the shield
  goes to the lowest member below 85%, which in a party is usually a damage dealer, and the save-mana rule never blocks it
  (`PriestActions.h:44`, efficiency very high).
- **Mana, in a fight:** below 60% the shared save-mana rule blocks Flash Heal on members at 25% and above (the tank at 45% and
  above) and Prayer of Healing and Binding Heal above 65% (`ConserveManaStrategy.cpp:14-56`); with no Greater Heal edge,
  Discipline has Penance, the shield and Prayer of Mending left (line 13). Shadowfiend and Inner Focus at 40%
  (`GenericPriestStrategy.cpp:65-69`; Inner Focus is Discipline's own spell and is cast without a heal behind it); a mana
  potion at 40% (90, `UsePotionsStrategy.cpp:10`); Hymn of Hope at 15%. Method adds Hymn of Hope before a spike and Mindbender
  early; neither is in the bots. Out of a fight: drink below 50% in a dungeon (`DungeonDrinkMana`), 15% elsewhere.
  Cost table (percent of base mana, data): Fortitude 1.0, Fear Ward 0.9, Pain Suppression 1.6, Holy Fire 1.8, Heal 1.9,
  Renew 2.6, Purify 2.6, Smite 2.7, Penance 3.1, Prayer of Mending 3.5, Prayer of Healing 4.5, Binding Heal 5.4, Flash
  Heal 5.9, Greater Heal 5.9, Power Word: Shield 6.1, Barrier 6.3, Halo 13.5. Derived: Penance and Heal first for most
  deficits, Flash Heal only for emergencies.
- **Out of combat top-ups:** Flash Heal at critical health, then Renew, Penance, Greater Heal on hurt members
  (`PriestNonCombatStrategy.cpp:48-71`). Derived: Renew only, then drink; Flash Heal and Penance before a drink spend mana the
  drink must give back.
- **Buffs:** Inner Fire (9) always; Power Word: Fortitude (22) on the group out of combat, as above. Both guides name
  Inner Fire (Method: maintain, Grace on the tank). Inner Will (80) is never used and does not need to be.
- **Resurrection:** Resurrection (18) out of combat only, 10 s cast, `PriestNonCombatStrategy.cpp:41-43`; a priest has no
  combat resurrection in 5.4.8.
- **Nothing to heal:** in a dungeon nothing (healer dps is off on dungeon maps; the Atonement rule above is the proposed
  exception); in the open world "healer dps" casts Shadow Word: Pain, Penance, Holy Fire, Smite when every member is at
  85% or more and mana is above a threshold (`GenericTriggers.cpp:HealerShouldAttackTrigger`, `GenericPriestStrategy.cpp:119-139`).

## Differences from the guide (5.4.8 against MoP Classic)

- Both guides are level 90. Heal is level 28, Greater Heal 34, Renew 26, Prayer of Healing 46, Binding Heal 48,
  Pain Suppression 58, Prayer of Mending 68, Barrier 70. Below those the bot must make do with what it has.
- Spirit Shell is a baseline Discipline spell from level 28 with a 60 s cooldown and a 10 s window, not a raid-only
  cooldown; Atonement is learned at 38, Evangelism 44, Grace 45, Archangel 50.
- The guides' Penance glyph is the first pick for a player because it allows casting while moving; the bots never cast
  while moving (`AI/PlayerbotAI.cpp:3828-3830`), so for them it is only a cost.
- Grace is +10% healing per stack, 3 stacks, 15 s (data); Method and wowtbc agree on 3 stacks.
- Method's "Hymn of Hope baseline for all specs" holds: it is level 66 for every priest.
- Method's "Power Infusion now affects only the priest" holds: 5.4.8 data gives haste, damage and a mana discount to the
  caster.
- Method and wowtbc rank Stat order for level 90. Mastery: Shield Discipline is level 80: the Critical-first order holds from
  there, not below.
- Neither guide covers Resurrection (18), Fortitude (22), Fear Ward (54), Binding Heal (48), Heal (28), nor the Power Word:
  Shield glyph; those lines are derived.
- Desperate Prayer shows a learn level of 18 in SpellLevels.dbc but is a tier 4 talent in Talent.dbc, so it is not available
  to a level 18 priest without the talent.
- Divine Star and Cascade exist as talents; no Discipline edge casts either. Power Word: Solace needs the Solace and Insanity
  talent.
- Mass Resurrection is a guild perk and not a priest spell.

## Gap list (ordered by value for levels 15 to 60 in dungeons)

Config:

1. **Fixed (compile pending): premade `000002`.** Premade tier 3: Mindbender is not shown to work by the bot's "shadowfiend" edge; From Darkness, Comes Light has a working
   edge (`HealPriestStrategy.cpp:102-103`) and Method accepts it. Premade tier 6: Divine Star is never cast, Halo has an edge.
2. **Fixed (compile pending): glyphs `42408,43371,42407,87276,79538,42400`.** Premade glyphs: the Penance glyph is a net loss for a bot that never casts while moving; Holy Fire and Smite are damage
   glyphs in dungeons where the healer does no damage. Replace by Power Word: Shield, Weakened Soul, Inner Sanctum.

Rotation:

3. **Fixed (compile pending).** No Greater Heal and no Heal edge in the Discipline strategy (core): `HealPriestStrategy.cpp:51-83` has Flash Heal as the
   only slow heal, and at 60% mana and below the save-mana rule (`ConserveManaStrategy.cpp:14-56`) leaves Penance, the
   shield and Prayer of Mending for members between 25% and 65% (a stall in long fights; verify live).
4. **Value repaired in BT-129 (compile pending).** Pain Suppression never goes to a group member (good, emergency): `PartyMemberToProtect::Calculate` returns nothing on its
   first line (`PartyMemberToHealValue.cpp:141-143`, since the first import of the module), so "protect party member"
   never fires; the same value feeds the warrior, paladin and monk protect edges.
5. **Fixed (compile pending) in combat; the pull-time shield before combat is still open.** No tank upkeep (good): no shield pre-pull, no Renew, no Grace, no Prayer of Mending on the tank; the reusable
   `RenewOnMainTankTrigger` and `PrayerOfMendingOnMainTankTrigger` of the Holy fixes (`PriestTriggers.h`) fit.
6. **Fixed (compile pending).** The shield leads every chain on the lowest member at every health band (13, 19, 24, 35, `HealPriestStrategy.cpp:51-83`;
   also "power word: shield on not full" 27 and 35) and is never blocked by save mana (good): 6.1% per damage dealer.
7. **Fixed (compile pending).** Group heals need five members near (`HealthTriggers.cpp:29-40`): Prayer of Healing, Halo, group Prayer of Mending, the
   group shield in a party of two to four never fire (good; the Holy priest got `HolyGroupHealTrigger` for it).
8. Atonement and Evangelism unused in dungeons, Archangel, Spirit Shell, Barrier, Void Shift, Mass Dispel missing (good to
   expert; the rule above).
9. Mana: Shadowfiend needs a current target (to check live), Mindbender's cast through "shadowfiend" (to check live), Hymn of
   Hope at 15% is late (good, to check), Inner Focus cast with no heal behind it.
10. Out-of-combat Flash Heal and Penance before drinking (`PriestNonCombatStrategy.cpp:48-71`): Renew instead (good).
11. **Resolved (BT-130, compile pending): the code default is now 700, as the shipped config; the live server read 700 already.** The shipped `DispelAuraDuration` of 700 against the code default of 7000 (to check which is live).
12. The dead default action "shoot" (`HealPriestStrategy.cpp:16-19`) and the solo set that adds the Holy strategy on top of
    "heal" (`AiFactory.cpp:595-600`): harmless, to check live.

## PvP playbook (arena, 5.4.8)

New section, 2026-11-19. Sources: player guides (5.x, patches 5.3 and 5.4: three short PvP videos by two players, one written guide) and community guides for the spec (MoP Classic raid guides, with a few PvP remarks), the 5.x patch notes, the server's own data (`data/dbc/` through `spec_data.py` and short read-only scripts) and the module code read on `main`. Lines say "(guide)" when a player guide gives them, "(adapted)" when they come from the Holy or Shadow priest arena material and were checked against the Discipline spell list, "(derived)" when they follow from the 5.4.8 data and game knowledge. About 45 percent of the section is sourced, 15 percent adapted, 40 percent derived. Where nothing reliable exists the line says GAP.

Where the spec stands: no Discipline priest is in the MoP Classic EU top 100 ladder of 2026-11-18 (96 players in 3v3, 54 in 2v2; the priests there are Holy and Shadow), a community tier list of July 2025 rates it C in 3v3 and C in 2v2, and a Classic guide says Holy outshines Discipline in PvP for the first time. In the original game (5.2) Discipline was a top healer in rogue, mage, priest and hunter comps. A niche pick for bots: the value of a rule is lower than for a ladder spec.

### Main plan (guide, derived)

1. Heal with instants and keep Renew, Penance (9 s) and Prayer of Mending (68, 10 s) rolling; Power Word: Shield (5, 6 s cooldown, Weakened Soul 15 s) is the instant answer to a burst call (guide).
2. Penance first, then Prayer of Mending: Penance can proc Divine Insight (100 percent in 5.4.8: the next Power Word: Shield ignores Weakened Soul) and builds Grace (three stacks, 10 percent each, 15 s) (guide, data).
3. Control for the team: Psyfiend or Dominate Mind, Psychic Scream, Leap of Faith, Mass Dispel and Dispel Magic; the healer's fear and mind control chain on the enemy healer while the partner kills (guide).

### Opener (guide, adapted)

Power Word: Shield and Renew on yourself and the partner at the gates, Penance, then Prayer of Mending (guide). Fortitude and Inner Fire in the prep room; Fear Ward (54, 180 s) on a partner when the enemy has a fear and no offensive dispel (adapted). The partner gets the first Shield: the partner is the kill target (derived). Spirit Shell in the opener: GAP (its absorbs are cut by Battle Fatigue since 5.4).

### Priority in a fight (guide, adapted, derived)

| Priority | Tier | Line |
|---|---|---|
| 1 | core | A target below 25 percent or stunned: Pain Suppression (58, 40 percent less damage for 8 s, castable stunned), Power Word: Shield, Penance, Flash Heal (instant and free with the From Darkness, Comes Light proc) |
| 2 | core | Under an enemy go: Prayer of Mending, Penance, Renew; Power Word: Barrier (70, 25 percent less damage in the circle, no pushback) when two friends stand together |
| 3 | good | Nobody hurt: Renew on both, Prayer of Mending on cooldown, Power Word: Shield on the partner |
| 4 | good | Offense when safe: Penance or Smite or Holy Fire on the enemy for Evangelism and Atonement (heals a low friend for 90 percent of the damage); Archangel (50) spends the stacks for 5 percent healing each |
| 5 | expert | Dispel Magic on a shield, a HoT or a Hand of Freedom before the partner's go; Mass Dispel (72) on an Ice Block or a Divine Shield with the glyph |
| 6 | expert | Inner Focus (36) before a cast that must land: 5 s immune to interrupt, silence and dispel |

Stat order (guides disagree): spirit first in all three guides, then mastery (shield potency) in two, crit second in one, haste 16 to 18 percent in the written guide, crit last there. Take spirit, mastery, then crit or haste by the gear (derived from the three).

### Crowd control (guide, derived)

- Psychic Scream (12, 30 s, 8 s fear, 8 yd): on the third damage dealer while the others lock the healer and the kill target; Psyfiend (45 s, fears the enemy that attacks the priest every 1.5 s for 10 s); Dominate Mind (1.8 s cast, 30 s, own diminishing category): the 2v2 healer lock. The two fears share the fear category with Blind and Fear: the second lasts half.
- Leap of Faith (84, 90 s, no facing requirement since 5.4): pull a partner out of a trap, stun or melee train.
- Shackle Undead (1.5 s cast) on a Gargoyle; Fear Ward as above; Dispel Magic and Purify (8 s) as the dispels.

### Defensives and positioning

Pain Suppression, Power Word: Barrier, Desperate Prayer (18, 120 s, 30 percent), Angelic Bulwark (a 20 percent shield below 30 percent health, 90 s) or Spectral Guise (6 s stealth, to land a fear or to drop a crowd control cast aimed at you), Phantasm (Fade frees from slows), Divine Star (heals while Holy is locked), Fade (24). Stay at a pillar edge with sight to the team (adapted). The Penance glyph (cast while moving) is a player's tool: the module refuses to channel or cast while moving, so it does nothing for a bot. The Glyph of Shadow Word: Death hurts the priest by the damage it would do to an enemy above 20 percent health, which frees it from an incapacitate (guide, data); the guide's macro first cancels the own shield, because the shield would absorb it.

### Matchups and partners

Guides: 2v2 with a frost mage (the mage controls, the priest dispels and fears), with a hunter (fear the healer after a trap), with a subtlety rogue (demanding); 3v3 rogue, mage, priest: dispel shields and HoTs, Scream the third damage dealer, Mass Dispel for the finish, cross crowd control Blind the healer, Polymorph the target, Fear the second damage dealer; hunter, feral, priest: shields and Leap of Faith, fear the healer after a trap. Swaps: Phantasm and Void Tendrils against melee cleave, the Mass Dispel and Shadow Word: Death glyphs against mage and paladin, Fear Ward against a warlock.

### PvP set

Premade today: `PremadeSpecLinkPvp.5.0 = 111111` (Psyfiend, Angelic Feather, Mindbender, Spectral Guise, Power Infusion, Divine Star) and glyphs `42404,43371,42402,87276,79538,42408` (Mass Dispel, Borrowed Time, Inner Fire, Holy Resurrection, the Heavens, Power Word: Shield); it was taken from the Holy and Shadow priest guides. No ladder player holds the spec. The player guides agree on From Darkness, Comes Light (tier 45) and Divine Insight (tier 75), and the module already has an edge for the Surge of Light proc (`HealPriestStrategy.cpp:140`) that idles while the set holds Mindbender.

### Differences from the guides (5.4.8)

- Divine Insight is 100 percent on Penance for Discipline (the 5.3 guides say 40); Atonement heals 90 percent; Penance is 9 s and Power Word: Shield 6 s; Spirit Shell lasts 10 s and Battle Fatigue (60 percent in 5.4.7) cuts absorbs; From Darkness, Comes Light also procs from Penance and Prayer of Mending (5.4.0).
- The Glyph of Mass Dispel only lets the dispel remove undispellable magic (data); one guide says it also shortens the cast; the base cast is 1.5 s in the data (the 5.3 notes say 0.5 s for Discipline: to check). The Fear Ward glyph shortens the cooldown (data), a guide says it makes the ward instant.
- Level gates: Spirit Shell 28, Atonement 38, Archangel 50, Pain Suppression 58, Prayer of Mending 68, Power Word: Barrier 70, Mass Dispel 72, Leap of Faith 84, Void Shift 87.

### Bots today (read on `main`, 2026-11-19)

| Rule | Bots today |
|---|---|
| Power Word: Shield, Prayer of Mending, Penance, Flash Heal on the party by health band | exists (`HealPriestStrategy.cpp:45,59-62,69-72,78-82`); Renew and Prayer of Mending upkeep on the main tank (`:101-108`) |
| Penance before Prayer of Mending, Divine Insight Shield | missing (no read of the Divine Insight aura) |
| From Darkness, Comes Light proc to an instant Flash Heal | exists (`HealPriestStrategy.cpp:140`); idle with the set's talent |
| Pain Suppression at critical health or on a tank in danger | exists (`HealPriestStrategy.cpp:147-153`); not tied to a stun or an enemy go |
| Power Word: Barrier | missing |
| Psychic Scream on a melee player in the face | partly (`GenericPriestStrategy.cpp:48-51`; not by role, not the third damage dealer) |
| Psyfiend, Spectral Guise, Angelic Feather, Divine Star | partly (talent policy `Utils/TalentSpellPolicy.h`: Psyfiend when an enemy player is close, Guise below 35 percent health, Feather to kite, Star on a target in range) |
| Dominate Mind, Leap of Faith, Mass Dispel triggers, Dispel Magic on a shield | missing (Mass Dispel and Dispel Magic actions exist, `PriestActions.h:23,27`; nothing fires them) |
| Fear Ward on a partner | partly (action exists; the edge covers the main tank, `GenericPriestStrategy.cpp:35-37`) |
| Offense when idle (Shadow Word: Pain, Penance, Holy Fire, Smite) | exists (`GenericPriestStrategy.cpp:130-141`) |
| Desperate Prayer at critical health | exists (`GenericPriestStrategy.cpp:57-58`) |

### Gap list (ordered by value; rotation items wait for the owner's in-game test)

Config:

1. `PremadeSpecLinkPvp.5.0` to `110121`: From Darkness, Comes Light at tier 45 (its edge exists) and Divine Insight at tier 75 (guides, derived); the rest stays.
2. Glyphs: keep Power Word: Shield (42408) and Inner Fire (42402); swap Mass Dispel (42404) for Prayer of Mending (42417) until a Mass Dispel trigger exists; the Penance glyph stays out (guide, derived).

Rotation:

1. Penance first, then a free Power Word: Shield on the Divine Insight proc (good).
2. A fear or Dominate Mind chain on the enemy healer in a window, with a fear category check (expert).
3. A Mass Dispel trigger for a crowd controlled team mate or an immunity in a kill window; Dispel Magic on a shield before the partner's go (expert).
4. Power Word: Barrier at the pillar when two friends are hurt (good).
5. Leap of Faith on a stunned or rooted partner (expert).
6. Pain Suppression on a stun or an enemy go, not only at critical health (good).
