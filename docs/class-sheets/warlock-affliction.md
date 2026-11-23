# Warlock: Affliction (damage)

- **Status:** checked against 5.4.8 data. Implemented, compile and live check pending: the owner approved gap list
  (2026-11-11) and the skill tiers of the priority lines (2026-11-16, `Utils/DpsSheetPolicy.h`). This pass added no
  Affliction rotation change. The "bots today" columns below describe the code before the 2026-11-11 implementation.
- **Sources:**
  - Method, "Affliction Warlock Class Guide, MoP Classic" (Phase 1, level 90 raid and dungeon, by Devorta, 23 July 2025), read
    2026-11-11 through a scripted fetch (the talent build is an image: only the talent hints in the text are usable).
  - wowtbc.gg, "Affliction Warlock PvE Rotation & Talent Build (MoP Classic)" (no patch line on the page), read 2026-11-11.
  - The server's own data (`data/dbc/`: Spell, SpellLevels, SpecializationSpells, SkillLineAbility, Talent, GlyphProperties,
    SpellCooldowns, SpellPower, SpellEffect, SpellMisc, SpellCategories, SpellCategory; `data/db2/Item-sparse.db2` for glyph
    item ids) through `spec_data.py` and short read-only scripts; the core source (the SkyFire core) for the
    spell scripts.
  - Not readable: Overgear and Icy Veins (HTTP 403 to a scripted fetch, retried 2026-11-11), Wowhead (needs a browser; its leveling
    page answers 404).
- **Levels covered:** 15 to 60 in detail (leveling and 5-player dungeons; a damage dealer follows the leader and the tank's
  marks), 61 to 90 briefly.

Which source was richer: **talents (core picks, dungeon picks), glyphs for dungeons, stat order, the haste breakpoints**
Method (wowtbc lists every talent option without a pick and has no stat order); **the 1-3 against 4+ target pet choice, the
defensive order, all talent options, the minor glyphs** wowtbc; **levels 15 to 60, what to do before Haunt, Agony or Malefic
Grasp is learned, pets while leveling** neither guide says anything (both are level 90 raid guides), so those lines come from
the 5.4.8 data and are marked "derived". The two guides agree on the single target order; they differ on when to refresh a DoT
(Method at about 40 percent left, wowtbc below half or earlier for procs) and on Drain Soul (Method only in the execute range,
wowtbc also when Haunt is not up); I use Method's 40 percent as the expert line and the plain "when it is missing or about to
end" as the core line, because the bots' DoT triggers cannot read a pandemic window.

## Talents

Tiers and columns are from Talent.dbc (the three warlock specialisations share one list). "Bots today" is
`AiPlayerbot.PremadeSpecLink.9.0 = 111101` (`conf/playerbots.conf.dist:2871`; one digit per tier, column counted from 0): Soul
Leech, Mortal Coil, Sacrificial Pact, Burning Rush, Grimoire of Supremacy, Kil'jaeden's Cunning.

| Tier (level) | Guide pick | Alternative and when | Spell id | Bots today (111101) |
|--------------|------------|----------------------|----------|---------------------|
| 1 (15) | Soul Leech (Method, core) | Dark Regeneration, Harvest Life (wowtbc lists them) | 108370 / 108359 / 108371 | Soul Leech, same (passive, no code needed) |
| 2 (30) | Mortal Coil (Method raid default); Shadowfury in dungeons (Method) | Shadowfury for pack stuns; Demonic Breath | 6789 / 30283 / 47897 | Mortal Coil, same as the raid pick; used as an interrupt on a casting enemy (`GenericWarlockStrategy.cpp:115`). Shadowfury also has code (`:168`) |
| 3 (45) | Soul Link (Method default) | Sacrificial Pact for burst (Method); Dark Bargain | 108415 / 108416 / 110913 | **Sacrificial Pact, differs.** Soul Link is a passive and nothing in the core applies its pet side (see "Soul Link" below), so Sacrificial Pact (coded at low health, `:107`) is the safer pick |
| 4 (60) | Burning Rush (Method, core) | Blood Horror, Unbound Will | 111400 / 111397 / 108482 | Burning Rush, same. **Inert:** a toggle that costs health every second; no action exists for it, so it is never cast |
| 5 (75) | Grimoire of Supremacy (Method, core) | Grimoire of Service, Grimoire of Sacrifice | 108499 / 108501 / 108503 | Grimoire of Supremacy, same; passive, the summon spells are swapped to the stronger demons by the core |
| 6 (90) | Archimonde's Darkness (Method default) | Mannoroth's Fury at 6 or more targets; Kil'jaeden's Cunning for heavy movement | 108505 / 108508 / 137587 | Kil'jaeden's Cunning, differs (level 90 only). Mannoroth's Fury has code (`:164`) |

Notes on the picks (data, not guide):

- Soul Leech (108370) shields the warlock for a share of the damage of listed spells; for Affliction the list is Malefic Grasp,
  Drain Soul, Fel Flame and Haunt (spell text). So from 15 to 41 only Drain Soul feeds it (Malefic Grasp is learned at 42, Haunt
  at 62) and the talent does almost nothing until 42 (derived). Dark Regeneration (108359) heals 2 percent per second for 12 s
  and has code (`GenericWarlockStrategy.cpp:100`, `:109`); it is the better pick for 15 to 41 and Soul Leech after that, but a
  premade link is one fixed choice (owner question 3).
