# Warlock: Destruction (damage)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). Gap item 15, first half
  (Ember Tap below 45 percent health with one ember while Sacrificial Pact and Unending Resolve are down, expert bots)
  is done; Flames of Xoroth is left for later. The "bots today" columns below describe the code before the 2026-11-11
  implementation.
- **Sources:**
  - Method, "Destruction Warlock Class Guide - MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 23 July 2025),
    read 2026-11-11 through a scripted fetch (the talent build is an image: only the talent hints in the text are usable).
  - wowtbc.gg, "Destruction Warlock PvE Guide, MoP Classic" (no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent, GlyphProperties,
    SpellCooldowns, SpellPower, SpellEffect, SpellMisc, SpellCategories, SpellCategory; `data/db2/Item-sparse.db2` for glyph
    item ids) through `spec_data.py` and short read-only scripts; the core source (the SkyFire core) for the
    spell scripts.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch, retried 2026-11-11), Wowhead (needs a browser; its leveling
    page answers 404).
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the tank's
  marks), 61 to 90 briefly.

Which source was richer: **talents (locked picks and the dungeon swaps), glyphs, stat order, the Fire and Brimstone and Rain of
Fire dungeon note** Method; **the full single target list (Shadowburn first, Rain of Fire and Fel Flame while moving), the
2-3 against 4+ target AoE lists, the 1-3 against more targets pet choice, the defensive order** wowtbc; **levels 15 to 60, the
ember-free levels 10 to 41** neither guide says anything, so those lines come from the 5.4.8 data and are marked "derived".
The guides agree on the order of Immolate, Conflagrate, Chaos Bolt, Incinerate. They differ on Chaos Bolt (Method: at 3 or
more embers outside cooldowns, wowtbc: at 3.5 or more or with strong buffs), on the AoE (Method's Rain of Fire under 4 targets
and Fire and Brimstone from 4, wowtbc's Immolate on all with Havoc under 4) and on the stat order after Hit (Method: Mastery,
then Critical Strike and Haste equal; wowtbc has none).

## Talents

Tiers and columns are from Talent.dbc. "Bots today" is `AiPlayerbot.PremadeSpecLink.9.2 = 111121` (`conf/playerbots.conf.dist:
2881`; one digit per tier, column counted from 0): Soul Leech, Mortal Coil, Sacrificial Pact, Burning Rush, Grimoire of
Sacrifice, Kil'jaeden's Cunning.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (111121) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Soul Leech (Method, locked in) | Dark Regeneration, Harvest Life (wowtbc lists them) | 108370 / 108359 / 108371 | Soul Leech, same. For Destruction it feeds on Incinerate, Fel Flame, Shadowburn and Chaos Bolt (spell text), so it works from 15 |
| 2 (30) | Mortal Coil (raid); Shadowfury in dungeons (Method) | Demonic Breath | 6789 / 30283 / 47897 | Mortal Coil, same as the raid pick; interrupt use (`GenericWarlockStrategy.cpp:115`). Shadowfury also has code (`:168`) |
| 3 (45) | Sacrificial Pact (Method, locked in: with Grimoire of Sacrifice there is no demon for Soul Link) | Dark Bargain | 108416 / 108415 / 110913 | Sacrificial Pact, same; coded at low health (`:107`) |
| 4 (60) | Burning Rush (Method, locked in) | Blood Horror, Unbound Will | 111400 / 111397 / 108482 | Burning Rush, same. **Inert:** a toggle that drains 4 percent of health per second; no action exists, so it is never cast |
| 5 (75) | Grimoire of Sacrifice (Method, locked in) | Grimoire of Supremacy (Observer), close but not better (Method) | 108503 / 108499 / 108501 | Grimoire of Sacrifice, same pick. **Dead:** there is no trigger for the `grimoire of sacrifice` action (grep of the strategies), so the demon is never sacrificed and the Imp stays out |
| 6 (90) | Kil'jaeden's Cunning (raid, moving a lot); Archimonde's Darkness in dungeons (Method) | Mannoroth's Fury only in heavy AoE without Fire and Brimstone | 137587 / 108505 / 108508 | Kil'jaeden's Cunning, same as the raid pick; level 90 only |

Notes on the picks (data, not guide):

- Grimoire of Sacrifice (108503): the demon is given up for one of its abilities, single target spells are stronger, and the
  effect ends when another demon is summoned (spell text). **When an action for it is written, the Imp strategy
  (`SummonImpStrategy`, `GenericWarlockNonCombatStrategy.cpp:106-108`: `no pet` and `wrong pet` re-summon at 51) must be switched
  off at the same time, or the bot summons an Imp the next tick and cancels the sacrifice.** Also, with no demon, Health Funnel,
  the pet interrupt and the pet health edges do nothing.
- Sacrificial Pact (108416): the demon gives 25 percent of its current health, the warlock gets a shield of four times that;
  with no demon (after the sacrifice) the warlock pays the health (script `spell_warlock.cpp:3568`). Cooldown 60 s.
- Kil'jaeden's Cunning (137587): Malefic Grasp, Incinerate and Shadow Bolt can be cast while moving (spell text). Useful for a
  bot that is told to step away; level 90 only.
- Burning Rush: do not add code for it without a health gate (it costs 4 percent per second).

