# Warlock: Demonology (damage)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). Gap item 14, second
  half (Soul Fire saved for Metamorphosis or 9 Molten Core stacks by expert bots) is done; Carrion Swarm as an interrupt
  is left for later (it needs the interrupt duty). The "bots today" columns below describe the code before the
  2026-11-11 implementation.
- **Sources:**
  - Method, "Demonology Warlock Class Guide, MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 23 July 2025), read
    2026-11-11 through a scripted fetch (the talent build is an image: only the talent hints in the text are usable).
  - wowtbc.gg, "Demonology Warlock Rotation & Talent Build - MoP Classic" (no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent, GlyphProperties,
    SpellCooldowns, SpellPower, SpellEffect, SpellMisc, SpellCategories, SpellCategory; `data/db2/Item-sparse.db2` for glyph
    item ids) through `spec_data.py` and short read-only scripts; the core source (the SkyFire core) for the
    spell scripts.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch, retried 2026-11-11), Wowhead (needs a browser; its leveling
    page answers 404).
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the tank's
  marks), 61 to 90 briefly.

Which source was richer: **talents, glyphs, stat order, the Metamorphosis entry and exit numbers, the 6 against 7 plus target
split** Method; **the full priority list (17 steps with Fel Flame while moving, Soul Fire at 9 Molten Core stacks), the AoE
tiers (2-3, 4-6, 7+), the minor glyphs and the pet choice for 1-3 against more targets** wowtbc; **levels 15 to 60** neither
guide says anything, so those lines come from the 5.4.8 data and are marked "derived". The guides agree on the order; they
differ on the Metamorphosis entry (Method: Dark Soul: Knowledge up or 900 Fury, exit below 300; wowtbc: 950 Fury, or after the
second Hand of Gul'dan, exit below 300 with DoTs refreshed) and on the stat order after Hit (Method has Haste to its first cap,
then Critical Strike, then Mastery); I take Method's numbers for the level 90 line and call them expert.

## Talents

Tiers and columns are from Talent.dbc. "Bots today" is `AiPlayerbot.PremadeSpecLink.9.1 = 111112` (`conf/playerbots.conf.dist:
2876`; one digit per tier, column counted from 0): Soul Leech, Mortal Coil, Sacrificial Pact, Burning Rush, Grimoire of
Service, Mannoroth's Fury.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (111112) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Soul Leech (Method, core) | Dark Regeneration, Harvest Life (wowtbc lists them) | 108370 / 108359 / 108371 | Soul Leech, same. For Demonology it feeds on Shadow Bolt, Soul Fire, Fel Flame and Touch of Chaos (spell text), so it works from level 15 |
| 2 (30) | Mortal Coil (Method raid default); Shadowfury in dungeons (Method) | Shadowfury for pack stuns; Demonic Breath | 6789 / 30283 / 47897 | Mortal Coil, same as the raid pick; used as an interrupt on a casting enemy (`GenericWarlockStrategy.cpp:115`). Shadowfury also has code (`:168`) |
| 3 (45) | Soul Link (Method default) | Sacrificial Pact for burst (Method); Dark Bargain | 108415 / 108416 / 110913 | **Sacrificial Pact, differs.** Soul Link is a passive and nothing in the core applies its pet side (see "Soul Link settled"), so Sacrificial Pact (coded at low health, `:107`) is the safer pick |
| 4 (60) | Burning Rush (Method, core) | Blood Horror, Unbound Will | 111400 / 111397 / 108482 | Burning Rush, same. **Inert:** a toggle that drains 4 percent of health per second; no action exists, so it is never cast |
| 5 (75) | Grimoire of Service (Method default; wowtbc lists all) | Grimoire of Supremacy ("cosmetic", not optimal, Method); Grimoire of Sacrifice | 108501 / 108499 / 108503 | Grimoire of Service, same pick. **Dead:** there is no `grimoire of service` action or trigger, so the second demon is never summoned. Supremacy would work today (passive swap plus the `summon abyssal ready` cooldown demon) |
| 6 (90) | Archimonde's Darkness (Method default) | Mannoroth's Fury for heavy AoE and for large dungeon packs (Method) | 108505 / 108508 / 137587 | Mannoroth's Fury, matches Method's dungeon note; coded at 3 or more targets (`GenericWarlockStrategy.cpp:164`). Level 90 only |

Notes on the picks (data, not guide):

- Demonology has no spec swap in the data (SpecializationSpells lists no "replaces" row for 266 except Dark Soul), so the
  Metamorphosis transformations are the only swaps and they are aura based; the bots' cast path handles those.
- Mortal Coil (6789): the target runs in horror for 3 s and the caster restores health; a feared mob cannot finish a cast, so it
  works as an interrupt.
- Sacrificial Pact (108416): the demon gives 25 percent of its current health and the warlock gets a shield of four times that
  (script `spell_warlock.cpp:3568`). Cooldown 60 s.
- Grimoire of Service (108501): a second demon for a limited time with an opening ability, 2 minute cooldown (spell text).
- Soul Leech works from 15 here, unlike Affliction; no reason to change tier 15.

Proposed link: **`111112` unchanged** until a `grimoire of service` action exists; if the maintainer does not plan to write it, tier 75
should be Grimoire of Supremacy (digit 0): `111102`. Owner question 1.