- Mortal Coil (6789): the target runs in horror for 3 s and the caster restores health (spell text and effect data); a feared
  mob cannot finish its cast, which is why it works as a bot interrupt.
- Sacrificial Pact (108416): the demon gives 25 percent of its current health and the warlock gets a shield of four times that
  (script `spell_warlock.cpp:3568`; the warlock pays the health when there is no demon). Cooldown 60 s.
- Burning Rush (111400): 4 percent of maximum health per second for 50 percent more speed (effect data). A bot that never
  casts it loses nothing; a bot that casts it would lose health while moving, so do not add code for it without a health gate.

Proposed link, guide aligned and safe for the code that exists: **`111101` unchanged** (the only differences from Method are
Sacrificial Pact for Soul Link, with the reason above, and a level 90 tier). A change to Dark Regeneration at tier 15
(`011101`) is an owner choice (question 3).

## Glyphs

Types from GlyphProperties.dbc (apply spell effect 74, 0 major, 1 minor), item ids from `Item-sparse.db2`, effect texts from
Spell.dbc. Slots unlock in pairs at 25, 50 and 75 (config comment `conf/playerbots.conf.dist:2631-2636`; the unlock levels are
core-side). The config order is major, minor, major, minor, minor, major; a glyph goes to the first free slot of its type.

| Type | Glyph | Item id | Why (one clause) | Bots today (`42472,43394,42462,42457,43389,42455`) |
|------|-------|--------:|------------------|----------------------------------------------------|
| Major | Unstable Affliction | 42472 | both guides; Unstable Affliction cast time down by 25 percent, learned at 10 | yes, slot 1 (level 25) |
| Major | Siphon Life | 42455 | both guides; Corruption heals the warlock on each tick (spell text), a free self-heal for a bot | yes, slot 3 (level 75), **late** |
| Major | Healthstone | 42462 | both guides; healthstone heals twice as much but over time (spell text) | yes, slot 2 (level 50). Derived doubt: the bots eat the stone at 25 percent health, where a heal over time is slow |
| Major, alternative | Eternal Resolve | 50077 | Method alternative; Unending Resolve can no longer be cast but gives a passive damage cut (effect data: 10) | no |
| Major, alternative | Soulstone | 42470 | Method alternative; a soulstone resurrection returns more health | no |
| Major, alternative | Demonic Circle | 45782 | Method alternative; not used by the bots | no |
| Major, AoE | Curse of the Elements | 42464 | wowtbc; the curse hits additional nearby targets | no |
| Minor | Soulwell | 43394 | wowtbc; cosmetic ("glows"); Create Soulwell is learned at 68 and the bots never cast it | yes, slot 1 (level 25) where it does nothing |
| Minor | Nightmares | 42457 | wowtbc; the mounts cross water and leave flames | yes, slot 2 (level 50) |
| Minor | Unending Breath | 43389 | swim speed of Unending Breath targets; the bots cast Unending Breath when swimming | yes, slot 3 (level 75) |
| Minor, derived | Health Funnel | 42461 | Health Funnel restores the demon at once, 10 s cooldown; the bots cast Health Funnel on a hurt demon (`GenericWarlockStrategy.cpp:99-100`) | no |

Disagreement: Method lists three majors and no minor, wowtbc adds Soulwell and Nightmares as minors and Curse of the Elements for
AoE. The premade follows wowtbc. Proposed `PremadeSpecGlyph.9.0` (derived order: a glyph in a slot must do something at the
level the slot unlocks, and the strongest self-heal should come early): `42472,42461,42455,42457,43389,42462` = Unstable
Affliction, Health Funnel, Siphon Life, Nightmares, Unending Breath, Healthstone (Soulwell dropped, Siphon Life moved from
slot 3 to slot 2, Healthstone to the last major slot).

## Stat order

Method: Intellect, then Hit and Expertise to 15 percent chance to hit, then Haste (breakpoints at 6637, 9778, 13737 rating),
then Mastery, then Critical Strike. wowtbc has no stat order. The caps are level 90 numbers and cannot be reached with the gear
of levels 15 to 60.

For loot rolls and quest rewards below level 90 (derived, with the reason):

- **Intellect first** (the primary stat; Spell Power is not an item stat in 5.x, inference: the data here does not list item
  stats).
- Then Haste (DoT ticks and Malefic Grasp ticks both speed up), then Hit, then Critical Strike. **Mastery last below 80**:
  Mastery: Potent Afflictions is learned at 80 (SpecializationSpells), so before that a Mastery rating does nothing.