Proposed link: **`111121` unchanged** (it is Method's raid build; the only dungeon differences are Shadowfury and Archimonde's
Darkness, owner questions 2 and 3).

## Glyphs

Types from GlyphProperties.dbc (apply spell effect 74, 0 major, 1 minor), item ids from `Item-sparse.db2`, effect texts from
Spell.dbc. Slots unlock in pairs at 25, 50 and 75 (config comment `conf/playerbots.conf.dist:2631-2636`; the unlock levels are
core-side). The config order is major, minor, major, minor, minor, major; a glyph goes to the first free slot of its type.

| Type | Glyph | Item id | Why (one clause) | Bots today (`42462,43394,42455,42457,43389,42464`) |
|------|-------|--------:|------------------|----------------------------------------------------|
| Major | Siphon Life | 42455 | both guides; Immolate heals the warlock on each periodic hit (spell text, the Immolate variant) | yes, slot 2 (level 50) |
| Major | Healthstone | 42462 | both guides; heals twice as much but over time | yes, slot 1 (level 25). Derived doubt: the bots use the stone at 25 percent health, where a heal over time is slow |
| Major | Curse of the Elements | 42464 | wowtbc; the curse hits additional nearby targets | yes, slot 3 (level 75) |
| Major, alternative | Eternal Resolve | 50077 | Method alternative; passive damage cut instead of a castable Unending Resolve | no |
| Major, alternative | Soulstone, Demonic Circle | 42470, 45782 | Method alternatives | no |
| Major, situational | Havoc | 42466 or 104054 | Havoc gains charges: two Chaos Bolts instead of three spells, longer cooldown (spell text); two items carry the name | no |
| Major, situational | Conflagrate | 42454 | Conflagrate slows without Immolate | no |
| Minor | Soulwell | 43394 | wowtbc; cosmetic ("glows"); Create Soulwell is learned at 68 and the bots never cast it | yes, slot 1 (level 25) where it does nothing |
| Minor | Nightmares | 42457 | wowtbc; the mounts cross water and leave flames | yes, slot 2 (level 50) |
| Minor | Unending Breath | 43389 | swim speed of Unending Breath targets; the bots cast Unending Breath when swimming | yes, slot 3 (level 75) |
| Minor, derived | Health Funnel | 42461 | Health Funnel restores the demon at once, 10 s cooldown; the bots cast Health Funnel on a hurt Imp. Useless after Grimoire of Sacrifice (no demon) | no |

Disagreement: Method lists Siphon Life and Healthstone as majors and no minor; wowtbc adds Curse of the Elements for AoE and
Soulwell and Nightmares as minors. The premade follows wowtbc. Proposed `PremadeSpecGlyph.9.2` (derived order: the self-heal
glyph first, because a glyph in a slot must do something at the level the slot unlocks): `42455,43389,42462,42457,42461,42464`
= Siphon Life, Unending Breath, Healthstone, Nightmares, Health Funnel, Curse of the Elements. Replace Health Funnel when
Grimoire of Sacrifice is implemented.

## Stat order

Method: Intellect, then Hit and Expertise to 15 percent chance to hit, then Mastery, then Critical Strike and Haste about equal.
wowtbc has no stat order. The caps are level 90 numbers and cannot be reached with the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **Intellect first** (the primary stat; Spell Power is not an item stat in 5.x, inference).
- Then Critical Strike and Haste about equal (Embers come from critical Immolate ticks, and Chaos Bolt grows with critical
  strike chance, spell texts), then Hit. **Mastery last below 80**: Mastery: Emberstorm is learned at 80 (SpecializationSpells),
  so before that a Mastery rating does nothing, although Method ranks it third at 90.