### Soul Link settled (data)

The talent spell 108415 has the passive attribute (Attributes 0x50040, bit 0x40), cast time 0 and no aura of its own: effect 0
is a dummy on the caster, and its text is the text of 108446 ("part of all damage taken is split with your demon, part of the
damage you deal is converted to healing on you and your demon"). The core learns a passive at once when the talent is taken
(`Player.cpp:3773`, `:3913`), so there is nothing to cast, and the bots cannot cast it either: `SpellIdValue` skips passives
(`strategy/value/SpellIdValue.cpp:63`). The effective aura is 108446 (level 20, area aura on the demon's owner), which has a
spell script (`spell_warlock.cpp:3374`); nothing in the core source or the client data grants or casts it (no SpellLearnSpell
row, no linked spell, no call), so whether the talent does anything on this core is unverified. The bots' two edges
(`GenericWarlockStrategy.cpp:102` at 95 in combat, `GenericWarlockNonCombatStrategy.cpp:77` at 28 out of combat) use a buff
trigger that fires whenever no aura named "Soul Link" exists, which is always: they re-arm every 5 s and fail. Both should go.
Fel Armor (104938) is also a passive (Attributes 0x150), as BT-146 found.

## Glyphs

Types from GlyphProperties.dbc (apply spell effect 74, 0 major, 1 minor), item ids from `Item-sparse.db2`, effect texts from
Spell.dbc. Slots unlock in pairs at 25, 50 and 75 (config comment `conf/playerbots.conf.dist:2631-2636`; the unlock levels are
core-side). The config order is major, minor, major, minor, minor, major; a glyph goes to the first free slot of its type.

| Type | Glyph | Item id | Why (one clause) | Bots today (`42465,42453,42462,43394,42457,42455`) |
|------|-------|--------:|------------------|----------------------------------------------------|
| Major | Imp Swarm | 42465 | both guides' first major; teaches Imp Swarm (104316, 2 minute cooldown, four Wild Imps; Demonology only, script `spell_warlock.cpp:2030-2110`) | yes, slot 1 (level 25). **Dead:** no `imp swarm` action exists, so the glyph teaches a spell the bot never casts |
| Major | Siphon Life | 42455 | both guides; Corruption heals the warlock on each tick (spell text) | yes, slot 3 (level 75), **late** |
| Major | Demon Training | 42460 | Method core; improves the demon's special abilities (Imp Firebolt faster, Voidwalker armor, Succubus Seduction cleanses) | no |
| Major | Healthstone | 42462 | wowtbc standard; heals twice as much but over time | yes, slot 2 (level 50). Derived doubt: the bots use the stone at 25 percent health, where a heal over time is slow |
| Major, alternative | Eternal Resolve | 50077 | Method alternative; passive damage cut instead of a castable Unending Resolve | no |
| Major, alternative | Soulstone, Demonic Circle | 42470, 45782 | Method alternatives | no |
| Major, AoE | Curse of the Elements | 42464 | wowtbc; hits additional nearby targets | no |
| Minor | Hand of Gul'dan | 42453 | both guides (Method "optional"); Hand of Gul'dan can be aimed at a location, which the bots cannot do | yes, slot 1 (level 25), no effect for a bot |
| Minor | Soulwell | 43394 | wowtbc; cosmetic ("glows"); Create Soulwell is learned at 68 and the bots never cast it | yes, slot 2 (level 50) |
| Minor | Nightmares | 42457 | wowtbc; the mounts cross water and leave flames | yes, slot 3 (level 75) |
| Minor, derived | Health Funnel | 42461 | Health Funnel restores the demon at once, 10 s cooldown; the bots cast Health Funnel on a hurt demon (`GenericWarlockStrategy.cpp:99-100`) | no |
| Minor, situational | Carrion Swarm | 42471 | wowtbc; Carrion Swarm no longer knocks back (learned at 47, not cast by the bots) | no |

Disagreement: Method lists Imp Swarm, Siphon Life and Demon Training as majors and Hand of Gul'dan as the only minor; wowtbc has
Imp Swarm, Healthstone and Siphon Life as majors (Demon Training only with Sacrificial Pact) and three minors. The premade
follows wowtbc. Proposed `PremadeSpecGlyph.9.1` for the code as it stands (derived order: a glyph in a slot must do something at
the level the slot unlocks): `42455,42461,42460,42457,43389,42465` = Siphon Life, Health Funnel, Demon Training, Nightmares,
Unending Breath, Imp Swarm (last, so it costs nothing until an `imp swarm` action exists). Once that action exists, move
Imp Swarm to the second major slot (level 50, above the spell's own level 32): `42455,42461,42465,42457,43389,42460`.

## Stat order

Method: Intellect, then Hit and Expertise to 15 percent chance to hit, then Haste to its first cap (about 4717 rating, one more
DoT tick), then Critical Strike, then Mastery, then more Haste. wowtbc has no stat order. The caps are level 90 numbers and
cannot be reached with the gear of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **Intellect first** (the primary stat; Spell Power is not an item stat in 5.x, inference).
- Then Haste and Critical Strike about equal (Wild Imps and Shadow Bolt both scale with them), then Hit. **Mastery last below
  80**: Mastery: Master Demonologist is learned at 80 (SpecializationSpells), so before that a Mastery rating does nothing.
- Spirit is in neither guide and a warlock has no spell that scales with it (inference); it ranks last. Expertise does not help
  a caster's own spells (inference); skip it.
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:527-538`, shared by all three warlock specialisations): Intellect 0.3,
  **Spirit 0.6**, Spell Power 1.0, Hit 1.1, Haste 1.0, Critical 0.8, Mastery 0.87. Against the guide: **Spirit outranks
  Intellect two to one**, and Mastery is above Critical although it is inactive before 80. The Spirit penalty of 0.4 applies
  only until Fel Armor (level 38) is known (`:722-726`).

## What the specialisation knows, by level (5.4.8 data)

Ids are the spells the player actually casts. Costs are percent of base mana. The global cooldown is 1.5 s. Demonic Fury (104315)
is the specialisation's resource (cap 1000): damaging spells and the demon's attacks generate it, Metamorphosis spends it.

| Level | Spell (id) |
|------:|------------|
| 1 | Shadow Bolt (686, 1.7 s cast, 5.5 percent, generates Demonic Fury), Summon Imp (688) |
| 3 | Corruption (172, instant, ticks generate Demonic Fury) |
| 7 | Drain Life (689) |
| 8 | Summon Voidwalker (697) |
| 9 | Create Healthstone (6201) |
| 10 | Metamorphosis (103958, 10 s cooldown, spends Fury while active; prevents Corruption until Doom (36) and Hand of Gul'dan until Chaos Wave (79), spell text), Demonic Fury; the specialisation is chosen here |
| 11 | Health Funnel (755) |
| 12 | Demonic Leap (109151) |
| 13 | Soul Fire (6353, 4 s cast, 15 percent, always critical) |
| 14 | Fear (5782) |
| 16 | Life Tap (1454) |
| 17 | Curse of Enfeeblement (109466) |
| 18 | Soulstone (20707) |
| 19 | Hand of Gul'dan (105174, instant, 5 percent, **2 charges, 15 s recharge**) |
| 20 | Summon Succubus (712) |
| 22 | Hellfire (1949, channel, 2 percent, costs the warlock health) |
| 24 | Unending Breath |
| 25 | Metamorphosis: Touch of Chaos (112089): in the form Shadow Bolt becomes Touch of Chaos (103964, costs 40 Fury) |
| 27 | Nether Plating (114129) |
| 29 | Summon Felhunter (691) |
| 30 | tier 2 talent |
| 32 | Wild Imps (114592, passive: Shadow Bolt, Soul Fire and Touch of Chaos summon a Wild Imp on a timer), Imp Swarm only through its glyph, Banish, Curse of Exhaustion |
| 36 | Metamorphosis: Doom (124913): Corruption becomes Doom (603, 60 s, costs 60 Fury) |
| 42 | Summon Felguard (30146, 5 s cast); Ritual of Summoning |
| 43 | Felguard: Pursuit (103129) |
| 45 | tier 3 talent |
| 47 | Metamorphosis: Carrion Swarm (103967, 12 s, 50 Fury, a shadow wave that interrupts spell casting) |
| 48 | Felguard: Felstorm (103134) |
| 49 | Summon Infernal (1122, 10 min cooldown shared with Doomguard) |
| 51 | Curse of the Elements |
| 53 | Felguard: Axe Toss (103131, stun) |
| 54 | Demonic Rebirth (108559, passive) |
| 56 | Command Demon (119898) |
| 58 | Summon Doomguard (18540) |
| 60 | tier 4 talent |
| 62 | Metamorphosis: Immolation Aura (109797): Hellfire becomes a non-channeled aura that no longer hurts the warlock |
| 64 | Unending Resolve (104773, 3 min, 8 s: 40 percent less damage taken, no interrupts) |
| 66 | Soulshatter; 67 Metamorphosis: Cursed Auras (116208) |
| 69 | Molten Core (122351, passive: Shadowflame or a Wild Imp hit can trigger it; Soul Fire becomes faster and cheaper) |
| 73 | Decimation (108869, passive: Soul Fire or Shadow Bolt on a target under 25 percent health triggers Molten Core) |
| 75 | tier 5 talent |
| 79 | Metamorphosis: Chaos Wave (124917): Hand of Gul'dan becomes Chaos Wave |
| 80 | Mastery: Master Demonologist (77219) |
| 84 | Dark Soul: Knowledge (113861, 2 min, more Mastery for 20 s) |
| 85 | Metamorphosis: Void Ray (129343) |
| 90 | tier 6 talent |

Demonology does **not** know Rain of Fire, Seed of Corruption, Agony, Unstable Affliction, Immolate or Incinerate in this data
(spec lists 265 and 267 have them). The bots' AoE strategy and the `curse of agony` strategy ask for several of these.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked "derived"
comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to `mod-aware-bots-548/src/strategy/
Classes/warlock/` at commit `52b7750`.

How the bots rank what they do (read, `strategy/Strategy.h:37-51`): ACTION_DEFAULT 5, ACTION_NORMAL 10, ACTION_HIGH 20,
ACTION_INTERRUPT 40, ACTION_EMERGENCY 90. A trigger edge adds its action with that number; the engine takes the highest action
that is possible and useful; an action whose spell is unknown is simply skipped. A Demonology warlock gets the `demo` combat
strategy plus `curse of agony`, `meta melee`, `dps assist`, `dps`, `aoe` and `cc` (`Factory/AiFactory.cpp:500-517`) and, outside
battlegrounds, `boost` (`:316`); out of combat `felguard`, `dps assist` and `ss self` (`:758-775`). Health thresholds in config:
critical 25, low 45, medium 65 (`conf/playerbots.conf.dist:670-672`).

### Levels 15 to 29

Known: Shadow Bolt, Corruption, Metamorphosis (10), Soul Fire (13), Hand of Gul'dan (19), Hellfire (22), Touch of Chaos in the
form (25), Drain Life, Fear, Life Tap, Soulstone, pets Imp, Voidwalker (8), Succubus (20).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | out of combat, no demon | Summon Voidwalker (697, 8) until the Felguard at 42 (derived: it has the tanking tools Suffering 15 and Shadow Shield 28) | yes by accident of the chain: `felguard` strategy (`AiFactory.cpp:765`), Felguard unknown, Voidlord unknown, so Voidwalker (`GenericWarlockNonCombatStrategy.cpp:43`, `:36`). At 42 the `wrong pet` trigger swaps to the Felguard |
| 2 | core | out of combat | Create Healthstone (9), Soulstone on self (18) | yes (`GenericWarlockNonCombatStrategy.cpp:72`, `:775` `ss self`) |
| 3 | core | target has no Corruption | Corruption (172, 3) | yes, edge `doom` 18.0 whose node falls back to `corruption` (`DemonoWarlockStrategy.cpp:77`, `:20`), default 5.5 (`:53`); other attackers 19.5 (`:76`) |
| 4 | core | a Hand of Gul'dan charge is up | Hand of Gul'dan (105174, 19) on the pack | yes, default 5.4 (`:54`), cast by id (`WarlockActions.cpp:223`). Spending at one charge, not two, is fine for a bot |
| 5 | core | filler | Shadow Bolt (686, 1): it builds Fury and feeds Wild Imps | yes, default 5.3 (`:56`) |
| 6 | core | mana low, health above 45 percent | Life Tap (1454, 16) | yes, late: "low mana" at 95 and "mana under 85" at 5.1 (`:88`, `GenericWarlockStrategy.cpp:103`) |
| 7 | good | Fury 500 or more | Metamorphosis (103958, 10). Derived: only from 36 (Doom); before that the form blocks Corruption and Hand of Gul'dan and offers only Touch of Chaos (25) | **differs:** from 10 at 500 Fury, 28.5 (`DemonoWarlockStrategy.cpp:71`, `WarlockTriggers.cpp:239-248`), above every DoT and ahead of Hand of Gul'dan charges |
| 8 | good | a second attacker or a marked target | Fear (14) | yes, `cc` 33 (`GenericWarlockStrategy.cpp:151`, BT-064) |
| 9 | good | health at or under 25 percent | Healthstone, then Drain Life | yes (`:108-109`) |
| 10 | good | health under 65 percent in a group | nothing special; Drain Life only below 45 percent (derived) | **differs:** "medium health" casts Drain Life at 40, above every DoT (`:106`) |
| 11 | good | 3 or more enemies, from 22 | Hellfire (1949, 22); Method says 4 or more without Mannoroth's Fury | partly: "medium aoe" (3 targets) `immolation aura` 34 with fallback `hellfire` (`:165`, `DemonoWarlockStrategy.cpp:26`); a melee-range action that walks the warlock to the pack and costs health |
| 12 | good | demon hurt | Health Funnel (11) | yes (`GenericWarlockStrategy.cpp:99-100`) |

Soul Fire (13) is a 4 s, 15 percent mana nuke before Molten Core (69) and is not a filler (derived, and neither guide casts it
without Molten Core or a target under 25 percent). The default list has it last at 5.2, behind Shadow Bolt, so it is not cast:
correct.

### Levels 30 to 44

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 13 | core | in Metamorphosis, from 25 | Touch of Chaos replaces Shadow Bolt (112089, 25) | yes: `shadow bolt` is swapped by the form's aura (`AI/PlayerbotAI.cpp:3743-3756`); the names `touch of chaos`, `chaos wave`, `void ray` are in the lists but never resolve (not in the spellbook) |
| 14 | core | in Metamorphosis, from 36 | Doom (603, 36) on the target, 60 s | yes: the Corruption trigger switches to `doom` in the form (`WarlockTriggers.cpp:250-259`), node falls back to `corruption`, the aura swap turns it into Doom |
| 15 | good | Fury under 300 | leave the form (Method, wowtbc) | **differs:** the bot leaves at 50 or less (`WarlockTriggers.cpp:241-244`), i.e. when the form ends by itself |
| 16 | good | a casting enemy | Mortal Coil (30) if picked, Felhunter Spell Lock does not exist for a Felguard bot | yes, `GenericWarlockStrategy.cpp:115` |
| 17 | good | pack of 3 or more, tier 2 = Shadowfury | Shadowfury (30) | yes if picked, `:168`; **not picked** |
| 18 | core | at 42, Felguard | Summon Felguard (30146, 42) | yes (`WrongPetTrigger`, `WarlockTriggers.cpp:145-206`) |
| 19 | expert | Imp Swarm ready (via glyph, spell level 32) | Imp Swarm (104316): both guides on cooldown | **missing:** no action, see Glyphs |

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 20 | core | health at or under 45 percent | Sacrificial Pact (45) | yes (`GenericWarlockStrategy.cpp:107`) |
| 21 | expert | the Felguard is out | Felstorm (48) and Axe Toss (53) on cooldown (Felstorm in both guides) | to check: no action; whether the pet uses them by autocast (`toggle pet spell`, `GenericWarlockNonCombatStrategy.cpp:71`) is unread |
| 22 | good | casting enemy in the form | Carrion Swarm (47, 12 s, 50 Fury) interrupts casting | missing |
| 23 | good | a boss or an elite pack | Summon Doomguard (58) on cooldown | missing (the AoE edge asks for `summon abyssal` only, with its Infernal fallback in the non-combat node factory, `GenericWarlockNonCombatStrategy.cpp:39`; to check live) |
| 24 | good | curse missing, on elites | Curse of the Elements (51) "only if assigned"; otherwise Enfeeblement | **differs:** every target gets one of the two at 25 (`GenericWarlockStrategy.cpp:92-93`) |
| 25 | good | threat high | Soulshatter (66) | yes, 55 (`:97`) |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 26 | core | in the form, 3 or more enemies | Immolation Aura (62) | yes, 34 (`GenericWarlockStrategy.cpp:165`), then `reach melee` 25.5 (`DemonoWarlockStrategy.cpp:93-100`) walks the caster into the pack; to check in a dungeon that this stays behind the tank |
| 27 | core | Molten Core up (69) | Soul Fire on the proc; Method saves procs for the form unless about to overcap (expert) | yes, 16.5 (`DemonoWarlockStrategy.cpp:84`); spends at once |
| 28 | good | big damage incoming | Unending Resolve (64) | yes, low health (`GenericWarlockStrategy.cpp:107`) |
| 29 | good | Hand of Gul'dan charges before the form (from 19 to 78 the form blocks it) | spend both charges, then enter (wowtbc "after the second Hand of Gul'dan") | missing: the form edge 28.5 outranks the Hand of Gul'dan default 5.4 |
| 30 | expert | Molten Core stacks | Soul Fire at 9 or more stacks (wowtbc) | missing |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 75 | Grimoire of Service: a second Felguard with Felstorm, on cooldown | missing (talent dead, see Talents) |
| 79 | Chaos Wave replaces Hand of Gul'dan in the form | the name is in the default list at 5.35 but never resolves; the aura swap handles the base id (`:55`) |
| 80 | Mastery: Master Demonologist (more damage in the form) | passive |
| 84 | Dark Soul: Knowledge on cooldown, enter the form while it is up | cast on cooldown (`boost` 28, `GenericWarlockStrategy.cpp:144`); no entry rule tied to it |
| 85 | Void Ray at 7 or more targets | name never resolves; the Fel Flame fallback (`DemonoWarlockStrategy.cpp:28`) has no edge |
| 90 | Method single target: Felguard, Corruption, Dark Soul, Imp Swarm, Doomguard, Service, Felstorm, form at 900 Fury or with Dark Soul, Doom, Touch of Chaos, Hand of Gul'dan at 2 charges, Soul Fire on Molten Core, Shadow Bolt | Felguard, Corruption/Doom, Hand of Gul'dan, Touch of Chaos, Soul Fire, Shadow Bolt are there; Imp Swarm, Doomguard, Service, Felstorm, the Fury thresholds are not |

## Cooldowns and defensives

The guides say "on cooldown"; the timing for a dungeon bot is derived (trash fights last 20 to 40 s).

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Metamorphosis | 10 | at 900 Fury or with Dark Soul | from 36; at 500 or more Fury on a pack of 3 or a boss, after the Hand of Gul'dan charges | at 500 from level 10, 28.5 |
| Hand of Gul'dan | 19 | at 2 charges | the same | default 5.4 |
| Dark Soul: Knowledge | 84 | on cooldown | the same | `boost` 28 |
| Imp Swarm | glyph (32) | on cooldown | pack of 3 or a boss | missing |
| Summon Doomguard / Infernal | 58 / 49 | on cooldown | a boss or an elite pack | missing |
| Grimoire of Service | 75 | on cooldown | the same | missing |
| Felstorm | 48 | on cooldown | the same | to check |
| Unending Resolve | 64 | first defensive | health under 45 percent | `:107` |
| Sacrificial Pact | 45 | burst fights | health under 45 percent | `:107` |
| Healthstone | item | wowtbc defensive 2 | health under 25 percent | `:108` |
| Mortal Coil | 30 | wowtbc defensive 3 | casting enemy, or panic | interrupt use only |

## Healer triage and mana (healers only)

Not applicable. Mana: Demonology uses Life Tap like Affliction (both guides list it for downtime), but its Soul Fire (15 percent)
and Hellfire are the mana-heavy spells; below Molten Core neither should be a filler. The mana lines the bots have: "low mana"
at 15 percent at 95, the `LifeTapTrigger` (mana under 85 percent, health above 45) at 5.1, out of combat at 23, and the generic
drink rule (`AiPlayerbot.DungeonDrinkMana = 50`).

## Warlock-wide findings that touch this sheet

**Spell names the bots ask for that a Demonology warlock does not know (the level 25 debug log).** `SpellIdValue` logs
`[SID] ... resolved=0` whenever a name matches no known non-passive spell (`SpellIdValue.cpp:147`); the value is cached 20 s
per name and bot, so about 24 names give 500 lines in seven minutes. From the code (inference), for Demonology the names are:
`soul link` (passive, never resolves; `GenericWarlockStrategy.cpp:102`, `GenericWarlockNonCombatStrategy.cpp:77`);
`agony` and `agony on attacker` (Agony is an Affliction spell; the `curse of agony` strategy is added to Demonology at
`AiFactory.cpp:503`, so both edges `GenericWarlockStrategy.cpp:181-182` always fail: remove the strategy for Demonology);
`curse of the elements` (51; `:93` has no known-spell test); `dark soul: misery`, `dark soul: instability` (never known) and
`dark soul: knowledge` (84), all in `boost` (`:143-145`);
from "medium aoe" (`:164-170`): `seed of corruption` and `seed of corruption on attacker` (never known), `rain of fire` (never
known), `shadowfury`, `mannoroth's fury`, `summon abyssal` (the Supremacy spell, in no spellbook), `immolation aura` (62);
`summon abyssal ready` (`WarlockTriggers.cpp:294`, every evaluation); the default list names `chaos wave`, `touch of chaos`
(`DemonoWarlockStrategy.cpp:55`, `:57`), which are override spells, never in the spellbook; `void ray`, `fel flame`;
talents not yet learned (`mortal coil`, `dark bargain`, `sacrificial pact`, `dark regeneration`), `twilight ward` (34),
`unending resolve` (64), `soulshatter` (66); pet chain names `summon felguard` (42), `summon voidlord`, and the `wrong pet`
trigger (`WarlockTriggers.cpp:145`) asking for `summon felguard` below 42. Fix direction: known-spell test in the buff and curse
triggers, level gates on the edges, specialisation gates on the shared lists, delete `soul link`, drop override-spell names that
can never resolve.

**Spec replacements are not applied to the bots' casts (read, to check live).** The real client path swaps a spell for the
specialisation's replacement (`SpellHandler.cpp:1531-1565`); the bots swap only through `SpellInfo::OverrideSpellList`
(`AI/PlayerbotAI.cpp:3729-3737`), and in this core the code that fills it is commented out (`SpellInfo.cpp:718-742`). This
specialisation has no such replacement except Dark Soul, so Demonology is the one warlock specialisation not hurt by it; the
aura-based Metamorphosis swaps work through the same function.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Doom is learned at 36, Touch of Chaos at 25, the Felguard at 42, Felstorm at 48, Immolation Aura at
  62, Molten Core at 69, Chaos Wave at 79, Dark Soul: Knowledge at 84, Grimoire of Service at 75; below those levels the lists
  above start without them.
- Metamorphosis prevents Corruption until level 36 and Hand of Gul'dan until level 79 (spell text); the guides, written for 90,
  never mention it.
- Imp Swarm is a glyph spell here as in the guides, but its learn level in the data is 32 and the glyph slot opens at 25.
- Soul Link is a passive (above); both guides call it passive, but the bots' code treats it as a cast.
- Method's Fury thresholds (900 in, 300 out) are for long boss fights; a dungeon pack lasts 20 to 40 s, so 500 in is a
  reasonable bot compromise (derived), but the exit at 50 is not the guide's.
- Burning Rush has no code in the bots; Method's pick is therefore inert.

## Gap list (ordered)

Config (can go at once):

1. Glyph set and order: `PremadeSpecGlyph.9.1` (`conf/playerbots.conf.dist:2877`) to `42455,42461,42460,42457,43389,42465`
   (Siphon Life, Health Funnel, Demon Training, Nightmares, Unending Breath, Imp Swarm). Reason: Imp Swarm and Hand of
   Gul'dan glyphs do nothing for a bot that cannot cast Imp Swarm or aim Hand of Gul'dan; move Imp Swarm up once the action
   exists (see Glyphs).
2. Tier 75: Grimoire of Supremacy (`111102`) until a Service action exists, otherwise keep (owner question 1).
3. Stat weights (gear scoring), applies to all three warlock sheets: Intellect above Spirit and Spirit to 0, Mastery below
   Haste, Hit and Critical until 80 (`Mgr/Item/StatsWeightCalculator.cpp:527-538`, `:722-726`).

Strategy (class fixes, wait for the owner's in-game test):

4. Remove the `soul link` edges (`GenericWarlockStrategy.cpp:102`, `GenericWarlockNonCombatStrategy.cpp:77`) and the trigger
   `SoulLinkTrigger` (`WarlockTriggers.h:20`): the talent is a passive, there is nothing to cast.
5. Do not add the `curse of agony` strategy to Demonology (`AiFactory.cpp:503`): Agony is an Affliction spell.
6. Silence the unknown-name loop (the level 25 log): the list in the findings above; verify by counting `resolved=0` lines per
   bot per minute in the debug log.
7. Metamorphosis entry: not before level 36, only after the Hand of Gul'dan charges are spent (or at 2 charges of waste), at
   500 or more Fury with a pack of 3 or a boss, and leave at 300 or under rather than 50 (`DemonoWarlockStrategy.cpp:71`,
   `WarlockTriggers.cpp:239-248`).
8. Imp Swarm action (glyph spell 104316) on a pack of 3 or a boss, on cooldown: new action in `WarlockActions.h` plus an edge
   in `DemonoWarlockStrategy::InitTriggers`. Both guides have it on cooldown.
9. Summon Doomguard (58) or Infernal (49) on a boss or elite pack: a trigger for the known spell and a single-target edge,
   not through the AoE edge; fix the Infernal fallback for the combat engine (`GenericWarlockNonCombatStrategy.cpp:39`).
10. Drain Life at "medium health" (`GenericWarlockStrategy.cpp:106`): move it to "low health" so a group warlock keeps its
    rotation at 65 percent; same list holds a Soulburn that is Affliction only.
11. Hellfire from 4 targets (Method; `GenericWarlockStrategy.cpp:164-170` is "medium aoe", 3 targets); keep the caster out of
    melee range of the pack unless the tank holds it (`MetaMeleeAoeStrategy`, `DemonoWarlockStrategy.cpp:93-100`).
12. Felstorm and Axe Toss: find out whether the pet casts them by autocast; if not, add pet commands.
13. Grimoire of Service action (Felguard, Felstorm) if tier 75 stays Service.
14. Carrion Swarm (47) as an interrupt in the form; Soul Fire stacking rule at 9 Molten Core stacks (expert).
15. Skill tiers: the warlock strategies have no tier gating (`BotSkill` is used only in the tank code). Lines marked good and
    expert above (Fear, Mortal Coil, Imp Swarm, Felstorm, Fury thresholds, Hand of Gul'dan before the form) are the ones a
    casual bot should drop.
16. To check live: BT-060 to BT-064 (deployed, live check pending); that `reach melee` after Immolation Aura keeps the caster
    behind the tank; that the glyph-taught Imp Swarm is castable at 25 to 31 although its spell level is 32.

Owner questions: (1) Tier 75: keep Grimoire of Service (guide default, needs a new action) or Supremacy (works today).
(2) Sacrificial Pact or Soul Link at tier 45 (Soul Link has no proven effect on this core). (3) Tier 30: Mortal Coil
(interrupt, premade) or Shadowfury (Method dungeon default). (4) Should Metamorphosis wait for level 36 and for the Hand of
Gul'dan charges. (5) May a Demonology bot walk into melee for Immolation Aura in a dungeon.

## PvP play (player guides, 5.x, read 2026-11-19)

- **Sources:** three player guides for Demonology: a 5.4.7 burst guide, a 5.4 2v2 guide with an annotated game, and a 5.1 to 5.2 rated battleground guide. No coach course. Facts only, nothing copied; data checked against this server's 5.4.8 tables.
- **Coverage:** well covered (burst, crowd control chain, fight flow). Opener, positioning and interrupts are thin. The battleground guide is a different mode (many targets) and is not used for arena rules. No Demonology player was in the 2026-11-18 ladder sample.

**The plan.** Demonology has no sustained damage, only burst. A game is a sequence of bursts: build Demonic Fury (300 is the floor, about 400 with Mannoroth's Fury and Immolation Aura, 600 to 800 for a kill burst); burst once to force cooldowns (trinket, Life Swap, Deterrence, Pain Suppression); then survive, control and rebuild Fury; burst again for the kill. Against two damage dealers the first burst is early and fast; against one damage dealer and a healer it is a long positional game.

**The burst.** Wait for the trinket to proc (apply Curse of the Elements and Corruption, Fel Flame until it procs), then one macro: on-use trinket, Dark Soul: Knowledge, Metamorphosis, then Touch of Chaos spam. Never Soul Fire in the burst. Chaos Wave and Touch of Chaos deal chaos damage, which ignores a Death Knight's Anti-Magic Shell and a rogue's Cloak of Shadows. Hand of Gul'dan makes Fury and keeps ticking, so a burst can start from 200 Fury.

**The chain.** The second demon of Grimoire of Service is a free instant control: Felguard stuns, Felhunter interrupts and silences, Succubus seduces a humanoid, Imp cleanses a magic effect from the warlock. Pick it so it does not share diminishing returns with the partner (a mage partner who counterspells and Deep Freezes: use the Imp; on a caster damage dealer: Felhunter). Typical chain: second demon stun, Howl of Terror at the end of the stun, the target trinkets, main demon stun, Mortal Coil, cast Fear with 1.5 s of the coil left. Time each control to the last 0.1 s of the previous one. Against a priest who Dominates your healer, burst the priest while it channels, after removing its Fear Ward.

**Defensives.** Dark Regeneration, Dark Bargain (preferred when the pet is an Imp or Felhunter, because Sacrificial Pact would be tiny), Unbound Will, Demonic Leap and Teleport to kite, Gateway only when needed, Unending Resolve to cast free against interrupts. Reset an arena by running far, tapping mana and letting the healer drink under cover of crowd control. GAP: cooldown trading rules.

**Talents and glyphs, guides against the set** (`PremadeSpecLinkPvp.9.1 = 020000`, glyphs `50077,42461,42462,42457,43389,45782`, copied from Affliction and Destruction):

| Tier | 2v2 guide | Set |
|---|---|---|
| 15 | Dark Regeneration | Dark Regeneration |
| 30 | Mortal Coil | Shadowfury |
| 45 | Dark Bargain | Soul Link |
| 60 | Unbound Will | Blood Horror |
| 75 | Grimoire of Service (gear dependent; Sacrifice scales better later) | Grimoire of Supremacy |
| 90 | Mannoroth's Fury | Archimonde's Darkness |

Glyphs the guides want: Demon Training (with an Imp), minor Glyph of Hand of Gul'dan (aimed meteor). Stat order (steers rolls only): the 5.4.7 test found crit better than mastery once a shaman or paladin gives the mastery buff; earlier it was mastery all the way.

**Differences from the guides (5.4.8).** Howl of Terror is baseline (level 30), not a talent; Blood Fear is now Blood Horror; Grimoire of Sacrifice raises maximum health only with Soul Link (the 30 percent Voidwalker claim of the battleground guide); Demonic Leap (level 12) also switches on Metamorphosis; Demonic Gateway is learned at 87 and its data cooldown is 10 s, against the "long cooldown" of the guide (to check live). Level gates: Metamorphosis and Hand of Gul'dan 10 and 19, Touch of Chaos 25, Doom 36, Carrion Swarm 47, Chaos Wave 79, Dark Soul: Knowledge 84.

**Bots today (read from `src/strategy/Classes/warlock/`, ordered by value).**
1. Metamorphosis is cast only on a pack of three or more targets within 8 yd or on a boss target, needs 500 Fury and every Hand of Gul'dan charge spent, and is dropped at 300 Fury or less (`WarlockTriggers.cpp:316-328`, edge `DemonoWarlockStrategy.cpp:73`): a bot in an arena never enters it against a player. This is the whole burst of the spec.
2. No Grimoire of Service action, so no free stun, silence or seduce in a chain.
3. Fear on a player in melee range only (`GenericWarlockStrategy.cpp:123`); Howl only as the fallback of Fear (`:54`); Mortal Coil only as an interrupt (`:116`).
4. Mannoroth's Fury plus Immolation Aura only in the area group (`:171-172`).
5. No Demonic Leap or Demonic Circle code; no Fury floor before a burst.
6. Dark Soul: Knowledge at level 84 and over on the boost strategy (`:147`), not tied to a trinket proc. Touch of Chaos, Chaos Wave and Hand of Gul'dan exist as low defaults (`DemonoWarlockStrategy.cpp:54-57`); Soul Fire only on Molten Core (`:90`).
7. Carrion Swarm in Metamorphosis and the pet's Spell Lock as interrupts exist (`:77`, `GenericWarlockStrategy.cpp:115`); defensives as for Destruction.

**Research update (public patch notes 5.1 to 5.4.7 and spell data, 2026-11-19; lines marked derived come from game knowledge and the 5.4.8 data).** Mannoroth's Fury is an active spell in 5.4 (10 s, 60 s cooldown, +500 percent area and +100 percent damage on Hellfire, Immolation Aura, Rain of Fire and Seed), so the guide's extra damage is a window to line up with the burst; Dark Soul: Knowledge adds 30 to the Mastery effect; Hellfire and Immolation Aura lost 20 percent; Doom gained 25 percent in 5.1; the Demonic Gateway debuff lasts 60 s; Felguard Legion Strike's healing reduction lasts 6 s since 5.4.7. Data: Fear 1.7 s, 30 yd; Demonic Circle: Teleport 30 s, 40 yd; Metamorphosis 10 s cooldown. Derived: build Fury at range with Shadow Bolt, Hand of Gul'dan and Wild Imps, do not enter Metamorphosis before the go; the second demon of Grimoire of Service is chosen by the partner's control categories; only Immolation Aura needs melee range, so no bot should walk into a melee cleave for it; Fear and Howl cap at 8 s on players and diminish together with Seduction. Extra gap lines: Mannoroth's Fury is cast only in the area group (`GenericWarlockStrategy.cpp:171`); no pet command (Axe Toss, Command Demon) is used for a stun.

**Proposals.** Config (no code): the 2v2 guide's tiers (Mortal Coil, Dark Bargain, Unbound Will, Grimoire of Service, Mannoroth's Fury) need Service code first; until then tiers 30, 45, 60 and 90 can follow the guide (Mortal Coil, Dark Bargain, Unbound Will, Mannoroth's Fury) and tier 75 stays Supremacy. Add Glyph of Demon Training only with an Imp. Rotation (waits for the owner's in-game test): a player-burst Metamorphosis rule with a 300 Fury floor, Grimoire of Service chain, Mannoroth's Fury in single target.

**Open questions.** (1) Tier 75: Service or Supremacy. (2) Whether Metamorphosis for a duel should wait for level 36 and a trinket proc. (3) Whether the Demonic Gateway cooldown on this core is 10 s.