- Spirit is in neither guide and a warlock has no spell that scales with it (inference); it ranks last.
- Expertise is on Method's line but a caster's own spells ignore it (inference); skip it.
- Bots today (`Mgr/Item/StatsWeightCalculator.cpp:527-538`, shared by all three warlock specialisations): Intellect 0.3,
  **Spirit 0.6**, Spell Power 1.0, Hit 1.1, Haste 1.0, Critical 0.8, Mastery 0.87. Against the guide: **Spirit outranks
  Intellect two to one** (a cloth item with Spirit beats one with Intellect), and Mastery is above Critical although it is
  inactive before 80. The Spirit penalty of 0.4 applies only until Fel Armor (level 38) is known (`:722-726`).

## What the specialisation knows, by level (5.4.8 data)

Ids are the spells the player actually casts; the data holds many old rows of the same name. Costs are percent of base mana.
The global cooldown is 1.5 s.

| Level | Spell (id) |
|------:|------------|
| 1 | Shadow Bolt (686, 1.7 s cast, 5.5 percent), Summon Imp (688) |
| 3 | Corruption (172, instant, 1.25 percent) |
| 7 | Drain Life (689, channel 6 s) |
| 8 | Summon Voidwalker (697) |
| 9 | Create Healthstone (6201, 3 s cast) |
| 10 | Unstable Affliction (30108, 1.5 s cast, 14 s, 1.5 percent); the specialisation is chosen here |
| 11 | Health Funnel (755) |
| 14 | Fear (5782, 1.7 s cast) |
| 16 | Life Tap (1454, instant: health for mana) |
| 17 | Curse of Enfeeblement (109466: less physical damage dealt, slower casting) |
| 18 | Soulstone (20707, 3 s cast, 10 min) |
| 19 | Drain Soul (1120, 12 s channel; stronger on a target under a health threshold, shard on death), Soulburn (74434, spends a shard), Soul Shards (117198) |
| 20 | Summon Succubus (712) |
| 21 | Rain of Fire (5740, 6 s channel, 8 percent) |
| 24 | Unending Breath (5697) |
| 29 | Summon Felhunter (691) |
| 30 | tier 2 talent |
| 32 | Banish (710), Curse of Exhaustion (18223) |
| 34 | Twilight Ward (6229) |
| 36 | Agony (980, instant, 24 s, builds up per tick) |
| 38 | Fel Armor (104938, passive: health) |
| 42 | Malefic Grasp (103103, 4 s channel, replaces Shadow Bolt: every tick makes the other periodic Affliction effects tick at once), Ritual of Summoning |
| 45 | tier 3 talent |
| 49 | Summon Infernal (1122, 10 min cooldown shared with Doomguard) |
| 51 | Curse of the Elements (1490, 5 percent more magic damage taken, 5 min) |
| 54 | Nightfall (108558, passive: Corruption ticks can return a Soul Shard) |
| 56 | Command Demon (119898) |
| 58 | Summon Doomguard (18540) |
| 60 | Seed of Corruption (27243, 2 s cast, 6 percent); tier 4 talent |
| 62 | Haunt (48181, 1.5 s cast, spends a shard, +periodic damage for 8 s) |
| 64 | Unending Resolve (104773, 3 min, 8 s: 40 percent less damage taken, no interrupts) |
| 66 | Soulshatter (29858, threat) |
| 68 | Create Soulwell (29893) |
| 73 | Soulburn: Curse (103112) |
| 75 | tier 5 talent |
| 76 | Demonic Circle: Summon and Teleport (48018, 48020) |
| 77 | Fel Flame (77799) |
| 79 | Soul Swap (86121) and Soulburn: Soul Swap (141931) |
| 80 | Mastery: Potent Afflictions (77215); Dark Intent 82 |
| 84 | Dark Soul: Misery (113860, 2 min, 20 s of haste) |
| 87 | Demonic Gateway (111771) |
| 90 | tier 6 talent; Pandemic (131973) |

Pet abilities that matter for the bands: Voidwalker Suffering 15 and Shadow Shield 28, Felhunter Devour Magic 38 and Spell Lock
50 (24 s interrupt), Succubus Seduction 26, Imp Singe Magic 68.

## Priority lists

Tier of a line: **core** (every bot), **good** (good and expert bots), **expert** (expert bots only). A line marked "derived"
comes from the 5.4.8 data and the reasoning in the line, not from a guide. File:line refer to `mod-aware-bots-548/src/strategy/
Classes/warlock/` at commit `52b7750`.

How the bots rank what they do (read, `strategy/Strategy.h:37-51`): ACTION_DEFAULT 5, ACTION_NORMAL 10, ACTION_HIGH 20,
ACTION_INTERRUPT 40, ACTION_EMERGENCY 90. A trigger edge adds its action with that number; the engine takes the highest action
that is possible and useful; an action whose spell is unknown is simply skipped. An Affliction warlock gets the `affli`
combat strategy plus `curse of agony`, `dps assist`, `dps`, `aoe` and `cc` (`Factory/AiFactory.cpp:510-517`) and, outside
battlegrounds, `boost` (`:316`); out of combat `felhunter`, `dps assist` and `ss self` (`:758-775`). Health thresholds in
config: critical 25, low 45, medium 65 (`conf/playerbots.conf.dist:670-672`); the "target critical health" trigger is 20 percent
(`strategy/triggers/HealthTriggers.h:138`).

### Levels 15 to 29