- Spirit is in neither guide and a warlock has no spell that scales with it (inference); it ranks last. Expertise does not help
  a caster's own spells (inference); skip it.
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:527-538`, shared by all three warlock specialisations): Intellect 0.3,
  **Spirit 0.6**, Spell Power 1.0, Hit 1.1, Haste 1.0, Critical 0.8, Mastery 0.87. Against the guide: **Spirit outranks
  Intellect two to one**, Critical is below Haste (the guide has them equal) and Mastery is above Critical although it is
  inactive before 80. The Spirit penalty of 0.4 applies only until Fel Armor (level 38) is known (`:722-726`).

## What the specialisation knows, by level (5.4.8 data)

Ids are the spells the player actually casts. Costs are percent of base mana; Burning Embers (108647) count 10 per ember, up to
four, and cost figures of 10 below are one ember. The global cooldown is 1.5 s.

| Level | Spell (id) |
|------:|------------|
| 1 | Shadow Bolt (686; replaced by Incinerate in the client from 10), Summon Imp (688) |
| 3 | Corruption (172; Immolate replaces it from 12) |
| 7 | Drain Life (689) |
| 8 | Summon Voidwalker (697) |
| 9 | Create Healthstone (6201) |
| 10 | Conflagrate (17962, instant, **2 charges, 12 s recharge**), Incinerate (29722, 2 s cast, 5 percent), Chaotic Energy (111546, passive: mana regenerates much faster, replaces Life Tap in the client); the specialisation is chosen here |
| 11 | Health Funnel (755) |
| 12 | Immolate (348, 1.5 s cast, 15 s DoT, 3 percent; critical ticks generate embers) |
| 14 | Fear (5782) |
| 16 | Life Tap (1454) exists in the data but Chaotic Energy replaces it in the client |
| 17 | Curse of Enfeeblement (109466) |
| 18 | Soulstone (20707) |
| 20 | Summon Succubus (712) |
| 21 | Rain of Fire (5740, 6 s channel, 8 percent) |
| 24 | Unending Breath |
| 29 | Summon Felhunter (691) |
| 30 | tier 2 talent |
| 32 | Backlash (108563, passive), Banish, Curse of Exhaustion |
| 34 | Twilight Ward |
| 36 | Havoc (80240, 25 s cooldown, 15 s: the next Chaos Bolt or three other single target spells also hit a second target) |
| 38 | Fel Armor (104938, passive) |
| 42 | Chaos Bolt (116858, 3 s cast, 1 ember, always critical), Burning Embers, Ember Tap (114635, 1 ember: heal), Self Burning Triggers; Ritual of Summoning |
| 45 | tier 3 talent |
| 47 | Shadowburn (17877, instant, 1 ember, only under 20 percent target health) |
| 49 | Summon Infernal (1122, 10 min cooldown shared with Doomguard) |
| 51 | Curse of the Elements |
| 54 | Fire and Brimstone (108683, toggle: Immolate, Incinerate, Conflagrate and curses hit all targets near the target and cost an ember each), Aftermath (109784) |
| 56 | Command Demon (119898) |
| 58 | Summon Doomguard (18540) |
| 60 | tier 4 talent |
| 64 | Unending Resolve (104773, 3 min, 8 s: 40 percent less damage taken, no interrupts) |
| 66 | Soulshatter |
| 69 | Backdraft (117896, passive) |
| 75 | tier 5 talent |
| 77 | Fel Flame (77799) |
| 79 | Flames of Xoroth (120451, 60 s, 1 ember) |
| 80 | Mastery: Emberstorm (77220) |
| 84 | Dark Soul: Instability (113858, 2 min, more critical strike for 20 s) |
| 86 | Pyroclasm (123686) |
| 90 | tier 6 talent |

Destruction does **not** know Soul Fire, Metamorphosis, Hand of Gul'dan, Unstable Affliction, Agony, Haunt, Drain Soul or Seed of
Corruption in this data. The bots' AoE and curse strategies ask for some of these.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked "derived"
comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to `mod-aware-bots-548/src/strategy/
Classes/warlock/` at commit `52b7750`.

How the bots rank what they do (read, `strategy/Strategy.h:37-51`): ACTION_DEFAULT 5, ACTION_NORMAL 10, ACTION_HIGH 20,
ACTION_INTERRUPT 40, ACTION_EMERGENCY 90. A trigger edge adds its action with that number; the engine takes the highest action
that is possible and useful; an action whose spell is unknown is simply skipped. A Destruction warlock gets the `destro` combat
strategy plus `curse of elements`, `dps assist`, `dps`, `aoe` and `cc` (`Factory/AiFactory.cpp:505-517`) and, outside
battlegrounds, `boost` (`:316`); out of combat `imp`, `dps assist` and `ss self` (`:768-775`). Health thresholds in config:
critical 25, low 45, medium 65 (`conf/playerbots.conf.dist:670-672`); the "target critical health" trigger is 20 percent
(`strategy/triggers/HealthTriggers.h:138`).

### Levels 15 to 29

Known: Incinerate, Conflagrate, Immolate, Drain Life, Fear, Life Tap (exists), Soulstone, Rain of Fire (21), pets Imp,
Voidwalker (8), Succubus (20). No embers yet (they start at 42).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | out of combat, no demon | Summon Voidwalker (697, 8) for 15 to 74 (derived: it has the tanking tools Suffering 15 and Shadow Shield 28; the Imp is a glass cannon) | **differs:** the `imp` strategy (`AiFactory.cpp:770`) summons the Imp at every level |
| 2 | core | out of combat | Create Healthstone (9), Soulstone on self (18) | yes (`GenericWarlockNonCombatStrategy.cpp:72`, `:775`) |
| 3 | core | target has no Immolate | Immolate (348, 12) | yes, edge 20.0 (`DestruWarlockStrategy.cpp:62`), default 5.9 (`:44`). It skips a target that dies within 8 s, which is sensible for trash |
| 4 | core | a Conflagrate charge is up | Conflagrate (17962, 10) | yes, default 5.8 (`:45`) |
| 5 | core | filler | Incinerate (29722, 10) | yes, default 5.6 (`:47`) |
| 6 | core | mana | nothing: Chaotic Energy regenerates mana fast; neither guide lists Life Tap for Destruction | **differs:** "low mana" Life Tap at 95 (`GenericWarlockStrategy.cpp:103`), edges at 29.5 and 5.1 (`DestruWarlockStrategy.cpp:79-80`) and out of combat at 23 (`GenericWarlockNonCombatStrategy.cpp:80`) cast the real Life Tap, which costs health |
| 7 | good | 3 or more enemies, from 21 | Rain of Fire (5740, 21) | yes, "medium aoe" 30 (`GenericWarlockStrategy.cpp:170`); whether the ground-targeted channel lands on the pack is to check live |
| 8 | good | other attackers have no Immolate (wowtbc AoE: "Immolate on all") | Immolate on each attacker | **missing:** the trigger and action exist (`WarlockTriggers.h` `ImmolateOnAttackerTrigger`, `WarlockActions.h` `CastImmolateOnAttackerAction`) but `DestruWarlockStrategy::InitTriggers` has no edge for them |
| 9 | good | a second attacker or a marked target | Fear (14) | yes, `cc` 33 (`GenericWarlockStrategy.cpp:151`, BT-064) |
| 10 | good | health at or under 25 percent | Healthstone, then Drain Life | yes (`:108-109`) |
| 11 | good | health under 65 percent in a group | nothing special; Drain Life only below 45 percent (derived) | **differs:** "medium health" casts Drain Life at 40, above every DoT (`:106`) |
| 12 | good | demon hurt | Health Funnel (11) | yes (`:99-100`) |

Before a spell is learned (derived): below 10 there is no specialisation (Shadow Bolt and Corruption are the kit); from 10 to 11
the kit is Incinerate and Conflagrate only, since Immolate waits for 12.

### Levels 30 to 44

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 13 | good | a casting enemy | Mortal Coil (30) if picked | yes, `GenericWarlockStrategy.cpp:115` |
| 14 | good | pack of 3 or more, tier 2 = Shadowfury | Shadowfury (30) | yes if picked, `:168`; **not picked** |
| 15 | good | a second attacker in reach | Havoc (80240, 36), then spend the next spells on the main target | yes, 19.8 (`DestruWarlockStrategy.cpp:68`, `WarlockActions.cpp:320-349`); it is cast whenever a second attacker exists and no Havoc of the bot is up, with no embers or Chaos Bolt needed (expert: cast it right before Chaos Bolt) |
| 16 | good | pet hurt | Health Funnel | yes |
| 17 | core | Chaos Bolt at 42 | see next band | |

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 18 | core | an ember or more | Chaos Bolt (116858, 42): as much as possible during Dark Soul, otherwise at 3 or more embers (Method) | partly: default 5.7 behind Immolate and Conflagrate, useful from 1 ember (`WarlockActions.cpp:228-231`), so embers never build to 3 (expert: hold them for the Dark Soul window); the Backdraft edge 19.0 (`DestruWarlockStrategy.cpp:63`) is from 69 |
| 19 | core | target at or under 20 percent health | Shadowburn (17877, 47) | yes, 18.0 (`:76`), under Immolate 20.0, Havoc 19.8 and Backdraft Chaos Bolt 19.0 |
| 20 | core | health at or under 45 percent | Sacrificial Pact (45) | yes (`GenericWarlockStrategy.cpp:107`) |
| 21 | good | 4 or more targets, from 54 | Fire and Brimstone (108683, 54), then Immolate, Conflagrate, Incinerate on the pack (Method) | **differs:** the toggle is cast whenever the bot has 4 embers (`WarlockTriggers.cpp:262-269`, edge `DestruWarlockStrategy.cpp:64` at 20.0), whatever the pack size; then Immolate, Conflagrate and Incinerate use the Fire and Brimstone ids and burn an ember each (`WarlockActions.cpp:232-265`). Rarely reached because Chaos Bolt spends embers from 1 (to check live) |
| 22 | good | curse missing, on elites | Curse of the Elements (51) "only if assigned"; otherwise Enfeeblement | **differs:** the `curse of elements` strategy (`AiFactory.cpp:508`, 29) and the generic edges (`GenericWarlockStrategy.cpp:92-93`, 25) put one of the two on every target |
| 23 | good | threat high | Soulshatter (66) | yes, 55 (`:97`) |
| 24 | good | a boss or an elite pack | Summon Doomguard (58) on cooldown | **missing** (the AoE edge asks for `summon abyssal` only, with its Infernal fallback in the non-combat node factory, `GenericWarlockNonCombatStrategy.cpp:39`; to check live) |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 25 | core | Backdraft up (69) | Chaos Bolt, then Incinerate under it | yes, 19.0 (`DestruWarlockStrategy.cpp:63`) |
| 26 | good | big damage incoming | Unending Resolve (64) | yes, low health (`GenericWarlockStrategy.cpp:107`) |
| 27 | good | moving | Rain of Fire (wowtbc) or Fel Flame from 77 | missing |
| 28 | expert | 2 Conflagrate charges | spend before capping (both guides) | partly: it is cast on every charge, so it never caps |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 75 | Grimoire of Sacrifice on the Imp before the first pull, no demon afterwards | dead (no action) and the Imp strategy would undo it, see Talents |
| 79 | Flames of Xoroth (1 ember) | action registered (`WarlockActions.h`), no edge |
| 80 | Mastery: Emberstorm | passive |
| 84 | Dark Soul: Instability on cooldown, Chaos Bolt in the window | cast on cooldown (`boost` 28, `GenericWarlockStrategy.cpp:145`); no ember hold |
| 90 | Method single target: Sacrifice, Dark Soul, Terrorguard or Doomguard, Immolate, Shadowburn execute, Conflagrate at 2 charges, Chaos Bolt at 3 embers, Incinerate | Immolate, Conflagrate, Chaos Bolt, Incinerate, Shadowburn, Havoc, Dark Soul are there; Sacrifice, the cooldown demon and the ember hold are not |

## Cooldowns and defensives

The guides say "on cooldown"; the timing for a dungeon bot is derived (trash fights last 20 to 40 s).

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Dark Soul: Instability | 84 | on cooldown, Chaos Bolt in the window | the same | `boost` 28 |
| Havoc | 36 (25 s) | with Chaos Bolt on a second target | a second attacker, right before Chaos Bolt | 19.8 whenever possible |
| Summon Doomguard / Infernal | 58 / 49 | on cooldown | a boss or an elite pack | missing |
| Grimoire of Sacrifice | 75 | before the pull | once, before the first pull | dead |
| Unending Resolve | 64 | the one defensive | health under 45 percent | `GenericWarlockStrategy.cpp:107` |
| Sacrificial Pact | 45 | locked-in talent | health under 45 percent | `:107` |
| Ember Tap | 42 | not named | health under 45 percent when Pact and Resolve are down (1 ember) | missing |
| Healthstone | item | wowtbc defensive 2 | health under 25 percent | `:108` |
| Mortal Coil | 30 | wowtbc defensive 3 | casting enemy, or panic | interrupt use only |

## Healer triage and mana (healers only)

Not applicable. Mana: Chaotic Energy (111546) raises mana regeneration a great deal and replaces Life Tap in the client, and
neither guide's priority list contains Life Tap for this specialisation. A bot that casts the real Life Tap anyway
(`GenericWarlockStrategy.cpp:103`, `DestruWarlockStrategy.cpp:79-80`, `GenericWarlockNonCombatStrategy.cpp:80`) loses health for
mana it does not need. The generic drink rule (`AiPlayerbot.DungeonDrinkMana = 50`) remains.

## Warlock-wide findings that touch this sheet

**Spell names the bots ask for that a Destruction warlock does not know (the level 25 debug log).** `SpellIdValue` logs
`[SID] ... resolved=0` whenever a name matches no known non-passive spell (`SpellIdValue.cpp:147`); the value is cached 20 s
per name and bot, so about 24 names give 500 lines in seven minutes. From the code (inference), for Destruction the names are:
`soul link` (passive, never resolves; `GenericWarlockStrategy.cpp:102`, `GenericWarlockNonCombatStrategy.cpp:77`);
`curse of the elements` (51; both the generic edge `:93` and the strategy `:187` ask, neither tests for the spell);
`dark soul: misery`, `dark soul: knowledge` (never known) and `dark soul: instability` (84), all in `boost` (`:143-145`);
from "medium aoe" (`:164-170`): `seed of corruption` and `seed of corruption on attacker` (never known), `shadowfury` and
`mannoroth's fury` (talents), `summon abyssal` (the Supremacy spell, in no spellbook), `immolation aura` (never known);
`summon abyssal ready` (`WarlockTriggers.cpp:294`, every evaluation); Chaos Bolt (42), Shadowburn (47), Havoc (36),
Backdraft (69), `fire and brimstone` (54) while their level is not reached (the Havoc and Backdraft triggers do test or read
auras, the Fire and Brimstone trigger does not); `corruption` is known but is the real Corruption (see below);
talents not yet learned (`mortal coil`, `dark bargain`, `sacrificial pact`, `dark regeneration`), `twilight ward` (34),
`unending resolve` (64), `soulshatter` (66); `shadowburn` (47) from the execute edge `DestruWarlockStrategy.cpp:76`. Fix
direction: known-spell test in the buff, curse and Fire and Brimstone triggers, level gates on the edges, specialisation gates
on the shared lists, delete `soul link`.