Known: Shadow Bolt, Corruption, Unstable Affliction, Drain Life, Fear, Life Tap, Soulstone, Drain Soul, Soulburn, Rain of Fire,
pets Imp, Voidwalker (8), Succubus (20).

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 1 | core | out of combat, no demon | Summon Voidwalker (697, 8): it has the tanking tools (Suffering 15, Shadow Shield 28) that the Succubus lacks (derived) | **differs:** the `felhunter` strategy (`AiFactory.cpp:761`) walks a chain of alternatives (`GenericWarlockNonCombatStrategy.cpp:37-46`): Felhunter, else Succubus (from 20), else Voidwalker (8 to 19). So 20 to 28 is a Succubus |
| 2 | core | out of combat, no healthstone | Create Healthstone (6201, 9) | yes (`GenericWarlockNonCombatStrategy.cpp:72`) |
| 3 | core | out of combat, no soulstone aura | Soulstone on self (20707, 18) | yes (`:775` strategy `ss self`) |
| 4 | core | target has no Corruption | Corruption (172, 3) | yes, edge 18.0 (`AffliWarlockStrategy.cpp:67`), default 5.5 (`:47`); on other attackers 19.5 (`:65`) |
| 5 | core | target has no Unstable Affliction | Unstable Affliction (30108, 10) | yes, edge 17.5 (`:68`), default 5.4; on other attackers 19.0 (`:66`) |
| 6 | core | filler | Shadow Bolt (686, 1) | yes: `malefic grasp` 5.2 falls back to `shadow bolt` while Malefic Grasp is unknown (`:15`, `:50`) |
| 7 | core | target at or under 20 percent health | Drain Soul (1120, 19) | yes, 15.5 (`:78`, trigger `HealthTriggers.h:138`) |
| 8 | good | mana low, health above 45 percent | Life Tap (1454, 16) | yes, but late: "low mana" (15 percent) at 95 (`GenericWarlockStrategy.cpp:103`) and "mana under 85" at 5.1 (`AffliWarlockStrategy.cpp:84`, trigger `WarlockTriggers.cpp:125`), which only runs when nothing else can be cast |
| 9 | good | health at or under 25 percent | Healthstone (item), then Drain Life (689, 7) | yes, `healthstone` 41 (`GenericWarlockStrategy.cpp:108`, BT-060), `drain life` 40 (`:109`) |
| 10 | good | health under 65 percent in a group | nothing special: the healer covers; Drain Life only below 45 percent (derived) | **differs:** "medium health" casts Drain Life at 40, above every DoT (`:106`), so a warlock at 64 percent stops its rotation for a 6 s channel |
| 11 | good | a second attacker or a marked target | Fear (5782, 14) | yes, `cc` strategy 33 (`:151`, BT-064) |
| 12 | good | demon hurt | Health Funnel (755, 11) | yes (`:99-100`) |
| 13 | good | 3 or more enemies within 8 yd of the target, from 21 | Rain of Fire (5740, 21) | yes, "medium aoe" 30 (`:170`); whether the channel lands on the pack is to check live (ground-targeted) |
| 14 | expert | DoT about to end | refresh at about 40 percent left (Method) | missing: `DebuffTrigger` is built with `beforeDuration` 0 (`strategy/triggers/GenericTriggers.h:402`), so a DoT is recast only when the aura is gone. It also skips a target that dies within 8 s (needLifeTime), which is sensible for trash |

Before a spell is learned (derived): below 10 there is no specialisation and no Unstable Affliction; the bot gets the
`affli` strategy only with the specialisation (`AiFactory.cpp:288-295` falls to `affli` for any non-Destruction non-Demonology
spec); from 10 to 18 the kit is Corruption, Unstable Affliction, Shadow Bolt, Drain Life, Fear.

### Levels 30 to 44

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 15 | core | target has no Agony, from 36 | Agony (980, 36) | yes, strategy `curse of agony`: 17.0, on attackers 18.5 (`GenericWarlockStrategy.cpp:181-182`) |
| 16 | core | filler, from 42 | Malefic Grasp (103103, 42) with the DoTs up (it makes them tick at once) | partly: default 5.2 (`AffliWarlockStrategy.cpp:50`), **but** the random `shadow bolt filler` edge (`:75`, 5.25, one tick in three, `WarlockTriggers.cpp:304-308`) keeps casting real Shadow Bolt after 42 (see "Spec replacements" below) |
| 17 | good | a casting enemy | Mortal Coil (6789, 30) if picked; the Felhunter's Spell Lock from 50 | yes, `mortal coil on enemy caster` 41 (`GenericWarlockStrategy.cpp:115`), `pet interrupt` 42 (`:114`) |
| 18 | good | 3 or more enemies, tier 2 = Shadowfury | Shadowfury (30283, 30) | yes if picked, "medium aoe" 32 (`:168`); **not picked in the premade** |
| 19 | good | a mob to hold off | Banish (710, 32) on an elemental or demon | yes, `cc` 32 (`:150`) |
| 20 | good | out of combat | Fel Armor (38) | nothing to cast: passive (BT-146 removed the request) |
| 21 | expert | demon out | Felhunter Spell Lock on a caster (from 50), Devour Magic (38) | `pet interrupt` yes (`WarlockActions.cpp:298-313`); Devour Magic not used |

### Levels 45 to 59

As above, plus:

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 22 | core | health at or under 45 percent | Sacrificial Pact (108416, 45) if picked, else Unending Resolve from 64 | yes (`GenericWarlockStrategy.cpp:107`) |
| 23 | good | out of combat, curse missing, on elites or bosses | Curse of the Elements (1490, 51) "only if assigned" (both guides), otherwise Enfeeblement | **differs:** every target gets Enfeeblement or the Elements, whichever is first, at 25 (`GenericWarlockStrategy.cpp:92-93`; the two triggers exclude each other, `WarlockTriggers.cpp:22-32`). For Affliction this is cheap (4 percent) but costs a global cooldown at every pull |
| 24 | good | a boss or an elite pack | Summon Doomguard (18540, 58) on cooldown, 10 minutes | **missing:** only the 3-enemy AoE edge asks for `summon abyssal`, and its fallback to Infernal lives in the non-combat node factory (`GenericWarlockNonCombatStrategy.cpp:39`), which the combat engine does not read (to check live) |
| 25 | good | threat high | Soulshatter (29858, 66) | yes, "medium threat" 55 (`GenericWarlockStrategy.cpp:97`) |
| 26 | good | demon dead or low | Health Funnel; Demonic Rebirth (54, passive) | see line 12 |

### Levels 60 to 74

| # | Tier | Condition | Spell (id, learned at) | Bots today |
|--:|------|-----------|------------------------|------------|
| 27 | core | Haunt missing and a shard | Haunt (48181, 62): keep it up in fights worth it | yes, 16.5 (`AffliWarlockStrategy.cpp:69`) and default 5.3; casts whenever the debuff is gone, with no shard rule |
| 28 | good | 3 or more enemies | Seed of Corruption (27243, 60); Method: above 5 targets with Soulburn | yes at 33 and 33.5 (`GenericWarlockStrategy.cpp:166-167`); starts at 3 targets, earlier than the guide |
| 29 | good | big damage incoming | Unending Resolve (104773, 64), 3 min | yes, low health (`:107`) |
| 30 | expert | 4 shards | Haunt outside burst (Method) | missing (no shard read) |
| 31 | expert | tier 4 | Burning Rush is a toggle | inert, see Talents |

### Levels 75 to 90, brief

| Level | Guide | Bots today |
|------:|-------|------------|
| 75 | Grimoire of Supremacy: the Observer replaces the Felhunter, a Terrorguard or Abyssal on cooldown | passive talent works; the cooldown demon runs through `summon abyssal ready` 21 (`GenericWarlockStrategy.cpp:119`, `WarlockTriggers.cpp:294-302`); the spell name `summon abyssal` (112921) is not in the spellbook, so to check live that the trigger can ever fire |
| 79 | Soul Swap spreads DoTs; Soulburn before it | missing (no Soul Swap, no Soulburn plan) |
| 82 | Dark Intent | yes, 60 out of combat (`GenericWarlockNonCombatStrategy.cpp:70`, `WarlockTriggers.cpp:231`) |
| 84 | Dark Soul: Misery on cooldown | yes, `boost` 28 (`GenericWarlockStrategy.cpp:143`) |
| 90 | Method single target: Observer, Dark Soul, Terrorguard, Agony, Corruption, Unstable Affliction, Haunt, Drain Soul execute, Malefic Grasp, Life Tap | everything but the cooldown demon and Soul Swap is there; Life Tap is a low-mana reaction, not a filler |

## Cooldowns and defensives

The guides say "on cooldown"; the timing for a dungeon bot is derived (trash fights last 20 to 40 s).

| Ability | Learned | Guide use | Derived timing for a dungeon bot | Bots today |
|---------|--------:|-----------|----------------------------------|------------|
| Dark Soul: Misery | 84 | on cooldown | the same | `boost` 28, on every ready cooldown |
| Summon Doomguard / Infernal | 58 / 49 | on cooldown (Terrorguard from Supremacy at 75) | a boss or an elite pack | missing (line 24) |
| Unending Resolve | 64 | first defensive | health under 45 percent | `:107` |
| Sacrificial Pact | 45 | burst fights | health under 45 percent | `:107` |
| Dark Bargain | 45 (talent) | not named | health under 65 percent | `:106` (talent not picked) |
| Healthstone | item | wowtbc defensive 2 | health under 25 percent | `:108` |
| Mortal Coil | 30 | wowtbc defensive 3 | casting enemy, or panic | interrupt use only |
| Drain Life | 7 | not in a guide | health under 45 percent | `:106` at 65 percent (too early) |

## Healer triage and mana (healers only)