**Soul Link settled (data).** The talent spell 108415 has the passive attribute (Attributes 0x50040, bit 0x40), cast time 0 and
no aura of its own (effect 0 is a dummy on the caster; its text is the text of 108446: part of the damage taken is split with
the demon). The core learns a passive at once when the talent is taken (`Player.cpp:3773`, `:3913`), so there is nothing to
cast, and `SpellIdValue` skips passives (`SpellIdValue.cpp:63`), so the bots' two edges can never work; nothing in the core or
the client data grants the effective aura 108446 either. Destruction does not pick it (Method: no demon after the sacrifice).

**Spec replacements are not applied to the bots' casts (read, to check live).** The real client path swaps a spell for the
specialisation's replacement (`SpellHandler.cpp:1531-1565`): for Destruction Shadow Bolt becomes Incinerate, Corruption becomes
Immolate and Life Tap becomes Chaotic Energy. The bots swap only through `SpellInfo::OverrideSpellList`
(`AI/PlayerbotAI.cpp:3729-3737`) and in this core the code that fills it is commented out (`SpellInfo.cpp:718-742`). So a
Destruction bot's `shadow bolt` (the pull action `WarlockAiObjectContext.cpp:37`, the default 5.2) and `corruption` (defaults
5.3 to 5.5, `DestruWarlockStrategy.cpp:48-49`, `:72-73`) cast the real Shadow Bolt and Corruption, and `life tap` casts the real
Life Tap. The explicit names (`incinerate`, `immolate`) are right. The Corruption and Shadow Bolt lines sit below Incinerate 5.6
and are normally not reached, but the pull is a real Shadow Bolt.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Embers and Chaos Bolt start at 42, Shadowburn at 47, Fire and Brimstone at 54, Backdraft at 69,
  Havoc at 36, Dark Soul: Instability at 84, Grimoire of Sacrifice at 75; the lists above start without them, and levels 10 to
  41 are a plain Immolate, Conflagrate, Incinerate rotation (derived).