Not applicable. Mana: Affliction relies on Life Tap, which turns health into mana and is on neither guide's opener but on both
priority lists as the downtime and movement spell. The mana lines the bots have: "low mana" at 15 percent at 95, the
`LifeTapTrigger` (mana under 85 percent and health above 45) at 5.1, and out of combat at 23 (`GenericWarlockNonCombatStrategy.
cpp:80`), plus the generic drink rule (`AiPlayerbot.DungeonDrinkMana = 50`). The Glyph of Life Tap (45785) makes Life Tap an
absorb instead of a cost, and the bots have a trigger for it (`life tap glyph buff`, `AffliWarlockStrategy.cpp:83`), but no
premade carries the glyph, so that edge never fires.

## Warlock-wide findings that touch this sheet

**Spell names the bots ask for that a warlock does not know (the level 25 debug log).** `SpellIdValue` logs
`[SID] ... qualifier='<name>' resolved=0` whenever a name matches no known, non-passive spell (`strategy/value/SpellIdValue.cpp:
147`), and the value is cached 20 s per name and bot (`:24`), so about 24 different unknown names would give the 500 lines in
seven minutes. From the code (inference, not checked against the log), the Affliction names a level 25 warlock asks for and
does not know are, by source:
`soul link` (passive, never resolves at any level; `GenericWarlockStrategy.cpp:102` at 95 in combat and
`GenericWarlockNonCombatStrategy.cpp:77`, a buff trigger with no known-spell test, so it re-arms every 5 s);
`curse of the elements` (learned 51; the generic edge `:93` has no known-spell test);
`dark soul: misery`, `dark soul: knowledge`, `dark soul: instability` (84; all three are in `boost`, `:143-145`, for every
specialisation, so two are never known even at 90);
`agony`, `agony on attacker` (Affliction 36, from the `curse of agony` strategy `:181-182`);
`malefic grasp` (42; the default `AffliWarlockStrategy.cpp:50`) and `haunt` (62; default `:49`, edge `:69`);
`seed of corruption`, `seed of corruption on attacker` (60), `shadowfury` and `mannoroth's fury` (talents), `immolation aura`
(Demonology only), `summon abyssal` (the Supremacy spell, not in any spellbook) from "medium aoe" (`:164-170`) and from
`summon abyssal ready` (`WarlockTriggers.cpp:294`, called every evaluation);
`mortal coil`, `dark bargain`, `sacrificial pact`, `dark regeneration` (talents not yet learned), `twilight ward` (34),
`unending resolve` (64) and `soulshatter` (66), each only while its trigger is active;
the pet chain names `summon felhunter` (29), `summon shivarra`, `summon voidlord`, and the `wrong pet` trigger
(`WarlockTriggers.cpp:145`) which, with a demon out, asks for `summon felhunter` on every evaluation while the Felhunter strategy is on and the spell is
not known (below 29). Fix direction: give the buff triggers a known-spell test, gate edges by level, delete `soul link`, and let
`WarlockSummonAbyssalTrigger` and `WrongPetTrigger` return early when the spell is unknown.

**Soul Link settled (data).** The talent spell 108415 has the passive attribute (Attributes 0x50040, bit 0x40), cast time 0 and
no aura of its own: effect 0 is a dummy on the caster, and its text is the text of 108446 ("part of all damage taken is split
with your demon, part of the damage you deal is converted to healing on you and your demon"). The core learns a passive at
once when the talent is taken (`Player.cpp:3773`, `:3913`), so there is nothing to cast. The bots cannot cast it either:
`SpellIdValue` skips passives (`SpellIdValue.cpp:63`). Their buff trigger therefore fires every 5 s forever (no aura named Soul
Link can exist). The effective aura is 108446 (level 20, area aura on the demon's owner), which has a spell script
(`spell_warlock.cpp:3374`) but, searching the core source and the client data, nothing grants or casts it (no SpellLearnSpell
row, no linked spell, no call): so whether the talent does anything on this core is unverified. Both bot edges should go.

**Spec replacements are not applied to the bots' casts (read, to check live).** In the real client path the server swaps a
spell for the specialisation's replacement (`SpellHandler.cpp:1531-1565`: Shadow Bolt to Malefic Grasp for Affliction at 42,
Shadow Bolt to Incinerate and Corruption to Immolate for Destruction, Life Tap to Chaotic Energy). The bots build the cast
themselves and swap only through `SpellInfo::OverrideSpellList` (`AI/PlayerbotAI.cpp:3729-3737`), and in this core the code that
fills that list is commented out (`SpellInfo.cpp:718-742`). So an Affliction bot's `shadow bolt` is the real Shadow Bolt at all
levels, which matters for the random filler edge (line 16). The aura-based swaps (Metamorphosis) do work through the same
function.

## Differences from the guide (5.4.8 against MoP Classic)

- The guides are level 90. Haunt is learned at 62, Agony at 36, Malefic Grasp at 42, Seed of Corruption at 60, Soul Swap at 79,
  Dark Soul: Misery at 84, Grimoire of Supremacy at 75; below those levels the lists above start without them.
- Agony arrives at 36: Affliction has only Corruption and Unstable Affliction as DoTs from 10 to 35.
- Soul Link is a passive in the data (above); both guides call it passive too, so this is not a difference, but the bots'
  code treats it as a cast.
- Method's "Haunt 1 second before the pull" and wowtbc's "pre-cast Haunt" need Haunt (62); before that the opening cast is
  Unstable Affliction (1.5 s), which the pull action does not use: `PullStrategy(botAI, "shadow bolt")`
  (`WarlockAiObjectContext.cpp:37`, used only when a pull is requested).
- Burning Rush has no code in the bots; Method's pick is therefore inert.
- The bots' `PremadeSpecName` labels follow the old WotLK names; nothing reads them.

## Gap list (ordered)

Config (can go at once):

1. Glyph set and order: `PremadeSpecGlyph.9.0` (`conf/playerbots.conf.dist:2872`) to `42472,42461,42455,42457,43389,42462`.
   Reason: Soulwell does nothing before 68 and the bots never cast it; Siphon Life should be in the first half; Health Funnel
   is a working minor for the bots' own pet-heal code.
2. Stat weights (gear scoring), applies to all three warlock sheets: Intellect above Spirit and Spirit to 0, Mastery below
   Haste, Hit and Critical until level 80 (`Mgr/Item/StatsWeightCalculator.cpp:527-538`, `:722-726`).
3. Tier 90: Archimonde's Darkness (last digit 0, `111100`) is Method's default; level 90 only,
   low value. Tier 15: Dark Regeneration for 15 to 41 is an owner choice.

Strategy (class fixes, wait for the owner's in-game test):

4. Remove the `soul link` edges: `GenericWarlockStrategy.cpp:102` and `GenericWarlockNonCombatStrategy.cpp:77`; the trigger
   `SoulLinkTrigger` (`WarlockTriggers.h:20`), the action and the node can go too. It is a passive: no cast exists.
5. Silence the unknown-name loop (the level 25 log): known-spell test on `curse of the elements`, `dark soul: *` (cast only the
   one of the bot's specialisation), the `summon abyssal ready` and `wrong pet` triggers, and level gates for `agony` before
   36 (files above). Verify with the debug log: the count of `resolved=0` lines per bot should fall to near zero.
6. Pet by level: Voidwalker for 8 to 28 instead of the Succubus (`GenericWarlockNonCombatStrategy.cpp:37-46`, alternative
   chain; derived). Felhunter from 29 as today.
7. Drain Life at "medium health" (`GenericWarlockStrategy.cpp:106`): move it to "low health" (45 percent) so a group warlock
   keeps its DoTs up at 65 percent; same for the Soulburn in the same list, which spends a shard for nothing there
   (Soulburn only empowers the next Drain Life, Fear, curses, Seed or Soul Swap; derived from the spell text).
8. The random Shadow Bolt filler (`AffliWarlockStrategy.cpp:75`, `WarlockTriggers.cpp:304`): stop it from 42 (Malefic Grasp
   is the filler; with no spec swap the bot casts the weaker real Shadow Bolt on one tick in three).
9. Summon Doomguard (58) or Infernal (49) on a boss or elite pack, on cooldown: a trigger for the known spell and a
   single-target edge, not through the AoE edge (`GenericWarlockStrategy.cpp:154-171`, `GenericWarlockNonCombatStrategy.cpp:39`).
10. Spec replacements for bot casts: apply the `SpecializationSpells` swap in `BotOverrideSpell`
    (`AI/PlayerbotAI.cpp:3729`) the way `SpellHandler.cpp:1531-1565` does. Affects all three warlock specialisations.
11. Haunt rule: spend at 4 shards or when a fight is worth it (expert); Soul Swap plan from 79 (expert).
12. Seed of Corruption from 4 or more targets (`GenericWarlockStrategy.cpp:166-167`, medium aoe is 3 targets).
13. Skill tiers: the warlock strategies have no tier gating (`BotSkill` is used only in the tank code, per the mage sheets).
    Lines marked good and expert above (Fear, Banish, Mortal Coil, pet interrupt, Rain of Fire, Haunt rule, DoT refresh
    window) are the ones a casual bot should drop.
14. To check live: BT-060 to BT-064 (deployed, live check pending); Rain of Fire as a ground target from a unit action;
    whether `summon abyssal` can resolve; that `soulstone` on self works with `ss self`; the summon fallback chain at 20 to 28.

Owner questions: (1) Sacrificial Pact (works) or Soul Link (Method default, no effect proven on this core) at tier 45.
(2) Tier 30: Mortal Coil (interrupt, premade) or Shadowfury (Method dungeon default, pack stun). (3) Dark Regeneration at
tier 15 for 15 to 41, Soul Leech after. (4) Are minors that the bots can use (Health Funnel, Unending Breath) preferred to the
guide's cosmetic ones. (5) Doomguard on bosses only, or on every pack.

## PvP set (ladder check, 2026-11-18)

- **Source:** MoP Classic EU ladder, top 100, 2026-11-18. 3v3: 10 players of this specialisation (rating 2436 to 2917). 2v2: 4 players (rating 2655 to 2762). Shares are the share of that bracket's players; pooled counts add both brackets (a player listed in both counts twice). No rated battleground data.
- **Rule:** a pick changes when 60 percent or more of the deciding players hold another one and it exists on 5.4.8; a split pick stays when it is one of the top two. The deciding count is 3v3 when it has 5 or more players, otherwise 3v3 plus 2v2 pooled; under 5 even pooled, a set is only noted. The module has one set per specialisation (duels are 1v1, nearer to 2v2; skirmishes nearer to 3v3): where the brackets split, the 3v3 pick stays and the 2v2 one is noted.
- **Deciding count here:** 3v3 (10 players)
- **BT-324 result:** updated (BT-450); 2v2 agrees
- **Set now:** `PremadeSpecLinkPvp.9.0 = 020200` (was `020000`), `PremadeSpecGlyphPvp.9.0 = 42472,42461,50077,42457,43394,42455` (was `42472,42461,50077,42457,43389,42462`)

| Tier (level) | Ladder picks: 3v3 / 2v2 / pooled | Set before | Set now | Result |
|---|---|---|---|---|
| 1 (15) | Dark Regeneration 100% / 100% / 14 of 14 | Dark Regeneration | Dark Regeneration | matches |
| 2 (30) | Shadowfury 100% / 100% / 14 of 14 | Shadowfury | Shadowfury | matches |
| 3 (45) | Soul Link 100% / 100% / 14 of 14 | Soul Link | Soul Link | matches |
| 4 (60) | Unbound Will 80% / 100% / 12 of 14, Blood Horror 20% / 0% / 2 of 14 | Blood Horror | Unbound Will | changed (8 of 10) |
| 5 (75) | Grimoire of Supremacy 90% / 100% / 13 of 14, Grimoire of Sacrifice 10% / 0% / 1 of 14 | Grimoire of Supremacy | Grimoire of Supremacy | matches |
| 6 (90) | Archimonde's Darkness 100% / 100% / 14 of 14 | Archimonde's Darkness | Archimonde's Darkness | matches |

| Type | Glyph (item id) | 3v3 | 2v2 | Pooled | Before | Now |
|---|---|---|---|---|---|---|
| major | Siphon Life (42455) | 100% | 100% | 14 of 14 | no | yes |
| major | Eternal Resolve (50077) | 100% | 100% | 14 of 14 | yes | yes |
| major | Unstable Affliction (42472) | 80% | 100% | 12 of 14 | yes | yes |
| major | Healthstone (42462) | 20% | 0% | 2 of 14 | yes | no |
| minor | Health Funnel (42461) | 90% | 25% | 10 of 14 | yes | yes |
| minor | Soulwell (43394) | 60% | 75% | 9 of 14 | no | yes |
| minor | Nightmares (42457) | 50% | 0% | 5 of 14 | yes | yes |
| minor | Unending Breath (43389) | 0% | 0% | 0 of 14 | yes | no |

Ladder glyphs under 25 percent pooled that the set does not hold are left out of the table.

- **What changed (3v3, 10 players):** tier 60 is Unbound Will (80 percent in 3v3, 100 in 2v2) instead of Blood Horror; the major glyph Siphon Life (100/100; on 5.4.8 Corruption heals the warlock when it deals periodic damage, and the affliction rotation keeps Corruption up) replaces Healthstone (20/0); the minor glyph Soulwell (60/75, cosmetic) replaces Unending Breath (0/0). The other tiers and Eternal Resolve 100/100, Unstable Affliction 80/100 matched.
- **2v2 differs on two held minors:** Health Funnel is 90 percent in 3v3 and 25 in 2v2, Nightmares 50 and 0; both kept (pooled 10 of 14 and 5 of 14).
- **Rules:** since 95085ba the 1v1 engine breaks crowd control with Unbound Will (BT-452, part); the other battleground and group lanes have no Unbound Will rule yet. Soul Link (100 percent in both brackets, kept) is a passive in the 5.4.8 data and its pet side is not applied by the core (BT-345).

## PvP class package: bots today (BT-663 to BT-667, 2026-11-19)

Status of the digest's section 12 rules after the Affliction package (see `docs/pvp/class-packages.md`; all wait for the owner's in-game test):

| Digest rule | Bots today |
|---|---|
| 1 Soulburn Soul Swap spread | not built (the exhale needs a live read of the override spell) |
| 2 refresh DoTs after a damage proc | not built |
| 3 Shadowfury, Fear, Howl on the healer | Shadowfury then Fear in the shared control chain (BT-663); Howl is a melee peel, not a chain link |
| 4 Fear a damage dealer when the dispel is spent | not built (the awareness book does not record dispels) |
| 5 no Fear on a healer on a pillar | the core cast check includes line of sight; no pillar read |
| 6 Howl on a melee in 8 yd | done (BT-664) |
| 7 Howl after Tremor | not built |
| 8 Demonic Circle behind a pillar | not built (no circle spells) |
| 9 Dark Regeneration with Healthstone | done (BT-667) |
| 10 Soulburn pet summon | not built |
| 11 Haunt in the window | done (BT-666) |
| 12 Fel Flame in a lockout | done for a shadow lockout (BT-665); not for a totem or Spell Reflection |
| 13 DoTs on pets | not built |

Set: unchanged, ladder 100 percent on four tiers and Unbound Will 80, Grimoire of Supremacy 90, Siphon Life and Eternal Resolve 100. Shadowfury now has a rule.