- Havoc's description in the data (next Chaos Bolt or three other single target spells) is not the guides' "spread Chaos
  Bolt" in full; it also copies Immolate and Incinerate.
- Grimoire of Sacrifice ends when another demon is summoned (spell text); the guides do not mention the Imp strategy the bots
  carry.
- Burning Rush and Grimoire of Sacrifice have no working code in the bots, so Method's locked-in picks are inert or dead.
- Soul Link is a passive (above); the bots' code treats it as a cast.

## Gap list (ordered)

Config (can go at once):

1. Glyph set and order: `PremadeSpecGlyph.9.2` (`conf/playerbots.conf.dist:2882`) to `42455,43389,42462,42457,42461,42464`.
   Reason: Siphon Life should unlock first (Immolate heals the warlock on every tick), Soulwell does nothing before 68 and
   the bots never cast it.
2. Stat weights (gear scoring), applies to all three warlock sheets: Intellect above Spirit and Spirit to 0, Mastery below
   Critical and Haste until 80 (`Mgr/Item/StatsWeightCalculator.cpp:527-538`, `:722-726`).
3. Tiers 30 and 90: dungeon picks Shadowfury and Archimonde's Darkness (link `121120`) are Method's dungeon default; owner choice (questions 2 and 3).

Strategy (class fixes, wait for the owner's in-game test):

4. Remove the `soul link` edges (`GenericWarlockStrategy.cpp:102`, `GenericWarlockNonCombatStrategy.cpp:77`) and the trigger
   `SoulLinkTrigger` (`WarlockTriggers.h:20`): the talent is a passive, there is nothing to cast.
5. Life Tap for Destruction: drop the edges `DestruWarlockStrategy.cpp:79-80`, and gate `GenericWarlockStrategy.cpp:103` and
   `GenericWarlockNonCombatStrategy.cpp:80` to the other two specialisations (Chaotic Energy replaces it).
6. Silence the unknown-name loop (the level 25 log): the list in the findings above; verify by counting `resolved=0` lines per
   bot per minute in the debug log.
7. Immolate on other attackers: wire `immolate on attacker` into `DestruWarlockStrategy::InitTriggers` (about 19.5, like the
   Affliction DoT edges); keep the Fire and Brimstone variants out of this path.
8. Grimoire of Sacrifice action and trigger at 75 (cast once, before the first pull), and at the same time switch off the
   `imp` pet strategy so the bot does not re-summon (`GenericWarlockNonCombatStrategy.cpp:106-108`, `AiFactory.cpp:770`).
9. Pet by level: Voidwalker for 8 to 74 instead of the Imp for the leveling Destruction warlock (`AiFactory.cpp:768-771`;
   derived; owner question 4).
10. Fire and Brimstone only with 4 or more enemies near the target (`FireAndBrimeStoneTrigger`, `WarlockTriggers.cpp:262-269`,
    edge `DestruWarlockStrategy.cpp:64`): add the count condition and a known-spell test; keep Chaos Bolt for single targets.
11. Chaos Bolt ember rule: hold embers for the Dark Soul window and spend from 3 embers otherwise (expert,
    `WarlockActions.cpp:228-231`); Havoc right before Chaos Bolt (expert, `DestruWarlockStrategy.cpp:68`).
12. Drain Life at "medium health" (`GenericWarlockStrategy.cpp:106`): move it to "low health" so a group warlock keeps its
    rotation at 65 percent.
13. Summon Doomguard (58) or Infernal (49) on a boss or elite pack: a trigger for the known spell and a single-target edge,
    not through the AoE edge; fix the Infernal fallback for the combat engine (`GenericWarlockNonCombatStrategy.cpp:39`).
14. Spec replacements for bot casts: apply the `SpecializationSpells` swap in `BotOverrideSpell` (`AI/PlayerbotAI.cpp:3729`)
    as `SpellHandler.cpp:1531-1565` does; the pull then casts Incinerate and `corruption` becomes Immolate.
15. Ember Tap (42) as a defensive when Pact and Resolve are down (expert); Flames of Xoroth (79) after the Imp dies (expert).
16. Skill tiers: the warlock strategies have no tier gating (`BotSkill` is used only in the tank code). Lines marked good and
    expert above (Fear, Havoc, Fire and Brimstone, the ember hold, Rain of Fire) are the ones a casual bot should drop.
17. To check live: BT-060 to BT-064 (deployed, live check pending); Rain of Fire as a ground target from a unit action; whether
    `summon abyssal` can resolve; that a Destruction bot under Backdraft casts Chaos Bolt at 19.0 (read, not tested).

Owner questions: (1) Keep the premade as Method's raid build. (2) Tier 30: Mortal Coil (interrupt, premade) or Shadowfury
(Method dungeon default, pack stun). (3) Tier 90: Kil'jaeden's Cunning (movement) or Archimonde's Darkness (Method dungeon
default). (4) Voidwalker or Imp while leveling. (5) Should a Destruction bot ever Life Tap. (6) Grimoire of Sacrifice: implement
(with the Imp strategy off) or pick Supremacy at 75 (works today).

## PvP play (player guides, 5.x, read 2026-11-19)

- **Sources:** three player guides for Destruction in arenas and battlegrounds: one from the 5.4 PTR (most current), one for 5.1 to 5.2 (written for rated battlegrounds), one recorded before the expansion shipped (beta spell text). No coach course. Facts only, nothing copied; data checked against this server's 5.4.8 tables.
- **Coverage:** talents, glyphs, stat order and the burst idea only. No guide shows an arena game, so opener, positioning, interrupts, matchups and defensive timing are GAP (lines marked "derived" or "adapted" are ours).

**The plan.** Destruction is a burst spec: Chaos Bolt is the one hard hit (always a critical strike, damage raised by crit chance), Shadowburn finishes a target under 20 percent, Havoc doubles the next Chaos Bolt (two Chaos Bolts with the glyph), Dark Soul: Instability adds crit, and with Archimonde's Darkness it has two charges so the burst can be repeated. Build to 3 or 4 embers before the first Chaos Bolt. Control is the baseline Howl of Terror (a beeline fear that also works as a peel), Mortal Coil, Fear; in 2v2 the author controls two targets at once.

**Tier choices by comp.** Mortal Coil when the partners already have stuns (a horror does not share stun diminishing returns, and it heals 15 percent); Shadowfury only when the team has no reliable stun. Grimoire of Sacrifice is the arena pick (more single target damage and a pet ability; with a Fel Hunter sacrificed, then a new pet and Spell Lock again gives two locks in a row); Supremacy only for sustained damage in battlegrounds. Dark Bargain (absorbs all damage for a short time, usable while crowd controlled, half repaid later) or Sacrificial Pact (shorter cooldown, but it sacrifices the warlock's own health when no demon is out, so it does not fit a sacrificed pet). Unbound Will breaks magic and loss of control; do not take Blood Horror together with Mortal Coil (both are horror and share diminishing returns).

**Defensives.** Dark Regeneration with Healthstone and its glyph (a 10 s heal over time) is the survival core; Demonic Circle with its glyph for repositioning; Soul Link is rejected for Destruction (little sustained damage to heal from). GAP: when to press which, Ember Tap use, line of sight.

**Talents and glyphs, guides against the set** (`PremadeSpecLinkPvp.9.2 = 020000`, `PremadeSpecGlyphPvp.9.2 = 50077,42461,42462,42457,43389,45782`):

| Tier | Guides (arena) | Set | Ladder (2 players, 3v3) |
|---|---|---|---|
| 15 | Dark Regeneration | Dark Regeneration | same |
| 30 | Mortal Coil (Shadowfury without a team stun) | Shadowfury | Mortal Coil |
| 45 | Dark Bargain, or Sacrificial Pact | Soul Link | Dark Bargain |
| 60 | Unbound Will | Blood Horror | Unbound Will |
| 75 | Grimoire of Sacrifice | Grimoire of Supremacy | Sacrifice |
| 90 | Archimonde's Darkness | same | same |

Glyphs the guides name: Conflagrate (all three; not in the set, ladder 100 percent), Healthstone and Demonic Circle (in the set). Havoc and Demon Training are battleground glyphs. Stat order (steers rolls only): hit to 6 percent, then mastery, crit, haste.

**Differences from the guides (5.4.8).** Howl of Terror is a baseline spell (level 30); the tier holds Demonic Breath, Mortal Coil, Shadowfury (the beta guide lists Howl as a talent). Blood Horror (111397) is a horror on melee hits, not the instant fear of the beta guide. Kil'jaeden's Cunning only lets Incinerate, Shadow Bolt and Malefic Grasp move (the beta text of a six second window is gone). Grimoire of Sacrifice raises maximum health only together with Soul Link. No Glyph of Burning Embers or Glyph of Dark Soul exists in the data. Shadowburn gives an ember only when the target yields experience or honor, so the claim that it feeds on pets and mirror images is not supported. **Glyph of Eternal Resolve (148683) removes the Unending Resolve button and gives a passive 10 percent reduction; the set holds that glyph while the bots cast the button (`ArenaPlan.cpp` defensive list, `Utils/PvpSpellCatalog.h:181`): one of the two is wrong.** Level gates: Havoc 36, Ember Tap 42, Chaos Bolt 42, Shadowburn 47, Fire and Brimstone 54, Flames of Xoroth 79, Dark Soul: Instability 84, Demonic Circle: Teleport 76, Mortal Coil level 30 (talent), Shadowfury and the rest by tier.

**Bots today (read from `src/strategy/Classes/warlock/`, ordered by value).**
1. Mortal Coil and Howl of Terror as peel and chain are missing: Mortal Coil is only an interrupt on a caster (`GenericWarlockStrategy.cpp:116`), Howl is only the fallback of Fear (`:54`), Shadowfury only in the area group (`:173`). `fear pvp` fires on a player in melee range (`:123`) with no chain or pillar check.
2. No Demonic Circle code at all, so no teleport as a defensive or reposition.
3. Chaos Bolt needs one ember (`DestruWarlockStrategy.cpp:41-47`, backdraft edge `:65`); no pooling to three.
4. Havoc on a second target is there (`:70`) but not paired with Chaos Bolt; Shadowburn under 20 percent exists (`:76`); Ember Tap at an expert level (`:78`).
5. Dark Soul: Instability only at level 84 and over, on the boost strategy (`GenericWarlockStrategy.cpp:148-149`); the second charge is not used.
6. Defensives: Dark Regeneration, Sacrificial Pact, Unending Resolve and Healthstone have edges (`:104-106`) and are in the shared defensive catalog; Dark Bargain has an edge but is not in the catalog; Unbound Will is in the break catalog (`PvpSpellCatalog.h:164`) but the set does not hold it.
7. The sacrifice of the pet is coded for Destruction (`WarlockTriggers.cpp:84`, `GenericWarlockNonCombatStrategy.cpp:72`) but the set holds Supremacy; no second Spell Lock.

**Research update (public patch notes 5.1 to 5.4.7, spell data and three more arena videos, 2026-11-19; lines marked derived come from game knowledge and the 5.4.8 data).** A commentary of four games (early in the expansion, so patch 5.0 to 5.1) shows the spell cleave plan: Gateway first, hold a pillar and bait the melee damage dealer into the open with Rain of Fire, keep him in the middle and tunnel him, control the healer as often as possible and the off damage dealer when the healer cannot be controlled, peel with Shadowfury, call the kill when the healer pushes into the open (cooldowns, stun, silence, burst). One damage dealer is almost always feared; the nukes are coordinated with the partner's control. Data: Chaos Bolt 3 s cast, 40 yd (it needs a stun, a horror or a safe target); Fear 1.7 s, 30 yd; Havoc 25 s, 15 s, 40 yd; Dark Soul: Instability 2 min, 20 s, +30 percent crit; Demonic Circle: Teleport 30 s, 40 yd. Patch facts: the Demonic Gateway debuff lasts 60 s since 5.4.0, so the Gateway is a once a minute tool per player; Rain of Fire stopped stunning in 5.1; Fel Flame no longer extends DoTs. Derived: Fear, Howl of Terror and Seduction share the fear category, 8 s cap on players, so fear two damage dealers in turn instead of three fears on one; Shadowfury goes first or on the other target because it shares the stun category with the partner's stuns; Dark Bargain is cast at high health (half of it is repaid later); start Chaos Bolt only inside a control window. Extra gap lines: Shadowfury is spent in the area group (`GenericWarlockStrategy.cpp:173`) and not held for the healer; the Havoc edge has no pairing with Chaos Bolt or a control check; nothing picks the pet by the enemy comp.

**Proposals.** Config (no code): tiers 30 to 75 to Mortal Coil, Dark Bargain, Unbound Will, Grimoire of Sacrifice (all four are the guides and the only two ladder players; the owner decides because the ladder rule asks five players), add Glyph of Conflagrate, and drop Glyph of Eternal Resolve or the Unending Resolve cast. Rotation (waits for the owner's in-game test): Mortal Coil and Howl as peel, Demonic Circle teleport, ember pooling, Havoc plus Chaos Bolt, Dark Soul second charge.

**Open questions.** (1) Eternal Resolve glyph or the Unending Resolve button. (2) Tier 45 Dark Bargain or Sacrificial Pact. (3) Whether the double Mortal Coil through Havoc really works on this core (not checked).

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 2 players of this specialisation (rating 2554 to 2592). 2v2: no player of this specialisation. Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** none, only 2 players in all (noted, not changed)
- **BT-324 result:** open (2 players)
- **Set now:** `PremadeSpecLinkPvp.9.2 = 020000`, `PremadeSpecGlyphPvp.9.2 = 50077,42461,42462,42457,43389,45782`

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Dark Regeneration 100% / - / 2 of 2 | Dark Regeneration | Dark Regeneration | matches |
| 2 (30) | Mortal Coil 100% / - / 2 of 2 | Shadowfury | Shadowfury | differs, not changed |
| 3 (45) | Dark Bargain 100% / - / 2 of 2 | Soul Link | Soul Link | differs, not changed |
| 4 (60) | Unbound Will 100% / - / 2 of 2 | Blood Horror | Blood Horror | differs, not changed |
| 5 (75) | Grimoire of Sacrifice 100% / - / 2 of 2 | Grimoire of Supremacy | Grimoire of Supremacy | differs, not changed |
| 6 (90) | Archimonde's Darkness 100% / - / 2 of 2 | Archimonde's Darkness | Archimonde's Darkness | matches |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Conflagrate (42454) | 100% | - | 2 of 2 | no | no |
| major | Healthstone (42462) | 100% | - | 2 of 2 | yes | yes |
| major | Demonic Circle (45782) | 100% | - | 2 of 2 | yes | yes |
| major | Eternal Resolve (50077) | 0% | - | 0 of 2 | yes | yes |
| minor | Nightmares (42457) | 100% | - | 2 of 2 | yes | yes |
| minor | Health Funnel (42461) | 100% | - | 2 of 2 | yes | yes |
| minor | Unending Breath (43389) | 50% | - | 1 of 2 | yes | yes |
| minor | Soulwell (43394) | 50% | - | 1 of 2 | no | no |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **Open (BT-451):** two players in 3v3, none in 2v2: they differ from the set at tier 30 (Mortal Coil, set Shadowfury), tier 45 (Dark Bargain, set Soul Link), tier 60 (Unbound Will, set Blood Horror) and tier 75 (Grimoire of Sacrifice, set Grimoire of Supremacy); the glyphs Demonic Circle, Healthstone, Health Funnel, Nightmares are held, Conflagrate (major, 100) is not. The set is still the affliction guide's copy.
